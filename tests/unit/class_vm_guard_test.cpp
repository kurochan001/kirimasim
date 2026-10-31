// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <string>
#include <utility>

#include "kirimasim/sir/classes.h"
#include "kirimasim/vm/vm.h"
#include "gtest/gtest.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;

s::Instr effect(s::Op op) { return {op, s::voidType(), s::kNone, {}, {0, 1, 1}}; }
s::Design design() {
    s::Design d;
    d.sources = {"class.sv"};
    d.instances = {{"top", "top", s::kNone}};
    s::ClassDescriptor type;
    type.name = "Object";
    type.constructor = s::SubroutineId{0};
    type.fields = {{"value", s::u4(129)}};
    d.classes.push_back(std::move(type));
    const auto object = s::classType(s::ClassId{0});
    d.slots = {{"top.object", object}};
    s::Constant text;
    text.type = {s::TypeKind::String, 0};
    text.bytes = "do not intern before class preflight";
    d.constants.push_back(std::move(text));
    s::Subroutine ctor;
    ctor.name = "top.new";
    ctor.automatic = true;
    ctor.loc = {0, 1, 1};
    ctor.classRole = s::ClassRole::Constructor;
    ctor.classOwner = s::ClassId{0};
    ctor.params = {{"this", s::Direction::Input, object, s::kNone, s::FrameVarId{0}}};
    ctor.frame = {{"this", object}};
    ctor.blocks.resize(1);
    ctor.blocks[0].instrs = {effect(s::Op::ObjectStageBegin), effect(s::Op::ObjectStageBody),
                             effect(s::Op::ObjectStageEnd), effect(s::Op::Ret)};
    d.subroutines.push_back(std::move(ctor));
    s::Process p;
    p.name = "top.initial";
    p.loc = {0, 1, 1};
    p.numTemps = 1;
    p.blocks.resize(1);
    p.blocks[0].instrs = {{s::Op::ObjectNew, object, s::TempId{0}, {}, {0, 1, 1}},
                          effect(s::Op::Ret)};
    d.processes.push_back(std::move(p));
    return d;
}
void unchanged(const k::Context& ctx) {
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().stats().cells, 0U);
    EXPECT_EQ(ctx.heap().stats().containers, 0U);
    EXPECT_EQ(ctx.heap().objectStats().live, 0U);
    EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
    EXPECT_EQ(ctx.strings().stats().live, 0U);
    EXPECT_EQ(ctx.activations().stats().live, 0U);
}
TEST(ClassVmGuard, ValidNewClassSIRStopsBeforeRuntimeAllocationUntilTheAdapterIsConnected) {
    const auto d = design();
    ASSERT_TRUE(s::verify(d).empty());
    k::Context context;
    const auto result = kirimasim::vm::run(d, context);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_NE(result.error.find("class SIR execution adapter"), std::string::npos);
    unchanged(context);
}
TEST(ClassVmGuard, MalformedClassesAndMissingDescriptorsFailBeforeValuesAreInterned) {
    for (const bool missing : {false, true}) {
        auto d = design();
        if (missing)
            d.classes.clear();
        else
            d.classes[0].fields[0].type.object = s::ClassId{0};
        ASSERT_TRUE(s::hasClassMetadata(d));
        ASSERT_FALSE(s::verify(d).empty());
        k::Context context;
        const auto result = kirimasim::vm::run(d, context);
        EXPECT_EQ(result.reason, k::EndReason::Internal);
        EXPECT_EQ(result.error, "invalid class SIR");
        unchanged(context);
    }
}
}  // namespace
