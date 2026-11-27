// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/analysis/tables.h"

namespace kirimasim::codegen {
/// Verified process-frame cells supported by generated counted loops.
inline bool supportsLoopField(const sir::FrameVar& field) {
    return field.home == sir::FrameHome::Ordinary &&
           (field.type == sir::s2(32) || field.type == sir::u2(32)) && field.dims.empty() &&
           !field.readonly && field.captureSource == sir::kNone &&
           field.activationScope == sir::kNone;
}

struct BodyEmission {
    bool attempted = false;
    std::string rejection;
};

struct CountedLoopSlot {
    sir::ProcessId process;
    sir::SlotId slot;
};

struct Model {
    std::string source;
    bool hasFusion = false;
    std::vector<sir::ProcessId> bodies;
    std::vector<CountedLoopSlot> countedLoopSlots;
    std::vector<sir::ProcessId> checkpointBodies;  ///< Replay original SIR branch checkpoints.
    std::vector<std::uint32_t> unsupportedIslands;
    /// Optional diagnostics for the regular model, indexed by SIR process ID.
    std::vector<BodyEmission> emissions;
};
/// Emit deterministic C++20 for complete supported islands. A finite longest
/// path inside the member bounds the instruction budget on every evaluation.
struct EmissionOptions {
    bool includeOrdinary = false;
    bool allowFrameLoops = false;
    bool allowScalarFunctions = false;
    bool approximateTwoState = false;
    bool reportEmissions = false;
    bool checkFusion = false;
};
Model emitModel(const sir::VerifiedDesign&, const analysis::TableData&, std::uint64_t maxSteps,
                bool checkBounds, EmissionOptions options = {});
struct KnownIsland {
    std::uint32_t island;
    std::vector<sir::SlotRange> inputs;
};
struct KnownModel {
    Model model;
    std::vector<KnownIsland> guards;
};
/// A second instance of the same C module ABI. Guards include internal state
/// the island may read. Only islands closed over known inputs have an entry.
KnownModel emitKnownModel(const sir::VerifiedDesign&, const analysis::TableData&,
                          std::uint64_t maxSteps, bool checkBounds, bool checkFusion = false);
}  // namespace kirimasim::codegen
