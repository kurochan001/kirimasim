// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This first version interprets SIR directly and covers the thin path of P1
// (stage M3a): system task calls ($display, $write, $finish), branches and
// returns in initial processes, all in the Active region. Bytecode, waits
// and the other instructions come in later stages.

#pragma once

#include <string>

#include "haremasim/kernel/context.h"
#include "haremasim/sir/design.h"

namespace haremasim::vm {

/// How a run ended.
struct RunResult {
    kernel::EndReason reason = kernel::EndReason::Quiescent;
    /// For EndReason::Internal: what went wrong.
    std::string error;
};

/// Runs the design to its end on the given context.
RunResult run(const sir::Design& design, kernel::Context& context);

}  // namespace haremasim::vm
