// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/sir/timescale.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kirimasim::sir {

namespace {

/// 10^n for 0 <= n <= 19; nothing beyond, since 10^20 does not fit.
std::optional<std::uint64_t> pow10(int n) {
    if (n < 0 || n > 19) return std::nullopt;
    std::uint64_t r = 1;
    for (int i = 0; i < n; ++i)
        r *= 10;
    return r;
}

std::optional<std::uint64_t> mulChecked(std::uint64_t a, std::uint64_t b) {
    if (a != 0 && b > UINT64_MAX / a) return std::nullopt;
    return a * b;
}

struct UnitName {
    std::string_view name;
    int exponent;
};
constexpr std::array<UnitName, 6> kUnits{{
    {"s", 0},
    {"ms", -3},
    {"us", -6},
    {"ns", -9},
    {"ps", -12},
    {"fs", -15},
}};

/// n * 10 + the digit c, or nothing if it does not fit in 64 bits.
std::optional<std::uint64_t> appendDigit(std::uint64_t n, char c) {
    const auto m = mulChecked(n, 10);
    const auto d = static_cast<std::uint64_t>(c - '0');
    if (!m || *m > UINT64_MAX - d) return std::nullopt;
    return *m + d;
}

/// Rounds digits * 10^shift to a whole number, half away from zero.
/// `digits` is a non-empty string of decimal digits. Nothing if the result
/// does not fit in 64 bits.
std::optional<std::uint64_t> roundScaled(std::string_view digits, int shift) {
    std::uint64_t steps = 0;
    if (shift >= 0) {
        for (const char c : digits) {
            const auto m = appendDigit(steps, c);
            if (!m) return std::nullopt;
            steps = *m;
        }
        const auto f = pow10(shift);
        if (!f) return steps == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
        return mulChecked(steps, *f);
    }
    // Keep the digits above the cut, and round on the first one below it
    // (half away from zero: a 5 or more rounds up).
    const auto drop = static_cast<std::size_t>(-shift);
    const std::size_t keep = digits.size() > drop ? digits.size() - drop : 0;
    for (std::size_t i = 0; i < keep; ++i) {
        const auto m = appendDigit(steps, digits[i]);
        if (!m) return std::nullopt;
        steps = *m;
    }
    const char first = digits.size() >= drop ? digits[keep] : '0';
    if (first >= '5') {
        if (steps == UINT64_MAX) return std::nullopt;
        ++steps;
    }
    return steps;
}

}  // namespace

std::optional<int> parseTimeExponent(std::string_view text) {
    int digits = 0;
    if (text.starts_with("100")) {
        digits = 2;
        text.remove_prefix(3);
    } else if (text.starts_with("10")) {
        digits = 1;
        text.remove_prefix(2);
    } else if (text.starts_with("1")) {
        text.remove_prefix(1);
    } else {
        return std::nullopt;
    }
    while (text.starts_with(" "))
        text.remove_prefix(1);
    for (const UnitName& u : kUnits) {
        if (text != u.name) continue;
        const int e = u.exponent + digits;
        if (e < kMinTimeExponent || e > kMaxTimeExponent) return std::nullopt;
        return e;
    }
    return std::nullopt;
}

std::string timeExponentText(int exponent) {
    // The unit is the largest one not above the exponent.
    for (const UnitName& u : kUnits) {
        if (exponent < u.exponent) continue;
        const int extra = exponent - u.exponent;
        const char* digits = extra == 0 ? "1" : extra == 1 ? "10" : "100";
        return std::string(digits) + std::string(u.name);
    }
    return "?";
}

std::optional<std::uint64_t> delayTicks(std::uint64_t value, TimeScale scale, int global) {
    const std::optional<std::uint64_t> f = pow10(scale.unit - global);
    if (!f) return value == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
    return mulChecked(value, *f);
}

std::optional<std::uint64_t> delayTicksReal(double value, TimeScale scale, int global) {
    if (!(value >= 0) || std::isinf(value)) return std::nullopt;
    // The value in steps of the precision is the double product, rounded
    // half away from zero, as a delay known only at run time has to be
    // (SCHEDULER_SPEC.md 1).
    double unitSteps = 1;
    for (int i = scale.precision; i < scale.unit; ++i)
        unitSteps *= 10;
    const double steps = std::round(value * unitSteps);
    if (!(steps < 18446744073709551616.0)) return std::nullopt;
    const auto n = static_cast<std::uint64_t>(steps);
    const auto f = pow10(scale.precision - global);
    if (!f) return n == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
    return mulChecked(n, *f);
}

std::optional<std::uint64_t> parseTimeTicks(std::string_view text, int global) {
    std::string digits;
    int fraction = -1;  // digits after the point; -1 before the point
    std::size_t i = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            digits += c;
            if (fraction >= 0) ++fraction;
        } else if (c == '.' && fraction < 0 && !digits.empty()) {
            fraction = 0;
        } else {
            break;
        }
    }
    if (digits.empty() || fraction == 0) return std::nullopt;
    const std::string_view unit = text.substr(i);
    for (const UnitName& u : kUnits) {
        if (unit != u.name) continue;
        return roundScaled(digits, u.exponent - global - std::max(fraction, 0));
    }
    return std::nullopt;
}

std::string timeText(std::uint64_t ticks, int global) {
    // The largest unit not above `exponent`, and how many zeros it needs.
    const auto inUnit = [](std::string number, int exponent) {
        for (const UnitName& u : kUnits) {
            if (exponent < u.exponent) continue;
            if (number != "0") number.append(static_cast<std::size_t>(exponent - u.exponent), '0');
            return number + " " + std::string(u.name);
        }
        return number + " ?";
    };
    if (ticks == 0) return inUnit("0", global);
    int exponent = global;
    while (ticks % 10 == 0) {
        ticks /= 10;
        ++exponent;
    }
    return inUnit(std::to_string(ticks), exponent);
}

}  // namespace kirimasim::sir
