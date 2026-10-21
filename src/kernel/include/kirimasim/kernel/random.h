// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// $random and the $dist_* functions (SYSTEM_TASKS.md 12, 20.14, Annex N),
// written from SYSTEM_TASKS.md 12 alone.
//
// Each takes the seed by reference, updates it as the function does, and
// returns the result. The real arithmetic must not be contracted into
// fused multiply-adds (SYSTEM_TASKS.md 12.2): the kernel is built with
// -ffp-contract=off.
//
// This header is part of the kernel's public interface and must stay
// compilable as C++20.

#pragma once

#include <cstdint>

namespace kirimasim::kernel {

/// A function's result, and whether an argument was out of its range
/// (0 or less where it is to be above 0: kirima-dist-argument).
struct DistResult {
    std::int32_t value = 0;
    bool badArgument = false;
};

/// $random(seed): $dist_uniform(seed, -2^31, 2^31 - 1).
std::int32_t randomInt(std::int32_t& seed);
DistResult distUniform(std::int32_t& seed, std::int32_t start, std::int32_t end);
DistResult distNormal(std::int32_t& seed, std::int32_t mean, std::int32_t sd);
DistResult distExponential(std::int32_t& seed, std::int32_t mean);
DistResult distPoisson(std::int32_t& seed, std::int32_t mean);
DistResult distChiSquare(std::int32_t& seed, std::int32_t df);
DistResult distT(std::int32_t& seed, std::int32_t df);
DistResult distErlang(std::int32_t& seed, std::int32_t k, std::int32_t mean);

}  // namespace kirimasim::kernel
