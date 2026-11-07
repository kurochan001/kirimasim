// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/module_path.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verified.h"
#include "kirimasim/valsem/module_path.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using O = s::Operand;
constexpr s::SourceLoc loc{0, 7, 3};
struct Model {
    s::Design d;
    Model() {
        d.sources = {"module_path.sv"};
        d.instances = {{"top", "top", s::kNone}};
        d.slots = {{"top.a", s::u4(1), s::SlotKind::Wire},
                   {"top.q", s::u4(1), s::SlotKind::Wire},
                   {"top.q.$driver0", s::u4(1), s::SlotKind::Wire}};
        d.slots[1].flags = s::kNeedsStrength | s::kMultiDriver;
        d.slots[2].driverOf = s::SlotId{1};
        d.strengthNodes = {{{s::SlotId{1}, 0, 1, 0, 1}, s::SlotKind::Wire, 0, {-6, 6}, loc}};
        d.strengthOrigins = {{0, s::ProcessId{1}, {-6, 6}, s::StrengthOriginKind::Process, loc}};
        d.constants = {{s::u2(3), {6}}, {s::u4(1), {1}, {0}}};
        d.processes.resize(3);
        for (unsigned p = 0; p < d.processes.size(); ++p) {
            auto& process = d.processes[p];
            process.name = "top.p" + std::to_string(p);
            process.kind = s::ProcessKind::Cont;
            process.loc = loc;
            process.blocks.resize(1);
        }
        auto& writer = d.processes[0];
        writer.writes = writer.drives = {{s::SlotId{2}}};
        writer.triggers = {{{s::SlotId{0}}, s::Edge::Any}};
        writer.numTemps = 1;
        writer.blocks[0].instrs = {
            {s::Op::Load, s::u4(1), s::TempId{0}, {O::slot(s::SlotId{0})}, loc},
            {s::Op::Store,
             s::voidType(),
             s::kNone,
             {O::slot(s::SlotId{2}), O::temp(s::TempId{0})},
             loc},
            {s::Op::Ret, s::voidType(), s::kNone, {}, loc}};
        auto& publisher = d.processes[1];
        publisher.netResolver = true;
        publisher.strengthWrites = {s::StrengthOriginId{0}};
        publisher.triggers = {{{s::SlotId{2}}, s::Edge::Any}};
        publisher.numTemps = 2;
        publisher.blocks[0].instrs = {
            {s::Op::Load, s::u4(1), s::TempId{0}, {O::slot(s::SlotId{2})}, loc},
            {s::Op::StrengthMake,
             s::strengthType(1),
             s::TempId{1},
             {O::temp(s::TempId{0}), O::constant(s::ConstId{0}), O::constant(s::ConstId{0})},
             loc},
            {s::Op::StrengthDrive,
             s::voidType(),
             s::kNone,
             {O::strengthOrigin(s::StrengthOriginId{0}), O::temp(s::TempId{1})},
             loc},
            {s::Op::Ret, s::voidType(), s::kNone, {}, loc}};
        auto& source = d.processes[2];
        source.writes = source.drives = {{s::SlotId{0}}};
        source.blocks[0].instrs = {{s::Op::Store,
                                    s::voidType(),
                                    s::kNone,
                                    {O::slot(s::SlotId{0}), O::constant(s::ConstId{1})},
                                    loc},
                                   {s::Op::Ret, s::voidType(), s::kNone, {}, loc}};
        s::ModulePath path;
        path.source = {s::SlotId{0}, 0, 1, 0, 1};
        path.destination = {s::SlotId{1}, 0, 1, 0, 1};
        path.functional = {s::SlotId{2}, 0, 1, 0, 1};
        path.origin = s::StrengthOriginId{0};
        path.functionalOwner = s::ProcessId{0};
        path.delays.fill(7);
        path.loc = loc;
        d.modulePaths = {path};
    }
};
TEST(ModulePathSir, NormalizationMatchesIndependentEndpointReferenceIncludingOverflow) {
    constexpr std::array<std::uint64_t, 12> input{17, 5, 13, 7, 19, 11, 3, 23, 2, 29, 31, 37};
    for (unsigned n : {1, 2, 3, 6, 12}) {
        std::vector<std::optional<std::uint64_t>> values(input.begin(), input.begin() + n);
        const auto normalized = s::normalizeModulePathDelays(values);
        ASSERT_TRUE(normalized.has_value());
        EXPECT_EQ(normalized, k::modulePathDelays(std::span(input).first(n)));
    }
    const std::array<std::optional<std::uint64_t>, 6> values{17, std::nullopt, 13, 7, 19, 11};
    const auto normalized = s::normalizeModulePathDelays(values);
    ASSERT_TRUE(normalized.has_value());
    const auto d = normalized.value_or(s::ModulePathDelays{});
    EXPECT_EQ(d[6], 13U);
    EXPECT_EQ(d[8], 19U);
    EXPECT_FALSE(d[9].has_value());  // latest possible endpoint out of X overflows
    EXPECT_EQ(d[11], 7U);
    EXPECT_FALSE(s::normalizeModulePathDelays({}));
}
TEST(ModulePathSir, VerifiedRowsPrintDeterministicallyAndExecuteThroughSharedKernel) {
    Model m;
    for (const auto& error : s::verify(m.d))
        ADD_FAILURE() << error.where << ": " << error.message;
    ASSERT_TRUE(s::verify(m.d).empty());
    const auto text = s::toText(m.d);
    EXPECT_NE(text.find("module-path !p0"), std::string::npos);
    EXPECT_NE(text.find("origin=!d0 edge=any delays=[7,7,7,7,7,7,7,7,7,7,7,7]"), std::string::npos);
    EXPECT_EQ(text, s::toText(m.d));
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(ctx.scheduler().time(), 7U);
    ASSERT_EQ(ctx.strengthDrivers().originCount(), 1U);
    const auto state = ctx.strengthDrivers().origin(0);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state.value_or(k::StrengthOriginState{}).committed, (k::StrengthRange{6, 6}));
    EXPECT_EQ(ctx.store().element(k::SlotId{1}, 0)[0] & 1U, 1U);
    EXPECT_EQ(ctx.store().element(k::SlotId{1}, 0)[1] & 1U, 0U);
}
TEST(ModulePathSir, MalformedOwnershipAndBindingsFailBeforeAllocatingRuntimeSlots) {
    for (unsigned mode = 0; mode < 23; ++mode) {
        Model m;
        auto& p = m.d.modulePaths[0];
        switch (mode) {
            case 0: p.source.slot = s::SlotId{99}; break;
            case 1: p.source.width = 0; break;
            case 2: p.source.numElems = 0; break;
            case 3: p.source.firstElem = 1; break;
            case 4: p.source.lsb = 1; break;
            case 5: m.d.slots[0].kind = s::SlotKind::Var; break;
            case 6: m.d.slots[0].type = s::u2(1); break;
            case 7: p.destination.slot = s::SlotId{0}; break;
            case 8: p.functional.slot = s::SlotId{1}; break;
            case 9: p.origin = s::StrengthOriginId{9}; break;
            case 10: p.functionalOwner = s::ProcessId{9}; break;
            case 11: p.functionalOwner = s::ProcessId{2}; break;
            case 12: p.instance = s::InstanceId{9}; break;
            case 13: p.edge = s::Edge::Both; break;
            case 14: p.loc = {}; break;
            case 15: m.d.modulePaths.push_back(p); break;
            case 16: m.d.strengthOrigins.push_back(m.d.strengthOrigins[0]); break;
            case 17: m.d.strengthEdges.push_back({0, 0, s::StrengthEdgeKind::Identity}); break;
            case 18: m.d.processes[1].netResolver = false; break;
            case 19:
                m.d.processes[1].blocks[0].instrs[0].operands[0] = O::slot(s::SlotId{0});
                break;
            case 20: m.d.processes[0].writes.clear(); break;
            case 21:
                m.d.strengthNodes.clear();
                m.d.strengthOrigins.clear();
                break;
            case 22: m.d.processes[1].blocks[0].instrs[2].op = s::Op::StrengthDriveDelay; break;
            default: FAIL() << "unknown malformed row";
        }
        SCOPED_TRACE(mode);
        ASSERT_FALSE(s::verify(m.d).empty());
        // The gate keeps this Design from any run (docs/notes/verified-design.md).
        const auto outcome = s::verifyDesign(std::move(m.d));
        EXPECT_FALSE(outcome.verified.has_value());
        EXPECT_FALSE(outcome.errors.empty());
    }
}
void withCondition(Model& model, unsigned logic = 1) {
    auto& d = model.d;
    d.constants.push_back({s::u4(1), {logic & 1U}, {logic >> 1}});
    s::Subroutine condition;
    condition.name = "top.$path";
    condition.kind = s::SubroutineKind::PathCondition;
    condition.returnType = s::u4(1);
    condition.loc = loc;
    condition.blocks.resize(1);
    condition.blocks[0].instrs = {
        {s::Op::Ret, s::voidType(), s::kNone, {O::constant(s::ConstId{2})}, loc}};
    d.subroutines.push_back(std::move(condition));
    auto& path = d.modulePaths[0];
    path.enable = s::ModulePathEnable::Condition;
    path.condition = s::SubroutineId{0};
    path.group = 17;
    path.sourceDeclaration = path.source.slot;
    path.destinationDeclaration = path.destination.slot;
    path.delays.fill(9);
    auto fallback = path;
    fallback.enable = s::ModulePathEnable::IfNone;
    fallback.condition = s::kNone;
    fallback.delays.fill(3);
    d.modulePaths.push_back(fallback);
}
TEST(ModulePathSirConditions, FourStateEvaluatorsPrintAndExecuteWithDetachedOwnerAfterRun) {
    for (unsigned logic = 0; logic < 4; ++logic) {
        Model model;
        withCondition(model, logic);
        for (const auto& error : s::verify(model.d))
            ADD_FAILURE() << error.where << ": " << error.message;
        ASSERT_TRUE(s::verify(model.d).empty());
        const auto text = s::toText(model.d);
        EXPECT_NE(text.find("path-condition"), std::string::npos);
        EXPECT_NE(text.find("group=17 condition=@0"), std::string::npos);
        EXPECT_NE(text.find("group=17 ifnone"), std::string::npos);
        EXPECT_EQ(text, s::toText(model.d));
        k::Context context;
        const auto result = kirimasim::vm::run(model.d, context);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(context.scheduler().time(), logic == 0 ? 3U : 9U);
        EXPECT_EQ(context.store().element(k::SlotId{1}, 0)[0] & 1U, 1U);
        // Interpreter storage has gone away, while Context still owns paths.
        EXPECT_EQ(context.scheduler().driveStrength(0, {-6, -6}), k::StrengthError::InvalidValue);
        const auto origin = context.strengthDrivers().origin(0);
        ASSERT_TRUE(origin.has_value());
        EXPECT_EQ(origin.value_or(k::StrengthOriginState{}).committed, (k::StrengthRange{6, 6}));
    }
}
TEST(ModulePathSirConditions, MalformedEvaluatorAndGroupMetadataFailsBeforeRuntimeAllocation) {
    for (unsigned mode = 0; mode < 33; ++mode) {
        SCOPED_TRACE(mode);
        Model model;
        withCondition(model);
        auto& d = model.d;
        auto& path = d.modulePaths[0];
        auto& condition = d.subroutines[0];
        switch (mode) {
            case 0: path.enable = static_cast<s::ModulePathEnable>(99); break;
            case 1: path.condition = s::SubroutineId{9}; break;
            case 2: condition.kind = s::SubroutineKind::Function; break;
            case 3: condition.instance = s::InstanceId{9}; break;
            case 4: condition.returnType = s::u2(1); break;
            case 5: condition.automatic = true; break;
            case 6: condition.frame.push_back({"bad", s::u4(1)}); break;
            case 7: condition.params.push_back({"bad", s::Direction::Input, s::u4(1)}); break;
            case 8: condition.frameOf = s::Owner::of(s::ProcessId{0}); break;
            case 9: condition.mayWait = true; break;
            case 10:
                condition.blocks[0].instrs.insert(
                    condition.blocks[0].instrs.begin(),
                    {s::Op::Store,
                     s::voidType(),
                     s::kNone,
                     {O::slot(s::SlotId{0}), O::constant(s::ConstId{2})},
                     loc});
                break;
            case 11:
                condition.blocks[0].instrs.insert(
                    condition.blocks[0].instrs.begin(),
                    {s::Op::Syscall, s::voidType(), s::kNone, {O::str("$display")}, loc});
                break;
            case 12: condition.blocks[0].instrs[0].operands[0] = O::frame(s::FrameVarId{0}); break;
            case 13: path.group = UINT32_MAX; break;
            case 14: path.edge = s::Edge::Pos; break;
            case 15: d.modulePaths[1].group = 18; break;
            case 16: d.modulePaths[1].condition = s::SubroutineId{0}; break;
            case 17: d.modulePaths.push_back(d.modulePaths[1]); break;
            case 18: {
                auto unconditional = path;
                unconditional.enable = s::ModulePathEnable::Always;
                unconditional.condition = s::kNone;
                unconditional.group = UINT32_MAX;
                d.modulePaths.push_back(unconditional);
                break;
            }
            case 19: condition.classRole = s::ClassRole::StaticMethod; break;
            case 20: condition.activation.scopes.push_back({}); break;
            case 21:
                condition.blocks[0].instrs = {
                    {s::Op::Br, s::voidType(), s::kNone, {O::block(s::BlockId{0})}, loc}};
                break;
            case 22:
            case 23:
            case 24: {
                auto slot = s::SlotId{2};
                if (mode != 22) {
                    slot = s::SlotId{0};
                    if (mode == 23)
                        d.slots[0].instance = s::InstanceId{9};
                    else
                        d.slots[0].subroutine = s::SubroutineId{0};
                }
                condition.numTemps = 1;
                condition.blocks[0].instrs = {
                    {s::Op::Load, s::u4(1), s::TempId{0}, {O::slot(slot)}, loc},
                    {s::Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{0})}, loc}};
                break;
            }
            case 25: d.constants[2].type = {s::TypeKind::Real, 0}; break;
            case 26: {
                auto& writer = d.processes[0];
                writer.numTemps = 2;
                writer.blocks[0].instrs.insert(writer.blocks[0].instrs.begin(),
                                               {s::Op::Call,
                                                s::u4(1),
                                                s::TempId{1},
                                                {O::subroutine(s::SubroutineId{0})},
                                                loc});
                break;
            }
            case 27:
                condition.numTemps = 1;
                condition.blocks[0].instrs = {
                    {s::Op::Bitsel,
                     s::u4(1),
                     s::TempId{0},
                     {O::constant(s::ConstId{2}), O::temp(s::TempId{0})},
                     loc},
                    {s::Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{0})}, loc}};
                break;
            case 28:
                d.slots.push_back({"top.alias", s::u4(1), s::SlotKind::Wire});
                d.slots.back().viewOf = s::SlotId{2};
                condition.numTemps = 1;
                condition.blocks[0].instrs = {
                    {s::Op::Load, s::u4(1), s::TempId{0}, {O::slot(s::SlotId{3})}, loc},
                    {s::Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{0})}, loc}};
                break;
            case 29:
                d.modulePaths.clear();
                d.strengthNodes.clear();
                d.strengthOrigins.clear();
                d.processes.clear();
                d.slots.clear();
                condition.frame.push_back({"bad", s::u4(1)});
                break;
            case 30: path.sourceDeclaration = s::kNone; break;
            case 31: path.sourceDeclaration = s::SlotId{9}; break;
            case 32: path.destinationDeclaration = path.source.slot; break;
            default: FAIL();
        }
        ASSERT_FALSE(s::verify(d).empty());
        // The gate keeps this Design from any run (docs/notes/verified-design.md).
        const auto outcome = s::verifyDesign(std::move(d));
        EXPECT_FALSE(outcome.verified.has_value());
        EXPECT_FALSE(outcome.errors.empty());
    }
}
TEST(ModulePathSirConditions, ScalarLocalNetViewReadsTheSamePhysicalPublicStorage) {
    Model model;
    withCondition(model);
    auto& d = model.d;
    d.slots.push_back({"top.alias", s::u4(1), s::SlotKind::Wire});
    d.slots.back().viewOf = s::SlotId{0};
    auto& condition = d.subroutines[0];
    condition.numTemps = 1;
    condition.blocks[0].instrs = {
        {s::Op::Load, s::u4(1), s::TempId{0}, {O::slot(s::SlotId{3})}, loc},
        {s::Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{0})}, loc}};
    ASSERT_TRUE(s::verify(d).empty());
    k::Context context;
    const auto result = kirimasim::vm::run(d, context);
    EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(context.store().size(), 3U);
    EXPECT_EQ(context.scheduler().time(), 9U);
}
TEST(ModulePathSirConditions, DeclarationAliasesRemainDistinctThroughVmConfiguration) {
    for (bool aliasDestination : {false, true}) {
        Model model;
        withCondition(model);
        auto& d = model.d;
        d.slots.push_back({"top.alias", s::u4(1), s::SlotKind::Wire});
        d.slots.back().viewOf = aliasDestination ? s::SlotId{1} : s::SlotId{0};
        d.constants.push_back({s::u4(1), {0}, {0}});
        auto condition = d.subroutines[0];
        condition.name = "top.$other";
        condition.blocks[0].instrs[0].operands[0] = O::constant(s::ConstId{3});
        d.subroutines.push_back(std::move(condition));
        d.modulePaths[1].delays.fill(3);
        auto second = d.modulePaths[0];
        second.condition = s::SubroutineId{1};
        second.group = 18;
        second.delays.fill(11);
        if (aliasDestination)
            second.destinationDeclaration = s::SlotId{3};
        else
            second.sourceDeclaration = s::SlotId{3};
        auto otherFallback = second;
        otherFallback.enable = s::ModulePathEnable::IfNone;
        otherFallback.condition = s::kNone;
        otherFallback.delays.fill(7);
        d.modulePaths.push_back(second);
        d.modulePaths.push_back(otherFallback);
        for (const auto& error : s::verify(d))
            ADD_FAILURE() << error.where << ": " << error.message;
        ASSERT_TRUE(s::verify(d).empty());
        k::Context context;
        const auto result = kirimasim::vm::run(d, context);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(context.scheduler().time(), 7U);
    }
}
}  // namespace
