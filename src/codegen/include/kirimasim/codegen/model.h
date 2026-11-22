// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/analysis/tables.h"

namespace kirimasim::codegen {
struct Model {
    std::string source;
    std::vector<sir::ProcessId> bodies;
    std::vector<std::uint32_t> unsupportedIslands;
};
/// Emit deterministic C++20 for complete supported islands. A finite longest
/// path inside the member bounds the instruction budget on every evaluation.
Model emitModel(const sir::VerifiedDesign&, const analysis::TableData&, std::uint64_t maxSteps,
                bool checkBounds, bool includeOrdinary = false);
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
                          std::uint64_t maxSteps, bool checkBounds);
}  // namespace kirimasim::codegen
