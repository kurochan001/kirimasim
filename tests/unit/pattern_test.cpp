// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/pattern.h"
#include "kirimasim/valsem/value.h"

namespace {
using namespace kirimasim::kernel;
using Kind = PatternValue::Kind;

PatternValue integer(const std::uint64_t* words, unsigned width = 32, bool four = false,
                     bool sign = false) {
    PatternValue value;
    value.integral = {words, width, four, sign};
    return value;
}
PatternValue text(std::string_view bytes) {
    PatternValue value;
    value.kind = Kind::String;
    value.text = bytes;
    return value;
}
PatternValue array(std::span<const PatternValue> elements) {
    PatternValue value;
    value.kind = Kind::Array;
    value.children = elements;
    return value;
}

// Independent reader for the relevant SV literal grammar (5.9). In particular,
// unknown escapes denote the escaped character: C's backspace/CR escapes would
// decode as letters, so merely accepting C-style quoting cannot pass this test.
std::optional<std::string> literal(std::string_view source) {
    if (source.size() < 2 || source.front() != '"' || source.back() != '"') return std::nullopt;
    std::string value;
    for (std::size_t i = 1; i + 1 < source.size(); ++i) {
        char ch = source[i];
        if (ch == '"' || ch == '\n') return std::nullopt;
        if (ch == '\\') {
            if (++i + 1 >= source.size()) return std::nullopt;
            ch = source[i];
            if (ch >= '0' && ch <= '7') {
                unsigned code = static_cast<unsigned>(ch - '0');
                for (unsigned j = 1;
                     j < 3 && i + 2 < source.size() && source[i + 1] >= '0' && source[i + 1] <= '7';
                     ++j)
                    code = code * 8U + static_cast<unsigned>(source[++i] - '0');
                if (code > 255) return std::nullopt;
                ch = static_cast<char>(code);
            } else {
                switch (ch) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'v': ch = '\v'; break;
                    case 'f': ch = '\f'; break;
                    case 'a': ch = '\a'; break;
                    default: break;
                }
            }
        }
        value += ch;
    }
    return value;
}

class LimitedMemory final : public std::pmr::memory_resource {
public:
    explicit LimitedMemory(std::size_t budget) : remaining_(budget) {}
    std::size_t live = 0;

private:
    std::size_t remaining_;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining_ == 0) throw std::bad_alloc{};
        --remaining_;
        auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        live += bytes;
        return result;
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        live -= bytes;
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(Pattern, ScalarWidthsSignednessAndUnknowns) {
    const std::array<std::uint64_t, 2> negative{{~std::uint64_t{2}, 1}};
    EXPECT_EQ(formatPattern(integer(negative.data(), 65, false, true)).text, "-3");
    EXPECT_EQ(formatPattern(integer(negative.data(), 65)).text, "36893488147419103229");
    const std::array<std::uint64_t, 2> unknown{{~std::uint64_t{0}, ~std::uint64_t{0}}};
    const std::array<std::uint64_t, 2> highz{{0, ~std::uint64_t{0}}};
    EXPECT_EQ(formatPattern(integer(unknown.data(), 32, true)).text, "x");
    EXPECT_EQ(formatPattern(integer(highz.data(), 32, true)).text, "z");
    PatternValue real;
    real.kind = Kind::Real;
    real.real = 1.25;
    EXPECT_EQ(formatPattern(real).text, "1.25");
    real.real = -0.0;
    EXPECT_EQ(formatPattern(real).text, "-0");
}

TEST(Pattern, WideEnumLookupMatchesReferenceCaseEquality) {
    for (const unsigned width : {1U, 7U, 32U, 64U, 65U, 129U, 257U}) {
        const auto count = (std::size_t{width} + 63U) / 64U;
        for (unsigned mode = 0; mode < 12; ++mode) {
            SCOPED_TRACE(width);
            SCOPED_TRACE(mode);
            const bool four = mode % 2U != 0;
            const bool candidateFour = mode % 3U != 0;
            std::vector<std::uint64_t> a(count, 0), b(count, 0), ca(count, 0), cb(count, 0);
            a[0] = 1;
            if (four && mode % 4U == 1) {
                for (std::size_t i = 0; i < count; ++i) {
                    a[i] = ~std::uint64_t{0};
                    b[i] = ~std::uint64_t{0};
                }
            }
            ca = a;
            cb = b;
            if (!candidateFour) cb.assign(count, 0);
            if (mode % 4U == 3) ca[0] ^= 1U;
            std::vector<std::uint64_t> storage(count * (four ? 2U : 1U));
            std::vector<std::uint64_t> candidate(count * (candidateFour ? 2U : 1U));
            for (std::size_t i = 0; i < count; ++i) {
                storage[i * (four ? 2U : 1U)] = a[i];
                if (four) storage[i * 2U + 1U] = b[i];
                candidate[i * (candidateFour ? 2U : 1U)] = ca[i];
                if (candidateFour) candidate[i * 2U + 1U] = cb[i];
            }
            // Padding is not part of the value, even when it looks like X/Z.
            if (width % 64U != 0 && candidateFour)
                candidate.back() |= ~((std::uint64_t{1} << (width % 64U)) - 1U);
            auto value = integer(storage.data(), width, four);
            const PatternEnumName name{{candidate.data(), width, candidateFour, false}, {"MATCH"}};
            value.kind = Kind::Enumeration;
            value.enumerators = std::span(&name, 1);
            const bool matches = kirimasim::valsem::toBinary(kirimasim::valsem::ceq(
                                     kirimasim::valsem::fromWords(a, b, width, false),
                                     kirimasim::valsem::fromWords(ca, cb, width, false))) == "1";
            const auto result = formatPattern(value);
            ASSERT_TRUE(result);
            const auto fallback = four && mode % 4U == 1 ? "x" : "1";
            EXPECT_EQ(result.text, matches ? "MATCH" : fallback);
        }
    }
}

TEST(Pattern, NestedRecordsAndPositionalMembers) {
    const std::array<std::uint64_t, 2> numbers{{7, 9}};
    const std::array<PatternValue, 2> items{{integer(&numbers[0]), integer(&numbers[1])}};
    const std::array<PatternValue, 2> fields{{array(items), text("quoted\"value")}};
    const std::array<PatternName, 2> names{{{"values"}, {"name"}}};
    PatternValue record;
    record.kind = Kind::Record;
    record.names = names;
    record.children = fields;
    EXPECT_EQ(formatPattern(record).text, "'{values:'{7, 9}, name:\"quoted\\\"value\"}");
    EXPECT_EQ(formatPattern(record, true).text, "'{'{7, 9}, \"quoted\\\"value\"}");
    const std::array<PatternValue, 1> outer{{record}};
    EXPECT_EQ(formatPattern(array(outer), true).text, "'{'{'{7, 9}, \"quoted\\\"value\"}}");
}

TEST(Pattern, SourceIdentifierSpellingIncludesEscapedAndReservedNames) {
    const std::uint64_t n = 9;
    const std::array<PatternValue, 3> fields{{integer(&n), integer(&n), integer(&n)}};
    const std::array<PatternName, 3> names{{{"int", true}, {"has-dash", true}, {"$field", true}}};
    PatternValue record;
    record.kind = Kind::Record;
    record.names = names;
    record.children = fields;
    EXPECT_EQ(formatPattern(record).text, "'{\\int :9, \\has-dash :9, \\$field :9}");
    const PatternEnumName named{{&n, 32, false, false}, {"end", true}};
    auto value = integer(&n);
    value.kind = Kind::Enumeration;
    value.enumerators = std::span(&named, 1);
    EXPECT_EQ(formatPattern(value).text, "\\end ");
}

TEST(Pattern, StringBytesRoundTripThroughSvLiteralRules) {
    std::string bytes;
    for (unsigned i = 0; i < 256; ++i)
        bytes += static_cast<char>(i);
    bytes += "7\b7\r7\"\\";
    const auto result = formatPattern(text(bytes));
    ASSERT_TRUE(result);
    const auto parsed = literal(result.text);
    if (!parsed) {
        FAIL() << "renderer did not produce a valid string literal";
        return;
    }
    EXPECT_EQ(*parsed, bytes);
    EXPECT_EQ(result.text.find("\\b"), std::string::npos);
    EXPECT_EQ(result.text.find("\\r"), std::string::npos);
}

TEST(Pattern, UnionOnlyFirstDeclaredMemberAndTaggedPayload) {
    const std::uint64_t n = 7;
    const std::array<PatternValue, 2> members{{integer(&n), text("not the first member")}};
    const std::array<PatternName, 2> names{{{"first"}, {"second"}}};
    PatternValue value;
    value.kind = Kind::Union;
    value.names = names;
    value.children = members;
    EXPECT_EQ(formatPattern(value).text, "'{first:7}");
    EXPECT_EQ(formatPattern(value, true).text, "'{7}");
    value = {};
    value.kind = Kind::TaggedUnion;
    value.tag = {"payload"};
    value.children = std::span(members).first(1);
    EXPECT_EQ(formatPattern(value).text, "payload:7");
    EXPECT_EQ(formatPattern(value, true).text, "payload:7");
}

TEST(Pattern, AssociativeEntriesFollowProvidedSemanticKeyOrder) {
    const std::array<std::uint64_t, 2> n{{7, 9}};
    const std::array<PatternValue, 2> keys{{text("alpha"), text("omega")}};
    const std::array<PatternValue, 2> values{{integer(&n[0]), integer(&n[1])}};
    PatternValue value;
    value.kind = Kind::Associative;
    value.keys = keys;
    value.children = values;
    EXPECT_EQ(formatPattern(value).text, "'{\"alpha\":7, \"omega\":9}");
    EXPECT_EQ(formatPattern(value, true).text, "'{\"alpha\":7, \"omega\":9}");
}

TEST(Pattern, EmptyContainersAndRecords) {
    for (const auto kind : {Kind::Array, Kind::Associative, Kind::Record}) {
        PatternValue value;
        value.kind = kind;
        const auto result = formatPattern(value);
        ASSERT_TRUE(result);
        EXPECT_EQ(result.text, "'{}");
    }
}

TEST(Pattern, HandleUsesSuppliedContextDisplayOrdinal) {
    PatternValue value;
    value.kind = Kind::Handle;
    EXPECT_EQ(formatPattern(value).text, "null");
    value.text = "pkg::Item";
    value.ordinal = 37;
    EXPECT_EQ(formatPattern(value).text, "pkg::Item@37");
    value.ordinal = 0;
    EXPECT_EQ(formatPattern(value).text, "null");
}

TEST(Pattern, LargeOutputHasNo1024CharacterTruncation) {
    const std::string bytes(8192, 'x');
    const std::array<PatternValue, 2> values{{text(bytes), text(bytes)}};
    std::string expected = "'{\"";
    expected += bytes;
    expected += "\", \"";
    expected += bytes;
    expected += "\"}";
    const auto result = formatPattern(array(values));
    ASSERT_TRUE(result);
    EXPECT_EQ(result.text, expected);
}

TEST(Pattern, DeepTraversalDoesNotUseTheNativeCallStack) {
    constexpr std::size_t depth = 20000;
    const std::uint64_t n = 7;
    std::vector<PatternValue> values(depth + 1);
    values.back() = integer(&n);
    for (std::size_t i = 0; i < depth; ++i)
        values[i] = array(std::span(&values[i + 1], 1));
    const auto result = formatPattern(values.front());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.text.size(), depth * 3 + 1);
    std::string prefix;
    for (std::size_t i = 0; i < depth; ++i)
        prefix += "'{";
    EXPECT_EQ(result.text.substr(0, depth * 2), prefix);
    EXPECT_EQ(result.text.substr(depth * 2), "7" + std::string(depth, '}'));
}

TEST(Pattern, CyclesFailWithoutOutputButRepeatedSubvaluesAreLegal) {
    PatternValue cycle;
    cycle = array(std::span(&cycle, 1));
    LimitedMemory memory(64);
    const auto result = formatPattern(cycle, false, &memory);
    EXPECT_EQ(result.error, PatternError::Cycle);
    EXPECT_TRUE(result.text.empty());
    EXPECT_EQ(memory.live, 0U);
    const std::uint64_t n = 7;
    const auto leaf = integer(&n);
    const auto shared = array(std::span(&leaf, 1));
    const auto wrapped = array(std::span(&shared, 1));
    const std::array<PatternValue, 2> children{{wrapped, wrapped}};
    EXPECT_EQ(formatPattern(array(children)).text, "'{'{'{7}}, '{'{7}}}");
}

TEST(Pattern, MalformedViewsFailWithoutOutput) {
    std::vector<PatternValue> bad(8);
    bad[0] = {};
    bad[1].kind = static_cast<Kind>(255);
    bad[2].kind = Kind::Handle;
    bad[2].ordinal = 1;
    bad[3].kind = Kind::Union;
    bad[4].kind = Kind::TaggedUnion;
    const PatternValue child = text("valid child");
    bad[5].kind = Kind::Record;
    bad[5].children = std::span(&child, 1);
    bad[6].kind = Kind::Associative;
    bad[6].children = std::span(&child, 1);
    bad[7] = text("scalar must not have children");
    bad[7].children = std::span(&child, 1);
    for (const auto& value : bad) {
        const auto result = formatPattern(value);
        EXPECT_EQ(result.error, PatternError::InvalidValue);
        EXPECT_TRUE(result.text.empty());
    }
}

TEST(Pattern, ScratchFailuresReturnNoPartialOutputAndReleaseStorage) {
    const std::string bytes(4096, 'x');
    const std::array<PatternValue, 2> values{{text(bytes), text(bytes)}};
    std::string expected = "'{\"";
    expected += bytes;
    expected += "\", \"";
    expected += bytes;
    expected += "\"}";
    unsigned failures = 0;
    bool success = false;
    for (std::size_t budget = 0; budget < 32; ++budget) {
        LimitedMemory memory(budget);
        const auto result = formatPattern(array(values), false, &memory);
        EXPECT_EQ(memory.live, 0U);
        if (result) {
            EXPECT_EQ(result.text, expected);
            success = true;
            break;
        }
        ++failures;
        EXPECT_EQ(result.error, PatternError::ResourceLimit);
        EXPECT_TRUE(result.text.empty());
    }
    EXPECT_GT(failures, 3U);
    EXPECT_TRUE(success);
}
}  // namespace
