// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/sir/design.h"
#include <span>

namespace kirimasim::sir {
/// Static equivalents of the execution tier's signed index and declaration-
/// order element conversion. Unknown, overflowing and out-of-bounds values
/// produce no address.
std::optional<std::int64_t> constantIndex(const Constant&);
std::optional<std::uint32_t> constantElement(const Design&, std::span<const ArrayDim>,
                                             std::span<const Operand> indices);
}  // namespace kirimasim::sir
