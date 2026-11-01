// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Original table for approved MOS/switch contract 8, IEEE 1800 21.2.1.4.
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/strength_value.h"
#include <array>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

namespace {
using namespace kirimasim::kernel;
TEST(StrengthFormat, EveryCompleteIntervalHasItsOwnToken) {
    // Rows start at lo=-7..7; columns are hi=lo..7. This literal oracle is
    // deliberately independent of the renderer's branches and rank lookup.
    constexpr std::array rows{"Su0 760 750 740 730 720 710 SuL 71X 72X 73X 74X 75X 76X SuX",
                              "St0 650 640 630 620 610 StL 61X 62X 63X 64X 65X StX 67X",
                              "Pu0 540 530 520 510 PuL 51X 52X 53X 54X PuX 56X 57X",
                              "La0 430 420 410 LaL 41X 42X 43X LaX 45X 46X 47X",
                              "We0 320 310 WeL 31X 32X WeX 34X 35X 36X 37X",
                              "Me0 210 MeL 21X MeX 23X 24X 25X 26X 27X",
                              "Sm0 SmL SmX 12X 13X 14X 15X 16X 17X",
                              "HiZ SmH MeH WeH LaH PuH StH SuH",
                              "Sm1 211 311 411 511 611 711",
                              "Me1 321 421 521 621 721",
                              "We1 431 531 631 731",
                              "La1 541 641 741",
                              "Pu1 651 751",
                              "St1 761",
                              "Su1"};
    unsigned count = 0;
    for (int lo = -7; lo <= 7; ++lo) {
        std::istringstream row(rows[lo + 7]);
        for (int hi = lo; hi <= 7; ++hi) {
            std::string expected;
            ASSERT_TRUE(bool(row >> expected));
            std::array<W4, 1> packed{};
            ASSERT_TRUE(writeStrengthRange(
                packed, 0, {static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)}));
            const auto result = formatStrength(packed, 1, {'v', 0});
            EXPECT_TRUE(result.error.empty());
            EXPECT_EQ(result.text, expected) << lo << ',' << hi;
            ++count;
        }
        std::string excess;
        EXPECT_FALSE(bool(row >> excess));
    }
    EXPECT_EQ(count, 120U);
}
TEST(StrengthFormat, VectorOrderWholeFieldAndPackedBoundaries) {
    for (const unsigned width : {1U, 8U, 9U, 65U, 129U}) {
        std::vector<W4> packed(strengthWordCount(width));
        std::string expected;
        for (unsigned bit = 0; bit < width; ++bit) {
            ASSERT_TRUE(writeStrengthRange(
                packed, bit, bit == width - 1 ? StrengthRange{-7, -7} : StrengthRange{3, 6}));
            if (bit) expected += '_';
            expected += bit == 0 ? "Su0" : "631";
        }
        EXPECT_EQ(formatStrength(packed, width, {'v', 0}).text, expected);
        EXPECT_EQ(formatStrength(packed, width, {'v', int(expected.size() + 2)}).text,
                  "  " + expected);
        FormatSpec left{'v', int(expected.size() + 2)};
        left.left = true;
        EXPECT_EQ(formatStrength(packed, width, left).text, expected + "  ");
        FormatSpec zero{'v', 0};
        zero.zero = true;
        EXPECT_EQ(formatStrength(packed, width, zero).text, expected);
    }
}
TEST(StrengthFormat, RuntimeProvenanceDoesNotReplaceRawValues) {
    std::array<W4, 1> packed{};
    ASSERT_TRUE(writeStrengthRange(packed, 0, {3, 6}));
    std::uint64_t words[]{1, 0};
    FormatValue value;
    value.integral = {words, 1, true, false};
    value.strength = packed;
    value.strengthWidth = 1;
    value.hasPattern = true;
    value.namedPattern = "1";
    value.positionalPattern = "1";
    const std::array args{value, value, value};
    const auto result = formatStringCall("%v %b %p", args, "top", "work.top", 1, -9, {});
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.text, "631 1 1");
    value.strength = {};
    EXPECT_EQ(formatStringCall("%v", {&value, 1}, "top", "work.top", 1, -9, {}).text, "St1");
}
TEST(StrengthFormat, InvalidShapesAndIntervalsNeverReturnPartialText) {
    std::array<W4, 1> packed{};
    EXPECT_FALSE(formatStrength(packed, 0, {'v', 0}).error.empty());
    EXPECT_FALSE(formatStrength(packed, 9, {'v', 0}).error.empty());
    EXPECT_FALSE(formatStrength(packed, 1, {'b', 0}).error.empty());
    for (const auto malformed : {W4{8, 8}, W4{7, 3}, W4{128, 0}, W4{0, 127}}) {
        packed[0] = malformed;
        const auto result = formatStrength(packed, 1, {'v', 0});
        EXPECT_FALSE(result.error.empty());
        EXPECT_TRUE(result.text.empty());
    }
    packed[0] = W4{8, 8};
    ASSERT_TRUE(writeStrengthRange(packed, 1, {3, 3}));
    const auto afterValidToken = formatStrength(packed, 2, {'v', 0});
    EXPECT_FALSE(afterValidToken.error.empty());
    EXPECT_TRUE(afterValidToken.text.empty());
}
}  // namespace
