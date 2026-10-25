// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original component-publication checks: approved MOS/switch contract
// 1/6/7, SCHEDULER_SPEC 9/12 and VALUE_SEMANTICS 5.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory_resource>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/logic.h"
#include "kirimasim/valsem/strength.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using R = k::StrengthRange;
using E = k::StrengthError;
constexpr std::array<R, 4> levels{{{-6, -6}, {6, 6}, {0, 0}, {-6, 6}}};
std::vector<R> read(const k::Context& ctx, k::SlotBits at) {
    const auto width = at.width == 0 ? ctx.store().layout(at.slot).width : at.width;
    std::vector<R> values(width);
    EXPECT_EQ(ctx.strengths().read(at, values), E::None);
    return values;
}
k::SlotId add(k::Context& ctx, unsigned width = 1, unsigned elements = 1, R range = {-6, 6}) {
    const auto value = k::strengthValue(range);
    const auto init = value.b   ? (value.a ? k::Init::X : k::Init::Z)
                      : value.a ? k::Init::One
                                : k::Init::Zero;
    const auto slot = ctx.store().add(k::Domain::Four, width, elements, init);
    std::vector<R> initial(std::size_t{width} * elements, range);
    EXPECT_EQ(ctx.scheduler().enableStrengths(slot, initial), E::None);
    return slot;
}
std::uint64_t counter(const k::Context& ctx, const char* name) {
    for (std::uint32_t i = 0; i < ctx.counters().size(); ++i)
        if (ctx.counters().name(k::CounterId{i}) == name)
            return ctx.counters().value(k::CounterId{i});
    return 0;
}
void coherent(const k::Context& ctx, k::SlotBits at, std::span<const R> expected) {
    EXPECT_EQ(read(ctx, at), std::vector<R>(expected.begin(), expected.end()));
    const auto* words = ctx.store().element(at.slot, at.elem);
    for (std::size_t bit = 0; bit < expected.size(); ++bit) {
        const auto value = v::valueOf({expected[bit].lo, expected[bit].hi});
        EXPECT_EQ(k::bitAt(words, 2, at.lsb + bit), value == v::Logic::One || value == v::Logic::X);
        EXPECT_EQ(k::bitAt(words + 1, 2, at.lsb + bit),
                  value == v::Logic::Z || value == v::Logic::X);
    }
}
k::StrengthPublication publish(k::Context& ctx, k::SlotBits at, std::span<const R> values) {
    const k::StrengthUpdate update{at, values};
    return ctx.scheduler().publishStrengths({&update, 1});
}
kirima_status idle(kirima_context*, void*) { return static_cast<kirima_status>(k::Status::Done); }
struct Notices {
    unsigned internal = 0, ordinary = 0;
    bool failure = false;
    std::vector<R> before, after;
    static void change(void* raw, k::SlotBits, std::span<const R> old,
                       std::span<const R> now) noexcept {
        auto& self = *static_cast<Notices*>(raw);
        ++self.internal;
        try {
            for (const auto value : old)
                self.before.push_back(value);
            for (const auto value : now)
                self.after.push_back(value);
        } catch (...) {
            self.failure = true;
        }
    }
    static std::int32_t guard(kirima_context*, void* raw) {
        ++static_cast<Notices*>(raw)->ordinary;
        return 0;
    }
};
void watchOrdinary(k::Context& ctx, k::SlotBits at, Notices& notices, k::Edge edge = k::Edge::Any) {
    const auto process = ctx.scheduler().addProcess(&idle, nullptr, k::Group::Active);
    const k::WaitItem item{
        {at.slot, at.elem, 1, at.lsb, at.width}, edge, &Notices::guard, &notices};
    ctx.scheduler().waitEvent(process, {&item, 1});
}

TEST(StrengthStore, RegistrationPreservesInitialValuesAndLoadsArrayPartsAndPackedTails) {
    for (const auto width : {1U, 7U, 8U, 9U, 33U, 65U, 129U})
        for (const auto elements : {1U, 3U}) {
            k::Context ctx;
            const auto slot = add(ctx, width, elements);
            EXPECT_EQ(counter(ctx, "slot.writes"), 0U);
            EXPECT_EQ(counter(ctx, "proc.created"), 0U);
            EXPECT_EQ(ctx.strengths().slotCount(), 1U);
            for (unsigned e = 0; e < elements; ++e) {
                std::vector<R> expected(width, {-6, 6});
                coherent(ctx, {slot, e, 0, width}, expected);
                const auto start = width > 32 ? 31U : 0U;
                const auto partWidth = width - start;
                std::vector<k::W4> packed(k::strengthWordCount(partWidth) + 1, {71, 73});
                ASSERT_EQ(ctx.strengths().readPacked({slot, e, start, partWidth}, packed), E::None);
                for (unsigned bit = 0; bit < partWidth; ++bit)
                    EXPECT_EQ(k::strengthRangeAt(packed, bit), std::optional<R>(R{-6, 6}));
                if (partWidth % 8 != 0) {
                    EXPECT_EQ(packed[packed.size() - 2].a >> (8 * (partWidth % 8)), 0U);
                    EXPECT_EQ(packed[packed.size() - 2].b >> (8 * (partWidth % 8)), 0U);
                }
                EXPECT_EQ(packed.back().a, 71U);
                EXPECT_EQ(packed.back().b, 73U);
            }
            EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            coherent(ctx, {slot, 0, 0, width}, std::vector<R>(width, {-6, 6}));
        }
}

TEST(StrengthStore, CompleteIntervalsSurvivePublicationInEveryBitAndArrayElement) {
    std::vector<R> intervals;
    for (int lo = -7; lo <= 7; ++lo)
        for (int hi = lo; hi <= 7; ++hi)
            intervals.push_back({static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)});
    for (const auto width : {1U, 8U, 9U, 33U, 65U, 129U}) {
        k::Context ctx;
        const auto slot = add(ctx, width, 3, {0, 0});
        for (unsigned seed = 0; seed < 120; ++seed) {
            std::array<std::vector<R>, 3> ranges;
            std::array<k::StrengthUpdate, 3> updates;
            for (unsigned e = 0; e < 3; ++e) {
                for (unsigned bit = 0; bit < width; ++bit)
                    ranges[e].push_back(intervals[(seed + bit + 7 * e) % 120]);
                updates[e] = {{slot, e, 0, width}, ranges[e]};
            }
            const auto result = ctx.scheduler().publishStrengths(updates);
            ASSERT_EQ(result.error, E::None);
            ASSERT_TRUE(result.committed);
            for (unsigned e = 0; e < 3; ++e)
                coherent(ctx, {slot, e, 0, width}, ranges[e]);
        }
    }
}

TEST(StrengthStore, RangeOnlyChangesNotifyInternalsButDoNotWakeOrdinaryWaitsOrCountValueChanges) {
    k::Context ctx;
    const auto slot = add(ctx, 1, 1, {3, 3});
    Notices notices;
    notices.before.reserve(8);
    notices.after.reserve(8);
    const auto watched = ctx.strengths().subscribe({slot}, &Notices::change, &notices);
    ASSERT_EQ(watched.error, E::None);
    watchOrdinary(ctx, {slot}, notices);
    const auto changes = counter(ctx, "slot.changes");
    const std::array<R, 1> full{{{3, 6}}}, strong{{{6, 6}}};
    for (const auto& values : {full, strong}) {
        const auto result = publish(ctx, {slot}, values);
        ASSERT_EQ(result.error, E::None);
        EXPECT_TRUE(result.committed);
        EXPECT_TRUE(result.changed);
        coherent(ctx, {slot, 0, 0, 1}, values);
    }
    const auto unchanged = publish(ctx, {slot}, strong);
    EXPECT_EQ(unchanged.error, E::None);
    EXPECT_TRUE(unchanged.committed);
    EXPECT_FALSE(unchanged.changed);
    EXPECT_FALSE(notices.failure);
    EXPECT_EQ(notices.internal, 2U);
    EXPECT_EQ(notices.ordinary, 0U);
    EXPECT_EQ(counter(ctx, "slot.changes"), changes);
    EXPECT_EQ(notices.before, (std::vector<R>{{3, 3}, {3, 6}}));
    EXPECT_EQ(notices.after, (std::vector<R>{{3, 6}, {6, 6}}));
}

struct Coherence {
    k::Context* ctx = nullptr;
    std::array<k::SlotBits, 3> targets;
    std::array<std::vector<R>, 3> expected;
    unsigned internal = 0, ordinary = 0, late = 0;
    bool grew = false;
    E nested = E::None;
    void check() const {
        for (unsigned i = 0; i < 3; ++i)
            coherent(*ctx, targets[i], expected[i]);
    }
    static void lateCall(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        ++static_cast<Coherence*>(raw)->late;
    }
    static void changed(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<Coherence*>(raw);
        try {
            ++self.internal;
            self.check();
            if (self.grew) return;
            self.grew = true;
            // Grow both slot stores and the watch registry while callbacks
            // still have the original component's notification snapshots.
            for (unsigned i = 0; i < 128; ++i) {
                add(*self.ctx, 1, 1, {0, 0});
                EXPECT_EQ(self.ctx->strengths().subscribe(self.targets[0], &lateCall, &self).error,
                          E::None);
            }
            const auto result = publish(*self.ctx, self.targets[0], self.expected[0]);
            self.nested = result.error;
            EXPECT_FALSE(result.committed);
        } catch (...) {
            self.ctx->scheduler().resourceLimit();
        }
    }
    static std::int32_t guard(kirima_context*, void* raw) {
        auto& self = *static_cast<Coherence*>(raw);
        ++self.ordinary;
        self.check();
        return 0;
    }
};
TEST(StrengthStore, PublishesAllFieldsBeforeAnyCallbackDespiteRegistryGrowthAndReentry) {
    for (const bool reverse : {false, true}) {
        k::Context ctx;
        const auto a = add(ctx, 129, 3, {0, 0}), b = add(ctx, 33, 2, {0, 0});
        Coherence state;
        state.ctx = &ctx;
        state.targets = {{{a, 2, 63, 65}, {b, 0, 3, 17}, {a, 2, 0, 9}}};
        for (unsigned i = 0; i < 3; ++i) {
            state.expected[i].assign(state.targets[i].width, {3, 6});
            state.expected[i].back() = {-6, 6};
        }
        ASSERT_EQ(ctx.strengths().subscribe({a, 2}, &Coherence::changed, &state).error, E::None);
        ASSERT_EQ(ctx.strengths().subscribe({b}, &Coherence::changed, &state).error, E::None);
        const auto process = ctx.scheduler().addProcess(&idle, nullptr, k::Group::Active);
        const std::array<k::WaitItem, 2> watches{
            {{{a, 2, 1}, k::Edge::Any, &Coherence::guard, &state},
             {{b}, k::Edge::Any, &Coherence::guard, &state}}};
        ctx.scheduler().waitEvent(process, watches);
        Notices unrelated;
        watchOrdinary(ctx, {a, 2, 32, 8}, unrelated);
        std::array<k::StrengthUpdate, 3> updates;
        for (unsigned i = 0; i < 3; ++i)
            updates[i] = {state.targets[i], state.expected[i]};
        if (reverse) std::reverse(updates.begin(), updates.end());
        const auto result = ctx.scheduler().publishStrengths(updates);
        ASSERT_EQ(result.error, E::None);
        ASSERT_TRUE(result.committed);
        EXPECT_EQ(state.internal, 3U);
        EXPECT_EQ(state.ordinary, 3U);
        EXPECT_EQ(state.late, 0U);
        EXPECT_EQ(state.nested, E::Reentrant);
        EXPECT_EQ(unrelated.ordinary, 0U);
        state.check();
        // New watches become eligible for the next distinct publication.
        state.expected[0].assign(65, {-6, -6});
        ASSERT_EQ(publish(ctx, state.targets[0], state.expected[0]).error, E::None);
        EXPECT_EQ(state.late, 128U);
        state.check();
        ctx.strengths().clearWatches();
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
    }
}

TEST(StrengthStore, MalformedBatchesAndConflictingPartsLeaveAllFieldsAndNoticesUntouched) {
    for (unsigned bad = 0; bad < 11; ++bad) {
        k::Context ctx;
        const auto slot = add(ctx, 65, 2, {0, 0});
        Notices n;
        n.before.reserve(512);
        n.after.reserve(512);
        ASSERT_EQ(ctx.strengths().subscribe({slot}, &Notices::change, &n).error, E::None);
        watchOrdinary(ctx, {slot}, n);
        std::vector<R> first(33, {6, 6}), second(32, {-6, -6});
        std::array<k::StrengthUpdate, 2> updates{
            {{{slot, 0, 0, 33}, first}, {{slot, 0, 33, 32}, second}}};
        if (bad == 0) updates[1].at.slot = k::SlotId{UINT32_MAX};
        if (bad == 1) updates[1].at.elem = 2;
        if (bad == 2) updates[1].at.lsb = 34;
        if (bad == 3) updates[1].at.width = UINT32_MAX;
        if (bad == 4) updates[1].values = {};
        if (bad == 5) second.back() = {-8, 0};
        if (bad == 6) second.back() = {0, 8};
        if (bad == 7) second.back() = {7, -7};
        if (bad == 8) updates[1].at.lsb = 32;
        if (bad == 9) updates[1] = updates[0];
        if (bad == 10) updates[1].at.width = 0;
        const auto result = ctx.scheduler().publishStrengths(updates);
        EXPECT_NE(result.error, E::None);
        EXPECT_FALSE(result.committed);
        coherent(ctx, {slot, 0, 0, 65}, std::vector<R>(65, {0, 0}));
        coherent(ctx, {slot, 1, 0, 65}, std::vector<R>(65, {0, 0}));
        EXPECT_FALSE(n.failure);
        EXPECT_EQ(n.internal, 0U);
        EXPECT_EQ(n.ordinary, 0U);
        EXPECT_EQ(counter(ctx, "slot.writes"), 0U);
        first.assign(33, {6, 6});
        EXPECT_EQ(publish(ctx, {slot, 0, 0, 33}, first).error, E::None);
    }
}

TEST(StrengthStore, RegistrationRejectsProjectionMismatchUnsupportedDomainsAndExistingHolds) {
    for (unsigned bad = 0; bad < 8; ++bad) {
        k::Context ctx;
        const auto domain = bad == 0   ? k::Domain::Two
                            : bad == 1 ? k::Domain::Real
                            : bad == 2 ? k::Domain::Handle
                                       : k::Domain::Four;
        const auto slot = ctx.store().add(domain, 1, bad == 3 ? 0 : 1, k::Init::X);
        std::array<R, 1> initial{{{-6, 6}}};
        if (bad == 4) initial[0] = {6, 6};
        if (bad == 5) initial[0] = {-8, 6};
        if (bad == 6) ctx.scheduler().markHeld(slot, true, true);
        const auto result =
            ctx.scheduler().enableStrengths(bad == 7 ? k::SlotId{99} : slot, initial);
        EXPECT_NE(result, E::None);
        EXPECT_FALSE(ctx.strengths().registered(slot));
    }
    k::Context ctx;
    const auto slot = add(ctx);
    const std::array<R, 1> initial{{{-6, 6}}};
    EXPECT_EQ(ctx.scheduler().enableStrengths(slot, initial), E::InvalidTarget);
    std::array<R, 1> untouched{{{3, 6}}};
    EXPECT_EQ(ctx.strengths().read({slot, 1}, untouched), E::InvalidTarget);
    EXPECT_EQ(ctx.strengths().read({slot}, {}), E::InvalidShape);
    EXPECT_EQ(untouched[0], (R{3, 6}));
}

struct Memory : std::pmr::memory_resource {
    int failAt = -1, calls = 0;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (calls++ == failAt) throw std::bad_alloc();
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(StrengthStore, AllocationFailuresDuringRegistrationAndPublicationNeverExposePartialState) {
    unsigned registrationFailures = 0, publicationFailures = 0, successes = 0;
    for (unsigned stage = 0; stage < 2; ++stage)
        for (int failure = 0; failure < 32; ++failure) {
            Memory memory;
            k::Context ctx({stdout, stderr}, nullptr, &memory);
            const auto a = add(ctx, 65, 2, {0, 0});
            const auto b = stage == 0 ? ctx.store().add(k::Domain::Four, 33, 1, k::Init::X)
                                      : add(ctx, 33, 1, {0, 0});
            Notices n;
            n.before.reserve(256);
            n.after.reserve(256);
            ASSERT_EQ(ctx.strengths().subscribe({a}, &Notices::change, &n).error, E::None);
            memory.calls = 0;
            memory.failAt = failure;
            if (stage == 0) {
                const std::vector<R> initial(33, {-6, 6});
                const auto result = ctx.scheduler().enableStrengths(b, initial);
                if (result == E::ResourceLimit) {
                    ++registrationFailures;
                    EXPECT_FALSE(ctx.strengths().registered(b));
                    EXPECT_EQ(ctx.strengths().slotCount(), 1U);
                    memory.failAt = -1;
                    EXPECT_EQ(ctx.scheduler().enableStrengths(b, initial), E::None);
                } else
                    EXPECT_EQ(result, E::None);
            } else {
                const std::vector<R> av(65, {3, 6}), bv(33, {-6, -3});
                const std::array<k::StrengthUpdate, 2> updates{{{{a}, av}, {{b}, bv}}};
                const auto result = ctx.scheduler().publishStrengths(updates);
                if (result.error == E::ResourceLimit) {
                    ++publicationFailures;
                    EXPECT_FALSE(result.committed);
                    coherent(ctx, {a, 0, 0, 65}, std::vector<R>(65, {0, 0}));
                    coherent(ctx, {b, 0, 0, 33}, std::vector<R>(33, {0, 0}));
                    EXPECT_EQ(ctx.scheduler().reason(), k::EndReason::ResourceLimit);
                    EXPECT_EQ(counter(ctx, "slot.writes"), 0U);
                    EXPECT_FALSE(n.failure);
                    EXPECT_EQ(n.internal, 0U);
                } else {
                    ASSERT_EQ(result.error, E::None);
                    EXPECT_TRUE(result.committed);
                    ++successes;
                    coherent(ctx, {a, 0, 0, 65}, av);
                    coherent(ctx, {b, 0, 0, 33}, bv);
                }
            }
            memory.failAt = -1;
            coherent(ctx, {a, 1, 0, 65}, std::vector<R>(65, {0, 0}));
        }
    EXPECT_GE(registrationFailures, 3U);
    EXPECT_GE(publicationFailures, 8U);
    EXPECT_GT(successes, 0U);
}

TEST(StrengthStore, WatchCancellationReusesRowsWithoutAllocationAndStaleTokensCannotCancelNewRows) {
    Memory memory;
    k::Context ctx({stdout, stderr}, nullptr, &memory);
    const auto slot = add(ctx, 1, 1, {0, 0});
    Notices n;
    n.before.reserve(4);
    n.after.reserve(4);
    auto current = ctx.strengths().subscribe({slot}, &Notices::change, &n);
    ASSERT_EQ(current.error, E::None);
    for (unsigned i = 0; i < 512; ++i) {
        const auto previous = current.token;
        ASSERT_TRUE(ctx.strengths().cancel(previous));
        memory.calls = 0;
        memory.failAt = 0;
        current = ctx.strengths().subscribe({slot}, &Notices::change, &n);
        ASSERT_EQ(current.error, E::None);
        EXPECT_NE(previous.generation, current.token.generation);
        EXPECT_FALSE(ctx.strengths().cancel(previous));
        EXPECT_EQ(ctx.strengths().watchRows(), 1U);
        EXPECT_EQ(ctx.strengths().liveWatches(), 1U);
    }
    memory.failAt = -1;
    const std::array<R, 1> one{{{3, 6}}};
    ASSERT_EQ(publish(ctx, {slot}, one).error, E::None);
    EXPECT_FALSE(n.failure);
    EXPECT_EQ(n.internal, 1U);
    memory.failAt = memory.calls;
    ctx.strengths().clearWatches();
    EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
    EXPECT_FALSE(ctx.strengths().cancel(current.token));
}

TEST(StrengthStore, OrdinaryValueNoticesKeepAllSixteenTransitionAndEdgeRules) {
    for (unsigned from = 0; from < 4; ++from)
        for (unsigned to = 0; to < 4; ++to) {
            k::Context ctx;
            const auto slot = add(ctx, 1, 1, levels[from]);
            std::array<Notices, 4> n;
            const std::array<k::Edge, 4> edges{k::Edge::Any, k::Edge::Pos, k::Edge::Neg,
                                               k::Edge::Both};
            for (unsigned i = 0; i < edges.size(); ++i)
                watchOrdinary(ctx, {slot}, n[i], edges[i]);
            const std::array<R, 1> value{{levels[to]}};
            ASSERT_EQ(publish(ctx, {slot}, value).error, E::None);
            const auto a = static_cast<v::Logic>(from), b = static_cast<v::Logic>(to);
            EXPECT_EQ(n[0].ordinary, from != to);
            EXPECT_EQ(n[1].ordinary, v::isPosedge(a, b));
            EXPECT_EQ(n[2].ordinary, v::isNegedge(a, b));
            EXPECT_EQ(n[3].ordinary, v::isPosedge(a, b) || v::isNegedge(a, b));
        }
}

TEST(StrengthStore, LegacyWritesAndHoldsCannotBypassARegisteredPublisher) {
    for (const bool hold : {false, true}) {
        k::Context ctx;
        const auto slot = add(ctx, 1, 1, {3, 6});
        if (hold)
            ctx.scheduler().markHeld(slot, true, true);
        else {
            const std::array<std::uint64_t, 2> zero{};
            EXPECT_FALSE(ctx.scheduler().write({slot}, zero.data()));
        }
        EXPECT_EQ(ctx.scheduler().reason(), k::EndReason::Internal);
        coherent(ctx, {slot, 0, 0, 1}, std::array<R, 1>{{{3, 6}}});
    }
}

TEST(StrengthStore, StrengthOnlyPublicationProducesNoVcdChangeWithOrWithoutGlitches) {
    for (const bool glitches : {false, true}) {
        const auto file =
            std::filesystem::temp_directory_path() / "kirimasim_strength_publication.vcd";
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() { std::filesystem::remove(path); }
        } cleanup{file};
        k::Context ctx;
        const auto slot = add(ctx, 1, 1, {3, 3});
        k::DumpSetup setup;
        setup.scopes = {{"top", "top", k::DumpScopeKind::Module, UINT32_MAX}};
        setup.vars = {{0, "net", "", k::DumpType::Wire, 1, slot}};
        setup.waveFile = file.string();
        setup.glitches = glitches;
        setup.date = "2026-10-26 00:00:00 UTC";
        setup.version = "kirimasim test";
        setup.timescale = "1 ns";
        ctx.dump().setup(std::move(setup));
        ctx.dump().endSlot(ctx.scheduler());
        ctx.dump().flush();
        const auto contents = [&]() {
            std::ifstream in(file);
            std::stringstream text;
            text << in.rdbuf();
            return text.str();
        };
        const auto before = contents();
        ASSERT_NE(before.find("\n1!\n"), std::string::npos);
        const std::array<R, 1> ambiguous{{{3, 6}}}, zero{{{-6, -6}}};
        ASSERT_EQ(publish(ctx, {slot}, ambiguous).error, E::None);
        ctx.dump().endSlot(ctx.scheduler());
        ctx.dump().flush();
        EXPECT_EQ(contents(), before);
        ASSERT_EQ(publish(ctx, {slot}, zero).error, E::None);
        ctx.dump().endSlot(ctx.scheduler());
        ctx.dump().flush();
        EXPECT_NE(contents().find("\n0!\n"), std::string::npos);
        ctx.dump().finish(ctx.scheduler());
    }
}
struct Cancellation {
    k::Context* ctx = nullptr;
    k::SlotId slot{0};
    k::StrengthWatchToken victim, replacement;
    bool acted = false, canceled = false;
    E status = E::InvalidTarget;
    unsigned oldCalls = 0, newCalls = 0;
    static void oldCall(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        ++static_cast<Cancellation*>(raw)->oldCalls;
    }
    static void newCall(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        ++static_cast<Cancellation*>(raw)->newCalls;
    }
    static void first(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<Cancellation*>(raw);
        if (self.acted) return;
        self.acted = true;
        self.canceled = self.ctx->strengths().cancel(self.victim);
        const auto next = self.ctx->strengths().subscribe({self.slot}, &newCall, &self);
        self.status = next.error;
        self.replacement = next.token;
    }
};
TEST(StrengthStore, CallbackCancellationAndImmediateRowReuseWaitForTheNextPublication) {
    k::Context ctx;
    Cancellation state;
    state.ctx = &ctx;
    state.slot = add(ctx, 1, 1, {0, 0});
    ASSERT_EQ(ctx.strengths().subscribe({state.slot}, &Cancellation::first, &state).error, E::None);
    const auto victim = ctx.strengths().subscribe({state.slot}, &Cancellation::oldCall, &state);
    ASSERT_EQ(victim.error, E::None);
    state.victim = victim.token;
    const std::array<R, 1> one{{{3, 6}}}, zero{{{-6, -6}}};
    ASSERT_EQ(publish(ctx, {state.slot}, one).error, E::None);
    EXPECT_TRUE(state.canceled);
    EXPECT_EQ(state.status, E::None);
    EXPECT_EQ(state.oldCalls, 0U);
    EXPECT_EQ(state.newCalls, 0U);
    EXPECT_FALSE(ctx.strengths().cancel(state.victim));
    ASSERT_EQ(publish(ctx, {state.slot}, zero).error, E::None);
    EXPECT_EQ(state.newCalls, 1U);
    EXPECT_EQ(state.oldCalls, 0U);
    EXPECT_TRUE(ctx.strengths().cancel(state.replacement));
}

struct NestedPublications {
    k::SlotId slot{0};
    std::array<E, 2> results{E::InvalidTarget, E::InvalidTarget};
    static std::int32_t guard(kirima_context* raw, void* frame) {
        auto& self = *static_cast<NestedPublications*>(frame);
        auto& ctx = k::fromC(raw);
        const std::array<R, 1> one{{{3, 6}}}, zero{{{-6, -6}}};
        self.results = {publish(ctx, {self.slot}, one).error,
                        publish(ctx, {self.slot}, zero).error};
        return 0;
    }
};
TEST(StrengthStore, DistinctCommittedTransitionsSurviveAnOuterNotificationWalk) {
    k::Context ctx;
    const auto slot = add(ctx, 1, 1, {-6, -6});
    const auto trigger = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
    Notices internal;
    internal.before.reserve(4);
    internal.after.reserve(4);
    ASSERT_EQ(ctx.strengths().subscribe({slot}, &Notices::change, &internal).error, E::None);
    std::array<Notices, 3> events;
    watchOrdinary(ctx, {slot}, events[0]);
    watchOrdinary(ctx, {slot}, events[1], k::Edge::Pos);
    watchOrdinary(ctx, {slot}, events[2], k::Edge::Neg);
    NestedPublications nested;
    nested.slot = slot;
    const auto process = ctx.scheduler().addProcess(&idle, nullptr, k::Group::Active);
    const k::WaitItem item{{trigger}, k::Edge::Any, &NestedPublications::guard, &nested};
    ctx.scheduler().waitEvent(process, {&item, 1});
    const std::array<std::uint64_t, 2> one{{1, 0}};
    ASSERT_TRUE(ctx.scheduler().write({trigger}, one.data()));
    EXPECT_EQ(nested.results, (std::array<E, 2>{E::None, E::None}));
    EXPECT_EQ(events[0].ordinary, 2U);
    EXPECT_EQ(events[1].ordinary, 1U);
    EXPECT_EQ(events[2].ordinary, 1U);
    EXPECT_FALSE(internal.failure);
    EXPECT_EQ(internal.before, (std::vector<R>{{-6, -6}, {3, 6}}));
    EXPECT_EQ(internal.after, (std::vector<R>{{3, 6}, {-6, -6}}));
    coherent(ctx, {slot, 0, 0, 1}, std::array<R, 1>{{{-6, -6}}});
}

std::int32_t allocationFailure(kirima_context*, void*) { throw std::bad_alloc(); }
TEST(StrengthStore, CallbackAllocationFailureReportsACompleteCommitAndDropsPendingNotices) {
    k::Context ctx;
    const auto a = add(ctx, 65, 1, {0, 0}), b = add(ctx, 33, 1, {0, 0});
    const auto process = ctx.scheduler().addProcess(&idle, nullptr, k::Group::Active);
    const k::WaitItem item{{a}, k::Edge::Any, &allocationFailure, nullptr};
    ctx.scheduler().waitEvent(process, {&item, 1});
    Notices later;
    watchOrdinary(ctx, {b}, later);
    const std::vector<R> av(65, {3, 6}), bv(33, {-6, -3});
    const std::array<k::StrengthUpdate, 2> updates{{{{a}, av}, {{b}, bv}}};
    const auto result = ctx.scheduler().publishStrengths(updates);
    ASSERT_EQ(result.error, E::ResourceLimit);
    EXPECT_TRUE(result.committed);
    coherent(ctx, {a, 0, 0, 65}, av);
    coherent(ctx, {b, 0, 0, 33}, bv);
    EXPECT_EQ(later.ordinary, 0U);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::ResourceLimit);
    EXPECT_EQ(later.ordinary, 0U);
    const k::StrengthUpdate invalid{{k::SlotId{999}}, {}};
    EXPECT_EQ(ctx.scheduler().publishStrengths({&invalid, 1}).error, E::InvalidTarget);
}

void endPublication(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
    static_cast<k::Context*>(raw)->scheduler().requestEnd(k::EndReason::Finish, 0);
}
TEST(StrengthStore, AStopDuringInternalNotificationSkipsLaterCallbacksWithoutPartialPublication) {
    k::Context ctx;
    const auto slot = add(ctx, 1, 1, {0, 0});
    ASSERT_EQ(ctx.strengths().subscribe({slot}, &endPublication, &ctx).error, E::None);
    Notices n;
    n.before.reserve(2);
    n.after.reserve(2);
    ASSERT_EQ(ctx.strengths().subscribe({slot}, &Notices::change, &n).error, E::None);
    watchOrdinary(ctx, {slot}, n);
    const std::array<R, 1> value{{{3, 6}}};
    EXPECT_TRUE(publish(ctx, {slot}, value).committed);
    coherent(ctx, {slot, 0, 0, 1}, value);
    EXPECT_EQ(ctx.scheduler().reason(), k::EndReason::Finish);
    EXPECT_EQ(n.internal, 0U);
    EXPECT_EQ(n.ordinary, 0U);
}

TEST(StrengthStore, ContextsOwnIndependentPublishedValuesAndWatchRegistries) {
    k::Context a, b;
    const auto as = add(a, 1, 1, {0, 0}), bs = add(b, 1, 1, {3, 6});
    Notices an, bn;
    an.before.reserve(2);
    an.after.reserve(2);
    bn.before.reserve(2);
    bn.after.reserve(2);
    ASSERT_EQ(a.strengths().subscribe({as}, &Notices::change, &an).error, E::None);
    ASSERT_EQ(b.strengths().subscribe({bs}, &Notices::change, &bn).error, E::None);
    const std::array<R, 1> value{{{-6, -3}}};
    ASSERT_EQ(publish(a, {as}, value).error, E::None);
    coherent(a, {as, 0, 0, 1}, value);
    coherent(b, {bs, 0, 0, 1}, std::array<R, 1>{{{3, 6}}});
    EXPECT_EQ(an.internal, 1U);
    EXPECT_EQ(bn.internal, 0U);
    a.strengths().clearWatches();
    EXPECT_EQ(b.strengths().liveWatches(), 1U);
    ASSERT_EQ(publish(b, {bs}, value).error, E::None);
    EXPECT_EQ(bn.internal, 1U);
    EXPECT_FALSE(an.failure);
    EXPECT_FALSE(bn.failure);
}
}  // namespace
