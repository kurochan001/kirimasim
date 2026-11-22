// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// vm: tier 1, which runs SIR processes on the shared kernel.
//
// This version interprets SIR directly. It runs the SIR that front makes
// today; front reports the constructs not supported yet before anything
// runs (CLI.md 5). Compiling SIR to bytecode comes later.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/run_policy.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verified.h"

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

using kernel::CondXMode;
using kernel::InitValuesMode;

struct IslandMember {
    sir::ProcessId process;
    std::uint32_t island, level;
    std::vector<sir::Trigger> triggers;
    kirima_resume_fn native = nullptr;
    kirima_resume_fn nativeKnown = nullptr;
};

struct KnownIsland {
    std::uint32_t island;
    std::vector<sir::SlotRange> inputs;
};

struct RunOptions {
    bool vpiCompileOnly = false;
    /// Process-ordered execution view, derived by the driver from verified SIR.
    std::vector<IslandMember> islands;
    std::vector<KnownIsland> knownIslands;
    /// Ordinary always @* processes keep their wait-first registration and
    /// re-arm individually. Entries are indexed by the verified process row.
    std::vector<kirima_resume_fn> combinationalBodies;
    bool tierCheck = false;
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

/// An independently clocked model for a C++ testbench. The verified design
/// and any generated modules in options outlive the model. Use it from one
/// thread at a time. Each instance owns its context and its pending events.
class Model {
public:
    static std::unique_ptr<Model> create(const sir::VerifiedDesign& design, RunResult& result,
                                         const RunOptions& options = {},
                                         std::unique_ptr<kernel::Context> context = {});
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    /// Resolve a public HDL name once. Alias views keep the same backing store.
    [[nodiscard]] std::optional<sir::SlotId> bind(std::string_view name) const;
    /// Whole packed integral element in kernel order (interleaved A/B for four states).
    /// Drive an externally supplied input, then settle with advanceTo(time()).
    bool drive(sir::SlotId id, std::span<const std::uint64_t> words, std::uint32_t element = 0);
    bool read(sir::SlotId id, std::span<std::uint64_t> words, std::uint32_t element = 0) const;
    [[nodiscard]] std::uint64_t time() const;
    /// Inclusive. False if the model ended or the requested time is in the past.
    bool advanceTo(std::uint64_t ticks);
    /// Stop at the current timestamp and run final procedures exactly once.
    RunResult finish();
    [[nodiscard]] const RunResult& result() const;

private:
    struct Impl;
    explicit Model(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

/// Runs the design to its end on the given context. Only a Design that has
/// passed sir::verifyDesign can be run (docs/notes/verified-design.md).
RunResult run(const sir::VerifiedDesign& design, kernel::Context& context,
              const RunOptions& options = {});

}  // namespace kirimasim::vm
