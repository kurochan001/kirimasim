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
}  // namespace kirimasim::sir
