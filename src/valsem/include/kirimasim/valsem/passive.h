// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Enumerated reference for approved MOS/switch contract 1/3/6.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "kirimasim/valsem/mos.h"

namespace kirimasim::valsem {
enum class Conduction : std::uint8_t { Off, On, Uncertain };
struct PassiveEdge {
    std::uint32_t first = 0, second = 0;
    StrengthTransfer transfer = StrengthTransfer::Nonresistive;
    Conduction state = Conduction::On;
};
struct PassiveRoot {
    std::uint32_t node = 0;
    Signal value;
};
/// Enumerate edge worlds, root points and simple paths separately, then
/// resolve each destination's physical roots before taking the final hull.
/// Net kinds have no implicit roots; callers materialize those explicitly.
/// Invalid input or multiple possible physical roots at a uwire give nothing.
std::optional<std::vector<Signal>> resolvePassive(std::span<const NetKind> kinds,
                                                  std::span<const PassiveRoot> roots,
                                                  std::span<const PassiveEdge> edges);
}  // namespace kirimasim::valsem
