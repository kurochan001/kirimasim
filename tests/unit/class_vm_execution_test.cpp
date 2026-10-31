// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Original SIR execution matrices, approved class contract 1, 3-6, 8-10.
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <memory_resource>
#include <string>
#include <utility>
#include <vector>

#include "kirimasim/sir/classes.h"
#include "kirimasim/valsem/objects.h"
#include "kirimasim/vm/vm.h"
#include "gtest/gtest.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
using O = s::Operand;
using Op = s::Op;
using W = k::W4;
s::Instr effect(Op op, std::vector<O> ops = {}) {
    return {op, s::voidType(), s::kNone, std::move(ops), {0, 1, 1}};
}
struct Program {
    s::Design d;
    s::Type scalar;
    explicit Program(s::Type type = s::u4(32)) : scalar(type) {
        d.sources = {"class-execution.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::TypeEntry q;
        q.kind = s::TypeEntryKind::Queue;
        q.value = s::classType(s::ClassId{1});
        d.types = {q};
        d.classes.resize(4);
        d.classes[0].name = "Derived";
        d.classes[0].base = s::ClassId{1};
        d.classes[0].fields = {{"value", s::u2(32), {}, s::kNone, false, true}};
        d.classes[1].name = "Base";
        d.classes[1].fields = {{"value", scalar, {}, s::kNone, false, true},
                               {"next", s::classType(s::ClassId{1})},
                               {"array", scalar, {{2, 0}}},
                               {"constant", scalar, {}, s::kNone, true, false}};
        d.classes[2].name = "Other";
        d.classes[3].name = "Weak";
        d.classes[3].weakTarget = s::ClassId{1};
        for (unsigned owner = 0; owner < 4; ++owner) {
            auto ctor = member(owner);
            ctor.classRole = s::ClassRole::Constructor;
            auto& code = ctor.blocks[0].instrs;
            if (owner == 0) code.push_back(effect(Op::ObjectBaseInit));
            code.push_back(effect(Op::ObjectStageBegin));
            if (owner < 2)
                code.push_back(effect(Op::ObjectInit, {O::classField({s::ClassId{owner}, 0}),
                                                       number(owner == 0 ? s::u2(32) : scalar,
                                                              owner == 0 ? 42 : 7)}));
            code.push_back(effect(Op::ObjectStageBody));
            if (owner == 1)
                code.push_back(
                    effect(Op::ObjectInit, {O::classField({s::ClassId{1}, 3}), number(scalar, 9)}));
            code.push_back(effect(Op::ObjectStageEnd));
            code.push_back(effect(Op::Ret));
            d.classes[owner].constructor = add(std::move(ctor));
        }
        d.virtualSlots = {{"get", s::ClassId{1}, s::SubroutineKind::Function, s::u2(64)}};
        for (unsigned owner = 0; owner < 4; ++owner) {
            d.classes[owner].virtualMethods = {s::SubroutineId{s::kNone}};
            if (owner > 1) continue;
            auto method = member(owner);
            method.returnType = s::u2(64);
            method.blocks[0].instrs = {effect(Op::Ret, {number(s::u2(64), owner == 0 ? 22 : 11)})};
            d.classes[owner].virtualMethods[0] = add(std::move(method));
        }
        d.slots = {{"top.object", s::classType(s::ClassId{1})},
                   {"top.second", s::classType(s::ClassId{1})},
                   {"top.result", s::u2(64)},
                   {"top.counter", s::u2(64)},
                   {"top.queue", s::queueType(s::TypeId{0})},
                   {"top.objects", s::classType(s::ClassId{1}), s::SlotKind::Var, {{0, 2}}},
                   {"top.value", scalar},
                   {"top.flag", s::u2(1)}};
        process();
    }
    O number(s::Type type, std::uint64_t a = 0, std::uint64_t b = 0) {
        s::Constant c;
        c.type = type;
        const auto count = s::isIntegral(type) ? s::integralWords(type.width) : 1;
        c.aWords.resize(count, 0);
        c.aWords[0] = a;
        if (s::isIntegral(type) && type.width < 64)
            c.aWords[0] &= (std::uint64_t{1} << type.width) - 1;
        if (s::isFourState(type)) {
            c.bWords.resize(count, 0);
            c.bWords[0] = b;
            if (type.width < 64) c.bWords[0] &= (std::uint64_t{1} << type.width) - 1;
        }
        const s::ConstId id{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back(std::move(c));
        return O::constant(id);
    }
    s::Subroutine member(unsigned owner) {
        s::Subroutine sub;
        sub.name = "top.method";
        sub.instance = s::InstanceId{0};
        sub.loc = {0, 1, 1};
        sub.automatic = true;
        sub.classRole = s::ClassRole::Method;
        sub.classOwner = s::ClassId{owner};
        sub.params = {{"this", s::Direction::Input, s::classType(sub.classOwner), s::kNone,
                       s::FrameVarId{0}}};
        sub.frame = {{"this", s::classType(sub.classOwner)}};
        sub.blocks.resize(1);
        return sub;
    }
    s::SubroutineId add(s::Subroutine sub) {
        const s::SubroutineId id{static_cast<std::uint32_t>(d.subroutines.size())};
        d.subroutines.push_back(std::move(sub));
        return id;
    }
    unsigned process() {
        s::Process p;
        p.name = "top.initial";
        p.loc = {0, 1, 1};
        p.blocks.resize(1);
        p.blocks[0].instrs = {effect(Op::Ret)};
        for (unsigned i = 0; i < d.slots.size(); ++i)
            p.writes.push_back({s::SlotId{i}});
        d.processes.push_back(std::move(p));
        return static_cast<unsigned>(d.processes.size() - 1);
    }
    O emit(Op op, s::Type type = s::voidType(), std::vector<O> ops = {}, unsigned p = 0,
           unsigned b = 0) {
        auto& process = d.processes[p];
        if (process.blocks.size() <= b) process.blocks.resize(b + 1);
        auto& code = process.blocks[b].instrs;
        if (!code.empty() && code.back().op == Op::Ret) code.pop_back();
        const s::TempId result =
            type == s::voidType() ? s::TempId{s::kNone} : s::TempId{process.numTemps++};
        code.push_back({op, type, result, std::move(ops), {0, 1, 1}});
        if (!s::isTerminator(op)) code.push_back(effect(Op::Ret));
        return O::temp(result);
    }
    void store(unsigned slot, const O& value, unsigned p = 0, unsigned b = 0) {
        emit(Op::Store, s::voidType(), {O::slot(s::SlotId{slot}), value}, p, b);
    }
    O object(unsigned owner = 0, unsigned p = 0, unsigned b = 0) {
        return emit(Op::ObjectNew, s::classType(s::ClassId{owner}), {}, p, b);
    }
    O read(const O& object, unsigned field = 0, unsigned p = 0, unsigned b = 0) {
        return emit(Op::ObjectRead, scalar, {O::classField({s::ClassId{1}, field}), object}, p, b);
    }
    void churn(unsigned p, unsigned start, unsigned count = 400) {
        auto& process = d.processes[p];
        process.blocks.resize(start + 3);
        const s::TempId index{process.numTemps++};
        process.blocks[start + 1].params.push_back({index, s::u2(64)});
        emit(Op::Br, s::voidType(), {O::block(s::BlockId{start + 1}, 1), number(s::u2(64))}, p,
             start);
        object(1, p, start + 1);
        const auto next =
            emit(Op::Add, s::u2(64), {O::temp(index), number(s::u2(64), 1)}, p, start + 1);
        const auto again = emit(Op::Lt, s::u2(1), {next, number(s::u2(64), count)}, p, start + 1);
        emit(Op::BrIf, s::voidType(),
             {again, O::block(s::BlockId{start + 1}, 1), next, O::block(s::BlockId{start + 2})}, p,
             start + 1);
        process.blocks[start + 2].instrs = {effect(Op::Ret)};
    }
    std::string errors() const {
        std::string result;
        for (const auto& error : s::verify(d))
            result += error.where + ": " + error.message + "\n";
        return result;
    }
    std::uint64_t slot(k::Context& ctx, unsigned id, unsigned element = 0) const {
        return *ctx.store().element(k::SlotId{id}, element);
    }
};

TEST(ClassVmExecution, ConstructorsHiddenFieldsAndVirtualCallsExecuteThroughStaticBaseViews) {
    Program p;
    const auto derived = p.object();
    const auto base = p.emit(Op::ObjectView, s::classType(s::ClassId{1}), {derived});
    p.store(0, base);
    const auto value = p.read(base);
    p.store(6, value);
    const auto call =
        p.emit(Op::ObjectCall, s::u2(64), {O::virtualSlot(s::VirtualSlotId{0}), base});
    p.store(2, call);
    const auto direct = p.emit(Op::ObjectCallDirect, s::u2(64),
                               {O::subroutine(p.d.classes[1].virtualMethods[0]), base});
    p.store(3, direct);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 6), 7U);
    EXPECT_EQ(p.slot(ctx, 2), 22U);
    EXPECT_EQ(p.slot(ctx, 3), 11U);
    const k::ObjectToken token{p.slot(ctx, 0), &ctx.heap()};
    const auto info = ctx.heap().objectInfo(token);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info.value.constructed);
    EXPECT_EQ(info.value.dynamicClass, k::ObjectClassId{0});
    auto hidden = ctx.heap().objectRef(token, {k::ObjectClassId{0}, 0});
    ASSERT_TRUE(hidden);
    W word{};
    ASSERT_EQ(ctx.heap().readObject(hidden.value, {&word, 1}), k::ObjectError::None);
    EXPECT_EQ(word.a, 42U);
}

TEST(ClassVmExecution, WidePropertiesAndArrayElementsExecuteWithTypedDefaultsAndConstStages) {
    for (const auto width : {1U, 32U, 65U, 129U, 257U})
        for (const bool four : {false, true}) {
            Program p(four ? s::u4(width) : s::u2(width));
            const auto object = p.object(1);
            p.store(0, object);
            p.emit(Op::ObjectWrite, s::voidType(),
                   {O::classField({s::ClassId{1}, 2}), object, p.number(s::s2(32), 1),
                    p.number(p.scalar, 5)});
            const auto read =
                p.emit(Op::ObjectRead, p.scalar,
                       {O::classField({s::ClassId{1}, 2}), object, p.number(s::s2(32), 1)});
            p.store(6, read);
            ASSERT_EQ(p.errors(), "");
            k::Context ctx;
            const auto result = kirimasim::vm::run(p.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error << width << four;
            EXPECT_EQ(p.slot(ctx, 6), 5U & ((width == 1) ? 1U : UINT64_MAX));
            const auto objectInfo = ctx.heap().objectInfo({p.slot(ctx, 0), &ctx.heap()});
            ASSERT_TRUE(objectInfo);
            EXPECT_TRUE(objectInfo.value.constructed);
        }
}

TEST(ClassVmExecution, EqualityTruthAndUnknownConditionalUseObjectIdentityRatherThanWidthZero) {
    Program p;
    const auto a = p.object(1), b = p.object(1);
    const auto equal = p.emit(Op::Eq, s::u2(1), {a, b});
    p.store(7, equal);
    const auto selected =
        p.emit(Op::Select, s::classType(s::ClassId{1}), {p.number(s::u4(1), 1, 1), a, b});
    p.store(0, selected);
    const auto yes = p.emit(Op::Truth, s::u2(1), {a});
    const auto extended = p.emit(Op::Zext, s::u2(64), {yes});
    p.store(2, extended);
    const auto shared =
        p.emit(Op::Select, s::classType(s::ClassId{1}), {p.number(s::u4(1), 0, 1), a, a});
    p.store(1, shared);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 7), 0U);
    EXPECT_EQ(p.slot(ctx, 0), 0U);
    EXPECT_EQ(p.slot(ctx, 2), 1U);
    EXPECT_NE(p.slot(ctx, 1), 0U);
}

TEST(ClassVmExecution, CopyCastsAndWeakOperationsPreserveTypedIdentityAndDynamicClass) {
    Program p;
    const auto a = p.object();
    const auto base = p.emit(Op::ObjectView, s::classType(s::ClassId{1}), {a});
    p.store(0, base);
    const auto copied = p.emit(Op::ObjectCopy, s::classType(s::ClassId{1}), {base});
    p.store(1, copied);
    const auto cast = p.emit(Op::ObjectCastCheck, s::u2(1), {O::classRow(s::ClassId{0}), base});
    p.store(7, cast);
    const auto weak = p.emit(Op::ObjectWeakNew, s::classType(s::ClassId{3}), {base});
    const auto locked = p.emit(Op::ObjectWeakLock, s::classType(s::ClassId{1}), {weak});
    const auto id = p.emit(Op::ObjectId, s::u2(64), {locked});
    p.store(2, id);
    p.emit(Op::ObjectWeakClear, s::voidType(), {weak});
    const auto empty = p.emit(Op::ObjectWeakLock, s::classType(s::ClassId{1}), {weak});
    const auto nullId = p.emit(Op::ObjectId, s::u2(64), {empty});
    p.store(3, nullId);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 7), 1U);
    EXPECT_EQ(p.slot(ctx, 3), 0U);
    EXPECT_NE(p.slot(ctx, 0), p.slot(ctx, 1));
    const auto aInfo = ctx.heap().objectInfo({p.slot(ctx, 0), &ctx.heap()}),
               bInfo = ctx.heap().objectInfo({p.slot(ctx, 1), &ctx.heap()});
    ASSERT_TRUE(aInfo);
    ASSERT_TRUE(bInfo);
    EXPECT_EQ(bInfo.value.dynamicClass, k::ObjectClassId{0});
    EXPECT_EQ(p.slot(ctx, 2), aInfo.value.identity);
}

TEST(ClassVmExecution, NullAccessAndRepeatedInstanceConstWritesProducePositionedDesignErrors) {
    for (const bool nullAccess : {false, true}) {
        Program p;
        if (nullAccess)
            p.read(p.number(s::classType(s::ClassId{1})));
        else {
            const auto object = p.object(1);
            const auto ctor = p.d.classes[1].constructor;
            auto& code = p.d.subroutines[ctor.index()].blocks[0].instrs;
            code.insert(code.end() - 2, effect(Op::ObjectInit, {O::classField({s::ClassId{1}, 3}),
                                                                p.number(p.scalar, 13)}));
            p.store(0, object);
        }
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        const auto result = kirimasim::vm::run(p.d, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
    }
}

TEST(ClassVmExecution,
     WaitingNestedMethodsRetainReceiversAndResolvedSitesAfterRebindingAndCollection) {
    for (const bool dynamic : {false, true}) {
        SCOPED_TRACE(dynamic);
        Program p;
        auto inner = p.member(1);
        inner.kind = s::SubroutineKind::Task;
        inner.mayWait = true;
        inner.numTemps = 2;
        inner.blocks.resize(2);
        inner.blocks[0].instrs = {
            effect(Op::WaitDelay, {p.number(s::u2(64), 5), O::block(s::BlockId{1})})};
        inner.blocks[1].instrs = {{Op::Fload,
                                   s::classType(s::ClassId{1}),
                                   s::TempId{0},
                                   {O::frame(s::FrameVarId{0})},
                                   {0, 1, 1}},
                                  {Op::ObjectRead,
                                   p.scalar,
                                   s::TempId{1},
                                   {O::classField({s::ClassId{1}, 0}), O::temp(s::TempId{0})},
                                   {0, 1, 1}},
                                  effect(Op::Store, {O::slot(s::SlotId{6}), O::temp(s::TempId{1})}),
                                  effect(Op::Ret)};
        const auto innerId = p.add(std::move(inner));
        auto outer = p.member(1);
        outer.kind = s::SubroutineKind::Task;
        outer.mayWait = true;
        outer.numTemps = 1;
        outer.blocks.resize(2);
        outer.blocks[0].instrs = {
            {Op::Fload,
             s::classType(s::ClassId{1}),
             s::TempId{0},
             {O::frame(s::FrameVarId{0})},
             {0, 1, 1}},
            effect(Op::ObjectCallTaskDirect,
                   {O::subroutine(innerId), O::temp(s::TempId{0}), O::block(s::BlockId{1})})};
        outer.blocks[1].instrs = {effect(Op::Ret)};
        const auto outerId = p.add(std::move(outer));
        p.d.virtualSlots.push_back(
            {"wait", s::ClassId{1}, s::SubroutineKind::Task, s::voidType(), {}, true});
        for (unsigned c = 0; c < p.d.classes.size(); ++c) {
            p.d.classes[c].virtualMethods.resize(2, s::SubroutineId{s::kNone});
            if (c < 2) p.d.classes[c].virtualMethods[1] = outerId;
        }
        const auto receiver = p.object(1);
        p.store(0, receiver);
        p.emit(dynamic ? Op::ObjectCallTask : Op::ObjectCallTaskDirect, s::voidType(),
               {dynamic ? O::virtualSlot(s::VirtualSlotId{1}) : O::subroutine(outerId), receiver,
                O::block(s::BlockId{1})});
        p.d.processes[0].blocks.resize(2);
        p.d.processes[0].blocks[1].instrs = {effect(Op::Ret)};
        const auto writer = p.process();
        p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})},
               writer);
        p.store(0, p.number(s::classType(s::ClassId{1})), writer, 1);
        p.churn(writer, 1);
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        auto result = kirimasim::vm::run(p.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(p.slot(ctx, 0), 0U);
        EXPECT_EQ(p.slot(ctx, 6), 7U);
        EXPECT_LT(ctx.heap().objectStats().live, 400U);
    }
}

TEST(ClassVmExecution, StaticSlotsArraysContainersAndObjectEdgesAreIndependentStrongRoots) {
    for (unsigned kind = 0; kind < 4; ++kind) {
        SCOPED_TRACE(kind);
        Program p;
        p.d.classes[1].fields.push_back({"text", {s::TypeKind::String, 0}});
        const auto object = p.object(1);
        if (kind == 0) p.store(0, object);
        if (kind == 1)
            p.emit(Op::StoreElem, s::voidType(),
                   {O::slot(s::SlotId{5}), p.number(s::s2(32), 1), object});
        if (kind == 2) p.store(4, p.emit(Op::ContainerPack, s::queueType(s::TypeId{0}), {object}));
        if (kind == 3) {
            const auto holder = p.object(1);
            p.emit(Op::ObjectWrite, s::voidType(),
                   {O::classField({s::ClassId{1}, 1}), holder, object});
            p.store(0, holder);
        }
        const auto consumer = p.process();
        p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})},
               consumer);
        p.churn(consumer, 1);
        O read;
        if (kind == 0 || kind == 3)
            read =
                p.emit(Op::Load, s::classType(s::ClassId{1}), {O::slot(s::SlotId{0})}, consumer, 3);
        if (kind == 1)
            read = p.emit(Op::LoadElem, s::classType(s::ClassId{1}),
                          {O::slot(s::SlotId{5}), p.number(s::s2(32), 1)}, consumer, 3);
        if (kind == 2) {
            const auto queue =
                p.emit(Op::Load, s::queueType(s::TypeId{0}), {O::slot(s::SlotId{4})}, consumer, 3);
            read = p.emit(Op::ContainerRead, s::classType(s::ClassId{1}),
                          {queue, p.number(s::s2(32))}, consumer, 3);
        }
        if (kind == 3)
            read = p.emit(Op::ObjectRead, s::classType(s::ClassId{1}),
                          {O::classField({s::ClassId{1}, 1}), read}, consumer, 3);
        p.store(6, p.read(read, 0, consumer, 3), consumer, 3);
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        const auto result = kirimasim::vm::run(p.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(p.slot(ctx, 6), 7U);
        EXPECT_LT(ctx.heap().objectStats().live, 400U);
    }
}

TEST(ClassVmExecution, NativeSsaHandlesRemainRootsAcrossAllocatingCalls) {
    Program p;
    const auto object = p.object(1);
    p.churn(0, 0);
    p.store(6, p.read(object, 0, 0, 2), 0, 2);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 6), 7U);
    EXPECT_LT(ctx.heap().objectStats().live, 400U);
}

TEST(ClassVmExecution, OrdinaryAndScopedFramesRetainScalarAndArrayHandlesAcrossSuspension) {
    for (const bool scoped : {false, true})
        for (const bool array : {false, true}) {
            SCOPED_TRACE(::testing::Message() << scoped << '/' << array);
            Program p;
            const auto type = s::classType(s::ClassId{1});
            p.d.processes[0].frame = {
                {"saved", type, s::kNone,
                 array ? std::vector<s::ArrayDim>{{0, 2}} : std::vector<s::ArrayDim>{}}};
            if (scoped) {
                auto& process = p.d.processes[0];
                process.frame[0].home = s::FrameHome::Scoped;
                process.frame[0].activationScope = s::ActivationScopeId{0};
                process.activation.root = s::ActivationScopeId{0};
                process.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
            }
            const auto object = p.object(1);
            const auto frame = O::frame(s::FrameVarId{0});
            const auto index = p.number(s::s2(32), 1);
            p.emit(array ? Op::StoreElem : Op::Fstore, s::voidType(),
                   array ? std::vector<O>{frame, index, object} : std::vector<O>{frame, object});
            p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})});
            p.churn(0, 1);
            const auto read =
                p.emit(array ? Op::LoadElem : Op::Fload, type,
                       array ? std::vector<O>{frame, index} : std::vector<O>{frame}, 0, 3);
            p.store(6, p.read(read, 0, 0, 3), 0, 3);
            ASSERT_EQ(p.errors(), "");
            k::Context ctx;
            const auto result = kirimasim::vm::run(p.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_EQ(p.slot(ctx, 6), 7U);
            EXPECT_EQ(ctx.activations().stats().live, 0U);
        }
}

TEST(ClassVmExecution, DetachedForkCapturesRetainObjectsAfterTheDeclaringProcessReturns) {
    Program p;
    auto& parent = p.d.processes[0];
    parent.frame = {{"saved", s::classType(s::ClassId{1})}};
    parent.frame[0].home = s::FrameHome::Scoped;
    parent.frame[0].activationScope = s::ActivationScopeId{0};
    parent.activation.root = s::ActivationScopeId{0};
    parent.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
    const auto object = p.object(1);
    p.emit(Op::Fstore, s::voidType(), {O::frame(s::FrameVarId{0}), object});
    const auto child = p.process();
    auto& process = p.d.processes[child];
    process.kind = s::ProcessKind::General;
    process.parent = s::Owner::of(s::ProcessId{0});
    process.activation.extents = {s::ActivationScopeId{0}};
    process.frame = p.d.processes[0].frame;
    process.frame[0].home = s::FrameHome::Capture;
    process.frame[0].activationScope = s::kNone;
    process.frame[0].captureSource = s::FrameVarId{0};
    p.emit(Op::Fork, s::voidType(),
           {O::process(s::ProcessId{child}), O::str("join_none"), O::block(s::BlockId{1})});
    p.emit(Op::Ret, s::voidType(), {}, 0, 1);
    p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})}, child);
    p.churn(child, 1);
    const auto read =
        p.emit(Op::Fload, s::classType(s::ClassId{1}), {O::frame(s::FrameVarId{0})}, child, 3);
    p.store(6, p.read(read, 0, child, 3), child, 3);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 6), 7U);
    EXPECT_EQ(ctx.activations().stats().live, 0U);
    EXPECT_EQ(ctx.activations().stats().leases, 0U);
}

TEST(ClassVmExecution, DelayedNbaIsAnIndependentStrongRootAfterItsProducerReturns) {
    Program p;
    const auto object = p.object(1);
    p.emit(Op::NbaDelay, s::voidType(), {O::slot(s::SlotId{0}), object, p.number(s::u2(64), 4)});
    const auto consumer = p.process();
    p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})},
           consumer);
    p.churn(consumer, 1);
    p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 4), O::block(s::BlockId{4})},
           consumer, 3);
    const auto read =
        p.emit(Op::Load, s::classType(s::ClassId{1}), {O::slot(s::SlotId{0})}, consumer, 4);
    p.store(6, p.read(read, 0, consumer, 4), consumer, 4);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 6), 7U);
}

TEST(ClassVmExecution, ClassReturnsOutputsRefsAndContainerPopsUseTheOrdinaryCallAndStoragePaths) {
    Program p;
    s::Subroutine function;
    function.name = "top.make";
    function.automatic = true;
    function.returnType = s::classType(s::ClassId{1});
    function.loc = {0, 1, 1};
    function.numTemps = 1;
    function.params = {
        {"out", s::Direction::Output, s::classType(s::ClassId{1}), s::kNone, s::FrameVarId{0}}};
    function.frame = {{"out", s::classType(s::ClassId{1})}};
    function.blocks.resize(1);
    function.blocks[0].instrs = {
        {Op::ObjectNew, s::classType(s::ClassId{1}), s::TempId{0}, {}, {0, 1, 1}},
        effect(Op::Fstore, {O::frame(s::FrameVarId{0}), O::temp(s::TempId{0})}),
        effect(Op::Ret, {O::temp(s::TempId{0})})};
    const auto id = p.add(std::move(function));
    const auto result =
        p.emit(Op::Call, s::classType(s::ClassId{1}), {O::subroutine(id), O::slot(s::SlotId{0})});
    p.store(1, result);
    const auto ref = p.emit(Op::RefMake, s::refType(s::TypeId{0}), {O::slot(s::SlotId{4})});
    p.emit(Op::QueuePushBack, s::voidType(), {O::typeRow(s::TypeId{0}), ref, result});
    const auto popped =
        p.emit(Op::QueuePopFront, s::classType(s::ClassId{1}), {O::typeRow(s::TypeId{0}), ref});
    const auto identity = p.emit(Op::ObjectId, s::u2(64), {popped});
    p.store(2, identity);
    const auto target =
        p.emit(Op::RefMake, s::classRefType(s::ClassId{1}), {O::slot(s::SlotId{0})});
    p.emit(Op::StoreRef, s::voidType(), {target, result});
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    auto run = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(run.reason, k::EndReason::Quiescent) << run.error;
    EXPECT_EQ(p.slot(ctx, 0), p.slot(ctx, 1));
    const auto info = ctx.heap().objectInfo({p.slot(ctx, 0), &ctx.heap()});
    ASSERT_TRUE(info);
    EXPECT_EQ(p.slot(ctx, 2), info.value.identity);
}

TEST(ClassVmExecution, BaseConstructionPublishesTheSameReceiverBeforeDerivedInitializers) {
    Program p;
    auto& getter = p.d.subroutines[p.d.classes[0].virtualMethods[0].index()];
    getter.numTemps = 3;
    getter.blocks[0].instrs = {
        {Op::Fload,
         s::classType(s::ClassId{0}),
         s::TempId{0},
         {O::frame(s::FrameVarId{0})},
         {0, 1, 1}},
        {Op::ObjectRead,
         s::u2(32),
         s::TempId{1},
         {O::classField({s::ClassId{0}, 0}), O::temp(s::TempId{0})},
         {0, 1, 1}},
        {Op::Zext, s::u2(64), s::TempId{2}, {O::temp(s::TempId{1})}, {0, 1, 1}},
        effect(Op::Ret, {O::temp(s::TempId{2})})};
    auto& base = p.d.subroutines[p.d.classes[1].constructor.index()];
    base.numTemps = 2;
    auto& code = base.blocks[0].instrs;
    const std::vector<s::Instr> escape = {
        {Op::Fload,
         s::classType(s::ClassId{1}),
         s::TempId{0},
         {O::frame(s::FrameVarId{0})},
         {0, 1, 1}},
        effect(Op::Store, {O::slot(s::SlotId{0}), O::temp(s::TempId{0})}),
        {Op::ObjectCall,
         s::u2(64),
         s::TempId{1},
         {O::virtualSlot(s::VirtualSlotId{0}), O::temp(s::TempId{0})},
         {0, 1, 1}},
        effect(Op::Store, {O::slot(s::SlotId{2}), O::temp(s::TempId{1})})};
    code.insert(code.end() - 1, escape.begin(), escape.end());
    const auto derived = p.object();
    const auto baseView = p.emit(Op::ObjectView, s::classType(s::ClassId{1}), {derived});
    p.store(1, baseView);
    p.store(3, p.emit(Op::ObjectCall, s::u2(64), {O::virtualSlot(s::VirtualSlotId{0}), baseView}));
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 0), p.slot(ctx, 1));
    EXPECT_EQ(p.slot(ctx, 2), 0U);
    EXPECT_EQ(p.slot(ctx, 3), 42U);
}

TEST(ClassVmExecution, SeedingAndCopyPreserveObjectStateWithoutRunningConstructorsAgain) {
    Program p;
    auto& ctor = p.d.subroutines[p.d.classes[1].constructor.index()];
    ctor.numTemps = 2;
    auto& code = ctor.blocks[0].instrs;
    const std::vector<s::Instr> sideEffect = {
        {Op::Load, s::u2(64), s::TempId{0}, {O::slot(s::SlotId{3})}, {0, 1, 1}},
        {Op::Add,
         s::u2(64),
         s::TempId{1},
         {O::temp(s::TempId{0}), p.number(s::u2(64), 1)},
         {0, 1, 1}},
        effect(Op::Store, {O::slot(s::SlotId{3}), O::temp(s::TempId{1})})};
    code.insert(code.end() - 1, sideEffect.begin(), sideEffect.end());
    const auto object = p.object(1);
    p.emit(Op::ObjectSeed, s::voidType(), {object, p.number(s::u2(32), 123)});
    p.store(0, object);
    p.store(1, p.emit(Op::ObjectCopy, s::classType(s::ClassId{1}), {object}));
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 3), 1U);
    const auto original = ctx.heap().objectInfo({p.slot(ctx, 0), &ctx.heap()});
    const auto copy = ctx.heap().objectInfo({p.slot(ctx, 1), &ctx.heap()});
    ASSERT_TRUE(original);
    ASSERT_TRUE(copy);
    EXPECT_EQ(original.value.randomState, 123U);
    EXPECT_EQ(copy.value.randomState, original.value.randomState);
    EXPECT_NE(original.value.identity, copy.value.identity);
}

TEST(ClassVmExecution, WeakHandlesDoNotRetainDeadReferentsAfterTheirProducerReturns) {
    Program p;
    p.d.slots.push_back({"top.weak", s::classType(s::ClassId{3})});
    p.d.processes[0].writes.push_back({s::SlotId{8}});
    const auto referent = p.object(1);
    p.store(8, p.emit(Op::ObjectWeakNew, s::classType(s::ClassId{3}), {referent}));
    const auto consumer = p.process();
    p.emit(Op::WaitDelay, s::voidType(), {p.number(s::u2(64), 1), O::block(s::BlockId{1})},
           consumer);
    p.churn(consumer, 1);
    const auto weak =
        p.emit(Op::Load, s::classType(s::ClassId{3}), {O::slot(s::SlotId{8})}, consumer, 3);
    const auto locked =
        p.emit(Op::ObjectWeakLock, s::classType(s::ClassId{1}), {weak}, consumer, 3);
    p.store(2, p.emit(Op::ObjectId, s::u2(64), {locked}, consumer, 3), consumer, 3);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    const auto result = kirimasim::vm::run(p.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(p.slot(ctx, 2), 0U);
    EXPECT_NE(p.slot(ctx, 8), 0U);
}

TEST(ClassVmExecution, CastsDistinguishDynamicClassTypedNullAndLiteralNull) {
    for (unsigned variant = 0; variant < 4; ++variant) {
        SCOPED_TRACE(variant);
        Program p;
        O source;
        if (variant < 2) {
            const auto object = p.object(variant == 0 ? 0 : 1);
            source = variant == 0 ? p.emit(Op::ObjectView, s::classType(s::ClassId{1}), {object})
                                  : object;
        } else
            source = p.number(variant == 2 ? s::classType(s::ClassId{1})
                                           : s::Type{s::TypeKind::Chandle, 0});
        p.store(7, p.emit(Op::ObjectCastCheck, s::u2(1), {O::classRow(s::ClassId{0}), source}));
        const auto cast = p.emit(Op::ObjectCast, s::classType(s::ClassId{0}), {source});
        p.store(2, p.emit(Op::ObjectId, s::u2(64), {cast}));
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        const auto result = kirimasim::vm::run(p.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(p.slot(ctx, 7), variant == 0 || variant == 3 ? 1U : 0U);
        EXPECT_EQ(p.slot(ctx, 2) != 0, variant == 0);
    }
}

TEST(ClassVmExecution, InterruptedConstructionReleasesStagesAndPinsAtEveryInstructionBoundary) {
    for (unsigned steps = 1; steps < 7; ++steps) {
        SCOPED_TRACE(steps);
        Program p;
        p.object(1);
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        kirimasim::vm::RunOptions options;
        options.maxSteps = steps;
        const auto result = kirimasim::vm::run(p.d, ctx, options);
        EXPECT_EQ(result.reason, k::EndReason::ZeroTimeLimit) << result.error;
        EXPECT_EQ(ctx.activations().stats().live, 0U);
        ASSERT_TRUE(ctx.heap().collect({}));
        EXPECT_EQ(ctx.heap().objectStats().live, 0U);
    }
}

TEST(ClassVmExecution, KernelIdentitySemanticsMatchTheReferenceAndRejectForeignAndStaleTokens) {
    namespace v = kirimasim::valsem;
    Program p;
    p.store(0, p.object(1));
    p.store(1, p.object(1));
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    ASSERT_EQ(kirimasim::vm::run(p.d, ctx).reason, k::EndReason::Quiescent);
    const std::array<k::ObjectToken, 3> tokens = {
        {{}, {p.slot(ctx, 0), &ctx.heap()}, {p.slot(ctx, 1), &ctx.heap()}}};
    const std::array<W, 4> conditions = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    const std::array<v::Logic, 4> logic = {v::Logic::Zero, v::Logic::One, v::Logic::X, v::Logic::Z};
    for (unsigned i = 0; i < tokens.size(); ++i) {
        ASSERT_TRUE(ctx.heap().objectTruth(tokens[i]));
        EXPECT_EQ(ctx.heap().objectTruth(tokens[i]).value, v::truthObjectHandle(i));
        for (unsigned j = 0; j < tokens.size(); ++j) {
            const auto equal = ctx.heap().equalObjects(tokens[i], tokens[j]);
            ASSERT_TRUE(equal);
            EXPECT_EQ(equal.value, v::equalObjectHandles(i, j));
            for (unsigned c = 0; c < conditions.size(); ++c) {
                const auto selected = ctx.heap().selectObject(conditions[c], tokens[i], tokens[j]);
                ASSERT_TRUE(selected);
                EXPECT_EQ(selected.value, tokens[v::selectObjectHandle(logic[c], i, j)]);
            }
        }
    }
    k::Context foreign;
    EXPECT_EQ(foreign.heap().equalObjects(tokens[1], {}).error, k::ObjectError::InvalidToken);
    EXPECT_EQ(ctx.heap().selectObject({0, 0}, {0, &ctx.heap()}, {}).error,
              k::ObjectError::InvalidToken);
    ASSERT_TRUE(ctx.heap().collect({}));
    EXPECT_EQ(ctx.heap().objectTruth(tokens[1]).error, k::ObjectError::InvalidToken);
}

TEST(ClassVmExecution, DeferredArgumentsRetainClassObjectsAfterTheirProducerReturns) {
    for (const auto* mode : {"#0", "final"}) {
        SCOPED_TRACE(mode);
        Program p;
        s::Subroutine callback;
        callback.name = "top.report";
        callback.automatic = true;
        callback.loc = {0, 1, 1};
        callback.params = {{"object", s::Direction::Input, s::classType(s::ClassId{1}), s::kNone,
                            s::FrameVarId{0}}};
        callback.frame = {{"object", s::classType(s::ClassId{1})}};
        callback.numTemps = 2;
        callback.blocks.resize(1);
        callback.blocks[0].instrs = {
            {Op::Fload,
             s::classType(s::ClassId{1}),
             s::TempId{0},
             {O::frame(s::FrameVarId{0})},
             {0, 1, 1}},
            {Op::ObjectRead,
             p.scalar,
             s::TempId{1},
             {O::classField({s::ClassId{1}, 0}), O::temp(s::TempId{0})},
             {0, 1, 1}},
            effect(Op::Syscall, {O::str("$display"), O::str("%0d"), O::temp(s::TempId{1})}),
            effect(Op::Ret)};
        const auto function = p.add(std::move(callback));
        p.emit(Op::ReportCall, s::voidType(), {O::str(mode), O::subroutine(function), p.object(1)});
        const auto consumer = p.process();
        p.churn(consumer, 0);
        ASSERT_EQ(p.errors(), "");
        std::unique_ptr<std::FILE, decltype(&std::fclose)> output{std::tmpfile(), &std::fclose};
        ASSERT_NE(output, nullptr);
        k::Context ctx({output.get(), output.get()});
        const auto result = kirimasim::vm::run(p.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        ASSERT_EQ(std::fseek(output.get(), 0, SEEK_SET), 0);
        std::array<char, 16> text{};
        const auto count = std::fread(text.data(), 1, text.size(), output.get());
        EXPECT_EQ(std::string(text.data(), count), "7\n");
        ASSERT_TRUE(ctx.heap().collect({}));
        EXPECT_EQ(ctx.heap().objectStats().live, 0U);
    }
}

TEST(ClassVmExecution, ClassSchemaResourceFailurePublishesNoRuntimeValues) {
    Program p;
    p.object(1);
    ASSERT_EQ(p.errors(), "");
    k::Context ctx({stdout, stderr}, nullptr, std::pmr::null_memory_resource());
    const auto result = kirimasim::vm::run(p.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().objectStats().live, 0U);
    EXPECT_EQ(ctx.heap().stats().cells, 0U);
    EXPECT_EQ(ctx.activations().stats().live, 0U);
}

TEST(ClassVmExecution, UnconnectedSourceInitializationOverridesFailBeforeRuntimeValues) {
    for (const auto mode : {kirimasim::vm::InitValuesMode::Zero, kirimasim::vm::InitValuesMode::One,
                            kirimasim::vm::InitValuesMode::Random}) {
        Program p;
        p.object(1);
        ASSERT_EQ(p.errors(), "");
        k::Context ctx;
        kirimasim::vm::RunOptions options;
        options.initValues = mode;
        const auto result = kirimasim::vm::run(p.d, ctx, options);
        EXPECT_EQ(result.reason, k::EndReason::Internal);
        EXPECT_NE(result.error.find("initialization overrides"), std::string::npos);
        EXPECT_EQ(ctx.store().size(), 0U);
        EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
        EXPECT_EQ(ctx.activations().stats().live, 0U);
    }
}

TEST(ClassVmExecution, UnsupportedObservationCombinationsStillFailBeforeAnyRuntimeValueIsCreated) {
    Program p;
    s::Subroutine guard;
    guard.name = "top.guard";
    guard.kind = s::SubroutineKind::Guard;
    guard.frameOf = s::Owner::of(s::ProcessId{0});
    guard.returnType = s::u2(1);
    guard.loc = {0, 1, 1};
    guard.blocks.resize(1);
    guard.blocks[0].instrs = {effect(Op::Ret, {p.number(s::u2(1), 1)})};
    const auto id = p.add(std::move(guard));
    p.emit(Op::WaitCond, s::voidType(), {O::subroutine(id), O::block(s::BlockId{1})});
    p.d.processes[0].blocks.resize(2);
    p.d.processes[0].blocks[1].instrs = {effect(Op::Ret)};
    ASSERT_EQ(p.errors(), "");
    k::Context ctx;
    auto result = kirimasim::vm::run(p.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_NE(result.error.find("observation"), std::string::npos);
    EXPECT_EQ(ctx.store().size(), 0U);
    EXPECT_EQ(ctx.heap().objectStats().entries, 0U);
}
}  // namespace
