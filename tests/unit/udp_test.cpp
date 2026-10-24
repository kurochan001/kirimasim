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
        std::vector<std::uint8_t> pc(n);
        std::vector<std::uint8_t> cc(n);
        for (unsigned i = 0; i < n; ++i) {
            prev.push_back(kValues[rng() % 4]);
            cur.push_back(kValues[rng() % 4]);
            pc[i] = code(prev[i]);
            cc[i] = code(cur[i]);
        }
        if (!sequential) {
            ASSERT_EQ(logic(k::udpOutput(t, cc.data())), v::udpOutput(rows, cur))
                << "round " << round;
            continue;
        }
        const Logic state = kValues[rng() % 3];
        ASSERT_EQ(logic(k::udpStep(t, code(state), pc.data(), cc.data())),
                  v::udpStep(rows, state, prev, cur))
            << "round " << round;
    }
}

TEST(Udp, WideInputsMatchIndependentReference) {
    std::mt19937_64 rng{kirimasim::test::fuzzSeed(0x75647077696465ULL)};
    constexpr Logic values[] = {L0, L1, LX, LZ};
    for (const unsigned n : {31U, 32U, 33U, 63U, 64U, 65U, 127U, 128U, 129U, 257U}) {
        for (std::uint64_t round = 0; round < 64 * kirimasim::test::fuzzScale(); ++round) {
            const bool sequential = round % 2 != 0;
            const auto rows = randomTable(rng, n, sequential);
            const auto table = compile(rows, n);
            std::vector<Logic> previous(n), current(n);
            std::vector<std::uint8_t> pc(n), cc(n);
            for (unsigned i = 0; i < n; ++i) {
                previous[i] = values[rng() % 4];
                current[i] = previous[i];
                pc[i] = code(previous[i]);
                cc[i] = code(current[i]);
            }
            for (const unsigned i : {0U, n / 2, n - 1}) {
                current[i] = values[rng() % 4];
                cc[i] = code(current[i]);
            }
            const auto readPrev = [&](unsigned i) { return pc[i]; };
            const auto readCur = [&](unsigned i) { return cc[i]; };
            SCOPED_TRACE(n);
            SCOPED_TRACE(round);
            if (!sequential) {
                const auto expected = v::udpOutput(rows, current);
                EXPECT_EQ(logic(k::udpOutput(table, cc.data())), expected);
                EXPECT_EQ(logic(k::udpOutputRead(table, readCur)), expected);
            } else {
                const auto state = values[rng() % 3];
                const auto expected = v::udpStep(rows, state, previous, current);
                EXPECT_EQ(logic(k::udpStep(table, code(state), pc.data(), cc.data())), expected);
                EXPECT_EQ(logic(k::udpStepRead(table, code(state), readPrev, readCur)), expected);
            }
        }
    }
}

TEST(Udp, WidePrefixOrderAndLastTerminal) {
    for (const unsigned n : {33U, 65U, 129U, 257U, 65537U}) {
        std::string earlier(n, '?'), later(n, '?');
        earlier[0] = 'r';
        earlier[n - 1] = '1';
        later[0] = '1';
        later[n - 1] = 'r';
        const std::vector<UdpRow> rows{{earlier, '?', '0'}, {later, 'x', '1'}};
        const auto table = compile(rows, n);
        std::vector<Logic> previous(n, L0), current(n, L0);
        current[0] = L1;
        current[n - 1] = L1;
        std::vector<std::uint8_t> pc(n), cc(n);
        cc[0] = cc[n - 1] = 1;
        // The early edge must not see the future last-terminal value.
        // The last edge does see the already committed first-terminal value.
        const auto expected = v::udpStep(rows, L0, previous, current);
        ASSERT_EQ(expected, L1);
        EXPECT_EQ(logic(k::udpStep(table, 0, pc.data(), cc.data())), expected) << n;
        EXPECT_EQ(
            logic(k::udpStepRead(
                table, 0, [&](unsigned i) { return pc[i]; }, [&](unsigned i) { return cc[i]; })),
            expected)
            << n;
        // Z and X are one UDP input value, so changing between them is no edge.
        pc[0] = 3;
        cc[0] = 2;
        pc[n - 1] = cc[n - 1] = 2;
        EXPECT_EQ(k::udpStep(table, 1, pc.data(), cc.data()), 1) << n;
    }
}

TEST(Udp, WideRowsReadBoundaryColumnsAndLevelPriority) {
    for (const unsigned n : {33U, 65U, 129U, 257U}) {
        for (const unsigned column : {0U, 31U, 32U, n - 1}) {
            std::vector<UdpRow> rows;
            for (const char symbol : {'0', '1', 'x'}) {
                std::string fields(n, '?');
                fields[column] = symbol;
                rows.push_back({fields, 0, symbol == '1' ? '1' : '0'});
            }
            const auto table = compile(rows, n);
            for (const auto value : {L0, L1, LX, LZ}) {
                std::vector<Logic> input(n, LZ);
                input[column] = value;
                std::vector<std::uint8_t> codes(n, 3);
                codes[column] = code(value);
                const auto expected = v::udpOutput(rows, input);
                ASSERT_EQ(expected, value == L1 ? L1 : L0);
                EXPECT_EQ(logic(k::udpOutput(table, codes.data())), expected) << n << ':' << column;
                EXPECT_EQ(logic(k::udpOutputRead(table, [&](unsigned i) { return codes[i]; })),
                          expected)
                    << n << ':' << column;
            }
        }
        std::string edge(n, '?'), level(n, '?');
        edge[n - 1] = 'r';
        level[n - 2] = '1';
        const std::vector<UdpRow> rows{{edge, '?', '1'}, {level, '?', '0'}};
        const auto table = compile(rows, n);
        std::vector<Logic> previous(n, L0), current(n, L0);
        previous[n - 2] = current[n - 2] = L1;
        current[n - 1] = L1;
        std::vector<std::uint8_t> pc(n), cc(n);
        pc[n - 2] = cc[n - 2] = cc[n - 1] = 1;
        ASSERT_EQ(v::udpStep(rows, L1, previous, current), L0);
        EXPECT_EQ(k::udpStep(table, 1, pc.data(), cc.data()), 0) << n;
    }
}

}  // namespace
