// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This version interprets SIR directly. It runs initial, always, always_comb
// and always_ff processes with integral values of up to 64 bits: loads and
// stores (also of array elements and run-time bit positions), NBAs, the
// value instructions, branches, delays and event controls on fixed signals,
// and $display, $write, $finish, $stop and $time. Bytecode, fork, final,
// wider values and the other instructions come in later stages.

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
};

/// Runs the design to its end on the given context.
RunResult run(const sir::Design& design, kernel::Context& context, const RunOptions& options = {});

}  // namespace kirimasim::vm
