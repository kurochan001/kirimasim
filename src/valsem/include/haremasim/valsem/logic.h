// Reference semantics for four-state values, one bit at a time.
//
// These definitions are slow but obviously correct. Fast implementations in
// the kernel and in generated code are tested against them.

#pragma once

#include <cstdint>

namespace haremasim::valsem {

/// One four-state bit.
enum class Logic : std::uint8_t { Zero, One, Z, X };

/// True for 0 and 1.
inline bool isKnown(Logic v) { return v == Logic::Zero || v == Logic::One; }

/// Bitwise AND, written from its truth table:
/// 0 if either side is 0, 1 if both sides are 1, X otherwise.
Logic logicAnd(Logic x, Logic y);

/// Bitwise OR: 1 if either side is 1, 0 if both sides are 0, X otherwise.
Logic logicOr(Logic x, Logic y);

/// Bitwise XOR: the exclusive or of two known bits, X otherwise.
Logic logicXor(Logic x, Logic y);

/// Bitwise XNOR: the negated exclusive or of two known bits, X otherwise.
Logic logicXnor(Logic x, Logic y);

/// Bitwise NOT: swaps 0 and 1; X and Z become X.
Logic logicNot(Logic x);

/// Whether a change of the least significant bit from `from` to `to` is a
/// posedge: 0 to 1, x or z; x or z to 1 (VALUE_SEMANTICS.md 4.3, 9.4.2).
bool isPosedge(Logic from, Logic to);

/// Whether a change from `from` to `to` is a negedge: 1 to 0, x or z;
/// x or z to 0.
bool isNegedge(Logic from, Logic to);

}  // namespace haremasim::valsem
