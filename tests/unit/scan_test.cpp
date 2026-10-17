// Formatted reading for $sscanf (SYSTEM_TASKS.md 6.3, 21.3.4.3).

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/scan.h"

namespace {

using kirimasim::kernel::parseScanFormat;
using kirimasim::kernel::ScanConversion;
using kirimasim::kernel::scanString;
using kirimasim::kernel::ScanValue;

std::vector<ScanValue> scan(const std::string& in, const std::string& fmt, int expect) {
    std::vector<ScanValue> out;
    EXPECT_EQ(scanString(in, fmt, out), expect) << "'" << in << "' by '" << fmt << "'";
    return out;
}

TEST(Scan, Decimal) {
    auto v = scan("  42 -7 +3", "%d %d %d", 3);
    EXPECT_EQ(v[0].bits.a, 42U);
    EXPECT_EQ(v[1].bits.a, ~std::uint64_t{6});  // -7 in two's complement
    EXPECT_EQ(v[2].bits.a, 3U);
    v = scan("1_000", "%d", 1);
    EXPECT_EQ(v[0].bits.a, 1000U);
    // A lone x or z fills the target.
    v = scan("x z ?", "%d %d %d", 3);
    EXPECT_TRUE(v[0].fill);
    EXPECT_EQ(v[0].bits.a, ~std::uint64_t{0});
    EXPECT_EQ(v[1].bits.a, 0U);
    EXPECT_EQ(v[2].bits.b, ~std::uint64_t{0});
    // A sign without digits does not match.
    scan("-x", "%d", 0);
}

TEST(Scan, Radix) {
    // No base prefix: "0xCAFE" is the digits 0, x, C, A, F and E.
    auto v = scan("0xCAFE", "%h", 1);
    EXPECT_EQ(v[0].bits.a, 0xFCAFEU);
    EXPECT_EQ(v[0].bits.b, 0xF0000U);
    v = scan("0o777", "%o", 1);  // the o ends the field
    EXPECT_EQ(v[0].bits.a, 0U);
    v = scan("10_1z", "%b", 1);
    EXPECT_EQ(v[0].bits.a, 0xAU);
    EXPECT_EQ(v[0].bits.b, 0x1U);
    v = scan("ff", "%x", 1);
    EXPECT_EQ(v[0].bits.a, 0xFFU);
    scan("g", "%h", 0);
}

TEST(Scan, WideValues) {
    // Every digit is kept, least significant chunk first; bits is the low
    // chunk (SYSTEM_TASKS.md 6.3, 9).
    auto v = scan("1_23456789abcdef0123456789", "%h", 1);
    ASSERT_EQ(v[0].wide.size(), 2U);
    EXPECT_EQ(v[0].wide[0].a, 0xabcdef0123456789U);
    EXPECT_EQ(v[0].wide[1].a, 0x123456789U);
    EXPECT_EQ(v[0].bits.a, 0xabcdef0123456789U);
    // 2^70 + 5 in decimal, and with a minus sign: the magnitude, negative.
    v = scan("1180591620717411303429 -1180591620717411303429", "%d %d", 2);
    ASSERT_EQ(v[0].wide.size(), 2U);
    EXPECT_EQ(v[0].wide[0].a, 5U);
    EXPECT_EQ(v[0].wide[1].a, 64U);
    EXPECT_FALSE(v[0].negative);
    EXPECT_TRUE(v[1].negative);
    EXPECT_EQ(v[1].wide[1].a, 64U);
    EXPECT_EQ(v[1].bits.a, ~std::uint64_t{5} + 1);
    // An x digit past the first chunk.
    v = scan("x" + std::string(64, '0'), "%b", 1);
    ASSERT_EQ(v[0].wide.size(), 2U);
    EXPECT_EQ(v[0].wide[1].a, 1U);
    EXPECT_EQ(v[0].wide[1].b, 1U);
    EXPECT_EQ(v[0].wide[0].b, 0U);
}

TEST(Scan, CharsStringsReals) {
    auto v = scan(" ab cd", "%c%s %s", 3);
    EXPECT_EQ(v[0].bits.a, static_cast<unsigned>(' '));  // %c takes white space
    EXPECT_EQ(v[1].text, "ab");
    EXPECT_EQ(v[2].text, "cd");
    v = scan("1.5 -2e3 7. 3e", "%f %e %g %f", 4);
    EXPECT_DOUBLE_EQ(v[0].real, 1.5);
    EXPECT_DOUBLE_EQ(v[1].real, -2000.0);
    EXPECT_DOUBLE_EQ(v[2].real, 7.0);
    EXPECT_DOUBLE_EQ(v[3].real, 3.0);  // an e without digits is not part of it
}

TEST(Scan, WidthsSuppressionLiterals) {
    auto v = scan("12345", "%2d%3d", 2);
    EXPECT_EQ(v[0].bits.a, 12U);
    EXPECT_EQ(v[1].bits.a, 345U);
    v = scan("a=1, b=2", "a=%*d, b=%d", 1);
    EXPECT_EQ(v[0].bits.a, 2U);
    v = scan("50%", "%d%%", 1);
    EXPECT_EQ(v[0].bits.a, 50U);
    // A literal that does not match ends the reading.
    v = scan("1;2", "%d,%d", 1);
    // NUL is white space.
    v = scan(std::string("7\0"
                         "8",
                         3),
             "%d %d", 2);
    EXPECT_EQ(v[1].bits.a, 8U);
}

TEST(Scan, EndOfInput) {
    // -1 when the input ends before the first conversion, the count after.
    scan("", "%d", -1);
    scan("   ", "%d", -1);
    scan("abc", "%d", 0);
    scan("5", "%d %d", 1);
    scan("", "x%d", -1);
}

TEST(Scan, PlusargValues) {
    using kirimasim::kernel::plusargValue;
    EXPECT_EQ(plusargValue("42", 'd').bits.a, 42U);
    EXPECT_EQ(plusargValue("", 'd').bits.a, 0U);
    EXPECT_EQ(plusargValue("-1", 'd').bits.a, ~std::uint64_t{0});
    EXPECT_EQ(plusargValue("ff", 'h').bits.a, 0xFFU);
    EXPECT_TRUE(plusargValue("12ab", 'd').fill);  // not readable whole: x
    EXPECT_EQ(plusargValue("12ab", 'd').bits.b, ~std::uint64_t{0});
    EXPECT_EQ(plusargValue("fast", 's').text, "fast");
    EXPECT_DOUBLE_EQ(plusargValue("2.5", 'f').real, 2.5);
}

TEST(Scan, ParseFormat) {
    std::vector<ScanConversion> c;
    std::string bad;
    ASSERT_TRUE(parseScanFormat("%d %*s %X %e %%", c, bad));
    ASSERT_EQ(c.size(), 4U);
    EXPECT_EQ(c[0].conv, 'd');
    EXPECT_FALSE(c[1].assigns);
    EXPECT_EQ(c[2].conv, 'h');
    EXPECT_EQ(c[3].conv, 'f');
    c.clear();
    EXPECT_FALSE(parseScanFormat("%t", c, bad));
    EXPECT_EQ(bad, "%t");
}

}  // namespace
