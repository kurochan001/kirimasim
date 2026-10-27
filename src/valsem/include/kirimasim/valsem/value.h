// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Reference semantics of the SIR value instructions (SIR_SPEC.md 5.2,
// VALUE_SEMANTICS.md 2 and 3), for integral values of any width, one bit at
// a time.
//
// Every function computes the four-state result. A two-state operand is a
// value without X or Z bits; storing a result into a two-state type is the
// separate conversion to2(). Widths follow the SIR rules: the operands of a
// binary operation have the width of the result unless the description
// says otherwise.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "kirimasim/valsem/logic.h"

namespace kirimasim::valsem {

/// An integral value. bits[0] is the least significant bit. The width is
/// bits.size() and is at least 1.
struct Value {
    std::vector<Logic> bits;
    bool isSigned = false;

    std::uint32_t width() const { return static_cast<std::uint32_t>(bits.size()); }
    bool hasUnknown() const;  ///< Any X or Z bit.
    friend bool operator==(const Value&, const Value&) = default;
};

/// A value of the given width from the (a, b) encoding of kernel/w4.h,
/// least significant word first.
Value fromWords(const std::vector<std::uint64_t>& a, const std::vector<std::uint64_t>& b,
                std::uint32_t width, bool isSigned);
/// A two-state value of at most 64 bits.
Value fromUint(std::uint64_t v, std::uint32_t width, bool isSigned);
/// A value whose bits are all `bit`.
Value filled(Logic bit, std::uint32_t width, bool isSigned);
/// A real converted to an integral value of `width` bits (VALUE_SEMANTICS.md
/// 7.4): rounded half away from zero (truncated toward zero for $rtoi,
/// SYSTEM_TASKS.md 8), then the low bits of that integer's two's
/// complement, at any magnitude. NaN and the infinities give X in every bit
/// (a two-state target takes to2 of it: 0).
Value fromReal(double d, std::uint32_t width, bool isSigned, bool truncate = false);
/// The (a, b) words of a value (the inverse of fromWords).
void toWords(const Value& v, std::vector<std::uint64_t>& a, std::vector<std::uint64_t>& b);
/// The bits as text, most significant first: "10xz".
std::string toBinary(const Value& v);

// Arithmetic (3.2). Any X or Z bit in an operand makes every result bit X.
Value add(const Value& x, const Value& y);
Value sub(const Value& x, const Value& y);
Value mul(const Value& x, const Value& y);
Value div(const Value& x, const Value& y);  ///< Truncates toward 0; X on division by 0.
Value mod(const Value& x, const Value& y);  ///< Sign of the dividend; X on division by 0.
Value pow(const Value& x, const Value& y);  ///< Table 11-4 for integers.
Value neg(const Value& x);

// Relational (3.3). Signed only when both operands are signed. The result
// is 1 bit: 1, 0, or X if any operand bit is X or Z.
Value lt(const Value& x, const Value& y);
Value le(const Value& x, const Value& y);
Value gt(const Value& x, const Value& y);
Value ge(const Value& x, const Value& y);

// Equality (3.4). 1-bit results.
Value eq(const Value& x, const Value& y);
Value ne(const Value& x, const Value& y);
Value ceq(const Value& x, const Value& y);  ///< ===, always 0 or 1.
Value cne(const Value& x, const Value& y);  ///< !==, always 0 or 1.
Value weq(const Value& x, const Value& y);  ///< ==?: X and Z in y match anything.
Value wne(const Value& x, const Value& y);
/// casez (ignoreX false) and casex (ignoreX true) matching (4.2): z bits,
/// and x bits too for casex, on either side are not compared; the other
/// bits must be the same value. Always 0 or 1.
Value caseMatch(const Value& x, const Value& y, bool ignoreX);

// Logical (3.5). The truth of a value: 1 if it has a 1 bit, 0 if all bits
// are 0, X otherwise. Results are 1 bit.
Value truth(const Value& x);
Value lnot(const Value& x);
Value land(const Value& x, const Value& y);
Value lor(const Value& x, const Value& y);

// Bitwise (3.6), per bit.
Value bitAnd(const Value& x, const Value& y);
Value bitOr(const Value& x, const Value& y);
Value bitXor(const Value& x, const Value& y);
Value bitXnor(const Value& x, const Value& y);
Value bitNot(const Value& x);

// Reduction (3.7). 1-bit unsigned results.
Value redAnd(const Value& x);
Value redNand(const Value& x);
Value redOr(const Value& x);
Value redNor(const Value& x);
Value redXor(const Value& x);
Value redXnor(const Value& x);

/// $countbits (20.9; SYSTEM_TASKS.md 8): the number of bits of x whose
/// value is one of those chosen, as a 32-bit signed two-state value (an
/// int). $countones, $onehot, $onehot0 and $isunknown are defined by it.
Value countBits(const Value& x, bool zeros, bool ones, bool zs, bool xs);

// Shifts (3.8). The amount is unsigned and may have any width; X or Z in
// it makes every result bit X. ashr fills with the sign bit only when x is
// signed.
Value shl(const Value& x, const Value& amount);
Value shr(const Value& x, const Value& amount);
Value ashr(const Value& x, const Value& amount);

/// Conditional operator with both sides evaluated (3.9): the truth of
/// `cond` picks a side; when it is unknown, equal sides give that value and
/// otherwise the sides are merged bit by bit (0/0 -> 0, 1/1 -> 1, else X).
Value select(const Value& cond, const Value& x, const Value& y);

// Conversions (2, 1.2). Widths change; signedness of the result is given.
Value zext(const Value& x, std::uint32_t width, bool isSigned);
Value sext(const Value& x, std::uint32_t width, bool isSigned);  ///< Fills with the sign bit.
Value trunc(const Value& x, std::uint32_t width, bool isSigned);
Value to2(const Value& x);  ///< X and Z become 0.

// Selects (3.11), concatenation and replication (7.1, 7.2). An index or a
// position is signed when its value is.

/// The value of an index or a position: nothing when it has X or Z bits or
/// lies beyond 64 signed bits, which is outside any value.
std::optional<std::int64_t> positionOf(const Value& v);
/// Bits [lsb, lsb + width) of x, unsigned; a bit outside x is X.
Value bits(const Value& x, std::int64_t lsb, std::uint32_t width);
/// x[index], 1 bit, unsigned: X when the index has no position or is
/// outside x.
Value bitSelect(const Value& x, const Value& index);
/// x[pos +: width]: every bit X when pos has no position, else bits().
Value indexedPartSelect(const Value& x, const Value& pos, std::uint32_t width);
/// {parts...}: the first is the most significant; unsigned.
Value concat(const std::vector<Value>& parts);
/// {n{x}}, n at least 1; unsigned.
Value replicate(std::uint32_t n, const Value& x);
/// x with the bits of v written from bit pos: only the bits inside x;
/// nothing when pos has no position.
Value writeBits(const Value& x, const Value& pos, const Value& v);

// Strings (6.16).

/// Reference scanner for string.atoreal() (6.16.10, 5.7.2, VALUE_SEMANTICS.md 7.6).
double stringToReal(const std::string& text);

/// Reference scanner for atoi, atohex, atooct and atobin (6.16.9,
/// VALUE_SEMANTICS.md 7.6), base 2, 8, 10 or 16.
std::uint32_t stringToInteger(const std::string& text, unsigned base);

/// The bytes of x as characters, the most significant first; zero bytes
/// are dropped, and X and Z bits read as 0.
std::string toText(const Value& x);
/// A string as an unsigned two-state value of `width` bits: the last
/// character in the low byte; characters that do not fit are dropped.
Value fromText(const std::string& s, std::uint32_t width);

}  // namespace kirimasim::valsem
