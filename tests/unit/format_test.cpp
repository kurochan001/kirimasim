// Tests for the formatting of $display values (SYSTEM_TASKS.md 3). The
// expected texts follow the examples and rules of that section.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/format.h"

namespace {

using namespace kirimasim::kernel;

/// A four-state value from 0, 1, x and z, most significant first.
struct Bits {
    std::vector<std::uint64_t> words;
    IntValue value;
    explicit Bits(const std::string& s, bool isSigned = false) {
        const std::size_t w = s.size();
        words.assign(2 * ((w + 63) / 64), 0);
        for (std::size_t i = 0; i < w; ++i) {
            const char c = s[w - 1 - i];
            const std::uint64_t bit = std::uint64_t{1} << (i % 64);
            if (c == '1' || c == 'x') words[2 * (i / 64)] |= bit;
            if (c == 'x' || c == 'z') words[2 * (i / 64) + 1] |= bit;
        }
        value = {words.data(), static_cast<std::uint32_t>(w), true, isSigned};
    }
};

/// A two-state value of up to 64 bits.
struct Num {
    std::uint64_t word;
    IntValue value;
    Num(std::uint64_t v, std::uint32_t w, bool isSigned = false)
        : word(v), value{&word, w, false, isSigned} {}
};

std::string fmt(const IntValue& v, char conv, int width = -1) {
    return formatInt(v, {conv, width});
}

TEST(Format, AutomaticWidth) {
    // 3.3: a 32-bit 10 is ten characters with %d and eight digits with %h.
    const Num ten(10, 32);
    EXPECT_EQ(fmt(ten.value, 'd'), "        10");
    EXPECT_EQ(fmt(ten.value, 'h'), "0000000a");
    EXPECT_EQ(fmt(ten.value, 'o'), "00000000012");
    EXPECT_EQ(fmt(Num(5, 4).value, 'b'), "0101");
    // A signed 32-bit value takes 11 characters, with room for the sign.
    EXPECT_EQ(fmt(Num(static_cast<std::uint32_t>(-5), 32, true).value, 'd'), "         -5");
    EXPECT_EQ(fmt(Num(0x80000000U, 32, true).value, 'd'), "-2147483648");
    EXPECT_EQ(fmt(Num(255, 8).value, 'd'), "255");
}

TEST(Format, ZeroAndExplicitWidths) {
    const Num five(5, 32);
    EXPECT_EQ(fmt(five.value, 'd', 0), "5");
    EXPECT_EQ(fmt(five.value, 'h', 0), "5");
    EXPECT_EQ(fmt(five.value, 'b', 0), "101");
    // 3.3: an explicit width pads (zeros for h, o, b) and never truncates.
    EXPECT_EQ(fmt(five.value, 'h', 3), "005");
    EXPECT_EQ(fmt(five.value, 'b', 12), "000000000101");
    EXPECT_EQ(fmt(five.value, 'd', 4), "   5");
    EXPECT_EQ(fmt(Num(1234, 32).value, 'd', 2), "1234");
    EXPECT_EQ(fmt(Num(0, 8).value, 'h', 0), "0");
}

TEST(Format, UnknownBits) {
    // 3.4: %d shows one character; %h and %o one per digit group.
    EXPECT_EQ(fmt(Bits("xxxxxxxx").value, 'd'), "  x");
    EXPECT_EQ(fmt(Bits("zzzzzzzz").value, 'd'), "  z");
    EXPECT_EQ(fmt(Bits("0000x001").value, 'd'), "  X");
    EXPECT_EQ(fmt(Bits("0000z001").value, 'd'), "  Z");
    EXPECT_EQ(fmt(Bits("xz000001").value, 'd', 0), "X");
    EXPECT_EQ(fmt(Bits("xxxx0101").value, 'h'), "x5");
    EXPECT_EQ(fmt(Bits("10z10101").value, 'h'), "Z5");
    EXPECT_EQ(fmt(Bits("zzzz1x01").value, 'h'), "zX");
    EXPECT_EQ(fmt(Bits("10xz").value, 'b'), "10xz");
    EXPECT_EQ(fmt(Bits("xx1zzz").value, 'o'), "Xz");
    EXPECT_EQ(fmt(Bits("xxxxxxxx").value, 'h', 0), "xx");
}

TEST(Format, Characters) {
    // 3.5: %s drops leading zero bytes; the automatic width pads them.
    EXPECT_EQ(fmt(Num(0x0041, 16).value, 's'), " A");
    EXPECT_EQ(fmt(Num(0x0041, 16).value, 's', 0), "A");
    EXPECT_EQ(fmt(Num(0x4142, 16).value, 's'), "AB");
    EXPECT_EQ(fmt(Num(0x141, 16).value, 'c'), "A");
}

TEST(Format, WideDecimal) {
    // 2^64 in a 128-bit value: 39 characters wide (2^128 - 1 has 39 digits).
    std::vector<std::uint64_t> w{0, 1};
    const IntValue v{w.data(), 128, false, false};
    const std::string s = fmt(v, 'd');
    EXPECT_EQ(s.size(), 39U);
    EXPECT_EQ(s.substr(s.find_first_not_of(' ')), "18446744073709551616");
}

TEST(Format, Time) {
    const Num t(5000, 64);
    TimeFormat tf;  // the defaults of 5.4 with a 1ps precision
    tf.unit = -12;
    EXPECT_EQ(formatTime(t.value, -12, tf, -1), std::string(16, ' ') + "5000");
    EXPECT_EQ(formatTime(t.value, -12, tf, 0), "5000");
    tf = {-9, 2, " ns", 0};
    EXPECT_EQ(formatTime(t.value, -12, tf, -1), "5.00 ns");
    EXPECT_EQ(formatTime(Num(1234, 64).value, -12, tf, -1), "1.23 ns");
    EXPECT_EQ(formatTime(Num(1235, 64).value, -12, tf, -1), "1.24 ns");  // half away from 0
    EXPECT_EQ(formatTime(Num(7, 64).value, -12, tf, -1), "0.01 ns");
    tf = {-15, 0, "", 0};
    EXPECT_EQ(formatTime(Num(3, 64).value, -12, tf, -1), "3000");
    EXPECT_EQ(formatTime(Num(0, 64).value, -12, tf, -1), "0");
}

TEST(Format, Reals) {
    // 3.6: C's formats, with inf, -inf and nan spelled one way.
    std::vector<FormatPiece> p;
    std::string error;
    ASSERT_TRUE(parseFormat("%f|%e|%g|%10.3f|%-8.2f|%+.1e", p, error));
    const auto spec = [&](std::size_t i) { return p[i].spec; };
    EXPECT_EQ(formatReal(5.0, spec(0)), "5.000000");
    EXPECT_EQ(formatReal(1.5, spec(2)), "1.500000e+00");
    EXPECT_EQ(formatReal(1.5, spec(4)), "1.5");
    EXPECT_EQ(formatReal(1.5, spec(6)), "     1.500");
    EXPECT_EQ(formatReal(1.5, spec(8)), "1.50    ");
    EXPECT_EQ(formatReal(1.5, spec(10)), "+1.5e+00");
    EXPECT_EQ(formatReal(1.5, FormatSpec{}), "1.5");  // no specifier: %g
    EXPECT_EQ(formatReal(123456789.0, FormatSpec{}), "1.23457e+08");
    EXPECT_EQ(formatReal(1.0 / 0.0, spec(0)), "inf");
    EXPECT_EQ(formatReal(-1.0 / 0.0, spec(6)), "      -inf");
    EXPECT_EQ(formatReal(-std::nan(""), spec(8)), "nan     ");
    p.clear();
    EXPECT_FALSE(parseFormat("%.2d", p, error));  // a precision is for reals only
}

TEST(Format, Parse) {
    std::vector<FormatPiece> p;
    std::string error;
    ASSERT_TRUE(parseFormat("a=%0d%%b %H|%5t", p, error));
    ASSERT_EQ(p.size(), 6U);
    EXPECT_EQ(p[0].text, "a=");
    EXPECT_TRUE(p[1].isSpec);
    EXPECT_EQ(p[1].spec.conv, 'd');
    EXPECT_EQ(p[1].spec.width, 0);
    EXPECT_EQ(p[2].text, "%b ");
    EXPECT_EQ(p[3].spec.conv, 'h');
    EXPECT_EQ(p[3].spec.width, -1);
    EXPECT_EQ(p[4].text, "|");
    EXPECT_EQ(p[5].spec.conv, 't');
    EXPECT_EQ(p[5].spec.width, 5);
    p.clear();
    EXPECT_FALSE(parseFormat("%q", p, error));
    EXPECT_FALSE(parseFormat("100%", p, error));
}

}  // namespace
