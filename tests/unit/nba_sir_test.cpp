// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original checks of the approved event-NBA SIR/lifetime contract.
#include <algorithm>
#include <bit>
#include <cstdint>
#include <new>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/nba.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/sir/version.h"

namespace {
namespace s = kirimasim::sir;
s::Instr instruction(s::Op op, std::vector<s::Operand> operands = {}, s::Type type = s::voidType(),
                     s::TempId result = s::kNone) {
    return {op, type, result, std::move(operands), {0, 3, 2}};
}
s::Operand constant(unsigned n) { return s::Operand::constant(s::ConstId{n}); }
s::NbaValueId capture(unsigned n) { return s::NbaValueId{n}; }
s::Design completeDesign() {
    s::Design d;
    d.sources = {"nba-layout.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::TypeEntry queue;
    queue.kind = s::TypeEntryKind::Queue;
    queue.name = "words";
    queue.value = s::u4(32);
    d.types.push_back(queue);
    const auto q = s::queueType(s::TypeId{0});
    const s::Type event{s::TypeKind::Event, 0}, string{s::TypeKind::String, 0},
        real{s::TypeKind::Real, 0};
    d.slots = {{"top.value", s::u4(129)}, {"top.clock", s::u2(1)}, {"top.array", s::u4(32)},
               {"top.event", event},      {"top.queue", q},        {"top.string", string},
               {"top.real", real}};
    d.slots[2].dims = {{3, 2}, {-1, 0}};
    for (unsigned i = 0; i < d.slots.size(); ++i)
        d.names.push_back({d.slots[i].name, s::SlotId{i}});
    d.constants = {
        {s::u4(129), {1, 0, 0}, {0, 0, 0}},
        {s::u4(129), {7, 0, 0}, {0, 0, 0}},
        {event, {0}, {}},
        {s::s2(64), {3}, {}},
        {s::s2(64), {UINT64_MAX}, {}},
        {s::s2(64), {120}, {}},
        {s::u4(13), {5}, {0}},
        {s::u4(32), {99}, {0}},
        {q, {0}, {}},
        {string, {}, {}, "snapshot"},
        {real, {std::bit_cast<std::uint64_t>(2.5)}, {}},
        {s::u2(1), {1}, {}},
    };
    s::Subroutine watch;
    watch.name = "top.detached_watch";
    watch.automatic = true;
    watch.returnType = s::u4(129);
    watch.loc = {0, 1, 1};
    watch.numTemps = 1;
    watch.blocks.resize(1);
    watch.blocks[0].instrs = {
        instruction(s::Op::Load, {s::Operand::slot(s::SlotId{0})}, s::u4(129), s::TempId{0}),
        instruction(s::Op::Ret, {s::Operand::temp(s::TempId{0})})};
    s::Subroutine guard;
    guard.name = "top.detached_iff";
    guard.automatic = true;
    guard.returnType = s::u2(1);
    guard.loc = {0, 1, 1};
    guard.blocks.resize(1);
    guard.blocks[0].instrs = {instruction(s::Op::Ret, {constant(11)})};
    d.subroutines = {watch, guard};
    s::NbaLayout layout;
    layout.values = {event,        s::u4(129), s::s2(64), s::s2(64), s::s2(64), s::u4(13),
                     s::refType(), s::u4(32),  s::u4(13), q,         string,    real};
    layout.events = {s::NbaSlotEvent{{s::SlotId{1}}, s::Edge::Pos, s::SubroutineId{1}},
                     s::NbaNamedEvent{capture(0), s::SubroutineId{1}},
                     s::NbaWatchEvent{s::SubroutineId{0}, s::Edge::Both, s::SubroutineId{1}}};
    layout.targets = {
        s::NbaSlotTarget{{s::SlotId{0}}, capture(1)},
        s::NbaElementTarget{s::SlotId{2}, {capture(2), capture(3)}, capture(7)},
        s::NbaBitsTarget{{s::SlotId{0}}, {}, capture(4), capture(5)},
        s::NbaRefTarget{capture(6), 0, s::u4(129), capture(1)},
        s::NbaRefBitsTarget{capture(6), 0, s::u4(129), capture(4), capture(8)},
        s::NbaSlotTarget{{s::SlotId{4}}, capture(9)},
        s::NbaSlotTarget{{s::SlotId{5}}, capture(10)},
        s::NbaSlotTarget{{s::SlotId{6}}, capture(11)},
    };
    d.nbaLayouts = {layout};
    s::Process process;
    process.name = "top.initial";
    process.loc = {0, 1, 1};
    process.writes = {
        {s::SlotId{0}}, {s::SlotId{2}}, {s::SlotId{4}}, {s::SlotId{5}}, {s::SlotId{6}}};
    process.numTemps = 1;
    process.blocks.resize(1);
    process.blocks[0].instrs = {
        instruction(s::Op::RefMake, {s::Operand::slot(s::SlotId{0})}, s::refType(), s::TempId{0}),
        instruction(s::Op::NbaEvent,
                    {s::Operand::nbaLayout(s::NbaLayoutId{0}), constant(0), constant(2),
                     constant(1), constant(3), constant(4), constant(5), constant(6),
                     s::Operand::temp(s::TempId{0}), constant(7), constant(6), constant(8),
                     constant(9), constant(10)}),
        instruction(s::Op::Ret)};
    d.processes = {process};
    return d;
}
::testing::AssertionResult valid(const s::Design& d) {
    const auto errors = s::verify(d);
    if (errors.empty()) return ::testing::AssertionSuccess();
    std::string out;
    for (const auto& error : errors)
        out += error.where + ": " + error.message + "\n";
    return ::testing::AssertionFailure() << out;
}
bool rejects(const s::Design& d, const std::string& message) {
    const auto errors = s::verify(d);
    return std::any_of(errors.begin(), errors.end(), [&](const auto& error) {
        return error.message.find(message) != std::string::npos;
    });
}
s::Instr& operation(s::Design& d) { return d.processes[0].blocks[0].instrs[1]; }

TEST(NbaSir, TypedCapturesCoverEveryEventAndTargetFormWithoutPerInstructionStorage) {
    auto d = completeDesign();
    ASSERT_TRUE(valid(d));
    EXPECT_TRUE(s::verifyNbaLayouts(d).empty());
    EXPECT_STREQ(s::opName(s::Op::NbaEvent), "nba.event");
    EXPECT_FALSE(s::isTerminator(s::Op::NbaEvent));
    EXPECT_FALSE(s::isWait(operation(d)));
    const auto writes = s::nbaWrites(d.nbaLayouts[0]);
    ASSERT_EQ(writes.size(), 6U);
    EXPECT_EQ(writes[0].slot, s::SlotId{0});
    EXPECT_EQ(writes[1].slot, s::SlotId{2});
    EXPECT_EQ(writes[2].slot, s::SlotId{0});
}
TEST(NbaSir, TextPreservesItemAndLeafOrderBindingsAndDescriptorTypes) {
    const auto d = completeDesign();
    const auto text = s::toText(d);
    EXPECT_EQ(text, s::toText(d));
    EXPECT_NE(text.find("nba.layout !n0 captures=(event, i4<129>"), std::string::npos);
    EXPECT_NE(text.find("event.slot %s1 edge=posedge iff=@1"), std::string::npos);
    EXPECT_NE(text.find("event.named $0 iff=@1"), std::string::npos);
    EXPECT_NE(text.find("event.watch @0 edge=edge iff=@1"), std::string::npos);
    EXPECT_NE(text.find("write.elem %s2 indices=($2, $3) value=$7"), std::string::npos);
    EXPECT_NE(text.find("write.bits %s0 indices=() position=$4 value=$5"), std::string::npos);
    EXPECT_NE(text.find("write.ref.bits $6 position=$4 leaf=0 type=i4<129> value=$8"),
              std::string::npos);
    EXPECT_NE(text.find("nba.event !n0, const i4<129>"), std::string::npos);
    EXPECT_LT(text.find("write.slot %s4"), text.find("write.slot %s5"));
    EXPECT_EQ(s::formatVersion(), 0U);  // Parser/grammar v1 remains a separate phase.
}
TEST(NbaSir, CountTypesRetainSignednessDomainAndArbitraryWidth) {
    for (auto kind : {s::TypeKind::U2, s::TypeKind::S2, s::TypeKind::U4, s::TypeKind::S4})
        for (auto width : {1U, 32U, 65U, 129U}) {
            auto d = completeDesign();
            auto& count = d.constants[0];
            count.type = {kind, width};
            count.aWords.assign((width + 63) / 64, 0);
            count.aWords[0] = 1;
            count.bWords.assign(s::isFourState(count.type) ? count.aWords.size() : 0, 0);
            EXPECT_TRUE(valid(d));
        }
}
TEST(NbaSir, EmptyPayloadStillRequiresAndPreservesOneCompleteControl) {
    auto d = completeDesign();
    d.nbaLayouts[0].targets.clear();
    d.processes[0].writes.clear();
    EXPECT_TRUE(valid(d));
    EXPECT_NE(s::toText(d).find("event.watch @0"), std::string::npos);
    d.nbaLayouts[0].events.clear();
    EXPECT_TRUE(rejects(d, "at least one event"));
}
TEST(NbaSir, CapturesAreValuesWithExactLayoutTypesAndOrdinals) {
    for (unsigned variant = 0; variant < 6; ++variant) {
        auto d = completeDesign();
        auto& ops = operation(d).operands;
        if (variant == 0) ops.pop_back();
        if (variant == 1) ops.push_back(constant(11));
        if (variant == 2) ops[2] = s::Operand::slot(s::SlotId{3});
        if (variant == 3) ops[3] = constant(11);
        if (variant == 4) ops[2].edge = s::Edge::Pos;
        if (variant == 5) std::get<s::NbaNamedEvent>(d.nbaLayouts[0].events[1]).value = capture(99);
        EXPECT_FALSE(valid(d));
    }
}
TEST(NbaSir, WrongCountLayoutAndResultShapesAreRejected) {
    for (unsigned variant = 0; variant < 6; ++variant) {
        auto d = completeDesign();
        auto& op = operation(d);
        if (variant == 0) op.operands[0] = s::Operand::nbaLayout(s::NbaLayoutId{9});
        if (variant == 1) op.operands[0] = s::Operand::typeRow(s::TypeId{0});
        if (variant == 2) op.operands[1] = constant(10);
        if (variant == 3) op.operands[1] = s::Operand::slot(s::SlotId{1});
        if (variant == 4) op.result = s::TempId{1};
        if (variant == 5) op.type = s::u2(1);
        EXPECT_FALSE(valid(d));
    }
}
TEST(NbaSir, DescriptorIDsCannotLeakIntoOrdinaryValueInstructions) {
    auto d = completeDesign();
    operation(d) = instruction(
        s::Op::Store, {s::Operand::slot(s::SlotId{0}), s::Operand::nbaLayout(s::NbaLayoutId{0})});
    EXPECT_TRUE(rejects(d, "NBA layout IDs"));
}
TEST(NbaSir, CallbacksCannotBorrowWaiterFramesArgumentsOrLexicalExtents) {
    for (unsigned variant = 0; variant < 8; ++variant) {
        auto d = completeDesign();
        auto& callback = d.subroutines[0];
        if (variant == 0) callback.automatic = false;
        if (variant == 1) callback.kind = s::SubroutineKind::Watch;
        if (variant == 2) callback.frameOf = s::Owner::of(s::ProcessId{0});
        if (variant == 3) callback.mayWait = true;
        if (variant == 4) callback.params.push_back({"arg", s::Direction::Input, s::u2(1)});
        if (variant == 5) callback.activation.inheritCaptures = true;
        if (variant == 6) callback.activation.extents.push_back(s::ActivationScopeId{0});
        if (variant == 7) {
            callback.frame.push_back({"captured", s::u2(1)});
            callback.frame.back().home = s::FrameHome::Capture;
        }
        EXPECT_FALSE(valid(d));
    }
}
TEST(NbaSir, CallbackReturnRolesAndReferencesAreValidatedEvenInUnusedLayouts) {
    for (unsigned variant = 0; variant < 4; ++variant) {
        auto d = completeDesign();
        d.nbaLayouts.push_back(d.nbaLayouts[0]);
        auto& events = d.nbaLayouts[1].events;
        if (variant == 0) std::get<s::NbaWatchEvent>(events[2]).watch = s::SubroutineId{99};
        if (variant == 1) std::get<s::NbaWatchEvent>(events[2]).guard = s::SubroutineId{0};
        if (variant == 2) std::get<s::NbaSlotEvent>(events[0]).edge = static_cast<s::Edge>(99);
        if (variant == 3) std::get<s::NbaNamedEvent>(events[1]).value = capture(1);
        EXPECT_FALSE(valid(d));
    }
}
TEST(NbaSir, TargetShapesRangesDomainsAndIndexArityAreChecked) {
    for (unsigned variant = 0; variant < 10; ++variant) {
        auto d = completeDesign();
        auto& targets = d.nbaLayouts[0].targets;
        if (variant == 0) std::get<s::NbaSlotTarget>(targets[0]).part.slot = s::SlotId{99};
        if (variant == 1) std::get<s::NbaSlotTarget>(targets[0]).part.width = 130;
        if (variant == 2) std::get<s::NbaElementTarget>(targets[1]).indices.pop_back();
        if (variant == 3) std::get<s::NbaElementTarget>(targets[1]).indices[0] = capture(0);
        if (variant == 4) std::get<s::NbaElementTarget>(targets[1]).value = capture(1);
        if (variant == 5) std::get<s::NbaBitsTarget>(targets[2]).position = capture(0);
        if (variant == 6) std::get<s::NbaBitsTarget>(targets[2]).value = capture(2);
        if (variant == 7) std::get<s::NbaRefTarget>(targets[3]).reference = capture(0);
        if (variant == 8) std::get<s::NbaRefTarget>(targets[3]).target = s::u2(129);
        if (variant == 9) std::get<s::NbaRefBitsTarget>(targets[4]).position = capture(0);
        EXPECT_FALSE(valid(d));
    }
}
TEST(NbaSir, BitValuesMayPartiallyOverlapTheParentBounds) {
    auto d = completeDesign();
    std::get<s::NbaBitsTarget>(d.nbaLayouts[0].targets[2]).value = capture(7);
    std::get<s::NbaRefBitsTarget>(d.nbaLayouts[0].targets[4]).value = capture(7);
    EXPECT_TRUE(valid(d));
}
TEST(NbaSir, StaticWriteFootprintsAndVariableEligibilityAreChecked) {
    auto d = completeDesign();
    d.processes[0].writes.clear();
    EXPECT_TRUE(rejects(d, "outside the write set"));
    d = completeDesign();
    d.slots[0].kind = s::SlotKind::Wire;
    EXPECT_TRUE(rejects(d, "NBA target must be a variable"));
}
struct ThrowingIndices {
    operator std::vector<s::NbaValueId>() const { throw std::bad_alloc{}; }
};
TEST(NbaSir, FailedVariantConstructionCannotEscapeVerification) {
    auto d = completeDesign();
    auto& item = d.nbaLayouts[0].targets[1];
    ASSERT_THROW(item.emplace<s::NbaElementTarget>(s::SlotId{2}, ThrowingIndices{}, capture(7)),
                 std::bad_alloc);
    // A library may preserve the old alternative or leave it empty after failure.
    // Both representations must remain safe input to the verifier.
    if (item.valueless_by_exception()) {
        EXPECT_TRUE(rejects(d, "invalid target variant"));
        EXPECT_EQ(s::nbaWrites(d.nbaLayouts[0]).size(), 5U);
    } else {
        EXPECT_TRUE(valid(d));
    }
    RecordProperty("valueless_variant_exercised", item.valueless_by_exception() ? 1 : 0);
}
}  // namespace
