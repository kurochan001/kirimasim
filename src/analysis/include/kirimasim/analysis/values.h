// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/analysis/tables.h"

namespace kirimasim::analysis {
/// Optional proofs for integral SSA values and operands. Unproved four-state
/// sites retain their declared domain. Preserved slots retain storage.
std::vector<ValueFact> analyzeValues(const sir::VerifiedDesign& verified, bool waveform);
}  // namespace kirimasim::analysis
