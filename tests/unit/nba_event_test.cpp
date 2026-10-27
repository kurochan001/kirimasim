// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved event-NBA contract: independent concrete subscriptions and origins.
// Original API tests; source event-NBA and expression monitors remain work.
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/logic.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;

kirima_status done(kirima_context*, void*) { return KIRIMA_DONE; }
struct Rig {
    unsigned cleanups = 0;
    k::Context c;
    k::ProcessId p;
    k::SlotId out;
    explicit Rig(k::Group group = k::Group::Active)
        : p(c.scheduler().addProcess(done, nullptr, group)),
          out(c.store().add(k::Domain::Two, 32, 1, k::Init::Zero)) {}
    ~Rig() { c.scheduler().cancelNbaEvents(); }
    static void release(void* raw) noexcept { ++static_cast<Rig*>(raw)->cleanups; }
    k::NbaCapture capture(std::uint64_t value = 42) {
        const k::NbaWrite write{{.slot = out}, {&value, 1}};
        return c.scheduler().captureNba(p, {&write, 1});
    }
    k::NbaEventRegistration arm(std::uint64_t count, std::span<const k::NbaEventItem> items,
                                std::uint64_t value = 42) {
        const auto payload = capture(value);
        EXPECT_TRUE(payload);
        const k::W4 word{count, 0};
        return c.scheduler().registerNbaEvent(p, payload.ticket, {64, false, true, {&word, 1}},
                                              items, {&release, this});
    }
    std::uint64_t output() { return c.store().element(out, 0)[0]; }
};
k::NbaEventItem named(k::EventId event, k::GuardFn guard = nullptr, void* frame = nullptr) {
    return k::NamedValueWaitItem{k::EventValue::legacy(event), guard, frame};
}
std::uint64_t counter(k::Context& c, const char* name) {
    return c.counters().value(c.counters().add(name));
}

TEST(NbaEvent, SynchronousNamedTriggersCountWithoutSuspendingOrCreatingChildren) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        Rig r(group);
        auto& s = r.c.scheduler();
        k::Limits limits;
        limits.maxProcs = 0;
        s.setLimits(limits);
        const auto event = s.addEvent(), ordinary = s.addEvent();
        s.waitNamed(r.p, ordinary);
        const auto created = counter(r.c, "proc.created");
        const std::array items{named(event)};
        const auto control = r.arm(2, items);
        ASSERT_TRUE(control);
        EXPECT_TRUE(s.hasCallbackOrigins(r.p));
        EXPECT_FALSE(s.finished(r.p));
        EXPECT_EQ(counter(r.c, "proc.created"), created);
        s.trigger(event);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        EXPECT_EQ(r.cleanups, 0U);
        s.trigger(event);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(r.p));
        EXPECT_EQ(r.cleanups, 1U);
        EXPECT_EQ(r.output(), 0U);
        s.trigger(ordinary);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_TRUE(s.finished(r.p));
        EXPECT_EQ(r.output(), 42U);
        EXPECT_EQ(
            counter(r.c, group == k::Group::Active ? "sched.events.nba" : "sched.events.re_nba"),
            1U);
    }
}

TEST(NbaEvent, SoleSlotSubscriptionObservesEveryCommittedWrite) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto clock = r.c.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const std::array items{k::NbaEventItem{k::WaitItem{{clock}}}};
    ASSERT_TRUE(r.arm(3, items));
    for (const std::uint64_t bit : {1U, 1U, 0U})
        s.write({.slot = clock}, &bit);
    EXPECT_EQ(s.nbaEventStats().live, 1U);  // repeated 1 is not a notification
    const std::uint64_t bit = 1;
    s.write({.slot = clock}, &bit);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}

TEST(NbaEvent, AllFourStateEdgesAndWideSelectedBitsMatchTheValueReference) {
    constexpr std::array values{v::Logic::Zero, v::Logic::One, v::Logic::Z, v::Logic::X};
    for (auto width : {1U, 65U, 129U})
        for (auto edge : {k::Edge::Any, k::Edge::Pos, k::Edge::Neg, k::Edge::Both})
            for (unsigned from = 0; from < 4; ++from)
                for (unsigned to = 0; to < 4; ++to) {
                    SCOPED_TRACE(::testing::Message() << width << ':' << static_cast<int>(edge)
                                                      << ':' << from << ':' << to);
                    Rig r;
                    auto& s = r.c.scheduler();
                    const auto clock = r.c.store().add(k::Domain::Four, width, 2, k::Init::Zero);
                    const k::SlotBits bit{.slot = clock, .elem = 1, .lsb = width - 1, .width = 1};
                    const std::array<std::uint64_t, 2> before{from & 1U, from >> 1U};
                    const std::array<std::uint64_t, 2> after{to & 1U, to >> 1U};
                    s.write(bit, before.data());
                    const std::array items{
                        k::NbaEventItem{k::WaitItem{{clock, 1, 1, width - 1, 1}, edge}}};
                    const auto control = r.arm(1, items);
                    ASSERT_TRUE(control);
                    s.write(bit, after.data());
                    const bool pos = v::isPosedge(values[from], values[to]);
                    const bool neg = v::isNegedge(values[from], values[to]);
                    const bool hit = edge == k::Edge::Any   ? from != to
                                     : edge == k::Edge::Pos ? pos
                                     : edge == k::Edge::Neg ? neg
                                                            : pos || neg;
                    EXPECT_EQ(s.nbaEventStats().live, hit ? 0U : 1U);
                    EXPECT_EQ(r.cleanups, hit ? 1U : 0U);
                    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
                    EXPECT_EQ(r.output(), hit ? 42U : 0U);
                }
}

struct OrderedGuard {
    std::vector<unsigned>* seen;
    unsigned tag;
    bool accept;
    static int run(kirima_context*, void* raw) {
        const auto& g = *static_cast<OrderedGuard*>(raw);
        g.seen->push_back(g.tag);
        return g.accept;
    }
};
TEST(NbaEvent, DuplicateAndOverlappingItemsVisitEveryGuardInSourceOrderButCountOnce) {
    for (bool useNamed : {false, true}) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto event = s.addEvent();
        const auto clock = r.c.store().add(k::Domain::Two, 4, 1, k::Init::Zero);
        std::vector<unsigned> seen;
        OrderedGuard a{&seen, 1, true}, b{&seen, 2, false}, c{&seen, 3, true};
        std::vector<k::NbaEventItem> items;
        for (auto* g : {&a, &b, &c}) {
            if (useNamed)
                items.push_back(named(event, &OrderedGuard::run, g));
            else
                items.push_back(k::WaitItem{
                    {clock, 0, 0, 0, g->tag == 2 ? 1U : 4U}, k::Edge::Any, &OrderedGuard::run, g});
        }
        ASSERT_TRUE(r.arm(2, items));
        if (useNamed)
            s.trigger(event);
        else {
            const std::uint64_t x = 1;
            s.write({.slot = clock}, &x);
        }
        EXPECT_EQ(seen, (std::vector<unsigned>{1, 2, 3}));
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        if (useNamed)
            s.trigger(event);
        else {
            const std::uint64_t x = 0;
            s.write({.slot = clock}, &x);
        }
        EXPECT_EQ(seen, (std::vector<unsigned>{1, 2, 3, 1, 2, 3}));
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(r.output(), 42U);
    }
}
TEST(NbaEvent, FalseGuardsDoNotCountAndMixedKindsShareOneCountdown) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    const auto clock = r.c.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    std::vector<unsigned> seen;
    OrderedGuard g{&seen, 9, false};
    const std::array<k::NbaEventItem, 2> items{
        named(event, &OrderedGuard::run, &g),
        k::WaitItem{{clock}, k::Edge::Any, &OrderedGuard::run, &g}};
    ASSERT_TRUE(r.arm(2, items));
    s.trigger(event);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    g.accept = true;
    const std::uint64_t one = 1;
    s.write({.slot = clock}, &one);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.trigger(event);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(seen.size(), 3U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}

TEST(NbaEvent, CountIsCopiedAtFullWidthAndPaddingIsIgnored) {
    for (auto width : {1U, 32U, 64U, 65U, 129U, 257U}) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto event = s.addEvent();
        const std::array items{named(event)};
        std::vector<k::W4> words((width + 63) / 64, {0, 0});
        words[0].a = width == 1 ? 1 : 2;
        if (width % 64 != 0) {
            const auto mask = (std::uint64_t{1} << (width % 64)) - 1;
            words.back().a |= ~mask;
            words.back().b |= ~mask;
        }
        const auto payload = r.capture();
        ASSERT_TRUE(payload);
        const auto control = s.registerNbaEvent(r.p, payload.ticket, {width, false, true, words},
                                                items, {&Rig::release, &r});
        ASSERT_TRUE(control);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        words[0].a = 0;
        s.trigger(event);
        EXPECT_EQ(s.nbaEventStats().live, width == 1 ? 0U : 1U);
        if (width != 1) s.trigger(event);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(r.output(), 42U);
    }
}
TEST(NbaEvent, FullWidthCountdownBorrowsAcrossEveryWordAndStopsAtTheFirstNonzeroWord) {
    for (unsigned size = 1; size <= 5; ++size)
        for (unsigned at = 0; at < size; ++at) {
            std::vector<std::uint64_t> words(size, 0), expected(size, 0);
            words[at] = 1;
            for (unsigned i = 0; i < at; ++i)
                expected[i] = UINT64_MAX;
            EXPECT_EQ(k::detail::decrementNbaCount(words), at == 0);
            EXPECT_EQ(words, expected);
            if (at != 0) {
                expected[0] -= 1;
                EXPECT_FALSE(k::detail::decrementNbaCount(words));
                EXPECT_EQ(words, expected);
            }
        }
}
TEST(NbaEvent, HugeUnsignedCountsNeitherTruncateNorSaturateToSmallValues) {
    for (auto width : {65U, 129U, 257U}) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto event = s.addEvent();
        const std::array items{named(event)};
        std::vector<k::W4> words((width + 63) / 64, {0, 0});
        words.back().a = 1;
        const auto payload = r.capture();
        ASSERT_TRUE(payload);
        ASSERT_TRUE(s.registerNbaEvent(r.p, payload.ticket, {width, false, false, words}, items));
        for (unsigned i = 0; i < 20; ++i)
            s.trigger(event);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        EXPECT_TRUE(s.hasCallbackOrigins(r.p));
        EXPECT_EQ(r.output(), 0U);
        s.cancelNbaEvents();
        EXPECT_FALSE(s.hasCallbackOrigins(r.p));
        EXPECT_EQ(s.nbaStorageStats().live, 0U);
    }
}
TEST(NbaEvent, NonpositiveAndUnknownCountsSkipBindingAndGuards) {
    for (auto width : {1U, 32U, 64U, 65U, 129U, 257U})
        for (unsigned kind = 0; kind < 4; ++kind) {
            Rig r;
            auto& s = r.c.scheduler();
            std::vector<k::W4> words((width + 63) / 64, {0, 0});
            if (kind == 1) words.back().a = std::uint64_t{1} << ((width - 1) % 64);
            if (kind >= 2) words[0] = {kind == 2 ? 0U : 1U, 1};
            const auto payload = r.capture();
            ASSERT_TRUE(payload);
            // Deliberately invalid: skipped controls never inspect descriptors.
            const std::array items{k::NbaEventItem{k::WaitItem{{k::SlotId{k::kNone}}}}};
            const auto control = s.registerNbaEvent(r.p, payload.ticket, {width, true, true, words},
                                                    items, {&Rig::release, &r});
            ASSERT_TRUE(control);
            EXPECT_EQ(control.ticket.index, k::kNone);
            EXPECT_EQ(s.nbaEventStats().records, 0U);
            EXPECT_FALSE(s.hasCallbackOrigins(r.p));
            EXPECT_EQ(r.cleanups, 1U);
            EXPECT_EQ(s.run(), k::EndReason::Quiescent);
            EXPECT_EQ(r.output(), 42U);
        }
}
TEST(NbaEvent, InvalidCountsAndPartsLeavePayloadAndCleanupWithCaller) {
    for (unsigned kind = 0; kind < 6; ++kind) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto payload = r.capture();
        ASSERT_TRUE(payload);
        const std::array words{k::W4{1, kind == 2 ? 1U : 0U}};
        const k::NbaRepeat count{kind == 0 ? 0U : kind == 1 ? 65U : 1U, false, kind != 2, words};
        k::WaitItem item{{r.out}};
        if (kind == 3) item.part.slot = k::SlotId{k::kNone};
        if (kind == 4) item.part.firstElem = 2;
        if (kind == 5) {
            item.part.lsb = 31;
            item.part.width = 2;
        }
        const std::array items{k::NbaEventItem{item}};
        const auto result =
            s.registerNbaEvent(r.p, payload.ticket, count, items, {&Rig::release, &r});
        EXPECT_FALSE(result);
        EXPECT_EQ(r.cleanups, 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(r.p));
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.discardNba(payload.ticket), k::NbaError::None);
    }
}
TEST(NbaEvent, BoundPayloadCannotBeReadiedDiscardedOrBoundTwice) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    const std::array items{named(event)};
    const auto payload = r.capture();
    ASSERT_TRUE(payload);
    const k::W4 one{1, 0};
    const auto control =
        s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, items);
    ASSERT_TRUE(control);
    EXPECT_EQ(s.readyNba(payload.ticket), k::NbaError::InvalidTicket);
    EXPECT_EQ(s.discardNba(payload.ticket), k::NbaError::InvalidTicket);
    EXPECT_EQ(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, items).error,
              k::NbaError::InvalidTicket);
    s.trigger(event);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}
TEST(NbaEvent, OriginSurvivesOrdinaryCancellationKillAndReleaseUntilTheLastControl) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent(), ordinary = s.addEvent();
    const std::array items{named(event)};
    const auto first = r.arm(1, items), second = r.arm(2, items);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    s.waitNamed(r.p, ordinary);
    s.cancelWait(r.p);
    s.kill(r.p);
    EXPECT_TRUE(s.finished(r.p));
    s.releaseProcess(r.p);
    EXPECT_NE(s.addProcess(done, nullptr, k::Group::Active), r.p);
    s.trigger(event);
    EXPECT_TRUE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(s.discardNbaEvent(second.ticket), k::NbaError::None);
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(s.callbackReleaseEpoch(), 1U);
    s.releaseProcess(r.p);
    EXPECT_EQ(s.addProcess(done, nullptr, k::Group::Active), r.p);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}

struct Roots {
    unsigned count = 0;
    static bool visit(void* raw, k::SlotId, std::span<const std::uint64_t>) noexcept {
        ++static_cast<Roots*>(raw)->count;
        return true;
    }
};
struct SelfCancel {
    Rig* rig;
    k::NbaEventTicket ticket;
    k::EventValue managed;
    static int run(kirima_context*, void* raw) {
        auto& f = *static_cast<SelfCancel*>(raw);
        auto& s = f.rig->c.scheduler();
        EXPECT_EQ(s.nbaEventStats().callbacks, 1U);
        EXPECT_EQ(s.discardNbaEvent(f.ticket), k::NbaError::None);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.nbaEventStats().callbacks, 1U);
        EXPECT_EQ(f.rig->cleanups, 0U);
        EXPECT_TRUE(s.hasCallbackOrigins(f.rig->p));
        Roots roots;
        EXPECT_TRUE(s.visitPendingValues(&Roots::visit, &roots));
        EXPECT_EQ(roots.count, 1U);
        if (f.managed.bits != 0) {
            EXPECT_EQ(f.rig->c.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_TRUE(f.rig->c.managedEvents().valid({f.managed.bits}));
        }
        return 1;
    }
};
TEST(NbaEvent, SelfCancellationDefersRootsPinsAndCleanupUntilCallbackReturn) {
    for (bool managed : {false, true}) {
        Rig r;
        auto& s = r.c.scheduler();
        auto owned = r.c.managedEvents().create();
        ASSERT_TRUE(owned);
        const auto event = managed ? r.c.managedEvents().resolve(owned.value).value : s.addEvent();
        const auto value =
            managed ? k::EventValue::managed(owned.value.token()) : k::EventValue::legacy(event);
        SelfCancel frame{&r, {}, managed ? value : k::EventValue{}};
        const std::array items{
            k::NbaEventItem{k::NamedValueWaitItem{value, &SelfCancel::run, &frame}},
            k::NbaEventItem{k::NamedValueWaitItem{value, &SelfCancel::run, &frame}}};
        const auto control = r.arm(1, items);
        ASSERT_TRUE(control);
        frame.ticket = control.ticket;
        owned.value.reset();
        s.trigger(event);
        EXPECT_EQ(r.cleanups, 1U);
        EXPECT_EQ(s.nbaEventStats().callbacks, 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(r.p));
        Roots roots;
        EXPECT_TRUE(s.visitPendingValues(&Roots::visit, &roots));
        EXPECT_EQ(roots.count, 0U);
        EXPECT_EQ(r.c.managedEvents().collect(), k::ManagedEventError::None);
        if (managed) EXPECT_FALSE(r.c.managedEvents().valid({value.bits}));
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(r.output(), 0U);
    }
}

struct Grow {
    Rig* rig;
    k::EventId event;
    static int run(kirima_context*, void* raw) {
        auto& f = *static_cast<Grow*>(raw);
        for (unsigned i = 0; i < 128; ++i) {
            const std::array items{named(f.event)};
            EXPECT_TRUE(f.rig->arm(1, items, 1000 + i));
            f.rig->c.scheduler().addEvent();
        }
        return 1;
    }
};
TEST(NbaEvent, ReentrantGrowthUsesStableTicketsAndExcludesNewControlsFromTheCurrentTrigger) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    Grow frame{&r, event};
    const std::array items{named(event, &Grow::run, &frame)};
    ASSERT_TRUE(r.arm(1, items));
    s.trigger(event);
    EXPECT_EQ(s.nbaEventStats().live, 128U);
    EXPECT_EQ(r.cleanups, 1U);
    s.trigger(event);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(r.cleanups, 129U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 1127U);
}
struct Writes {
    Rig* rig;
    k::SlotId clock;
    unsigned calls = 0, depth = 0, maxDepth = 0;
    static int run(kirima_context*, void* raw) {
        auto& f = *static_cast<Writes*>(raw);
        ++f.calls;
        ++f.depth;
        if (f.depth > f.maxDepth) f.maxDepth = f.depth;
        if (f.calls == 1) {
            const std::uint64_t zero = 0, one = 1;
            f.rig->c.scheduler().write({.slot = f.clock}, &zero);
            f.rig->c.scheduler().write({.slot = f.clock}, &one);
        }
        --f.depth;
        return 1;
    }
};
TEST(NbaEvent, CallbackWritesAreSeparateDeferredNotificationsWithoutRecursiveMatching) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto clock = r.c.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    Writes frame{&r, clock};
    const std::array items{
        k::NbaEventItem{k::WaitItem{{clock}, k::Edge::Any, &Writes::run, &frame}}};
    ASSERT_TRUE(r.arm(3, items));
    const std::uint64_t one = 1;
    s.write({.slot = clock}, &one);
    EXPECT_EQ(frame.calls, 3U);
    EXPECT_EQ(frame.maxDepth, 1U);
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}

struct ReentrantCleanup {
    Rig* rig;
    k::EventId event;
    unsigned calls = 0;
    static void run(void* raw) noexcept {
        auto& f = *static_cast<ReentrantCleanup*>(raw);
        ++f.calls;
        EXPECT_TRUE(f.rig->c.scheduler().hasCallbackOrigins(f.rig->p));
        f.rig->c.scheduler().cancelNbaEvents();
        EXPECT_TRUE(f.rig->c.scheduler().hasCallbackOrigins(f.rig->p));
        const std::array items{named(f.event)};
        EXPECT_TRUE(f.rig->arm(1, items, 71));
    }
};
TEST(NbaEvent, CleanupCanCancelOtherControlsAndRegisterAgainWithoutDoubleReleasingOrigin) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    ReentrantCleanup frame{&r, event};
    const std::array items{named(event)};
    const auto payload = r.capture();
    ASSERT_TRUE(payload);
    const k::W4 one{1, 0};
    ASSERT_TRUE(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, items,
                                   {&ReentrantCleanup::run, &frame}));
    ASSERT_TRUE(r.arm(2, items));
    s.trigger(event);
    EXPECT_EQ(frame.calls, 1U);
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    EXPECT_TRUE(s.hasCallbackOrigins(r.p));
    s.trigger(event);
    EXPECT_EQ(r.cleanups, 2U);
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(s.callbackReleaseEpoch(), 1U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 71U);
}
TEST(NbaEvent, ManagedBindingsPinCapturedObjectsAndManagedCancellationLeavesLegacyControls) {
    Rig r;
    auto& s = r.c.scheduler();
    auto owned = r.c.managedEvents().create();
    auto replacement = r.c.managedEvents().create();
    ASSERT_TRUE(owned);
    ASSERT_TRUE(replacement);
    const auto old = k::EventValue::managed(owned.value.token());
    const auto event = s.addEvent();
    const std::array managed{k::NbaEventItem{k::NamedValueWaitItem{old}}};
    const std::array legacy{named(event)};
    ASSERT_TRUE(r.arm(1, managed, 10));
    ASSERT_TRUE(r.arm(1, legacy, 20));
    owned.value.reset();
    ASSERT_EQ(r.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_TRUE(r.c.managedEvents().valid({old.bits}));
    EXPECT_EQ(s.triggerValue(k::EventValue::managed(replacement.value.token())),
              k::ManagedEventError::None);
    EXPECT_EQ(s.nbaEventStats().live, 2U);
    s.cancelManagedEventOperations();
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    ASSERT_EQ(r.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_FALSE(r.c.managedEvents().valid({old.bits}));
    EXPECT_TRUE(s.hasCallbackOrigins(r.p));
    s.trigger(event);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 20U);
}
TEST(NbaEvent, NullNamedItemNeverFiresButDoesNotSuppressOtherItems) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    const std::array<k::NbaEventItem, 2> items{k::NamedValueWaitItem{}, named(event)};
    ASSERT_TRUE(r.arm(1, items));
    EXPECT_EQ(s.triggerValue({}), k::ManagedEventError::None);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.trigger(event);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}
TEST(NbaEvent, EmptyPayloadStillEvaluatesControlAndGuard) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto payload = s.captureNba(r.p, {});
    ASSERT_TRUE(payload);
    const auto event = s.addEvent();
    std::vector<unsigned> seen;
    OrderedGuard guard{&seen, 7, true};
    const std::array items{named(event, &OrderedGuard::run, &guard)};
    const k::W4 one{1, 0};
    ASSERT_TRUE(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, items,
                                   {&Rig::release, &r}));
    s.trigger(event);
    EXPECT_EQ(seen, (std::vector<unsigned>{7}));
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(s.nbaStorageStats().live, 0U);
    EXPECT_EQ(counter(r.c, "nba.scheduled"), 0U);
}
TEST(NbaEvent, QuiescenceDiscardsControlsWithoutKeepingAProcessAlive) {
    Rig r;
    auto& s = r.c.scheduler();
    const std::array items{named(s.addEvent())};
    ASSERT_TRUE(r.arm(1, items));
    s.start(r.p);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_TRUE(s.finished(r.p));
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(r.output(), 0U);
    EXPECT_EQ(s.nbaEventStats().subscriptions, 0U);
}
TEST(NbaEvent, ProgramInitialCompletionDropsPendingControlsAndPreservesFinish) {
    Rig r;
    auto& s = r.c.scheduler();
    r.p = s.addProgramInitial(done, nullptr, s.addProgram());
    const std::array items{named(s.addEvent())};
    ASSERT_TRUE(r.arm(1, items));
    s.start(r.p);
    EXPECT_EQ(s.run(), k::EndReason::Finish);
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(r.output(), 0U);
}
TEST(NbaEvent, TicketReuseIsBoundedAndStaleCancellationCannotAffectTheNextGeneration) {
    Rig r;
    auto& s = r.c.scheduler();
    const std::array items{named(s.addEvent())};
    k::NbaEventTicket previous;
    for (unsigned i = 0; i < 200; ++i) {
        const auto control = r.arm(2, items);
        ASSERT_TRUE(control);
        if (i != 0) {
            EXPECT_EQ(control.ticket.index, previous.index);
            EXPECT_GT(control.ticket.serial, previous.serial);
            EXPECT_EQ(s.discardNbaEvent(previous), k::NbaError::InvalidTicket);
        }
        EXPECT_EQ(s.discardNbaEvent(control.ticket), k::NbaError::None);
        previous = control.ticket;
        EXPECT_EQ(s.nbaEventStats().records, 1U);
        EXPECT_EQ(s.nbaEventStats().subscriptions, 0U);
        EXPECT_EQ(s.nbaStorageStats().live, 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    }
    EXPECT_EQ(r.cleanups, 200U);
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
TEST(NbaEvent, EachStagingAllocationFailurePublishesNoSubscriptionOrOriginAndKeepsCallerCleanup) {
    unsigned failures = 0, successes = 0;
    for (unsigned failAfter = 0; failAfter < 24; ++failAfter) {
        Memory memory;
        unsigned cleanups = 0;
        {
            k::Context c({stdout, stderr}, nullptr, &memory);
            auto& s = c.scheduler();
            const auto p = s.addProcess(done, nullptr, k::Group::Active);
            const auto slot = c.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
            auto owner = c.managedEvents().create();
            ASSERT_TRUE(owner);
            const auto value = k::EventValue::managed(owner.value.token());
            const std::uint64_t n = 17;
            const k::NbaWrite write{{.slot = slot}, {&n, 1}};
            const auto payload = s.captureNba(p, {&write, 1});
            ASSERT_TRUE(payload);
            const std::array<k::NbaEventItem, 2> items{k::WaitItem{{slot}},
                                                       k::NamedValueWaitItem{value}};
            const k::W4 one{1, 0};
            memory.remaining = failAfter;
            const auto control = s.registerNbaEvent(
                p, payload.ticket, {1, false, true, {&one, 1}}, items,
                {[](void* raw) noexcept { ++*static_cast<unsigned*>(raw); }, &cleanups});
            memory.remaining.reset();
            if (control) {
                ++successes;
                EXPECT_EQ(s.discardNbaEvent(control.ticket), k::NbaError::None);
                EXPECT_EQ(cleanups, 1U);
            } else {
                ++failures;
                EXPECT_EQ(control.error, k::NbaError::ResourceLimit);
                EXPECT_EQ(s.reason(), k::EndReason::ResourceLimit);
                EXPECT_EQ(cleanups, 0U);
                EXPECT_FALSE(s.hasCallbackOrigins(p));
                EXPECT_EQ(s.nbaEventStats().live, 0U);
                EXPECT_EQ(s.discardNba(payload.ticket), k::NbaError::None);
            }
            owner.value.reset();
            ASSERT_EQ(c.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_FALSE(c.managedEvents().valid({value.bits}));
            EXPECT_EQ(c.store().element(slot, 0)[0], 0U);
        }
        EXPECT_EQ(memory.liveBytes, 0U);
    }
    EXPECT_GE(failures, 6U);
    EXPECT_GT(successes, 0U);
}
TEST(NbaEvent, NotificationAllocationFailureStopsBeforeAnyPayloadUpdate) {
    Memory memory;
    unsigned cleaned = 0;
    k::Context c({stdout, stderr}, nullptr, &memory);
    auto& s = c.scheduler();
    const auto p = s.addProcess(done, nullptr, k::Group::Active);
    const auto slot = c.store().add(k::Domain::Two, 32, 1, k::Init::Zero);
    const auto event = s.addEvent();
    const std::uint64_t n = 23;
    const k::NbaWrite write{{.slot = slot}, {&n, 1}};
    const auto payload = s.captureNba(p, {&write, 1});
    ASSERT_TRUE(payload);
    const k::W4 one{1, 0};
    const std::array items{named(event)};
    ASSERT_TRUE(
        s.registerNbaEvent(p, payload.ticket, {1, false, true, {&one, 1}}, items,
                           {[](void* raw) noexcept { ++*static_cast<unsigned*>(raw); }, &cleaned}));
    memory.remaining = 0;
    s.trigger(event);
    memory.remaining.reset();
    EXPECT_EQ(s.reason(), k::EndReason::ResourceLimit);
    EXPECT_EQ(c.store().element(slot, 0)[0], 0U);
    EXPECT_EQ(s.run(), k::EndReason::ResourceLimit);
    EXPECT_EQ(cleaned, 1U);
    EXPECT_FALSE(s.hasCallbackOrigins(p));
}
TEST(NbaEvent, DestructionReleasesControlsBeforeCallbackFramesAndManagedStorage) {
    unsigned cleanups = 0;
    {
        k::Context c;
        auto& s = c.scheduler();
        const auto p = s.addProcess(done, nullptr, k::Group::Active);
        const auto payload = s.captureNba(p, {});
        ASSERT_TRUE(payload);
        const k::W4 one{1, 0};
        const std::array items{named(s.addEvent())};
        ASSERT_TRUE(s.registerNbaEvent(
            p, payload.ticket, {1, false, true, {&one, 1}}, items,
            {[](void* raw) noexcept { ++*static_cast<unsigned*>(raw); }, &cleanups}));
    }
    EXPECT_EQ(cleanups, 1U);
}
TEST(NbaEvent, ContextsDoNotShareSubscriptionsCountersOrOrigins) {
    Rig first, second;
    const auto a = first.c.scheduler().addEvent(), b = second.c.scheduler().addEvent();
    const std::array ai{named(a)}, bi{named(b)};
    ASSERT_TRUE(first.arm(1, ai, 11));
    ASSERT_TRUE(second.arm(2, bi, 22));
    first.c.scheduler().trigger(a);
    EXPECT_FALSE(first.c.scheduler().hasCallbackOrigins(first.p));
    EXPECT_TRUE(second.c.scheduler().hasCallbackOrigins(second.p));
    EXPECT_EQ(second.cleanups, 0U);
    second.c.scheduler().trigger(b);
    EXPECT_EQ(second.c.scheduler().nbaEventStats().live, 1U);
    second.c.scheduler().trigger(b);
    EXPECT_EQ(second.c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(first.output(), 0U);
    EXPECT_EQ(first.c.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(first.output(), 11U);
    EXPECT_EQ(second.output(), 22U);
}

struct CancelingCleanup {
    Rig* rig;
    k::EventId event;
    k::NbaTicket unbound;
    unsigned calls = 0;
    k::NbaError result = k::NbaError::None;
    static void run(void* raw) noexcept {
        auto& f = *static_cast<CancelingCleanup*>(raw);
        ++f.calls;
        auto& s = f.rig->c.scheduler();
        s.cancelNbaEvents();
        const k::W4 one{1, 0};
        const std::array items{named(f.event)};
        const auto result = s.registerNbaEvent(f.rig->p, f.unbound, {1, false, true, {&one, 1}},
                                               items, {&Rig::release, f.rig});
        f.result = result.error;
        if (result) static_cast<void>(s.discardNbaEvent(result.ticket));
    }
};
TEST(NbaEvent, CancelAllRejectsReentrantPublicationAndReturnsCleanupOwnershipOnFailure) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    const auto first = r.capture(), unbound = r.capture(71);
    ASSERT_TRUE(first);
    ASSERT_TRUE(unbound);
    CancelingCleanup f{&r, event, unbound.ticket};
    const k::W4 one{1, 0};
    const std::array items{named(event)};
    ASSERT_TRUE(s.registerNbaEvent(r.p, first.ticket, {1, false, true, {&one, 1}}, items,
                                   {&CancelingCleanup::run, &f}));
    s.cancelNbaEvents();
    EXPECT_EQ(f.calls, 1U);
    EXPECT_EQ(f.result, k::NbaError::InvalidPhase);
    EXPECT_EQ(r.cleanups, 0U);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
    EXPECT_EQ(s.discardNba(unbound.ticket), k::NbaError::None);
    ASSERT_TRUE(r.arm(1, items, 99));
    s.trigger(event);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 99U);
}
TEST(NbaEvent, SlotElementAndBitSelectionsIgnoreUnrelatedWrites) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto slot = r.c.store().add(k::Domain::Two, 129, 3, k::Init::Zero);
    std::vector<unsigned> seen;
    OrderedGuard guard{&seen, 1, true};
    const std::array items{k::NbaEventItem{
        k::WaitItem{{slot, 1, 1, 64, 2}, k::Edge::Any, &OrderedGuard::run, &guard}}};
    ASSERT_TRUE(r.arm(1, items));
    const std::uint64_t one = 1;
    s.write({.slot = slot, .elem = 0, .lsb = 64, .width = 1}, &one);
    s.write({.slot = slot, .elem = 2, .lsb = 64, .width = 1}, &one);
    s.write({.slot = slot, .elem = 1, .lsb = 63, .width = 1}, &one);
    s.write({.slot = slot, .elem = 1, .lsb = 66, .width = 1}, &one);
    EXPECT_TRUE(seen.empty());
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.write({.slot = slot, .elem = 1, .lsb = 65, .width = 1}, &one);
    EXPECT_EQ(seen, (std::vector<unsigned>{1}));
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(r.output(), 42U);
}
TEST(NbaEvent, RealAndHandleSlotsOnlyAcceptChangeEvents) {
    for (auto domain : {k::Domain::Real, k::Domain::Handle}) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto slot = r.c.store().add(domain, 64, 1, k::Init::Zero);
        const auto payload = r.capture();
        ASSERT_TRUE(payload);
        const k::W4 one{1, 0};
        const std::array pos{k::NbaEventItem{k::WaitItem{{slot}, k::Edge::Pos}}};
        EXPECT_EQ(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, pos).error,
                  k::NbaError::InvalidTarget);
        const std::array any{k::NbaEventItem{k::WaitItem{{slot}}}};
        ASSERT_TRUE(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, any));
        const std::uint64_t changed = 17;
        s.write({.slot = slot}, &changed);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(r.output(), 42U);
    }
}
struct StopGuard {
    Rig* rig;
    static int run(kirima_context*, void* raw) {
        auto& f = *static_cast<StopGuard*>(raw);
        f.rig->c.scheduler().requestEnd(k::EndReason::Finish, 0);
        return 1;
    }
};
TEST(NbaEvent, TerminationInsideGuardDoesNotPublishThePayloadOrCallLaterGuards) {
    Rig r;
    auto& s = r.c.scheduler();
    const auto event = s.addEvent();
    StopGuard stop{&r};
    std::vector<unsigned> seen;
    OrderedGuard after{&seen, 2, true};
    const std::array items{named(event, &StopGuard::run, &stop),
                           named(event, &OrderedGuard::run, &after)};
    ASSERT_TRUE(r.arm(1, items));
    s.trigger(event);
    EXPECT_TRUE(seen.empty());
    EXPECT_EQ(s.run(), k::EndReason::Finish);
    EXPECT_EQ(r.cleanups, 1U);
    EXPECT_EQ(r.output(), 0U);
    EXPECT_FALSE(s.hasCallbackOrigins(r.p));
}
struct FinalAttempt {
    Rig* rig;
    k::NbaTicket payload;
    k::EventId event;
    k::NbaError error = k::NbaError::None;
    unsigned calls = 0;
    static kirima_status resume(kirima_context*, void* raw) {
        auto& f = *static_cast<FinalAttempt*>(raw);
        ++f.calls;
        const k::W4 one{1, 0};
        const std::array items{named(f.event)};
        f.error = f.rig->c.scheduler()
                      .registerNbaEvent(f.rig->p, f.payload, {1, false, true, {&one, 1}}, items,
                                        {&Rig::release, f.rig})
                      .error;
        return KIRIMA_DONE;
    }
};
TEST(NbaEvent, TerminationAndFinalRejectRegistrationWithoutTakingCleanupOwnership) {
    for (bool terminate : {false, true}) {
        Rig r;
        auto& s = r.c.scheduler();
        const auto payload = r.capture();
        ASSERT_TRUE(payload);
        const auto event = s.addEvent();
        FinalAttempt attempt{&r, payload.ticket, event};
        const std::array finals{s.addProcess(&FinalAttempt::resume, &attempt, k::Group::Active)};
        if (terminate) {
            s.requestEnd(k::EndReason::Finish, 0);
            const k::W4 one{1, 0};
            const std::array items{named(event)};
            EXPECT_EQ(s.registerNbaEvent(r.p, payload.ticket, {1, false, true, {&one, 1}}, items,
                                         {&Rig::release, &r})
                          .error,
                      k::NbaError::InvalidPhase);
        }
        EXPECT_EQ(s.run(), terminate ? k::EndReason::Finish : k::EndReason::Quiescent);
        s.runFinal(finals);
        EXPECT_EQ(attempt.calls, 1U);
        EXPECT_EQ(attempt.error, k::NbaError::InvalidPhase);
        EXPECT_EQ(r.cleanups, 0U);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(r.output(), 0U);
    }
}
struct Enabled {
    bool value = false;
    static int guard(kirima_context*, void* raw) { return static_cast<Enabled*>(raw)->value; }
};
TEST(NbaEvent, RandomizedIndependentModelCountsOrItemsOnceAndPreservesIssueOrder) {
    const auto seed = kirimasim::test::fuzzSeed(0x6bb4a93dU);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    std::mt19937_64 random(seed);
    for (auto group : {k::Group::Active, k::Group::Reactive})
        for (std::uint64_t round = 0; round < 30 * kirimasim::test::fuzzScale(); ++round) {
            Rig r(group);
            auto& scheduler = r.c.scheduler();
            std::array<k::EventId, 4> events;
            for (auto& event : events)
                event = scheduler.addEvent();
            constexpr unsigned controls = 20;
            std::array<Enabled, controls> enabled;
            std::array<unsigned, controls> remaining;
            std::array<bool, controls> ready{};
            for (unsigned i = 0; i < controls; ++i) {
                remaining[i] = 1 + random() % 7;
                const std::array items{named(events[i % 4], &Enabled::guard, &enabled[i]),
                                       named(events[i % 4], &Enabled::guard, &enabled[i]),
                                       named(events[(i / 4) % 4], &Enabled::guard, &enabled[i])};
                ASSERT_TRUE(r.arm(remaining[i], items, 1000 + i));
            }
            for (unsigned notice = 0; notice < 50; ++notice) {
                const unsigned event = random() % events.size();
                unsigned live = 0;
                for (unsigned i = 0; i < controls; ++i) {
                    enabled[i].value = (random() & 1U) != 0;
                    if (remaining[i] != 0 && enabled[i].value &&
                        (i % 4 == event || (i / 4) % 4 == event)) {
                        --remaining[i];
                        ready[i] = remaining[i] == 0;
                    }
                    live += remaining[i] != 0;
                }
                scheduler.trigger(events[event]);
                EXPECT_EQ(scheduler.nbaEventStats().live, live);
                EXPECT_EQ(scheduler.hasCallbackOrigins(r.p), live != 0);
            }
            std::uint64_t expected = 0;
            for (unsigned i = 0; i < controls; ++i)
                if (ready[i]) expected = 1000 + i;
            EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
            EXPECT_EQ(r.output(), expected);
            EXPECT_EQ(r.cleanups, controls);
            EXPECT_FALSE(scheduler.hasCallbackOrigins(r.p));
        }
}
}  // namespace
