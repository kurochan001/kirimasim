// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original point/control enumeration and worked examples for the approved
// MOS/switch contract 2; IEEE 1800-2023 28.7/28.9/28.13/28.14.
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/mos.h"
#include "kirimasim/valsem/mos.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
constexpr std::array<k::W4, 4> controls{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
constexpr std::array<v::Logic, 4> logic{v::Logic::Zero, v::Logic::One, v::Logic::Z, v::Logic::X};
constexpr std::array<k::MosKind, 6> kinds{k::MosKind::Nmos,  k::MosKind::Pmos, k::MosKind::Rnmos,
                                          k::MosKind::Rpmos, k::MosKind::Cmos, k::MosKind::Rcmos};
v::MosKind referenceKind(k::MosKind kind) { return static_cast<v::MosKind>(kind); }
v::StrengthTransfer referenceKind(k::StrengthTransfer kind) {
    return static_cast<v::StrengthTransfer>(kind);
}
void expectWord(k::W4 actual, k::W4 expected) {
    EXPECT_EQ(actual.a, expected.a);
    EXPECT_EQ(actual.b, expected.b);
}

void agrees(k::StrengthRange input, k::MosKind kind, unsigned first, unsigned second) {
    const auto expected =
        v::mosStrength(referenceKind(kind), {input.lo, input.hi}, logic[first], logic[second]);
    const auto actual = k::mosStrength(kind, input, controls[first], controls[second]);
    if (!expected.has_value()) {
        FAIL() << "missing valid expected result";
    }
    if (!actual.has_value()) {
        FAIL() << "missing valid actual result";
    }
    EXPECT_EQ(actual->lo, expected->lo);
    EXPECT_EQ(actual->hi, expected->hi);
    EXPECT_LE(actual->lo, actual->hi);
}
void example(k::MosKind kind, k::StrengthRange data, unsigned a, unsigned b,
             k::StrengthRange expected) {
    const auto fast = k::mosStrength(kind, data, controls[a], controls[b]);
    const auto slow = v::mosStrength(referenceKind(kind), {data.lo, data.hi}, logic[a], logic[b]);
    if (!fast.has_value()) {
        FAIL() << "missing valid fast result";
    }
    if (!slow.has_value()) {
        FAIL() << "missing valid slow result";
    }
    EXPECT_EQ(*fast, expected);
    EXPECT_EQ(*slow, (v::Signal{expected.lo, expected.hi}));
}

TEST(MosStrength, PointReductionsMatchTheApprovedTablesIncludingZeroAndSigns) {
    constexpr std::array<int, 8> identity{0, 1, 2, 3, 4, 5, 6, 7};
    constexpr std::array<int, 8> nonresistive{0, 1, 2, 3, 4, 5, 6, 6};
    constexpr std::array<int, 8> resistive{0, 1, 1, 2, 2, 3, 5, 5};
    unsigned cases = 0;
    for (const auto kind : {k::StrengthTransfer::Identity, k::StrengthTransfer::Nonresistive,
                            k::StrengthTransfer::Resistive}) {
        const auto& expected = kind == k::StrengthTransfer::Identity       ? identity
                               : kind == k::StrengthTransfer::Nonresistive ? nonresistive
                                                                           : resistive;
        for (int rank = 0; rank <= 7; ++rank)
            for (const int sign : {-1, 1}) {
                const auto fast = k::transferStrengthPoint(kind, rank * sign);
                const auto slow =
                    v::transferStrength(referenceKind(kind), {rank * sign, rank * sign});
                if (!fast.has_value()) {
                    FAIL() << "missing valid fast result";
                }
                if (!slow.has_value()) {
                    FAIL() << "missing valid slow result";
                }
                EXPECT_EQ(*fast, expected[static_cast<unsigned>(rank)] * sign);
                EXPECT_EQ(slow->lo, expected[static_cast<unsigned>(rank)] * sign);
                EXPECT_EQ(slow->hi, expected[static_cast<unsigned>(rank)] * sign);
                ++cases;
            }
    }
    EXPECT_EQ(cases, 48U);
}

TEST(MosStrength, EveryIntervalMatchesTheIndependentPointTransferEnumerator) {
    unsigned cases = 0;
    for (const auto kind : {k::StrengthTransfer::Identity, k::StrengthTransfer::Nonresistive,
                            k::StrengthTransfer::Resistive})
        for (int lo = -7; lo <= 7; ++lo)
            for (int hi = lo; hi <= 7; ++hi) {
                const auto fast = k::transferStrength(
                    kind, {static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)});
                const auto slow = v::transferStrength(referenceKind(kind), {lo, hi});
                if (!fast.has_value()) {
                    FAIL() << "missing valid fast result";
                }
                if (!slow.has_value()) {
                    FAIL() << "missing valid slow result";
                }
                EXPECT_EQ(fast->lo, slow->lo);
                EXPECT_EQ(fast->hi, slow->hi);
                ++cases;
            }
    EXPECT_EQ(cases, 360U);
}

TEST(MosStrength, EveryDeviceIntervalAndControlPairMatchesEnumeratedReference) {
    unsigned cases = 0;
    for (const auto kind : kinds)
        for (int lo = -7; lo <= 7; ++lo)
            for (int hi = lo; hi <= 7; ++hi)
                for (unsigned a = 0; a < controls.size(); ++a)
                    for (unsigned b = 0; b < controls.size(); ++b) {
                        SCOPED_TRACE(::testing::Message()
                                     << static_cast<unsigned>(kind) << '/' << lo << '/' << hi << '/'
                                     << a << '/' << b);
                        agrees({static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)}, kind,
                               a, b);
                        ++cases;
                    }
    EXPECT_EQ(cases, 11520U);
}

TEST(MosStrength, CmosParallelLegsApplyOneReductionAndADefiniteLegDominates) {
    for (const auto kind : {k::MosKind::Cmos, k::MosKind::Rcmos}) {
        const std::int8_t rank = kind == k::MosKind::Cmos ? 6 : 5;
        example(kind, {7, 7}, 1, 0, {rank, rank});
        example(kind, {7, 7}, 1, 2, {rank, rank});
        example(kind, {7, 7}, 1, 3, {rank, rank});
        example(kind, {7, 7}, 2, 0, {rank, rank});
        example(kind, {7, 7}, 3, 0, {rank, rank});
        example(kind, {7, 7}, 0, 1, {0, 0});
        example(kind, {7, 7}, 2, 1, {0, rank});
        // Two unknown controls are independent choices; no Boolean
        // correlation is inferred from equal X/Z encodings.
        example(kind, {7, 7}, 3, 3, {0, rank});
        example(kind, {7, 7}, 2, 2, {0, rank});
    }
}

TEST(MosStrength, NAndPLegsHaveOppositeKnownEnablePolarities) {
    example(k::MosKind::Nmos, {7, 7}, 0, 0, {0, 0});
    example(k::MosKind::Nmos, {7, 7}, 1, 0, {6, 6});
    example(k::MosKind::Pmos, {-7, -7}, 0, 0, {-6, -6});
    example(k::MosKind::Pmos, {-7, -7}, 1, 0, {0, 0});
    example(k::MosKind::Rnmos, {7, 7}, 0, 0, {0, 0});
    example(k::MosKind::Rnmos, {7, 7}, 1, 0, {5, 5});
    example(k::MosKind::Rpmos, {-7, -7}, 0, 0, {-5, -5});
    example(k::MosKind::Rpmos, {-7, -7}, 1, 0, {0, 0});
}

TEST(MosStrength, UnknownControlsPreserveHiZAndBothEndsOfAmbiguousData) {
    for (const auto kind : kinds)
        for (unsigned a = 0; a < 4; ++a)
            for (unsigned b = 0; b < 4; ++b)
                example(kind, {0, 0}, a, b, {0, 0});
    for (const unsigned control : {2U, 3U}) {
        example(k::MosKind::Nmos, {3, 6}, control, 0, {0, 6});
        example(k::MosKind::Pmos, {-6, -3}, control, 0, {-6, 0});
        example(k::MosKind::Rnmos, {3, 6}, control, 0, {0, 5});
        example(k::MosKind::Rpmos, {-6, -3}, control, 0, {-5, 0});
        example(k::MosKind::Cmos, {-7, 4}, control, 1, {-6, 4});
        example(k::MosKind::Rcmos, {-7, 4}, control, 1, {-5, 2});
    }
}

TEST(MosStrength, ChainedTransferPreservesSignedRanksAndIdentityPortEdges) {
    constexpr std::array<int, 7> ranks{7, 5, 3, 2, 1, 1, 1};
    for (const int sign : {-1, 1}) {
        auto point = static_cast<std::int8_t>(sign * ranks[0]);
        for (unsigned hop = 1; hop < ranks.size(); ++hop) {
            const auto same = k::transferStrength(k::StrengthTransfer::Identity, {point, point});
            if (!same.has_value()) {
                FAIL() << "missing valid same result";
            }
            EXPECT_EQ(*same, (k::StrengthRange{point, point}));
            const auto reduced = k::transferStrength(k::StrengthTransfer::Resistive, *same);
            if (!reduced.has_value()) {
                FAIL() << "missing valid reduced result";
            }
            point = static_cast<std::int8_t>(sign * ranks[hop]);
            EXPECT_EQ(*reduced, (k::StrengthRange{point, point}));
        }
        const auto nonres = k::transferStrengthPoint(k::StrengthTransfer::Nonresistive, sign * 7);
        if (!nonres.has_value()) {
            FAIL() << "missing valid nonres result";
        }
        EXPECT_EQ(*nonres, sign * 6);
        EXPECT_EQ(k::transferStrengthPoint(k::StrengthTransfer::Nonresistive, *nonres), nonres);
    }
}

TEST(MosStrength, FullIntervalsRetainOppositionAndStrengthOnlyDifferences) {
    const auto interval = k::mosStrength(k::MosKind::Nmos, {3, 6}, {1, 0});
    if (!interval.has_value()) {
        FAIL() << "missing valid interval result";
    }
    EXPECT_EQ(*interval, (k::StrengthRange{3, 6}));
    expectWord(k::strengthValue(*interval), (k::W4{1, 0}));
    const k::StrengthRange drivers[]{*interval, {-5, -5}};
    const auto resolved = k::resolveStrengths(k::NetKind::Wire, drivers, 2);
    EXPECT_EQ(resolved, (k::StrengthRange{-5, 6}));
    EXPECT_EQ(v::resolveSignals(v::NetKind::Wire, {{3, 6}, {-5, -5}}), (v::Signal{-5, 6}));
    expectWord(k::strengthValue(resolved), (k::W4{1, 1}));
    const auto weak = k::mosStrength(k::MosKind::Nmos, {3, 3}, {1, 0});
    const auto strong = k::mosStrength(k::MosKind::Nmos, {6, 6}, {1, 0});
    if (!weak.has_value()) {
        FAIL() << "missing valid weak result";
    }
    if (!strong.has_value()) {
        FAIL() << "missing valid strong result";
    }
    expectWord(k::strengthValue(*weak), k::strengthValue(*strong));
    const k::StrengthRange first[]{*weak, {-5, -5}}, second[]{*strong, {-5, -5}};
    expectWord(k::strengthValue(k::resolveStrengths(k::NetKind::Wire, first, 2)), (k::W4{0, 0}));
    expectWord(k::strengthValue(k::resolveStrengths(k::NetKind::Wire, second, 2)), (k::W4{1, 0}));
}

TEST(MosStrength, InvalidKindsAndRangesAreRejectedEvenWithDisabledControls) {
    const auto badTransfer = static_cast<k::StrengthTransfer>(255);
    const auto badReferenceTransfer = static_cast<v::StrengthTransfer>(255);
    EXPECT_FALSE(k::transferStrength(badTransfer, {0, 0}));
    EXPECT_FALSE(v::transferStrength(badReferenceTransfer, {0, 0}));
    for (const int point :
         {std::numeric_limits<int>::min(), -128, -8, 8, 127, std::numeric_limits<int>::max()})
        EXPECT_FALSE(k::transferStrengthPoint(k::StrengthTransfer::Resistive, point));
    for (const k::StrengthRange invalid : {k::StrengthRange{-8, 0}, {-7, 8}, {3, -3}, {127, 127}}) {
        EXPECT_FALSE(k::transferStrength(k::StrengthTransfer::Identity, invalid));
        EXPECT_FALSE(v::transferStrength(v::StrengthTransfer::Identity, {invalid.lo, invalid.hi}));
        for (const auto kind : kinds)
            for (const auto control : controls) {
                EXPECT_FALSE(k::mosStrength(kind, invalid, control, {1, 0}));
                EXPECT_FALSE(v::mosStrength(
                    referenceKind(kind), {invalid.lo, invalid.hi},
                    control.b == 0 ? (control.a ? v::Logic::One : v::Logic::Zero) : v::Logic::X,
                    v::Logic::One));
            }
    }
    EXPECT_FALSE(k::mosStrength(static_cast<k::MosKind>(255), {0, 0}, {0, 0}));
    EXPECT_FALSE(v::mosStrength(static_cast<v::MosKind>(255), {0, 0}, v::Logic::Zero));
}

TEST(MosStrength, ScalarControlTransportIgnoresUnusedWordBits) {
    for (const auto kind : kinds)
        for (const auto first : controls)
            for (const auto second : controls) {
                const auto baseline = k::mosStrength(kind, {-7, 6}, first, second);
                const auto noisy = k::mosStrength(kind, {-7, 6}, {first.a | ~1ULL, first.b | ~1ULL},
                                                  {second.a | ~1ULL, second.b | ~1ULL});
                if (!baseline.has_value()) {
                    FAIL() << "missing valid baseline result";
                }
                if (!noisy.has_value()) {
                    FAIL() << "missing valid noisy result";
                }
                EXPECT_EQ(noisy, baseline);
            }
}
}  // namespace
