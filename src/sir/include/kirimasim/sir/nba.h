// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <vector>

#include "kirimasim/sir/verify.h"

namespace kirimasim::sir {
/// Structural checks of all typed event-NBA layouts, including unused rows.
/// Instruction SSA/dominance, capture bindings and write sets are checked by verify().
std::vector<VerifyError> verifyNbaLayouts(const Design& design);
/// All statically named destinations, conservatively widened for dynamic selects.
/// Reference destinations are resolved and checked at capture, like nba.ref.
std::vector<SlotRange> nbaWrites(const NbaLayout& layout);
}  // namespace kirimasim::sir
