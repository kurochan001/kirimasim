// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/sir/design.h"
#include <optional>
#include <span>

namespace kirimasim::codegen {
struct BoundedLoops {
    std::vector<bool> latch;
    std::vector<std::uint32_t> extraVisits;
    sir::SlotId slotCounter = sir::kNone;
    explicit BoundedLoops(std::size_t blocks) : latch(blocks), extraVisits(blocks) {}
};

// Recognize disjoint counted loops in verified SIR. The caller must also prove
// that removing their latch edges leaves the entire process acyclic.
std::optional<BoundedLoops> frameLoops(const sir::Design&, sir::ProcessId);
// A module variable used as an induction counter must have no other writer or
// writable alias. An empty table means the design has unanalysed capabilities.
std::vector<sir::ProcessId> exclusiveSlotOwners(const sir::Design&);
std::optional<BoundedLoops> slotLoops(const sir::Design&, sir::ProcessId,
                                      std::span<const sir::ProcessId> owners);
}  // namespace kirimasim::codegen
