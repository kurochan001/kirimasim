// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Formatted reading for $sscanf and $fscanf (SYSTEM_TASKS.md 6.3, 21.3.4.3).

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/scan.h"

namespace {

using kirimasim::kernel::parseScanFormat;
using kirimasim::kernel::ScanConversion;
using kirimasim::kernel::ScanInput;
using kirimasim::kernel::scanInput;
using kirimasim::kernel::scanString;
using kirimasim::kernel::ScanValue;

/// The characters of a string, one at a time, counting how many were taken.
class FromString final : public ScanInput {
public:
    explicit FromString(std::string s) : s_(std::move(s)) {}
    int get() override {
        return taken_ < s_.size() ? static_cast<unsigned char>(s_[taken_++]) : -1;
    }
    [[nodiscard]] std::size_t taken() const { return taken_; }

private:
    std::string s_;
    std::size_t taken_ = 0;
};

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

// $fscanf: the same reading, from characters taken one at a time; what was
// looked at and not used comes back, and nothing after it is taken.
TEST(Scan, FromAStream) {
    FromString in("12 ab 1e+x rest");
    std::vector<ScanValue> out;
    std::string unread;
    EXPECT_EQ(scanInput(in, "%d %s %f", out, unread), 3);
    EXPECT_EQ(out[0].bits.a, 12U);
    EXPECT_EQ(out[1].text, "ab");
    EXPECT_DOUBLE_EQ(out[2].real, 1.0);
    // %f looked at "e+x" for an exponent and did not use it.
    EXPECT_EQ(unread, "e+x");
    EXPECT_EQ(in.taken(), 10U);
    // A literal that does not match is looked at and given back.
    FromString lit("7;8");
    out.clear();
    EXPECT_EQ(scanInput(lit, "%d,%d", out, unread), 1);
    EXPECT_EQ(unread, ";");
    // The end before the first conversion is -1; NUL is not white space.
    FromString empty("  ");
    out.clear();
    EXPECT_EQ(scanInput(empty, "%d", out, unread), -1);
    FromString nul(std::string("\0"
                               "5",
                               2));
    out.clear();
    EXPECT_EQ(scanInput(nul, "%d", out, unread), 0);
    EXPECT_EQ(scan(std::string("\0"
                               "5",
                               2),
                   "%d", 1)[0]
                  .bits.a,
              5U);
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

TEST(Scan, RuntimeFormatTargetTypes) {
    using Target = kirimasim::kernel::ScanTarget;
    struct Row {
        const char* format;
        std::array<bool, 4> accepts;  // integral, string, real, other
    };
    const Row rows[] = {{"%b", {true, false, false, false}}, {"%o", {true, false, false, false}},
                        {"%d", {true, false, false, false}}, {"%h", {true, false, false, false}},
                        {"%X", {true, false, false, false}}, {"%c", {true, false, false, false}},
                        {"%s", {true, true, false, false}},  {"%f", {false, false, true, false}},
                        {"%E", {false, false, true, false}}, {"%g", {false, false, true, false}}};
    const std::array types{Target::Integral, Target::String, Target::Real, Target::Other};
    for (const Row& row : rows) {
        for (std::size_t i = 0; i < types.size(); ++i) {
            const std::array targets{types[i]};
            const auto plan = kirimasim::kernel::prepareScanFormat(row.format, targets);
            EXPECT_EQ(plan.error.empty(), row.accepts[i]) << row.format << " type " << i;
            EXPECT_FALSE(plan.missingTargets);
        }
    }
}

TEST(Scan, RuntimeFormatMissingTargetsStopReading) {
    const std::array targets{kirimasim::kernel::ScanTarget::Integral};
    const auto plan = kirimasim::kernel::prepareScanFormat("%d,%d", targets);
    ASSERT_TRUE(plan.error.empty());
    ASSERT_TRUE(plan.missingTargets);
    EXPECT_EQ(plan.format, "%d,");
    FromString input("12,34");
    std::vector<ScanValue> out;
    std::string unread;
    EXPECT_EQ(scanInput(input, plan.format, out, unread), 1);
    EXPECT_EQ(input.taken() - unread.size(), 3U);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0].bits.a, 12U);
    const auto suppressed = kirimasim::kernel::prepareScanFormat("%*d %% %d", {});
    EXPECT_TRUE(suppressed.missingTargets);
    EXPECT_EQ(suppressed.format, "%*d %% ");
}

TEST(Scan, RuntimeFormatExtraTargetsAndMalformedFormats) {
    const std::array targets{kirimasim::kernel::ScanTarget::Integral,
                             kirimasim::kernel::ScanTarget::Other};
    const auto extra = kirimasim::kernel::prepareScanFormat("%d", targets);
    EXPECT_TRUE(extra.error.empty());
    EXPECT_FALSE(extra.missingTargets);
    for (const std::string format : {"%", "%5", "%q", "%t", "%v"}) {
        const auto plan = kirimasim::kernel::prepareScanFormat(format, targets);
        EXPECT_FALSE(plan.error.empty()) << format;
    }
}

TEST(Scan, PlusargRuntimePrefixAndConversion) {
    struct Row {
        const char* format;
        const char* prefix;
        char conv;
        kirimasim::kernel::ScanTarget type;
    };
    using Target = kirimasim::kernel::ScanTarget;
    const Row rows[] = {
        {"n=%d", "n=", 'd', Target::Integral},     {"hex=%0X", "hex=", 'h', Target::Integral},
        {"oct=%O", "oct=", 'o', Target::Integral}, {"bits=%b", "bits=", 'b', Target::Integral},
        {"%s", "", 's', Target::String},           {"text=%s", "text=", 's', Target::Integral},
        {"r=%E", "r=", 'f', Target::Real},         {"r=%g", "r=", 'f', Target::Real},
        {"r=%f", "r=", 'f', Target::Real}};
    for (const Row& row : rows) {
        const auto plan = kirimasim::kernel::preparePlusargFormat(row.format, row.type);
        EXPECT_TRUE(plan.error.empty()) << row.format;
        EXPECT_EQ(plan.prefix, row.prefix);
        EXPECT_EQ(plan.conv, row.conv);
    }
}

TEST(Scan, PlusargRuntimeTargetTypes) {
    using Target = kirimasim::kernel::ScanTarget;
    const std::array types{Target::Integral, Target::String, Target::Real, Target::Other};
    for (const char conv : std::string("bodhxsfeg")) {
        for (const Target type : types) {
            const std::string format = std::string("key=%") + conv;
            const bool accepts = conv == 'f' || conv == 'e' || conv == 'g' ? type == Target::Real
                                 : conv == 's' ? type == Target::Integral || type == Target::String
                                               : type == Target::Integral;
            const auto plan = kirimasim::kernel::preparePlusargFormat(format, type);
            EXPECT_EQ(plan.error.empty(), accepts) << format << " type " << static_cast<int>(type);
        }
    }
}

TEST(Scan, PlusargRuntimeMalformedFormats) {
    for (const std::string format : {"", "key=", "key=%", "key=%0", "key=%q", "key=%t", "key=%%",
                                     "key=%d%d", "key=%*d", "key=%*d%s", "key=%%=%d"}) {
        const auto plan = kirimasim::kernel::preparePlusargFormat(
            format, kirimasim::kernel::ScanTarget::Integral);
        EXPECT_FALSE(plan.error.empty()) << format;
    }
}

// Independent native-word payloads: the scanner never uses its formatter
// to build expected inputs (21.3.4.3, SYSTEM_TASKS.md 6.3).
std::string binaryWords(std::initializer_list<std::uint32_t> words) {
    std::string bytes;
    for (const std::uint32_t word : words) {
        const auto chars = std::bit_cast<std::array<char, 4>>(word);
        bytes.append(chars.data(), chars.size());
    }
    return bytes;
}

TEST(Scan, BinaryWidthsAndMasks) {
    for (const std::uint32_t width : {1U, 4U, 31U, 32U, 33U, 63U, 64U, 65U, 127U, 129U}) {
        for (const char conv : std::string("uzUZ")) {
            for (const bool explicitWidth : {false, true}) {
                const bool four = conv == 'z' || conv == 'Z';
                const std::uint32_t transfer = explicitWidth ? 33U : width;
                std::string bytes;
                for (std::uint32_t i = 0; i < (transfer + 31) / 32; ++i)
                    bytes += four ? binaryWords({0xabcdef01U + i, 0x12345678U + i})
                                  : binaryWords({0xabcdef01U + i});
                FromString input(bytes + "!");
                std::vector<ScanValue> values;
                std::string unread;
                const std::string format = std::string("%") + (explicitWidth ? "33" : "0") + conv;
                const std::array widths{width};
                ASSERT_EQ(scanInput(input, format, values, unread, widths), 1);
                EXPECT_EQ(input.taken(), bytes.size());
                EXPECT_TRUE(unread.empty());
                ASSERT_EQ(values.size(), 1U);
                ASSERT_EQ(values[0].wide.size(), (transfer + 63) / 64);
                for (std::uint32_t bit = 0; bit < values[0].wide.size() * 64; ++bit) {
                    const auto a = values[0].wide[bit / 64].a >> (bit % 64) & 1U;
                    const auto b = values[0].wide[bit / 64].b >> (bit % 64) & 1U;
                    EXPECT_EQ(a, bit < transfer ? (0xabcdef01U + bit / 32) >> (bit % 32) & 1U : 0);
                    EXPECT_EQ(b, four && bit < transfer
                                     ? (0x12345678U + bit / 32) >> (bit % 32) & 1U
                                     : 0);
                }
                EXPECT_EQ(values[0].bits.a, values[0].wide[0].a);
                EXPECT_EQ(values[0].bits.b, values[0].wide[0].b);
            }
        }
    }
}

TEST(Scan, BinaryShortFieldsAndSuppression) {
    for (const std::uint32_t width : {4U, 33U, 65U, 129U}) {
        for (const char conv : std::string("uz")) {
            const std::size_t size =
                (static_cast<std::size_t>(width) + 31) / 32 * (conv == 'u' ? 4U : 8U);
            for (std::size_t length = 0; length < size; ++length) {
                const std::string data(length, 'A');
                for (const bool preceding : {false, true}) {
                    FromString input((preceding ? "7:" : "") + data);
                    const std::array widths{32U, width};
                    std::vector<ScanValue> values;
                    std::string unread;
                    const std::string format = std::string(preceding ? "%d:" : "") + "%" + conv;
                    EXPECT_EQ(scanInput(input, format, values, unread,
                                        preceding
                                            ? std::span<const std::uint32_t>(widths)
                                            : std::span<const std::uint32_t>(widths).subspan(1)),
                              preceding ? 1 : -1);
                    EXPECT_EQ(values.size(), preceding ? 1U : 0U);
                    EXPECT_EQ(input.taken(), length + (preceding ? 2 : 0));
                    EXPECT_TRUE(unread.empty());
                }
            }
        }
    }
    const std::string data = binaryWords({0x20, 0x09, 0x00});
    auto values = scan(data + "Q", "%*u%*z%c", 1);
    ASSERT_EQ(values.size(), 1U);
    EXPECT_EQ(values[0].bits.a, static_cast<unsigned>('Q'));
    EXPECT_TRUE(scan(data.substr(0, 4), "%*u%z", 0).empty());
    values = scan(data, "%u%z", 2);
    EXPECT_EQ(values[0].bits.a, 0x20U);
    EXPECT_EQ(values[1].bits.a, 0x09U);
    EXPECT_EQ(values[1].bits.b, 0x00U);
    // Explicit format whitespace still skips; raw NUL never does.
    values = scan(" \t" + binaryWords({0x41410041}), " %u", 1);
    ASSERT_EQ(values.size(), 1U);
    EXPECT_EQ(values[0].bits.a, 0x41410041U);
}

TEST(Scan, BinarySuppressedWidthsAndRawBytes) {
    for (const std::uint32_t width : {4U, 33U, 65U, 129U}) {
        for (const char conv : std::string("uz")) {
            const std::size_t size =
                (static_cast<std::size_t>(width) + 31) / 32 * (conv == 'u' ? 4U : 8U);
            FromString input(std::string(size, '\0') + "Q");
            std::vector<ScanValue> values;
            std::string unread;
            const std::array widths{8U};
            ASSERT_EQ(scanInput(input, "%*" + std::to_string(width) + conv + "%c", values, unread,
                                widths),
                      1);
            ASSERT_EQ(values.size(), 1U);
            EXPECT_EQ(values[0].bits.a, static_cast<unsigned>('Q'));
            EXPECT_EQ(input.taken(), size + 1);
            EXPECT_TRUE(unread.empty());
        }
    }
    for (const unsigned char first : {0, 9, 10, 13, 32, 255}) {
        const std::string data{static_cast<char>(first), 'A', '\0', 'B'};
        const std::array<char, 4> bytes{static_cast<char>(first), 'A', '\0', 'B'};
        const std::uint32_t expected = std::bit_cast<std::uint32_t>(bytes);
        const auto values = scan(data, "%u", 1);
        ASSERT_EQ(values.size(), 1U);
        EXPECT_EQ(values[0].bits.a, expected);
        FromString input(data);
        std::vector<ScanValue> streamed;
        std::string unread;
        ASSERT_EQ(scanInput(input, "%u", streamed, unread), 1);
        ASSERT_EQ(streamed.size(), 1U);
        EXPECT_EQ(streamed[0].bits.a, expected);
        EXPECT_EQ(input.taken(), 4U);
    }
}

TEST(Scan, BinaryTypesAndPlusargExclusion) {
    using Target = kirimasim::kernel::ScanTarget;
    for (const char conv : std::string("uzUZ")) {
        std::vector<ScanConversion> conversions;
        std::string bad;
        ASSERT_TRUE(parseScanFormat(std::string("%*") + conv + "%" + conv, conversions, bad));
        ASSERT_EQ(conversions.size(), 2U);
        EXPECT_FALSE(conversions[0].assigns);
        for (const Target type : {Target::Integral, Target::String, Target::Real, Target::Other}) {
            const std::array targets{type};
            const auto plan =
                kirimasim::kernel::prepareScanFormat(std::string("%") + conv, targets);
            EXPECT_EQ(plan.error.empty(), type == Target::Integral);
            EXPECT_FALSE(kirimasim::kernel::preparePlusargFormat(std::string("key=%") + conv, type)
                             .error.empty());
        }
    }
}

}  // namespace
