// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "native_model.h"
#include <iosfwd>

namespace kirimasim::driver {
void writeTierReport(std::ostream&, const sir::Design&, const analysis::TableData*,
                     const IslandModules&, std::span<const vm::ProcessTierCounts>);
}  // namespace kirimasim::driver
