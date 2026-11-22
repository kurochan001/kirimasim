// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/sir/verified.h"

namespace kirimasim::analysis {
struct BlockingRace {
    sir::SlotId slot, clock;
    sir::Edge edge;
    sir::ProcessId writer, reader;
    sir::SourceLoc write, read;
};
/// Potential blocking write/read conflicts in the same active clock edge.
std::vector<BlockingRace> blockingRaces(const sir::VerifiedDesign&);
}  // namespace kirimasim::analysis
