// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include "kirimasim/kernel/w4.h"
#include <cstdint>
#include <optional>

namespace kirimasim::vm::detail {
/// Private register encoding. Bits 1..32 of b remain available for the
/// actual container type and bits 60..63 for the independent rights/tags.
struct OrdinaryReference {
    static constexpr unsigned generationShift = 33;
    static constexpr std::uint32_t lastGeneration = (1U << 27) - 1;
    static std::optional<kernel::W4> make(std::uint32_t row, std::uint32_t generation,
                                          std::uint64_t word) noexcept {
        if (word > UINT32_MAX || !generation || generation > lastGeneration) return {};
        return kernel::W4{(std::uint64_t(row) << 32) | word,
                          (std::uint64_t(generation) << generationShift) | 1};
    }
    static std::uint32_t generation(kernel::W4 token) noexcept {
        return static_cast<std::uint32_t>((token.b >> generationShift) & lastGeneration);
    }
    static bool matches(kernel::W4 token, bool live, std::uint32_t current) noexcept {
        return live && current && generation(token) == current;
    }
};
}  // namespace kirimasim::vm::detail
