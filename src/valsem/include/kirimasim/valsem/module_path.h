// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "kirimasim/valsem/logic.h"

namespace kirimasim::valsem {
/// Independent scalar module-path reference, approved specify contract §2.
/// Invalid arity returns no value. Equal endpoints return zero.
std::optional<std::uint64_t> modulePathDelay(std::span<const std::uint64_t> values, Logic from,
                                             Logic to);
}  // namespace kirimasim::valsem
