// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Point/control-choice reference for approved MOS/switch contract 2.
#pragma once

#include <cstdint>
#include <optional>

#include "kirimasim/valsem/strength.h"

namespace kirimasim::valsem {
enum class StrengthTransfer : std::uint8_t { Identity, Nonresistive, Resistive };
enum class MosKind : std::uint8_t { Nmos, Pmos, Rnmos, Rpmos, Cmos, Rcmos };

/// Enumerate the image of every strength point; invalid inputs give nothing.
std::optional<Signal> transferStrength(StrengthTransfer kind, Signal input);
/// Enumerate every data point and admitted control choice independently.
/// CMOS legs share that data point and apply one reduction after their OR.
std::optional<Signal> mosStrength(MosKind kind, Signal input, Logic first,
                                  Logic second = Logic::Zero);
}  // namespace kirimasim::valsem
