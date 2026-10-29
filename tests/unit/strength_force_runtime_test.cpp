// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Native lifecycle checks for the approved MOS/switch contract 5. These do not
// enable source force/release, SIR instructions or procedural override owners.
#include <array>
#include <csignal>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using R = k::StrengthRange;
using E = k::StrengthError;
using F = k::StrengthForceUpdate;

k::SlotId addStrengthSlot(k::Context& ctx, unsigned width) {
    const auto id = ctx.store().add(k::Domain::Four, width, 1, k::Init::Z);
    EXPECT_EQ(ctx.scheduler().enableStrengths(id, std::vector<R>(width)), E::None);
    return id;
}

void expectForce(const k::Context& ctx, unsigned node, std::optional<k::W4> expected) {
    const auto actual = ctx.strengthDrivers().forced(node);
    ASSERT_EQ(actual.has_value(), expected.has_value());
    if (actual && expected) {
        EXPECT_EQ(actual->a, expected->a);
        EXPECT_EQ(actual->b, expected->b);
    }
}

void expectSnapshot(const k::Context& ctx, k::SlotId slot, std::span<const R> ranges,
                    std::uint64_t a, std::uint64_t b) {
    std::vector<R> actual(ranges.size());
    EXPECT_EQ(ctx.strengths().read({slot, 0, 0, static_cast<unsigned>(ranges.size())}, actual),
              E::None);
    EXPECT_EQ(actual, (std::vector<R>(ranges.begin(), ranges.end())));
    const auto* words = ctx.store().element(slot, 0);
    const auto mask = (std::uint64_t{1} << ranges.size()) - 1;
    EXPECT_EQ(words[0] & mask, a);
    EXPECT_EQ(words[1] & mask, b);
}

void configurePair(k::Context& ctx, k::SlotId slot, R initial = {3, 3}) {
    const std::array<k::StrengthNodeSpec, 2> nodes{{{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}}};
    const k::StrengthOriginSpec origin{0, initial};
    const k::StrengthEdgeSpec edge{0, 1};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, {&origin, 1}, {&edge, 1}), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
}

void configureComponents(k::Context& ctx, k::SlotId slot) {
    const std::array<k::StrengthNodeSpec, 4> nodes{{{{slot, 0, 0, 1}, k::NetKind::Wire, 0},
                                                    {{slot, 0, 1, 1}, k::NetKind::Wire, 0},
                                                    {{slot, 0, 2, 1}, k::NetKind::Wire, 1},
                                                    {{slot, 0, 3, 1}, k::NetKind::Wire, 1}}};
    const std::array<k::StrengthOriginSpec, 2> origins{{{0, {3, 3}}, {2, {-3, -3}}}};
    const std::array<k::StrengthEdgeSpec, 2> edges{{{0, 1}, {2, 3}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins, edges), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
}

k::StrengthPublication force(k::Context& ctx, unsigned node, std::optional<k::W4> value) {
    const F update{node, value};
    return ctx.scheduler().setStrengthForces({&update, 1});
}

std::uint64_t events(const k::Context& ctx, const char* name) {
    for (std::uint32_t i = 0; i < ctx.counters().size(); ++i)
        if (ctx.counters().name(k::CounterId{i}) == name)
            return ctx.counters().value(k::CounterId{i});
    return 0;
}

TEST(StrengthForceRuntime, ForceClampsItsReadoutButDoesNotBlockTransitAndReleaseReadsCurrentRoots) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 3);
    const std::array<k::StrengthNodeSpec, 3> nodes{
        {{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}, {{slot, 0, 2, 1}}}};
    // The two origins at node 1 stand for ordinary and implicit local roots.
    const std::array<k::StrengthOriginSpec, 3> origins{{{0, {7, 7}}, {1, {-7, -7}}, {1, {-5, -5}}}};
    const std::array<k::StrengthEdgeSpec, 2> edges{
        {{0, 1, k::StrengthTransfer::Identity}, {1, 2, k::StrengthTransfer::Identity}}};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins, edges), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    expectSnapshot(ctx, slot, std::array<R, 3>{R{-7, 7}, R{-7, 7}, R{-7, 7}}, 7, 7);

    const auto forced = force(ctx, 1, k::W4{0, 1});
    ASSERT_EQ(forced.error, E::None);
    EXPECT_TRUE(forced.committed);
    EXPECT_TRUE(forced.changed);
    // Z is a direct readout clamp, while the remote supply still transits it.
    expectSnapshot(ctx, slot, std::array<R, 3>{R{7, 7}, R{}, R{7, 7}}, 5, 2);
    expectForce(ctx, 1, k::W4{0, 1});
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {}), E::None);
    ASSERT_EQ(ctx.scheduler().driveStrength(1, {4, 4}), E::None);
    ASSERT_EQ(ctx.scheduler().driveStrength(2, {2, 2}), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    expectSnapshot(ctx, slot, std::array<R, 3>{R{}, R{}, R{}}, 0, 7);

    const auto released = force(ctx, 1, std::nullopt);
    ASSERT_EQ(released.error, E::None);
    EXPECT_TRUE(released.committed);
    // This is the statement immediately after release: there is no run/settle.
    expectSnapshot(ctx, slot, std::array<R, 3>{R{4, 4}, R{4, 4}, R{4, 4}}, 7, 0);
    expectForce(ctx, 1, std::nullopt);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    expectSnapshot(ctx, slot, std::array<R, 3>{R{4, 4}, R{4, 4}, R{4, 4}}, 7, 0);
}

struct CoherentNotices {
    k::Context* ctx;
    k::SlotId slot;
    std::array<R, 4> expected;
    unsigned internal = 0, ordinary = 0, errors = 0;
    void inspect() noexcept {
        std::array<R, 4> actual;
        if (ctx->strengths().read({slot, 0, 0, 4}, actual) != E::None || actual != expected)
            ++errors;
        const auto* words = ctx->store().element(slot, 0);
        if ((words[0] & 15U) != 12U || (words[1] & 15U) != 0U) ++errors;
    }
    static void changed(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<CoherentNotices*>(raw);
        ++self.internal;
        self.inspect();
    }
    static std::int32_t guard(kirima_context*, void* raw) {
        auto& self = *static_cast<CoherentNotices*>(raw);
        ++self.ordinary;
        self.inspect();
        return 0;
    }
};

kirima_status finished(kirima_context*, void*) { return KIRIMA_DONE; }

TEST(StrengthForceRuntime, MultiComponentBatchCommitsEveryFieldBeforeEitherKindOfCallback) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 4);
    configureComponents(ctx, slot);
    CoherentNotices notice{&ctx, slot, {R{-6, -6}, R{-6, -6}, R{6, 6}, R{6, 6}}};
    ASSERT_EQ(ctx.strengths().subscribe({slot, 0, 0, 4}, &CoherentNotices::changed, &notice).error,
              E::None);
    const auto process = ctx.scheduler().addProcess(&finished, nullptr, k::Group::Active);
    const k::WaitItem item{{slot, 0, 1, 0, 4}, k::Edge::Any, &CoherentNotices::guard, &notice};
    ctx.scheduler().waitEvent(process, {&item, 1});
    const std::array<F, 2> updates{{{2, k::W4{1, 0}}, {0, k::W4{0, 0}}}};
    const auto result = ctx.scheduler().setStrengthForces(updates);
    ASSERT_EQ(result.error, E::None);
    EXPECT_TRUE(result.committed);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(notice.internal, 4U);
    EXPECT_EQ(notice.ordinary, 4U);
    EXPECT_EQ(notice.errors, 0U);
    expectSnapshot(ctx, slot, notice.expected, 12, 0);
}

struct Counts {
    unsigned internal = 0, ordinary = 0;
    static void changed(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        ++static_cast<Counts*>(raw)->internal;
    }
    static std::int32_t guard(kirima_context*, void* raw) {
        ++static_cast<Counts*>(raw)->ordinary;
        return 0;
    }
};

TEST(StrengthForceRuntime, StrengthOnlyForcingAndReleaseDoNotWakeValueWaitsAndRepeatsDoNothing) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 2);
    configurePair(ctx, slot);
    Counts notices;
    ASSERT_EQ(ctx.strengths().subscribe({slot, 0, 0, 2}, &Counts::changed, &notices).error,
              E::None);
    const auto process = ctx.scheduler().addProcess(&finished, nullptr, k::Group::Active);
    const k::WaitItem item{{slot, 0, 1, 0, 2}, k::Edge::Any, &Counts::guard, &notices};
    ctx.scheduler().waitEvent(process, {&item, 1});
    const auto first = force(ctx, 0, k::W4{1, 0});
    ASSERT_EQ(first.error, E::None);
    EXPECT_TRUE(first.committed);
    EXPECT_TRUE(first.changed);
    EXPECT_EQ(notices.internal, 2U);
    EXPECT_EQ(notices.ordinary, 0U);
    const auto same = force(ctx, 0, k::W4{1, 0});
    EXPECT_EQ(same.error, E::None);
    EXPECT_TRUE(same.committed);
    EXPECT_FALSE(same.changed);
    EXPECT_EQ(notices.internal, 2U);
    const auto released = force(ctx, 0, std::nullopt);
    EXPECT_EQ(released.error, E::None);
    EXPECT_TRUE(released.committed);
    EXPECT_TRUE(released.changed);
    EXPECT_EQ(notices.internal, 4U);
    EXPECT_EQ(notices.ordinary, 0U);
    const auto again = force(ctx, 0, std::nullopt);
    EXPECT_EQ(again.error, E::None);
    EXPECT_TRUE(again.committed);
    EXPECT_FALSE(again.changed);
    EXPECT_EQ(notices.internal, 4U);
}

struct Reentry {
    k::Context* ctx;
    k::SlotId slot;
    unsigned calls = 0, errors = 0;
    E nested = E::None;
    static void changed(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<Reentry*>(raw);
        ++self.calls;
        const F update{0, k::W4{1, 0}};
        const auto result = self.ctx->scheduler().setStrengthForces({&update, 1});
        self.nested = result.error;
        if (result.error != E::Reentrant || result.committed || result.changed) ++self.errors;
        if (self.calls <= 2) {
            std::array<R, 2> current;
            if (self.ctx->strengths().read({self.slot, 0, 0, 2}, current) != E::None ||
                current != std::array<R, 2>{R{-6, -6}, R{-6, -6}})
                ++self.errors;
        }
        if (self.calls == 1 && self.ctx->scheduler().driveStrength(0, {7, 7}) != E::None)
            ++self.errors;
    }
};

TEST(StrengthForceRuntime, ReentrantForceIsRejectedButCallbackDriverChangesQueueANewSolve) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 2);
    const std::array<k::StrengthNodeSpec, 2> nodes{{{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}}};
    const k::StrengthOriginSpec origin{1, {}};
    const k::StrengthEdgeSpec edge{0, 1};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, {&origin, 1}, {&edge, 1}), E::None);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    Reentry notice{&ctx, slot};
    ASSERT_EQ(ctx.strengths().subscribe({slot, 0, 0, 2}, &Reentry::changed, &notice).error,
              E::None);
    ASSERT_EQ(force(ctx, 0, k::W4{0, 0}).error, E::None);
    expectSnapshot(ctx, slot, std::array<R, 2>{R{-6, -6}, R{-6, -6}}, 0, 0);
    EXPECT_EQ(notice.calls, 2U);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    expectSnapshot(ctx, slot, std::array<R, 2>{R{-6, -6}, R{7, 7}}, 2, 0);
    EXPECT_EQ(notice.calls, 3U);
    EXPECT_EQ(notice.errors, 0U);
    EXPECT_EQ(notice.nested, E::Reentrant);
    expectForce(ctx, 0, k::W4{0, 0});
}

struct PendingScript {
    k::ProcessId process{0};
    k::SlotId slot;
    std::uint64_t delay;
    unsigned step = 0;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& self = *static_cast<PendingScript*>(frame);
        auto& ctx = k::fromC(raw);
        auto& scheduler = ctx.scheduler();
        if (self.step++ == 0) {
            EXPECT_EQ(force(ctx, 1, k::W4{0, 0}).error, E::None);
            EXPECT_EQ(scheduler.driveStrengthDelayed(0, {7, 7}, self.delay, self.delay, self.delay),
                      E::None);
            EXPECT_EQ(scheduler.driveStrengthDelayed(1, {5, 5}, self.delay, self.delay, self.delay),
                      E::None);
            EXPECT_EQ(scheduler.setStrengthEdgeDelayed(0, k::StrengthConduction::Off, self.delay,
                                                       self.delay),
                      E::None);
            // Changing the visible force again must leave already pending
            // ordinary-origin and conduction transactions intact.
            EXPECT_EQ(force(ctx, 1, k::W4{1, 1}).error, E::None);
            EXPECT_EQ(force(ctx, 1, k::W4{0, 0}).error, E::None);
            const auto pendingOrigin = ctx.strengthDrivers().origin(0);
            const auto pendingEdge = ctx.strengthDrivers().edge(0);
            EXPECT_TRUE(pendingOrigin.has_value() && pendingOrigin->hasPending);
            EXPECT_TRUE(pendingEdge.has_value() && pendingEdge->hasPending);
            scheduler.waitDelay(self.process, self.delay + 1);
            return KIRIMA_WAITING;
        }
        const auto first = ctx.strengthDrivers().origin(0),
                   second = ctx.strengthDrivers().origin(1);
        const auto edge = ctx.strengthDrivers().edge(0);
        EXPECT_TRUE(first.has_value());
        EXPECT_TRUE(second.has_value());
        EXPECT_TRUE(edge.has_value());
        if (first && second && edge) {
            EXPECT_EQ(first->committed, (R{7, 7}));
            EXPECT_EQ(second->committed, (R{5, 5}));
            EXPECT_FALSE(first->hasPending);
            EXPECT_FALSE(second->hasPending);
            EXPECT_EQ(edge->committed, k::StrengthConduction::Off);
            EXPECT_FALSE(edge->hasPending);
        }
        expectSnapshot(ctx, self.slot, std::array<R, 2>{R{7, 7}, R{-6, -6}}, 1, 0);
        EXPECT_EQ(force(ctx, 1, std::nullopt).error, E::None);
        expectSnapshot(ctx, self.slot, std::array<R, 2>{R{7, 7}, R{5, 5}}, 3, 0);
        return KIRIMA_DONE;
    }
};

TEST(StrengthForceRuntime, DelayedOriginsAndConductionKeepAdvancingUnderForceInBothEvaluationSets) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        for (const auto delay : {0U, 2U}) {
            SCOPED_TRACE(static_cast<unsigned>(group));
            SCOPED_TRACE(delay);
            k::Context ctx;
            const auto slot = addStrengthSlot(ctx, 2);
            const std::array<k::StrengthNodeSpec, 2> nodes{{{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}}};
            const std::array<k::StrengthOriginSpec, 2> origins{{{0, {3, 3}}, {1, {-7, -7}}}};
            const k::StrengthEdgeSpec edge{0, 1};
            ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins, {&edge, 1}),
                      E::None);
            ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
            ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            PendingScript script{k::ProcessId{0}, slot, delay};
            script.process = ctx.scheduler().addProcess(&PendingScript::run, &script, group);
            ctx.scheduler().start(script.process);
            EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(script.step, 2U);
            expectForce(ctx, 1, std::nullopt);
        }
    }
}

struct StartupForce {
    k::SlotId slot;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& ctx = k::fromC(raw);
        auto& self = *static_cast<StartupForce*>(frame);
        EXPECT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
        EXPECT_EQ(ctx.scheduler().driveStrength(0, {3, 3}), E::None);
        EXPECT_EQ(force(ctx, 0, k::W4{0, 0}).error, E::None);
        EXPECT_EQ(ctx.scheduler().driveStrength(0, {7, 7}), E::None);
        EXPECT_EQ(force(ctx, 0, std::nullopt).error, E::None);
        expectSnapshot(ctx, self.slot, std::array<R, 1>{R{7, 7}}, 1, 0);
        return KIRIMA_DONE;
    }
};

TEST(StrengthForceRuntime, SynchronousForcingPreservesBothAlreadyQueuedEvaluationSets) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        SCOPED_TRACE(static_cast<unsigned>(group));
        k::Context ctx;
        const auto slot = addStrengthSlot(ctx, 1);
        const k::StrengthNodeSpec node{{slot, 0, 0, 1}};
        const k::StrengthOriginSpec origin{0, {}};
        ASSERT_EQ(ctx.scheduler().configureStrengthDrivers({&node, 1}, {&origin, 1}), E::None);
        StartupForce script{slot};
        const auto process = ctx.scheduler().addProcess(&StartupForce::run, &script, group);
        ctx.scheduler().start(process);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        expectSnapshot(ctx, slot, std::array<R, 1>{R{7, 7}}, 1, 0);
        // Initial resolution always belongs to Active; same-set driver updates
        // coalesce. Force/release must neither consume nor duplicate that work.
        EXPECT_EQ(events(ctx, "sched.events.active"), group == k::Group::Active ? 2U : 1U);
        EXPECT_EQ(events(ctx, "sched.events.reactive"), group == k::Group::Active ? 0U : 2U);
    }
}

TEST(StrengthForceRuntime, InvalidBatchesCannotPartiallyChangeAnExistingForceOrPublishedValue) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 2);
    const F first{0, k::W4{0, 0}};
    EXPECT_EQ(ctx.scheduler().setStrengthForces({&first, 1}).error, E::InvalidTarget);
    const std::array<k::StrengthNodeSpec, 2> nodes{{{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}}};
    const k::StrengthOriginSpec origin{0, {3, 3}};
    const k::StrengthEdgeSpec edge{0, 1};
    ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, {&origin, 1}, {&edge, 1}), E::None);
    EXPECT_EQ(ctx.scheduler().setStrengthForces({&first, 1}).error, E::InvalidTarget);
    ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(force(ctx, 0, k::W4{1, 0}).error, E::None);
    const auto empty = ctx.scheduler().setStrengthForces({});
    EXPECT_EQ(empty.error, E::InvalidShape);
    EXPECT_FALSE(empty.committed);
    const std::array<F, 2> duplicates{{{0, std::nullopt}, {0, k::W4{0, 0}}}};
    const auto duplicate = ctx.scheduler().setStrengthForces(duplicates);
    EXPECT_EQ(duplicate.error, E::InvalidShape);
    EXPECT_FALSE(duplicate.committed);
    for (const F invalid : {F{999, k::W4{0, 0}}, F{1, k::W4{2, 0}}, F{1, k::W4{0, 2}},
                            F{1, k::W4{UINT64_C(1) << 63, 1}}}) {
        const std::array<F, 2> updates{{{0, std::nullopt}, invalid}};
        const auto result = ctx.scheduler().setStrengthForces(updates);
        EXPECT_EQ(result.error, invalid.node == 999 ? E::InvalidTarget : E::InvalidValue);
        EXPECT_FALSE(result.committed);
        EXPECT_FALSE(result.changed);
        expectForce(ctx, 0, k::W4{1, 0});
        expectForce(ctx, 1, std::nullopt);
        expectSnapshot(ctx, slot, std::array<R, 2>{R{6, 6}, R{6, 6}}, 3, 0);
    }
    expectForce(ctx, 999, std::nullopt);
}

class ForceMemory final : public std::pmr::memory_resource {
public:
    std::size_t remaining = SIZE_MAX;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        if (remaining != SIZE_MAX) --remaining;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(StrengthForceRuntime, AllocationFailureBeforeCommitRollsBackTheWholeForceBankAndSnapshot) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 64; ++budget) {
        SCOPED_TRACE(budget);
        ForceMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        const auto slot = addStrengthSlot(ctx, 4);
        configureComponents(ctx, slot);
        ASSERT_EQ(force(ctx, 0, k::W4{1, 0}).error, E::None);
        const std::array<F, 2> updates{{{0, k::W4{0, 0}}, {2, k::W4{1, 0}}}};
        memory.remaining = budget;
        const auto result = ctx.scheduler().setStrengthForces(updates);
        memory.remaining = SIZE_MAX;
        if (result.error == E::ResourceLimit) {
            ++failures;
            EXPECT_FALSE(result.committed);
            expectForce(ctx, 0, k::W4{1, 0});
            expectForce(ctx, 2, std::nullopt);
            expectSnapshot(ctx, slot, std::array<R, 4>{R{6, 6}, R{6, 6}, R{-3, -3}, R{-3, -3}}, 3,
                           0);
        } else {
            ++successes;
            ASSERT_EQ(result.error, E::None);
            EXPECT_TRUE(result.committed);
            expectForce(ctx, 0, k::W4{0, 0});
            expectForce(ctx, 2, k::W4{1, 0});
            expectSnapshot(ctx, slot, std::array<R, 4>{R{-6, -6}, R{-6, -6}, R{6, 6}, R{6, 6}}, 12,
                           0);
        }
        const auto first = ctx.strengthDrivers().origin(0),
                   second = ctx.strengthDrivers().origin(1);
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        const auto firstValue = first.value_or(k::StrengthOriginState{});
        const auto secondValue = second.value_or(k::StrengthOriginState{});
        EXPECT_EQ(firstValue.committed, (R{3, 3}));
        EXPECT_EQ(secondValue.committed, (R{-3, -3}));
        EXPECT_FALSE(firstValue.hasPending);
        EXPECT_FALSE(secondValue.hasPending);
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}

std::int32_t failOrdinaryGuard(kirima_context*, void*) { throw std::bad_alloc(); }

TEST(StrengthForceRuntime, FailureInAnOrdinaryCallbackKeepsACommittedOverrideAndCompleteReadout) {
    k::Context ctx;
    const auto slot = addStrengthSlot(ctx, 4);
    configureComponents(ctx, slot);
    const auto process = ctx.scheduler().addProcess(&finished, nullptr, k::Group::Active);
    const k::WaitItem item{{slot, 0, 1, 0, 1}, k::Edge::Any, &failOrdinaryGuard, nullptr};
    ctx.scheduler().waitEvent(process, {&item, 1});
    const std::array<F, 2> updates{{{0, k::W4{0, 0}}, {2, k::W4{1, 0}}}};
    const auto result = ctx.scheduler().setStrengthForces(updates);
    EXPECT_EQ(result.error, E::ResourceLimit);
    EXPECT_TRUE(result.committed);
    EXPECT_TRUE(result.changed);
    expectForce(ctx, 0, k::W4{0, 0});
    expectForce(ctx, 2, k::W4{1, 0});
    expectSnapshot(ctx, slot, std::array<R, 4>{R{-6, -6}, R{-6, -6}, R{6, 6}, R{6, 6}}, 12, 0);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::ResourceLimit);
}

TEST(StrengthForceRuntime, InterruptCannotCommitAForceAndContextsKeepIndependentOverrides) {
    k::Context first, second;
    const auto a = addStrengthSlot(first, 2), b = addStrengthSlot(second, 2);
    configurePair(first, a);
    configurePair(second, b);
    ASSERT_EQ(force(first, 0, k::W4{0, 0}).error, E::None);
    expectSnapshot(first, a, std::array<R, 2>{R{-6, -6}, R{-6, -6}}, 0, 0);
    expectSnapshot(second, b, std::array<R, 2>{R{3, 3}, R{3, 3}}, 3, 0);
    expectForce(second, 0, std::nullopt);
    volatile std::sig_atomic_t interrupted = 1;
    second.scheduler().setInterruptFlag(&interrupted);
    const auto result = force(second, 0, k::W4{1, 1});
    EXPECT_EQ(result.error, E::Interrupted);
    EXPECT_FALSE(result.committed);
    EXPECT_FALSE(result.changed);
    expectForce(second, 0, std::nullopt);
    expectSnapshot(second, b, std::array<R, 2>{R{3, 3}, R{3, 3}}, 3, 0);
    expectForce(first, 0, k::W4{0, 0});
    EXPECT_EQ(second.scheduler().run(), k::EndReason::Interrupted);
}
}  // namespace
