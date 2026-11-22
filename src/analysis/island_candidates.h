// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/analysis/tables.h"

namespace kirimasim::analysis {
struct IslandCandidate {
    std::vector<sir::SlotRange> inputs;
    std::vector<sir::SlotRange> immediate;
    std::vector<ExclusionReason> reasons;
};
std::vector<IslandCandidate> islandCandidates(const sir::Design& design);
}  // namespace kirimasim::analysis
