// Fast four-state operations on values of 1 to 64 bits held in one W4.
//
// This header is part of the kernel's public interface. Generated code reads
// it, so it must stay compilable as C++20.
//
// Every function takes the width w (1 to 64) of its operands and expects the
// bits above w to be 0 in both a and b; results keep them 0. The reference
// definitions are in valsem/value.h, and tests/unit/w4ops_test.cpp compares
// the two (VALUE_SEMANTICS.md 3). Nothing here has undefined behavior in
// C++: signed division, shifts and overflow are handled explicitly.

#pragma once

#include <bit>
#include <cstdint>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel {

/// The bits of a w-bit value.
inline std::uint64_t mask(unsigned w) {
    return w >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << w) - 1;
}

/// Every bit X.
inline W4 allX(unsigned w) { return {mask(w), mask(w)}; }

inline constexpr W4 kW4Zero{0, 0};
inline constexpr W4 kW4One{1, 0};
inline constexpr W4 kW4X{1, 1};

inline bool hasUnknown(W4 x) { return x.b != 0; }

/// The value of a w-bit word read as a signed number (two's complement).
inline std::int64_t asSigned(std::uint64_t v, unsigned w) {
    const unsigned s = 64 - w;
    return static_cast<std::int64_t>(v << s) >> s;  // C++20: arithmetic shift
}

// --- Bitwise (VALUE_SEMANTICS.md 3.6). and4 is in w4.h. ---------------------

inline W4 or4(W4 x, W4 y) {
    if ((x.b | y.b) == 0) return {x.a | y.a, 0};
    const std::uint64_t one = (x.a & ~x.b) | (y.a & ~y.b);     // either side is a known 1
    const std::uint64_t zero = (~x.a & ~x.b) & (~y.a & ~y.b);  // both sides are known 0
    const std::uint64_t unknown = ~one & ~zero;  // bits above w are known 0 on both sides
    return {one | unknown, unknown};
}

inline W4 xor4(W4 x, W4 y) {
    const std::uint64_t unknown = x.b | y.b;
    return {(x.a ^ y.a) | unknown, unknown};
}

inline W4 xnor4(W4 x, W4 y, unsigned w) {
    const std::uint64_t unknown = x.b | y.b;
    return {(~(x.a ^ y.a) & mask(w)) | unknown, unknown};
}

inline W4 not4(W4 x, unsigned w) { return {(~x.a & mask(w)) | x.b, x.b}; }

// --- Truth and logical operators (3.5). Results are 1 bit. -----------------

inline W4 truth4(W4 x) {
    if ((x.a & ~x.b) != 0) return kW4One;
    return x.b != 0 ? kW4X : kW4Zero;
}

inline W4 lnot4(W4 x) {
    const W4 t = truth4(x);
    return t.b != 0 ? kW4X : W4{t.a ^ 1U, 0};
}

inline W4 land4(W4 x, W4 y) { return and4(truth4(x), truth4(y)); }
inline W4 lor4(W4 x, W4 y) { return or4(truth4(x), truth4(y)); }

// --- Reduction (3.7). Results are 1 bit. -----------------------------------

inline W4 redAnd4(W4 x, unsigned w) {
    if ((~x.a & ~x.b & mask(w)) != 0) return kW4Zero;  // a known 0
    return x.b != 0 ? kW4X : kW4One;
}

inline W4 redOr4(W4 x) {
    if ((x.a & ~x.b) != 0) return kW4One;  // a known 1
    return x.b != 0 ? kW4X : kW4Zero;
}

inline W4 redXor4(W4 x) {
    if (x.b != 0) return kW4X;
    std::uint64_t v = x.a;
    v ^= v >> 32;
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return {v & 1U, 0};
}

inline W4 negate1(W4 r) { return r.b != 0 ? kW4X : W4{r.a ^ 1U, 0}; }
inline W4 redNand4(W4 x, unsigned w) { return negate1(redAnd4(x, w)); }
inline W4 redNor4(W4 x) { return negate1(redOr4(x)); }
inline W4 redXnor4(W4 x) { return negate1(redXor4(x)); }

/// $countbits (20.9): the number of bits of the w-bit x whose value is one
/// of those chosen. The reference is valsem::countBits.
inline unsigned countBits4(W4 x, unsigned w, bool zeros, bool ones, bool zs, bool xs) {
    std::uint64_t chosen = 0;
    if (zeros) chosen |= ~x.a & ~x.b;
    if (ones) chosen |= x.a & ~x.b;
    if (zs) chosen |= ~x.a & x.b;
    if (xs) chosen |= x.a & x.b;
    return static_cast<unsigned>(std::popcount(chosen & mask(w)));
}

// --- Arithmetic (3.2). Any X or Z bit makes every result bit X. ------------

inline W4 add4(W4 x, W4 y, unsigned w) {
    if ((x.b | y.b) != 0) return allX(w);
    return {(x.a + y.a) & mask(w), 0};
}

inline W4 sub4(W4 x, W4 y, unsigned w) {
    if ((x.b | y.b) != 0) return allX(w);
    return {(x.a - y.a) & mask(w), 0};
}

inline W4 mul4(W4 x, W4 y, unsigned w) {
    if ((x.b | y.b) != 0) return allX(w);
    return {(x.a * y.a) & mask(w), 0};
}

inline W4 neg4(W4 x, unsigned w) {
    if (x.b != 0) return allX(w);
    return {(0 - x.a) & mask(w), 0};
}

/// Division truncating toward 0. Signed when isSigned; X on division by 0.
inline W4 div4(W4 x, W4 y, unsigned w, bool isSigned) {
    if ((x.b | y.b) != 0 || y.a == 0) return allX(w);
    if (!isSigned) return {x.a / y.a, 0};
    const std::int64_t n = asSigned(x.a, w);
    const std::int64_t d = asSigned(y.a, w);
    if (d == -1) return {(0 - x.a) & mask(w), 0};  // also covers INT64_MIN / -1
    return {static_cast<std::uint64_t>(n / d) & mask(w), 0};
}

/// Remainder with the sign of the dividend. X on division by 0.
inline W4 mod4(W4 x, W4 y, unsigned w, bool isSigned) {
    if ((x.b | y.b) != 0 || y.a == 0) return allX(w);
    if (!isSigned) return {x.a % y.a, 0};
    const std::int64_t n = asSigned(x.a, w);
    const std::int64_t d = asSigned(y.a, w);
    if (d == -1) return kW4Zero;  // avoids INT64_MIN % -1
    return {static_cast<std::uint64_t>(n % d) & mask(w), 0};
}

/// Integer power (table 11-4). The exponent y has width yw and is signed
/// when ySigned; x is signed when xSigned.
inline W4 pow4(W4 x, W4 y, unsigned w, bool xSigned, unsigned yw, bool ySigned) {
    if ((x.b | y.b) != 0) return allX(w);
    const std::uint64_t m = mask(w);
    const bool expNegative = ySigned && ((y.a >> (yw - 1)) & 1U) != 0;
    if (y.a == 0) return {1, 0};
    if (x.a == 0) return expNegative ? allX(w) : kW4Zero;
    if (x.a == 1) return {1, 0};
    if (xSigned && x.a == m) return {(y.a & 1U) != 0 ? m : 1, 0};  // -1 ** y
    if (expNegative) return kW4Zero;
    std::uint64_t result = 1;
    for (unsigned i = yw; i-- > 0;) {
        result *= result;
        if (((y.a >> i) & 1U) != 0) result *= x.a;
    }
    return {result & m, 0};
}

// --- Relational (3.3). 1-bit results; signed only if isSigned. -------------

inline int compare64(W4 x, W4 y, unsigned w, bool isSigned) {
    if (isSigned) {
        const std::int64_t a = asSigned(x.a, w);
        const std::int64_t b = asSigned(y.a, w);
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    return x.a < y.a ? -1 : (x.a > y.a ? 1 : 0);
}

inline W4 lt4(W4 x, W4 y, unsigned w, bool s) {
    if ((x.b | y.b) != 0) return kW4X;
    return {compare64(x, y, w, s) < 0 ? 1U : 0U, 0};
}
inline W4 le4(W4 x, W4 y, unsigned w, bool s) {
    if ((x.b | y.b) != 0) return kW4X;
    return {compare64(x, y, w, s) <= 0 ? 1U : 0U, 0};
}
inline W4 gt4(W4 x, W4 y, unsigned w, bool s) {
    if ((x.b | y.b) != 0) return kW4X;
    return {compare64(x, y, w, s) > 0 ? 1U : 0U, 0};
}
inline W4 ge4(W4 x, W4 y, unsigned w, bool s) {
    if ((x.b | y.b) != 0) return kW4X;
    return {compare64(x, y, w, s) >= 0 ? 1U : 0U, 0};
}

// --- Equality (3.4). 1-bit results. ----------------------------------------

inline W4 eq4(W4 x, W4 y) {
    const std::uint64_t knownBoth = ~x.b & ~y.b;
    if (((x.a ^ y.a) & knownBoth) != 0) return kW4Zero;  // a known mismatch
    return (x.b | y.b) != 0 ? kW4X : kW4One;
}

inline W4 ne4(W4 x, W4 y) { return negate1(eq4(x, y)); }

inline W4 ceq4(W4 x, W4 y) { return {x.a == y.a && x.b == y.b ? 1U : 0U, 0}; }
inline W4 cne4(W4 x, W4 y) { return {x.a == y.a && x.b == y.b ? 0U : 1U, 0}; }

/// ==?: the X and Z bits of y match anything; X and Z bits of x do not.
inline W4 weq4(W4 x, W4 y) {
    const std::uint64_t care = ~y.b;  // positions compared
    if (((x.a ^ y.a) & care & ~x.b) != 0) return kW4Zero;
    return (x.b & care) != 0 ? kW4X : kW4One;
}

inline W4 wne4(W4 x, W4 y) { return negate1(weq4(x, y)); }

/// casez (ignoreX false) and casex matching (4.2): z bits, and x bits for
/// casex, on either side are not compared. Always 0 or 1.
inline W4 caseMatch4(W4 x, W4 y, bool ignoreX) {
    std::uint64_t ignored = (~x.a & x.b) | (~y.a & y.b);
    if (ignoreX) ignored |= x.b | y.b;
    const std::uint64_t differ = (x.a ^ y.a) | (x.b ^ y.b);
    return (differ & ~ignored) != 0 ? kW4Zero : kW4One;
}

// --- Shifts (3.8). The amount is unsigned, at most 64 bits. ----------------

inline W4 shl4(W4 x, W4 amount, unsigned w) {
    if (amount.b != 0) return allX(w);
    if (amount.a >= w) return kW4Zero;
    const auto n = static_cast<unsigned>(amount.a);
    return {(x.a << n) & mask(w), (x.b << n) & mask(w)};
}

inline W4 shr4(W4 x, W4 amount, unsigned w) {
    if (amount.b != 0) return allX(w);
    if (amount.a >= w) return kW4Zero;
    const auto n = static_cast<unsigned>(amount.a);
    return {x.a >> n, x.b >> n};
}

/// >>>: fills with the sign bit (a and b alike) when isSigned, else with 0.
inline W4 ashr4(W4 x, W4 amount, unsigned w, bool isSigned) {
    if (!isSigned) return shr4(x, amount, w);
    if (amount.b != 0) return allX(w);
    const std::uint64_t m = mask(w);
    const std::uint64_t signA = ((x.a >> (w - 1)) & 1U) != 0 ? m : 0;
    const std::uint64_t signB = ((x.b >> (w - 1)) & 1U) != 0 ? m : 0;
    if (amount.a >= w) return {signA, signB};
    const auto n = static_cast<unsigned>(amount.a);
    const std::uint64_t fill = m & ~(m >> n);  // the top n bits
    return {(x.a >> n) | (signA & fill), (x.b >> n) | (signB & fill)};
}

// --- Conditional operator with both sides evaluated (3.9). -----------------

inline W4 select4(W4 cond, W4 x, W4 y, unsigned w) {
    const W4 t = truth4(cond);
    if (t.b == 0) return t.a != 0 ? x : y;
    if (eq4(x, y).a == 1 && eq4(x, y).b == 0) return x;
    const std::uint64_t same = ~(x.a ^ y.a) & ~x.b & ~y.b & mask(w);  // equal known bits
    const std::uint64_t unknown = ~same & mask(w);
    return {(x.a & same) | unknown, unknown};
}

// --- Conversions (2, 1.2). -------------------------------------------------

/// Sign extension from width w to width to: the bits above w copy bit w-1.
inline W4 sext4(W4 x, unsigned w, unsigned to) {
    const std::uint64_t fill = mask(to) & ~mask(w);
    const bool signA = ((x.a >> (w - 1)) & 1U) != 0;
    const bool signB = ((x.b >> (w - 1)) & 1U) != 0;
    return {x.a | (signA ? fill : 0), x.b | (signB ? fill : 0)};
}

/// Truncation to width to.
inline W4 trunc4(W4 x, unsigned to) { return {x.a & mask(to), x.b & mask(to)}; }

/// Four-state to two-state: X and Z become 0.
inline W4 to2(W4 x) { return {x.a & ~x.b, 0}; }

}  // namespace kirimasim::kernel
