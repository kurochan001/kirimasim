// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// SIR (slot-indexed IR): the intermediate representation shared by all tiers.

#pragma once

#include <cstdint>

namespace kirimasim::sir {

/// Version of the current canonical SIR interchange format.
std::uint32_t formatVersion();

}  // namespace kirimasim::sir
