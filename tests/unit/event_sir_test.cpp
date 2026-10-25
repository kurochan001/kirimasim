// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original SIR checks of automatic-capture contract 5/7 (IEEE 1800 6.8/15.5).
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/sir/version.h"

namespace {
namespace s = kirimasim::sir;
const s::Type kEvent{s::TypeKind::Event, 0};

s::Instr instruction(s::Op op, std::vector<s::Operand> operands = {}, s::Type type = s::voidType(),
                     s::TempId result = s::kNone) {
    return {op, type, result, std::move(operands), {0, 3, 2}};
}
s::Design design() {
    s::Design d;
    d.sources = {"events.sv"};
    d.instances = {{"top", "top", s::kNone}};
    d.constants = {{kEvent, {0}, {}}, {s::u2(64), {5}, {}}};
    s::Slot slot;
    slot.name = "top.events";
    slot.type = kEvent;
    slot.dims = {{3, 2}};
    d.slots.push_back(slot);
    d.names = {{slot.name, s::SlotId{0}}};
    s::Process process;
    process.name = "top.initial";
    process.kind = s::ProcessKind::Initial;
    process.loc = {0, 1, 1};
    process.numTemps = 1;
    process.blocks.resize(1);
    process.blocks[0].instrs = {instruction(s::Op::EventNew, {}, kEvent, s::TempId{0}),
                                instruction(s::Op::Ret)};
    d.processes.push_back(process);
    return d;
}
::testing::AssertionResult valid(const s::Design& d) {
    const auto errors = s::verify(d);
    if (errors.empty()) return ::testing::AssertionSuccess();
    std::string message;
    for (const auto& error : errors)
        message += error.where + ": " + error.message + "\n";
    return ::testing::AssertionFailure() << message;
}
bool rejects(const s::Design& d, const std::string& part) {
    const auto errors = s::verify(d);
    return std::any_of(errors.begin(), errors.end(),
                       [&](const auto& e) { return e.message.find(part) != std::string::npos; });
}
s::Operand target(unsigned kind) {
    if (kind == 0) return s::Operand::temp(s::TempId{0});
    if (kind == 1) return s::Operand::constant(s::ConstId{0});
    auto slot = s::Operand::slot(s::SlotId{0});
    slot.firstElem = 1;
    slot.numElems = 1;
    return slot;
}
void operation(s::Design& d, s::Op op, const s::Operand& value, bool delayed = false) {
    std::vector<s::Operand> operands{value};
    if (delayed) operands.push_back(s::Operand::constant(s::ConstId{1}));
    if (op == s::Op::WaitNamed || op == s::Op::WaitEvent) {
        d.processes[0].blocks.resize(2);
        d.processes[0].blocks[1].instrs = {instruction(s::Op::Ret)};
        operands.push_back(s::Operand::block(s::BlockId{1}));
        d.processes[0].blocks[0].instrs.back() = instruction(op, std::move(operands));
    } else {
        auto& body = d.processes[0].blocks[0].instrs;
        body.insert(body.end() - 1, instruction(op, std::move(operands)));
    }
}

TEST(EventSir, CreationIsTypedNonterminatingAndDeterministicInDraftText) {
    const auto d = design();
    ASSERT_TRUE(valid(d));
    EXPECT_STREQ(s::opName(s::Op::EventNew), "event.new");
    EXPECT_FALSE(s::isTerminator(s::Op::EventNew));
    EXPECT_FALSE(s::isWait(d.processes[0].blocks[0].instrs[0]));
    const auto text = s::toText(d);
    EXPECT_EQ(text, s::toText(d));
    EXPECT_NE(text.find("%t0 = event.new event"), std::string::npos);
    EXPECT_EQ(s::constantText(d.constants[0]), "null");
    EXPECT_EQ(s::formatVersion(), 0U);
}

TEST(EventSir, LoadedNullAndLegacyTargetsShareTriggerAndWaitForms) {
    for (const auto op : {s::Op::Trigger, s::Op::TriggerNb, s::Op::WaitNamed, s::Op::WaitEvent})
        for (unsigned kind = 0; kind < 3; ++kind) {
            SCOPED_TRACE(s::opName(op));
            SCOPED_TRACE(kind);
            auto d = design();
            operation(d, op, target(kind));
            ASSERT_TRUE(valid(d));
            if (op == s::Op::TriggerNb) {
                d = design();
                operation(d, op, target(kind), true);
                EXPECT_TRUE(valid(d));
            }
        }
}

TEST(EventSir, LoadedEventItemsPreserveMixedSignalAndIffOrder) {
    auto d = design();
    s::Slot signal;
    signal.name = "top.signal";
    signal.type = s::u4(1);
    d.slots.push_back(signal);
    d.names.push_back({signal.name, s::SlotId{1}});
    s::Subroutine guard;
    guard.name = "guard";
    guard.kind = s::SubroutineKind::Guard;
    guard.automatic = true;
    guard.returnType = s::u2(1);
    guard.frameOf = s::Owner::of(s::ProcessId{0});
    guard.loc = {0, 2, 1};
    d.constants.push_back({s::u2(1), {1}, {}});
    guard.blocks = {{{}, {instruction(s::Op::Ret, {s::Operand::constant(s::ConstId{2})})}}};
    d.subroutines.push_back(guard);
    operation(d, s::Op::WaitEvent, target(0));
    auto signalItem = s::Operand::slot(s::SlotId{1});
    signalItem.edge = s::Edge::Pos;
    auto& operands = d.processes[0].blocks[0].instrs.back().operands;
    operands.insert(operands.end() - 1, {s::Operand::subroutine(s::SubroutineId{0}), signalItem,
                                         s::Operand::subroutine(s::SubroutineId{0}), target(1)});
    ASSERT_TRUE(valid(d));
    EXPECT_NE(s::toText(d).find("wait.event %t0 iff @0, posedge %s1 iff @0, const event null, bb1"),
              std::string::npos);
}

TEST(EventSir, CreationRejectsOperandsMissingResultsAndOtherResultTypes) {
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        SCOPED_TRACE(mutation);
        auto d = design();
        auto& in = d.processes[0].blocks[0].instrs[0];
        switch (mutation) {
            case 0: in.operands.push_back(target(1)); break;
            case 1: in.result = s::kNone; break;
            case 2: in.type = s::voidType(); break;
            case 3: in.type = s::u2(64); break;
            case 4: in.type = {s::TypeKind::Chandle, 0}; break;
            default: FAIL() << mutation;
        }
        EXPECT_TRUE(rejects(d, "event.new:"));
    }
}

TEST(EventSir, EventConstantsCannotForgeLiveHandlesOrCarryUnknownBits) {
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        SCOPED_TRACE(mutation);
        auto d = design();
        auto& value = d.constants[0];
        switch (mutation) {
            case 0: value.aWords.clear(); break;
            case 1: value.aWords[0] = 1; break;
            case 2: value.aWords[0] = std::uint64_t{1} << 32; break;
            case 3: value.aWords.push_back(0); break;
            case 4: value.bWords.push_back(0); break;
            default: FAIL() << mutation;
        }
        EXPECT_TRUE(rejects(d, "an event constant must be null"));
    }
}

TEST(EventSir, ValueTargetsRejectOtherDomainsAndHiddenSelections) {
    for (const auto op : {s::Op::Trigger, s::Op::TriggerNb, s::Op::WaitNamed, s::Op::WaitEvent})
        for (unsigned mutation = 0; mutation < 7; ++mutation) {
            SCOPED_TRACE(s::opName(op));
            SCOPED_TRACE(mutation);
            auto d = design();
            auto value = target(0);
            switch (mutation) {
                case 0: value = s::Operand::constant(s::ConstId{1}); break;
                case 1: value.edge = s::Edge::Pos; break;
                case 2: value.lsb = 1; break;
                case 3: value.width = 1; break;
                case 4: value.firstElem = 1; break;
                case 5: value.numElems = 1; break;
                case 6: value.count = 1; break;
                default: FAIL() << mutation;
            }
            operation(d, op, value);
            EXPECT_FALSE(s::verify(d).empty());
        }
}

TEST(EventSir, DeferredTriggersStillRequireAnIntegralDelay) {
    auto d = design();
    operation(d, s::Op::TriggerNb, target(0), true);
    d.processes[0].blocks[0].instrs[1].operands[1] = target(1);
    EXPECT_TRUE(rejects(d, "optional integral delay"));
}

TEST(EventSir, EventValuesStillObeySsaDominanceAndWaitBoundaries) {
    auto d = design();
    operation(d, s::Op::WaitNamed, target(0));
    auto& after = d.processes[0].blocks[1].instrs;
    after.insert(after.begin(), instruction(s::Op::Trigger, {target(0)}));
    EXPECT_TRUE(rejects(d, "wait"));
}
}  // namespace
