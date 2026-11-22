// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/sir/process_policy.h"

#include <algorithm>

namespace kirimasim::sir {
ProcessPolicy processPolicy(const Design& design, const Process& p) {
    using Start = ProcessStart;
    using Reg = ProcessRegistration;
    const auto group =
        design.instance(p.instance).kind == InstanceKind::Program ? Reg::Reactive : Reg::Active;
    if (p.parent.kind != Owner::Kind::None) return {Start::Child, group, false};
    switch (p.kind) {
        case ProcessKind::StaticInit: return {Start::StaticInit, Reg::Active, false};
        case ProcessKind::Postponed: return {Start::Postponed, Reg::Active, false};
        case ProcessKind::Cont:
            return {p.armBeforeStart ? Start::RunEntryBeforeStart : Start::Continuous,
                    p.netResolver ? Reg::NetResolver : group, false};
        case ProcessKind::Initial:
            return {p.armBeforeStart ? Start::RunEntryBeforeStart : Start::Initial,
                    group == Reg::Reactive ? Reg::ProgramInitial : group, true};
        case ProcessKind::General:
            return {p.armBeforeStart ? Start::RunEntryBeforeStart : Start::Always, group, true};
        case ProcessKind::Comb:
        case ProcessKind::Latch: return {Start::Combinational, group, true};
        case ProcessKind::Ff:
        case ProcessKind::FfPlain:
        case ProcessKind::CombAt: {
            const bool watch = std::any_of(p.triggers.begin(), p.triggers.end(),
                                           [](const Trigger& t) { return t.watch != kNone; });
            if (!watch) return {Start::ArmNow, group, true};
            return {p.armBeforeStart ? Start::RunEntryBeforeStart : Start::ArmAfterRegistration,
                    group, true};
        }
        case ProcessKind::Final: return {Start::Final, group, true};
        case ProcessKind::Override: return {Start::Override, group, false};
    }
    return {Start::Invalid, group, false};
}
bool isArrayParam(const Subroutine& s, const Param& p, const Design& d) {
    return p.frameVar != kNone ? !s.frame[p.frameVar.index()].dims.empty() &&
                                     s.frame[p.frameVar.index()].type.kind != TypeKind::Ref
                               : !d.slot(p.slot).dims.empty();
}
std::optional<std::uint32_t> returnedScalarInput(const Subroutine& sub) {
    const auto scalar = [](Type type) { return isIntegral(type) && type.width <= 64; };
    if (!sub.activation.scopes.empty() || !sub.activation.extents.empty() ||
        sub.activation.inheritCaptures || sub.frameOf.kind != Owner::Kind::None)
        return {};
    if (!sub.automatic || sub.kind != SubroutineKind::Function || sub.mayWait ||
        sub.classRole != ClassRole::None || sub.blocks.size() != 1 || !scalar(sub.returnType))
        return {};
    std::vector<std::optional<std::uint32_t>> frame(sub.frame.size()), temps(sub.numTemps);
    if (std::ranges::any_of(sub.frame, [&](const auto& var) {
            return var.home != FrameHome::Ordinary || var.defaultInit || !var.dims.empty() ||
                   !scalar(var.type);
        }))
        return {};
    for (std::uint32_t i = 0; i < sub.params.size(); ++i) {
        const auto& param = sub.params[i];
        if (param.dir != Direction::Input || param.frameVar == kNone) return {};
        frame[param.frameVar.index()] = i;
    }
    for (const auto& in : sub.blocks.front().instrs) {
        const auto& ops = in.operands;
        if (in.op == Op::Ret && ops.size() == 1 && ops[0].kind == Operand::Kind::Temp) {
            const auto index = temps[ops[0].id];
            if (index && sub.params[*index].type == sub.returnType) return index;
            return {};
        }
        if (ops.empty() || ops[0].kind != Operand::Kind::Frame || ops[0].lsb || ops[0].width ||
            ops[0].firstElem || ops[0].numElems)
            return {};
        const auto id = ops[0].id;
        if (in.op == Op::Fload && in.type == sub.frame[id].type) {
            temps[in.result.index()] = frame[id];
        } else if (in.op == Op::Fstore && ops.size() == 2 && ops[1].kind == Operand::Kind::Const) {
            // A local initializer stops this field carrying a formal until overwritten.
            frame[id].reset();
        } else if (in.op == Op::Fstore && ops.size() == 2 && ops[1].kind == Operand::Kind::Temp) {
            const auto from = temps[ops[1].id];
            if (!from || sub.frame[id].type != sub.params[*from].type) return {};
            frame[id] = from;
        } else
            return {};
    }
    return {};
}

}  // namespace kirimasim::sir
