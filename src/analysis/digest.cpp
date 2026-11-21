// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kirimasim/analysis/tables.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace kirimasim::analysis {
namespace {
using State = std::array<std::uint32_t, 8>;
constexpr std::array<std::uint32_t, 64> roundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

void compress(State& state, const std::uint8_t* block) {
    std::array<std::uint32_t, 64> word{};
    for (std::size_t i = 0; i < 16; ++i) {
        const auto* p = block + 4 * i;
        word[i] = (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) |
                  (std::uint32_t{p[2]} << 8) | p[3];
    }
    for (unsigned i = 16; i < word.size(); ++i) {
        const auto x = word[i - 15], y = word[i - 2];
        const auto sigma0 = std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
        const auto sigma1 = std::rotr(y, 17) ^ std::rotr(y, 19) ^ (y >> 10);
        word[i] = word[i - 16] + sigma0 + word[i - 7] + sigma1;
    }
    auto [a, b, c, d, e, f, g, h] = state;
    for (unsigned i = 0; i < word.size(); ++i) {
        const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const auto t1 = h + sum1 + ((e & f) ^ (~e & g)) + roundConstants[i] + word[i];
        const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const auto t2 = sum0 + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    const State result{a, b, c, d, e, f, g, h};
    for (unsigned i = 0; i < state.size(); ++i)
        state[i] += result[i];
}
}  // namespace

Digest contentDigest(std::string_view bytes) {
    State state{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    const auto length = static_cast<std::uint64_t>(bytes.size()) * 8;
    while (bytes.size() >= 64) {
        compress(state, reinterpret_cast<const std::uint8_t*>(bytes.data()));
        bytes.remove_prefix(64);
    }
    std::array<std::uint8_t, 128> tail{};
    if (!bytes.empty()) std::memcpy(tail.data(), bytes.data(), bytes.size());
    tail[bytes.size()] = 0x80;
    const unsigned end = bytes.size() < 56 ? 64 : 128;
    for (unsigned i = 0; i < 8; ++i)
        tail[end - 1 - i] = static_cast<std::uint8_t>(length >> (8 * i));
    compress(state, tail.data());
    if (end == 128) compress(state, tail.data() + 64);
    Digest digest{};
    for (unsigned i = 0; i < digest.size(); ++i)
        digest[i] = static_cast<std::uint8_t>(state[i / 4] >> (24 - 8 * (i % 4)));
    return digest;
}
}  // namespace kirimasim::analysis
