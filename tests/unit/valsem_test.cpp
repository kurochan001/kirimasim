// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Checks the valsem reference against the examples that VALUE_SEMANTICS.md
// records as checked against the standard (and, where noted there, against
// Icarus Verilog 13.0).

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "kirimasim/kernel/strings.h"
#include "kirimasim/valsem/value.h"

namespace {

TEST(StringReal, ScansOnlyTheRealConstantPrefix) {
    struct Case {
        const char* text;
        double value;
    };
    for (const Case c : {Case{"7_2.5", 72.5},
                         {" 4.5", 0},
                         {".75", 0},
                         {"", 0},
                         {"_12", 0},
                         {"+1.5", 1.5},
                         {"-1.5", -1.5},
                         {"nan", 0},
                         {"inf", 0},
                         {"0x1p4", 0},
                         {"2.5e3x", 2500},
                         {"1E-2", 0.01},
                         {"1e+2", 100},
                         {"1e+tail", 1},
                         {"12.", 12},
                         {"9__9", 99},
                         {"1.2_3", 1.23},
                         {"1e1_2", 1e12}}) {
        SCOPED_TRACE(c.text);
        EXPECT_DOUBLE_EQ(kirimasim::valsem::stringToReal(c.text), c.value);
        const auto actual = kirimasim::kernel::stringToReal(c.text);
        ASSERT_TRUE(actual);
        EXPECT_DOUBLE_EQ(actual.value_or(std::numeric_limits<double>::quiet_NaN()), c.value);
    }
}

TEST(StringReal, MatchesTheIndependentReferenceAcrossSeparatorsAndSuffixes) {
    for (int n : {0, 1, 19, 257, 9999})
        for (const char* separator : {"", "_", "__"})
            for (const char* suffix : {"", "tail", "e+2", "E-2", "e+", "_5"}) {
                const std::string text = std::to_string(n) + separator + ".25" + suffix;
                SCOPED_TRACE(text);
                const auto actual = kirimasim::kernel::stringToReal(text);
                ASSERT_TRUE(actual);
                EXPECT_DOUBLE_EQ(actual.value_or(std::numeric_limits<double>::quiet_NaN()),
                                 kirimasim::valsem::stringToReal(text));
            }
}

TEST(StringReal, ReadsOneLeadingSignFollowedByADigit) {  // VALUE_SEMANTICS.md 7.6
    struct Case {
        const char* text;
        double value;
    };
    for (const Case c : {Case{"-2.5", -2.5},
                         {"+2.5", 2.5},
                         {"-7_2.5", -72.5},
                         {"-1e3", -1000},
                         {"+1.5E-2", 0.015},
                         {"-2.5e3x", -2500},
                         {"-.5", 0},
                         {"- 2.5", 0},
                         {"--2.5", 0},
                         {"+-1", 0},
                         {"-_1", 0},
                         {"-", 0},
                         {"+", 0},
                         {" -1", 0},
                         {"-x", 0}}) {
        SCOPED_TRACE(c.text);
        EXPECT_DOUBLE_EQ(kirimasim::valsem::stringToReal(c.text), c.value);
        const auto actual = kirimasim::kernel::stringToReal(c.text);
        ASSERT_TRUE(actual);
        EXPECT_DOUBLE_EQ(actual.value_or(std::numeric_limits<double>::quiet_NaN()), c.value);
    }
    // A minus zero keeps its sign (IEEE 754, 5.7.2).
    EXPECT_TRUE(std::signbit(kirimasim::valsem::stringToReal("-0.0")));
    EXPECT_TRUE(std::signbit(kirimasim::kernel::stringToReal("-0.0").value_or(0.0)));
}

TEST(StringInteger, ReadsOneLeadingMinusThenDigitsAndUnderscores) {  // VALUE_SEMANTICS.md 7.6
    struct Case {
        const char* text;
        unsigned base;
        std::uint32_t value;
    };
    for (const Case c : {Case{"123_45ab", 10, 12345},
                         {"-5", 10, 0xFFFFFFFBU},
                         {"-0", 10, 0},
                         {"+5", 10, 0},
                         {" 5", 10, 0},
                         {"_5", 10, 5},
                         {"-_5", 10, 0xFFFFFFFBU},
                         {"--5", 10, 0},
                         {"-", 10, 0},
                         {"", 10, 0},
                         {"5_0", 10, 50},
                         {"-1fZz", 16, 0xFFFFFFE1U},
                         {"FF", 16, 255},
                         {"-17", 8, 0xFFFFFFF1U},
                         {"178", 8, 15},
                         {"-1012", 2, 0xFFFFFFFBU},
                         {"4294967301", 10, 5},
                         {"-2147483648", 10, 0x80000000U},
                         {"123456789", 16, 0x23456789U}}) {
        SCOPED_TRACE(c.text);
        EXPECT_EQ(kirimasim::valsem::stringToInteger(c.text, c.base), c.value);
        EXPECT_EQ(kirimasim::kernel::stringToInteger(c.text, c.base), c.value);
    }
}

TEST(StringInteger, MatchesTheIndependentReference) {
    for (const unsigned base : {2U, 8U, 10U, 16U})
        for (const char* sign : {"", "-", "+", "--", "-_", " "})
            for (const char* body : {"0", "1", "7", "9", "f", "F", "g", "1_0", "_1", "",
                                     "10101010101010101010101010101010101", "77777777777",
                                     "99999999999", "fffffffff"}) {
                const std::string text = std::string(sign) + body;
                SCOPED_TRACE(text + " in base " + std::to_string(base));
                EXPECT_EQ(kirimasim::kernel::stringToInteger(text, base),
                          kirimasim::valsem::stringToInteger(text, base));
            }
}

TEST(StringReal, SignedTextMatchesTheIndependentReference) {
    for (const char* sign : {"+", "-", "+-", "-_", "- "})
        for (const char* body : {"0", "19.25", "2_5e-1", ".5", "1e+", "x", ""}) {
            const std::string text = std::string(sign) + body;
            SCOPED_TRACE(text);
            const auto actual = kirimasim::kernel::stringToReal(text);
            ASSERT_TRUE(actual);
            EXPECT_DOUBLE_EQ(actual.value_or(std::numeric_limits<double>::quiet_NaN()),
                             kirimasim::valsem::stringToReal(text));
        }
}

using namespace kirimasim::valsem;

/// A value from a string of 0, 1, x and z, most significant first.
Value bits(const std::string& s, bool isSigned = false) {
    Value v;
    v.isSigned = isSigned;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        switch (*it) {
            case '0': v.bits.push_back(Logic::Zero); break;
            case '1': v.bits.push_back(Logic::One); break;
            case 'z': v.bits.push_back(Logic::Z); break;
            default: v.bits.push_back(Logic::X); break;
        }
    }
    return v;
}

std::string str(const Value& v) { return toBinary(v); }

Value s8(std::int64_t v) { return fromUint(static_cast<std::uint64_t>(v) & 0xff, 8, true); }

TEST(Valsem, ArithmeticWithXIsAllX) {  // 3.2
    EXPECT_EQ(str(add(bits("0001"), bits("000x"))), "xxxx");
    EXPECT_EQ(str(mul(bits("0010"), bits("z000"))), "xxxx");
}

TEST(Valsem, DivisionTruncatesTowardZero) {  // 3.2
    EXPECT_EQ(div(s8(-7), s8(2)), s8(-3));
    EXPECT_EQ(mod(s8(-7), s8(2)), s8(-1));  // sign of the dividend
    EXPECT_EQ(mod(s8(7), s8(-2)), s8(1));
    EXPECT_EQ(str(div(bits("0100"), bits("0000"))), "xxxx");
    EXPECT_EQ(str(mod(bits("0100"), bits("0000"))), "xxxx");
    // The most negative value divided by -1 wraps around.
    EXPECT_EQ(div(s8(-128), s8(-1)), s8(-128));
}

TEST(Valsem, PowerFollowsTable11_4) {  // 3.2
    EXPECT_EQ(pow(s8(0), s8(0)), s8(1));
    EXPECT_EQ(str(pow(s8(0), s8(-1))), "xxxxxxxx");
    EXPECT_EQ(pow(s8(1), s8(-5)), s8(1));
    EXPECT_EQ(pow(s8(-1), s8(3)), s8(-1));
    EXPECT_EQ(pow(s8(-1), s8(4)), s8(1));
    EXPECT_EQ(pow(s8(2), s8(-1)), s8(0));
    EXPECT_EQ(pow(s8(3), s8(4)), s8(81));
    EXPECT_EQ(pow(s8(2), s8(9)), s8(0));  // modulo 2^8
}

TEST(Valsem, RelationalWithXIsX) {  // 3.3: even when the known bits decide
    EXPECT_EQ(str(gt(bits("1x"), bits("00"))), "x");
    EXPECT_EQ(str(lt(s8(-1), s8(1))), "1");
    EXPECT_EQ(str(lt(fromUint(0xff, 8, false), fromUint(1, 8, false))), "0");
}

TEST(Valsem, EqualityRules) {                         // 3.4
    EXPECT_EQ(str(eq(bits("1x"), bits("0x"))), "0");  // a known mismatch wins
    EXPECT_EQ(str(eq(bits("1x"), bits("1x"))), "x");
    EXPECT_EQ(str(ceq(bits("1x"), bits("1x"))), "1");
    EXPECT_EQ(str(ceq(bits("1z"), bits("1x"))), "0");
    EXPECT_EQ(str(weq(bits("1010"), bits("1x1z"))), "1");  // X and Z on the right match
    EXPECT_EQ(str(weq(bits("1x10"), bits("1010"))), "x");  // X on the left does not
    EXPECT_EQ(str(weq(bits("1x10"), bits("0010"))), "0");
}

TEST(Valsem, CaseMatchIgnoresBothSides) {  // 4.2, 12.5.1
    // casez ignores z on either side; x is compared as a value.
    EXPECT_EQ(str(caseMatch(bits("10z1"), bits("1001"), false)), "1");
    EXPECT_EQ(str(caseMatch(bits("1001"), bits("10z1"), false)), "1");
    EXPECT_EQ(str(caseMatch(bits("10x1"), bits("1001"), false)), "0");
    EXPECT_EQ(str(caseMatch(bits("10x1"), bits("10x1"), false)), "1");
    // casex also ignores x.
    EXPECT_EQ(str(caseMatch(bits("10x1"), bits("1001"), true)), "1");
    EXPECT_EQ(str(caseMatch(bits("1001"), bits("1x11"), true)), "0");
}

TEST(Valsem, TruthAndLogical) {  // 3.5
    EXPECT_EQ(str(truth(bits("1x"))), "1");
    EXPECT_EQ(str(truth(bits("0x"))), "x");
    EXPECT_EQ(str(truth(bits("00"))), "0");
    EXPECT_EQ(str(lnot(bits("0x"))), "x");
    EXPECT_EQ(str(land(bits("0x"), bits("00"))), "0");
    EXPECT_EQ(str(lor(bits("0x"), bits("01"))), "1");
}

TEST(Valsem, BitwiseIsPerBitEvenWhenSigned) {  // 3.6, the interpretation of 11.8.4
    EXPECT_EQ(str(bitAnd(bits("10x1", true), bits("1111", true))), "10x1");
    EXPECT_EQ(str(bitOr(bits("0z01"), bits("0001"))), "0x01");
    EXPECT_EQ(str(bitNot(bits("01xz"))), "10xx");
}

TEST(Valsem, Reductions) {  // 3.7
    EXPECT_EQ(str(redAnd(bits("1x0"))), "0");
    EXPECT_EQ(str(redAnd(bits("1x1"))), "x");
    EXPECT_EQ(str(redOr(bits("0x1"))), "1");
    EXPECT_EQ(str(redXor(bits("0110"))), "0");
    EXPECT_EQ(str(redXnor(bits("0111"))), "0");
    // A 1-bit z reduces to x (checked with Icarus Verilog 13.0).
    EXPECT_EQ(str(redAnd(bits("z"))), "x");
    EXPECT_EQ(str(redXor(bits("z"))), "x");
}

TEST(Valsem, Shifts) {  // 3.8
    EXPECT_EQ(str(shl(bits("0011"), fromUint(1, 4, false))), "0110");
    EXPECT_EQ(str(shr(bits("1100"), fromUint(5, 4, false))), "0000");
    EXPECT_EQ(str(ashr(bits("1000", true), fromUint(2, 4, false))), "1110");
    EXPECT_EQ(str(ashr(bits("1000", false), fromUint(2, 4, false))), "0010");
    EXPECT_EQ(str(ashr(bits("x000", true), fromUint(9, 4, false))), "xxxx");
    EXPECT_EQ(str(shl(bits("0011"), bits("0x"))), "xxxx");
}

TEST(Valsem, SelectMergesWhenTheConditionIsUnknown) {  // 3.9, table 11-20
    EXPECT_EQ(str(select(bits("x"), bits("z1"), bits("z0"))), "xx");
    EXPECT_EQ(str(select(bits("x"), bits("1010"), bits("1010"))), "1010");
    EXPECT_EQ(str(select(bits("x"), bits("1100"), bits("1010"))), "1xx0");
    EXPECT_EQ(str(select(bits("1"), bits("01"), bits("10"))), "01");
}

TEST(Valsem, SignExtensionCopiesXAndZ) {  // 2
    EXPECT_EQ(str(sext(bits("z111", true), 8, true)), "zzzzz111");
    EXPECT_EQ(str(zext(bits("z111", true), 8, false)), "0000z111");
    EXPECT_EQ(str(to2(bits("1xz0"))), "1000");
}

TEST(Valsem, Edges) {  // 4.3
    EXPECT_TRUE(isPosedge(Logic::Zero, Logic::Z));
    EXPECT_TRUE(isPosedge(Logic::X, Logic::One));
    EXPECT_FALSE(isPosedge(Logic::X, Logic::Z));
    EXPECT_TRUE(isNegedge(Logic::One, Logic::X));
    EXPECT_FALSE(isNegedge(Logic::Z, Logic::X));
}

TEST(Valsem, WideValues) {
    // 65-bit addition carries into the second word.
    const Value a = fromWords({~std::uint64_t{0}, 0}, {}, 65, false);
    const Value one = fromUint(1, 65, false);
    std::vector<std::uint64_t> wa;
    std::vector<std::uint64_t> wb;
    toWords(add(a, one), wa, wb);
    EXPECT_EQ(wa[0], 0U);
    EXPECT_EQ(wa[1], 1U);
    // 128-bit unsigned division.
    const Value big = fromWords({0, 1}, {}, 128, false);  // 2^64
    toWords(div(big, fromUint(2, 128, false)), wa, wb);
    EXPECT_EQ(wa[0], std::uint64_t{1} << 63);
    EXPECT_EQ(wa[1], 0U);
}

}  // namespace
