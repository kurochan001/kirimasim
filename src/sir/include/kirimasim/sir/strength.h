// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Static shape of an owned metadata load (MOS/switch contract 7).
#pragma once
#include "kirimasim/sir/design.h"
#include <cstdint>
#include <optional>

namespace kirimasim::sir {
const char* strengthEdgeName(StrengthEdgeKind kind);
bool conditionalStrengthEdge(StrengthEdgeKind kind);
bool resistiveStrengthEdge(StrengthEdgeKind kind);
bool activeHighStrengthEdge(StrengthEdgeKind kind);
struct StrengthLoadShape {
    std::uint32_t indices = 0;
    bool position = false;
};
/// Validate the static target/window/selector shape; operand value types
/// and SSA ownership are checked by the verifier. Array numElems=1 names a
/// fixed ordinal; numElems=0 takes one captured index per declared dimension.
std::optional<StrengthLoadShape> strengthLoadShape(const Design& design, const Instr& in);
bool hasStrengthNetwork(const Design& design);
bool isStrengthNet(const Design& design, SlotId slot);
}  // namespace kirimasim::sir
