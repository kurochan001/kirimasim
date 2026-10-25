// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/sir/version.h"
#include "kirimasim/vm/vm.h"

namespace {
using namespace kirimasim::sir;

Instr instruction(Op op, std::vector<Operand> operands = {}, Type type = voidType(),
                  TempId result = kNone) {
    return {op, type, result, std::move(operands), {0, 1, 1}};
}

struct Program {
    Design d;
    Program() {
        d.sources = {"activation.sv"};
        d.instances = {{"top", "top", kNone}};
        Process p;
        p.name = "top.parent";
        p.loc = {0, 1, 1};
        p.frame = {{"root", u4(32)}, {"block", u4(129)}, {"ordinary", u2(32)}};
        p.frame[0].home = FrameHome::Scoped;
        p.frame[0].activationScope = ActivationScopeId{0};
        p.frame[1].home = FrameHome::Scoped;
        p.frame[1].activationScope = ActivationScopeId{1};
        p.activation.scopes = {{kNone, {FrameVarId{0}}},
                               {ActivationScopeId{0}, {FrameVarId{1}}},
                               {ActivationScopeId{1}, {}}};
        p.activation.root = ActivationScopeId{0};
        p.blocks.resize(2);
        p.blocks[0].instrs = {
            instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
            instruction(Op::Fork, {Operand::process(ProcessId{1}), Operand::str("join_none"),
                                   Operand::block(BlockId{1})})};
        p.blocks[1].instrs = {
            instruction(Op::ActivationLeave, {Operand::activation(ActivationScopeId{1})}),
            instruction(Op::Ret)};
        d.processes.push_back(std::move(p));
        Process child;
        child.name = "top.child";
        child.kind = ProcessKind::General;
        child.loc = {0, 1, 1};
        child.parent = Owner::of(ProcessId{0});
        child.frame = {d.processes[0].frame[1]};
        child.frame[0].home = FrameHome::Capture;
        child.frame[0].activationScope = kNone;
        child.frame[0].captureSource = FrameVarId{1};
        child.activation.extents = {ActivationScopeId{0}, ActivationScopeId{1}};
        child.blocks.resize(1);
        child.blocks[0].instrs = {instruction(Op::Ret)};
        d.processes.push_back(std::move(child));
    }
};

::testing::AssertionResult valid(const Design& design) {
    const auto errors = verify(design);
    if (errors.empty()) return ::testing::AssertionSuccess();
    std::string message;
    for (const auto& error : errors)
        message += error.where + ": " + error.message + "\n";
    return ::testing::AssertionFailure() << message;
}
bool activationError(const Design& design, const std::string& fragment) {
    const auto errors = verifyActivationScopes(design);
    return std::any_of(errors.begin(), errors.end(), [&](const auto& e) {
        return e.message.find(fragment) != std::string::npos;
    });
}

TEST(ActivationSir, RootBlockFieldsAndCaptureHomesHaveDeterministicTextWithoutPhysicalOffsets) {
    Program p;
    ASSERT_TRUE(valid(p.d));
    EXPECT_EQ(formatVersion(), 0U);
    const auto text = toText(p.d);
    EXPECT_EQ(text, toText(p.d));
    EXPECT_NE(text.find("activation #0 root fields=(%f0)"), std::string::npos);
    EXPECT_NE(text.find("activation #1 parent=#0 fields=(%f1)"), std::string::npos);
    EXPECT_NE(text.find("home=activation#1"), std::string::npos);
    EXPECT_NE(text.find("home=capture%f1"), std::string::npos);
    EXPECT_NE(text.find("capture.extents #0 #1"), std::string::npos);
    EXPECT_NE(text.find("activation.enter #1"), std::string::npos);
    EXPECT_NE(text.find("activation.leave #1"), std::string::npos);
    EXPECT_EQ(text.find("offset="), std::string::npos);
}

TEST(ActivationSir, OrdinaryDesignsEmitNoAutomaticTablesOrHomeAnnotations) {
    Program p;
    p.d.processes.resize(1);
    auto& parent = p.d.processes[0];
    parent.activation = {};
    parent.frame = {{"ordinary", u4(65)}};
    parent.blocks.resize(1);
    parent.blocks[0].instrs = {instruction(Op::Ret)};
    ASSERT_TRUE(valid(p.d));
    const auto text = toText(p.d);
    EXPECT_EQ(text.find("  activation #"), std::string::npos);
    EXPECT_EQ(text.find("home="), std::string::npos);
    EXPECT_EQ(text.find("capture.extents"), std::string::npos);
}

TEST(ActivationSir, MalformedScopeAndCaptureTablesAreRejectedByTheSharedVerifier) {
    for (unsigned mutation = 0; mutation != 25; ++mutation) {
        SCOPED_TRACE(mutation);
        Program p;
        auto& parent = p.d.processes[0];
        auto& child = p.d.processes[1];
        std::string expected;
        switch (mutation) {
            case 0:
                parent.activation.root = ActivationScopeId{9};
                expected = "root scope";
                break;
            case 1:
                parent.activation.scopes[0].parent = ActivationScopeId{1};
                expected = "parent cycle";
                break;
            case 2:
                parent.activation.scopes[1].parent = ActivationScopeId{9};
                expected = "parent is out";
                break;
            case 3:
                parent.activation.scopes[1].fields = {FrameVarId{9}};
                expected = "field is out";
                break;
            case 4:
                parent.activation.scopes[0].fields.push_back(FrameVarId{0});
                expected = "exactly one";
                break;
            case 5:
                parent.frame[1].activationScope = ActivationScopeId{0};
                expected = "different home";
                break;
            case 6:
                parent.frame[1].home = FrameHome::Ordinary;
                expected = "different home";
                break;
            case 7:
                parent.frame[1].captureSource = FrameVarId{0};
                expected = "exactly one";
                break;
            case 8:
                parent.frame[1].type = u4(0);
                expected = "typed storage";
                break;
            case 9:
                parent.frame[1].dims = {{INT64_MIN, INT64_MAX}};
                expected = "typed storage";
                break;
            case 10:
                parent.frame[1].dims = {{0, INT64_MAX}};
                expected = "typed storage";
                break;
            case 11:
                parent.frame[1].type = {TypeKind::Event, 1};
                expected = "typed storage";
                break;
            case 12:
                parent.frame[1].type = refType();
                expected = "typed storage";
                break;
            case 13:
                parent.frame[1].home = static_cast<FrameHome>(255);
                expected = "unknown frame";
                break;
            case 14:
                child.frame[0].captureSource = FrameVarId{9};
                expected = "source is invalid";
                break;
            case 15:
                child.frame[0].type = u4(128);
                expected = "type, shape";
                break;
            case 16:
                child.frame[0].target = u2(1);
                expected = "type, shape";
                break;
            case 17:
                child.frame[0].declType = TypeId{9};
                expected = "type, shape";
                break;
            case 18:
                child.frame[0].dims = {{1, 0}};
                expected = "type, shape";
                break;
            case 19:
                child.frame[0].activationScope = ActivationScopeId{0};
                expected = "source is invalid";
                break;
            case 20:
                child.activation.extents.push_back(ActivationScopeId{9});
                expected = "extent is out";
                break;
            case 21:
                child.activation.extents.push_back(ActivationScopeId{1});
                expected = "extent is duplicated";
                break;
            case 22:
                child.activation.extents.pop_back();
                expected = "lacks its parent extent";
                break;
            case 23:
                child.parent = Owner::of(ProcessId{9});
                expected = "source is invalid";
                break;
            case 24:
                child.frame[0].captureSource = FrameVarId{2};
                child.frame[0].type = u2(32);
                expected = "promoted shared";
                break;
            default: FAIL() << "unexpected metadata mutation selector";
        }
        EXPECT_TRUE(activationError(p.d, expected));
        EXPECT_FALSE(verify(p.d).empty());
    }
}

TEST(ActivationSir, LexicalOwnershipIsRequiredAtReadsLeavesForksAndMerges) {
    for (unsigned mutation = 0; mutation != 8; ++mutation) {
        SCOPED_TRACE(mutation);
        Program p;
        auto& body = p.d.processes[0];
        auto& first = body.blocks[0].instrs;
        std::string expected;
        switch (mutation) {
            case 0:
                first.erase(first.begin());
                expected = "retain all active";
                break;
            case 1:
                first.insert(first.begin(), instruction(Op::Fload, {Operand::frame(FrameVarId{1})},
                                                        u4(129), TempId{0}));
                body.numTemps = 1;
                expected = "no executing";
                break;
            case 2:
                body.blocks[1].instrs[0].operands[0] = Operand::activation(ActivationScopeId{0});
                expected = "innermost";
                break;
            case 3:
                first.insert(first.begin() + 1, first.front());
                expected = "lexical parent";
                break;
            case 4:
                first[0].operands[0] = Operand::activation(ActivationScopeId{2});
                expected = "lexical parent";
                break;
            case 5:
                first[0].operands[0] = Operand::activation(ActivationScopeId{9});
                expected = "valid local";
                break;
            case 6:
                first[0].operands.clear();
                expected = "valid local";
                break;
            case 7:
                body.blocks.resize(4);
                p.d.constants.push_back({u2(1), {1}, {}});
                body.blocks[0].instrs = {instruction(Op::BrIf, {Operand::constant(ConstId{0}),
                                                                Operand::block(BlockId{1}),
                                                                Operand::block(BlockId{2})})};
                body.blocks[1].instrs = {
                    instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
                    instruction(Op::Br, {Operand::block(BlockId{3})})};
                body.blocks[2].instrs = {instruction(Op::Br, {Operand::block(BlockId{3})})};
                body.blocks[3].instrs = {instruction(Op::Ret)};
                expected = "CFG edges";
                break;
            default: FAIL() << "unexpected ownership mutation selector";
        }
        EXPECT_TRUE(activationError(p.d, expected));
    }
}

TEST(ActivationSir, WaitsPreserveOwnershipAndRepeatedBlockEntriesAreBalanced) {
    Program p;
    auto& body = p.d.processes[0];
    body.blocks.resize(3);
    p.d.constants.push_back({u2(32), {1}, {}});
    body.blocks[0].instrs = {
        instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
        instruction(Op::WaitDelay, {Operand::constant(ConstId{0}), Operand::block(BlockId{1})})};
    body.blocks[1].instrs = {
        instruction(Op::Fork, {Operand::process(ProcessId{1}), Operand::str("join_none"),
                               Operand::block(BlockId{2})})};
    body.blocks[2].instrs = {
        instruction(Op::ActivationLeave, {Operand::activation(ActivationScopeId{1})}),
        instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
        instruction(Op::ActivationLeave, {Operand::activation(ActivationScopeId{1})}),
        instruction(Op::Ret)};
    EXPECT_TRUE(valid(p.d));
}

TEST(ActivationSir, NamedDisableRestoresTheAutomaticDepthRecordedAtControlEntry) {
    for (bool disable : {false, true}) {
        Program p;
        p.d.numScopes = 1;
        auto& body = p.d.processes[0];
        body.blocks.resize(3);
        body.blocks[0].instrs = {
            instruction(Op::ScopeEnter, {Operand::scope(0), Operand::block(BlockId{2})}),
            instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
            instruction(Op::Br, {Operand::block(BlockId{1})})};
        body.blocks[1].instrs = {
            instruction(disable ? Op::DisableScope : Op::ScopeLeave, {Operand::scope(0)}),
            instruction(Op::Br, {Operand::block(BlockId{2})})};
        body.blocks[2].instrs = {instruction(Op::Ret)};
        EXPECT_TRUE(valid(p.d));
    }
}

TEST(ActivationSir, NestedCapturesForwardOriginalBindingsAndReadonlyRights) {
    Program p;
    auto& child = p.d.processes[1];
    child.frame[0].readonly = true;
    child.blocks.resize(2);
    child.blocks[0].instrs = {
        instruction(Op::Fork, {Operand::process(ProcessId{2}), Operand::str("join_none"),
                               Operand::block(BlockId{1})})};
    child.blocks[1].instrs = {instruction(Op::Ret)};
    Process grandchild;
    grandchild.kind = ProcessKind::General;
    grandchild.parent = Owner::of(ProcessId{1});
    grandchild.loc = {0, 1, 1};
    grandchild.frame = child.frame;
    grandchild.frame[0].captureSource = FrameVarId{0};
    grandchild.activation.inheritCaptures = true;
    grandchild.blocks.resize(1);
    grandchild.blocks[0].instrs = {instruction(Op::Ret)};
    p.d.processes.push_back(std::move(grandchild));
    ASSERT_TRUE(valid(p.d));
    EXPECT_NE(toText(p.d).find(" readonly"), std::string::npos);
    p.d.processes[2].frame[0].readonly = false;
    EXPECT_TRUE(activationError(p.d, "access rights"));
    p.d.processes[2].frame[0].readonly = true;
    p.d.processes[2].activation.inheritCaptures = false;
    EXPECT_TRUE(activationError(p.d, "inherited extents"));
}

TEST(ActivationSir, AReadonlyCaptureCannotBeAssignedThroughItsLogicalFrameRow) {
    Program p;
    auto& child = p.d.processes[1];
    child.frame[0].readonly = true;
    p.d.constants.push_back({u4(129), {7, 0, 0}, {0, 0, 0}});
    child.blocks[0].instrs.insert(
        child.blocks[0].instrs.begin(),
        instruction(Op::Fstore, {Operand::frame(FrameVarId{0}), Operand::constant(ConstId{0})}));
    EXPECT_TRUE(activationError(p.d, "readonly automatic"));
}

TEST(ActivationSir, DeepLexicalLayoutsAreCheckedWithoutRecursiveNativeCalls) {
    Program p;
    auto& body = p.d.processes[0];
    for (std::uint32_t i = 3; i < 10000; ++i)
        body.activation.scopes.push_back({ActivationScopeId{i - 1}, {}});
    EXPECT_TRUE(valid(p.d));
    body.activation.scopes[9999].parent = ActivationScopeId{9999};
    EXPECT_TRUE(activationError(p.d, "parent cycle"));
}

TEST(ActivationSir, ForkParentsCannotFormACaptureCycle) {
    Program p;
    p.d.processes[0].parent = Owner::of(ProcessId{1});
    EXPECT_TRUE(activationError(p.d, "fork parent cycle"));
}

TEST(ActivationSir, BorrowedRefCapturesRequireJoinAndStaticQualifiersSurviveForwarding) {
    for (const bool qualified : {false, true})
        for (const bool readonly : {false, true})
            for (const std::string join : {"join", "join_any", "join_none"}) {
                SCOPED_TRACE(join);
                Program p;
                p.d.processes[0].activation = {};
                p.d.processes[0].frame.clear();
                p.d.processes[0].blocks.resize(1);
                p.d.processes[0].blocks[0].instrs = {instruction(Op::Ret)};
                Subroutine task;
                task.name = "top.task";
                task.kind = SubroutineKind::Task;
                task.automatic = true;
                task.mayWait = true;
                task.loc = {0, 1, 1};
                FrameVar argument;
                argument.name = "argument";
                argument.type = refType();
                argument.target = u4(32);
                argument.readonly = readonly;
                argument.refStatic = qualified;
                task.frame = {argument};
                task.params = {{"argument", readonly ? Direction::ConstRef : Direction::Ref, u4(32),
                                kNone, FrameVarId{0}, qualified}};
                task.blocks.resize(2);
                task.blocks[0].instrs = {
                    instruction(Op::Fork, {Operand::process(ProcessId{1}), Operand::str(join),
                                           Operand::block(BlockId{1})})};
                task.blocks[1].instrs = {instruction(Op::Ret)};
                p.d.subroutines.push_back(std::move(task));
                auto& child = p.d.processes[1];
                child.parent = Owner::of(SubroutineId{0});
                child.activation = {};
                child.frame = {argument};
                child.frame[0].home = FrameHome::Capture;
                child.frame[0].captureSource = FrameVarId{0};
                if (qualified || join == "join") {
                    EXPECT_TRUE(valid(p.d));
                    if (qualified) EXPECT_NE(toText(p.d).find("ref-static"), std::string::npos);
                } else {
                    EXPECT_TRUE(activationError(p.d, "borrowed ref cannot escape"));
                    EXPECT_FALSE(verify(p.d).empty());
                }
                p.d.subroutines[0].frame[0].refStatic = !qualified;
                EXPECT_TRUE(activationError(p.d, "reference parameter qualifiers"));
            }
}

TEST(ActivationSir, WaiterHelpersRequireTheirLexicalScopeAtTheWaitSite) {
    Program p;
    p.d.processes.resize(1);
    Subroutine watch;
    watch.name = "top.watch";
    watch.kind = SubroutineKind::Watch;
    watch.frameOf = Owner::of(ProcessId{0});
    watch.loc = {0, 1, 1};
    watch.returnType = u4(129);
    watch.numTemps = 1;
    watch.blocks.resize(1);
    watch.blocks[0].instrs = {
        instruction(Op::Fload, {Operand::frame(FrameVarId{1})}, u4(129), TempId{0}),
        instruction(Op::Ret, {Operand::temp(TempId{0})})};
    p.d.subroutines.push_back(std::move(watch));
    auto& body = p.d.processes[0];
    body.blocks.resize(2);
    body.blocks[0].instrs = {
        instruction(Op::ActivationEnter, {Operand::activation(ActivationScopeId{1})}),
        instruction(Op::WaitEvent,
                    {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{1})})};
    body.blocks[1].instrs = {instruction(Op::Ret)};
    ASSERT_TRUE(valid(p.d));
    body.blocks[0].instrs.erase(body.blocks[0].instrs.begin());
    EXPECT_TRUE(activationError(p.d, "helper accesses an inactive"));
    EXPECT_FALSE(verify(p.d).empty());
    p.d.subroutines[0].activation.scopes.push_back({});
    EXPECT_TRUE(activationError(p.d, "helpers cannot own"));
}

TEST(ActivationSir, TypedFieldsPreserveWideAndFixedArrayShapesWithoutInventingRoots) {
    std::vector<Type> types{{TypeKind::Real, 0},
                            {TypeKind::ShortReal, 0},
                            {TypeKind::String, 0},
                            {TypeKind::Chandle, 0}};
    for (const auto width : {1U, 65U, 129U, 257U})
        for (const auto type : {u2(width), u4(width), s2(width), s4(width)})
            types.push_back(type);
    types.push_back(dynarrayType(TypeId{0}));
    types.push_back(queueType(TypeId{0}));
    for (const auto type : types)
        for (const auto& dims : {std::vector<ArrayDim>{}, {{-2, 2}}, {{4, 3}, {-1, 1}}}) {
            Program p;
            if (isContainer(type)) {
                TypeEntry descriptor;
                descriptor.kind = type.kind == TypeKind::DynArray ? TypeEntryKind::DynArray
                                                                  : TypeEntryKind::Queue;
                descriptor.name = "values";
                descriptor.value = u4(9);
                p.d.types.push_back(std::move(descriptor));
            }
            p.d.processes[0].frame[1].type = type;
            p.d.processes[0].frame[1].dims = dims;
            p.d.processes[1].frame[0].type = type;
            p.d.processes[1].frame[0].dims = dims;
            EXPECT_TRUE(valid(p.d));
        }
}

TEST(ActivationSir, MalformedAutomaticMetadataIsRejectedBeforeExecutionAllocatesSlots) {
    Program p;
    p.d.processes[1].frame[0].captureSource = FrameVarId{99};
    kirimasim::kernel::Context context;
    const auto result = kirimasim::vm::run(p.d, context, {});
    EXPECT_EQ(result.reason, kirimasim::kernel::EndReason::Internal);
    EXPECT_EQ(result.error, "invalid automatic scope metadata");
    EXPECT_EQ(context.store().size(), 0U);
    EXPECT_EQ(context.activations().stats().live, 0U);
}

TEST(ActivationSir, WaiterHelpersCannotWriteReadonlyCaptureViews) {
    Program p;
    p.d.constants = {{u4(129), {7, 0, 0}, {0, 0, 0}}, {u2(1), {1}, {}}};
    auto& child = p.d.processes[1];
    child.frame[0].readonly = true;
    child.blocks.resize(2);
    child.blocks[0].instrs = {instruction(
        Op::WaitCond, {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{1})})};
    child.blocks[1].instrs = {instruction(Op::Ret)};
    Subroutine guard;
    guard.name = "top.guard";
    guard.kind = SubroutineKind::Guard;
    guard.returnType = u2(1);
    guard.frameOf = Owner::of(ProcessId{1});
    guard.loc = {0, 1, 1};
    guard.blocks.resize(1);
    guard.blocks[0].instrs = {
        instruction(Op::Fstore, {Operand::frame(FrameVarId{0}), Operand::constant(ConstId{0})}),
        instruction(Op::Ret, {Operand::constant(ConstId{1})})};
    p.d.subroutines.push_back(std::move(guard));
    EXPECT_TRUE(activationError(p.d, "readonly automatic binding"));
    EXPECT_FALSE(verify(p.d).empty());
}

TEST(ActivationSir, ScopedParametersAreAvailableInTheRootBeforeCopyIn) {
    Program p;
    Subroutine task;
    task.name = "top.task";
    task.kind = SubroutineKind::Task;
    task.automatic = true;
    task.loc = {0, 1, 1};
    task.frame = {p.d.processes[0].frame[0]};
    task.activation.root = ActivationScopeId{0};
    task.activation.scopes = {{kNone, {FrameVarId{0}}}, {ActivationScopeId{0}, {}}};
    task.params = {{"argument", Direction::Input, u4(32), kNone, FrameVarId{0}}};
    task.blocks.resize(1);
    task.blocks[0].instrs = {instruction(Op::Ret)};
    p.d.subroutines.push_back(std::move(task));
    ASSERT_TRUE(valid(p.d));
    auto& sub = p.d.subroutines[0];
    sub.frame[0].activationScope = ActivationScopeId{1};
    sub.activation.scopes[0].fields.clear();
    sub.activation.scopes[1].fields = {FrameVarId{0}};
    EXPECT_TRUE(activationError(p.d, "root before copy-in"));
    EXPECT_FALSE(verify(p.d).empty());
}

}  // namespace
