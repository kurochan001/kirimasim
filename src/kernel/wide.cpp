// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Four-state operations on values of any width (wide.h).

#include "kirimasim/kernel/wide.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "kirimasim/kernel/w4ops.h"

namespace kirimasim::kernel::wide {

namespace {

/// The bits of chunk k that a w-bit value uses.
std::uint64_t chunkMask(unsigned w, unsigned k) {
    const unsigned lo = 64 * k;
    if (lo >= w) return 0;
    return w - lo >= 64 ? ~std::uint64_t{0} : mask(w - lo);
}

bool bitA(const W4* x, unsigned i) { return ((x[i / 64].a >> (i % 64)) & 1U) != 0; }
bool bitB(const W4* x, unsigned i) { return ((x[i / 64].b >> (i % 64)) & 1U) != 0; }

void setBit(W4* r, unsigned i, bool a, bool b) {
    const std::uint64_t m = std::uint64_t{1} << (i % 64);
    W4& c = r[i / 64];
    c.a = a ? c.a | m : c.a & ~m;
    c.b = b ? c.b | m : c.b & ~m;
}

void clear(W4* r, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = {0, 0};
}

bool unknown2(const W4* x, const W4* y, unsigned w) { return hasUnknown(x, w) || hasUnknown(y, w); }

/// The a words of a two-state value.
std::vector<std::uint64_t> words(const W4* x, unsigned w) {
    std::vector<std::uint64_t> v(chunks(w));
    for (unsigned k = 0; k < v.size(); ++k)
        v[k] = x[k].a;
    return v;
}

void store(W4* r, const std::vector<std::uint64_t>& v, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = {k < v.size() ? v[k] & chunkMask(w, k) : 0, 0};
}

bool negative(const std::vector<std::uint64_t>& v, unsigned w) {
    return ((v[(w - 1) / 64] >> ((w - 1) % 64)) & 1U) != 0;
}

/// v = -v modulo 2^w.
void negateWords(std::vector<std::uint64_t>& v, unsigned w) {
    std::uint64_t carry = 1;
    for (unsigned k = 0; k < v.size(); ++k) {
        const std::uint64_t n = ~v[k] + carry;
        carry = carry != 0 && n == 0 ? 1 : 0;
        v[k] = n & chunkMask(w, k);
    }
}

bool isZero(const std::vector<std::uint64_t>& v) {
    for (const std::uint64_t x : v)
        if (x != 0) return false;
    return true;
}

/// Unsigned comparison of two values of the same width.
int compareWords(const std::vector<std::uint64_t>& x, const std::vector<std::uint64_t>& y) {
    for (std::size_t k = x.size(); k-- > 0;)
        if (x[k] != y[k]) return x[k] < y[k] ? -1 : 1;
    return 0;
}

std::vector<std::uint64_t> addWords(const std::vector<std::uint64_t>& x,
                                    const std::vector<std::uint64_t>& y, unsigned w) {
    std::vector<std::uint64_t> r(x.size());
    std::uint64_t carry = 0;
    for (unsigned k = 0; k < r.size(); ++k) {
        const std::uint64_t s = x[k] + y[k];
        const std::uint64_t t = s + carry;
        carry = (s < x[k] ? 1 : 0) + (t < s ? 1 : 0);
        r[k] = t & chunkMask(w, k);
    }
    return r;
}

std::vector<std::uint64_t> mulWords(const std::vector<std::uint64_t>& x,
                                    const std::vector<std::uint64_t>& y, unsigned w) {
    // Schoolbook on 32-bit halves, modulo 2^w.
    const std::size_t n = x.size() * 2;
    std::vector<std::uint32_t> a(n), b(n), p(n, 0);
    for (std::size_t k = 0; k < x.size(); ++k) {
        a[2 * k] = static_cast<std::uint32_t>(x[k]);
        a[2 * k + 1] = static_cast<std::uint32_t>(x[k] >> 32);
        b[2 * k] = static_cast<std::uint32_t>(y[k]);
        b[2 * k + 1] = static_cast<std::uint32_t>(y[k] >> 32);
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; i + j < n; ++j) {
            const std::uint64_t t = std::uint64_t{a[i]} * b[j] + p[i + j] + carry;
            p[i + j] = static_cast<std::uint32_t>(t);
            carry = t >> 32;
        }
    }
    std::vector<std::uint64_t> r(x.size());
    for (std::size_t k = 0; k < r.size(); ++k)
        r[k] = (std::uint64_t{p[2 * k]} | (std::uint64_t{p[2 * k + 1]} << 32)) &
               chunkMask(w, static_cast<unsigned>(k));
    return r;
}

/// Unsigned long division: quotient and remainder of x / y, y not 0.
void divWords(const std::vector<std::uint64_t>& x, const std::vector<std::uint64_t>& y, unsigned w,
              std::vector<std::uint64_t>& q, std::vector<std::uint64_t>& rem) {
    q.assign(x.size(), 0);
    rem.assign(x.size(), 0);
    for (unsigned i = w; i-- > 0;) {
        // rem = rem << 1 | bit i of x
        for (std::size_t k = rem.size(); k-- > 0;) {
            rem[k] <<= 1;
            if (k > 0) rem[k] |= rem[k - 1] >> 63;
        }
        rem[0] |= (x[i / 64] >> (i % 64)) & 1U;
        if (compareWords(rem, y) >= 0) {
            std::vector<std::uint64_t> ny = y;
            negateWords(ny, static_cast<unsigned>(64 * rem.size()));
            rem = addWords(rem, ny, static_cast<unsigned>(64 * rem.size()));
            q[i / 64] |= std::uint64_t{1} << (i % 64);
        }
    }
}

/// The shift amount, or nullopt-like UINT64_MAX when it reaches w or more.
std::uint64_t amountOf(const W4* amount, unsigned aw, unsigned w) {
    for (unsigned k = 1; k < chunks(aw); ++k)
        if (amount[k].a != 0) return UINT64_MAX;
    return amount[0].a >= w ? UINT64_MAX : amount[0].a;
}

W4 bit1(bool v) { return v ? kW4One : kW4Zero; }

W4 relational(const W4* x, const W4* y, unsigned w, bool isSigned, int want0, int want1) {
    // want0/want1: the comparison results (-1, 0, 1) that make it true.
    if (unknown2(x, y, w)) return kW4X;
    const std::vector<std::uint64_t> a = words(x, w);
    const std::vector<std::uint64_t> b = words(y, w);
    int c = 0;
    if (isSigned && negative(a, w) != negative(b, w))
        c = negative(a, w) ? -1 : 1;
    else
        c = compareWords(a, b);
    return bit1(c == want0 || c == want1);
}

W4 negate1Bit(W4 r) { return r.b != 0 ? kW4X : W4{r.a ^ 1U, 0}; }

}  // namespace

// --- Conversions -------------------------------------------------------------

void resize(W4* r, unsigned rw, const W4* x, unsigned xw, bool signExtend) {
    const bool signA = signExtend && bitA(x, xw - 1);
    const bool signB = signExtend && bitB(x, xw - 1);
    std::vector<W4> out(chunks(rw), W4{0, 0});
    for (unsigned k = 0; k < out.size(); ++k) {
        const std::uint64_t keep = chunkMask(rw, k);
        const std::uint64_t have = k < chunks(xw) ? chunkMask(xw, k) : 0;
        W4 c = k < chunks(xw) ? x[k] : W4{0, 0};
        const std::uint64_t fill = keep & ~have;
        c.a = (c.a & have) | (signA ? fill : 0);
        c.b = (c.b & have) | (signB ? fill : 0);
        out[k] = {c.a & keep, c.b & keep};
    }
    for (unsigned k = 0; k < out.size(); ++k)
        r[k] = out[k];
}

void to2(W4* r, const W4* x, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = {x[k].a & ~x[k].b, 0};
}

void allX(W4* r, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = {chunkMask(w, k), chunkMask(w, k)};
}

bool hasUnknown(const W4* x, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        if (x[k].b != 0) return true;
    return false;
}

// --- Bitwise and reduction -----------------------------------------------------

void bitAnd(W4* r, const W4* x, const W4* y, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = and4(x[k], y[k]);
}

void bitOr(W4* r, const W4* x, const W4* y, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = or4(x[k], y[k]);
}

void bitXor(W4* r, const W4* x, const W4* y, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = xor4(x[k], y[k]);
}

void bitXnor(W4* r, const W4* x, const W4* y, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k) {
        const W4 t = xor4(x[k], y[k]);
        r[k] = {(~t.a & chunkMask(w, k)) | t.b, t.b};
    }
}

void bitNot(W4* r, const W4* x, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        r[k] = {(~x[k].a & chunkMask(w, k)) | x[k].b, x[k].b};
}

W4 redAnd(const W4* x, unsigned w) {
    bool unknown = false;
    for (unsigned k = 0; k < chunks(w); ++k) {
        if ((~x[k].a & ~x[k].b & chunkMask(w, k)) != 0) return kW4Zero;
        unknown = unknown || x[k].b != 0;
    }
    return unknown ? kW4X : kW4One;
}

W4 redOr(const W4* x, unsigned w) {
    bool unknown = false;
    for (unsigned k = 0; k < chunks(w); ++k) {
        if ((x[k].a & ~x[k].b) != 0) return kW4One;
        unknown = unknown || x[k].b != 0;
    }
    return unknown ? kW4X : kW4Zero;
}

W4 redXor(const W4* x, unsigned w) {
    std::uint64_t parity = 0;
    for (unsigned k = 0; k < chunks(w); ++k) {
        if (x[k].b != 0) return kW4X;
        parity ^= x[k].a;
    }
    return redXor4({parity, 0});
}

unsigned countBits(const W4* x, unsigned w, bool zeros, bool ones, bool zs, bool xs) {
    unsigned n = 0;
    for (unsigned k = 0; k < chunks(w); ++k) {
        const unsigned cw = w - 64 * k >= 64 ? 64 : w - 64 * k;
        n += countBits4(x[k], cw, zeros, ones, zs, xs);
    }
    return n;
}

W4 truth(const W4* x, unsigned w) { return redOr(x, w); }

// --- Arithmetic ----------------------------------------------------------------

void add(W4* r, const W4* x, const W4* y, unsigned w) {
    if (unknown2(x, y, w)) return allX(r, w);
    store(r, addWords(words(x, w), words(y, w), w), w);
}

void sub(W4* r, const W4* x, const W4* y, unsigned w) {
    if (unknown2(x, y, w)) return allX(r, w);
    std::vector<std::uint64_t> b = words(y, w);
    negateWords(b, w);
    store(r, addWords(words(x, w), b, w), w);
}

void mul(W4* r, const W4* x, const W4* y, unsigned w) {
    if (unknown2(x, y, w)) return allX(r, w);
    store(r, mulWords(words(x, w), words(y, w), w), w);
}

void neg(W4* r, const W4* x, unsigned w) {
    if (hasUnknown(x, w)) return allX(r, w);
    std::vector<std::uint64_t> v = words(x, w);
    negateWords(v, w);
    store(r, v, w);
}

void div(W4* r, const W4* x, const W4* y, unsigned w, bool isSigned) {
    std::vector<std::uint64_t> a = words(x, w);
    std::vector<std::uint64_t> b = words(y, w);
    if (unknown2(x, y, w) || isZero(b)) return allX(r, w);
    const bool na = isSigned && negative(a, w);
    const bool nb = isSigned && negative(b, w);
    if (na) negateWords(a, w);
    if (nb) negateWords(b, w);
    std::vector<std::uint64_t> q, rem;
    divWords(a, b, w, q, rem);
    if (na != nb) negateWords(q, w);
    store(r, q, w);
}

void mod(W4* r, const W4* x, const W4* y, unsigned w, bool isSigned) {
    std::vector<std::uint64_t> a = words(x, w);
    std::vector<std::uint64_t> b = words(y, w);
    if (unknown2(x, y, w) || isZero(b)) return allX(r, w);
    const bool na = isSigned && negative(a, w);
    if (na) negateWords(a, w);
    if (isSigned && negative(b, w)) negateWords(b, w);
    std::vector<std::uint64_t> q, rem;
    divWords(a, b, w, q, rem);
    if (na) negateWords(rem, w);
    store(r, rem, w);
}

void pow(W4* r, const W4* x, unsigned w, bool xSigned, const W4* y, unsigned yw, bool ySigned) {
    if (hasUnknown(x, w) || hasUnknown(y, yw)) return allX(r, w);
    const std::vector<std::uint64_t> a = words(x, w);
    const std::vector<std::uint64_t> e = words(y, yw);
    const bool expNegative = ySigned && negative(e, yw);
    std::vector<std::uint64_t> one(a.size(), 0);
    one[0] = 1;
    std::vector<std::uint64_t> minusOne = one;
    negateWords(minusOne, w);
    if (isZero(e)) return store(r, one, w);
    if (isZero(a)) {
        if (expNegative) return allX(r, w);
        return store(r, std::vector<std::uint64_t>(a.size(), 0), w);
    }
    if (compareWords(a, one) == 0) return store(r, one, w);
    if (xSigned && compareWords(a, minusOne) == 0)
        return store(r, (e[0] & 1U) != 0 ? minusOne : one, w);  // -1 ** y
    if (expNegative) return store(r, std::vector<std::uint64_t>(a.size(), 0), w);
    std::vector<std::uint64_t> result = one;
    for (unsigned i = yw; i-- > 0;) {
        result = mulWords(result, result, w);
        if (((e[i / 64] >> (i % 64)) & 1U) != 0) result = mulWords(result, a, w);
    }
    store(r, result, w);
}

// --- Relational and equality ---------------------------------------------------

W4 lt(const W4* x, const W4* y, unsigned w, bool s) { return relational(x, y, w, s, -1, -1); }
W4 le(const W4* x, const W4* y, unsigned w, bool s) { return relational(x, y, w, s, -1, 0); }
W4 gt(const W4* x, const W4* y, unsigned w, bool s) { return relational(x, y, w, s, 1, 1); }
W4 ge(const W4* x, const W4* y, unsigned w, bool s) { return relational(x, y, w, s, 1, 0); }

W4 eq(const W4* x, const W4* y, unsigned w) {
    bool unknown = false;
    for (unsigned k = 0; k < chunks(w); ++k) {
        const W4 c = eq4(x[k], y[k]);
        if (c.b == 0 && c.a == 0) return kW4Zero;
        unknown = unknown || c.b != 0;
    }
    return unknown ? kW4X : kW4One;
}

W4 ne(const W4* x, const W4* y, unsigned w) { return negate1Bit(eq(x, y, w)); }

W4 ceq(const W4* x, const W4* y, unsigned w) {
    for (unsigned k = 0; k < chunks(w); ++k)
        if (x[k].a != y[k].a || x[k].b != y[k].b) return kW4Zero;
    return kW4One;
}

W4 cne(const W4* x, const W4* y, unsigned w) { return negate1Bit(ceq(x, y, w)); }

W4 weq(const W4* x, const W4* y, unsigned w) {
    bool unknown = false;
    for (unsigned k = 0; k < chunks(w); ++k) {
        const W4 c = weq4(x[k], y[k]);
        if (c.b == 0 && c.a == 0) return kW4Zero;
        unknown = unknown || c.b != 0;
    }
    return unknown ? kW4X : kW4One;
}

W4 wne(const W4* x, const W4* y, unsigned w) { return negate1Bit(weq(x, y, w)); }

W4 caseMatch(const W4* x, const W4* y, unsigned w, bool ignoreX) {
    for (unsigned k = 0; k < chunks(w); ++k)
        if (caseMatch4(x[k], y[k], ignoreX).a == 0) return kW4Zero;
    return kW4One;
}

// --- Shifts --------------------------------------------------------------------

void shl(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw) {
    if (hasUnknown(amount, aw)) return allX(r, w);
    const std::uint64_t n = amountOf(amount, aw, w);
    std::vector<W4> out(chunks(w), W4{0, 0});
    for (unsigned i = 0; n != UINT64_MAX && i + n < w; ++i)
        setBit(out.data(), static_cast<unsigned>(i + n), bitA(x, i), bitB(x, i));
    for (unsigned k = 0; k < out.size(); ++k)
        r[k] = out[k];
}

void shr(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw) {
    ashr(r, x, w, amount, aw, false);
}

void ashr(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw, bool isSigned) {
    if (hasUnknown(amount, aw)) return allX(r, w);
    const std::uint64_t n = amountOf(amount, aw, w);
    const bool signA = isSigned && bitA(x, w - 1);
    const bool signB = isSigned && bitB(x, w - 1);
    std::vector<W4> out(chunks(w), W4{0, 0});
    for (unsigned i = 0; i < w; ++i) {
        const std::uint64_t from = n == UINT64_MAX ? UINT64_MAX : i + n;
        if (from < w)
            setBit(out.data(), i, bitA(x, static_cast<unsigned>(from)),
                   bitB(x, static_cast<unsigned>(from)));
        else
            setBit(out.data(), i, signA, signB);
    }
    for (unsigned k = 0; k < out.size(); ++k)
        r[k] = out[k];
}

// --- Conditional ---------------------------------------------------------------

void select(W4* r, W4 cond, const W4* x, const W4* y, unsigned w) {
    const W4 t = truth4(cond);
    if (t.b == 0) {
        const W4* from = t.a != 0 ? x : y;
        for (unsigned k = 0; k < chunks(w); ++k)
            r[k] = from[k];
        return;
    }
    for (unsigned k = 0; k < chunks(w); ++k) {
        const std::uint64_t same = ~(x[k].a ^ y[k].a) & ~x[k].b & ~y[k].b & chunkMask(w, k);
        const std::uint64_t unknown = ~same & chunkMask(w, k);
        r[k] = {(x[k].a & same) | unknown, unknown};
    }
}

// --- Selects and concatenation -------------------------------------------------

void bits(W4* r, unsigned rw, const W4* x, unsigned xw, std::int64_t lsb) {
    std::vector<W4> out(chunks(rw), W4{0, 0});
    // Wholly outside: x, without computing lsb + i, which could overflow.
    const bool outside =
        lsb >= static_cast<std::int64_t>(xw) || lsb <= -static_cast<std::int64_t>(rw);
    for (unsigned i = 0; i < rw; ++i) {
        if (outside) {
            setBit(out.data(), i, true, true);
            continue;
        }
        const std::int64_t s = lsb + i;
        if (s < 0 || s >= xw)
            setBit(out.data(), i, true, true);
        else
            setBit(out.data(), i, bitA(x, static_cast<unsigned>(s)),
                   bitB(x, static_cast<unsigned>(s)));
    }
    for (unsigned k = 0; k < out.size(); ++k)
        r[k] = out[k];
}

std::optional<std::int64_t> position(const W4* x, unsigned w, bool s) {
    if (hasUnknown(x, w)) return std::nullopt;
    if (w <= 64) return position4(x[0], w, s);
    const unsigned n = chunks(w);
    const bool neg = s && ((x[n - 1].a >> ((w - 1) % 64)) & 1U) != 0;
    for (unsigned k = 1; k < n; ++k) {
        const std::uint64_t fill = neg ? (k + 1 == n ? mask(w - 64 * k) : ~std::uint64_t{0}) : 0;
        if (x[k].a != fill) return std::nullopt;
    }
    if ((x[0].a >> 63) != (neg ? 1U : 0U)) return std::nullopt;  // past 64 signed bits
    return static_cast<std::int64_t>(x[0].a);
}

void writeBits(W4* x, unsigned xw, std::int64_t pos, const W4* v, unsigned vw) {
    const std::optional<Inside> in = inside(pos, vw, xw);
    if (!in) return;
    std::vector<W4> part(chunks(in->n));
    bits(part.data(), in->n, v, vw, in->skip);
    put(x, in->lo, part.data(), in->n);
}

std::string toText(const W4* x, unsigned w) {
    std::string s;
    for (unsigned k = bytesFor(w); k-- > 0;) {
        const W4& c = x[k / 8];
        const auto ch = static_cast<char>(((c.a & ~c.b) >> (k % 8 * 8)) & 0xffU);
        if (ch != 0) s.push_back(ch);
    }
    return s;
}

void fromText(W4* r, unsigned w, std::string_view s) {
    const unsigned n = chunks(w);
    for (unsigned i = 0; i < n; ++i)
        r[i] = {0, 0};
    unsigned byte = 0;
    for (auto c = s.rbegin(); c != s.rend() && byte * 8 < w; ++c, ++byte)
        r[byte / 8].a |= std::uint64_t{static_cast<unsigned char>(*c)} << (byte % 8 * 8);
    if (w % 64 != 0) r[n - 1].a &= mask(w % 64);
}

void put(W4* r, unsigned lsb, const W4* x, unsigned xw) {
    for (unsigned i = 0; i < xw; ++i)
        setBit(r, lsb + i, bitA(x, i), bitB(x, i));
}

// --- Reals -----------------------------------------------------------------------

double toReal(const W4* x, unsigned w, bool isSigned) {
    std::vector<std::uint64_t> v(chunks(w));
    for (unsigned k = 0; k < v.size(); ++k)
        v[k] = x[k].a & ~x[k].b;
    const bool minus = isSigned && negative(v, w);
    if (minus) negateWords(v, w);
    // The 64 bits from the highest 1, the lower ones folded into the last
    // bit, so that the conversion rounds once and correctly.
    int top = -1;
    for (unsigned i = w; i-- > 0 && top < 0;)
        if (((v[i / 64] >> (i % 64)) & 1U) != 0) top = static_cast<int>(i);
    if (top < 0) return 0.0;
    std::uint64_t m = 0;
    bool sticky = false;
    for (int i = top, k = 63; i >= 0; --i, --k) {
        const bool b = ((v[static_cast<unsigned>(i) / 64] >> (i % 64)) & 1U) != 0;
        if (k >= 0)
            m |= b ? std::uint64_t{1} << k : 0;
        else
            sticky = sticky || b;
    }
    if (top < 63) m >>= 63 - top;
    if (sticky) m |= 1;
    const double d = std::ldexp(static_cast<double>(m), top < 63 ? 0 : top - 63);
    return minus ? -d : d;
}

void fromReal(W4* r, unsigned w, double d, bool fourState, bool truncate) {
    clear(r, w);
    if (std::isnan(d) || std::isinf(d)) {
        if (fourState) allX(r, w);
        return;
    }
    const double v = truncate ? std::trunc(d) : std::round(d);  // round: half away from zero
    if (std::fabs(v) < 9223372036854775808.0) {
        const W4 low{static_cast<std::uint64_t>(static_cast<std::int64_t>(v)), 0};
        resize(r, w, &low, 64, true);
        return;
    }
    // |v| = mant * 2^shift, with mant below 2^53 and shift at least 11.
    int exp = 0;
    const auto mant = static_cast<std::uint64_t>(std::ldexp(std::frexp(std::fabs(v), &exp), 53));
    const int shift = exp - 53;
    std::vector<std::uint64_t> words(chunks(w), 0);
    for (int k = 0; k < 53; ++k) {
        const long i = static_cast<long>(k) + shift;
        if (((mant >> k) & 1U) != 0 && i < static_cast<long>(w))
            words[static_cast<std::size_t>(i) / 64] |= std::uint64_t{1} << (i % 64);
    }
    if (v < 0) negateWords(words, w);
    for (std::size_t k = 0; k < words.size(); ++k)
        r[k] = {words[k], 0};
}

}  // namespace kirimasim::kernel::wide
