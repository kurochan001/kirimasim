// Tests for time units and delay conversion (SCHEDULER_SPEC.md 1).

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "kirimasim/sir/timescale.h"

namespace {

using namespace kirimasim::sir;

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

TEST(Timescale, MaxTimeText) {
    // CLI.md 2.5: a decimal number and a unit, no space, rounded half away
    // from zero to the global precision.
    EXPECT_EQ(parseTimeTicks("10us", -9), 10'000U);
    EXPECT_EQ(parseTimeTicks("1.5ns", -12), 1'500U);
    EXPECT_EQ(parseTimeTicks("1.5ns", -9), 2U);
    EXPECT_EQ(parseTimeTicks("1.49ns", -9), 1U);
    EXPECT_EQ(parseTimeTicks("0.25ns", -10), 3U);
    EXPECT_EQ(parseTimeTicks("0s", -15), 0U);
    EXPECT_EQ(parseTimeTicks("007ps", -12), 7U);
    EXPECT_EQ(parseTimeTicks("1fs", 2), 0U);
    EXPECT_EQ(parseTimeTicks("18446744073709551615fs", -15), UINT64_MAX);
    EXPECT_EQ(parseTimeTicks("18446744073709551616fs", -15), std::nullopt);
    EXPECT_EQ(parseTimeTicks("100000000000s", -9), std::nullopt);
    for (const char* bad : {"", "10", "ns", "1 ns", "1.ns", ".5ns", "-1ns", "1e3ns", "1nss",
                            "1.5.0ns", "10 us", "+1ns", "1NS"})
        EXPECT_EQ(parseTimeTicks(bad, -12), std::nullopt) << bad;
}

TEST(Timescale, TimeText) {
    // CLI.md 4: a whole number and the largest unit that shows it exactly.
    EXPECT_EQ(timeText(120'000, -12), "120 ns");
    EXPECT_EQ(timeText(1'500, -12), "1500 ps");
    EXPECT_EQ(timeText(1'000, -12), "1 ns");
    EXPECT_EQ(timeText(7, -11), "70 ps");
    EXPECT_EQ(timeText(3, -9), "3 ns");
    EXPECT_EQ(timeText(5, 2), "500 s");
    EXPECT_EQ(timeText(1'000'000, -15), "1 ns");
    EXPECT_EQ(timeText(UINT64_MAX, -15), "18446744073709551615 fs");
    // Time 0 is shown in the unit of the precision.
    EXPECT_EQ(timeText(0, -12), "0 ps");
    EXPECT_EQ(timeText(0, -11), "0 ps");
    EXPECT_EQ(timeText(0, -9), "0 ns");
    EXPECT_EQ(timeText(0, 2), "0 s");
    // The text reads back as the same number of ticks.
    for (const std::uint64_t t : {1ULL, 10ULL, 1234ULL, 1'000'000'000ULL, 999'999'999'999ULL}) {
        std::string text = timeText(t, -12);
        text.erase(text.find(' '), 1);
        EXPECT_EQ(parseTimeTicks(text, -12), t) << text;
    }
}

}  // namespace
