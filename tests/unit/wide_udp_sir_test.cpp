// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independent SIR operands exercise the VM's wide UDP transport and verifier.
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/valsem/udp.h"
#include "kirimasim/valsem/value.h"
#include "kirimasim/vm/vm.h"
#include "verified_run.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;

s::Design udpDesign(unsigned n, bool sequential) {
    s::Design d;
    d.sources = {"wide_udp.sv"};
    d.instances = {{"top", "top", s::kNone}};
    d.slots = {{"top.result", s::u4(1)}};
    s::TypeEntry table;
    table.name = "wide";
    table.kind = s::TypeEntryKind::Udp;
    table.inputs = n;
    table.sequential = sequential;
    std::string row(n, '?');
    row[n - 1] = sequential ? 'r' : '1';
    table.rows = {{row, sequential ? '?' : char{0}, '1'}};
    d.types.push_back(std::move(table));
    const auto constant = [&](unsigned code) {
        const auto id = static_cast<s::ConstId>(d.constants.size());
        const std::uint64_t a = code == 3 ? 0 : code & 1;
        const std::uint64_t b = code >= 2 ? 1 : 0;
        d.constants.push_back({s::u4(1), {a}, {b}, {}});
        return s::Operand::constant(id);
    };
    const auto zero = constant(0), one = constant(1);
    std::vector<s::Operand> operands{s::Operand::typeRow(s::TypeId{0})};
    if (sequential) {
        operands.push_back(zero);
        operands.insert(operands.end(), n, zero);
    }
    operands.insert(operands.end(), n, zero);
    operands.back() = one;
    s::Process p;
    p.name = "top.check";
    p.loc = {0, 1, 1};
    p.kind = s::ProcessKind::Initial;
    p.numTemps = 1;
    p.writes = {{s::SlotId{0}}};
    p.blocks.resize(1);
    p.blocks[0].instrs = {
        {sequential ? s::Op::UdpSeq : s::Op::Udp, s::u4(1), s::TempId{0}, operands, {0, 1, 1}},
        {s::Op::Store,
         s::voidType(),
         s::kNone,
         {s::Operand::slot(s::SlotId{0}), s::Operand::temp(s::TempId{0})},
         {0, 1, 1}},
        {s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}}};
    d.processes.push_back(std::move(p));
    return d;
}

void checkSmallUdpValues() {
    namespace v = kirimasim::valsem;
    const std::array states{v::Logic::Zero, v::Logic::One, v::Logic::X, v::Logic::Z};
    // VALUE_SEMANTICS 5.9: all four input states through constants and
    // loaded temps; four terminals also exercise the ordinary interpreter.
    for (const unsigned n : {1U, 2U, 3U, 4U}) {
        std::string high(n, '?'), low(n, '?');
        high.back() = '1';
        low.back() = '0';
        const std::vector<v::UdpRow> rows{{high, 0, '1'}, {low, 0, '0'}};
        for (unsigned bits = 0; bits < (1U << (2 * n)); ++bits)
            for (const bool loaded : {false, true}) {
                SCOPED_TRACE(::testing::Message() << n << '/' << bits << '/' << loaded);
                auto d = udpDesign(n, false);
                d.types[0].rows = {{high, 0, '1'}, {low, 0, '0'}};
                auto& process = d.processes[0];
                auto& instructions = process.blocks[0].instrs;
                std::vector<s::Instr> prefix;
                std::vector<v::Logic> inputs;
                for (unsigned i = 0; i < n; ++i) {
                    const auto state = (bits >> (2 * i)) & 3U;
                    inputs.push_back(states[state]);
                    const auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
                    d.constants.push_back(
                        {s::u4(1), {state == 1 || state == 2 ? 1U : 0U}, {state >= 2 ? 1U : 0U}});
                    auto operand = s::Operand::constant(id);
                    if (loaded) {
                        const auto slot = s::SlotId{static_cast<std::uint32_t>(d.slots.size())};
                        d.slots.push_back({"top.input" + std::to_string(i), s::u4(1)});
                        process.writes.push_back({slot});
                        const auto temp = s::TempId{i + 1};
                        prefix.push_back({s::Op::Store,
                                          s::voidType(),
                                          s::kNone,
                                          {s::Operand::slot(slot), operand},
                                          {0, 1, 1}});
                        prefix.push_back(
                            {s::Op::Load, s::u4(1), temp, {s::Operand::slot(slot)}, {0, 1, 1}});
                        operand = s::Operand::temp(temp);
                    }
                    instructions[0].operands[i + 1] = operand;
                }
                if (loaded) process.numTemps = n + 1;
                instructions.insert(instructions.begin(), prefix.begin(), prefix.end());
                ASSERT_TRUE(s::verify(d).empty());
                k::Context context;
                const auto result = kirimasim::test::runVerified(d, context);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                std::vector<std::uint64_t> a, b;
                v::toWords(v::Value{{v::udpOutput(rows, inputs)}, false}, a, b);
                const auto* actual = context.store().element(k::SlotId{0}, 0);
                EXPECT_EQ(actual[0], a[0]);
                EXPECT_EQ(actual[1], b[0]);
            }
    }
}

TEST(WideUdpSir, VerifiesAndExecutesLastTerminal) {
    ASSERT_NO_FATAL_FAILURE(checkSmallUdpValues());
    for (const unsigned n : {31U, 32U, 33U, 63U, 64U, 65U, 127U, 128U, 129U, 257U}) {
        for (const bool sequential : {false, true}) {
            auto d = udpDesign(n, sequential);
            SCOPED_TRACE(n);
            SCOPED_TRACE(sequential);
            ASSERT_TRUE(s::verify(d).empty());
            EXPECT_NE(s::toText(d).find("inputs=" + std::to_string(n)), std::string::npos);
            k::Context context;
            const auto result = kirimasim::test::runVerified(d, context);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            const auto* value = context.store().element(k::SlotId{0}, 0);
            EXPECT_EQ(value[0], 1);
            EXPECT_EQ(value[1], 0);
        }
    }
}

TEST(WideUdpSir, RejectsEmptyAndMismatchedOperandsWithoutCountWrap) {
    for (const bool sequential : {false, true}) {
        auto d = udpDesign(33, sequential);
        d.processes[0].blocks[0].instrs[0].operands.pop_back();
        EXPECT_FALSE(s::verify(d).empty());
        d = udpDesign(33, sequential);
        d.types[0].inputs = 0;
        EXPECT_FALSE(s::verify(d).empty());
        d = udpDesign(33, sequential);
        d.types[0].inputs = std::numeric_limits<std::uint32_t>::max();
        EXPECT_FALSE(s::verify(d).empty());
    }
    auto d = udpDesign(33, true);
    d.types[0].inputs = std::uint32_t{1} << 31;
    d.types[0].rows.clear();
    d.processes[0].blocks[0].instrs[0].operands.resize(2);
    const auto errors = s::verify(d);
    bool operandError = false;
    for (const auto& error : errors)
        operandError =
            operandError || error.message.find("needs one value per input") != std::string::npos;
    EXPECT_TRUE(operandError);
}
}  // namespace
