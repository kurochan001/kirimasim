// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/classes.h"

#include <type_traits>

namespace kirimasim::sir {
namespace {
bool objectType(const Design& d, Type type) {
    for (std::size_t depth = 0; depth <= d.types.size(); ++depth) {
        if (isClass(type) || type.object != kNone) return true;
        if ((!isContainer(type) && type.kind != TypeKind::Ref) || type.detail == kNone)
            return false;
        if (type.detail.index() >= d.types.size()) return true;
        type = d.types[type.detail.index()].value;
    }
    return true;
}
struct BodyUse {
    bool object = false;
    std::vector<std::size_t> callees;
};
}  // namespace

bool classExecutionShapeSupported(const Design& d) {
    for (const auto& pattern : d.patterns)
        if (objectType(d, pattern.value)) return false;
    for (const auto& slot : d.slots)
        if (objectType(d, slot.type) && (slot.flags & kForceTarget) != 0) return false;

    const auto processes = d.processes.size();
    std::vector<BodyUse> uses(processes + d.subroutines.size());
    std::vector<bool> observers(uses.size(), false);
    const auto observeSub = [&](SubroutineId id) {
        if (id != kNone) observers[processes + id.index()] = true;
    };
    const auto observeSlot = [&](SlotId id) {
        return id == kNone || !objectType(d, d.slots[id.index()].type);
    };
    const auto scan = [&](const auto& item, std::size_t id, std::span<const FrameVar> frame) {
        std::vector<Type> temps(item.numTemps);
        for (const auto& block : item.blocks) {
            for (const auto& param : block.params)
                temps[param.temp.index()] = param.type;
            for (const auto& in : block.instrs)
                if (in.result != kNone) temps[in.result.index()] = in.type;
        }
        const auto operandType = [&](const Operand& operand) {
            switch (operand.kind) {
                case Operand::Kind::Temp: return temps[operand.id];
                case Operand::Kind::Const: return d.constants[operand.id].type;
                case Operand::Kind::Slot: return d.slots[operand.id].type;
                case Operand::Kind::Frame: return frame[operand.id].type;
                default: return voidType();
            }
        };
        auto& use = uses[id];
        for (const auto& block : item.blocks)
            for (const auto& in : block.instrs) {
                // Property capture, formatting and overrides keep their
                // existing implementation boundary in every body.
                switch (in.op) {
                    case Op::OverrideStart:
                    case Op::OverrideStop:
                    case Op::StoreOverride:
                    case Op::FormatPattern: return false;
                    default: break;
                }
                use.object = use.object || isObjectOp(in.op) || objectType(d, in.type) ||
                             in.type.kind == TypeKind::Ref;
                for (const auto& operand : in.operands) {
                    const auto type = operandType(operand);
                    // Untyped refs can designate an object property. Their
                    // provenance is still outside object observer support.
                    use.object = use.object || objectType(d, type) || type.kind == TypeKind::Ref;
                    if (operand.kind == Operand::Kind::Subroutine)
                        use.callees.push_back(processes + operand.id);
                    if ((in.op == Op::Syscall || in.op == Op::Builtin || in.op == Op::Strobe) &&
                        objectType(d, type))
                        return false;
                    if ((in.op == Op::WaitEvent || in.op == Op::WaitNamed) &&
                        (objectType(d, type) || type.kind == TypeKind::Ref))
                        return false;
                }
                if (in.op == Op::Monitor) observers[in.operands[0].id] = true;
            }
        return true;
    };
    for (std::size_t i = 0; i < processes; ++i) {
        const auto& process = d.processes[i];
        switch (process.kind) {
            case ProcessKind::Cont:
            case ProcessKind::Comb:
            case ProcessKind::Latch:
            case ProcessKind::CombAt:
            case ProcessKind::Ff:
            case ProcessKind::FfPlain: observers[i] = true; break;
            case ProcessKind::Override: return false;
            default: break;
        }
        for (const auto& trigger : process.triggers) {
            if (!observeSlot(trigger.range.slot)) return false;
            observeSub(trigger.guard);
            observeSub(trigger.watch);
        }
        if (!scan(process, i, process.frame)) return false;
    }
    for (std::size_t i = 0; i < d.subroutines.size(); ++i) {
        const auto& sub = d.subroutines[i];
        std::span<const FrameVar> frame = sub.frame;
        if (readsWaiterFrame(sub.kind)) {
            observers[processes + i] = true;
            frame = sub.frameOf.kind == Owner::Kind::Process
                        ? std::span(d.processes[sub.frameOf.id].frame)
                        : std::span(d.subroutines[sub.frameOf.id].frame);
        }
        if (!scan(sub, processes + i, frame)) return false;
    }
    for (const auto& layout : d.nbaLayouts) {
        for (const auto type : layout.values)
            if (objectType(d, type) || type.kind == TypeKind::Ref) return false;
        for (const auto& event : layout.events)
            if (!std::visit(
                    [&](const auto& item) {
                        observeSub(item.guard);
                        if constexpr (std::is_same_v<std::decay_t<decltype(item)>, NbaWatchEvent>)
                            observeSub(item.watch);
                        if constexpr (std::is_same_v<std::decay_t<decltype(item)>, NbaSlotEvent>)
                            return observeSlot(item.part.slot);
                        return true;
                    },
                    event))
                return false;
        for (const auto& target : layout.targets)
            if (!std::visit(
                    [&](const auto& item) {
                        using T = std::decay_t<decltype(item)>;
                        if constexpr (std::is_same_v<T, NbaSlotTarget>)
                            return observeSlot(item.part.slot);
                        else if constexpr (std::is_same_v<T, NbaBitsTarget>)
                            return observeSlot(item.parent.slot);
                        else if constexpr (std::is_same_v<T, NbaElementTarget> ||
                                           std::is_same_v<T, NbaStringCharTarget>)
                            return item.slot != kNone && observeSlot(item.slot);
                        else
                            return false;
                    },
                    target))
                return false;
    }

    // Propagate through ordinary calls to a fixed point, including recursive
    // call groups. No observer can hide object reads in another function.
    std::vector<std::vector<std::size_t>> callers(uses.size());
    std::vector<std::size_t> pending;
    pending.reserve(uses.size());
    for (std::size_t i = 0; i < uses.size(); ++i) {
        if (uses[i].object) pending.push_back(i);
        for (const auto callee : uses[i].callees)
            callers[callee].push_back(i);
    }
    for (std::size_t next = 0; next < pending.size(); ++next)
        for (const auto caller : callers[pending[next]])
            if (!uses[caller].object) {
                uses[caller].object = true;
                pending.push_back(caller);
            }
    for (std::size_t i = 0; i < uses.size(); ++i)
        if (observers[i] && uses[i].object) return false;
    return true;
}
}  // namespace kirimasim::sir
