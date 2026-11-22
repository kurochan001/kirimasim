// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "kirimasim/analysis/tables.h"
#include <span>

namespace kirimasim::analysis {
/// Build the optional optimization table from verified, immutable SIR.
struct IterationRow {
    std::uint32_t component = UINT32_MAX;
    std::vector<sir::ProcessId> internalPredecessors;
};
/// When requested, retain the internal cycle edges in a companion to the
/// portable table of dependencies between levels. Both index verified SIR rows.
std::optional<TableData> analyzeIslands(const sir::VerifiedDesign& verified,
                                        std::vector<IterationRow>* iterations = nullptr);
/// Rebind a supplied table to the current design and the static-island pass.
bool matchesIslandAnalysis(const sir::VerifiedDesign&, const TableData&);
/// Narrow implicit whole-vector waits when every use of a load selects a
/// constant range. Explicit event controls retain their exact SIR triggers.
std::vector<sir::Trigger> islandSensitivity(const sir::Design&, const sir::Process&);
/// Stable process order, followed by totals. A missing table has unknown entries.
std::string islandReport(const sir::Design& design, const TableData* table,
                         std::span<const std::uint32_t> unsupportedIslands = {},
                         std::span<const IterationRow> iterations = {});
}  // namespace kirimasim::analysis
