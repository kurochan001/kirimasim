// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "fuzz.h"

#include <optional>

namespace kirimasim::test {
namespace {

class FuzzSetting : public ::testing::Test {
protected:
    void SetUp() override {
        const char* seed = std::getenv("KIRIMA_FUZZ_SEED");    // NOLINT(concurrency-mt-unsafe)
        const char* scale = std::getenv("KIRIMA_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
        if (seed != nullptr) seed_ = seed;
        if (scale != nullptr) scale_ = scale;
        unsetenv("KIRIMA_FUZZ_SEED");   // NOLINT(concurrency-mt-unsafe)
        unsetenv("KIRIMA_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
    }
    void TearDown() override {
        if (seed_)
            setenv("KIRIMA_FUZZ_SEED", seed_->c_str(), 1);  // NOLINT(concurrency-mt-unsafe)
        else
            unsetenv("KIRIMA_FUZZ_SEED");  // NOLINT(concurrency-mt-unsafe)
        if (scale_)
            setenv("KIRIMA_FUZZ_SCALE", scale_->c_str(), 1);  // NOLINT(concurrency-mt-unsafe)
        else
            unsetenv("KIRIMA_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
    }

private:
    std::optional<std::string> seed_;
    std::optional<std::string> scale_;
};

TEST_F(FuzzSetting, UnsetSeedKeepsFixedStreamAndExactFilter) {
    EXPECT_EQ(fuzzSeed(731), 731);
    EXPECT_EQ(fuzzScale(), 1);
    const auto text = fuzzSetting(fuzzSeed(731));
    EXPECT_NE(text.find("mixed_seed=731, KIRIMA_FUZZ_SEED=<unset>"), std::string::npos);
    EXPECT_NE(text.find("env -u KIRIMA_FUZZ_SEED -u KIRIMA_FUZZ_SCALE"), std::string::npos);
    EXPECT_NE(text.find("--gtest_filter=FuzzSetting.UnsetSeedKeepsFixedStreamAndExactFilter"),
              std::string::npos);
    EXPECT_NE(text.find(fuzzShellQuote(fuzzExecutable())), std::string::npos);
}

TEST_F(FuzzSetting, OriginalDateAndMixedSeedAreDistinct) {
    setenv("KIRIMA_FUZZ_SEED", "20261103", 1);  // NOLINT(concurrency-mt-unsafe)
    setenv("KIRIMA_FUZZ_SCALE", "100", 1);      // NOLINT(concurrency-mt-unsafe)
    const auto mixed = std::uint64_t{731} ^ (std::uint64_t{20261103} * 0x9e3779b97f4a7c15ULL);
    EXPECT_EQ(fuzzSeed(731), mixed);
    EXPECT_EQ(fuzzScale(), 100);
    const auto text = fuzzSetting(mixed);
    EXPECT_NE(text.find("mixed_seed=" + std::to_string(mixed)), std::string::npos);
    EXPECT_NE(text.find("'KIRIMA_FUZZ_SEED=20261103'"), std::string::npos);
    EXPECT_NE(text.find("'KIRIMA_FUZZ_SCALE=100'"), std::string::npos);
    EXPECT_EQ(text.find("'KIRIMA_FUZZ_SEED=" + std::to_string(mixed) + "'"), std::string::npos);
}

TEST_F(FuzzSetting, PreserveZeroAndPermissiveParsingAndQuoteOriginalValues) {
    setenv("KIRIMA_FUZZ_SEED", "0' ; literal", 1);  // NOLINT(concurrency-mt-unsafe)
    setenv("KIRIMA_FUZZ_SCALE", "0", 1);            // NOLINT(concurrency-mt-unsafe)
    EXPECT_EQ(fuzzSeed(731), 731);
    EXPECT_EQ(fuzzScale(), 1);
    const auto text = fuzzSetting(fuzzSeed(731));
    EXPECT_NE(text.find("'KIRIMA_FUZZ_SEED=0'\\'' ; literal'"), std::string::npos);
    EXPECT_NE(text.find("'KIRIMA_FUZZ_SCALE=0'"), std::string::npos);
    EXPECT_NE(text.find("parsed_scale=1"), std::string::npos);
}

}  // namespace
}  // namespace kirimasim::test
