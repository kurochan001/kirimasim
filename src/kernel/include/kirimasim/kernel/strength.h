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

/// One driver of a net: its value and its strengths for 1 and for 0, each
/// from 0 (highz) to 7 (supply).
struct StrengthDriver {
    W4 value;
    std::uint8_t s1;
    std::uint8_t s0;
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

}  // namespace detail

/// One bit of a net (5.4): the four-state value of the narrowest range
/// that holds the outcome of every choice of points, with L and H read as
/// X (5.2). `bit` selects the bit of the driver values.
inline W4 resolveBit(NetKind kind, const StrengthDriver* drivers, unsigned count, unsigned bit) {
    std::uint32_t states = detail::stateBit(0, 1);
    const auto range = [&](int lo, int hi) { states = detail::addRange(states, lo, hi); };
    for (unsigned d = 0; d < count; ++d) {
        const unsigned a = (drivers[d].value.a >> bit) & 1U;
        const unsigned b = (drivers[d].value.b >> bit) & 1U;
        const int s1 = drivers[d].s1;
        const int s0 = drivers[d].s0;
        if (b == 0)
            range(a != 0 ? s1 : -s0, a != 0 ? s1 : -s0);  // 1 or 0
        else if (a != 0)
            range(-s0, s1);  // x
        else
            range(0, 0);  // z
    }
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
    if (lo == 0 && hi == 0) return {0, 1};  // z
    if (hi < 0) return {0, 0};              // 0
    if (lo > 0) return {1, 0};              // 1
    return {1, 1};                          // x, and L and H
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
