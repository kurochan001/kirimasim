// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This version interprets SIR directly. It runs the SIR that front makes
// today; front reports the constructs not supported yet before anything
// runs (CLI.md 5). Compiling SIR to bytecode comes later.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
/// How deep the calls of one process may nest by default (CLI.md 2.7).
inline constexpr std::uint64_t kDefaultCallDepth = 10000;

enum class CondXMode : std::uint8_t { Off, Warn, Error };
enum class InitValuesMode : std::uint8_t { X, Zero, One, Random };

struct RunOptions {
    /// --max-steps: instructions one process may run without waiting; 0 is
    /// no limit. Going over ends the run with zero_time_limit
    /// (SCHEDULER_SPEC.md 16.2).
    std::uint64_t maxSteps = 0;
    /// --max-call-depth: how deep the calls of one process may nest, the
    /// calls of tasks that wait, which the frame chains, and the others,
    /// which run on the C++ stack, together. The call that would go deeper
    /// is not made, and the run ends with resource_limit (SCHEDULER_SPEC.md
    /// 16.2). The driver sizes the stack of the simulation by it
    /// (RUNTIME_ABI.md 5).
    std::uint64_t maxCallDepth = kDefaultCallDepth;
    /// The run-time plusargs, in order, without their leading + (CLI.md 2.4).
    std::vector<std::string> plusargs;
    /// For the waveform (WAVEFORM.md 3, 5): when the run started, which
    /// the driver reads ("YYYY-MM-DD hh:mm:ss UTC"; RUNTIME_ABI.md 9.3.8),
    /// "kirimasim <version>", and --wave, --wave-scope and --wave-glitches.
    std::string date;
    std::string version;
    std::string waveFile;
    std::vector<std::string> waveScopes;
    bool waveGlitches = false;
    /// --seed: the default seed of $urandom, $urandom_range and randcase
    /// (SYSTEM_TASKS.md 13.2).
    std::uint64_t seed = 0;
    /// --bounds-check=warn: report invalid array indices and packed
    /// selections once per source position without changing their values.
    bool boundsCheck = false;
    /// --cond-x: observe marked if/?: conditions and case expressions.
    /// Warnings are once per source position, with an evaluation count;
    /// Error ends the process as a run-time error of the design.
    CondXMode condX = CondXMode::Off;
    /// --init-values: defaults of newly created four-state source variables.
    InitValuesMode initValues = InitValuesMode::X;
    /// --report-pulses: note every pending update removed by inertial delay.
    bool reportPulses = false;
};

/// Runs the design to its end on the given context.
RunResult run(const sir::Design& design, kernel::Context& context, const RunOptions& options = {});

}  // namespace kirimasim::vm
