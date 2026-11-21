// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// analysis: X-reachability analysis, static island detection, levelization, observability.
// The optional analysis-table contract is in tables.h.

#pragma once

#include <string_view>

namespace kirimasim::analysis {

/// Name of the analysis library.
std::string_view libraryName();

}  // namespace kirimasim::analysis
