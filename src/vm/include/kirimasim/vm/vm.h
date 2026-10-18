// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This version interprets SIR directly. It runs the SIR that front makes
// today; front reports the constructs not supported yet before anything
// runs (CLI.md 5). Compiling SIR to bytecode comes later.

#pragma once

#include <cstdint>
#include <string>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"

namespace kirimasim::vm {

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
    /// The run-time plusargs, in order, without their leading + (CLI.md 2.4).
    std::vector<std::string> plusargs;
};

/// Runs the design to its end on the given context.
RunResult run(const sir::Design& design, kernel::Context& context, const RunOptions& options = {});

}  // namespace kirimasim::vm
