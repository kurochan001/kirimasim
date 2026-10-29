// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original small-graph oracles for approved MOS/switch contract 5.
// The reference enumerates worlds, root points and simple paths independently.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/passive.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using Range = k::StrengthRange;
using Error = k::StrengthError;
using Root = k::StrengthOriginSpec;
using Edge = k::StrengthEdgeSpec;
using Transfer = k::StrengthTransfer;
using Conduction = k::StrengthConduction;
using Forces = std::vector<std::optional<v::Logic>>;

k::W4 forceValue(v::Logic value) {
    switch (value) {
        case v::Logic::Zero: return {0, 0};
        case v::Logic::One: return {1, 0};
        case v::Logic::X: return {1, 1};
        case v::Logic::Z: return {0, 1};
    }
    return {};
}
struct Network {
    k::Context context;
    k::SlotId slot;
    std::vector<k::StrengthNodeSpec> nodes;
    explicit Network(unsigned count, unsigned width = 0) {
        const auto slotWidth = width == 0 ? count : width;
        const auto elements = width == 0 ? 1U : count / width;
        slot = context.store().add(k::Domain::Four, slotWidth, elements, k::Init::Z);
        EXPECT_EQ(context.scheduler().enableStrengths(slot, std::vector<Range>(count)),
                  Error::None);
        nodes.reserve(count);
        for (unsigned n = 0; n < count; ++n)
            nodes.push_back({{slot, width == 0 ? 0 : n / width, width == 0 ? n : n % width, 1}});
    }
    void start(std::span<const Root> roots, std::span<const Edge> edges) {
        ASSERT_EQ(context.scheduler().configureStrengthDrivers(nodes, roots, edges), Error::None);
        ASSERT_EQ(context.scheduler().startStrengthDrivers(), Error::None);
        ASSERT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    }
    Range read(unsigned node) const {
        Range out;
        EXPECT_EQ(context.strengths().read(nodes[node].at, {&out, 1}), Error::None);
        return out;
    }
    void force(std::span<const std::optional<v::Logic>> values) {
        ASSERT_EQ(values.size(), nodes.size());
        std::vector<k::StrengthForceUpdate> updates;
        updates.reserve(values.size());
        for (unsigned n = 0; n < values.size(); ++n) {
            const auto value = values[n];
            updates.push_back({n, value ? std::optional{forceValue(*value)} : std::nullopt});
        }
        const auto result = context.scheduler().setStrengthForces(updates);
        ASSERT_EQ(result.error, Error::None);
        ASSERT_TRUE(result.committed);
    }
    void compare(std::span<const Root> roots, std::span<const Edge> edges,
                 std::span<const std::optional<v::Logic>> forced) const {
        std::vector<v::NetKind> kinds;
        std::vector<v::PassiveRoot> sources;
        std::vector<v::PassiveEdge> paths;
        kinds.reserve(nodes.size());
        sources.reserve(roots.size());
        paths.reserve(edges.size());
        for (const auto& node : nodes)
            kinds.push_back(static_cast<v::NetKind>(node.kind));
        for (const auto& root : roots)
            sources.push_back({root.node, {root.initial.lo, root.initial.hi}});
        for (const auto& edge : edges)
            paths.push_back({edge.first, edge.second,
                             static_cast<v::StrengthTransfer>(edge.transfer),
                             static_cast<v::Conduction>(edge.initial)});
        const auto expected = v::resolvePassive(kinds, sources, paths, forced);
        ASSERT_TRUE(expected.has_value());
        const auto expectedValues = expected.value_or(std::vector<v::Signal>{});
        ASSERT_EQ(expectedValues.size(), nodes.size());
        for (unsigned n = 0; n < nodes.size(); ++n) {
            EXPECT_EQ(read(n), (Range{static_cast<std::int8_t>(expectedValues[n].lo),
                                      static_cast<std::int8_t>(expectedValues[n].hi)}))
                << "node " << n;
            const auto at = nodes[n].at;
            const auto* words = context.store().element(at.slot, at.elem);
            const std::size_t chunk = at.lsb / 64;
            const auto bit = at.lsb % 64;
            const auto projected = forceValue(v::valueOf(expectedValues[n]));
            EXPECT_EQ((words[2 * chunk] >> bit) & 1U, projected.a) << n;
            EXPECT_EQ((words[2 * chunk + 1] >> bit) & 1U, projected.b) << n;
            const auto actualForce = context.strengthDrivers().forced(n);
            const auto wanted = forced[n];
            ASSERT_EQ(actualForce.has_value(), wanted.has_value()) << n;
            if (actualForce && wanted) {
                const auto expectedForce = forceValue(*wanted);
                EXPECT_EQ(actualForce->a, expectedForce.a) << n;
                EXPECT_EQ(actualForce->b, expectedForce.b) << n;
            }
        }
    }
};

TEST(StrengthForceValue, EveryIntervalAndForceValueMatchesIndependentWorldEnumeration) {
    unsigned checked = 0;
    for (const auto kind : {k::NetKind::Wire, k::NetKind::Tri, k::NetKind::Wand, k::NetKind::Triand,
                            k::NetKind::Wor, k::NetKind::Trior}) {
        Network network(3);
        network.nodes[1].kind = kind;
        network.nodes[2].kind = k::NetKind::Wor;
        std::array<Root, 2> roots{{{0, {}}, {1, {-7, 7}}}};
        const std::array<Edge, 3> edges{{{0, 1, Transfer::Identity, Conduction::Uncertain},
                                         {1, 2, Transfer::Resistive, Conduction::On},
                                         {0, 2, Transfer::Nonresistive, Conduction::Uncertain}}};
        network.start(roots, edges);
        Forces forced(3);
        for (int lo = -7; lo <= 7; ++lo)
            for (int hi = lo; hi <= 7; ++hi) {
                roots[0].initial = {static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)};
                ASSERT_EQ(network.context.scheduler().driveStrength(0, roots[0].initial),
                          Error::None);
                ASSERT_EQ(network.context.scheduler().run(), k::EndReason::Quiescent);
                for (const auto value : {v::Logic::Zero, v::Logic::One, v::Logic::X, v::Logic::Z}) {
                    SCOPED_TRACE(std::to_string(static_cast<int>(kind)) + ":" + std::to_string(lo) +
                                 ":" + std::to_string(hi) + ":" +
                                 std::to_string(static_cast<int>(value)));
                    forced[1] = value;
                    network.force(forced);
                    network.compare(roots, edges, forced);
                    ++checked;
                }
            }
    }
    EXPECT_EQ(checked, 2880U);
}

TEST(StrengthForceValue, AllThreeEdgeWorldsAndTransfersRetainOnePhysicalForceRoot) {
    unsigned checked = 0;
    for (unsigned maps = 0; maps < 27; ++maps) {
        Network network(3);
        network.nodes[1].kind = k::NetKind::Wand;
        network.nodes[2].kind = k::NetKind::Wor;
        std::array<Edge, 3> edges{{{0, 1}, {1, 2}, {0, 2}}};
        auto code = maps;
        for (auto& edge : edges) {
            edge.transfer = static_cast<Transfer>(code % 3);
            code /= 3;
        }
        const std::array<Root, 2> roots{{{0, {-4, 2}}, {2, {3, 3}}}};
        network.start(roots, edges);
        Forces forced(3);
        for (unsigned controls = 0; controls < 27; ++controls) {
            code = controls;
            for (unsigned e = 0; e < edges.size(); ++e) {
                edges[e].initial = static_cast<Conduction>(code % 3);
                code /= 3;
                ASSERT_EQ(network.context.scheduler().setStrengthEdge(e, edges[e].initial),
                          Error::None);
            }
            ASSERT_EQ(network.context.scheduler().run(), k::EndReason::Quiescent);
            for (const auto value : {v::Logic::Zero, v::Logic::One, v::Logic::X, v::Logic::Z}) {
                SCOPED_TRACE(std::to_string(maps) + ":" + std::to_string(controls) + ":" +
                             std::to_string(static_cast<unsigned>(value)));
                forced[1] = value;
                network.force(forced);
                network.compare(roots, edges, forced);
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, 2916U);
}

TEST(StrengthForceValue, DuplicateRoutesCyclesAndOrderPermutationsDoNotInventForceRoots) {
    std::array<Edge, 5> edges{{{0, 1, Transfer::Identity},
                               {0, 1, Transfer::Resistive},
                               {1, 2, Transfer::Nonresistive, Conduction::Uncertain},
                               {2, 0, Transfer::Identity},
                               {1, 1, Transfer::Resistive}}};
    std::array<Root, 2> roots{{{0, {-2, 4}}, {2, {-5, -5}}}};
    unsigned checked = 0;
    for (unsigned order = 0; order < edges.size() * 2; ++order) {
        Network network(3);
        network.nodes[2].kind = k::NetKind::Wand;
        network.start(roots, edges);
        Forces forced(3);
        forced[0] = v::Logic::X;
        forced[1] = v::Logic::One;
        network.force(forced);
        network.compare(roots, edges, forced);
        EXPECT_EQ(network.context.strengthDrivers().originCount(), roots.size());
        ++checked;
        std::rotate(edges.begin(), edges.begin() + 1, edges.end());
        if (order == edges.size() - 1) {
            std::reverse(edges.begin(), edges.end());
            std::reverse(roots.begin(), roots.end());
            for (auto& edge : edges)
                std::swap(edge.first, edge.second);
        }
    }
    EXPECT_EQ(checked, 10U);
}

TEST(StrengthForceValue, ForcedReadoutIsExactWhileRemoteSupplyStillTransitsForceZ) {
    Network network(3);
    const std::array<Root, 2> roots{{{0, {7, 7}}, {1, {-7, -7}}}};
    const std::array<Edge, 2> edges{{{0, 1, Transfer::Identity}, {1, 2, Transfer::Identity}}};
    network.start(roots, edges);
    Forces forced(3);
    for (const auto value : {v::Logic::Zero, v::Logic::One, v::Logic::X, v::Logic::Z}) {
        forced[1] = value;
        network.force(forced);
        network.compare(roots, edges, forced);
        EXPECT_EQ(network.read(2), (Range{7, 7}));
    }
    EXPECT_EQ(network.read(1), (Range{}));
    EXPECT_EQ(network.read(0), (Range{7, 7}));
}

TEST(StrengthForceValue, RootlessAndUwireNetworksTransportForcesWithoutChangingStaticOwnership) {
    for (const bool ordinaryRoot : {false, true}) {
        Network network(3);
        network.nodes[2].kind = k::NetKind::Uwire;
        const std::vector<Root> roots =
            ordinaryRoot ? std::vector<Root>{{0, {7, 7}}} : std::vector<Root>{};
        const std::array<Edge, 3> edges{{{0, 1, Transfer::Resistive},
                                         {1, 2, Transfer::Identity},
                                         {0, 2, Transfer::Nonresistive}}};
        network.start(roots, edges);
        Forces forced(3);
        forced[0] = v::Logic::Zero;
        forced[1] = v::Logic::One;
        network.force(forced);
        network.compare(roots, edges, forced);
        EXPECT_EQ(network.context.strengthDrivers().originCount(), roots.size());
        forced.assign(3, std::nullopt);
        network.force(forced);
        network.compare(roots, edges, forced);
        if (!ordinaryRoot)
            for (unsigned n = 0; n < 3; ++n)
                EXPECT_EQ(network.read(n), (Range{}));
    }
}

TEST(StrengthForceValue, ResistiveForcePathsCrossWideWordsAndElementBoundaries) {
    Network network(130, 65);
    std::vector<Edge> edges;
    for (unsigned n = 1; n < 130; ++n)
        edges.push_back({n - 1, n, Transfer::Resistive});
    edges.push_back({64, 64, Transfer::Identity});
    network.start({}, edges);
    Forces forced(130);
    forced[0] = v::Logic::One;
    network.force(forced);
    network.compare({}, edges, forced);
    EXPECT_EQ(network.read(0), (Range{6, 6}));
    for (const auto n : {63U, 64U, 65U, 129U})
        EXPECT_EQ(network.read(n), (Range{1, 1}));
    forced[64] = v::Logic::Z;
    forced[129] = v::Logic::Zero;
    network.force(forced);
    network.compare({}, edges, forced);
    EXPECT_EQ(network.read(64), (Range{}));
    EXPECT_EQ(network.read(129), (Range{-6, -6}));
    forced.assign(130, std::nullopt);
    network.force(forced);
    network.compare({}, edges, forced);
}

TEST(StrengthForceValue, EmptyForceOverlayPreservesLegacyResultsAndUnderlyingValidation) {
    const std::array kinds{v::NetKind::Wire, v::NetKind::Uwire};
    const std::array<v::PassiveEdge, 1> edges{
        {{0, 1, v::StrengthTransfer::Resistive, v::Conduction::Uncertain}}};
    const std::array<v::PassiveRoot, 1> roots{{{0, {-7, 7}}}};
    const std::array<std::optional<v::Logic>, 2> empty{};
    EXPECT_EQ(v::resolvePassive(kinds, roots, edges), v::resolvePassive(kinds, roots, edges, {}));
    EXPECT_EQ(v::resolvePassive(kinds, roots, edges),
              v::resolvePassive(kinds, roots, edges, empty));
    const std::array<std::optional<v::Logic>, 2> forced{{v::Logic::Z, v::Logic::One}};
    EXPECT_FALSE(
        v::resolvePassive(kinds, std::array<v::PassiveRoot, 2>{{{0, {}}, {1, {}}}}, edges, forced));
    EXPECT_FALSE(v::resolvePassive(kinds, std::array<v::PassiveRoot, 1>{{{2, {}}}}, edges, forced));
    EXPECT_FALSE(
        v::resolvePassive(kinds, std::array<v::PassiveRoot, 1>{{{0, {-8, 0}}}}, edges, forced));
    EXPECT_FALSE(v::resolvePassive(kinds, roots, std::array<v::PassiveEdge, 1>{{{0, 2}}}, forced));
    EXPECT_FALSE(
        v::resolvePassive(std::array{v::NetKind::Tri0, v::NetKind::Wire}, roots, edges, forced));
    EXPECT_FALSE(v::resolvePassive(kinds, roots, edges,
                                   std::array<std::optional<v::Logic>, 1>{{v::Logic::Zero}}));
    EXPECT_FALSE(v::resolvePassive(
        kinds, roots, edges,
        std::array<std::optional<v::Logic>, 2>{{static_cast<v::Logic>(255), std::nullopt}}));
}
}  // namespace
