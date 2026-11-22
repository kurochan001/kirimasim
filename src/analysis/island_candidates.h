// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/analysis/tables.h"

#include <span>

namespace kirimasim::analysis {
/// Packed and unpacked range referenced by a verified ordinary storage
/// instruction. Dynamic addresses conservatively cover the physical slot.
std::optional<sir::SlotRange> storageAccessRange(const sir::Design&, const sir::Instr&,
                                                 std::span<const sir::Type> temps, bool& dynamic);
struct IslandCandidate {
    std::vector<sir::SlotRange> inputs;
    std::vector<sir::SlotRange> immediate;
    std::vector<ExclusionReason> reasons;
};
std::vector<IslandCandidate> islandCandidates(const sir::Design& design,
                                              bool dynamicRanges = false);
}  // namespace kirimasim::analysis
