// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Owned packed strength values (MOS/switch contract 1/2/7). This transport
// is internal, not a language type or a C ABI. Eight signed lo bytes in a,
// eight signed hi bytes in b; bit zero is in the least significant byte.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include "kirimasim/kernel/mos.h"

namespace kirimasim::kernel {

constexpr std::uint32_t strengthWordCount(std::uint32_t width) noexcept {
    return width == 0 ? 0 : (width - 1) / 8 + 1;
}

constexpr std::optional<StrengthRange> strengthRangeAt(std::span<const W4> words,
                                                       std::uint32_t bit) noexcept {
    if (bit / 8 >= words.size()) return std::nullopt;
    const auto& word = words[bit / 8];
    const auto shift = 8 * (bit % 8);
    const auto signedByte = [](std::uint64_t byte) {
        return static_cast<std::int8_t>(byte < 128 ? static_cast<int>(byte)
                                                   : static_cast<int>(byte) - 256);
    };
    const StrengthRange range{signedByte((word.a >> shift) & 255U),
                              signedByte((word.b >> shift) & 255U)};
    if (range.lo < -7 || range.hi > 7 || range.lo > range.hi) return std::nullopt;
    return range;
}

constexpr bool writeStrengthRange(std::span<W4> words, std::uint32_t bit,
                                  StrengthRange range) noexcept {
    if (bit / 8 >= words.size() || range.lo < -7 || range.hi > 7 || range.lo > range.hi)
        return false;
    auto& word = words[bit / 8];
    const auto shift = 8 * (bit % 8);
    const auto mask = ~(std::uint64_t{255} << shift);
    word.a = (word.a & mask) | (std::uint64_t{static_cast<std::uint8_t>(range.lo)} << shift);
    word.b = (word.b & mask) | (std::uint64_t{static_cast<std::uint8_t>(range.hi)} << shift);
    return true;
}

namespace detail {
constexpr std::uint64_t valueWordCount(std::uint64_t width) noexcept {
    return width == 0 ? 0 : (width - 1) / 64 + 1;
}
constexpr bool strengthBit(std::span<const W4> words, std::uint64_t bit) noexcept {
    return ((words[bit / 64].a >> (bit % 64)) & 1U) != 0;
}
inline bool validStrengthValue(std::span<const W4> words, std::uint32_t width) noexcept {
    if (width == 0 || words.size() < strengthWordCount(width)) return false;
    for (std::uint32_t bit = 0; bit < width; ++bit)
        if (!strengthRangeAt(words, bit)) return false;
    return true;
}
}  // namespace detail

// Output spans must be disjoint from input spans. Invalid shape, ranks or
// endpoints return false before writing output. Successful operations zero
// unused bytes/bits in the last used word and leave surplus output words alone.
// Side bit b enables the one side, side bit width+b enables the zero side;
// an absent side enables both. Side words carry two-state a bits only.
inline bool makeStrengthValue(std::span<W4> out, std::uint32_t width, std::span<const W4> value,
                              unsigned s1, unsigned s0, std::span<const W4> side = {}) noexcept {
    if (width == 0 || out.size() < strengthWordCount(width) ||
        value.size() < detail::valueWordCount(width) || s1 > 7 || s0 > 7 ||
        (!side.empty() && side.size() < detail::valueWordCount(std::uint64_t{width} * 2)))
        return false;
    std::fill_n(out.begin(), strengthWordCount(width), W4{});
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const auto& word = value[bit / 64];
        const bool a = ((word.a >> (bit % 64)) & 1U) != 0;
        const bool b = ((word.b >> (bit % 64)) & 1U) != 0;
        const auto one =
            static_cast<std::int8_t>(side.empty() || detail::strengthBit(side, bit) ? s1 : 0);
        const auto zero = static_cast<std::int8_t>(
            side.empty() || detail::strengthBit(side, std::uint64_t{width} + bit) ? -int(s0) : 0);
        const StrengthRange range = !b  ? (a ? StrengthRange{one, one} : StrengthRange{zero, zero})
                                    : a ? StrengthRange{zero, one}
                                        : StrengthRange{};
        writeStrengthRange(out, bit, range);
    }
    return true;
}

inline bool projectStrengthValue(std::span<W4> out, std::uint32_t width,
                                 std::span<const W4> input) noexcept {
    if (out.size() < detail::valueWordCount(width) || !detail::validStrengthValue(input, width))
        return false;
    std::fill_n(out.begin(), detail::valueWordCount(width), W4{});
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const auto range = strengthRangeAt(input, bit);
        if (!range) return false;  // Prevalidated, with disjoint output.
        const auto value = strengthValue(*range);
        out[bit / 64].a |= value.a << (bit % 64);
        out[bit / 64].b |= value.b << (bit % 64);
    }
    return true;
}

inline bool mosStrengthValue(std::span<W4> out, std::uint32_t width, MosKind kind,
                             std::span<const W4> input, W4 first, W4 second = {}) noexcept {
    if (out.size() < strengthWordCount(width) || !detail::validStrengthValue(input, width) ||
        !mosStrength(kind, {}, first, second))
        return false;
    std::fill_n(out.begin(), strengthWordCount(width), W4{});
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const auto range = strengthRangeAt(input, bit);
        if (!range) return false;
        const auto result = mosStrength(kind, *range, first, second);
        if (!result) return false;
        writeStrengthRange(out, bit, *result);
    }
    return true;
}

}  // namespace kirimasim::kernel
