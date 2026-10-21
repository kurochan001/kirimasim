// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Net resolution with strengths (VALUE_SEMANTICS.md 5): the valsem
// reference on cases worked out from 28.12, and the kernel's fast version
// (kernel/strength.h) compared with the reference.

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/strength.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/valsem/strength.h"

namespace {

namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using v::Logic;
using v::NetKind;
using v::Signal;

constexpr Logic L0 = Logic::Zero;
constexpr Logic L1 = Logic::One;
constexpr Logic LX = Logic::X;
constexpr Logic LZ = Logic::Z;

Signal resolved(NetKind kind, const std::vector<v::Driver>& ds) { return v::resolve(kind, ds); }

// The strengths: 7 supply, 6 strong, 5 pull, 3 weak, 0 highz (table 28-7).
TEST(Strength, StrongerWins) {
    // A pull 1 and a strong 0 give the strong 0 (28.12.1).
    EXPECT_EQ(resolved(NetKind::Wire, {{L1, 5, 5}, {L0, 6, 6}}), (Signal{-6, -6}));
    // Like values keep the greater strength.
    EXPECT_EQ(resolved(NetKind::Wire, {{L1, 3, 3}, {L1, 6, 6}}), (Signal{6, 6}));
    // z gives way to anything.
    EXPECT_EQ(resolved(NetKind::Wire, {{LZ, 6, 6}, {L0, 3, 3}}), (Signal{-3, -3}));
    EXPECT_EQ(resolved(NetKind::Wire, {{LZ, 6, 6}, {LZ, 6, 6}}), (Signal{0, 0}));
}

TEST(Strength, EqualStrengthsOppositeValues) {
    // Equal and opposite: x with both strengths and every weaker one (28.12.2).
    EXPECT_EQ(resolved(NetKind::Wire, {{L1, 3, 3}, {L0, 3, 3}}), (Signal{-3, 3}));
    EXPECT_EQ(v::valueOf(Signal{-3, 3}), LX);
}

TEST(Strength, AmbiguousSignals) {
    // An x driven with highz for 0 is H, with highz for 1 is L (28.12.2).
    EXPECT_EQ(v::driverSignal({LX, 6, 0}), (Signal{0, 6}));
    EXPECT_EQ(v::driverSignal({LX, 0, 6}), (Signal{-6, 0}));
    // A pull H and a weak L: the range from weak 0 to pull 1 (28.12.2).
    EXPECT_EQ(resolved(NetKind::Wire, {{LX, 5, 0}, {LX, 0, 3}}), (Signal{-3, 5}));
    // A strong H and a weak 0 (28.12.2, 28.12.3).
    EXPECT_EQ(resolved(NetKind::Wire, {{LX, 6, 0}, {L0, 3, 3}}), (Signal{-3, 6}));
    // A strong L and a strong 1: the strong 0 of the L meets the strong 1
    // at equal strength, so x (5.4; Icarus 13.0 gives StX too).
    EXPECT_EQ(resolved(NetKind::Wire, {{LX, 0, 6}, {L1, 6, 6}}), (Signal{-6, 6}));
    // A strong L and a pull 1: the pull 1 beats the weaker part of the L,
    // the strong 0 remains, and the gap between is filled (28.12.3).
    EXPECT_EQ(resolved(NetKind::Wire, {{LX, 0, 6}, {L1, 5, 5}}), (Signal{-6, 5}));
    // L and H read as x (5.2).
    EXPECT_EQ(v::valueOf(Signal{0, 6}), LX);
    EXPECT_EQ(v::valueOf(Signal{-6, 0}), LX);
}

TEST(Strength, WiredLogic) {
    // Equal strengths: the logic function (28.12.4).
    EXPECT_EQ(resolved(NetKind::Wand, {{L0, 5, 5}, {L1, 5, 5}}), (Signal{-5, -5}));
    EXPECT_EQ(resolved(NetKind::Wor, {{L0, 5, 5}, {L1, 5, 5}}), (Signal{5, 5}));
    EXPECT_EQ(resolved(NetKind::Triand, {{L0, 6, 6}, {L1, 6, 6}}), (Signal{-6, -6}));
    EXPECT_EQ(resolved(NetKind::Trior, {{L0, 6, 6}, {L1, 6, 6}}), (Signal{6, 6}));
    // Unequal strengths: the stronger wins (28.12.4, figure 28-25).
    EXPECT_EQ(resolved(NetKind::Wor, {{L0, 6, 6}, {L1, 5, 5}}), (Signal{-6, -6}));
    EXPECT_EQ(resolved(NetKind::Wand, {{L0, 3, 3}, {L1, 6, 6}}), (Signal{6, 6}));
    // x on a wired net still makes x where it can win.
    EXPECT_EQ(v::valueOf(resolved(NetKind::Wand, {{LX, 6, 6}, {L1, 6, 6}})), LX);
    EXPECT_EQ(v::valueOf(resolved(NetKind::Wand, {{LX, 6, 6}, {L0, 6, 6}})), L0);
}

TEST(Strength, OwnDrivers) {
    // tri0 and tri1 pull; a weak driver loses, a strong one wins (28.15.1).
    EXPECT_EQ(resolved(NetKind::Tri0, {}), (Signal{-5, -5}));
    EXPECT_EQ(resolved(NetKind::Tri0, {{L1, 3, 3}}), (Signal{-5, -5}));
    EXPECT_EQ(resolved(NetKind::Tri1, {{L0, 6, 6}}), (Signal{-6, -6}));
    EXPECT_EQ(resolved(NetKind::Tri1, {{LZ, 6, 6}}), (Signal{5, 5}));
    EXPECT_EQ(v::valueOf(resolved(NetKind::Tri0, {{L1, 6, 6}, {L0, 6, 6}})), LX);
    // supply0 and supply1 drive with supply strength (28.15.3).
    EXPECT_EQ(resolved(NetKind::Supply0, {{L1, 6, 6}}), (Signal{-7, -7}));
    EXPECT_EQ(resolved(NetKind::Supply1, {{L0, 7, 7}}), (Signal{-7, 7}));
}

TEST(Strength, ValueOf) {
    EXPECT_EQ(v::valueOf(Signal{0, 0}), LZ);
    EXPECT_EQ(v::valueOf(Signal{-6, -3}), L0);
    EXPECT_EQ(v::valueOf(Signal{3, 6}), L1);
}

k::NetKind kernelKind(NetKind n) { return static_cast<k::NetKind>(n); }

v::Value ref(k::W4 x, unsigned w) { return v::fromWords({x.a}, {x.b}, w, false); }

/// The kernel and the reference on the given drivers of a net of width w.
::testing::AssertionResult same(NetKind kind, const std::vector<k::StrengthDriver>& ds,
                                unsigned w) {
    std::vector<v::Value> values;
    std::vector<int> s1;
    std::vector<int> s0;
    std::string what = "kind " + std::to_string(static_cast<int>(kind)) + " w=" + std::to_string(w);
    for (const k::StrengthDriver& d : ds) {
        values.push_back(ref(d.value, w));
        s1.push_back(d.s1);
        s0.push_back(d.s0);
        what += " " + v::toBinary(values.back()) + "(" + std::to_string(d.s1) + "," +
                std::to_string(d.s0) + ")";
    }
    const v::Value slow = v::resolveNet(kind, w, values, s1, s0);
    const v::Value fast =
        ref(k::resolve4(kernelKind(kind), ds.data(), static_cast<unsigned>(ds.size()), w), w);
    if (fast == slow) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << what << ": fast " << v::toBinary(fast) << " reference " << v::toBinary(slow);
}

constexpr NetKind kKinds[] = {NetKind::Wire,    NetKind::Tri,     NetKind::Wand, NetKind::Triand,
                              NetKind::Wor,     NetKind::Trior,   NetKind::Tri0, NetKind::Tri1,
                              NetKind::Supply0, NetKind::Supply1, NetKind::Uwire};

// Every value of two one-bit drivers with the driving strengths and highz.
TEST(Strength, KernelMatchesReferenceOnTwoDrivers) {
    constexpr std::uint8_t kStrengths[] = {0, 3, 5, 6, 7};
    const k::W4 values[] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (NetKind kind : kKinds)
        for (k::W4 x : values)
            for (k::W4 y : values)
                for (std::uint8_t a1 : kStrengths)
                    for (std::uint8_t a0 : kStrengths)
                        for (std::uint8_t b1 : kStrengths)
                            for (std::uint8_t b0 : kStrengths)
                                ASSERT_TRUE(same(kind, {{x, a1, a0}, {y, b1, b0}}, 1));
}

TEST(Strength, KernelMatchesReferenceOnRandomNets) {
    const std::uint64_t seed = kirimasim::test::fuzzSeed(0x737472656e67ULL);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    std::mt19937_64 rng{seed};
    const std::uint64_t rounds = 2'000 * kirimasim::test::fuzzScale();
    for (std::uint64_t n = 0; n < rounds; ++n) {
        const NetKind kind = kKinds[rng() % std::size(kKinds)];
        const auto w = static_cast<unsigned>(1 + rng() % 16);
        const auto count = static_cast<unsigned>(rng() % 4);
        std::vector<k::StrengthDriver> ds;
        for (unsigned d = 0; d < count; ++d) {
            const std::uint64_t m = k::mask(w);
            ds.push_back({{rng() & m, rng() & rng() & m},
                          static_cast<std::uint8_t>(rng() % 8),
                          static_cast<std::uint8_t>(rng() % 8)});
        }
        ASSERT_TRUE(same(kind, ds, w));
    }
}

// Drivers with side slots (SIR_SPEC.md 3.5): a bit that does not drive a
// side has highz strength on it, so each bit is the reference with that
// strength set to 0.
TEST(Strength, KernelMatchesReferenceWithSides) {
    const std::uint64_t seed = kirimasim::test::fuzzSeed(0x7369646573ULL);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    std::mt19937_64 rng{seed};
    const std::uint64_t rounds = 2'000 * kirimasim::test::fuzzScale();
    for (std::uint64_t n = 0; n < rounds; ++n) {
        const NetKind kind = kKinds[rng() % std::size(kKinds)];
        const auto w = static_cast<unsigned>(1 + rng() % 16);
        const auto count = static_cast<unsigned>(rng() % 4);
        const std::uint64_t m = k::mask(w);
        std::vector<k::StrengthDriver> ds;
        ds.reserve(count);
        for (unsigned d = 0; d < count; ++d)
            ds.push_back({{rng() & m, rng() & rng() & m},
                          static_cast<std::uint8_t>(rng() % 8),
                          static_cast<std::uint8_t>(rng() % 8),
                          rng() & m,
                          rng() & m});
        const k::W4 fast = k::resolve4(kernelKind(kind), ds.data(), count, w);
        for (unsigned bit = 0; bit < w; ++bit) {
            std::vector<v::Driver> bits;
            bits.reserve(ds.size());
            for (const k::StrengthDriver& d : ds) {
                const Logic value =
                    v::fromWords({d.value.a >> bit}, {d.value.b >> bit}, 1, false).bits[0];
                bits.push_back({value, ((d.one >> bit) & 1U) != 0 ? d.s1 : 0,
                                ((d.zero >> bit) & 1U) != 0 ? d.s0 : 0});
            }
            const Logic slow = v::valueOf(v::resolve(kind, bits));
            const Logic got = v::fromWords({fast.a >> bit}, {fast.b >> bit}, 1, false).bits[0];
            ASSERT_EQ(got, slow) << "kind " << static_cast<int>(kind) << " bit " << bit;
        }
    }
}

}  // namespace
