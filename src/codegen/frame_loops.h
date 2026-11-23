// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/sir/design.h"
#include <optional>

namespace kirimasim::codegen {
struct FrameLoops {
    std::vector<bool> latch;
    std::uint64_t repeatedSteps = 0;
};

// Recognize disjoint counted loops in verified SIR. The caller must also prove
// that removing their latch edges leaves the entire process acyclic.
std::optional<FrameLoops> frameLoops(const sir::Design&, sir::ProcessId);
}  // namespace kirimasim::codegen
