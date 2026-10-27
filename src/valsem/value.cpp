// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/valsem/value.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <regex>

namespace kirimasim::valsem {

double stringToReal(const std::string& text) {
    const std::regex syntax("^[+-]?[0-9][0-9_]*(\\.[0-9][0-9_]*)?([eE][+-]?[0-9][0-9_]*)?");
    std::smatch match;
    if (!std::regex_search(text, match, syntax)) return 0;
    std::string number;
    for (const char c : match.str())
        if (c != '_') number += c;
    return std::strtod(number.c_str(), nullptr);
}

namespace {

using Bits = std::vector<bool>;  // Known bits, least significant first.

Bits known(const Value& v) {
    Bits out(v.bits.size());
    for (std::size_t i = 0; i < v.bits.size(); ++i)
        out[i] = v.bits[i] == Logic::One;
    return out;
}

Value fromBits(const Bits& b, bool isSigned) {
    Value v;
    v.isSigned = isSigned;
    v.bits.resize(b.size());
    for (std::size_t i = 0; i < b.size(); ++i)
        v.bits[i] = b[i] ? Logic::One : Logic::Zero;
    return v;
}

Value allX(std::uint32_t width, bool isSigned) { return filled(Logic::X, width, isSigned); }

Value bit1(Logic b) { return {{b}, false}; }

bool isZero(const Bits& b) {
    for (const bool x : b)
        if (x) return false;
    return true;
}

bool msb(const Bits& b) { return b.back(); }

/// Sum modulo 2^width, by rippling the carry.
Bits addBits(const Bits& x, const Bits& y, bool carryIn = false) {
    Bits out(x.size());
    bool c = carryIn;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const int s = int{x[i]} + int{y[i]} + int{c};
        out[i] = (s & 1) != 0;
        c = s >= 2;
    }
    return out;
}

Bits notBits(const Bits& x) {
    Bits out(x.size());
    for (std::size_t i = 0; i < x.size(); ++i)
        out[i] = !x[i];
    return out;
}

/// Two's complement negation modulo 2^width.
Bits negBits(const Bits& x) { return addBits(notBits(x), Bits(x.size(), false), true); }

Bits subBits(const Bits& x, const Bits& y) { return addBits(x, notBits(y), true); }

/// Product modulo 2^width, by shift and add.
Bits mulBits(const Bits& x, const Bits& y) {
    const std::size_t w = x.size();
    Bits acc(w, false);
    for (std::size_t i = 0; i < w; ++i) {
        if (!y[i]) continue;
        Bits shifted(w, false);
        for (std::size_t k = i; k < w; ++k)
            shifted[k] = x[k - i];
        acc = addBits(acc, shifted);
    }
    return acc;
}

/// Unsigned comparison: -1, 0 or 1.
int cmpUnsigned(const Bits& x, const Bits& y) {
    for (std::size_t i = x.size(); i-- > 0;)
        if (x[i] != y[i]) return x[i] ? 1 : -1;
    return 0;
}

/// Unsigned long division; the divisor is not zero.
void divModUnsigned(const Bits& x, const Bits& y, Bits& q, Bits& r) {
    const std::size_t w = x.size();
    q.assign(w, false);
    r.assign(w, false);
    for (std::size_t i = w; i-- > 0;) {
        // r = (r << 1) | x[i]
        for (std::size_t k = w; k-- > 1;)
            r[k] = r[k - 1];
        r[0] = x[i];
        if (cmpUnsigned(r, y) >= 0) {
            r = subBits(r, y);
            q[i] = true;
        }
    }
}

/// Division with truncation toward 0 and the remainder taking the sign of
/// the dividend, for signed or unsigned operands of one width.
void divMod(const Bits& x, const Bits& y, bool isSigned, Bits& q, Bits& r) {
    if (!isSigned) {
        divModUnsigned(x, y, q, r);
        return;
    }
    const bool nx = msb(x);
    const bool ny = msb(y);
    divModUnsigned(nx ? negBits(x) : x, ny ? negBits(y) : y, q, r);
    if (nx != ny) q = negBits(q);
    if (nx) r = negBits(r);
}

Bits oneBits(std::size_t w) {
    Bits b(w, false);
    b[0] = true;
    return b;
}

/// Equality of two values with the rule of 3.4: a known mismatch gives 0,
/// otherwise an unknown bit gives X, otherwise 1. Bits where `wild` is X
/// or Z are skipped (for ==?).
Logic equality(const Value& x, const Value& y, bool wildcardRight) {
    bool unknown = false;
    for (std::size_t i = 0; i < x.bits.size(); ++i) {
        const Logic a = x.bits[i];
        const Logic b = y.bits[i];
        if (wildcardRight && !isKnown(b)) continue;
        if (isKnown(a) && isKnown(b)) {
            if (a != b) return Logic::Zero;
        } else {
            unknown = true;
        }
    }
    return unknown ? Logic::X : Logic::One;
}

Logic invert(Logic v) { return logicNot(v); }

Logic truthOf(const Value& x) {
    bool unknown = false;
    for (const Logic b : x.bits) {
        if (b == Logic::One) return Logic::One;
        if (b != Logic::Zero) unknown = true;
    }
    return unknown ? Logic::X : Logic::Zero;
}

/// The relation of 3.3 as -1, 0 or 1; only for operands without X or Z.
int compare(const Value& x, const Value& y) {
    Bits a = known(x);
    Bits b = known(y);
    if (x.isSigned && y.isSigned) {
        a.back() = !a.back();  // Flipping the sign bits orders signed values.
        b.back() = !b.back();
    }
    return cmpUnsigned(a, b);
}

template <typename Pred> Value relational(const Value& x, const Value& y, Pred pred) {
    if (x.hasUnknown() || y.hasUnknown()) return bit1(Logic::X);
    return bit1(pred(compare(x, y)) ? Logic::One : Logic::Zero);
}

template <typename F> Value perBit(const Value& x, const Value& y, F f) {
    Value out;
    out.isSigned = x.isSigned;
    out.bits.resize(x.bits.size());
    for (std::size_t i = 0; i < x.bits.size(); ++i)
        out.bits[i] = f(x.bits[i], y.bits[i]);
    return out;
}

template <typename F> Value reduce(const Value& x, F f, bool negate) {
    // A lone Z bit still gives X: z never comes out of these operators (3.6).
    Logic acc = isKnown(x.bits[0]) ? x.bits[0] : Logic::X;
    for (std::size_t i = 1; i < x.bits.size(); ++i)
        acc = f(acc, x.bits[i]);
    return bit1(negate ? invert(acc) : acc);
}

/// The shift amount as a number, saturated to `limit`. False if it has X or Z.
bool shiftAmount(const Value& amount, std::uint64_t limit, std::uint64_t& n) {
    if (amount.hasUnknown()) return false;
    n = 0;
    for (std::size_t i = amount.bits.size(); i-- > 0;) {
        if (n >= limit) return true;  // Already past the limit; stays saturated.
        n = n * 2 + (amount.bits[i] == Logic::One ? 1 : 0);
    }
    if (n > limit) n = limit;
    return true;
}

}  // namespace

bool Value::hasUnknown() const {
    for (const Logic b : bits)
        if (!isKnown(b)) return true;
    return false;
}

Value fromWords(const std::vector<std::uint64_t>& a, const std::vector<std::uint64_t>& b,
                std::uint32_t width, bool isSigned) {
    Value v;
    v.isSigned = isSigned;
    v.bits.resize(width);
    for (std::uint32_t i = 0; i < width; ++i) {
        const std::size_t w = i / 64;
        const bool av = w < a.size() && ((a[w] >> (i % 64)) & 1U) != 0;
        const bool bv = w < b.size() && ((b[w] >> (i % 64)) & 1U) != 0;
        v.bits[i] = bv ? (av ? Logic::X : Logic::Z) : (av ? Logic::One : Logic::Zero);
    }
    return v;
}

Value fromUint(std::uint64_t v, std::uint32_t width, bool isSigned) {
    return fromWords({v}, {}, width, isSigned);
}

Value filled(Logic bit, std::uint32_t width, bool isSigned) {
    return {std::vector<Logic>(width, bit), isSigned};
}

Value fromReal(double d, std::uint32_t width, bool isSigned, bool truncate) {
    if (std::isnan(d) || std::isinf(d)) return filled(Logic::X, width, isSigned);
    const double r = truncate ? std::trunc(d) : std::round(d);  // round: half away from zero
    // |r| = mant * 2^(exp - 53), with mant an integer below 2^53.
    int exp = 0;
    const double frac = std::frexp(std::fabs(r), &exp);
    const auto mant = static_cast<std::uint64_t>(std::ldexp(frac, 53));
    const int shift = exp - 53;
    Value v = filled(Logic::Zero, width, isSigned);
    for (std::uint32_t i = 0; i < width; ++i) {
        const std::int64_t k = static_cast<std::int64_t>(i) - shift;  // the bit of mant
        if (k >= 0 && k < 53 && ((mant >> k) & 1U) != 0) v.bits[i] = Logic::One;
    }
    return r < 0 ? neg(v) : v;
}

void toWords(const Value& v, std::vector<std::uint64_t>& a, std::vector<std::uint64_t>& b) {
    const std::size_t words = v.bits.size() / 64 + (v.bits.size() % 64 != 0);
    a.assign(words, 0);
    b.assign(words, 0);
    for (std::size_t i = 0; i < v.bits.size(); ++i) {
        const Logic x = v.bits[i];
        const std::uint64_t m = std::uint64_t{1} << (i % 64);
        if (x == Logic::One || x == Logic::X) a[i / 64] |= m;
        if (x == Logic::Z || x == Logic::X) b[i / 64] |= m;
    }
}

std::string toBinary(const Value& v) {
    std::string s;
    for (std::size_t i = v.bits.size(); i-- > 0;) {
        static constexpr char kChars[] = {'0', '1', 'z', 'x'};
        s += kChars[static_cast<int>(v.bits[i])];
    }
    return s;
}

Value add(const Value& x, const Value& y) {
    if (x.hasUnknown() || y.hasUnknown()) return allX(x.width(), x.isSigned);
    return fromBits(addBits(known(x), known(y)), x.isSigned);
}

Value sub(const Value& x, const Value& y) {
    if (x.hasUnknown() || y.hasUnknown()) return allX(x.width(), x.isSigned);
    return fromBits(subBits(known(x), known(y)), x.isSigned);
}

Value mul(const Value& x, const Value& y) {
    if (x.hasUnknown() || y.hasUnknown()) return allX(x.width(), x.isSigned);
    return fromBits(mulBits(known(x), known(y)), x.isSigned);
}

Value div(const Value& x, const Value& y) {
    if (x.hasUnknown() || y.hasUnknown() || isZero(known(y))) return allX(x.width(), x.isSigned);
    Bits q;
    Bits r;
    divMod(known(x), known(y), x.isSigned && y.isSigned, q, r);
    return fromBits(q, x.isSigned);
}

Value mod(const Value& x, const Value& y) {
    if (x.hasUnknown() || y.hasUnknown() || isZero(known(y))) return allX(x.width(), x.isSigned);
    Bits q;
    Bits r;
    divMod(known(x), known(y), x.isSigned && y.isSigned, q, r);
    return fromBits(r, x.isSigned);
}

Value pow(const Value& x, const Value& y) {
    const std::uint32_t w = x.width();
    if (x.hasUnknown() || y.hasUnknown()) return allX(w, x.isSigned);
    const Bits base = known(x);
    const Bits exp = known(y);
    const bool expNegative = y.isSigned && msb(exp);
    if (isZero(exp)) return fromBits(oneBits(w), x.isSigned);  // x ** 0 is 1, even 0 ** 0
    if (isZero(base))
        return expNegative ? allX(w, x.isSigned) : fromBits(Bits(w, false), x.isSigned);
    const bool baseIsOne = cmpUnsigned(base, oneBits(w)) == 0;
    const bool baseIsMinusOne = x.isSigned && cmpUnsigned(base, Bits(w, true)) == 0;
    if (baseIsOne) return fromBits(oneBits(w), x.isSigned);
    if (baseIsMinusOne) return fromBits(exp[0] ? base : oneBits(w), x.isSigned);
    if (expNegative) return fromBits(Bits(w, false), x.isSigned);
    // Square and multiply over the exponent's bits, modulo 2^w.
    Bits result = oneBits(w);
    for (std::size_t i = exp.size(); i-- > 0;) {
        result = mulBits(result, result);
        if (exp[i]) result = mulBits(result, base);
    }
    return fromBits(result, x.isSigned);
}

Value neg(const Value& x) {
    if (x.hasUnknown()) return allX(x.width(), x.isSigned);
    return fromBits(negBits(known(x)), x.isSigned);
}

Value lt(const Value& x, const Value& y) {
    return relational(x, y, [](int c) { return c < 0; });
}
Value le(const Value& x, const Value& y) {
    return relational(x, y, [](int c) { return c <= 0; });
}
Value gt(const Value& x, const Value& y) {
    return relational(x, y, [](int c) { return c > 0; });
}
Value ge(const Value& x, const Value& y) {
    return relational(x, y, [](int c) { return c >= 0; });
}

Value eq(const Value& x, const Value& y) { return bit1(equality(x, y, false)); }
Value ne(const Value& x, const Value& y) { return bit1(invert(equality(x, y, false))); }
Value weq(const Value& x, const Value& y) { return bit1(equality(x, y, true)); }
Value wne(const Value& x, const Value& y) { return bit1(invert(equality(x, y, true))); }

Value caseMatch(const Value& x, const Value& y, bool ignoreX) {
    for (std::size_t i = 0; i < x.bits.size(); ++i) {
        const Logic a = x.bits[i];
        const Logic b = y.bits[i];
        const auto ignored = [&](Logic l) { return l == Logic::Z || (ignoreX && l == Logic::X); };
        if (ignored(a) || ignored(b)) continue;
        if (a != b) return bit1(Logic::Zero);
    }
    return bit1(Logic::One);
}

Value ceq(const Value& x, const Value& y) {
    return bit1(x.bits == y.bits ? Logic::One : Logic::Zero);
}
Value cne(const Value& x, const Value& y) {
    return bit1(x.bits == y.bits ? Logic::Zero : Logic::One);
}

Value truth(const Value& x) { return bit1(truthOf(x)); }
Value lnot(const Value& x) { return bit1(invert(truthOf(x))); }
Value land(const Value& x, const Value& y) { return bit1(logicAnd(truthOf(x), truthOf(y))); }
Value lor(const Value& x, const Value& y) { return bit1(logicOr(truthOf(x), truthOf(y))); }

Value bitAnd(const Value& x, const Value& y) { return perBit(x, y, logicAnd); }
Value bitOr(const Value& x, const Value& y) { return perBit(x, y, logicOr); }
Value bitXor(const Value& x, const Value& y) { return perBit(x, y, logicXor); }
Value bitXnor(const Value& x, const Value& y) { return perBit(x, y, logicXnor); }
Value bitNot(const Value& x) {
    return perBit(x, x, [](Logic a, Logic) { return logicNot(a); });
}

Value redAnd(const Value& x) { return reduce(x, logicAnd, false); }
Value redNand(const Value& x) { return reduce(x, logicAnd, true); }
Value redOr(const Value& x) { return reduce(x, logicOr, false); }
Value redNor(const Value& x) { return reduce(x, logicOr, true); }
Value redXor(const Value& x) { return reduce(x, logicXor, false); }
Value redXnor(const Value& x) { return reduce(x, logicXor, true); }

Value countBits(const Value& x, bool zeros, bool ones, bool zs, bool xs) {
    std::uint64_t n = 0;
    for (const Logic b : x.bits) {
        const bool counted = (b == Logic::Zero && zeros) || (b == Logic::One && ones) ||
                             (b == Logic::Z && zs) || (b == Logic::X && xs);
        if (counted) ++n;
    }
    return fromUint(n, 32, true);
}

Value shl(const Value& x, const Value& amount) {
    std::uint64_t n = 0;
    if (!shiftAmount(amount, x.width(), n)) return allX(x.width(), x.isSigned);
    Value out = filled(Logic::Zero, x.width(), x.isSigned);
    for (std::uint64_t i = n; i < x.width(); ++i)
        out.bits[i] = x.bits[i - n];
    return out;
}

Value shr(const Value& x, const Value& amount) {
    std::uint64_t n = 0;
    if (!shiftAmount(amount, x.width(), n)) return allX(x.width(), x.isSigned);
    Value out = filled(Logic::Zero, x.width(), x.isSigned);
    for (std::uint64_t i = 0; i + n < x.width(); ++i)
        out.bits[i] = x.bits[i + n];
    return out;
}

Value ashr(const Value& x, const Value& amount) {
    std::uint64_t n = 0;
    if (!shiftAmount(amount, x.width(), n)) return allX(x.width(), x.isSigned);
    const Logic fill = x.isSigned ? x.bits.back() : Logic::Zero;
    Value out = filled(fill, x.width(), x.isSigned);
    for (std::uint64_t i = 0; i + n < x.width(); ++i)
        out.bits[i] = x.bits[i + n];
    return out;
}

Value select(const Value& cond, const Value& x, const Value& y) {
    const Logic c = truthOf(cond);
    if (c == Logic::One) return x;
    if (c == Logic::Zero) return y;
    if (equality(x, y, false) == Logic::One) return x;
    Value out = filled(Logic::X, x.width(), x.isSigned);
    for (std::size_t i = 0; i < x.bits.size(); ++i)
        if (isKnown(x.bits[i]) && x.bits[i] == y.bits[i]) out.bits[i] = x.bits[i];
    return out;
}

Value zext(const Value& x, std::uint32_t width, bool isSigned) {
    Value out = filled(Logic::Zero, width, isSigned);
    for (std::uint32_t i = 0; i < x.width() && i < width; ++i)
        out.bits[i] = x.bits[i];
    return out;
}

Value sext(const Value& x, std::uint32_t width, bool isSigned) {
    Value out = filled(x.bits.back(), width, isSigned);
    for (std::uint32_t i = 0; i < x.width() && i < width; ++i)
        out.bits[i] = x.bits[i];
    return out;
}

Value trunc(const Value& x, std::uint32_t width, bool isSigned) { return zext(x, width, isSigned); }

std::optional<std::int64_t> positionOf(const Value& v) {
    if (v.hasUnknown()) return std::nullopt;
    const bool negative = v.isSigned && v.bits.back() == Logic::One;
    // Beyond 64 signed bits, every bit above 62 equals the sign.
    for (std::size_t i = 63; i < v.bits.size(); ++i)
        if ((v.bits[i] == Logic::One) != negative) return std::nullopt;
    std::uint64_t u = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        const Logic b = i < v.bits.size() ? v.bits[i] : (negative ? Logic::One : Logic::Zero);
        if (b == Logic::One) u |= std::uint64_t{1} << i;
    }
    if (!negative && (u >> 63) != 0) return std::nullopt;
    return static_cast<std::int64_t>(u);
}

Value bits(const Value& x, std::int64_t lsb, std::uint32_t width) {
    Value out = filled(Logic::X, width, false);
    // Wholly outside, without computing lsb + i, which could overflow.
    if (lsb >= static_cast<std::int64_t>(x.width()) || lsb <= -static_cast<std::int64_t>(width))
        return out;
    for (std::uint32_t i = 0; i < width; ++i) {
        const std::int64_t at = lsb + i;
        if (at >= 0 && at < static_cast<std::int64_t>(x.width()))
            out.bits[i] = x.bits[static_cast<std::size_t>(at)];
    }
    return out;
}

Value bitSelect(const Value& x, const Value& index) {
    const std::optional<std::int64_t> at = positionOf(index);
    if (!at) return filled(Logic::X, 1, false);
    return bits(x, *at, 1);
}

Value indexedPartSelect(const Value& x, const Value& pos, std::uint32_t width) {
    const std::optional<std::int64_t> at = positionOf(pos);
    if (!at) return filled(Logic::X, width, false);
    return bits(x, *at, width);
}

Value concat(const std::vector<Value>& parts) {
    Value out;
    for (auto p = parts.rbegin(); p != parts.rend(); ++p)
        out.bits.insert(out.bits.end(), p->bits.begin(), p->bits.end());
    return out;
}

Value replicate(std::uint32_t n, const Value& x) { return concat(std::vector<Value>(n, x)); }

Value writeBits(const Value& x, const Value& pos, const Value& v) {
    Value out = x;
    const std::optional<std::int64_t> at = positionOf(pos);
    // Nothing inside x, without computing *at + i, which could overflow.
    if (!at || *at >= static_cast<std::int64_t>(x.width()) ||
        *at <= -static_cast<std::int64_t>(v.width()))
        return out;
    for (std::uint32_t i = 0; i < v.width(); ++i) {
        const std::int64_t to = *at + i;
        if (to >= 0 && to < static_cast<std::int64_t>(x.width()))
            out.bits[static_cast<std::size_t>(to)] = v.bits[i];
    }
    return out;
}

std::string toText(const Value& x) {
    std::string s;
    for (std::uint32_t k = static_cast<std::uint32_t>((std::uint64_t{x.width()} + 7) / 8);
         k-- > 0;) {
        unsigned c = 0;
        for (std::uint32_t i = 0; i < 8 && 8 * k + i < x.width(); ++i)
            if (x.bits[8 * k + i] == Logic::One) c |= 1U << i;
        if (c != 0) s.push_back(static_cast<char>(c));
    }
    return s;
}

Value fromText(const std::string& s, std::uint32_t width) {
    Value out = filled(Logic::Zero, width, false);
    std::uint32_t byte = 0;
    for (auto c = s.rbegin(); c != s.rend() && 8 * byte < width; ++c, ++byte)
        for (std::uint32_t i = 0; i < 8 && 8 * byte + i < width; ++i)
            if (((static_cast<unsigned char>(*c) >> i) & 1U) != 0)
                out.bits[8 * byte + i] = Logic::One;
    return out;
}

Value to2(const Value& x) {
    Value out = x;
    for (Logic& b : out.bits)
        if (!isKnown(b)) b = Logic::Zero;
    return out;
}

}  // namespace kirimasim::valsem
