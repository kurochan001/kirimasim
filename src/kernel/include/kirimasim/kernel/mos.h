// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Pure full-interval device operations: MOS/switch contract 2, IEEE 1800
// 28.7/28.9/28.13/28.14. C++20, nonallocating, independent of SIR.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "kirimasim/kernel/strength.h"

namespace kirimasim::kernel {

enum class StrengthTransfer : std::uint8_t { Identity, Nonresistive, Resistive };
enum class MosKind : std::uint8_t { Nmos, Pmos, Rnmos, Rpmos, Cmos, Rcmos };

/// A point on the signed strength line; invalid points/kinds are rejected.
constexpr std::optional<std::int8_t> transferStrengthPoint(StrengthTransfer kind,
                                                           int point) noexcept {
    if (point < -7 || point > 7) return std::nullopt;
    int rank = point < 0 ? -point : point;
    switch (kind) {
        case StrengthTransfer::Identity: break;
        case StrengthTransfer::Nonresistive: rank = std::min(rank, 6); break;
        case StrengthTransfer::Resistive: rank = rank >= 6 ? 5 : (rank + 1) / 2; break;
        default: return std::nullopt;
    }
    return static_cast<std::int8_t>(point < 0 ? -rank : rank);
}

/// All three maps are monotone, so the complete image hull uses both ends.
constexpr std::optional<StrengthRange> transferStrength(StrengthTransfer kind,
                                                        StrengthRange input) noexcept {
    if (input.lo > input.hi) return std::nullopt;
    const auto lo = transferStrengthPoint(kind, input.lo);
    const auto hi = transferStrengthPoint(kind, input.hi);
    if (!lo || !hi) return std::nullopt;
    return StrengthRange{*lo, *hi};
}

namespace detail {
enum class MosConduction : std::uint8_t { Off, On, Uncertain };
constexpr MosConduction mosControl(W4 control, bool activeHigh) noexcept {
    if ((control.b & 1U) != 0) return MosConduction::Uncertain;
    return ((control.a & 1U) != 0) == activeHigh ? MosConduction::On : MosConduction::Off;
}
}  // namespace detail

/// Each control is scalar (bit zero; unused word bits are ignored). For
/// CMOS, first is the n-leg control, second is the p-leg control. The second
/// control is ignored by single-leg devices. No projection or scheduling.
constexpr std::optional<StrengthRange> mosStrength(MosKind kind, StrengthRange input, W4 first,
                                                   W4 second = {}) noexcept {
    using C = detail::MosConduction;
    C state = C::Off;
    bool resistive = false;
    switch (kind) {
        case MosKind::Rnmos: resistive = true; [[fallthrough]];
        case MosKind::Nmos: state = detail::mosControl(first, true); break;
        case MosKind::Rpmos: resistive = true; [[fallthrough]];
        case MosKind::Pmos: state = detail::mosControl(first, false); break;
        case MosKind::Rcmos: resistive = true; [[fallthrough]];
        case MosKind::Cmos: {
            const auto n = detail::mosControl(first, true);
            const auto p = detail::mosControl(second, false);
            state = n == C::On || p == C::On     ? C::On
                    : n == C::Off && p == C::Off ? C::Off
                                                 : C::Uncertain;
            break;
        }
        default: return std::nullopt;
    }
    // Validate even disabled inputs before returning a valid zero drive.
    auto output = transferStrength(
        resistive ? StrengthTransfer::Resistive : StrengthTransfer::Nonresistive, input);
    if (!output) return std::nullopt;
    if (state == C::Off) return StrengthRange{0, 0};
    if (state == C::Uncertain) {
        output->lo = std::min<std::int8_t>(output->lo, 0);
        output->hi = std::max<std::int8_t>(output->hi, 0);
    }
    return output;
}

}  // namespace kirimasim::kernel
