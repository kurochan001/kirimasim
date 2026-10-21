// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// $random and the $dist_* functions, written from SYSTEM_TASKS.md 12
// alone. Every real operation is its own statement, so that nothing is
// contracted into a fused multiply-add (12.2).

#include "kirimasim/kernel/random.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace kirimasim::kernel {

namespace {

constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
constexpr double kStep = 1.0 / 8388608.0;  // 2^-23

/// The state of one call: the seed as a 32-bit pattern (12.3), and the
/// draws and steps built on it (12.4, 12.5).
class Stream {
public:
    explicit Stream(std::int32_t seed) : s_(static_cast<std::uint32_t>(seed)) {}
    [[nodiscard]] std::int32_t seed() const { return static_cast<std::int32_t>(s_); }

    /// D(lo, hi), lo < hi (12.4).
    double draw(std::int64_t lo, std::int64_t hi) {
        if (s_ == 0) s_ = 259341593U;
        s_ = 69069U * s_ + 1U;
        const std::uint32_t m = s_ >> 9;
        const double f = 1.0 + static_cast<double>(m) * kStep;  // exact
        const double g = f + f * kStep;                         // exact
        const double h = g - 1.0;                               // exact
        const double scaled = static_cast<double>(hi - lo) * h;
        return scaled + static_cast<double>(lo);
    }
    double u() { return draw(0, 1); }
    double w() { return draw(-1, 1); }

    /// N(mean, sd), the polar method.
    double normal(double mean, double sd) {
        for (;;) {
            const double a1 = w();
            const double a2 = w();
            const double s1 = a1 * a1;
            const double s2 = a2 * a2;
            const double sum = s1 + s2;
            if (sum >= 1.0 || sum == 0.0) continue;
            const double l = std::log(sum);
            const double t = -2.0 * l;
            const double q = t / sum;
            const double z = a1 * std::sqrt(q);
            const double zs = z * sd;
            return zs + mean;
        }
    }
    /// E(mean).
    double exponential(double mean) {
        const double y = u();
        const double l = -std::log(y);
        return l * mean;
    }
    /// P(mean).
    std::int64_t poisson(double mean) {
        const double limit = std::exp(-mean);
        std::int64_t j = 0;
        double prod = u();
        while (limit < prod) {
            ++j;
            const double v = u();
            prod = v * prod;
        }
        return j;
    }
    /// C(df), df > 0.
    double chiSquare(std::int32_t df) {
        double acc = 0.0;
        if (df % 2 != 0) {
            const double z = normal(0.0, 1.0);
            acc = z * z;
        }
        for (std::int32_t i = 0; i < df / 2; ++i) {
            const double e = exponential(1.0);
            const double twice = 2.0 * e;
            acc = acc + twice;
        }
        return acc;
    }
    /// T(df), df > 0.
    double studentT(std::int32_t df) {
        const double y = chiSquare(df);
        const double den = std::sqrt(y / static_cast<double>(df));
        const double z = normal(0.0, 1.0);
        return z / den;
    }
    /// R(k, mean), k > 0.
    double erlang(std::int32_t k, double mean) {
        double prod = 1.0;
        for (std::int32_t i = 0; i < k; ++i) {
            const double v = u();
            prod = prod * v;
        }
        const double l = std::log(prod);
        const double t = -mean * l;
        return t / static_cast<double>(k);
    }

private:
    std::uint32_t s_;
};

/// A real converted to 32 bits where C leaves it undefined: the nearest
/// of -2^31 and 2^31 - 1, and 0 for a NaN (12.6).
std::int32_t toInt(double x) {
    if (std::isnan(x)) return 0;
    const double t = std::trunc(x);
    if (t >= 2147483648.0) return kMax;
    if (t < -2147483648.0) return kMin;
    return static_cast<std::int32_t>(t);
}

/// round(y) (12.6): half away from zero, after the rounding of y + 0.5.
std::int32_t roundToInt(double y) {
    if (y >= 0) {
        const double s = y + 0.5;
        return toInt(s);
    }
    const double s = -y + 0.5;
    return -toInt(s);
}

/// k of $dist_uniform: trunc(y), or trunc(y - 1) for a negative y.
std::int64_t wholePart(double y) {
    if (y >= 0) return static_cast<std::int64_t>(std::trunc(y));
    const double less = y - 1.0;
    return static_cast<std::int64_t>(std::trunc(less));
}

std::int32_t toInt32(std::int64_t k) {
    if (k > kMax) return kMax;
    if (k < kMin) return kMin;
    return static_cast<std::int32_t>(k);
}

}  // namespace

DistResult distUniform(std::int32_t& seed, std::int32_t start, std::int32_t end) {
    if (start >= end) return {start, false};
    Stream st(seed);
    std::int64_t k = 0;
    if (end != kMax) {
        const std::int64_t e = std::int64_t{end} + 1;
        k = wholePart(st.draw(start, e));
        if (k < start) k = start;
        if (k >= e) k = end;
    } else if (start != kMin) {
        const std::int64_t b = std::int64_t{start} - 1;
        const double d = st.draw(b, end);
        k = wholePart(d + 1.0);
        if (k <= b) k = start;
        if (k > end) k = end;
    } else {
        // The whole range, with no clamping: k can be 2^31 + 511.
        const double v = st.draw(kMin, kMax);
        const double s = v + 2147483648.0;
        const double y = s / 4294967295.0;
        const double p = y * 4294967296.0;
        k = wholePart(p - 2147483648.0);
    }
    seed = st.seed();
    return {toInt32(k), false};
}

std::int32_t randomInt(std::int32_t& seed) { return distUniform(seed, kMin, kMax).value; }

DistResult distNormal(std::int32_t& seed, std::int32_t mean, std::int32_t sd) {
    Stream st(seed);
    const double v = st.normal(static_cast<double>(mean), static_cast<double>(sd));
    seed = st.seed();
    return {roundToInt(v), false};
}

DistResult distExponential(std::int32_t& seed, std::int32_t mean) {
    if (mean <= 0) return {0, true};
    Stream st(seed);
    const double v = st.exponential(static_cast<double>(mean));
    seed = st.seed();
    return {roundToInt(v), false};
}

DistResult distPoisson(std::int32_t& seed, std::int32_t mean) {
    if (mean <= 0) return {0, true};
    Stream st(seed);
    const std::int64_t j = st.poisson(static_cast<double>(mean));
    seed = st.seed();
    return {toInt32(j), false};
}

DistResult distChiSquare(std::int32_t& seed, std::int32_t df) {
    if (df <= 0) return {0, true};
    Stream st(seed);
    const double v = st.chiSquare(df);
    seed = st.seed();
    return {roundToInt(v), false};
}

DistResult distT(std::int32_t& seed, std::int32_t df) {
    if (df <= 0) return {0, true};
    Stream st(seed);
    const double v = st.studentT(df);
    seed = st.seed();
    return {roundToInt(v), false};
}

DistResult distErlang(std::int32_t& seed, std::int32_t k, std::int32_t mean) {
    if (k <= 0) return {0, true};
    Stream st(seed);
    const double v = st.erlang(k, static_cast<double>(mean));
    seed = st.seed();
    return {roundToInt(v), false};
}

}  // namespace kirimasim::kernel
