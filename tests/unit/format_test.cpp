// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Tests for the formatting of $display values (SYSTEM_TASKS.md 3). The
// expected texts follow the examples and rules of that section.

#include <array>
#include <bit>
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

std::string nativeWords(std::initializer_list<std::uint32_t> words) {
    std::string text;
    for (const std::uint32_t word : words) {
        const auto bytes = std::bit_cast<std::array<char, 4>>(word);
        text.append(bytes.data(), bytes.size());
    }
    return text;
}

TEST(Format, UnformattedWords) {
    // 21.2.1.1: raw words, not text digits; X and Z both become 0 in %u.
    const Bits mixed("1xz0");
    EXPECT_EQ(fmt(mixed.value, 'u'), nativeWords({8}));
    EXPECT_EQ(fmt(mixed.value, 'z'), nativeWords({12, 6}));
    EXPECT_EQ(fmt(Num(0, 1).value, 'u'), std::string(4, '\0'));
    EXPECT_EQ(fmt(Num(0, 1).value, 'z'), std::string(8, '\0'));
    EXPECT_EQ(fmt(Num(0x00414200, 32).value, 'u'), nativeWords({0x00414200}));
    // No sign extension, even in an incomplete final word.
    EXPECT_EQ(fmt(Num(31, 5, true).value, 'z'), nativeWords({31, 0}));
    EXPECT_EQ(fmt(Num(UINT64_MAX, 33, true).value, 'u'), nativeWords({UINT32_MAX, 1}));
}

TEST(Format, UnformattedWideAndPadding) {
    const std::uint64_t words[] = {0x0123456789abcdef, 0xfedcba9876543210, UINT64_MAX};
    const IntValue value{words, 129, false, true};
    const std::string expected = nativeWords({0x89abcdef, 0x01234567, 0x76543210, 0xfedcba98, 1});
    EXPECT_EQ(fmt(value, 'u'), expected);
    EXPECT_EQ(fmt(value, 'u', 100), expected);
    const Bits unknown("x" + std::string(64, 'z'));
    EXPECT_EQ(fmt(unknown.value, 'u'), std::string(12, '\0'));
    EXPECT_EQ(fmt(unknown.value, 'z'), nativeWords({0, UINT32_MAX, 0, UINT32_MAX, 1, 1}));
}

TEST(Format, UnformattedParseAndRuntime) {
    std::vector<FormatPiece> pieces;
    std::string error;
    ASSERT_TRUE(parseFormat("%u%Z", pieces, error));
    ASSERT_EQ(pieces.size(), 2U);
    EXPECT_EQ(pieces[0].spec.conv, 'u');
    EXPECT_EQ(pieces[1].spec.conv, 'z');
    const Bits value("1xz0");
    FormatValue arg;
    arg.integral = value.value;
    const std::array args{arg, arg};
    const auto result = formatStringCall("%u%z", args, "top", "work.top", 1, -9, {});
    EXPECT_TRUE(result.error.empty());
    EXPECT_FALSE(result.argumentMismatch);
    EXPECT_EQ(result.text, nativeWords({8, 12, 6}));
    // SV strings cannot hold NUL; packed targets keep the raw buffer.
    EXPECT_EQ(formattedString(result.text), std::string("\x08\x0c\x06", 3));
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
    // A zero byte after the first nonzero one is a space, with %0s too.
    EXPECT_EQ(fmt(Num(0x00410042, 32).value, 's'), " A B");
    EXPECT_EQ(fmt(Num(0x00410042, 32).value, 's', 0), "A B");
    EXPECT_EQ(fmt(Num(0x41000000, 32).value, 's', 0), "A   ");
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

// A real %t is rounded once, half away from zero, on its shortest decimal
// text (SYSTEM_TASKS.md 3.7): 1250.5 ticks of 1ps is 1.2505 ns.
TEST(Format, RealTimes) {
    TimeFormat tf;
    tf.unit = -9;
    tf.decimals = 3;
    tf.suffix = " ns";
    tf.minWidth = 0;
    EXPECT_EQ(formatTimeReal(1250.5, -12, tf, -1), "1.251 ns");
    EXPECT_EQ(formatTimeReal(1250.4, -12, tf, -1), "1.250 ns");
    EXPECT_EQ(formatTimeReal(-1250.5, -12, tf, -1), "-1.251 ns");
    EXPECT_EQ(formatTimeReal(0.0, -12, tf, -1), "0.000 ns");
    EXPECT_EQ(formatTimeReal(999999.5, -12, tf, -1), "1000.000 ns");
    tf.decimals = 0;
    EXPECT_EQ(formatTimeReal(1500.0, -12, tf, -1), "2 ns");
    EXPECT_EQ(formatTimeReal(1499.0, -12, tf, -1), "1 ns");
    EXPECT_EQ(formatTimeReal(0.4, -12, tf, -1), "0 ns");
    tf.unit = -15;  // a unit finer than the precision
    EXPECT_EQ(formatTimeReal(2.5, -12, tf, 8), " 2500 ns");
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

TEST(Format, CapitalRealFormats) {
    // 3.6: %E and %G write a capital E as C does, %F is %f; infinities and
    // NaN keep their one spelling.
    std::vector<FormatPiece> p;
    std::string error;
    ASSERT_TRUE(parseFormat("%E|%G|%F|%10.2E", p, error));
    const auto spec = [&](std::size_t i) { return p[i].spec; };
    EXPECT_EQ(formatReal(1.23e-4, spec(0)), "1.230000E-04");
    EXPECT_EQ(formatReal(1.0e10, spec(2)), "1E+10");
    EXPECT_EQ(formatReal(2.5, spec(4)), "2.500000");
    EXPECT_EQ(formatReal(-1.5e3, spec(6)), " -1.50E+03");
    EXPECT_EQ(formatReal(1.0 / 0.0, spec(0)), "inf");
    EXPECT_EQ(formatReal(std::nan(""), spec(2)), "nan");
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

TEST(Format, FlagsParse) {
    // 3.12: - and a width that starts with 0 and is not 0; %0d is the width
    // 0; the other flags and a precision are for the real formats only.
    struct Case {
        const char* format;
        int width;
        bool left;
        bool zero;
    };
    const Case cases[] = {
        {"%-d", -1, true, false},  {"%-5h", 5, true, false}, {"%05d", 5, false, true},
        {"%005d", 5, false, true}, {"%-05d", 5, true, true}, {"%0d", 0, false, false},
        {"%00d", 0, false, false}, {"%-0s", 0, true, false}, {"%010t", 10, false, true},
    };
    for (const Case& c : cases) {
        std::vector<FormatPiece> p;
        std::string error;
        ASSERT_TRUE(parseFormat(c.format, p, error)) << c.format << ": " << error;
        ASSERT_EQ(p.size(), 1U) << c.format;
        EXPECT_EQ(p[0].spec.width, c.width) << c.format;
        EXPECT_EQ(p[0].spec.left, c.left) << c.format;
        EXPECT_EQ(p[0].spec.zero, c.zero) << c.format;
    }
    for (const char* bad : {"%+d", "% d", "%#h", "%+s", "%.3t", "%-.2b"}) {
        std::vector<FormatPiece> p;
        std::string error;
        EXPECT_FALSE(parseFormat(bad, p, error)) << bad;
    }
    // The real formats keep their flags for C, and not as left or zero.
    std::vector<FormatPiece> p;
    std::string error;
    ASSERT_TRUE(parseFormat("%-08.2f", p, error));
    EXPECT_EQ(p[0].spec.cformat, "%-08.2f");
    EXPECT_FALSE(p[0].spec.left);
    EXPECT_FALSE(p[0].spec.zero);
}

TEST(Format, LeftFlag) {
    // 3.12: the value in the smallest width at the left of its field, the
    // written width or the automatic one.
    const auto left = [](const IntValue& v, char conv, int width) {
        FormatSpec s{conv, width};
        s.left = true;
        return formatInt(v, s);
    };
    const Num five(5, 32, true);
    EXPECT_EQ(left(five.value, 'd', 4), "5   ");
    EXPECT_EQ(left(five.value, 'd', -1), "5          ");  // 11, with room for the sign
    EXPECT_EQ(left(five.value, 'h', -1), "5       ");
    EXPECT_EQ(left(five.value, 'o', 3), "5  ");
    EXPECT_EQ(left(Num(5, 4).value, 'b', -1), "101 ");
    EXPECT_EQ(left(five.value, 'd', 0), "5");
    EXPECT_EQ(left(five.value, 'd', 1), "5");
    EXPECT_EQ(left(Num(static_cast<std::uint32_t>(-3), 32, true).value, 'd', 5), "-3   ");
    EXPECT_EQ(left(Bits("xxxxxxxx").value, 'd', -1), "x  ");
    EXPECT_EQ(left(Bits("0000x101").value, 'h', -1), "X ");
    EXPECT_EQ(left(Num(65, 8).value, 'c', 3), "A  ");
    EXPECT_EQ(left(Num(0x006869, 24).value, 's', -1), "hi ");
    EXPECT_EQ(left(Num(0x6162, 16).value, 's', 5), "ab   ");
    FormatSpec s{'s', 5};
    s.left = true;
    EXPECT_EQ(formatString("abc", s), "abc  ");
    EXPECT_EQ(formatString("abc", FormatSpec{'s', 5}), "  abc");
    EXPECT_EQ(formatString("abc", FormatSpec{'s', -1}), "abc");
    const TimeFormat tf;
    EXPECT_EQ(formatTime(Num(3, 64).value, -9, tf, -1, true), "3" + std::string(19, ' '));
    EXPECT_EQ(formatTime(Num(3, 64).value, -9, tf, 4, true), "3   ");
    EXPECT_EQ(formatTimeReal(3.0, -9, tf, 4, true), "3   ");
}

TEST(Format, ZeroFlag) {
    // 3.12: %d pads with 0 after the sign, a lone x or z too; the other
    // formats do not change, and - wins.
    const auto zero = [](const IntValue& v, char conv, int width, bool left = false) {
        FormatSpec s{conv, width};
        s.zero = true;
        s.left = left;
        return formatInt(v, s);
    };
    EXPECT_EQ(zero(Num(5, 32, true).value, 'd', 5), "00005");
    EXPECT_EQ(zero(Num(static_cast<std::uint32_t>(-3), 32, true).value, 'd', 5), "-0003");
    EXPECT_EQ(zero(Bits("xxxxxxxx").value, 'd', 5), "0000x");
    EXPECT_EQ(zero(Bits("zzzzzzzz").value, 'd', 5), "0000z");
    EXPECT_EQ(zero(Bits("0000x101").value, 'd', 5), "0000X");
    EXPECT_EQ(zero(Num(123456, 32).value, 'd', 3), "123456");
    EXPECT_EQ(zero(Num(5, 32).value, 'h', 5), "00005");
    EXPECT_EQ(zero(Num(65, 8).value, 'c', 3), "  A");
    EXPECT_EQ(zero(Num(0x6162, 16).value, 's', 4), "  ab");
    EXPECT_EQ(zero(Num(5, 32, true).value, 'd', 5, true), "5    ");
}

TEST(Format, RuntimeStringValues) {
    const Num n(static_cast<std::uint32_t>(-3), 32, true);
    const FormatValue args[] = {{FormatValue::Kind::Integral, n.value},
                                {FormatValue::Kind::Integral, n.value},
                                {.kind = FormatValue::Kind::Real, .real = 1.5},
                                {.kind = FormatValue::Kind::String, .text = "literal %0d"}};
    const auto out =
        formatStringCall("%0d|%0.1f|%0d|%s", args, "top", "work.top", 1000, -12, TimeFormat{});
    EXPECT_TRUE(out.error.empty());
    EXPECT_FALSE(out.argumentMismatch);
    EXPECT_EQ(out.text, "-3|-3.0|2|literal %0d");
}

TEST(Format, RuntimeWideValues) {
    const std::string bits = std::string(65, '0') + "10xz" + std::string(60, '0');
    const Bits n(bits);
    const FormatValue args[] = {{FormatValue::Kind::Integral, n.value},
                                {FormatValue::Kind::Integral, n.value}};
    const auto out = formatStringCall("%b|%0.0f", args, "top", "work.top", 1, -9, TimeFormat{});
    EXPECT_TRUE(out.error.empty());
    EXPECT_EQ(out.text, bits + "|9223372036854775808");
}

TEST(Format, RuntimeTimeUnits) {
    const Num n(2, 8);
    const Bits x("xx");
    const FormatValue args[] = {{FormatValue::Kind::Integral, n.value},
                                {.kind = FormatValue::Kind::Real, .real = 1.255},
                                {FormatValue::Kind::Integral, x.value}};
    const TimeFormat tf{-9, 2, "ns", 0};
    const auto out = formatStringCall("%t|%8t|%t", args, "top", "work.top", 1000, -12, tf);
    EXPECT_TRUE(out.error.empty());
    EXPECT_EQ(out.text, "2.00ns|  1.26ns|xns");
    const auto unscaled = formatStringCall("%t", std::span(args).first(1), "", "", 1, -9, tf);
    EXPECT_EQ(unscaled.text, "2.00ns");
}

TEST(Format, RuntimeScopeIsLiteral) {
    const auto out = formatStringCall("%m|%l|%%m", {}, "top.%g", "work.unit", 1, -9, TimeFormat{});
    EXPECT_TRUE(out.error.empty());
    EXPECT_FALSE(out.argumentMismatch);
    EXPECT_EQ(out.text, "top.%g|work.unit|%m");
    const auto missing = formatStringCall("%l", {}, "p.run", "", 1, -9, TimeFormat{});
    EXPECT_EQ(missing.error, "%l outside a module");
}

TEST(Format, RuntimeArgumentCountsAndEmpty) {
    const Num n(7, 32);
    const FormatValue args[] = {{FormatValue::Kind::Integral, n.value},
                                {.kind = FormatValue::Kind::Empty}};
    const auto missing = formatStringCall("A%0dB%0dC%0dD", args, "", "", 1, -9, TimeFormat{});
    EXPECT_TRUE(missing.error.empty());
    EXPECT_TRUE(missing.argumentMismatch);
    EXPECT_EQ(missing.text, "A7B CD");
    const auto extra = formatStringCall("A%0d", args, "", "", 1, -9, TimeFormat{});
    EXPECT_TRUE(extra.argumentMismatch);
    EXPECT_EQ(extra.text, "A7");
}

TEST(Format, RuntimeInvalidFormatsAndTypes) {
    const FormatValue arg{.kind = FormatValue::Kind::String, .text = "seed"};
    for (const std::string text : {"%", "%3", "%q", "%p", "%+d", "%.2d", "%0d", "%t"}) {
        const auto out = formatStringCall(text, std::span(&arg, 1), "", "", 1, -9, TimeFormat{});
        EXPECT_FALSE(out.error.empty()) << text;
    }
}

}  // namespace
