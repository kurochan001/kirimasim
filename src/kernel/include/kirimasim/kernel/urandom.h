// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The random number generators of $urandom, $urandom_range and randcase
// (SYSTEM_TASKS.md 13, 18.13, 18.14): SplitMix64, and the seeds the
// initialization generators of instances and packages start from.
//
// Integer arithmetic only, so that every OS and tier gives the same
// numbers. The state of each generator lives with what owns it: a
// process of the tier, or an instance's initialization generator.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>
#include <string_view>

namespace kirimasim::kernel {

/// One random number generator: a state of 64 bits (SYSTEM_TASKS.md 13.1).
struct Rng {
    std::uint64_t state = 0;

    /// One step: the state moves on, and the step's value of 64 bits.
    std::uint64_t next() {
        state += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    /// A random number of 32 bits: the upper half of a step's value.
    std::uint32_t draw() { return static_cast<std::uint32_t>(next() >> 32); }
    /// A new generator, seeded with the whole value of a step (18.14.2).
    Rng child() { return Rng{next()}; }
};

/// The state an initialization generator starts from: the default seed
/// XOR the 64-bit FNV-1a hash of the hierarchical name (SYSTEM_TASKS.md
/// 13.2).
inline std::uint64_t initialState(std::uint64_t seed, std::string_view name) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : name) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ULL;
    }
    return seed ^ h;
}

/// $urandom_range(maxval, minval): the two swapped when maxval is the
/// smaller, then minval + floor(r * n / 2^32) for the next random number r
/// and n = maxval - minval + 1 (SYSTEM_TASKS.md 13.3).
inline std::uint32_t urandomRange(Rng& g, std::uint32_t maxval, std::uint32_t minval) {
    if (maxval < minval) {
        const std::uint32_t t = maxval;
        maxval = minval;
        minval = t;
    }
    const std::uint64_t n = std::uint64_t{maxval} - minval + 1;
    return minval + static_cast<std::uint32_t>((std::uint64_t{g.draw()} * n) >> 32);
}

}  // namespace kirimasim::kernel
