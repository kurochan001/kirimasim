// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The size and the seed of the fuzz tests (TEST_SUITE.md, the valsem gate).
//
// By default the fuzzers run a fixed number of rounds from a fixed seed, so
// every CI run sees the same cases. The nightly job sets KIRIMA_FUZZ_SCALE=100
// and KIRIMA_FUZZ_SEED from the date. Only the tests read these variables.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace kirimasim::test {

/// How many times the default number of rounds to run (at least 1).
inline std::uint64_t fuzzScale() {
    const char* s = std::getenv("KIRIMA_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
    const std::uint64_t n = s != nullptr ? std::strtoull(s, nullptr, 10) : 1;
    return n == 0 ? 1 : n;
}

/// The seed: `fixed` unless KIRIMA_FUZZ_SEED is set, then mixed with it so that
/// the fuzzers that share one seed still differ.
inline std::uint64_t fuzzSeed(std::uint64_t fixed) {
    const char* s = std::getenv("KIRIMA_FUZZ_SEED");  // NOLINT(concurrency-mt-unsafe)
    if (s == nullptr) return fixed;
    return fixed ^ (std::strtoull(s, nullptr, 10) * 0x9e3779b97f4a7c15ULL);
}

/// Quote an argument for the POSIX shell, including literal seed strings.
inline std::string fuzzShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char c : value)
        quoted += c == '\'' ? "'\\''" : std::string(1, c);
    return quoted + "'";
}

inline std::string fuzzExecutable() {
#if defined(__APPLE__)
    std::uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) return buffer.data();
#elif defined(__linux__)
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return path.string();
#endif
    return "<unit-test-binary>";
}

/// For failure messages: the original environment seed is not the mixed seed.
/// Do not change the permissive decimal parsing or mixing above: CI and the
/// date-seeded nightly must keep exactly the same random streams.
inline std::string fuzzSetting(std::uint64_t seed) {
    const char* rawSeed = std::getenv("KIRIMA_FUZZ_SEED");    // NOLINT(concurrency-mt-unsafe)
    const char* rawScale = std::getenv("KIRIMA_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string filter =
        info != nullptr ? std::string(info->test_suite_name()) + "." + info->name() : "*";
    std::string command = "env -u KIRIMA_FUZZ_SEED -u KIRIMA_FUZZ_SCALE";
    if (rawSeed != nullptr)
        command += " " + fuzzShellQuote("KIRIMA_FUZZ_SEED=" + std::string(rawSeed));
    if (rawScale != nullptr)
        command += " " + fuzzShellQuote("KIRIMA_FUZZ_SCALE=" + std::string(rawScale));
    command +=
        " " + fuzzShellQuote(fuzzExecutable()) + " " + fuzzShellQuote("--gtest_filter=" + filter);
    return "mixed_seed=" + std::to_string(seed) +
           ", KIRIMA_FUZZ_SEED=" + (rawSeed != nullptr ? fuzzShellQuote(rawSeed) : "<unset>") +
           ", parsed_scale=" + std::to_string(fuzzScale()) + "; replay: " + command;
}

}  // namespace kirimasim::test
