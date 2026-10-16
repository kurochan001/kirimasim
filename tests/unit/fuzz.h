// The size and the seed of the fuzz tests (TEST_SUITE.md, the valsem gate).
//
// By default the fuzzers run a fixed number of rounds from a fixed seed, so
// every CI run sees the same cases. The nightly job sets HS_FUZZ_SCALE=100
// and HS_FUZZ_SEED from the date. Only the tests read these variables.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

namespace kirimasim::test {

/// How many times the default number of rounds to run (at least 1).
inline std::uint64_t fuzzScale() {
    const char* s = std::getenv("HS_FUZZ_SCALE");  // NOLINT(concurrency-mt-unsafe)
    const std::uint64_t n = s != nullptr ? std::strtoull(s, nullptr, 10) : 1;
    return n == 0 ? 1 : n;
}

/// The seed: `fixed` unless HS_FUZZ_SEED is set, then mixed with it so that
/// the fuzzers that share one seed still differ.
inline std::uint64_t fuzzSeed(std::uint64_t fixed) {
    const char* s = std::getenv("HS_FUZZ_SEED");  // NOLINT(concurrency-mt-unsafe)
    if (s == nullptr) return fixed;
    return fixed ^ (std::strtoull(s, nullptr, 10) * 0x9e3779b97f4a7c15ULL);
}

/// For failure messages: how to run the same cases again.
inline std::string fuzzSetting(std::uint64_t seed) {
    return "seed " + std::to_string(seed) + ", HS_FUZZ_SCALE=" + std::to_string(fuzzScale());
}

}  // namespace kirimasim::test
