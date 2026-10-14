// Reference semantics for four-state values, one bit at a time.
//
// These definitions are slow but obviously correct. Fast implementations in
// the kernel and in generated code are tested against them.

#pragma once

#include <cstdint>

namespace haremasim::valsem {

/// One four-state bit.
enum class Logic : std::uint8_t { Zero, One, Z, X };

/// Bitwise AND, written from its truth table:
/// 0 if either side is 0, 1 if both sides are 1, X otherwise.
Logic logicAnd(Logic x, Logic y);

}  // namespace haremasim::valsem
