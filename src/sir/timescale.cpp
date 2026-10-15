#include "haremasim/sir/timescale.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace haremasim::sir {

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
    // The shortest decimal text that reads back as `value`: digits and an
    // exponent, in the form d.ddde[+-]xx.
    std::array<char, 64> buf{};
    const auto res =
        std::to_chars(buf.data(), buf.data() + buf.size(), value, std::chars_format::scientific);
    if (res.ec != std::errc()) return std::nullopt;
    const std::string_view text(buf.data(), static_cast<std::size_t>(res.ptr - buf.data()));
    const std::size_t epos = text.find('e');
    if (epos == std::string_view::npos) return std::nullopt;
    std::string digits;
    for (const char c : text.substr(0, epos))
        if (c != '.') digits += c;
    int exp10 = 0;
    std::string_view es = text.substr(epos + 1);
    if (es.starts_with("+")) es.remove_prefix(1);
    if (std::from_chars(es.data(), es.data() + es.size(), exp10).ec != std::errc())
        return std::nullopt;
    // value = digits * 10^(exp10 - (digits.size() - 1)) units. Count it in
    // precision steps: steps = digits * 10^shift.
    const int shift = exp10 - static_cast<int>(digits.size() - 1) + (scale.unit - scale.precision);
    std::uint64_t steps = 0;
    if (shift >= 0) {
        for (const char c : digits) {
            const auto m = mulChecked(steps, 10);
            if (!m) return std::nullopt;
            steps = *m + static_cast<std::uint64_t>(c - '0');
        }
        const auto f = pow10(shift);
        if (!f) return steps == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
        const auto s = mulChecked(steps, *f);
        if (!s) return std::nullopt;
        steps = *s;
    } else {
        // Keep the digits above the cut, and round on the first one below
        // it (half away from zero: a 5 or more rounds up).
        const auto drop = static_cast<std::size_t>(-shift);
        const std::size_t keep = digits.size() > drop ? digits.size() - drop : 0;
        for (std::size_t i = 0; i < keep; ++i) {
            const auto m = mulChecked(steps, 10);
            if (!m) return std::nullopt;
            steps = *m + static_cast<std::uint64_t>(digits[i] - '0');
        }
        const char first = digits.size() >= drop ? digits[keep] : '0';
        if (first >= '5') {
            if (steps == UINT64_MAX) return std::nullopt;
            ++steps;
        }
    }
    const auto f = pow10(scale.precision - global);
    if (!f) return steps == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
    return mulChecked(steps, *f);
}

}  // namespace haremasim::sir
