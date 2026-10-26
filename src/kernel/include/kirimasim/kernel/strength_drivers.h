// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Stable physical origins and scalar inertial state (approved MOS/switch
// contract 1/3/6/7). Passive worlds preserve physical origin identity.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

#include "kirimasim/kernel/mos.h"
#include "kirimasim/kernel/strength_store.h"

namespace kirimasim::kernel {

struct StrengthNodeSpec {
    SlotBits at;  ///< One normalized scalar bit, width exactly one.
    NetKind kind = NetKind::Wire;
    std::uint32_t component = 0;  ///< Dense IDs, starting at zero.
};
struct StrengthOriginSpec {
    std::uint32_t node = 0;
    StrengthRange initial;
};
struct StrengthOriginState {
    StrengthRange committed, pending;
    std::uint64_t generation = 0;
    bool hasPending = false;
};

enum class StrengthConduction : std::uint8_t { Off, On, Uncertain };
/// Scalar four-state control, independent of the source container (contract 3).
constexpr StrengthConduction strengthConduction(W4 control, bool activeHigh) noexcept {
    if ((control.b & 1U) != 0) return StrengthConduction::Uncertain;
    return ((control.a & 1U) != 0) == activeHigh ? StrengthConduction::On : StrengthConduction::Off;
}
struct StrengthEdgeSpec {
    std::uint32_t first = 0, second = 0;
    StrengthTransfer transfer = StrengthTransfer::Nonresistive;
    StrengthConduction initial = StrengthConduction::On;
};
struct StrengthEdgeState {
    StrengthConduction committed = StrengthConduction::Off;
    StrengthConduction pending = StrengthConduction::Off;
    std::uint64_t generation = 0;
    bool hasPending = false;
};

class StrengthDrivers {
public:
    explicit StrengthDrivers(std::pmr::memory_resource* memory) noexcept
        : memory_(memory), nodes_(memory), origins_(memory), order_(memory), components_(memory),
          componentNodes_(memory), values_(memory), updates_(memory), edges_(memory),
          componentEdges_(memory), adjacencyOffsets_(memory), adjacency_(memory), routes_(memory),
          inQueue_(memory), queue_(memory), choices_(memory), states_(memory), islands_(memory),
          islandNodes_(memory), islandEdges_(memory) {}
    StrengthDrivers(const StrengthDrivers&) = delete;
    StrengthDrivers& operator=(const StrengthDrivers&) = delete;
    [[nodiscard]] bool configured() const noexcept { return configured_; }
    [[nodiscard]] std::size_t nodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t originCount() const noexcept { return origins_.size(); }
    [[nodiscard]] std::optional<StrengthOriginState> origin(std::uint32_t id) const noexcept;

    [[nodiscard]] std::size_t edgeCount() const noexcept { return edges_.size(); }
    [[nodiscard]] std::optional<StrengthEdgeState> edge(std::uint32_t id) const noexcept;

private:
    friend class Scheduler;
    struct Node {
        StrengthNodeSpec spec;
        std::uint32_t first = 0, count = 0;
    };
    struct Origin {
        std::uint32_t node = 0;
        StrengthOriginState state;
    };
    struct Edge {
        StrengthEdgeSpec spec;
        StrengthEdgeState state;
    };
    struct Component {
        std::uint32_t first = 0, count = 0;
        bool queued = false;
        std::uint32_t edgeFirst = 0, edgeCount = 0;
        std::uint32_t islandFirst = 0, islandCount = 0;
    };
    std::pmr::memory_resource* memory_;
    std::pmr::vector<Node> nodes_;
    std::pmr::vector<Origin> origins_;
    std::pmr::vector<std::uint32_t> order_;
    std::pmr::vector<Component> components_;
    std::pmr::vector<std::uint32_t> componentNodes_;
    std::pmr::vector<StrengthRange> values_;
    std::pmr::vector<StrengthUpdate> updates_;
    std::pmr::vector<Edge> edges_;
    std::pmr::vector<std::uint32_t> componentEdges_;
    std::pmr::vector<std::size_t> adjacencyOffsets_;
    std::pmr::vector<std::uint32_t> adjacency_;
    std::pmr::vector<std::uint8_t> routes_, inQueue_;
    std::pmr::vector<std::uint32_t> queue_;
    std::pmr::vector<std::uint8_t> choices_;
    std::pmr::vector<std::uint32_t> states_;
    std::pmr::vector<Component> islands_;  ///< Potential connectivity, independent of publication.
    std::pmr::vector<std::uint32_t> islandNodes_, islandEdges_;
    bool configured_ = false, started_ = false;

    StrengthError configure(const StrengthStorage& storage, std::span<const StrengthNodeSpec> nodes,
                            std::span<const StrengthOriginSpec> origins,
                            std::span<const StrengthEdgeSpec> edges) noexcept;
    StrengthError configureEdges(std::span<const StrengthEdgeSpec> edges);
};
}  // namespace kirimasim::kernel
