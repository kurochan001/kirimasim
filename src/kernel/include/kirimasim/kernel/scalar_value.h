// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>

#include "kirimasim/kernel/w4ops.h"

namespace kirimasim::kernel {

static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);

/// Real and shortreal storage (SIR_SPEC 3.2). Shortreal occupies the low 32 bits.
inline double readReal(W4 word, bool shortReal) {
    if (shortReal) return std::bit_cast<float>(static_cast<std::uint32_t>(word.a));
    return std::bit_cast<double>(word.a);
}
inline W4 writeReal(double value, bool shortReal) {
    if (shortReal) return {std::bit_cast<std::uint32_t>(static_cast<float>(value)), 0};
    return {std::bit_cast<std::uint64_t>(value), 0};
}
inline W4 realTruth(W4 value, bool shortReal) { return {readReal(value, shortReal) != 0.0, 0}; }

/// Integer to real or shortreal rounds once to the destination precision (VALUE_SEMANTICS 1.5).
template <typename T> inline T integerToFloating(W4 value, unsigned width, bool signedValue) {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
    const auto bits = value.a & ~value.b;
    return signedValue ? static_cast<T>(asSigned(bits, width)) : static_cast<T>(bits);
}

/// Branch conditions already hold a one-bit truth value (VALUE_SEMANTICS 3.9).
inline constexpr bool trueBit(W4 value) { return (value.a & ~value.b & 1U) != 0; }

/// Integral delay in precision ticks. X/Z anywhere makes zero, even if a higher
/// word is nonzero. A known value too wide for the scheduler has no tick count.
inline std::optional<std::uint64_t> delayValue(std::span<const W4> words) {
    if (words.empty()) return std::nullopt;
    bool overflow = false;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (words[i].b != 0) return 0;
        if (i != 0 && words[i].a != 0) overflow = true;
    }
    if (overflow) return std::nullopt;
    return words.front().a;
}

}  // namespace kirimasim::kernel
