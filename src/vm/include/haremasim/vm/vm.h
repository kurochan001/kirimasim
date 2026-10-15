// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This first version interprets SIR directly and covers the thin path of P1
// (stage M3a): system task calls ($display, $write, $finish), branches and
// returns in initial processes, all in the Active region. Bytecode, waits
// and the other instructions come in later stages.

#pragma once

#include <cstdint>
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

/// Limits of the VM (CLI.md 2.2).
struct RunOptions {
    /// --max-steps: instructions one process may run without waiting; 0 is
    /// no limit. Going over ends the run with zero_time_limit
    /// (SCHEDULER_SPEC.md 16.2).
    std::uint64_t maxSteps = 0;
};

/// Runs the design to its end on the given context.
RunResult run(const sir::Design& design, kernel::Context& context, const RunOptions& options = {});

}  // namespace haremasim::vm
