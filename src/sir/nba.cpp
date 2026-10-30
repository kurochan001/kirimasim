// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/nba.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace kirimasim::sir {
namespace {
Type unsignedValue(Type t) {
    if (t.kind == TypeKind::S2) t.kind = TypeKind::U2;
    if (t.kind == TypeKind::S4) t.kind = TypeKind::U4;
    return t;
}
bool fitsValue(Type target, Type value) {
    return value.kind != TypeKind::Void && unsignedValue(target) == unsignedValue(value);
}
Type rangeType(const Design& d, SlotRange r) {
    if (!validSlotRange(d, r)) return voidType();
    auto type = d.slot(r.slot).type;
    if (r.width != 0) {
        type = unsignedValue(type);
        type.width = r.width;
    }
    return type;
}
}  // namespace

std::vector<SlotRange> nbaWrites(const NbaLayout& layout) {
    std::vector<SlotRange> result;
    for (const auto& target : layout.targets) {
        if (target.valueless_by_exception()) continue;  // Reported by verifyNbaLayouts.
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, NbaSlotTarget>)
                    result.push_back(value.part);
                else if constexpr (std::is_same_v<T, NbaElementTarget>)
                    result.push_back({value.slot});
                else if constexpr (std::is_same_v<T, NbaBitsTarget>)
                    result.push_back(value.parent);
            },
            target);
    }
    return result;
}

std::vector<VerifyError> verifyNbaLayouts(const Design& d) {
    std::vector<VerifyError> errors;
    for (std::size_t index = 0; index < d.nbaLayouts.size(); ++index) {
        const auto& layout = d.nbaLayouts[index];
        const auto bad = [&](const std::string& message) {
            errors.push_back({"nba layout !n" + std::to_string(index), message, {}});
        };
        const auto type = [&](NbaValueId value) {
            if (value.index() >= layout.values.size()) {
                bad("captured value ordinal out of range");
                return voidType();
            }
            return layout.values[value.index()];
        };
        const auto callback = [&](SubroutineId id, bool guard) {
            if (id.index() >= d.subroutines.size()) {
                bad("callback subroutine out of range");
                return;
            }
            const auto& sub = d.subroutine(id);
            if (sub.kind != SubroutineKind::Function || !sub.automatic || sub.mayWait ||
                !sub.params.empty() || sub.frameOf.kind != Owner::Kind::None ||
                sub.activation.inheritCaptures || !sub.activation.extents.empty())
                bad("callback needs an automatic zero-argument nonwaiting function without "
                    "an enclosing activation");
            for (const auto& var : sub.frame)
                if (var.home == FrameHome::Capture || var.captureSource != kNone)
                    bad("callback cannot borrow enclosing frame rows");
            if (!isIntegral(sub.returnType) || sub.returnType.width == 0 ||
                (guard && sub.returnType.width != 1))
                bad(guard ? "iff callback must return one integral bit"
                          : "watch callback must return an integral value");
        };
        if (layout.events.empty()) bad("needs at least one event item");
        for (const auto& item : layout.events) {
            if (item.valueless_by_exception()) {
                bad("invalid event variant");
                continue;
            }
            std::visit(
                [&](const auto& event) {
                    using T = std::decay_t<decltype(event)>;
                    if constexpr (std::is_same_v<T, NbaSlotEvent>) {
                        if (!validSlotRange(d, event.part) || !isIntegral(rangeType(d, event.part)))
                            bad("signal item needs a valid integral slot range");
                    } else if constexpr (std::is_same_v<T, NbaNamedEvent>) {
                        if (type(event.value) != Type{TypeKind::Event, 0})
                            bad("named event needs a captured event value");
                    } else {
                        callback(event.watch, false);
                    }
                    if constexpr (!std::is_same_v<T, NbaNamedEvent>)
                        if (static_cast<unsigned>(event.edge) > static_cast<unsigned>(Edge::Both))
                            bad("invalid edge");
                    if (event.guard != kNone) callback(event.guard, true);
                },
                item);
        }
        for (const auto& item : layout.targets) {
            if (item.valueless_by_exception()) {
                bad("invalid target variant");
                continue;
            }
            std::visit(
                [&](const auto& target) {
                    using T = std::decay_t<decltype(target)>;
                    const auto value = type(target.value);
                    SlotId slot = kNone;
                    if constexpr (std::is_same_v<T, NbaSlotTarget>) {
                        slot = target.part.slot;
                        if (!validSlotRange(d, target.part) || target.part.numElems > 1 ||
                            (slot.index() < d.slots.size() && !d.slot(slot).dims.empty() &&
                             target.part.numElems != 1) ||
                            !fitsValue(rangeType(d, target.part), value))
                            bad("fixed target needs one valid element and a fitting captured "
                                "value");
                    } else if constexpr (std::is_same_v<T, NbaElementTarget> ||
                                         std::is_same_v<T, NbaBitsTarget>) {
                        if constexpr (std::is_same_v<T, NbaElementTarget>)
                            slot = target.slot;
                        else
                            slot = target.parent.slot;
                        if (slot.index() >= d.slots.size()) {
                            bad("target slot out of range");
                            return;
                        }
                        const auto& shape = d.slot(slot);
                        if (target.indices.size() != shape.dims.size())
                            bad("target needs one index per dimension");
                        for (const auto operand : target.indices)
                            if (!isIntegral(type(operand))) bad("target index must be integral");
                        if constexpr (std::is_same_v<T, NbaElementTarget>) {
                            if (shape.dims.empty() || !fitsValue(shape.type, value))
                                bad("element target needs an array and a fitting captured value");
                        } else {
                            if (!validSlotRange(d, target.parent) || target.parent.firstElem != 0 ||
                                target.parent.numElems != 0 || !isIntegral(shape.type) ||
                                !isIntegral(value) || value.width == 0 ||
                                isFourState(shape.type) != isFourState(value) ||
                                !isIntegral(type(target.position)))
                                bad("bit target needs an integral parent, position and value");
                        }
                    } else {
                        const auto reference = type(target.reference);
                        if (target.indices.size() != target.dims.size())
                            bad("reference target needs one index per dimension");
                        for (const auto operand : target.indices)
                            if (!isIntegral(type(operand)))
                                bad("reference target index must be integral");
                        Slot shape;
                        shape.dims = target.dims;
                        if (elementCount(shape) == 0 || elementCount(shape) > UINT32_MAX ||
                            (!target.dims.empty() &&
                             (target.leaf != 0 || reference.detail != kNone)))
                            bad("invalid reference array shape");
                        if (reference.kind != TypeKind::Ref ||
                            target.target.kind == TypeKind::Ref ||
                            target.target.kind == TypeKind::Void)
                            bad("reference target needs a captured ref and a fitting leaf type");
                        if (reference.detail != kNone &&
                            (target.leaf != 0 || !isContainer(target.target) ||
                             target.target.detail != reference.detail))
                            bad("container reference target differs from its declared value type");
                        if constexpr (std::is_same_v<T, NbaRefBitsTarget>)
                            if (!isIntegral(target.target) || !isIntegral(value) ||
                                value.width == 0 ||
                                isFourState(target.target) != isFourState(value) ||
                                !isIntegral(type(target.position)))
                                bad("reference bit target needs integral position and value");
                        if constexpr (std::is_same_v<T, NbaRefTarget>)
                            if (!fitsValue(target.target, value))
                                bad("reference value differs from its leaf type");
                    }
                    if (slot != kNone && slot.index() < d.slots.size() &&
                        d.slot(slot).kind != SlotKind::Var)
                        bad("NBA target must be a variable");
                },
                item);
        }
    }
    return errors;
}
}  // namespace kirimasim::sir
