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
#include <string>
#include <vector>

#include "haremasim/valsem/logic.h"

namespace haremasim::valsem {

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

}  // namespace haremasim::valsem
