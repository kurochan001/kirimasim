// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original packed-value checks, MOS/switch contract 1/2/7; IEEE 1800
// 28.7/28.9/28.13/28.14 and VALUE_SEMANTICS 5.
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/strength_value.h"
#include "kirimasim/valsem/mos.h"
#include "kirimasim/valsem/strength.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
constexpr std::array<k::W4, 4> bits{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
void bit(std::vector<k::W4>& words, unsigned at, k::W4 value) {
    words[at / 64].a |= value.a << (at % 64);
    words[at / 64].b |= value.b << (at % 64);
}
TEST(StrengthValue, AllIntervalsPackWithoutHostByteOrderOrNeighbourChanges) {
    std::array<k::W4, 4> words{};
    for (unsigned at = 0; at < 32; ++at)
        for (int lo = -7; lo <= 7; ++lo)
            for (int hi = lo; hi <= 7; ++hi) {
                const auto previous = words;
                const k::StrengthRange range{static_cast<std::int8_t>(lo),
                                             static_cast<std::int8_t>(hi)};
                ASSERT_TRUE(k::writeStrengthRange(words, at, range));
                const auto decoded = k::strengthRangeAt(words, at);
                if (!decoded.has_value()) {
                    FAIL() << "missing validated value";
                }
                EXPECT_EQ(*decoded, range);
                for (unsigned other = 0; other < 32; ++other)
                    if (other != at)
                        EXPECT_EQ(k::strengthRangeAt(words, other),
                                  k::strengthRangeAt(previous, other));
                EXPECT_EQ((words[at / 8].a >> (8 * (at % 8))) & 255, static_cast<std::uint8_t>(lo));
                EXPECT_EQ((words[at / 8].b >> (8 * (at % 8))) & 255, static_cast<std::uint8_t>(hi));
            }
    EXPECT_EQ(k::strengthWordCount(0), 0U);
    EXPECT_EQ(k::strengthWordCount(UINT32_MAX), 536870912U);
}

TEST(StrengthValue, MakeAndProjectionMatchReferenceAcrossEveryRankSideAndLogic) {
    for (const unsigned width : {1U, 7U, 8U, 9U, 31U, 32U, 33U, 63U, 64U, 65U, 129U})
        for (unsigned s1 = 0; s1 <= 7; ++s1)
            for (unsigned s0 = 0; s0 <= 7; ++s0)
                for (const bool masked : {false, true}) {
                    std::vector<k::W4> data((width + 63) / 64), side((2 * width + 63) / 64);
                    for (unsigned at = 0; at < width; ++at) {
                        bit(data, at, bits[at % 4]);
                        bit(side, at, {at % 3 != 0 ? 1U : 0U, 0});
                        bit(side, width + at, {at % 5 != 0 ? 1U : 0U, 0});
                    }
                    std::vector<k::W4> packed(k::strengthWordCount(width) + 1, {~0ULL, ~0ULL});
                    ASSERT_TRUE(k::makeStrengthValue(packed, width, data, s1, s0,
                                                     masked ? side : std::span<const k::W4>{}));
                    std::vector<k::W4> projected(data.size() + 1, {~0ULL, ~0ULL});
                    ASSERT_TRUE(k::projectStrengthValue(projected, width, packed));
                    for (unsigned at = 0; at < width; ++at) {
                        const auto expected =
                            v::driverSignal({static_cast<v::Logic>(at % 4),
                                             masked && at % 3 == 0 ? 0 : static_cast<int>(s1),
                                             masked && at % 5 == 0 ? 0 : static_cast<int>(s0)});
                        const auto actual = k::strengthRangeAt(packed, at);
                        if (!actual.has_value()) {
                            FAIL() << "missing validated value";
                        }
                        EXPECT_EQ(actual->lo, expected.lo);
                        EXPECT_EQ(actual->hi, expected.hi);
                        const auto logic = v::valueOf(expected);
                        EXPECT_EQ((projected[at / 64].a >> (at % 64)) & 1,
                                  logic == v::Logic::One || logic == v::Logic::X);
                        EXPECT_EQ((projected[at / 64].b >> (at % 64)) & 1,
                                  logic == v::Logic::Z || logic == v::Logic::X);
                    }
                    if (width % 8 != 0) {
                        EXPECT_EQ(packed[packed.size() - 2].a >> (8 * (width % 8)), 0U);
                        EXPECT_EQ(packed[packed.size() - 2].b >> (8 * (width % 8)), 0U);
                    }
                    if (width % 64 != 0) {
                        EXPECT_EQ(projected[projected.size() - 2].a >> (width % 64), 0U);
                        EXPECT_EQ(projected[projected.size() - 2].b >> (width % 64), 0U);
                    }
                    EXPECT_EQ(packed.back().a, ~0ULL);
                    EXPECT_EQ(packed.back().b, ~0ULL);
                    EXPECT_EQ(projected.back().a, ~0ULL);
                    EXPECT_EQ(projected.back().b, ~0ULL);
                }
}

TEST(StrengthValue, VectorMosKeepsAllIntervalsAndIndependentControlChoices) {
    std::vector<k::StrengthRange> intervals;
    for (int lo = -7; lo <= 7; ++lo)
        for (int hi = lo; hi <= 7; ++hi)
            intervals.push_back({static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)});
    ASSERT_EQ(intervals.size(), 120U);
    unsigned comparisons = 0;
    for (unsigned kind = 0; kind < 6; ++kind)
        for (unsigned first = 0; first < 4; ++first)
            for (unsigned second = 0; second < 4; ++second) {
                // 129 puts all 120 intervals through word boundaries and
                // includes a partial tail, with distinct repeated inputs.
                std::vector<k::W4> input(k::strengthWordCount(129)), out(input.size() + 1);
                out.back() = {79, 83};
                for (unsigned at = 0; at < 129; ++at)
                    ASSERT_TRUE(k::writeStrengthRange(input, at, intervals[at % 120]));
                ASSERT_TRUE(k::mosStrengthValue(out, 129, static_cast<k::MosKind>(kind), input,
                                                bits[first], bits[second]));
                for (unsigned at = 0; at < 129; ++at) {
                    const auto r = intervals[at % 120];
                    const auto expected =
                        v::mosStrength(static_cast<v::MosKind>(kind), {r.lo, r.hi},
                                       static_cast<v::Logic>(first), static_cast<v::Logic>(second));
                    if (!expected.has_value()) {
                        FAIL() << "missing validated value";
                    }
                    const auto actual = k::strengthRangeAt(out, at);
                    if (!actual.has_value()) {
                        FAIL() << "missing validated value";
                    }
                    EXPECT_EQ(actual->lo, expected->lo);
                    EXPECT_EQ(actual->hi, expected->hi);
                    ++comparisons;
                }
                EXPECT_EQ(out[out.size() - 2].a >> 8, 0U);
                EXPECT_EQ(out[out.size() - 2].b >> 8, 0U);
                EXPECT_EQ(out.back().a, 79U);
                EXPECT_EQ(out.back().b, 83U);
            }
    EXPECT_EQ(comparisons, 12384U);
}

TEST(StrengthValue, MalformedEndpointsAndShapesDoNotPartiallyWriteOutput) {
    const auto untouched = [](const auto& out) {
        for (const auto& word : out) {
            EXPECT_EQ(word.a, 71U);
            EXPECT_EQ(word.b, 73U);
        }
    };
    std::array<k::W4, 2> out{{{71, 73}, {71, 73}}};
    const std::array<k::W4, 1> data{{{1, 0}}};
    EXPECT_FALSE(k::makeStrengthValue(out, 0, data, 6, 6));
    EXPECT_FALSE(k::makeStrengthValue(out, 65, data, 6, 6));
    EXPECT_FALSE(k::makeStrengthValue(out, 1, data, 8, 6));
    EXPECT_FALSE(k::makeStrengthValue(out, 1, data, 6, UINT32_MAX));
    EXPECT_FALSE(k::makeStrengthValue({}, 1, data, 6, 6));
    EXPECT_FALSE(k::makeStrengthValue(out, 9, {}, 6, 6));
    std::array<k::W4, 2> data2{};
    std::array<k::W4, 5> bigOut{};
    bigOut.fill({71, 73});
    EXPECT_FALSE(k::makeStrengthValue(bigOut, 33, data2, 6, 6, data));
    untouched(bigOut);
    EXPECT_FALSE(k::strengthRangeAt(data, 8).has_value());
    EXPECT_FALSE(k::writeStrengthRange(out, 16, {0, 0}));
    for (const k::StrengthRange range : {k::StrengthRange{-8, 0}, {0, 8}, {7, -7}})
        EXPECT_FALSE(k::writeStrengthRange(out, 0, range));
    untouched(out);
    for (const auto invalid : {std::uint64_t{8}, std::uint64_t{248}, std::uint64_t{1}}) {
        // Invalid data is in the final bit; earlier output must stay intact.
        std::array<k::W4, 2> input{};
        input[1] = {invalid, 0};
        EXPECT_FALSE(k::projectStrengthValue(out, 9, input));
        EXPECT_FALSE(k::mosStrengthValue(out, 9, k::MosKind::Nmos, input, bits[0]));
        untouched(out);
    }
    std::array<k::W4, 2> valid{};
    const std::array<k::W4, 2> badHigh{{{}, {0, 8}}};
    EXPECT_FALSE(k::projectStrengthValue(out, 9, badHigh));
    EXPECT_FALSE(k::mosStrengthValue(out, 9, k::MosKind::Nmos, badHigh, bits[0]));
    EXPECT_FALSE(k::mosStrengthValue(out, 9, static_cast<k::MosKind>(255), valid, bits[1]));
    EXPECT_FALSE(k::projectStrengthValue({}, 9, valid));
    EXPECT_FALSE(k::mosStrengthValue({}, 9, k::MosKind::Nmos, valid, bits[1]));
    untouched(out);
}
}  // namespace
