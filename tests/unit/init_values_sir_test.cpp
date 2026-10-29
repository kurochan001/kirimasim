// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"

namespace {
using namespace kirimasim::sir;

Design program() {
    Design d;
    d.sources = {"initial-values.sv"};
    d.instances = {{"top", "top", kNone}};
    Slot variable;
    variable.name = "top.value";
    variable.type = u4(129);
    d.slots.push_back(std::move(variable));
    Process p;
    p.name = "top.initial";
    p.kind = ProcessKind::Initial;
    p.loc = {0, 1, 1};
    p.numTemps = 1;
    p.frame.push_back({"output", u4(129)});
    p.blocks.resize(1);
    p.blocks[0].instrs = {
        {Op::DefaultInit, u4(129), TempId{0}, {}, {0, 2, 3}},
        {Op::Ret, voidType(), TempId{kNone}, {}, {0, 3, 3}},
    };
    d.processes.push_back(std::move(p));
    return d;
}

bool hasError(const Design& d, const std::string& message) {
    const auto errors = verify(d);
    return std::ranges::any_of(errors,
                               [&](const VerifyError& error) { return error.message == message; });
}

TEST(InitValuesSir, MarkersDefaultOffAndAreSerializedOnlyWhenEnabled) {
    auto d = program();
    EXPECT_FALSE(d.slots[0].defaultInit);
    EXPECT_FALSE(d.processes[0].frame[0].defaultInit);
    EXPECT_TRUE(verify(d).empty());
    const auto ordinary = toText(d);
    EXPECT_EQ(ordinary.find("default-init"), std::string::npos);
    EXPECT_NE(ordinary.find("%t0 = default.init i4<129>"), std::string::npos);
    d.slots[0].defaultInit = true;
    d.processes[0].frame[0].defaultInit = true;
    EXPECT_TRUE(verify(d).empty());
    const auto marked = toText(d);
    EXPECT_NE(marked.find("slot %s0 \"top.value\" : i4<129> var inst=0 default-init"),
              std::string::npos);
    EXPECT_NE(marked.find("frame %f0 \"output\" : i4<129> default-init"), std::string::npos);
}

TEST(InitValuesSir, DefaultInstructionRequiresAFourStateResultWithoutOperands) {
    for (const auto type : {u4(1), s4(65), u4(129)}) {
        auto d = program();
        d.processes[0].blocks[0].instrs[0].type = type;
        EXPECT_TRUE(verify(d).empty()) << toString(type);
    }
    for (const auto type : {u2(129), s2(65), Type{TypeKind::Real, 0}}) {
        auto d = program();
        d.processes[0].blocks[0].instrs[0].type = type;
        EXPECT_TRUE(hasError(d, "default.init: needs a four-state integral result"))
            << toString(type);
    }
    auto d = program();
    d.processes[0].blocks[0].instrs[0].operands = {Operand::str("unused")};
    EXPECT_FALSE(verify(d).empty());
    d = program();
    d.processes[0].blocks[0].instrs[0].result = TempId{kNone};
    EXPECT_FALSE(verify(d).empty());
}

TEST(InitValuesSir, StaticMarkersRejectNetsAndTwoStateVariables) {
    for (const auto kind : {SlotKind::Wire, SlotKind::Tri, SlotKind::Wand}) {
        auto d = program();
        d.slots[0].kind = kind;
        d.slots[0].defaultInit = true;
        EXPECT_TRUE(hasError(d, "source default initialization needs four-state variable storage"));
    }
    auto d = program();
    d.slots[0].type = u2(129);
    d.slots[0].defaultInit = true;
    EXPECT_TRUE(hasError(d, "source default initialization needs four-state variable storage"));
}

TEST(InitValuesSir, FrameMarkersRejectBorrowedOrNonFourStateStorage) {
    auto d = program();
    auto& frame = d.processes[0].frame[0];
    frame.defaultInit = true;
    frame.type = u2(129);
    EXPECT_TRUE(hasError(d, "source default initialization needs fresh four-state storage"));
    frame.type = u4(129);
    frame.home = FrameHome::Capture;
    EXPECT_TRUE(hasError(d, "source default initialization needs fresh four-state storage"));
    frame.home = FrameHome::Ordinary;
    frame.readonly = true;
    EXPECT_TRUE(hasError(d, "source default initialization needs fresh four-state storage"));
}

TEST(InitValuesSir, ScopedMarkersRequireTheRootActivation) {
    auto d = program();
    auto& p = d.processes[0];
    p.frame[0].defaultInit = true;
    p.frame[0].home = FrameHome::Scoped;
    p.frame[0].activationScope = ActivationScopeId{1};
    p.activation.scopes = {{kNone, {}}, {ActivationScopeId{0}, {FrameVarId{0}}}};
    p.activation.root = ActivationScopeId{0};
    EXPECT_TRUE(hasError(d, "scoped source default initialization needs the root activation"));
    p.frame[0].activationScope = ActivationScopeId{0};
    p.activation.scopes = {{kNone, {FrameVarId{0}}}};
    EXPECT_TRUE(verify(d).empty());
}

}  // namespace
