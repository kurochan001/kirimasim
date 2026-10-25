// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Stable physical origins and scalar inertial state (approved MOS/switch
// contract 1/6/7). This bank resolves independent nodes, not passive edges.
#pragma once

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

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

class StrengthDrivers {
public:
    explicit StrengthDrivers(std::pmr::memory_resource* memory) noexcept
        : memory_(memory), nodes_(memory), origins_(memory), order_(memory), components_(memory),
          componentNodes_(memory), values_(memory), updates_(memory) {}
    StrengthDrivers(const StrengthDrivers&) = delete;
    StrengthDrivers& operator=(const StrengthDrivers&) = delete;
    [[nodiscard]] bool configured() const noexcept { return configured_; }
    [[nodiscard]] std::size_t nodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t originCount() const noexcept { return origins_.size(); }
    [[nodiscard]] std::optional<StrengthOriginState> origin(std::uint32_t id) const noexcept;

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
    struct Component {
        std::uint32_t first = 0, count = 0;
        bool queued = false;
    };
    std::pmr::memory_resource* memory_;
    std::pmr::vector<Node> nodes_;
    std::pmr::vector<Origin> origins_;
    std::pmr::vector<std::uint32_t> order_;
    std::pmr::vector<Component> components_;
    std::pmr::vector<std::uint32_t> componentNodes_;
    std::pmr::vector<StrengthRange> values_;
    std::pmr::vector<StrengthUpdate> updates_;
    bool configured_ = false, started_ = false;

    StrengthError configure(const StrengthStorage& storage, std::span<const StrengthNodeSpec> nodes,
                            std::span<const StrengthOriginSpec> origins) noexcept;
};
}  // namespace kirimasim::kernel
