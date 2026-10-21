// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Time units and the conversion of delays to ticks of the global time
// precision (SCHEDULER_SPEC.md 1).
//
// Units are powers of ten of a second, written as exponents: -15 is 1 fs,
// -9 is 1 ns, 0 is 1 s. A time literal such as "10ps" is the exponent -11.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kirimasim::sir {

/// The smallest and largest exponents a time unit can have (1 fs to 100 s).
inline constexpr int kMinTimeExponent = -15;
inline constexpr int kMaxTimeExponent = 2;

/// The time unit and precision of a design element, as exponents.
/// precision <= unit.
struct TimeScale {
    int unit = -9;
    int precision = -9;
};

/// Reads "1ns", "10ps", "100us", "1s" and the like. Spaces between the
/// number and the unit are allowed. Returns nothing for anything else.
std::optional<int> parseTimeExponent(std::string_view text);

/// The text of an exponent: -9 -> "1ns", -11 -> "10ps", 2 -> "100s".
std::string timeExponentText(int exponent);

/// Converts a delay of `value` time units to ticks of the global precision
/// `global` (global <= scale.precision). An integer delay needs no
/// rounding. Returns nothing if the result does not fit in 64 bits.
std::optional<std::uint64_t> delayTicks(std::uint64_t value, TimeScale scale, int global);

/// Converts a real delay of `value` time units to ticks of `global`. The
/// value is first rounded to the precision of `scale`, half away from zero
/// (SCHEDULER_SPEC.md 1), then scaled; the second step is exact. Negative
/// values, NaN and results that do not fit in 64 bits give nothing.
///
/// The rounding takes the product of the double and the power of ten in
/// double arithmetic, as a delay known only at run time does, so 1.005
/// with two decimals of precision rounds to 1.00, the double being just
/// below 1.005, and 2.675 to 2.68, the product being 267.5.
std::optional<std::uint64_t> delayTicksReal(double value, TimeScale scale, int global);

/// Reads a time such as "10us" or "1.5ns" (CLI.md 2.5: a decimal number
/// and a unit, with no space) and converts it to ticks of `global`,
/// rounding half away from zero. Nothing for any other text, or if the
/// result does not fit in 64 bits.
std::optional<std::uint64_t> parseTimeTicks(std::string_view text, int global);

/// The text of a time of `ticks` steps of `global` (CLI.md 4): a whole
/// number and the largest unit that shows it exactly, such as "120 ns" or
/// "1500 ps". Time 0 is shown in the unit of `global`.
std::string timeText(std::uint64_t ticks, int global);

}  // namespace kirimasim::sir
