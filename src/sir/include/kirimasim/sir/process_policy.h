// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/sir/design.h"

namespace kirimasim::sir {

/// Derived from the verified process, hierarchy and trigger records. These
/// decisions are available to any execution tier without extending the wire.
enum class ProcessStart {
    Invalid,
    Child,
    StaticInit,
    Postponed,
    Continuous,
    Always,
    Initial,
    Combinational,
    ArmNow,
    ArmAfterRegistration,
    RunEntryBeforeStart,
    Final,
    Override
};
enum class ProcessRegistration { Active, Reactive, ProgramInitial, NetResolver };
struct ProcessPolicy {
    ProcessStart start;
    ProcessRegistration registration;
    bool procedureRng;
};
ProcessPolicy processPolicy(const Design& design, const Process& process);
/// Continuous drivers and active procedural overrides track input changes
/// during execution; other procedures re-arm after executing their body.
inline bool sensitiveDuringExecution(ProcessKind kind, bool holding) {
    return kind == ProcessKind::Cont || (kind == ProcessKind::Override && holding);
}
/// Formal storage and copy-out policy (SIR_SPEC 7.4). Arrays passed by ref
/// use the referenced storage; an output is copied back only on normal return.
/// For a verified scalar function whose only work copies ordinary locals,
/// return the input returned without conversion. Callers retain resource limits
/// and any observable initialization of its automatic storage.
std::optional<std::uint32_t> returnedScalarInput(const Subroutine&);
bool isArrayParam(const Subroutine&, const Param&, const Design&);
inline bool isRefParam(const Param& param) noexcept {
    return param.dir == Direction::Ref || param.dir == Direction::ConstRef;
}
inline bool copiesOutput(const Param& param) noexcept {
    return param.dir == Direction::Output || param.dir == Direction::Inout;
}
}  // namespace kirimasim::sir
