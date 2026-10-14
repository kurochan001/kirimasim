// Four-state 64-bit words and their fast bitwise operations.
//
// This header is part of the kernel's public interface. Generated code reads
// it, so it must stay compilable as C++20.

#pragma once

#include <cstdint>

namespace haremasim::kernel {

/// 64 four-state bits. Each bit position is encoded by the pair (a, b),
/// with the same meaning as aval/bval in VPI s_vpi_vecval and DPI
/// svLogicVecVal:
///
///   value | a | b
///   ------+---+---
///     0   | 0 | 0
///     1   | 1 | 0
///     Z   | 0 | 1
///     X   | 1 | 1
struct W4 {
    std::uint64_t a;
    std::uint64_t b;
};

/// Bitwise AND of two four-state words.
///
/// When neither input has an X or Z bit, only the two-state AND is computed.
/// Otherwise, per bit: the result is 0 if either input is a known 0, 1 if
/// both inputs are known 1, and X in every other case.
///
/// The reference definition lives in valsem; tests/unit compares the two.
inline W4 and4(W4 x, W4 y) {
    if ((x.b | y.b) == 0) return {x.a & y.a, 0};               // no X/Z: two-state only
    const std::uint64_t zero = (~x.a & ~x.b) | (~y.a & ~y.b);  // either side is a known 0
    const std::uint64_t one = (x.a & ~x.b) & (y.a & ~y.b);     // both sides are known 1
    return {~zero, ~zero & ~one};                              // everything else is X
}

}  // namespace haremasim::kernel
