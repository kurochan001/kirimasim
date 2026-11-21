// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "kirimasim/sir/verified.h"

namespace kirimasim::sir {

// Internal interchange for testing the P2 grammar and records before the format freezes.
// The writer returns no text if a verified value cannot be represented by the draft.
std::optional<std::string> print1(const VerifiedDesign& design);
VerifyOutcome parse1(std::string_view text);

}  // namespace kirimasim::sir
