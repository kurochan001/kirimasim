// Checks the valsem reference against the examples that VALUE_SEMANTICS.md
// records as checked against the standard (and, where noted there, against
// Icarus Verilog 13.0).

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "haremasim/valsem/value.h"

namespace {

using namespace haremasim::valsem;

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
