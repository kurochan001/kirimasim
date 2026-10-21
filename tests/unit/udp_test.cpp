// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// User-defined primitives (VALUE_SEMANTICS.md 5.9): the valsem reference on
// tables worked out from chapter 29, and the kernel's compiled tables
// (kernel/udp.h) compared with the reference on random tables.

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/udp.h"
#include "kirimasim/valsem/udp.h"

namespace {

namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using v::Logic;
using v::UdpRow;

constexpr Logic L0 = Logic::Zero;
constexpr Logic L1 = Logic::One;
constexpr Logic LX = Logic::X;
constexpr Logic LZ = Logic::Z;

// A rising-edge D flip-flop with data first (29.6): the edge of the clock
// takes the data; a fall or a change of the data keeps the state.
std::vector<UdpRow> dff() {
    return {
        {"0(01)", '?', '0'}, {"1(01)", '?', '1'}, {"?(?0)", '?', '-'},
        {"0(0x)", '0', '-'}, {"1(0x)", '1', '-'}, {"*?", '?', '-'},
    };
}

TEST(Udp, Combinational) {
    const std::vector<UdpRow> mux = {{"0?0", 0, '0'}, {"1?0", 0, '1'}, {"?01", 0, '0'},
                                     {"?11", 0, '1'}, {"00?", 0, '0'}, {"11?", 0, '1'}};
    EXPECT_EQ(v::udpOutput(mux, {L1, L0, L0}), L1);
    EXPECT_EQ(v::udpOutput(mux, {L1, L0, L1}), L0);
    EXPECT_EQ(v::udpOutput(mux, {L1, L1, LX}), L1);  // both data equal: the select is ignored
    EXPECT_EQ(v::udpOutput(mux, {L1, L0, LZ}), LX);  // no row matches; z reads as x
}

TEST(Udp, EdgeSensitive) {
    const std::vector<UdpRow> kDff = dff();
    EXPECT_EQ(v::udpNext(kDff, LX, {L1, L1}, 1, L0), L1);  // rise takes the data
    EXPECT_EQ(v::udpNext(kDff, L1, {L0, L0}, 1, L1), L1);  // fall keeps
    EXPECT_EQ(v::udpNext(kDff, L1, {L0, L1}, 0, L1), L1);  // data change keeps
    EXPECT_EQ(v::udpNext(kDff, L1, {L1, L1}, 1, LX), LX);  // x to 1 is not listed: x
    // Both inputs change at once: data first, so the rise takes the new
    // data (VALUE_SEMANTICS.md 5.9).
    EXPECT_EQ(v::udpStep(kDff, L0, {L0, L0}, {L1, L1}), L1);
}

TEST(Udp, LevelRowsDecide) {
    // 29.9: a level row that matches decides over an edge row.
    const std::vector<UdpRow> t = {{"r0", '?', '1'}, {"?1", '?', '0'}, {"r?", '?', '1'}};
    EXPECT_EQ(v::udpNext(t, L0, {L1, L1}, 0, L0), L0);
    EXPECT_EQ(v::udpNext(t, L0, {L1, L0}, 0, L0), L1);
}

// Random rows of random symbols, at most one edge each.
std::vector<UdpRow> randomTable(std::mt19937_64& rng, unsigned n, bool sequential) {
    static constexpr char kLevel[] = {'0', '1', 'x', '?', 'b'};
    static const std::string kEdges[] = {"r", "f", "p", "n", "*"};
    static constexpr char kOut[] = {'0', '1', 'x', '-'};
    std::vector<UdpRow> rows;
    const auto count = static_cast<unsigned>(1 + rng() % 12);
    for (unsigned r = 0; r < count; ++r) {
        UdpRow row;
        const unsigned edge = sequential && rng() % 2 == 0 ? static_cast<unsigned>(rng() % n) : n;
        for (unsigned i = 0; i < n; ++i) {
            if (i != edge) {
                row.inputs += kLevel[rng() % 5];
            } else if (rng() % 2 == 0) {
                row.inputs += kEdges[rng() % 5];
            } else {
                row.inputs += '(';
                row.inputs += kLevel[rng() % 5];
                row.inputs += kLevel[rng() % 5];
                row.inputs += ')';
            }
        }
        row.state = sequential ? kLevel[rng() % 5] : 0;
        row.output = kOut[rng() % (sequential ? 4 : 3)];
        rows.push_back(row);
    }
    return rows;
}

k::UdpTable compile(const std::vector<UdpRow>& rows, unsigned n) {
    k::UdpTable t;
    t.inputs = n;
    for (const UdpRow& r : rows)
        t.rows.push_back(k::compileUdpRow(r.inputs, r.state, r.output));
    return t;
}

std::uint8_t code(Logic l) { return l == L0 ? 0 : l == L1 ? 1 : l == LZ ? 3 : 2; }
Logic logic(std::uint8_t c) { return c == 0 ? L0 : c == 1 ? L1 : LX; }

TEST(Udp, KernelMatchesReference) {
    const std::uint64_t seed = kirimasim::test::fuzzSeed(0x756470ULL);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    std::mt19937_64 rng{seed};
    const std::uint64_t rounds = 4'000 * kirimasim::test::fuzzScale();
    constexpr Logic kValues[] = {L0, L1, LX, LZ};
    for (std::uint64_t round = 0; round < rounds; ++round) {
        const auto n = static_cast<unsigned>(1 + rng() % 4);
        const bool sequential = rng() % 2 == 0;
        const std::vector<UdpRow> rows = randomTable(rng, n, sequential);
        const k::UdpTable t = compile(rows, n);
        std::vector<Logic> prev;
        std::vector<Logic> cur;
        std::uint8_t pc[k::kUdpMaxInputs];
        std::uint8_t cc[k::kUdpMaxInputs];
        for (unsigned i = 0; i < n; ++i) {
            prev.push_back(kValues[rng() % 4]);
            cur.push_back(kValues[rng() % 4]);
            pc[i] = code(prev[i]);
            cc[i] = code(cur[i]);
        }
        if (!sequential) {
            ASSERT_EQ(logic(k::udpOutput(t, cc)), v::udpOutput(rows, cur)) << "round " << round;
            continue;
        }
        const Logic state = kValues[rng() % 3];
        ASSERT_EQ(logic(k::udpStep(t, code(state), pc, cc)), v::udpStep(rows, state, prev, cur))
            << "round " << round;
    }
}

}  // namespace
