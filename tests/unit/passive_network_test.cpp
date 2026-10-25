// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original graph/world/timer matrices for approved MOS/switch contract 1/3/6/7.
// Reference enumerates root points and simple paths; kernel uses route maps.
#include <array>
#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/passive.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using R = k::StrengthRange;
using E = k::StrengthError;
using C = k::StrengthConduction;
using T = k::StrengthTransfer;
using Root = k::StrengthOriginSpec;
using Edge = k::StrengthEdgeSpec;
k::StrengthEdgeState edgeState(const k::Context& ctx, std::uint32_t id) {
    const auto value = ctx.strengthDrivers().edge(id);
    EXPECT_TRUE(value.has_value());
    return value.value_or(k::StrengthEdgeState{});
}
struct Network {
    k::Context ctx;
    k::SlotId slot;
    std::vector<k::StrengthNodeSpec> nodes;
    explicit Network(unsigned count, unsigned width = 0) : slot(make(count, width)) {
        for (unsigned n = 0; n < count; ++n)
            nodes.push_back({{slot, width == 0 ? 0 : n / width, width == 0 ? n : n % width, 1}});
    }
    k::SlotId make(unsigned count, unsigned width) {
        const auto id = ctx.store().add(k::Domain::Four, width == 0 ? count : width,
                                        width == 0 ? 1 : count / width, k::Init::Z);
        EXPECT_EQ(ctx.scheduler().enableStrengths(id, std::vector<R>(count)), E::None);
        return id;
    }
    R read(unsigned n) const {
        R value;
        EXPECT_EQ(ctx.strengths().read(nodes[n].at, {&value, 1}), E::None);
        return value;
    }
    void compare(std::span<const Root> roots, std::span<const Edge> edges) {
        std::vector<v::NetKind> kinds;
        std::vector<v::PassiveRoot> referenceRoots;
        std::vector<v::PassiveEdge> referenceEdges;
        kinds.reserve(nodes.size());
        referenceRoots.reserve(roots.size());
        referenceEdges.reserve(edges.size());
        for (const auto& n : nodes)
            kinds.push_back(static_cast<v::NetKind>(n.kind));
        for (const auto& r : roots)
            referenceRoots.push_back({r.node, {r.initial.lo, r.initial.hi}});
        for (const auto& e : edges)
            referenceEdges.push_back({e.first, e.second,
                                      static_cast<v::StrengthTransfer>(e.transfer),
                                      static_cast<v::Conduction>(e.initial)});
        const auto expected = v::resolvePassive(kinds, referenceRoots, referenceEdges);
        if (!expected.has_value()) {
            ADD_FAILURE() << "reference rejected valid input";
            return;
        }
        for (unsigned n = 0; n < nodes.size(); ++n)
            EXPECT_EQ(read(n), (R{static_cast<std::int8_t>((*expected)[n].lo),
                                  static_cast<std::int8_t>((*expected)[n].hi)}))
                << n;
    }
    void start(std::span<const Root> roots, std::span<const Edge> edges) {
        ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, roots, edges), E::None);
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    }
};
std::uint64_t count(const k::Context& ctx, const char* name) {
    for (std::uint32_t i = 0; i < ctx.counters().size(); ++i)
        if (ctx.counters().name(k::CounterId{i}) == name)
            return ctx.counters().value(k::CounterId{i});
    return 0;
}
struct Notice {
    k::Context* ctx = nullptr;
    std::array<std::uint64_t, 32> times{};
    std::array<R, 32> values{};
    unsigned used = 0;
    static void record(void* raw, k::SlotBits, std::span<const R>,
                       std::span<const R> after) noexcept {
        auto& self = *static_cast<Notice*>(raw);
        if (self.used < self.times.size()) {
            self.times[self.used] = self.ctx->scheduler().time();
            self.values[self.used] = after[0];
        }
        ++self.used;
    }
};
void observe(Network& n, unsigned node, Notice& notice) {
    notice.ctx = &n.ctx;
    ASSERT_EQ(n.ctx.strengths().subscribe(n.nodes[node].at, &Notice::record, &notice).error,
              E::None);
}
struct Action {
    std::uint64_t at = 0;
    C value = C::Off;
    std::uint64_t on = 0, off = 0;
    bool direct = false, root = false;
    R range;
    std::uint32_t id = 0;
};
struct Script {
    k::ProcessId process{0};
    std::vector<Action> actions;
    std::size_t next = 0;
    unsigned errors = 0;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& self = *static_cast<Script*>(frame);
        auto& scheduler = k::fromC(raw).scheduler();
        while (self.next < self.actions.size()) {
            const auto a = self.actions[self.next];
            if (scheduler.time() < a.at) {
                scheduler.waitDelay(self.process, a.at - scheduler.time());
                return static_cast<kirima_status>(k::Status::Waiting);
            }
            ++self.next;
            const auto status = a.root ? scheduler.driveStrength(a.id, a.range)
                                : a.direct
                                    ? scheduler.setStrengthEdge(a.id, a.value)
                                    : scheduler.setStrengthEdgeDelayed(a.id, a.value, a.on, a.off);
            if (status != E::None) ++self.errors;
        }
        return static_cast<kirima_status>(k::Status::Done);
    }
    void start(k::Context& ctx) {
        process = ctx.scheduler().addProcess(&run, this, k::Group::Active);
        ctx.scheduler().start(process);
    }
};

TEST(PassiveNetwork, AllIntervalsAndOpposingPointsMatchIndependentDestinationKinds) {
    unsigned checked = 0;
    for (const auto kind : {k::NetKind::Wire, k::NetKind::Tri, k::NetKind::Wand, k::NetKind::Triand,
                            k::NetKind::Wor, k::NetKind::Trior}) {
        Network n(3);
        n.nodes[1].kind = kind;
        std::array<Root, 2> roots{{{0, {}}, {2, {}}}};
        const std::array<Edge, 3> edges{{{0, 1, T::Identity, C::Uncertain},
                                         {1, 2, T::Nonresistive, C::On},
                                         {0, 2, T::Resistive, C::Uncertain}}};
        n.start(roots, edges);
        for (int lo = -7; lo <= 7; ++lo)
            for (int hi = lo; hi <= 7; ++hi)
                for (int point = -7; point <= 7; ++point) {
                    roots[0].initial = {static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)};
                    roots[1].initial = {static_cast<std::int8_t>(point),
                                        static_cast<std::int8_t>(point)};
                    ASSERT_EQ(n.ctx.scheduler().driveStrength(0, roots[0].initial), E::None);
                    ASSERT_EQ(n.ctx.scheduler().driveStrength(1, roots[1].initial), E::None);
                    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
                    n.compare(roots, edges);
                    ++checked;
                }
    }
    EXPECT_EQ(checked, 10800U);
}

TEST(PassiveNetwork, EveryThreeEdgeTransferAndControlCombinationPreservesReconvergence) {
    unsigned checked = 0;
    for (unsigned maps = 0; maps < 27; ++maps) {
        Network n(4);
        n.nodes[1].kind = k::NetKind::Wand;
        n.nodes[2].kind = k::NetKind::Wor;
        std::array<Edge, 4> edges{{{0, 1}, {1, 3}, {0, 2}, {2, 3, T::Resistive}}};
        auto code = maps;
        for (unsigned e = 0; e < 3; ++e) {
            edges[e].transfer = static_cast<T>(code % 3);
            code /= 3;
        }
        std::array<Root, 2> roots{{{0, {-7, 7}}, {3, {}}}};
        n.start(roots, edges);
        for (unsigned controls = 0; controls < 27; ++controls) {
            code = controls;
            for (unsigned e = 0; e < 3; ++e) {
                edges[e].initial = static_cast<C>(code % 3);
                code /= 3;
                ASSERT_EQ(n.ctx.scheduler().setStrengthEdge(e, edges[e].initial), E::None);
            }
            for (const int point : {-6, -1, 0, 1, 6}) {
                roots[1].initial = {static_cast<std::int8_t>(point),
                                    static_cast<std::int8_t>(point)};
                ASSERT_EQ(n.ctx.scheduler().driveStrength(1, roots[1].initial), E::None);
                ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
                n.compare(roots, edges);
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, 3645U);
}

TEST(PassiveNetwork, SharedUncertainEdgeResolvesBeforeWorldHullsAndNotifiesRangeOnly) {
    Network n(2);
    n.nodes[1].kind = k::NetKind::Wor;
    const std::array<Root, 3> roots{{{0, {-6, -6}}, {0, {6, 6}}, {1, {1, 1}}}};
    std::array<Edge, 1> edges{{{0, 1, T::Identity, C::On}}};
    n.start(roots, edges);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(1), (R{6, 6}));
    Notice notice;
    observe(n, 1, notice);
    const auto writes = count(n.ctx, "slot.writes");
    const auto changes = count(n.ctx, "slot.changes");
    ASSERT_EQ(n.ctx.scheduler().setStrengthEdge(0, C::Uncertain), E::None);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(1), (R{1, 6}));
    EXPECT_EQ(k::strengthValue(n.read(1)).a, 1U);
    EXPECT_EQ(k::strengthValue(n.read(1)).b, 0U);
    EXPECT_EQ(notice.used, 1U);
    EXPECT_EQ(count(n.ctx, "slot.writes"), writes + 2);
    EXPECT_EQ(count(n.ctx, "slot.changes"), changes);
    edges[0].initial = C::Uncertain;
    n.compare(roots, edges);
}

TEST(PassiveNetwork, IntermediateWiredReadoutNeverBecomesAnotherPhysicalRoot) {
    for (const auto kind : {k::NetKind::Wand, k::NetKind::Wor}) {
        Network n(3);
        n.nodes[1].kind = kind;
        const std::array<Root, 2> roots{{{0, {-6, -6}}, {2, {6, 6}}}};
        const std::array<Edge, 2> edges{{{0, 1, T::Identity}, {1, 2, T::Identity}}};
        n.start(roots, edges);
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(n.read(0), (R{-6, 6}));
        EXPECT_EQ(n.read(1), kind == k::NetKind::Wand ? (R{-6, -6}) : (R{6, 6}));
        EXPECT_EQ(n.read(2), (R{-6, 6}));
        n.compare(roots, edges);
    }
}

struct AtomicRing {
    Network* net;
    unsigned calls = 0, errors = 0;
    static void notice(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<AtomicRing*>(raw);
        ++self.calls;
        for (unsigned n = 0; n < 3; ++n) {
            const auto expected = self.calls <= 3 ? (n == 0 ? R{7, 7} : R{6, 6}) : R{};
            R actual;
            if (self.net->ctx.strengths().read(self.net->nodes[n].at, {&actual, 1}) != E::None ||
                actual != expected)
                ++self.errors;
        }
        if (self.calls == 1 && self.net->ctx.scheduler().driveStrength(0, {}) != E::None)
            ++self.errors;
    }
};
TEST(PassiveNetwork, RingLosesLastRootAndCallbackUpdatesKeepWholeSnapshotsDistinct) {
    Network n(3);
    const Root root{0, {7, 7}};
    const std::array<Edge, 3> edges{
        {{0, 1, T::Nonresistive}, {1, 2, T::Identity}, {2, 0, T::Resistive}}};
    n.start({&root, 1}, edges);
    AtomicRing ring{&n};
    ASSERT_EQ(n.ctx.strengths().subscribe({n.slot, 0, 0, 3}, &AtomicRing::notice, &ring).error,
              E::None);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ring.calls, 6U);
    EXPECT_EQ(ring.errors, 0U);
    for (unsigned bit = 0; bit < 3; ++bit)
        EXPECT_EQ(n.read(bit), R{});
    EXPECT_EQ(count(n.ctx, "proc.created"), 0U);
}

TEST(PassiveNetwork, LongResistivePathsCrossPackedAndElementBoundariesBothWays) {
    Network n(130, 65);
    std::vector<Edge> edges;
    for (unsigned bit = 1; bit < 130; ++bit)
        edges.push_back({bit - 1, bit, T::Resistive});
    // A self edge cannot attenuate the origin's own local contribution.
    edges.push_back({0, 0, T::Nonresistive});
    const std::array<Root, 2> roots{{{0, {7, 7}}, {129, {}}}};
    n.start(roots, edges);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(0), (R{7, 7}));
    for (const auto bit : {63U, 64U, 65U, 129U})
        EXPECT_EQ(n.read(bit), (R{1, 1}));
    ASSERT_EQ(n.ctx.scheduler().driveStrength(0, {}), E::None);
    ASSERT_EQ(n.ctx.scheduler().driveStrength(1, {-7, -7}), E::None);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(129), (R{-7, -7}));
    for (const auto bit : {0U, 63U, 64U, 65U})
        EXPECT_EQ(n.read(bit), (R{-1, -1}));
}

TEST(PassiveNetwork, UwireCountsPotentialDistinctOriginsNotMultiplePathsOrInitialOff) {
    Network n(3);
    n.nodes[2].kind = k::NetKind::Uwire;
    const std::array<Edge, 3> edges{
        {{0, 1, T::Identity}, {1, 2, T::Resistive}, {0, 2, T::Nonresistive}}};
    const Root root{0, {7, 7}};
    n.start({&root, 1}, edges);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(2), (R{6, 6}));
    EXPECT_EQ(n.ctx.strengthDrivers().originCount(), 1U);
    EXPECT_EQ(n.ctx.strengthDrivers().edgeCount(), 3U);
    EXPECT_FALSE(n.ctx.strengthDrivers().edge(3).has_value());
    Network bad(2);
    bad.nodes[1].kind = k::NetKind::Uwire;
    const std::array<Root, 2> roots{{{0, {}}, {1, {}}}};
    const Edge off{0, 1, T::Identity, C::Off};
    EXPECT_EQ(bad.ctx.scheduler().configureStrengthDrivers(bad.nodes, roots, {&off, 1}),
              E::InvalidTarget);
    EXPECT_FALSE(bad.ctx.strengthDrivers().configured());
    EXPECT_FALSE(v::resolvePassive(std::array{v::NetKind::Wire, v::NetKind::Uwire},
                                   std::array<v::PassiveRoot, 2>{{{0, {}}, {1, {}}}},
                                   std::array<v::PassiveEdge, 1>{
                                       {{0, 1, v::StrengthTransfer::Identity, v::Conduction::Off}}})
                     .has_value());
}

TEST(PassiveNetwork, DisconnectedRootsInOnePublicationComponentDoNotViolateUwire) {
    Network n(3);
    n.nodes[0].kind = k::NetKind::Uwire;
    const std::array<Root, 2> roots{{{0, {7, 7}}, {2, {-6, -6}}}};
    const Edge edge{0, 1, T::Identity};
    n.start(roots, {&edge, 1});
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(0), (R{7, 7}));
    EXPECT_EQ(n.read(1), (R{7, 7}));
    EXPECT_EQ(n.read(2), (R{-6, -6}));
}

TEST(PassiveNetwork, InvalidGraphConfigurationIsRetryableWithoutPublication) {
    for (unsigned mode = 0; mode < 6; ++mode) {
        Network n(2);
        const std::array<Root, 2> roots{{{0, {6, 6}}, {1, {}}}};
        Edge edge{0, 1, T::Identity};
        if (mode == 0) edge.first = 2;
        if (mode == 1) edge.second = 2;
        if (mode == 2) n.nodes[1].component = 1;
        if (mode == 3) edge.transfer = static_cast<T>(99);
        if (mode == 4) edge.initial = static_cast<C>(99);
        if (mode == 5) n.nodes[1].kind = k::NetKind::Uwire;
        EXPECT_NE(n.ctx.scheduler().configureStrengthDrivers(n.nodes, roots, {&edge, 1}), E::None);
        EXPECT_FALSE(n.ctx.strengthDrivers().configured());
        EXPECT_EQ(n.ctx.strengthDrivers().nodeCount(), 0U);
        EXPECT_EQ(n.ctx.strengthDrivers().originCount(), 0U);
        EXPECT_EQ(n.ctx.strengthDrivers().edgeCount(), 0U);
        EXPECT_EQ(n.read(0), R{});
        EXPECT_EQ(n.read(1), R{});
        n.nodes[1].component = 0;
        edge = {0, 1, T::Identity};
        n.start({roots.data(), 1}, {&edge, 1});
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(n.read(1), (R{6, 6}));
    }
}

class FailingMemory final : public std::pmr::memory_resource {
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
TEST(PassiveNetwork, GraphScratchAndPublicationAllocationFailuresNeverCommitHalfNetwork) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 48; ++budget) {
        FailingMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        const auto id = ctx.store().add(k::Domain::Four, 2, 1, k::Init::Z);
        ASSERT_EQ(ctx.scheduler().enableStrengths(id, std::array<R, 2>{}), E::None);
        const std::array<k::StrengthNodeSpec, 2> nodes{{{{id, 0, 0, 1}}, {{id, 0, 1, 1}}}};
        const Root root{0, {6, 6}};
        const Edge edge{0, 1, T::Identity, C::Uncertain};
        memory.remaining = budget;
        const auto error = ctx.scheduler().configureStrengthDrivers(nodes, {&root, 1}, {&edge, 1});
        memory.remaining = SIZE_MAX;
        if (error == E::ResourceLimit) {
            ++failures;
            EXPECT_FALSE(ctx.strengthDrivers().configured());
            ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, {&root, 1}, {&edge, 1}),
                      E::None);
        } else {
            ++successes;
            ASSERT_EQ(error, E::None);
        }
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
        memory.remaining = 0;
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::ResourceLimit);
        std::array<R, 2> values;
        EXPECT_EQ(ctx.strengths().read({id, 0, 0, 2}, values), E::None);
        EXPECT_EQ(values, (std::array<R, 2>{}));
        EXPECT_EQ(count(ctx, "slot.writes"), 0U);
        memory.remaining = SIZE_MAX;
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}

TEST(PassiveNetwork, ConfigurationOwnsNodeOriginAndEdgeTables) {
    Network n(2);
    {
        std::array<Root, 1> roots{{{0, {7, 7}}}};
        std::array<Edge, 1> edges{{{0, 1, T::Identity}}};
        auto nodes = n.nodes;
        ASSERT_EQ(n.ctx.scheduler().configureStrengthDrivers(nodes, roots, edges), E::None);
        nodes[0].at.lsb = 1;
        roots[0].initial = {-6, -6};
        edges[0].initial = C::Off;
        edges[0].transfer = T::Resistive;
    }
    EXPECT_EQ(n.read(0), R{});
    EXPECT_EQ(n.read(1), R{});
    ASSERT_EQ(n.ctx.scheduler().startStrengthDrivers(), E::None);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.read(0), (R{7, 7}));
    EXPECT_EQ(n.read(1), (R{7, 7}));
    EXPECT_EQ(n.ctx.scheduler().setStrengthEdge(1, C::On), E::InvalidTarget);
    EXPECT_EQ(n.ctx.scheduler().setStrengthEdge(0, static_cast<C>(99)), E::InvalidValue);
    EXPECT_EQ(n.ctx.scheduler().setStrengthEdgeDelayed(0, static_cast<C>(99), 1, 1),
              E::InvalidValue);
    EXPECT_EQ(edgeState(n.ctx, 0).committed, C::On);
}

TEST(PassiveNetwork, EveryConductionTransitionUsesItsDestinationDelayAfterIssuerEnds) {
    for (const auto initial : {C::Off, C::On, C::Uncertain})
        for (const auto next : {C::Off, C::On, C::Uncertain}) {
            Network n(2);
            const Root root{0, {6, 6}};
            const Edge edge{0, 1, T::Identity, initial};
            n.start({&root, 1}, {&edge, 1});
            ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
            Notice notice;
            observe(n, 1, notice);
            Script script{k::ProcessId{0}, {{2, next, 5, 7}}};
            script.start(n.ctx);
            ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(script.errors, 0U);
            EXPECT_EQ(notice.used, next == initial ? 0U : 1U);
            if (next != initial) EXPECT_EQ(notice.times[0], next == C::Off ? 9U : 7U);
            EXPECT_EQ(n.read(1), (next == C::Off ? R{} : next == C::On ? R{6, 6} : R{0, 6}));
            EXPECT_EQ(count(n.ctx, "proc.created"), 1U);
            EXPECT_FALSE(edgeState(n.ctx, 0).hasPending);
        }
}

TEST(PassiveNetwork, SamePendingStateRetainsDeadlineAndCarriesCurrentDataWithoutPathDelay) {
    Network n(2);
    const Root root{0, {6, 6}};
    const Edge edge{0, 1, T::Identity, C::Off};
    n.start({&root, 1}, {&edge, 1});
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    Notice notice;
    observe(n, 1, notice);
    Script script{k::ProcessId{0},
                  {{0, C::On, 5, 9},
                   {1, C::Off, 0, 0, false, true, {-6, -6}},
                   {2, C::On, 99, 99},
                   {3, C::Off, 0, 0, false, true, {-6, -3}}}};
    script.start(n.ctx);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(script.errors, 0U);
    ASSERT_EQ(notice.used, 1U);
    EXPECT_EQ(notice.times[0], 5U);
    EXPECT_EQ(notice.values[0], (R{-6, -3}));
    EXPECT_EQ(edgeState(n.ctx, 0).generation, 1U);
}

TEST(PassiveNetwork, DifferentPendingStatesCancelAndCommittedReversionSchedulesNothing) {
    for (unsigned mode = 0; mode < 4; ++mode) {
        Network n(2);
        const Root root{0, {6, 6}};
        const Edge edge{0, 1, T::Identity, C::Off};
        n.start({&root, 1}, {&edge, 1});
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        Notice notice;
        observe(n, 1, notice);
        Script script{k::ProcessId{0}, {{0, C::On, 5, 5}}};
        if (mode == 0 || mode == 3) script.actions.push_back({2, C::Uncertain, 9, 3});
        if (mode == 1) script.actions.push_back({2, C::Off, 99, 99});
        if (mode == 2) script.actions.push_back({2, C::Off, 0, 0, true});
        if (mode == 3) script.actions.push_back({3, C::On, 6, 5});
        script.start(n.ctx);
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(script.errors, 0U);
        EXPECT_EQ(notice.used, mode == 0 || mode == 3 ? 1U : 0U);
        if (mode == 0) {
            EXPECT_EQ(notice.times[0], 5U);
            EXPECT_EQ(notice.values[0], (R{0, 6}));
        }
        if (mode == 3) {
            EXPECT_EQ(notice.times[0], 9U);
            EXPECT_EQ(notice.values[0], (R{6, 6}));
        }
        if (mode == 1 || mode == 2) EXPECT_EQ(n.read(1), R{});
        EXPECT_FALSE(edgeState(n.ctx, 0).hasPending);
    }
}

TEST(PassiveNetwork, DataTraversesCommittedEdgeWhileOffPropagationIsStillPending) {
    Network n(2);
    const Root root{0, {6, 6}};
    const Edge edge{0, 1, T::Identity, C::On};
    n.start({&root, 1}, {&edge, 1});
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    Notice notice;
    observe(n, 1, notice);
    Script script{k::ProcessId{0}, {{0, C::Off, 3, 5}, {2, C::Off, 0, 0, false, true, {-6, -6}}}};
    script.start(n.ctx);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(script.errors, 0U);
    ASSERT_EQ(notice.used, 2U);
    EXPECT_EQ(notice.times[0], 2U);
    EXPECT_EQ(notice.values[0], (R{-6, -6}));
    EXPECT_EQ(notice.times[1], 5U);
    EXPECT_EQ(notice.values[1], R{});
}

TEST(PassiveNetwork, ScalarEdgesKeepIndependentPendingGenerations) {
    Network n(3);
    const Root root{0, {6, 6}};
    const std::array<Edge, 2> edges{{{0, 1, T::Identity, C::Off}, {0, 2, T::Identity, C::Off}}};
    n.start({&root, 1}, edges);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    Notice first, second;
    observe(n, 1, first);
    observe(n, 2, second);
    Script script{
        k::ProcessId{0},
        {{0, C::On, 5, 5}, {0, C::On, 3, 3, false, false, {}, 1}, {2, C::Uncertain, 7, 9}}};
    script.start(n.ctx);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(script.errors, 0U);
    ASSERT_EQ(first.used, 1U);
    ASSERT_EQ(second.used, 1U);
    EXPECT_EQ(first.times[0], 9U);
    EXPECT_EQ(first.values[0], (R{0, 6}));
    EXPECT_EQ(second.times[0], 3U);
    EXPECT_EQ(second.values[0], (R{6, 6}));
    EXPECT_EQ(edgeState(n.ctx, 0).generation, 2U);
    EXPECT_EQ(edgeState(n.ctx, 1).generation, 1U);
}

struct OrderedEdge {
    Network* n;
    bool reactive;
    std::array<unsigned, 8> order{};
    unsigned next = 0;
    static void notice(void* raw, k::SlotBits, std::span<const R>, std::span<const R>) noexcept {
        auto& self = *static_cast<OrderedEdge*>(raw);
        self.order[self.next++] = 3;
    }
    static kirima_status drive(kirima_context*, void* raw) {
        auto& self = *static_cast<OrderedEdge*>(raw);
        self.order[self.next++] = 1;
        EXPECT_EQ(self.reactive ? self.n->ctx.scheduler().setStrengthEdge(0, C::On)
                                : self.n->ctx.scheduler().setStrengthEdgeDelayed(0, C::On, 0, 0),
                  E::None);
        return static_cast<kirima_status>(k::Status::Done);
    }
    static kirima_status probe(kirima_context*, void* raw) {
        auto& self = *static_cast<OrderedEdge*>(raw);
        self.order[self.next++] = 2;
        EXPECT_EQ(self.n->read(1), R{});
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(PassiveNetwork, ExplicitZeroIsInactiveAndReactiveChangesQueueActiveResolution) {
    for (const bool reactive : {false, true}) {
        Network n(2);
        const Root root{0, {6, 6}};
        const Edge edge{0, 1, T::Identity, C::Off};
        n.start({&root, 1}, {&edge, 1});
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        OrderedEdge record{&n, reactive};
        ASSERT_EQ(n.ctx.strengths().subscribe(n.nodes[1].at, &OrderedEdge::notice, &record).error,
                  E::None);
        const auto group = reactive ? k::Group::Reactive : k::Group::Active;
        const auto first = n.ctx.scheduler().addProcess(&OrderedEdge::drive, &record, group);
        const auto second = n.ctx.scheduler().addProcess(&OrderedEdge::probe, &record, group);
        n.ctx.scheduler().start(first);
        n.ctx.scheduler().start(second);
        ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(record.next, 3U);
        EXPECT_EQ(record.order[0], 1U);
        EXPECT_EQ(record.order[1], 2U);
        EXPECT_EQ(record.order[2], 3U);
        EXPECT_EQ(count(n.ctx, "sched.events.active"), reactive ? 2U : 4U);
        EXPECT_EQ(count(n.ctx, "sched.events.reactive"), reactive ? 2U : 0U);
        EXPECT_EQ(count(n.ctx, "sched.events.inactive"), reactive ? 0U : 1U);
        EXPECT_EQ(count(n.ctx, "proc.created"), 2U);
    }
}

struct OverflowEdge {
    k::ProcessId process{0};
    unsigned next = 0;
    static kirima_status run(kirima_context* raw, void* frame) {
        auto& self = *static_cast<OverflowEdge*>(frame);
        auto& ctx = k::fromC(raw);
        auto& scheduler = ctx.scheduler();
        if (self.next++ == 0) {
            scheduler.waitDelay(self.process, UINT64_MAX - 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        EXPECT_EQ(scheduler.setStrengthEdgeDelayed(0, C::On, 1, 1), E::None);
        const auto generation = edgeState(ctx, 0).generation;
        EXPECT_EQ(scheduler.setStrengthEdgeDelayed(0, C::Uncertain, 3, 3), E::InvalidValue);
        EXPECT_TRUE(scheduler.takeOverflow());
        EXPECT_FALSE(scheduler.takeOverflow());
        EXPECT_EQ(edgeState(ctx, 0).generation, generation);
        EXPECT_EQ(edgeState(ctx, 0).pending, C::On);
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(PassiveNetwork, OverflowPreservesOwnedPendingConductionAndItsDeadline) {
    Network n(2);
    const Root root{0, {6, 6}};
    const Edge edge{0, 1, T::Identity, C::Off};
    n.start({&root, 1}, {&edge, 1});
    OverflowEdge record;
    record.process = n.ctx.scheduler().addProcess(&OverflowEdge::run, &record, k::Group::Active);
    n.ctx.scheduler().start(record.process);
    ASSERT_EQ(n.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(n.ctx.scheduler().time(), UINT64_MAX);
    EXPECT_EQ(n.read(1), (R{6, 6}));
}

TEST(PassiveNetwork, MoreThan64UncertainEdgesHaveNoWordMaskCutoffAndHonorWallBudget) {
    Network empty(2);
    std::vector<Edge> edges(65, {0, 1, T::Identity, C::Uncertain});
    empty.start({}, edges);
    ASSERT_EQ(empty.ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(empty.read(0), R{});
    EXPECT_EQ(empty.read(1), R{});
    Network n(2);
    const Root root{0, {6, 6}};
    n.start({&root, 1}, edges);
    n.ctx.scheduler().setWallTimeLimit(1, std::chrono::steady_clock::now());
    const auto result = n.ctx.scheduler().run();
    // A faster exact algorithm may finish. Otherwise interruption must leave
    // the entire component's previous snapshot, never a partial world hull.
    if (result == k::EndReason::Quiescent) {
        EXPECT_EQ(n.read(0), (R{6, 6}));
        EXPECT_EQ(n.read(1), (R{0, 6}));
    } else {
        EXPECT_EQ(result, k::EndReason::WallTimeLimit);
        EXPECT_EQ(n.read(0), R{});
        EXPECT_EQ(n.read(1), R{});
        EXPECT_EQ(count(n.ctx, "slot.writes"), 0U);
    }
    EXPECT_EQ(empty.read(0), R{});
    EXPECT_EQ(empty.read(1), R{});
}
}  // namespace
