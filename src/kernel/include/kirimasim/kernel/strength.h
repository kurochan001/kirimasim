// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The resolution of nets with strengths (VALUE_SEMANTICS.md 5).
//
// This header is part of the kernel's public interface. Generated code reads
// it, so it must stay compilable as C++20.
//
// The reference definition, which tries every choice of points one by one,
// lives in valsem; tests/unit compares the two.

#pragma once

#include <cstdint>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// The kinds of net that resolve (VALUE_SEMANTICS.md 5.1).
enum class NetKind : std::uint8_t {
    Wire,
    Tri,
    Wand,
    Triand,
    Wor,
    Trior,
    Tri0,
    Tri1,
    Supply0,
    Supply1,
    Uwire,
};

/// One bit's complete strength interval (VALUE_SEMANTICS.md 5.2):
/// -7 <= lo <= hi <= 7. Negative points are 0, positive points are 1,
/// and zero is highz. Keep both endpoints until a four-state read.
struct StrengthRange {
    std::int8_t lo = 0;
    std::int8_t hi = 0;
    friend bool operator==(const StrengthRange&, const StrengthRange&) = default;
};

/// Project a valid strength interval to a four-state bit (5.2).
constexpr W4 strengthValue(StrengthRange signal) noexcept {
    if (signal.lo == 0 && signal.hi == 0) return {0, 1};
    if (signal.hi < 0) return {0, 0};
    if (signal.lo > 0) return {1, 0};
    return {1, 1};  // x, including L and H
}

/// One driver of a net: its value and its strengths for 1 and for 0, each
/// from 0 (highz) to 7 (supply). A driver with a side slot (the L and H of
/// a three-state gate, SIR_SPEC.md 3.5) drives its 1 side only on the bits
/// set in `one` and its 0 side only on those set in `zero`; on the others
/// that strength is highz.
struct StrengthDriver {
    W4 value;
    std::uint8_t s1;
    std::uint8_t s0;
    std::uint64_t one = ~std::uint64_t{0};
    std::uint64_t zero = ~std::uint64_t{0};
};

namespace detail {

// The outcome of a partial choice of points (5.4, step 2) only depends on
// the greatest strength m chosen so far and on which values were chosen at
// m. A set of such states is a bit mask: bit 0 is m = 0 (only highz so
// far); bit 1 + 3 * (m - 1) + (sides - 1) is m from 1 to 7 with the values
// at m (sides: 1 for 0 only, 2 for 1 only, 3 for both).
inline std::uint32_t stateBit(int m, int sides) {
    return m == 0 ? 1U : 1U << static_cast<unsigned>(1 + 3 * (m - 1) + (sides - 1));
}

/// The states after adding one more driver whose points are lo..hi.
inline std::uint32_t addRange(std::uint32_t states, int lo, int hi) {
    std::uint32_t out = 0;
    for (int m = 0; m <= 7; ++m) {
        for (int sides = 1; sides <= 3; ++sides) {
            if (m == 0 && sides != 1) continue;
            if ((states & stateBit(m, sides)) == 0) continue;
            for (int p = lo; p <= hi; ++p) {
                const int s = p < 0 ? -p : p;
                const int side = p < 0 ? 1 : 2;
                if (s > m)
                    out |= stateBit(s, side);
                else if (s == m && s != 0)
                    out |= stateBit(m, sides | side);
                else
                    out |= stateBit(m, sides);
            }
        }
    }
    return out;
}

/// Finish the point-choice states with the net's own driver and tie rule.
inline StrengthRange finishRange(NetKind kind, std::uint32_t states) noexcept {
    const auto range = [&](int lo, int hi) { states = addRange(states, lo, hi); };
    // The net's own pull or supply (5.3).
    if (kind == NetKind::Tri0) range(-5, -5);
    if (kind == NetKind::Tri1) range(5, 5);
    if (kind == NetKind::Supply0) range(-7, -7);
    if (kind == NetKind::Supply1) range(7, 7);

    const bool wand = kind == NetKind::Wand || kind == NetKind::Triand;
    const bool wor = kind == NetKind::Wor || kind == NetKind::Trior;
    int lo = 8;
    int hi = -8;
    for (int m = 0; m <= 7; ++m) {
        for (int sides = 1; sides <= 3; ++sides) {
            if ((m == 0 && sides != 1) || (states & detail::stateBit(m, sides)) == 0) continue;
            int l = 0;
            int h = 0;
            if (m != 0) {
                if (sides == 1 || (sides == 3 && wand)) {
                    l = h = -m;
                } else if (sides == 2 || wor) {
                    l = h = m;
                } else {
                    l = -m;
                    h = m;
                }
            }
            lo = l < lo ? l : lo;
            hi = h > hi ? h : hi;
        }
    }
    return {static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)};
}

}  // namespace detail

/// Resolve valid strength intervals directly (5.4), including ambiguous
/// intervals wholly on one side of zero. No allocation or early projection.
inline StrengthRange resolveStrengths(NetKind kind, const StrengthRange* signals,
                                      unsigned count) noexcept {
    std::uint32_t states = detail::stateBit(0, 1);
    for (unsigned d = 0; d < count; ++d)
        states = detail::addRange(states, signals[d].lo, signals[d].hi);
    return detail::finishRange(kind, states);
}

/// The complete result interval of one driver bit (5.3, 5.4). Driver
/// strengths are 0..7 and bit is 0..63; side masks retain L/H distinctions.
inline StrengthRange resolveStrengthBit(NetKind kind, const StrengthDriver* drivers, unsigned count,
                                        unsigned bit) noexcept {
    std::uint32_t states = detail::stateBit(0, 1);
    const auto range = [&](int lo, int hi) { states = detail::addRange(states, lo, hi); };
    for (unsigned d = 0; d < count; ++d) {
        const unsigned a = (drivers[d].value.a >> bit) & 1U;
        const unsigned b = (drivers[d].value.b >> bit) & 1U;
        const int s1 = ((drivers[d].one >> bit) & 1U) != 0 ? drivers[d].s1 : 0;
        const int s0 = ((drivers[d].zero >> bit) & 1U) != 0 ? drivers[d].s0 : 0;
        if (b == 0)
            range(a != 0 ? s1 : -s0, a != 0 ? s1 : -s0);  // 1 or 0
        else if (a != 0)
            range(-s0, s1);  // x
        else
            range(0, 0);  // z
    }
    return detail::finishRange(kind, states);
}

/// A four-state read of one resolved driver bit (5.2).
inline W4 resolveBit(NetKind kind, const StrengthDriver* drivers, unsigned count, unsigned bit) {
    return strengthValue(resolveStrengthBit(kind, drivers, count, bit));
}

/// A net of `width` bits (at most 64) with `count` drivers.
inline W4 resolve4(NetKind kind, const StrengthDriver* drivers, unsigned count, unsigned width) {
    W4 out{0, 0};
    for (unsigned i = 0; i < width; ++i) {
        const W4 r = resolveBit(kind, drivers, count, i);
        out.a |= r.a << i;
        out.b |= r.b << i;
    }
    return out;
}

}  // namespace kirimasim::kernel
