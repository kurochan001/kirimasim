// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved event-NBA contract: captured values, pending issue order and roots.
// Tests drive the shared API directly; source event-NBAs are still unsupported.
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;

kirima_status done(kirima_context*, void*) { return KIRIMA_DONE; }
k::ProcessId origin(k::Context& c, k::Group group = k::Group::Active) {
    return c.scheduler().addProcess(done, nullptr, group);
}
k::SlotId slot(k::Context& c, unsigned width = 32, k::Domain domain = k::Domain::Two,
               unsigned count = 1) {
    return c.store().add(domain, width, count, k::Init::Zero);
}
k::NbaCapture capture(k::Context& c, k::ProcessId p, k::SlotId s, std::uint64_t value) {
    const k::NbaWrite write{{.slot = s}, {&value, 1}};
    return c.scheduler().captureNba(p, {&write, 1});
}
std::uint64_t counter(k::Context& c, const char* name) {
    return c.counters().value(c.counters().add(name));
}
struct Values {
    k::SlotId source;
    std::vector<std::uint64_t> seen;
    static int record(kirima_context* context, void* frame) {
        auto& values = *static_cast<Values*>(frame);
        values.seen.push_back(k::fromC(context).store().element(values.source, 0)[0]);
        return 0;  // keep the observer subscribed for every committed write
    }
};
void observe(k::Context& c, Values& values) {
    const k::WaitItem item{{values.source}, k::Edge::Any, &Values::record, &values};
    c.scheduler().waitEvent(origin(c), {&item, 1});
}

TEST(NbaPayload, IssueCounterNeverWrapsAndFailureDoesNotAdvance) {
    std::uint64_t next = UINT64_MAX - 2;
    EXPECT_EQ(k::detail::takeNbaIssue(next), UINT64_MAX - 2);
    EXPECT_EQ(k::detail::takeNbaIssue(next), UINT64_MAX - 1);
    EXPECT_EQ(k::detail::takeNbaIssue(next), 0U);
    EXPECT_EQ(k::detail::takeNbaIssue(next), 0U);
    EXPECT_EQ(next, UINT64_MAX);
    next = 0;
    EXPECT_EQ(k::detail::takeNbaIssue(next), 0U);
    EXPECT_EQ(next, 0U);
}

TEST(NbaPayload, CaptureOwnsTargetAndWordsWithoutWritingOrCreatingAProcess) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c, 8, k::Domain::Two, 2);
    std::uint64_t value = 23;
    k::NbaWrite write{{.slot = s, .elem = 1}, {&value, 1}};
    const auto before = counter(c, "proc.created");
    const auto batch = c.scheduler().captureNba(p, {&write, 1});
    ASSERT_TRUE(batch);
    value = 99;
    write.target.elem = 0;
    EXPECT_EQ(c.store().element(s, 1)[0], 0U);
    EXPECT_EQ(counter(c, "proc.created"), before);
    EXPECT_EQ(counter(c, "nba.scheduled"), 1U);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 1U);
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.store().element(s, 0)[0], 0U);
    EXPECT_EQ(c.store().element(s, 1)[0], 23U);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
}

TEST(NbaPayload, EmptyCaptureCompletesWithoutAnUpdateEventOrProcess) {
    k::Context c;
    const auto p = origin(c);
    const auto batch = c.scheduler().captureNba(p, {});
    ASSERT_TRUE(batch);
    EXPECT_EQ(counter(c, "nba.scheduled"), 0U);
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(counter(c, "sched.events.nba"), 0U);
    EXPECT_EQ(counter(c, "proc.created"), 1U);
}

TEST(NbaPayload, OrdinaryAndCapturedIssuesShareOrderRegardlessOfReadinessOrder) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        for (bool ordinaryFirst : {false, true}) {
            k::Context c;
            const auto p = origin(c, group);
            const auto s = slot(c);
            Values values{s, {}};
            observe(c, values);
            const std::uint64_t immediate = 2;
            if (ordinaryFirst) c.scheduler().scheduleNba(p, {.slot = s}, &immediate, 0);
            const auto batch = capture(c, p, s, 1);
            ASSERT_TRUE(batch);
            if (!ordinaryFirst) c.scheduler().scheduleNba(p, {.slot = s}, &immediate, 0);
            ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
            EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(values.seen, (ordinaryFirst ? std::vector<std::uint64_t>{2, 1}
                                                  : std::vector<std::uint64_t>{1, 2}));
            EXPECT_EQ(
                counter(c, group == k::Group::Active ? "sched.events.nba" : "sched.events.re_nba"),
                2U);
        }
    }
}

TEST(NbaPayload, RandomReadinessMatchesIndependentIssueListInBothGroups) {
    const auto seed = kirimasim::test::fuzzSeed(219);
    std::mt19937_64 random(seed);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    for (unsigned round = 0; round < 60 * kirimasim::test::fuzzScale(); ++round) {
        k::Context c;
        const auto active = origin(c), reactive = origin(c, k::Group::Reactive);
        const auto s = slot(c);
        Values values{s, {}};
        observe(c, values);
        struct Expected {
            unsigned issue;
            bool reactive;
            std::uint64_t value;
        };
        std::vector<Expected> expected;
        std::vector<k::NbaTicket> tickets;
        for (unsigned issue = 0; issue < 64; ++issue) {
            const bool inReactive = random() % 2 != 0;
            const auto p = inReactive ? reactive : active;
            const std::uint64_t value = issue + 1;
            if (random() % 3 == 0) {
                c.scheduler().scheduleNba(p, {.slot = s}, &value, 0);
            } else {
                const auto batch = capture(c, p, s, value);
                ASSERT_TRUE(batch);
                tickets.push_back(batch.ticket);
            }
            expected.push_back({issue, inReactive, value});
        }
        std::shuffle(tickets.begin(), tickets.end(), random);
        for (const auto ticket : tickets)
            ASSERT_EQ(c.scheduler().readyNba(ticket), k::NbaError::None);
        std::stable_sort(expected.begin(), expected.end(), [](const auto& a, const auto& b) {
            return a.reactive != b.reactive ? !a.reactive : a.issue < b.issue;
        });
        EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
        std::vector<std::uint64_t> wanted;
        wanted.reserve(expected.size());
        for (const auto& event : expected)
            wanted.push_back(event.value);
        EXPECT_EQ(values.seen, wanted) << round;
    }
}

struct MakeReady {
    k::NbaTicket ticket;
    k::NbaError result = k::NbaError::InvalidTicket;
    static int guard(kirima_context* context, void* frame) {
        auto& self = *static_cast<MakeReady*>(frame);
        self.result = k::fromC(context).scheduler().readyNba(self.ticket);
        return 0;
    }
};
TEST(NbaPayload, ReadinessDuringNbaInsertsBeforeOnlyTheUnexecutedSuffix) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context c;
        const auto p = origin(c, group);
        const auto s = slot(c), trigger = slot(c);
        Values values{s, {}};
        observe(c, values);
        const auto batch = capture(c, p, s, 10);
        ASSERT_TRUE(batch);
        MakeReady ready{batch.ticket};
        const k::WaitItem item{{trigger}, k::Edge::Any, &MakeReady::guard, &ready};
        c.scheduler().waitEvent(origin(c), {&item, 1});
        const std::uint64_t first = 20, edge = 1, last = 30;
        c.scheduler().scheduleNba(p, {.slot = s}, &first, 0);
        c.scheduler().scheduleNba(p, {.slot = trigger}, &edge, 0);
        c.scheduler().scheduleNba(p, {.slot = s}, &last, 0);
        EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(ready.result, k::NbaError::None);
        EXPECT_EQ(values.seen, (std::vector<std::uint64_t>{20, 10, 30}));
    }
}

struct LateReady {
    k::ProcessId process;
    k::NbaTicket ticket;
    k::SlotId slot;
    unsigned step = 0;
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& self = *static_cast<LateReady*>(frame);
        auto& c = k::fromC(context);
        if (self.step++ == 0) {
            c.scheduler().waitDelay(self.process, 2);
            return KIRIMA_WAITING;
        }
        const std::uint64_t value = 30;
        c.scheduler().scheduleNba(self.process, {.slot = self.slot}, &value, 0);
        EXPECT_EQ(c.scheduler().readyNba(self.ticket), k::NbaError::None);
        return KIRIMA_DONE;
    }
};
TEST(NbaPayload, EarlierSlotCapturePreservesTimedDuePriorityAndCurrentIssueOrder) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context c;
        const auto p = origin(c, group);
        const auto s = slot(c);
        Values values{s, {}};
        observe(c, values);
        const auto batch = capture(c, p, s, 10);
        ASSERT_TRUE(batch);
        const std::uint64_t due1 = 20, due2 = 21;
        c.scheduler().scheduleNba(p, {.slot = s}, &due1, 2);
        c.scheduler().scheduleNba(p, {.slot = s}, &due2, 2);
        LateReady frame{{}, batch.ticket, s};
        frame.process = c.scheduler().addProcess(&LateReady::resume, &frame, group);
        c.scheduler().start(frame.process);
        EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(values.seen, (std::vector<std::uint64_t>{20, 21, 10, 30}));
        EXPECT_EQ(c.scheduler().time(), 2U);
    }
}

TEST(NbaPayload, AReadyOlderIssueCannotReorderAnAlreadyAppliedUpdate) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c);
    Values values{s, {}};
    observe(c, values);
    const auto batch = capture(c, p, s, 10);
    ASSERT_TRUE(batch);
    const std::uint64_t applied = 20;
    c.scheduler().scheduleNba(p, {.slot = s}, &applied, 0);
    LateReady frame{{}, batch.ticket, s};
    frame.process = c.scheduler().addProcess(&LateReady::resume, &frame, k::Group::Active);
    c.scheduler().start(frame.process);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(values.seen, (std::vector<std::uint64_t>{20, 10, 30}));
}

TEST(NbaPayload, CapturedGroupSurvivesIssuerEndKillReleaseAndRecordReuse) {
    for (bool kill : {false, true}) {
        k::Context c;
        const auto p = origin(c, k::Group::Reactive);
        const auto s = slot(c);
        const auto batch = capture(c, p, s, 45);
        ASSERT_TRUE(batch);
        if (kill)
            c.scheduler().kill(p);
        else
            c.scheduler().ended(p);
        c.scheduler().releaseProcess(p);
        EXPECT_EQ(origin(c), p);  // reuse for a process in the opposite group
        ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
        EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(c.store().element(s, 0)[0], 45U);
        EXPECT_EQ(counter(c, "sched.events.re_nba"), 1U);
        EXPECT_EQ(counter(c, "sched.events.nba"), 0U);
    }
}

TEST(NbaPayload, DiscardAndCompletionInvalidateTicketsWithoutCancelingOtherGenerations) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c);
    const auto first = capture(c, p, s, 11);
    ASSERT_TRUE(first);
    ASSERT_EQ(c.scheduler().discardNba(first.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().discardNba(first.ticket), k::NbaError::InvalidTicket);
    const auto next = capture(c, p, s, 22);
    ASSERT_TRUE(next);
    EXPECT_EQ(next.ticket.index, first.ticket.index);
    EXPECT_NE(next.ticket.serial, first.ticket.serial);
    EXPECT_EQ(c.scheduler().readyNba(first.ticket), k::NbaError::InvalidTicket);
    ASSERT_EQ(c.scheduler().readyNba(next.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().readyNba(next.ticket), k::NbaError::InvalidTicket);
    EXPECT_EQ(c.scheduler().discardNba(next.ticket), k::NbaError::InvalidTicket);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.store().element(s, 0)[0], 22U);
    EXPECT_EQ(c.scheduler().readyNba(next.ticket), k::NbaError::InvalidTicket);
}

TEST(NbaPayload, OrderedLeavesSnapshotWideParts) {
    for (const auto domain : {k::Domain::Two, k::Domain::Four}) {
        for (const unsigned width : {1U, 31U, 64U, 65U, 129U}) {
            k::Context c;
            const auto p = origin(c);
            const auto s = slot(c, width + 4, domain, 2);
            const auto words = (width + 63) / 64 * (domain == k::Domain::Four ? 2 : 1);
            std::vector<std::uint64_t> first(words, 0), second(words, 0);
            first[0] = 1;
            second[0] = 3;
            std::array writes{
                k::NbaWrite{{.slot = s, .elem = 1, .lsb = 2, .width = width}, first},
                k::NbaWrite{{.slot = s, .elem = 1, .lsb = 2, .width = width}, second}};
            const auto batch = c.scheduler().captureNba(p, writes);
            ASSERT_TRUE(batch);
            std::fill(first.begin(), first.end(), ~std::uint64_t{0});
            std::fill(second.begin(), second.end(), 0);
            ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
            EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(c.store().element(s, 1)[0], width == 1 ? 4U : 12U);
            EXPECT_EQ(c.store().element(s, 0)[0], 0U);
        }
    }
}

TEST(NbaPayload, WholeVariableForceKeepsItsValueUntilTheNextProceduralWrite) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c);
    c.scheduler().markHeld(s, false, false);
    const k::SlotPart whole{s};
    c.scheduler().hold(k::Hold::Force, p, whole);
    const std::uint64_t zero = 0;
    c.scheduler().writeHeld(k::Hold::Force, p, {.slot = s}, &zero);
    const auto batch = capture(c, p, s, 5);
    ASSERT_TRUE(batch);
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.store().element(s, 0)[0], 0U);
    c.scheduler().release(k::Hold::Force, whole);
    EXPECT_EQ(c.store().element(s, 0)[0], 0U);  // SCHEDULER_SPEC 14.3
    const std::uint64_t next = 7;
    c.scheduler().write({.slot = s}, &next);
    EXPECT_EQ(c.store().element(s, 0)[0], 7U);
}

TEST(NbaPayload, AllWordsAndFourStatePlanesSurviveSnapshotting) {
    for (const auto domain : {k::Domain::Two, k::Domain::Four}) {
        for (const unsigned width : {1U, 31U, 64U, 65U, 129U}) {
            k::Context c;
            const auto p = origin(c);
            const auto s = slot(c, width, domain);
            const auto planes = domain == k::Domain::Four ? 2U : 1U;
            std::vector<std::uint64_t> words(std::size_t{(width + 63) / 64} * planes);
            for (unsigned i = 0; i < words.size(); ++i)
                words[i] = 0xf0a53164c8d72e09ULL ^ (0x193af4ab64ULL * i);
            if (width % 64 != 0) {
                const auto mask = (std::uint64_t{1} << (width % 64)) - 1;
                for (unsigned i = 0; i < planes; ++i)
                    words[words.size() - planes + i] &= mask;
            }
            const auto expected = words;
            const k::NbaWrite write{{.slot = s}, words};
            const auto batch = c.scheduler().captureNba(p, {&write, 1});
            ASSERT_TRUE(batch);
            std::fill(words.begin(), words.end(), 0);
            ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
            EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
            const auto* start = c.store().element(s, 0);
            EXPECT_EQ((std::vector<std::uint64_t>{start, start + expected.size()}), expected);
        }
    }
}

struct Roots {
    std::vector<std::pair<k::SlotId, std::vector<std::uint64_t>>> values;
    bool accept = true;
    static bool visit(void* frame, k::SlotId slot, std::span<const std::uint64_t> data) noexcept {
        auto& self = *static_cast<Roots*>(frame);
        try {
            self.values.emplace_back(slot, std::vector<std::uint64_t>{data.begin(), data.end()});
            return self.accept;
        } catch (const std::bad_alloc&) {
            return false;
        } catch (const std::length_error&) {
            return false;
        }
    }
};
TEST(NbaPayload, UnreadyReadyAndDiscardedPayloadsHaveExactTypedRootVisits) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c, 64, k::Domain::Handle), other = slot(c);
    const std::uint64_t handle = 123, integral = 456;
    const std::array writes{k::NbaWrite{{.slot = s}, {&handle, 1}},
                            k::NbaWrite{{.slot = other}, {&integral, 1}}};
    const auto batch = c.scheduler().captureNba(p, writes);
    ASSERT_TRUE(batch);
    Roots unready;
    ASSERT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &unready));
    ASSERT_EQ(unready.values.size(), 2U);
    EXPECT_EQ(unready.values[0].first, s);
    EXPECT_EQ(unready.values[0].second, (std::vector<std::uint64_t>{123}));
    EXPECT_EQ(unready.values[1].first, other);
    Roots stop;
    stop.accept = false;
    EXPECT_FALSE(c.scheduler().visitPendingValues(&Roots::visit, &stop));
    EXPECT_EQ(stop.values.size(), 1U);
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    Roots ready;
    ASSERT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &ready));
    EXPECT_EQ(ready.values, unready.values);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    Roots after;
    EXPECT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &after));
    EXPECT_TRUE(after.values.empty());
    const auto discarded = capture(c, p, other, 9);
    ASSERT_TRUE(discarded);
    ASSERT_EQ(c.scheduler().discardNba(discarded.ticket), k::NbaError::None);
    Roots empty;
    EXPECT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &empty));
    EXPECT_TRUE(empty.values.empty());
}

TEST(NbaPayload, TypedVisitorKeepsRealHeapSnapshotsUntilDiscard) {
    k::Context c;
    const auto p = origin(c);
    const auto handleSlot = slot(c, 64, k::Domain::Handle);
    const std::array layoutWords{k::HeapWord{}};
    const auto layout = c.heap().addLayout(layoutWords);
    ASSERT_TRUE(layout);
    const auto type = c.heap().addType({k::ContainerKind::DynamicArray, layout.value, 0});
    ASSERT_TRUE(type);
    const std::array data{k::W4{17, 0}, k::W4{23, 0}};
    const auto value = c.heap().fromValues(type.value, data);
    ASSERT_TRUE(value);
    const auto batch = capture(c, p, handleSlot, value.value.bits);
    ASSERT_TRUE(batch);
    Roots pending;
    ASSERT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &pending));
    ASSERT_EQ(pending.values.size(), 1U);
    ASSERT_EQ(pending.values[0].first, handleSlot);
    const std::array roots{k::HeapRoot::container(k::ContainerToken{pending.values[0].second[0]})};
    ASSERT_TRUE(c.heap().collect(roots));
    EXPECT_EQ(c.heap().stats().containers, 1U);
    EXPECT_EQ(c.heap().stats().cells, 2U);
    ASSERT_EQ(c.scheduler().discardNba(batch.ticket), k::NbaError::None);
    Roots after;
    ASSERT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &after));
    EXPECT_TRUE(after.values.empty());
    ASSERT_TRUE(c.heap().collect({}));
    EXPECT_EQ(c.heap().stats().containers, 0U);
    EXPECT_EQ(c.heap().stats().cells, 0U);
}

struct Reenter {
    k::ProcessId origin;
    k::SlotId target;
    k::NbaTicket executing;
    std::size_t roots = 0;
    k::NbaError discard = k::NbaError::None;
    static int guard(kirima_context* context, void* frame) {
        auto& self = *static_cast<Reenter*>(frame);
        auto& c = k::fromC(context);
        Roots roots;
        EXPECT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &roots));
        self.roots = roots.values.size();
        self.discard = c.scheduler().discardNba(self.executing);
        for (unsigned i = 0; i < 128; ++i) {
            const auto another = capture(c, self.origin, self.target, i + 1);
            EXPECT_TRUE(another);
        }
        return 0;
    }
};
TEST(NbaPayload, DeliveryKeepsAllRootsAndStableFieldsAcrossReentrantTableGrowth) {
    k::Context c;
    const auto p = origin(c);
    const auto first = slot(c), second = slot(c), other = slot(c);
    const std::uint64_t a = 7, b = 9;
    const std::array writes{k::NbaWrite{{.slot = first}, {&a, 1}},
                            k::NbaWrite{{.slot = second}, {&b, 1}}};
    const auto batch = c.scheduler().captureNba(p, writes);
    ASSERT_TRUE(batch);
    Reenter frame{p, other, batch.ticket};
    const k::WaitItem item{{first}, k::Edge::Any, &Reenter::guard, &frame};
    c.scheduler().waitEvent(origin(c), {&item, 1});
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(frame.roots, 2U);
    EXPECT_EQ(frame.discard, k::NbaError::InvalidTicket);
    EXPECT_EQ(c.store().element(first, 0)[0], 7U);
    EXPECT_EQ(c.store().element(second, 0)[0], 9U);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
}

TEST(NbaPayload, InvalidRequestsPublishNoPrefixAndDoNotConsumeOtherTickets) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c, 8);
    const auto good = capture(c, p, s, 7);
    ASSERT_TRUE(good);
    const auto before = counter(c, "nba.scheduled");
    const std::uint64_t value = 8;
    for (const auto at : {k::SlotBits{k::SlotId{k::kNone}}, k::SlotBits{s, 1, 0, 8},
                          k::SlotBits{s, 0, 7, 2}, k::SlotBits{s, 0, 0, 9}}) {
        const std::array writes{k::NbaWrite{{.slot = s}, {&value, 1}},
                                k::NbaWrite{at, {&value, 1}}};
        EXPECT_EQ(c.scheduler().captureNba(p, writes).error, k::NbaError::InvalidTarget);
    }
    const k::NbaWrite badValue{{.slot = s}, {}};
    EXPECT_EQ(c.scheduler().captureNba(p, {&badValue, 1}).error, k::NbaError::InvalidValue);
    EXPECT_EQ(c.scheduler().captureNba(k::ProcessId{k::kNone}, {}).error,
              k::NbaError::InvalidOrigin);
    EXPECT_EQ(c.scheduler().readyNba({}), k::NbaError::InvalidTicket);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 1U);
    EXPECT_EQ(counter(c, "nba.scheduled"), before);
    EXPECT_FALSE(c.scheduler().ending());
    ASSERT_EQ(c.scheduler().readyNba(good.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(c.store().element(s, 0)[0], 7U);
}

TEST(NbaPayload, QuiescenceAndExplicitTerminationDropUnreadyPayloadRoots) {
    for (bool explicitEnd : {false, true}) {
        k::Context c;
        const auto p = origin(c);
        const auto s = slot(c);
        const auto batch = capture(c, p, s, 12);
        ASSERT_TRUE(batch);
        if (explicitEnd) c.scheduler().requestEnd(k::EndReason::Finish, 0);
        EXPECT_EQ(c.scheduler().run(),
                  explicitEnd ? k::EndReason::Finish : k::EndReason::Quiescent);
        EXPECT_EQ(c.store().element(s, 0)[0], 0U);
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        EXPECT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::InvalidTicket);
    }
}

TEST(NbaPayload, CompletedProgramDoesNotStayAliveForUnreadyPayloads) {
    k::Context c;
    const auto program = c.scheduler().addProgram();
    const auto p = c.scheduler().addProgramInitial(done, nullptr, program);
    const auto s = slot(c);
    const auto batch = capture(c, p, s, 31);
    ASSERT_TRUE(batch);
    c.scheduler().start(p);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Finish);
    EXPECT_EQ(c.scheduler().time(), 0U);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
    EXPECT_EQ(c.store().element(s, 0)[0], 0U);
}

TEST(NbaPayload, TerminationDuringPayloadDropsRemainingLeavesAndRoots) {
    k::Context c;
    const auto p = origin(c);
    const auto first = slot(c), second = slot(c);
    const std::uint64_t a = 7, b = 9;
    const std::array writes{k::NbaWrite{{.slot = first}, {&a, 1}},
                            k::NbaWrite{{.slot = second}, {&b, 1}}};
    const auto batch = c.scheduler().captureNba(p, writes);
    ASSERT_TRUE(batch);
    const auto end = [](kirima_context* context, void*) -> int {
        k::fromC(context).scheduler().requestEnd(k::EndReason::Finish, 0);
        return 0;
    };
    const k::WaitItem item{{first}, k::Edge::Any, end, nullptr};
    c.scheduler().waitEvent(origin(c), {&item, 1});
    ASSERT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Finish);
    EXPECT_EQ(c.store().element(first, 0)[0], 7U);
    EXPECT_EQ(c.store().element(second, 0)[0], 0U);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
}

TEST(NbaPayload, TerminationAndFinalRejectNewCapturesWithoutPublishing) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c);
    const auto batch = capture(c, p, s, 5);
    ASSERT_TRUE(batch);
    c.scheduler().requestEnd(k::EndReason::Finish, 0);
    EXPECT_EQ(capture(c, p, s, 7).error, k::NbaError::InvalidPhase);
    EXPECT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::InvalidPhase);
    EXPECT_EQ(c.scheduler().run(), k::EndReason::Finish);
    struct Final {
        k::ProcessId p;
        k::NbaError error = k::NbaError::None;
    };
    Final frame;
    const auto final = [](kirima_context* context, void* raw) -> kirima_status {
        auto& frame = *static_cast<Final*>(raw);
        frame.error = k::fromC(context).scheduler().captureNba(frame.p, {}).error;
        return KIRIMA_DONE;
    };
    frame.p = c.scheduler().addProcess(final, &frame, k::Group::Active);
    c.scheduler().runFinal({&frame.p, 1});
    EXPECT_EQ(frame.error, k::NbaError::InvalidPhase);
    EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
}

TEST(NbaPayload, ChurnReusesOnlyCompletedOrDiscardedGenerationRows) {
    k::Context c;
    const auto p = origin(c);
    const auto s = slot(c);
    for (unsigned round = 0; round < 200; ++round) {
        const auto batch = capture(c, p, s, round + 1);
        ASSERT_TRUE(batch);
        if (round % 2 == 0) {
            EXPECT_EQ(c.scheduler().discardNba(batch.ticket), k::NbaError::None);
        } else {
            EXPECT_EQ(c.scheduler().readyNba(batch.ticket), k::NbaError::None);
            EXPECT_EQ(c.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(c.store().element(s, 0)[0], round + 1);
        }
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        EXPECT_EQ(c.scheduler().nbaStorageStats().records, 1U);
    }
}

class Memory final : public std::pmr::memory_resource {
public:
    std::optional<unsigned> remaining;
    std::size_t liveBytes = 0;

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        if (remaining) {
            if (*remaining == 0) throw std::bad_alloc{};
            --*remaining;
        }
        auto* p = std::pmr::new_delete_resource()->allocate(size, alignment);
        liveBytes += size;
        return p;
    }
    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override {
        liveBytes -= size;
        std::pmr::new_delete_resource()->deallocate(p, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(NbaPayload, AllocationFailurePublishesNoWritesCountersOrPartialRoots) {
    unsigned failures = 0, successes = 0;
    for (unsigned failAfter = 0; failAfter < 8; ++failAfter) {
        Memory memory;
        {
            k::Context c({stdout, stderr}, nullptr, &memory);
            const auto p = origin(c);
            const auto s = slot(c);
            const std::uint64_t value = 42;
            const std::array writes{k::NbaWrite{{.slot = s}, {&value, 1}},
                                    k::NbaWrite{{.slot = s}, {&value, 1}}};
            memory.remaining = failAfter;
            const auto batch = c.scheduler().captureNba(p, writes);
            memory.remaining.reset();
            if (batch) {
                ++successes;
                EXPECT_EQ(c.scheduler().discardNba(batch.ticket), k::NbaError::None);
            } else {
                ++failures;
                EXPECT_EQ(batch.error, k::NbaError::ResourceLimit);
                EXPECT_EQ(c.scheduler().reason(), k::EndReason::ResourceLimit);
                EXPECT_EQ(counter(c, "nba.scheduled"), 0U);
                EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
                Roots roots;
                EXPECT_TRUE(c.scheduler().visitPendingValues(&Roots::visit, &roots));
                EXPECT_TRUE(roots.values.empty());
                EXPECT_EQ(c.store().element(s, 0)[0], 0U);
            }
        }
        EXPECT_EQ(memory.liveBytes, 0U);
    }
    EXPECT_GE(failures, 4U);
    EXPECT_GT(successes, 0U);
}

TEST(NbaPayload, ContextsKeepIndependentIssueCountersPayloadsAndGroups) {
    k::Context first, second;
    const auto p1 = origin(first), p2 = origin(second, k::Group::Reactive);
    const auto s1 = slot(first), s2 = slot(second);
    const auto a = capture(first, p1, s1, 11), b = capture(second, p2, s2, 22);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_EQ(second.scheduler().readyNba(b.ticket), k::NbaError::None);
    ASSERT_EQ(first.scheduler().readyNba(a.ticket), k::NbaError::None);
    EXPECT_EQ(second.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(first.store().element(s1, 0)[0], 0U);
    EXPECT_EQ(first.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(first.store().element(s1, 0)[0], 11U);
    EXPECT_EQ(second.store().element(s2, 0)[0], 22U);
    EXPECT_EQ(counter(first, "sched.events.nba"), 1U);
    EXPECT_EQ(counter(second, "sched.events.re_nba"), 1U);
}
}  // namespace
