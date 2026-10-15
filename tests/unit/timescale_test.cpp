// Tests for time units and delay conversion (SCHEDULER_SPEC.md 1).

#include <cmath>
#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

#include "haremasim/sir/timescale.h"

namespace {

using namespace haremasim::sir;

TEST(Timescale, ParseUnits) {
    EXPECT_EQ(parseTimeExponent("1ns"), -9);
    EXPECT_EQ(parseTimeExponent("10ps"), -11);
    EXPECT_EQ(parseTimeExponent("100 us"), -4);
    EXPECT_EQ(parseTimeExponent("1s"), 0);
    EXPECT_EQ(parseTimeExponent("100s"), 2);
    EXPECT_EQ(parseTimeExponent("1fs"), -15);
    EXPECT_EQ(parseTimeExponent("1000ns"), std::nullopt);
    EXPECT_EQ(parseTimeExponent("2ns"), std::nullopt);
    EXPECT_EQ(parseTimeExponent("1 xs"), std::nullopt);
    EXPECT_EQ(parseTimeExponent(""), std::nullopt);
}

TEST(Timescale, UnitText) {
    EXPECT_EQ(timeExponentText(-9), "1ns");
    EXPECT_EQ(timeExponentText(-11), "10ps");
    EXPECT_EQ(timeExponentText(-13), "100fs");
    EXPECT_EQ(timeExponentText(-15), "1fs");
    EXPECT_EQ(timeExponentText(2), "100s");
    for (int e = kMinTimeExponent; e <= kMaxTimeExponent; ++e)
        EXPECT_EQ(parseTimeExponent(timeExponentText(e)), e);
}

TEST(Timescale, IntegerDelays) {
    EXPECT_EQ(delayTicks(5, {-9, -9}, -12), 5000U);
    EXPECT_EQ(delayTicks(5, {-9, -12}, -12), 5000U);
    EXPECT_EQ(delayTicks(0, {2, 2}, -15), 0U);
    EXPECT_EQ(delayTicks(7, {-12, -12}, -12), 7U);
    // 100 s in femtoseconds is 10^17; 10^3 of them overflow 64 bits.
    EXPECT_EQ(delayTicks(1, {2, 2}, -15), 100'000'000'000'000'000U);
    EXPECT_EQ(delayTicks(1000, {2, 2}, -15), std::nullopt);
}

TEST(Timescale, RealDelaysRoundToThePrecision) {
    // Unit 1ns, precision 100ps: 2.75ns is 2.8ns (3.14.1).
    EXPECT_EQ(delayTicksReal(2.75, {-9, -10}, -12), 2800U);
    // Rounded as written: 1.15 -> 1.2 (its binary value is just below).
    EXPECT_EQ(delayTicksReal(1.15, {-9, -10}, -10), 12U);
    // Precision equal to the unit rounds to whole units, half away from 0.
    EXPECT_EQ(delayTicksReal(0.25, {-9, -9}, -9), 0U);
    EXPECT_EQ(delayTicksReal(0.5, {-9, -9}, -9), 1U);
    EXPECT_EQ(delayTicksReal(1.5, {-9, -9}, -12), 2000U);
    EXPECT_EQ(delayTicksReal(2.5, {-9, -9}, -9), 3U);
    EXPECT_EQ(delayTicksReal(2.4999, {-9, -9}, -9), 2U);
    EXPECT_EQ(delayTicksReal(0.0, {-9, -9}, -9), 0U);
    EXPECT_EQ(delayTicksReal(1e-30, {-9, -9}, -9), 0U);
    EXPECT_EQ(delayTicksReal(123.0, {-9, -9}, -9), 123U);
    EXPECT_EQ(delayTicksReal(1.5e3, {-6, -9}, -12), 1'500'000'000U);
}

TEST(Timescale, RealDelaysOutOfRange) {
    EXPECT_EQ(delayTicksReal(-1.0, {-9, -9}, -9), std::nullopt);
    EXPECT_EQ(delayTicksReal(NAN, {-9, -9}, -9), std::nullopt);
    EXPECT_EQ(delayTicksReal(INFINITY, {-9, -9}, -9), std::nullopt);
    EXPECT_EQ(delayTicksReal(1e30, {-9, -9}, -9), std::nullopt);
}

}  // namespace
