// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/sir/monitor_reads.h"
#include "kirimasim/sir/systasks.h"
#include <algorithm>

namespace kirimasim::sir {
const std::vector<FrameVar>& effectiveFrame(const Design& design, Owner body) {
    if (body.kind == Owner::Kind::Process) return design.process(body.asProcess()).frame;
    const auto& sub = design.subroutine(body.asSubroutine());
    if (!readsWaiterFrame(sub.kind)) return sub.frame;
    return sub.frameOf.kind == Owner::Kind::Process
               ? design.process(sub.frameOf.asProcess()).frame
               : design.subroutine(sub.frameOf.asSubroutine()).frame;
}
namespace {
/// The instructions whose first operand is the slot they write or the
/// event they trigger.
bool isWriteTarget(Op op) {
    switch (op) {
        case Op::Store:
        case Op::Nba:
        case Op::NbaDelay:
        case Op::DriveDelay:
        case Op::StoreElem:
        case Op::NbaElem:
        case Op::NbaElemDelay:
        case Op::StoreBits:
        case Op::NbaBits:
        case Op::NbaBitsDelay:
        case Op::NbaStrChar:
        case Op::NbaStrCharDelay:
        case Op::Trigger:
        case Op::TriggerNb: return true;
        default: return false;
    }
}

}  // namespace
MonitorReads monitorReads(const Design& design, Owner root,
                          std::span<const PatternShape> patterns) {
    MonitorReads deps;
    struct Cursor {
        Owner body;
        std::size_t block = 0, instruction = 0;
    };
    std::vector<Cursor> stack{{root}};
    std::vector<bool> seen(design.subroutines.size());
    if (root.kind == Owner::Kind::Subroutine) seen[root.id] = true;
    while (!stack.empty()) {
        auto& cursor = stack.back();
        const auto body = cursor.body;
        const auto& blocks = body.kind == Owner::Kind::Process
                                 ? design.process(body.asProcess()).blocks
                                 : design.subroutine(body.asSubroutine()).blocks;
        if (cursor.block == blocks.size()) {
            stack.pop_back();
            continue;
        }
        const auto& instructions = blocks[cursor.block].instrs;
        if (cursor.instruction == instructions.size()) {
            ++cursor.block;
            cursor.instruction = 0;
            continue;
        }
        const auto& in = instructions[cursor.instruction];
        // Visit the callee before its call operands, preserving declaration order.
        if (in.op == Op::Call && !seen[in.operands[0].id]) {
            seen[in.operands[0].id] = true;
            stack.push_back({Owner::of(in.operands[0].asSubroutine())});
            continue;
        }
        ++cursor.instruction;
        const bool own = stack.size() == 1;
        const auto& frame = effectiveFrame(design, body);
        if (in.op == Op::StrengthLoad) deps.strength = true;
        if (in.op == Op::FormatPattern && !in.operands.empty() &&
            in.operands[0].id < patterns.size() && patterns[in.operands[0].id].heap)
            deps.heap = true;
        if (isContainerOp(in.op) || in.op == Op::LoadRef || in.op == Op::LoadRefElem)
            deps.heap = true;
        if (in.op == Op::Builtin && builtinMethod(in.operands[0].text) == BuiltinMethod::Triggered)
            deps.events = true;
        // A ref of the body's own frame (that of the waiter); the
        // refs of the functions it calls are bound by the call.
        const bool forwardedRef = in.op == Op::RefMake &&
                                  in.operands[0].kind == Operand::Kind::Frame &&
                                  frame[in.operands[0].id].type.kind == TypeKind::Ref;
        // Forwarding an enclosing ref also reads its binding. An
        // empty container has no heap identity to observe; replacing
        // its variable must still reevaluate the called expression.
        if (own && (in.op == Op::LoadRef || in.op == Op::LoadRefElem || forwardedRef) &&
            in.operands[0].kind == Operand::Kind::Frame) {
            std::uint32_t leaf = 0;
            if (in.op == Op::LoadRef && in.operands.size() == 2 &&
                in.operands[1].kind == Operand::Kind::Const) {
                const Constant& c = design.constants[in.operands[1].id];
                leaf = c.aWords.empty() ? 0 : static_cast<std::uint32_t>(c.aWords[0]);
            }
            deps.refs.push_back({in.operands[0].asFrame(), leaf, in.op == Op::LoadRefElem});
        }
        // The target of a write or a trigger is not read: an
        // expression whose function triggers an event does not wait
        // on it, which would wake it again for its own trigger.
        const bool target = isWriteTarget(in.op);
        for (std::size_t i = 0; i < in.operands.size(); ++i) {
            const Operand& o = in.operands[i];
            const bool frameWrite = (target || in.op == Op::Fstore) && i == 0;
            const bool outputArgument =
                in.op == Op::Call && i > 0 &&
                i <= design.subroutine(in.operands[0].asSubroutine()).params.size() &&
                design.subroutine(in.operands[0].asSubroutine()).params[i - 1].dir ==
                    Direction::Output;
            if (own && o.kind == Operand::Kind::Frame && !frameWrite && !outputArgument &&
                frame[o.id].type.kind != TypeKind::Ref)
                deps.frames.push_back({o.asFrame(), in.op == Op::Fload ? o.lsb : 0,
                                       in.op == Op::Fload ? o.width : 0});
            if (o.kind != Operand::Kind::Slot || (target && i == 0) ||
                (in.op == Op::DriveDelay && i >= 5 && (i - 5) % 2 == 0))
                continue;
            const bool whole = in.op != Op::Load;
            deps.reads.push_back({o.asSlot(), whole ? 0 : o.firstElem, whole ? 0 : o.numElems,
                                  whole ? 0 : o.lsb, whole ? 0 : o.width});
        }
    }
    const auto called = [&](SlotId slot) {
        const auto sub = design.slot(slot).subroutine;
        return sub != kNone && seen[sub.index()];
    };
    std::erase_if(deps.reads, [&](const SlotRange& r) { return called(r.slot); });
    for (std::uint32_t i = 0; i < design.slots.size(); ++i)
        if (isContainer(design.slots[i].type) && called(SlotId{i}))
            deps.privateContainers.push_back(SlotId{i});
    return deps;
}
}  // namespace kirimasim::sir
