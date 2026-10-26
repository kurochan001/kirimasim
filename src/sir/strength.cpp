// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/sir/strength.h"
#include <algorithm>

namespace kirimasim::sir {
const char* strengthEdgeName(StrengthEdgeKind kind) {
    switch (kind) {
        case StrengthEdgeKind::Identity: return "identity";
        case StrengthEdgeKind::Tran: return "tran";
        case StrengthEdgeKind::Rtran: return "rtran";
        case StrengthEdgeKind::Tranif0: return "tranif0";
        case StrengthEdgeKind::Tranif1: return "tranif1";
        case StrengthEdgeKind::Rtranif0: return "rtranif0";
        case StrengthEdgeKind::Rtranif1: return "rtranif1";
    }
    return "?";
}
bool conditionalStrengthEdge(StrengthEdgeKind kind) {
    return kind == StrengthEdgeKind::Tranif0 || kind == StrengthEdgeKind::Tranif1 ||
           kind == StrengthEdgeKind::Rtranif0 || kind == StrengthEdgeKind::Rtranif1;
}
bool resistiveStrengthEdge(StrengthEdgeKind kind) {
    return kind == StrengthEdgeKind::Rtran || kind == StrengthEdgeKind::Rtranif0 ||
           kind == StrengthEdgeKind::Rtranif1;
}
bool activeHighStrengthEdge(StrengthEdgeKind kind) {
    return kind == StrengthEdgeKind::Tranif1 || kind == StrengthEdgeKind::Rtranif1;
}

bool hasStrengthNetwork(const Design& d) {
    return !d.strengthNodes.empty() || !d.strengthOrigins.empty() || !d.strengthEdges.empty() ||
           std::any_of(d.slots.begin(), d.slots.end(),
                       [](const Slot& slot) { return (slot.flags & kNeedsStrength) != 0; });
}
bool isStrengthNet(const Design& d, SlotId slot) {
    if (slot.index() >= d.slots.size()) return false;
    const auto physical = physicalSlot(d, slot);
    return physical.index() < d.slots.size() && (d.slot(physical).flags & kNeedsStrength) != 0;
}
std::optional<StrengthLoadShape> strengthLoadShape(const Design& d, const Instr& in) {
    if (in.op != Op::StrengthLoad || in.type.kind != TypeKind::Strength || in.type.width == 0 ||
        in.operands.empty())
        return std::nullopt;
    const auto& o = in.operands[0];
    if (o.kind != Operand::Kind::Slot || !isStrengthNet(d, o.asSlot()) || o.edge != Edge::Any ||
        o.count != 0 || o.numElems > 1 || o.id >= d.slots.size())
        return std::nullopt;
    const auto& slot = d.slots[o.id];
    if (!isFourState(slot.type) || !isIntegral(slot.type)) return std::nullopt;
    if (slot.dims.empty() && (o.firstElem != 0 || o.numElems != 0)) return std::nullopt;
    const auto width = o.width == 0 ? slot.type.width : o.width;
    if ((o.width == 0 && o.lsb != 0) || o.lsb > slot.type.width || width == 0 ||
        width > slot.type.width - o.lsb)
        return std::nullopt;
    StrengthLoadShape shape;
    if (!slot.dims.empty() && o.numElems == 0) {
        if (o.firstElem != 0) return std::nullopt;
        if (slot.dims.size() > UINT32_MAX) return std::nullopt;
        shape.indices = static_cast<std::uint32_t>(slot.dims.size());
    } else if (o.firstElem >= elementCount(slot))
        return std::nullopt;
    const auto count = std::size_t{1} + shape.indices;
    if (in.operands.size() != count && in.operands.size() != count + 1) return std::nullopt;
    shape.position = in.operands.size() == count + 1;
    if (!shape.position && in.type.width != width) return std::nullopt;
    return shape;
}
}  // namespace kirimasim::sir
