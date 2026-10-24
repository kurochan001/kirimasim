// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include <cstdint>
#include <string>
#include <vector>

namespace kirimasim::sir {
struct PatternShape {
    std::uint64_t leaves = 0;
    std::uint64_t nodes = 0;
    bool heap = false;
};
struct PatternValidation {
    std::vector<PatternShape> shapes;
    std::vector<VerifyError> errors;
};
/// Checks all metadata iteratively, including cycles and size arithmetic.
PatternValidation validatePatterns(const Design& design);
/// Requires validated metadata. Expands only as far as the supplied operand
/// count, including zero-leaf repeated shapes without an enormous loop.
bool patternArguments(const Design& design, PatternId layout,
                      const std::vector<PatternShape>& shapes, const std::vector<Type>& actual);
std::string patternLayoutText(const Design& design, const PatternLayout& layout);
}  // namespace kirimasim::sir
