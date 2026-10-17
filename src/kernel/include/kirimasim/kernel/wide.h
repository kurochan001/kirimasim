// Four-state operations on integral values of any width, held in
// consecutive W4 chunks, the least significant first: chunk k holds bits
// [64k, 64k + 64). Operands and results have their bits above the width 0
// in both a and b. These are the same operations as w4ops.h for values
// wider than 64 bits (VALUE_SEMANTICS.md 3); the reference definitions are
// in valsem/value.h, and tests/unit/wide_test.cpp compares the two.
//
// This header is part of the kernel's public interface. Generated code reads
// it, so it must stay compilable as C++20.

#pragma once

#include <cstdint>

#include "kirimasim/kernel/w4.h"

namespace kirimasim::kernel::wide {

/// The chunks of a w-bit value.
inline unsigned chunks(unsigned w) { return (w + 63) / 64; }

// --- Conversions (VALUE_SEMANTICS.md 2, 1.2). ------------------------------

/// x of width xw to width rw: truncated, or extended with 0 or, when
/// signExtend, with bit xw-1 (a and b alike).
void resize(W4* r, unsigned rw, const W4* x, unsigned xw, bool signExtend);
/// X and Z become 0.
void to2(W4* r, const W4* x, unsigned w);
/// Every bit X.
void allX(W4* r, unsigned w);
bool hasUnknown(const W4* x, unsigned w);

// --- Bitwise (3.6) and reduction (3.7). ------------------------------------

void bitAnd(W4* r, const W4* x, const W4* y, unsigned w);
void bitOr(W4* r, const W4* x, const W4* y, unsigned w);
void bitXor(W4* r, const W4* x, const W4* y, unsigned w);
void bitXnor(W4* r, const W4* x, const W4* y, unsigned w);
void bitNot(W4* r, const W4* x, unsigned w);
W4 redAnd(const W4* x, unsigned w);
W4 redOr(const W4* x, unsigned w);
W4 redXor(const W4* x, unsigned w);
/// $countbits (20.9) of x.
unsigned countBits(const W4* x, unsigned w, bool zeros, bool ones, bool zs, bool xs);

// --- Truth (3.5). 1-bit results. --------------------------------------------

W4 truth(const W4* x, unsigned w);

// --- Arithmetic (3.2). Any X or Z bit makes every result bit X. ------------

void add(W4* r, const W4* x, const W4* y, unsigned w);
void sub(W4* r, const W4* x, const W4* y, unsigned w);
void mul(W4* r, const W4* x, const W4* y, unsigned w);
void neg(W4* r, const W4* x, unsigned w);
/// Division truncating toward 0, and the remainder with the sign of the
/// dividend; X on division by 0.
void div(W4* r, const W4* x, const W4* y, unsigned w, bool isSigned);
void mod(W4* r, const W4* x, const W4* y, unsigned w, bool isSigned);
/// Integer power (table 11-4): x of width w, the exponent y of width yw.
void pow(W4* r, const W4* x, unsigned w, bool xSigned, const W4* y, unsigned yw, bool ySigned);

// --- Relational (3.3) and equality (3.4). 1-bit results. --------------------

W4 lt(const W4* x, const W4* y, unsigned w, bool isSigned);
W4 le(const W4* x, const W4* y, unsigned w, bool isSigned);
W4 gt(const W4* x, const W4* y, unsigned w, bool isSigned);
W4 ge(const W4* x, const W4* y, unsigned w, bool isSigned);
W4 eq(const W4* x, const W4* y, unsigned w);
W4 ne(const W4* x, const W4* y, unsigned w);
W4 ceq(const W4* x, const W4* y, unsigned w);
W4 cne(const W4* x, const W4* y, unsigned w);
W4 weq(const W4* x, const W4* y, unsigned w);
W4 wne(const W4* x, const W4* y, unsigned w);
/// casez (ignoreX false) and casex matching (4.2). Always 0 or 1.
W4 caseMatch(const W4* x, const W4* y, unsigned w, bool ignoreX);

// --- Shifts (3.8). The amount is unsigned, of width aw. ---------------------

void shl(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw);
void shr(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw);
/// >>>: fills with the sign bit when isSigned, else with 0.
void ashr(W4* r, const W4* x, unsigned w, const W4* amount, unsigned aw, bool isSigned);

// --- Conditional operator with both sides evaluated (3.9). ------------------

void select(W4* r, W4 cond, const W4* x, const W4* y, unsigned w);

// --- Selects (3.11) and concatenation (7.1). --------------------------------

/// Bits [lsb, lsb + rw) of x; the bits outside x are X.
void bits(W4* r, unsigned rw, const W4* x, unsigned xw, std::int64_t lsb);
/// Writes the xw bits of x into r at bit lsb, leaving the others.
void put(W4* r, unsigned lsb, const W4* x, unsigned xw);

// --- Reals (1.5). -----------------------------------------------------------

/// The value as a real; signed when isSigned. X and Z bits count as 0.
double toReal(const W4* x, unsigned w, bool isSigned);
/// A real rounded half away from zero, to w bits; NaN and values that do
/// not fit give 0 (7.4).
void fromReal(W4* r, unsigned w, double d);

}  // namespace kirimasim::kernel::wide
