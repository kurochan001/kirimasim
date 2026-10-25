// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Storage must cover every declared bit before allocation (SIR_SPEC 3.2/4.1,
// RUNTIME_ABI 5). Original arithmetic boundary checks; no giant allocation.
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/wide.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

TEST(StorageWidth, CeilingWordCountsRemainValidAtTheFullUnsignedWidth) {
    struct Case {
        std::uint32_t bits, words;
    };
    constexpr std::array<Case, 10> cases{{{0, 0},
                                          {1, 1},
                                          {63, 1},
                                          {64, 1},
                                          {65, 2},
                                          {129, 3},
                                          {UINT32_MAX - 64, 67108863},
                                          {UINT32_MAX - 63, 67108863},
                                          {UINT32_MAX - 62, 67108864},
                                          {UINT32_MAX, 67108864}}};
    for (const auto c : cases) {
        const auto words = kirimasim::kernel::wordsFor(c.bits);
        EXPECT_EQ(words, c.words) << c.bits;
        EXPECT_EQ(kirimasim::kernel::wide::chunks(c.bits), c.words);
        EXPECT_EQ(kirimasim::sir::integralWords(c.bits), c.words);
        EXPECT_GE(std::uint64_t{words} * 64, c.bits);
        if (c.bits != 0) {
            ASSERT_NE(words, 0U);
            EXPECT_LT((std::uint64_t{words} - 1) * 64, c.bits);
        }
    }
}

TEST(StorageWidth, ByteCountsCoverUnalignedAndMaximumWidthsWithoutWrapping) {
    struct Case {
        std::uint32_t bits, bytes;
    };
    constexpr std::array<Case, 12> cases{{{0, 0},
                                          {1, 1},
                                          {7, 1},
                                          {8, 1},
                                          {9, 2},
                                          {63, 8},
                                          {64, 8},
                                          {65, 9},
                                          {UINT32_MAX - 8, 536870911},
                                          {UINT32_MAX - 7, 536870911},
                                          {UINT32_MAX - 6, 536870912},
                                          {UINT32_MAX, 536870912}}};
    for (const auto c : cases) {
        const auto bytes = kirimasim::kernel::bytesFor(c.bits);
        EXPECT_EQ(bytes, c.bytes);
        EXPECT_EQ(kirimasim::sir::integralBytes(c.bits), c.bytes);
        EXPECT_GE(std::uint64_t{bytes} * 8, c.bits);
        if (c.bits != 0) {
            ASSERT_NE(bytes, 0U);
            EXPECT_LT((std::uint64_t{bytes} - 1) * 8, c.bits);
        }
    }
}

TEST(StorageWidth, TruncatedHugeConstantsAreRejectedWithoutAllocatingTheirDeclaredWidth) {
    namespace s = kirimasim::sir;
    for (const auto kind : {s::TypeKind::U2, s::TypeKind::S2, s::TypeKind::U4, s::TypeKind::S4})
        for (const auto width : {UINT32_MAX - 62, UINT32_MAX}) {
            s::Design d;
            d.sources = {"storage_width.sv"};
            d.instances = {{"top", "top", s::kNone}};
            d.constants.push_back({{kind, width}, {}, {}});
            const auto errors = s::verify(d);
            EXPECT_TRUE(std::any_of(errors.begin(), errors.end(), [](const auto& error) {
                return error.where == "const #0" && error.message == "wrong number of words";
            })) << width;
        }
}
