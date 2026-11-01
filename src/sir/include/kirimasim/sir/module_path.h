// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <span>

#include "kirimasim/sir/verify.h"

namespace kirimasim::sir {
using ModulePathDelays = std::array<std::optional<std::uint64_t>, 12>;
/// Normalize 1/2/3/6/12 selected delays; a missing value is positive overflow.
std::optional<ModulePathDelays>
normalizeModulePathDelays(std::span<const std::optional<std::uint64_t>> values);
std::vector<VerifyError> verifyModulePaths(const Design& design);
}  // namespace kirimasim::sir
