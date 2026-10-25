// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original execution matrices for SIR_SPEC 7/8, RUNTIME_ABI 4/6 and the
// approved automatic-capture contract (IEEE 1800-2023 6.21/9.3.2/13.5.2).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/w4ops.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
using s::Op;
using s::Operand;
constexpr s::Owner mainBody{s::Owner::Kind::Process, 0};
Operand frame(std::uint32_t n) { return Operand::frame(s::FrameVarId{n}); }

struct Model {
    s::Design d;
    Model() {
        d.sources = {"vm_activation.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::Process p;
        p.name = "top.main";
        p.loc = {0, 1, 1};
        d.processes.push_back(std::move(p));
    }
    auto& fields(s::Owner body) {
        return body.kind == s::Owner::Kind::Process ? d.processes[body.id].frame
                                                    : d.subroutines[body.id].frame;
    }
    auto& activation(s::Owner body) {
        return body.kind == s::Owner::Kind::Process ? d.processes[body.id].activation
                                                    : d.subroutines[body.id].activation;
    }
    void root(s::Owner body, std::vector<s::FrameVar> rows) {
        fields(body) = std::move(rows);
        auto& info = activation(body);
        info.root = s::ActivationScopeId{0};
        info.scopes.resize(1);
        for (std::uint32_t i = 0; i < fields(body).size(); ++i) {
            auto& row = fields(body)[i];
            if (row.type.kind == s::TypeKind::Ref) continue;
            row.home = s::FrameHome::Scoped;
            row.activationScope = s::ActivationScopeId{0};
            info.scopes[0].fields.push_back(s::FrameVarId{i});
        }
    }
    s::Owner task(bool wait = false) {
        const s::SubroutineId id{static_cast<std::uint32_t>(d.subroutines.size())};
        s::Subroutine sub;
        sub.name = "top.task" + std::to_string(id.index());
        sub.kind = s::SubroutineKind::Task;
        sub.automatic = true;
        sub.mayWait = wait;
        sub.loc = {0, 1, 1};
        d.subroutines.push_back(std::move(sub));
        return s::Owner::of(id);
    }
    s::Owner child(s::Owner parent, const std::vector<std::uint32_t>& rows,
                   std::vector<s::ActivationScopeId> extents = {s::ActivationScopeId{0}},
                   bool inherit = false) {
        s::Process p;
        p.name = "top.child" + std::to_string(d.processes.size());
        p.kind = s::ProcessKind::General;
        p.parent = parent;
        p.loc = {0, 1, 1};
        p.activation.extents = std::move(extents);
        p.activation.inheritCaptures = inherit;
        for (auto id : rows) {
            auto row = fields(parent)[id];
            row.home = s::FrameHome::Capture;
            row.activationScope = s::kNone;
            row.captureSource = s::FrameVarId{id};
            p.frame.push_back(std::move(row));
        }
        const auto id = s::ProcessId{static_cast<std::uint32_t>(d.processes.size())};
        d.processes.push_back(std::move(p));
        return s::Owner::of(id);
    }
    Operand slot(s::Type type, std::vector<s::ArrayDim> dims = {}) {
        s::Slot slot;
        slot.name = "top.answer" + std::to_string(d.slots.size());
        slot.type = type;
        slot.kind = s::SlotKind::Var;
        slot.flags = s::kObservable;
        slot.dims = std::move(dims);
        const s::SlotId id{static_cast<std::uint32_t>(d.slots.size())};
        d.slots.push_back(std::move(slot));
        return Operand::slot(id);
    }
    Operand constant(s::Type type, std::uint64_t a, std::uint64_t b = 0) {
        s::Constant c;
        c.type = type;
        const auto count = s::isIntegral(type) ? (type.width + 63) / 64 : 1;
        for (std::uint32_t word = 0; word < count; ++word) {
            const auto mask =
                s::isIntegral(type) ? k::mask(std::min(64U, type.width - word * 64)) : UINT64_MAX;
            c.aWords.push_back(a & mask);
            if (s::isFourState(type)) c.bWords.push_back(b & mask);
        }
        const auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back(std::move(c));
        return Operand::constant(id);
    }
    Operand emit(s::Owner body, unsigned block, Op op, std::vector<Operand> args = {},
                 s::Type type = s::voidType()) {
        auto& blocks = body.kind == s::Owner::Kind::Process ? d.processes[body.id].blocks
                                                            : d.subroutines[body.id].blocks;
        auto& temps = body.kind == s::Owner::Kind::Process ? d.processes[body.id].numTemps
                                                           : d.subroutines[body.id].numTemps;
        if (blocks.size() <= block) blocks.resize(block + 1);
        const auto id = s::TempId{type == s::voidType() ? s::kNone : temps++};
        blocks[block].instrs.push_back({op, type, id, std::move(args), {0, 1, 1}});
        return Operand::temp(id);
    }
    void store(s::Owner body, unsigned block, const Operand& target, const Operand& value) {
        emit(body, block, target.kind == Operand::Kind::Frame ? Op::Fstore : Op::Store,
             {target, value});
    }
    void fork(s::Owner body, unsigned block, const std::vector<s::Owner>& children,
              std::string join, unsigned next) {
        std::vector<Operand> args;
        args.reserve(children.size() + 2);
        for (auto child : children)
            args.push_back(Operand::process(child.asProcess()));
        args.push_back(Operand::str(std::move(join)));
        args.push_back(Operand::block(s::BlockId{next}));
        emit(body, block, Op::Fork, std::move(args));
    }
    ::testing::AssertionResult valid() {
        for (auto& process : d.processes) {
            process.writes.clear();
            for (std::uint32_t i = 0; i < d.slots.size(); ++i)
                process.writes.push_back({s::SlotId{i}});
        }
        const auto errors = s::verify(d);
        if (errors.empty()) return ::testing::AssertionSuccess();
        std::string message;
        for (const auto& error : errors)
            message += error.where + ": " + error.message + "\n";
        return ::testing::AssertionFailure() << message;
    }
};
std::uint64_t word(const k::Context& context, const Operand& slot, unsigned element = 0,
                   unsigned at = 0) {
    return context.store().element(k::SlotId{slot.id}, element)[at];
}
void released(const k::Context& context) {
    EXPECT_EQ(context.activations().stats().live, 0U);
    EXPECT_EQ(context.activations().stats().leases, 0U);
    EXPECT_EQ(context.activations().stats().words, 0U);
}

TEST(VmActivation, ScopedWideArraysUseTheSameSelectedTransportAndRefs) {
    unsigned cases = 0;
    for (const auto width : {7U, 32U, 64U, 65U, 129U, 257U})
        for (const bool four : {false, true})
            for (const bool ref : {false, true}) {
                SCOPED_TRACE(::testing::Message() << width << '/' << four << '/' << ref);
                Model m;
                const auto type = four ? s::u4(width) : s::u2(width);
                const auto answer = m.slot(type);
                m.root(mainBody, {{"prefix", s::u4(129)},
                                  {"array", type, s::kNone, {{-1, 1}}},
                                  {"alias", s::refType(), s::kNone, {{-1, 1}}, type}});
                const auto initial = m.constant(type, UINT64_MAX, four ? 0xaaaaaaaaaaaaaaaaULL : 0);
                const auto index = m.constant(s::s2(64), 0);
                const auto pos = m.constant(s::s2(64), width / 2);
                const auto bit = m.constant(four ? s::u4(1) : s::u2(1), 0, four ? 1 : 0);
                m.store(mainBody, 0, frame(0), m.constant(s::u4(129), 37, 19));
                m.emit(mainBody, 0, Op::StoreElem, {frame(1), index, initial});
                if (ref) {
                    const auto alias = m.emit(mainBody, 0, Op::RefMake, {frame(1)}, s::refType());
                    m.store(mainBody, 0, frame(2), alias);
                    m.emit(mainBody, 0, Op::StoreRefBits, {frame(2), index, pos, bit});
                } else
                    m.emit(mainBody, 0, Op::StoreBits, {frame(1), index, pos, bit});
                const auto value = m.emit(mainBody, 0, ref ? Op::LoadRefElem : Op::LoadElem,
                                          {frame(ref ? 2 : 1), index}, type);
                m.store(mainBody, 0, answer, value);
                m.emit(mainBody, 0, Op::Ret);
                ASSERT_TRUE(m.valid());
                k::Context context;
                const auto result = kirimasim::vm::run(m.d, context);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                for (unsigned chunk = 0; chunk < (width + 63) / 64; ++chunk) {
                    auto expected = k::mask(std::min(64U, width - chunk * 64));
                    auto unknown = (four ? 0xaaaaaaaaaaaaaaaaULL : 0) & expected;
                    if (chunk == width / 2 / 64) {
                        expected &= ~(std::uint64_t{1} << (width / 2 % 64));
                        if (four) unknown |= std::uint64_t{1} << (width / 2 % 64);
                    }
                    EXPECT_EQ(word(context, answer, 0, chunk * (four ? 2 : 1)), expected);
                    if (four) EXPECT_EQ(word(context, answer, 0, chunk * 2 + 1), unknown);
                }
                released(context);
                ++cases;
            }
    EXPECT_EQ(cases, 24U);
}

TEST(VmActivation, SiblingsAndParentShareOneValueAcrossAllJoins) {
    for (const std::string join : {"join", "join_any", "join_none"}) {
        Model m;
        const auto seen = m.slot(s::u2(32)), final = m.slot(s::u2(32));
        m.root(mainBody, {{"value", s::u2(32)}});
        const auto first = m.child(mainBody, {0}), second = m.child(mainBody, {0});
        const auto one = m.constant(s::u2(32), 1), seven = m.constant(s::u2(32), 7),
                   nine = m.constant(s::u2(32), 9);
        m.store(mainBody, 0, frame(0), seven);
        m.fork(mainBody, 0, {first, second}, join, 1);
        m.emit(mainBody, 1, Op::WaitFork, {Operand::block(s::BlockId{2})});
        const auto parent = m.emit(mainBody, 2, Op::Fload, {frame(0)}, s::u2(32));
        m.store(mainBody, 2, final, parent);
        m.emit(mainBody, 2, Op::Ret);
        m.emit(first, 0, Op::WaitDelay, {one, Operand::block(s::BlockId{1})});
        m.store(first, 1, frame(0), nine);
        m.emit(first, 1, Op::Ret);
        m.emit(second, 0, Op::WaitDelay, {m.constant(s::u2(32), 2), Operand::block(s::BlockId{1})});
        const auto child = m.emit(second, 1, Op::Fload, {frame(0)}, s::u2(32));
        m.store(second, 1, seen, child);
        m.emit(second, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(m.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, seen), 9U);
        EXPECT_EQ(word(context, final), 9U);
        released(context);
    }
}

TEST(VmActivation, ReturnedOwnersAndReenteredBlocksDoNotRedirectEarlierChildren) {
    Model m;
    const auto answer = m.slot(s::u2(32), {{0, 1}});
    m.root(mainBody, {{"value", s::u2(32)}, {"index", s::u2(32)}});
    auto& info = m.activation(mainBody);
    info.scopes[0].fields.clear();
    info.scopes.push_back({s::ActivationScopeId{0}, {s::FrameVarId{0}, s::FrameVarId{1}}});
    for (auto& row : m.fields(mainBody))
        row.activationScope = s::ActivationScopeId{1};
    const auto child =
        m.child(mainBody, {0, 1}, {s::ActivationScopeId{0}, s::ActivationScopeId{1}});
    for (unsigned entry = 0; entry < 2; ++entry) {
        m.emit(mainBody, entry, Op::ActivationEnter,
               {Operand::activation(s::ActivationScopeId{1})});
        m.store(mainBody, entry, frame(0), m.constant(s::u2(32), entry == 0 ? 7 : 29));
        m.store(mainBody, entry, frame(1), m.constant(s::u2(32), entry));
        m.fork(mainBody, entry, {child}, "join_none", entry + 1);
        m.emit(mainBody, entry + 1, Op::ActivationLeave,
               {Operand::activation(s::ActivationScopeId{1})});
    }
    m.emit(mainBody, 2, Op::Ret);
    const auto value = m.emit(child, 0, Op::Fload, {frame(0)}, s::u2(32));
    const auto index = m.emit(child, 0, Op::Fload, {frame(1)}, s::u2(32));
    m.emit(child, 0, Op::StoreElem, {answer, index, value});
    m.emit(child, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer, 0), 7U);
    EXPECT_EQ(word(context, answer, 1), 29U);
    EXPECT_LE(context.activations().stats().entries, 3U);
    released(context);
}

TEST(VmActivation, DescendantsKeepActualOriginsAfterBothEnclosingBodiesReturn) {
    Model m;
    const auto answer = m.slot(s::u4(129));
    m.root(mainBody, {{"value", s::u4(129)}});
    const auto child = m.child(mainBody, {0});
    const auto descendant = m.child(child, {0}, {}, true);
    m.store(mainBody, 0, frame(0),
            m.constant(s::u4(129), 0x123456789abcdef0ULL, 0x0101010101010101ULL));
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::Ret);
    m.fork(child, 0, {descendant}, "join_none", 1);
    m.emit(child, 1, Op::Ret);
    const auto ref = m.emit(descendant, 0, Op::RefMake, {frame(0)}, s::refType());
    const auto value = m.emit(descendant, 0, Op::LoadRef, {ref}, s::u4(129));
    m.store(descendant, 0, answer, value);
    m.emit(descendant, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer, 0, 0), 0x123456789abcdef0ULL);
    EXPECT_EQ(word(context, answer, 0, 1), 0x0101010101010101ULL);
    EXPECT_EQ(word(context, answer, 0, 2), 0x123456789abcdef0ULL);
    EXPECT_EQ(word(context, answer, 0, 3), 0x0101010101010101ULL);
    EXPECT_EQ(word(context, answer, 0, 4), 0U);
    EXPECT_EQ(word(context, answer, 0, 5), 1U);
    released(context);
}

TEST(VmActivation, OutputCopybackOccursOnceBeforeTaskOrSynchronousOwnerReturns) {
    for (const bool wait : {false, true}) {
        Model m;
        const auto callerAnswer = m.slot(s::u4(32)), childAnswer = m.slot(s::u4(32));
        m.root(mainBody, {{"actual", s::u4(32)}});
        const auto task = m.task(wait);
        m.root(task, {{"formal", s::u4(32)}});
        m.d.subroutines[task.id].params = {
            {"formal", s::Direction::Output, s::u4(32), s::kNone, s::FrameVarId{0}}};
        const auto child = m.child(task, {0});
        const auto one = m.constant(s::u2(32), 1), two = m.constant(s::u2(32), 2);
        m.store(mainBody, 0, frame(0), m.constant(s::u4(32), 3));
        if (wait)
            m.emit(mainBody, 0, Op::CallTask,
                   {Operand::subroutine(task.asSubroutine()), frame(0),
                    Operand::block(s::BlockId{1})});
        else {
            m.emit(mainBody, 0, Op::Call, {Operand::subroutine(task.asSubroutine()), frame(0)});
            m.emit(mainBody, 0, Op::Br, {Operand::block(s::BlockId{1})});
        }
        const auto copied = m.emit(mainBody, 1, Op::Fload, {frame(0)}, s::u4(32));
        m.store(mainBody, 1, callerAnswer, copied);
        m.emit(mainBody, 1, Op::Ret);
        m.store(task, 0, frame(0), m.constant(s::u4(32), 7));
        m.fork(task, 0, {child}, "join_none", 1);
        if (wait) m.emit(task, 1, Op::WaitDelay, {one, Operand::block(s::BlockId{2})});
        m.emit(task, wait ? 2 : 1, Op::Ret);
        m.emit(child, 0, Op::WaitDelay, {two, Operand::block(s::BlockId{1})});
        m.store(child, 1, frame(0), m.constant(s::u4(32), 9));
        const auto changed = m.emit(child, 1, Op::Fload, {frame(0)}, s::u4(32));
        m.store(child, 1, childAnswer, changed);
        m.emit(child, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(m.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, callerAnswer), 7U);
        EXPECT_EQ(word(context, callerAnswer, 0, 1), 0U);
        EXPECT_EQ(word(context, childAnswer), 9U);
        released(context);
    }
}

TEST(VmActivation, OutputFormalDefaultsAreInitializedInRootBeforeCopyback) {
    for (const auto width : {1U, 32U, 65U, 129U}) {
        Model m;
        const auto type = s::u4(width);
        const auto task = m.task();
        const auto answer = m.slot(type);
        m.root(mainBody, {{"actual", type}});
        m.root(task, {{"formal", type}});
        m.d.subroutines[task.id].params = {
            {"formal", s::Direction::Output, type, s::kNone, s::FrameVarId{0}}};
        m.emit(task, 0, Op::Ret);
        m.emit(mainBody, 0, Op::Call, {Operand::subroutine(task.asSubroutine()), frame(0)});
        const auto value = m.emit(mainBody, 0, Op::Fload, {frame(0)}, type);
        m.store(mainBody, 0, answer, value);
        m.emit(mainBody, 0, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(m.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        for (unsigned at = 0; at < (width + 63) / 64; ++at) {
            const auto mask = k::mask(std::min(64U, width - at * 64));
            EXPECT_EQ(word(context, answer, 0, 2 * at), mask);
            EXPECT_EQ(word(context, answer, 0, 2 * at + 1), mask);
        }
        released(context);
    }
}

TEST(VmActivation, GuardOfSharedScopeWakesFromCapturedChildWrite) {
    for (const bool taskWaiter : {false, true}) {
        Model m;
        const auto answer = m.slot(s::u2(32));
        m.root(mainBody, {{"outer", s::u2(32)}});
        const auto waiter = taskWaiter ? m.task(true) : mainBody;
        if (taskWaiter) m.root(waiter, {{"inner", s::u2(32)}});
        const auto writer = m.child(waiter, {0});
        s::Subroutine guard;
        guard.name = "top.guard";
        guard.kind = s::SubroutineKind::Guard;
        guard.frameOf = waiter;
        guard.returnType = s::u2(1);
        guard.loc = {0, 1, 1};
        const auto guardId = s::SubroutineId{static_cast<std::uint32_t>(m.d.subroutines.size())};
        m.d.subroutines.push_back(std::move(guard));
        const auto body = s::Owner::of(guardId);
        const auto one = m.constant(s::u2(32), 1), zero = m.constant(s::u2(32), 0);
        const auto read = m.emit(body, 0, Op::Fload, {frame(0)}, s::u2(32));
        const auto truth = m.emit(body, 0, Op::Truth, {read}, s::u2(1));
        m.emit(body, 0, Op::Ret, {truth});
        if (taskWaiter) {
            m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 99));
            m.emit(mainBody, 0, Op::CallTask,
                   {Operand::subroutine(waiter.asSubroutine()), Operand::block(s::BlockId{1})});
            m.emit(mainBody, 1, Op::Ret);
        }
        m.store(waiter, 0, frame(0), zero);
        m.fork(waiter, 0, {writer}, "join_none", 1);
        m.emit(waiter, 1, Op::WaitCond,
               {Operand::subroutine(guardId), Operand::block(s::BlockId{2})});
        const auto awake = m.emit(waiter, 2, Op::Fload, {frame(0)}, s::u2(32));
        m.store(waiter, 2, answer, awake);
        m.emit(waiter, 2, Op::Ret);
        m.emit(writer, 0, Op::WaitDelay, {one, Operand::block(s::BlockId{1})});
        m.store(writer, 1, frame(0), one);
        m.emit(writer, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(m.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, answer), 1U);
        EXPECT_EQ(context.scheduler().time(), 1U);
        released(context);
    }
}

struct OwnerObserver {
    k::ProcessId process{0};
    bool waiting = false, seen = false;
    k::ActivationStats stats;
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<OwnerObserver*>(frame);
        auto& context = k::fromC(raw);
        if (!self.waiting) {
            self.waiting = true;
            context.scheduler().waitDelay(self.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        self.seen = true;
        self.stats = context.activations().stats();
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(VmActivation, ExecutingOwnershipEndsBeforeTheRetainedControlRecordIsReclaimed) {
    Model m;
    const auto answer = m.slot(s::u2(32));
    m.root(mainBody, {{"shared", s::u2(32)}, {"unread", s::Type{s::TypeKind::String}}});
    const auto child = m.child(mainBody, {0});
    m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 19));
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::Ret);
    m.emit(child, 0, Op::WaitDelay, {m.constant(s::u2(32), 2), Operand::block(s::BlockId{1})});
    const auto value = m.emit(child, 1, Op::Fload, {frame(0)}, s::u2(32));
    m.store(child, 1, answer, value);
    m.emit(child, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    OwnerObserver observer;
    observer.process =
        context.scheduler().addProcess(&OwnerObserver::resume, &observer, k::Group::Active);
    context.scheduler().start(observer.process);
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    ASSERT_TRUE(observer.seen);
    EXPECT_EQ(observer.stats.live, 1U);
    EXPECT_EQ(observer.stats.leases,
              1U);  // Only the child, although the parent's control record remains.
    EXPECT_EQ(observer.stats.words, 2U);  // The unread field also belongs to the extended scope.
    EXPECT_EQ(word(context, answer), 19U);
    released(context);
}

TEST(VmActivation, QualifiedReferenceCapturesKeepBindingsAcrossFormalReuse) {
    for (const bool readonly : {false, true}) {
        Model m;
        const auto first = m.slot(s::u2(32)), second = m.slot(s::u2(32)),
                   answer = m.slot(s::u2(32), {{0, 1}});
        const auto task = m.task();
        m.root(task, {{"alias", s::refType(), s::kNone, {}, s::u2(32)}, {"index", s::u2(32)}});
        m.fields(task)[0].refStatic = true;
        m.fields(task)[0].readonly = readonly;
        m.d.subroutines[task.id].params = {
            {"alias", readonly ? s::Direction::ConstRef : s::Direction::Ref, s::u2(32), s::kNone,
             s::FrameVarId{0}, true},
            {"index", s::Direction::Input, s::u2(32), s::kNone, s::FrameVarId{1}}};
        const auto child = m.child(task, {0, 1});
        m.store(mainBody, 0, first, m.constant(s::u2(32), 7));
        m.store(mainBody, 0, second, m.constant(s::u2(32), 29));
        for (unsigned i = 0; i < 2; ++i) {
            const auto ref =
                m.emit(mainBody, 0, Op::RefMake, {i == 0 ? first : second}, s::refType());
            m.emit(mainBody, 0, Op::Call,
                   {Operand::subroutine(task.asSubroutine()), ref, m.constant(s::u2(32), i)});
        }
        m.emit(mainBody, 0, Op::Ret);
        m.fork(task, 0, {child}, "join_none", 1);
        m.emit(task, 1, Op::Ret);
        const auto read = m.emit(child, 0, Op::LoadRef, {frame(0)}, s::u2(32));
        const auto index = m.emit(child, 0, Op::Fload, {frame(1)}, s::u2(32));
        m.emit(child, 0, Op::StoreElem, {answer, index, read});
        m.emit(child, 0, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(m.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, answer, 0), 7U);
        EXPECT_EQ(word(context, answer, 1), 29U);
        released(context);
    }
}

TEST(VmActivation, RuntimeRightsFollowReadonlyAliasesThroughReferenceResults) {
    Model m;
    const auto actual = m.slot(s::u2(32));
    const auto task = m.task();
    m.root(task, {{"alias", s::refType(), s::kNone, {}, s::u2(32)}});
    m.fields(task)[0].refStatic = true;
    m.fields(task)[0].readonly = true;
    m.d.subroutines[task.id].params = {
        {"alias", s::Direction::ConstRef, s::u2(32), s::kNone, s::FrameVarId{0}, true}};
    const auto child = m.child(task, {0});
    m.store(mainBody, 0, actual, m.constant(s::u2(32), 7));
    const auto ref = m.emit(mainBody, 0, Op::RefMake, {actual}, s::refType());
    m.emit(mainBody, 0, Op::Call, {Operand::subroutine(task.asSubroutine()), ref});
    m.emit(mainBody, 0, Op::Ret);
    m.fork(task, 0, {child}, "join_none", 1);
    m.emit(task, 1, Op::Ret);
    const auto forwarded = m.emit(child, 0, Op::RefMake, {frame(0)}, s::refType());
    // The current structural verifier does not prove complete SSA const
    // provenance. Runtime transport must still never recover write rights.
    m.emit(child, 0, Op::StoreRef, {forwarded, m.constant(s::u2(32), 99)});
    m.emit(child, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    EXPECT_EQ(word(context, actual), 7U);
    released(context);
}

TEST(VmActivation, StaticSensitivityKeepsReadsFromUnexecutedGuardBranches) {
    Model m;
    const auto stamp = m.slot(s::u2(64));
    m.root(mainBody, {{"selector", s::u2(1)}, {"branch", s::u2(32)}});
    const auto child = m.child(mainBody, {1});
    s::Subroutine sub;
    sub.name = "top.guard";
    sub.kind = s::SubroutineKind::Guard;
    sub.frameOf = mainBody;
    sub.returnType = s::u2(1);
    sub.loc = {0, 1, 1};
    const s::SubroutineId guard{0};
    m.d.subroutines.push_back(std::move(sub));
    const auto body = s::Owner::of(guard);
    const auto now =
        m.emit(body, 0, Op::Syscall, {Operand::str("$time"), m.constant(s::u2(64), 1)}, s::u2(64));
    m.store(body, 0, stamp, now);
    const auto select = m.emit(body, 0, Op::Fload, {frame(0)}, s::u2(1));
    m.emit(body, 0, Op::BrIf,
           {select, Operand::block(s::BlockId{1}), Operand::block(s::BlockId{2})});
    const auto read = m.emit(body, 1, Op::Fload, {frame(1)}, s::u2(32));
    const auto truth = m.emit(body, 1, Op::Truth, {read}, s::u2(1));
    m.emit(body, 1, Op::Ret, {truth});
    m.emit(body, 2, Op::Ret, {m.constant(s::u2(1), 0)});
    m.store(mainBody, 0, frame(0), m.constant(s::u2(1), 0));
    m.store(mainBody, 0, frame(1), m.constant(s::u2(32), 0));
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::WaitCond, {Operand::subroutine(guard), Operand::block(s::BlockId{2})});
    m.emit(mainBody, 2, Op::Ret);
    const auto one = m.constant(s::u2(32), 1);
    m.emit(child, 0, Op::WaitDelay, {one, Operand::block(s::BlockId{1})});
    m.store(child, 1, frame(0), one);
    m.emit(child, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, stamp), 1U);
    released(context);
}

class AllocationBudget final : public std::pmr::memory_resource {
public:
    std::size_t remaining = 0, live = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        --remaining;
        auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++live;
        return result;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        --live;
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(VmActivation, KernelAllocationFailuresReturnResourceLimitAndReleaseWholeForkGroups) {
    Model m;
    const auto answer = m.slot(s::u2(32));
    m.root(mainBody, {{"value", s::u2(32)}});
    const auto child = m.child(mainBody, {0});
    auto& branch = m.d.processes[child.id];
    branch.frame.push_back({"local", s::u4(129)});
    branch.frame[1].home = s::FrameHome::Scoped;
    branch.frame[1].activationScope = s::ActivationScopeId{0};
    branch.activation.root = s::ActivationScopeId{0};
    branch.activation.scopes = {{s::kNone, {s::FrameVarId{1}}}};
    m.fork(mainBody, 0, {child, child, child}, "join", 1);
    m.emit(mainBody, 1, Op::Ret);
    m.store(child, 0, answer, m.constant(s::u2(32), 31));
    m.emit(child, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    unsigned failed = 0, passed = 0;
    for (std::size_t budget = 0; budget < 80; ++budget) {
        SCOPED_TRACE(budget);
        AllocationBudget memory;
        memory.remaining = budget;
        {
            k::Context context({nullptr, nullptr}, nullptr, &memory);
            const auto result = kirimasim::vm::run(m.d, context);
            if (result.reason == k::EndReason::Quiescent) {
                ++passed;
                EXPECT_EQ(word(context, answer), 31U);
            } else {
                ++failed;
                EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
                if (context.store().size() != 0) EXPECT_EQ(word(context, answer), 0U);
            }
            released(context);
        }
        EXPECT_EQ(memory.live, 0U);
    }
    EXPECT_GT(failed, 5U);
    EXPECT_GT(passed, 0U);
}

TEST(VmActivation, NamedDisableUnwindsLocalOwnersAndCancelsQueuedChildren) {
    Model m;
    m.d.numScopes = 92;
    const auto answer = m.slot(s::u2(32)), forbidden = m.slot(s::u2(32));
    m.root(mainBody, {{"outer", s::u2(32)}, {"inner", s::u2(32)}});
    auto& info = m.activation(mainBody);
    info.scopes[0].fields = {s::FrameVarId{0}};
    info.scopes.push_back({s::ActivationScopeId{0}, {s::FrameVarId{1}}});
    m.fields(mainBody)[1].activationScope = s::ActivationScopeId{1};
    const auto child = m.child(mainBody, {1}, {s::ActivationScopeId{0}, s::ActivationScopeId{1}});
    m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 7));
    m.emit(mainBody, 0, Op::ScopeEnter, {Operand::scope(91), Operand::block(s::BlockId{2})});
    m.emit(mainBody, 0, Op::ActivationEnter, {Operand::activation(s::ActivationScopeId{1})});
    m.store(mainBody, 0, frame(1), m.constant(s::u2(32), 19));
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::DisableScope, {Operand::scope(91)});
    m.emit(mainBody, 1, Op::Br, {Operand::block(s::BlockId{2})});
    const auto read = m.emit(mainBody, 2, Op::Fload, {frame(0)}, s::u2(32));
    m.store(mainBody, 2, answer, read);
    m.emit(mainBody, 2, Op::Ret);
    const auto inner = m.emit(child, 0, Op::Fload, {frame(0)}, s::u2(32));
    m.store(child, 0, forbidden, inner);
    m.emit(child, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer), 7U);
    EXPECT_EQ(word(context, forbidden), 0U);
    released(context);
}

TEST(VmActivation, SharedActualReferencesSurviveSynchronousAndWaitingCalls) {
    for (const bool waiting : {false, true})
        for (const auto width : {7U, 65U, 129U}) {
            Model m;
            const auto type = s::u4(width);
            const auto answer = m.slot(type);
            m.root(mainBody, {{"actual", type}});
            const auto task = m.task(waiting);
            m.root(task, {{"alias", s::refType(), s::kNone, {}, type}});
            m.d.subroutines[task.id].params = {
                {"alias", s::Direction::Ref, type, s::kNone, s::FrameVarId{0}}};
            const auto actual = m.emit(mainBody, 0, Op::RefMake, {frame(0)}, s::refType());
            if (waiting)
                m.emit(mainBody, 0, Op::CallTask,
                       {Operand::subroutine(task.asSubroutine()), actual,
                        Operand::block(s::BlockId{1})});
            else {
                m.emit(mainBody, 0, Op::Call, {Operand::subroutine(task.asSubroutine()), actual});
                m.emit(mainBody, 0, Op::Br, {Operand::block(s::BlockId{1})});
            }
            const auto read = m.emit(mainBody, 1, Op::Fload, {frame(0)}, type);
            m.store(mainBody, 1, answer, read);
            m.emit(mainBody, 1, Op::Ret);
            if (waiting)
                m.emit(task, 0, Op::WaitDelay,
                       {m.constant(s::u2(32), 1), Operand::block(s::BlockId{1})});
            m.emit(task, waiting ? 1 : 0, Op::StoreRef, {frame(0), m.constant(type, 17, 1)});
            m.emit(task, waiting ? 1 : 0, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context context;
            const auto result = kirimasim::vm::run(m.d, context);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            for (unsigned i = 0; i < (width + 63) / 64; ++i) {
                const auto mask = k::mask(std::min(64U, width - i * 64));
                EXPECT_EQ(word(context, answer, 0, 2 * i), 17 & mask);
                EXPECT_EQ(word(context, answer, 0, 2 * i + 1), 1U);
            }
            released(context);
        }
}

struct HeapOwnerObserver {
    k::ProcessId process{0};
    bool waiting = false, seen = false;
    k::HeapStats stats;
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<HeapOwnerObserver*>(frame);
        auto& context = k::fromC(raw);
        if (!self.waiting) {
            self.waiting = true;
            context.scheduler().waitDelay(self.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        self.seen = true;
        self.stats = context.heap().stats();
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(VmActivation, ExtendedScopesKeepUnreadTypedHeapFieldsAlive) {
    Model m;
    s::TypeEntry te;
    te.kind = s::TypeEntryKind::DynArray;
    te.name = "words";
    te.value = s::u2(32);
    m.d.types.push_back(te);
    const auto array = s::dynarrayType(s::TypeId{0});
    const auto answer = m.slot(s::u2(32));
    m.root(mainBody, {{"read", s::u2(32)}, {"unread", array}});
    const auto child = m.child(mainBody, {0});
    const auto packed = m.emit(mainBody, 0, Op::ContainerPack,
                               {m.constant(s::u2(32), 11), m.constant(s::u2(32), 13)}, array);
    m.store(mainBody, 0, frame(1), packed);
    m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 7));
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::Ret);
    m.emit(child, 0, Op::WaitDelay, {m.constant(s::u2(32), 2), Operand::block(s::BlockId{1})});
    const auto value = m.emit(child, 1, Op::Fload, {frame(0)}, s::u2(32));
    m.store(child, 1, answer, value);
    m.emit(child, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    HeapOwnerObserver observer;
    observer.process =
        context.scheduler().addProcess(&HeapOwnerObserver::resume, &observer, k::Group::Active);
    context.scheduler().start(observer.process);
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    ASSERT_TRUE(observer.seen);
    EXPECT_EQ(observer.stats.containers, 1U);
    EXPECT_EQ(observer.stats.cells, 2U);
    EXPECT_EQ(word(context, answer), 7U);
    released(context);
    EXPECT_EQ(context.heap().stats().containers, 0U);
    EXPECT_EQ(context.heap().stats().cells, 0U);
}

TEST(VmActivation, ProgramCompletionKillsRetainedChildrenBeforeFinalRuns) {
    Model m;
    m.d.instances[0].kind = s::InstanceKind::Program;
    const auto childRan = m.slot(s::u2(32)), finalRan = m.slot(s::u2(32));
    m.root(mainBody, {{"value", s::u2(32)}});
    const auto child = m.child(mainBody, {0});
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::WaitDelay, {m.constant(s::u2(32), 1), Operand::block(s::BlockId{2})});
    m.emit(mainBody, 2, Op::Ret);
    m.emit(child, 0, Op::WaitDelay, {m.constant(s::u2(32), 3), Operand::block(s::BlockId{1})});
    m.store(child, 1, childRan, m.constant(s::u2(32), 99));
    m.emit(child, 1, Op::Ret);
    s::Process final;
    final.name = "top.final";
    final.kind = s::ProcessKind::Final;
    final.loc = {0, 1, 1};
    const auto body = s::Owner::of(s::ProcessId{static_cast<std::uint32_t>(m.d.processes.size())});
    m.d.processes.push_back(std::move(final));
    m.store(body, 0, finalRan, m.constant(s::u2(32), 1));
    m.emit(body, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Finish) << result.error;
    EXPECT_EQ(word(context, childRan), 0U);
    EXPECT_EQ(word(context, finalRan), 1U);
    released(context);
}

TEST(VmActivation, PendingIffIsDetachedBeforeItsLexicalFrameExpires) {
    Model m;
    const auto trigger = m.slot(s::u2(1)), answer = m.slot(s::u2(32));
    m.root(mainBody, {{"local", s::u2(32)}});
    s::Subroutine guard;
    guard.name = "top.iff";
    guard.kind = s::SubroutineKind::Guard;
    guard.frameOf = mainBody;
    guard.returnType = s::u2(1);
    guard.loc = {0, 1, 1};
    m.d.subroutines.push_back(std::move(guard));
    const auto body = s::Owner::of(s::SubroutineId{0});
    const auto read = m.emit(body, 0, Op::Fload, {frame(0)}, s::u2(32));
    const auto truth = m.emit(body, 0, Op::Truth, {read}, s::u2(1));
    m.emit(body, 0, Op::Ret, {truth});
    m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 19));
    m.emit(mainBody, 0, Op::WaitEvent,
           {trigger, Operand::subroutine(s::SubroutineId{0}), Operand::block(s::BlockId{1})});
    m.store(mainBody, 1, answer, m.constant(s::u2(32), 99));
    m.emit(mainBody, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    released(context);
    const std::uint64_t one = 1;
    EXPECT_TRUE(context.scheduler().write({k::SlotId{trigger.id}, 0, 0, 1}, &one));
    context.scheduler().run();
    EXPECT_EQ(word(context, answer), 0U);
}

TEST(VmActivation, BorrowedDescriptorsExpireAtScopeLeaveEvenWhenChildrenRetainData) {
    Model m;
    m.root(mainBody, {{"local", s::u2(32)}, {"alias", s::refType(), s::kNone, {}, s::u2(32)}});
    auto& info = m.activation(mainBody);
    info.scopes[0].fields.clear();
    info.scopes.push_back({s::ActivationScopeId{0}, {s::FrameVarId{0}}});
    m.fields(mainBody)[0].activationScope = s::ActivationScopeId{1};
    const auto child = m.child(mainBody, {0}, {s::ActivationScopeId{0}, s::ActivationScopeId{1}});
    m.emit(mainBody, 0, Op::ActivationEnter, {Operand::activation(s::ActivationScopeId{1})});
    m.store(mainBody, 0, frame(0), m.constant(s::u2(32), 7));
    const auto alias = m.emit(mainBody, 0, Op::RefMake, {frame(0)}, s::refType());
    m.store(mainBody, 0, frame(1), alias);
    m.fork(mainBody, 0, {child}, "join_none", 1);
    m.emit(mainBody, 1, Op::ActivationLeave, {Operand::activation(s::ActivationScopeId{1})});
    // Defense for hand-built SIR: retained data must not extend a stale
    // borrowed ticket. Source refs are not first-class escaping values.
    m.emit(mainBody, 1, Op::StoreRef, {frame(1), m.constant(s::u2(32), 99)});
    m.emit(mainBody, 1, Op::Ret);
    m.emit(child, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    EXPECT_EQ(result.reason, k::EndReason::Internal);
    released(context);
}

TEST(VmActivation, NamedDisableUsesBirthSerialAfterEarlierChildRecordsAreReclaimed) {
    Model m;
    m.d.numScopes = 1;
    const auto survived = m.slot(s::u2(32)), forbidden = m.slot(s::u2(32));
    m.root(mainBody, {{"value", s::u2(32)}});
    const auto fast = m.child(mainBody, {}), older = m.child(mainBody, {0}),
               inside = m.child(mainBody, {0});
    m.fork(mainBody, 0, {fast, older}, "join_any", 1);
    m.emit(mainBody, 1, Op::ScopeEnter, {Operand::scope(0), Operand::block(s::BlockId{3})});
    m.fork(mainBody, 1, {inside}, "join_none", 2);
    m.emit(mainBody, 2, Op::DisableScope, {Operand::scope(0)});
    m.emit(mainBody, 2, Op::Br, {Operand::block(s::BlockId{3})});
    m.emit(mainBody, 3, Op::WaitFork, {Operand::block(s::BlockId{4})});
    m.emit(mainBody, 4, Op::Ret);
    m.emit(fast, 0, Op::Ret);
    m.emit(older, 0, Op::WaitDelay, {m.constant(s::u2(32), 2), Operand::block(s::BlockId{1})});
    m.store(older, 1, survived, m.constant(s::u2(32), 1));
    m.emit(older, 1, Op::Ret);
    m.emit(inside, 0, Op::WaitDelay, {m.constant(s::u2(32), 5), Operand::block(s::BlockId{1})});
    m.store(inside, 1, forbidden, m.constant(s::u2(32), 1));
    m.emit(inside, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(m.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, survived), 1U);
    EXPECT_EQ(word(context, forbidden), 0U);
    EXPECT_EQ(context.scheduler().time(), 2U);
    released(context);
}

}  // namespace
