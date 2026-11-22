// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "kirimasim/sir/verified.h"

namespace kirimasim::sir {

inline constexpr std::uint32_t kTextV1Version = 1;

/// The maximum input size for a format-1 design, including whitespace and comments.
inline constexpr std::size_t kMaxTextV1Bytes = std::size_t{64} * 1024 * 1024;

/// Produce the canonical format-1 bytes of a verified design within the format limits.
std::optional<std::string> print1(const VerifiedDesign& design);
VerifyOutcome parse1(std::string_view text);

}  // namespace kirimasim::sir
