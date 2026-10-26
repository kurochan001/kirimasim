// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original instruction-boundary checks for the approved RUNTIME_ABI 6 contract.
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
using s::Op;
using s::Operand;
using Body = s::Owner;
const s::Type event{s::TypeKind::Event, 0};
Operand field(unsigned n) { return Operand::frame(s::FrameVarId{n}); }
Operand block(unsigned n, unsigned count = 0) {
    auto result = Operand::block(s::BlockId{n});
    result.count = count;
    return result;
}
struct Graph {
    s::Design d;
    Graph() {
        d.sources = {"event_gc.sv"};
        d.instances = {{"top", "top", s::kNone}};
    }
    Body process() {
        s::Process p;
        p.name = "top.p" + std::to_string(d.processes.size());
        p.loc = {0, 1, 1};
        d.processes.push_back(std::move(p));
        return Body::of(s::ProcessId{static_cast<std::uint32_t>(d.processes.size() - 1)});
    }
    Body sub(s::SubroutineKind kind, s::Type result) {
        s::Subroutine p;
        p.name = "top.f" + std::to_string(d.subroutines.size());
        p.kind = kind;
        p.automatic = true;
        p.returnType = result;
        p.loc = {0, 2, 1};
        d.subroutines.push_back(std::move(p));
        return Body::of(s::SubroutineId{static_cast<std::uint32_t>(d.subroutines.size() - 1)});
    }
    auto& blocks(Body b) {
        return b.kind == Body::Kind::Process ? d.processes[b.asProcess().index()].blocks
                                             : d.subroutines[b.asSubroutine().index()].blocks;
    }
    auto& temps(Body b) {
        return b.kind == Body::Kind::Process ? d.processes[b.asProcess().index()].numTemps
                                             : d.subroutines[b.asSubroutine().index()].numTemps;
    }
    void scoped(Body b, std::vector<s::FrameVar> rows, bool lexical = false) {
        auto& info = b.kind == Body::Kind::Process ? d.processes[b.id].activation
                                                   : d.subroutines[b.id].activation;
        info.root = s::ActivationScopeId{0};
        info.scopes.resize(lexical ? 2 : 1);
        if (lexical) info.scopes[1].parent = s::ActivationScopeId{0};
        for (unsigned i = 0; i < rows.size(); ++i) {
            rows[i].home = s::FrameHome::Scoped;
            rows[i].activationScope = s::ActivationScopeId{lexical ? 1U : 0U};
            info.scopes[lexical ? 1 : 0].fields.push_back(s::FrameVarId{i});
        }
        if (b.kind == Body::Kind::Process)
            d.processes[b.id].frame = std::move(rows);
        else
            d.subroutines[b.id].frame = std::move(rows);
    }
    Operand value(s::Type type, std::uint64_t value) {
        const auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back(
            {type,
             {value},
             s::isFourState(type) ? std::vector<std::uint64_t>{0} : std::vector<std::uint64_t>{}});
        return Operand::constant(id);
    }
    Operand number(std::uint64_t n) { return value(s::u2(64), n); }
    Operand slot(s::Type type) {
        s::Slot slot;
        slot.name = "top.v" + std::to_string(d.slots.size());
        slot.type = type;
        const auto id = s::SlotId{static_cast<std::uint32_t>(d.slots.size())};
        d.slots.push_back(std::move(slot));
        return Operand::slot(id);
    }
    Operand emit(Body b, unsigned bb, Op op, std::vector<Operand> args = {},
                 s::Type type = s::voidType()) {
        if (blocks(b).size() <= bb) blocks(b).resize(bb + 1);
        const s::TempId id{type == s::voidType() ? s::kNone : temps(b)++};
        blocks(b)[bb].instrs.push_back({op, type, id, std::move(args), {0, 7, 1}});
        return Operand::temp(id);
    }
    void churn(Body b) {
        emit(b, 0, Op::EventNew, {}, event);
        emit(b, 0, Op::EventNew, {}, event);
        emit(b, 0, Op::Ret, {value(s::u2(1), 1)});
    }
    void guard(Body guard, Body waiter, Body helper, const Operand& signal) {
        d.subroutines[guard.asSubroutine().index()].frameOf = waiter;
        const auto on = emit(guard, 0, Op::Load, {signal}, s::u2(1));
        emit(guard, 0, Op::BrIf, {on, block(1), block(2)});
        const auto result =
            emit(guard, 1, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
        emit(guard, 1, Op::Ret, {result});
        emit(guard, 2, Op::Ret, {value(s::u2(1), 0)});
    }
    ::testing::AssertionResult valid() {
        for (auto& p : d.processes)
            for (unsigned i = 0; i < d.slots.size(); ++i)
                p.writes.push_back({s::SlotId{i}});
        const auto errors = s::verify(d);
        if (errors.empty()) return ::testing::AssertionSuccess();
        std::string text;
        for (const auto& error : errors)
            text += error.where + ": " + error.message + '\n';
        return ::testing::AssertionFailure() << text;
    }
};
std::uint64_t word(const k::Context& context, const Operand& slot) {
    return *context.store().element(k::SlotId{slot.id}, 0);
}
struct Memory : std::pmr::memory_resource {
    bool fail = false;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (fail) throw std::bad_alloc();
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
struct Observer {
    k::Context* context;
    k::SlotId signal;
    k::ProcessId process = k::kNone;
    std::vector<std::size_t> live;
    Memory* memory = nullptr;
    static int inspect(kirima_context*, void* raw) {
        auto& self = *static_cast<Observer*>(raw);
        self.live.push_back(self.context->managedEvents().liveObjects());
        if (self.memory != nullptr) self.memory->fail = true;
        return 0;
    }
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<Observer*>(frame);
        const k::WaitItem item{{self.signal}, k::Edge::Any, &inspect, &self};
        k::fromC(raw).scheduler().waitEvent(self.process, {&item, 1});
        return static_cast<kirima_status>(k::Status::Waiting);
    }
    void start() {
        process = context->scheduler().addProcess(&resume, this, k::Group::Active);
        context->scheduler().start(process);
    }
};

TEST(VmEventGc, NonwaitingProcessLoopsReusePhysicalRowsWithoutAdvancingTime) {
    for (const unsigned iterations : {1U, 2U, 65U, 4096U}) {
        Graph g;
        const auto p = g.process();
        const auto answer = g.slot(s::u2(64));
        const s::TempId iteration{g.temps(p)++};
        g.blocks(p).resize(3);
        g.blocks(p)[1].params = {{iteration, s::u2(64)}};
        g.emit(p, 0, Op::Br, {block(1, 1), g.number(0)});
        const auto fresh = g.emit(p, 1, Op::EventNew, {}, event);
        g.emit(p, 1, Op::Trigger, {fresh});
        const auto next = g.emit(p, 1, Op::Add, {Operand::temp(iteration), g.number(1)}, s::u2(64));
        g.emit(p, 1, Op::Store, {answer, next});
        const auto again = g.emit(p, 1, Op::Lt, {next, g.number(iterations)}, s::u2(1));
        g.emit(p, 1, Op::BrIf, {again, block(1, 1), next, block(2)});
        g.emit(p, 2, Op::Ret);
        ASSERT_TRUE(g.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(g.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, answer), iterations);
        EXPECT_EQ(context.scheduler().time(), 0U);
        EXPECT_LE(context.managedEvents().allocatedObjects(), 2U);
        EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
    }
}

TEST(VmEventGc, DeferredCopyInAndNestedCallsPublishOrdinaryFieldsAfterInputPinsEnd) {
    for (const bool automatic : {false, true}) {
        SCOPED_TRACE(automatic);
        Graph g;
        const auto issuer = g.process(), waiter = g.process();
        const auto flag = g.slot(s::u2(1)), answer = g.slot(s::u2(1));
        const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
        g.churn(helper);
        const auto guard = g.sub(s::SubroutineKind::Guard, s::u2(1));
        g.guard(guard, waiter, helper, flag);
        g.emit(waiter, 0, Op::WaitCond, {Operand::subroutine(guard.asSubroutine()), block(1)});
        g.emit(waiter, 1, Op::Ret);
        const auto action = g.sub(s::SubroutineKind::Function, s::voidType());
        auto& sub = g.d.subroutines[action.asSubroutine().index()];
        sub.automatic = automatic;
        Operand source;
        if (automatic) {
            sub.frame = {{"target", event}, {"flag", s::u2(1)}};
            sub.params = {{"flag", s::Direction::Input, s::u2(1), s::kNone, s::FrameVarId{1}},
                          {"target", s::Direction::Input, event, s::kNone, s::FrameVarId{0}}};
            source = field(0);
        } else {
            source = g.slot(event);
            sub.params = {{"flag", s::Direction::Input, s::u2(1), s::SlotId{flag.id}},
                          {"target", s::Direction::Input, event, s::SlotId{source.id}}};
        }
        g.emit(action, 0, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
        const auto target = g.emit(action, 0, automatic ? Op::Fload : Op::Load, {source}, event);
        g.emit(action, 0, Op::Trigger, {target});
        const auto on =
            g.emit(action, 0, Op::Builtin, {Operand::str("triggered"), target}, s::u2(1));
        g.emit(action, 0, Op::Store, {answer, on});
        g.emit(action, 0, Op::Ret);
        const auto fresh = g.emit(issuer, 0, Op::EventNew, {}, event);
        g.emit(issuer, 0, Op::ReportCall,
               {Operand::str("#0"), Operand::subroutine(action.asSubroutine()),
                g.value(s::u2(1), 1), fresh});
        g.emit(issuer, 0, Op::Ret);
        ASSERT_TRUE(g.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(g.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, answer), 1U);
        EXPECT_LE(context.managedEvents().allocatedObjects(), 3U);
        EXPECT_EQ(context.managedEvents().liveObjects(), automatic ? 0U : 1U);
    }
}

TEST(VmEventGc, ReturnPinSurvivesCollectionInAnOutputCopybackGuard) {
    Graph g;
    const auto caller = g.process(), waiter = g.process();
    const auto flag = g.slot(s::u2(1)), answer = g.slot(s::u2(1));
    const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
    g.churn(helper);
    const auto guard = g.sub(s::SubroutineKind::Guard, s::u2(1));
    g.guard(guard, waiter, helper, flag);
    g.emit(waiter, 0, Op::WaitCond, {Operand::subroutine(guard.asSubroutine()), block(1)});
    g.emit(waiter, 1, Op::Ret);
    const auto function = g.sub(s::SubroutineKind::Function, event);
    auto& sub = g.d.subroutines[function.asSubroutine().index()];
    sub.frame = {{"flag", s::u2(1)}};
    sub.params = {{"flag", s::Direction::Output, s::u2(1), s::kNone, s::FrameVarId{0}}};
    g.emit(function, 0, Op::Fstore, {field(0), g.value(s::u2(1), 1)});
    const auto fresh = g.emit(function, 0, Op::EventNew, {}, event);
    g.emit(function, 0, Op::Ret, {fresh});
    g.emit(caller, 0, Op::WaitDelay, {g.number(0), block(1)});
    const auto returned =
        g.emit(caller, 1, Op::Call, {Operand::subroutine(function.asSubroutine()), flag}, event);
    g.emit(caller, 1, Op::Trigger, {returned});
    const auto on = g.emit(caller, 1, Op::Builtin, {Operand::str("triggered"), returned}, s::u2(1));
    g.emit(caller, 1, Op::Store, {answer, on});
    g.emit(caller, 1, Op::Ret);
    ASSERT_TRUE(g.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(g.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer), 1U);
    EXPECT_EQ(context.scheduler().time(), 0U);
    EXPECT_LE(context.managedEvents().allocatedObjects(), 3U);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(VmEventGc, NonwaitingFunctionAndTaskLoopsRetireFreshLexicalEventArrays) {
    for (const bool task : {false, true})
        for (const bool lexical : {false, true}) {
            SCOPED_TRACE(::testing::Message() << task << '/' << lexical);
            Graph g;
            const auto p = g.process();
            const auto f =
                g.sub(task ? s::SubroutineKind::Task : s::SubroutineKind::Function, s::voidType());
            g.scoped(f, {{"array", event, s::kNone, {{-1, 1}}}}, lexical);
            const auto answer = g.slot(s::u2(64));
            const s::TempId iteration{g.temps(f)++};
            g.blocks(f).resize(3);
            g.blocks(f)[1].params = {{iteration, s::u2(64)}};
            g.emit(f, 0, Op::Br, {block(1, 1), g.number(0)});
            if (lexical)
                g.emit(f, 1, Op::ActivationEnter, {Operand::activation(s::ActivationScopeId{1})});
            for (std::int64_t e = -1; e <= 1; ++e) {
                const auto fresh = g.emit(f, 1, Op::EventNew, {}, event);
                g.emit(f, 1, Op::StoreElem, {field(0), g.value(s::s2(64), e), fresh});
            }
            // Earlier elements remain typed roots even without any subsequent read.
            if (lexical)
                g.emit(f, 1, Op::ActivationLeave, {Operand::activation(s::ActivationScopeId{1})});
            const auto next =
                g.emit(f, 1, Op::Add, {Operand::temp(iteration), g.number(1)}, s::u2(64));
            g.emit(f, 1, Op::Store, {answer, next});
            const auto again = g.emit(f, 1, Op::Lt, {next, g.number(1024)}, s::u2(1));
            g.emit(f, 1, Op::BrIf, {again, block(1, 1), next, block(2)});
            g.emit(f, 2, Op::Ret);
            g.emit(p, 0, Op::Call, {Operand::subroutine(f.asSubroutine())});
            g.emit(p, 0, Op::Ret);
            ASSERT_TRUE(g.valid());
            k::Context context;
            const auto result = kirimasim::vm::run(g.d, context);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_EQ(word(context, answer), 1024U);
            EXPECT_EQ(context.scheduler().time(), 0U);
            EXPECT_LE(context.managedEvents().allocatedObjects(), 6U);
            EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
            EXPECT_EQ(context.activations().stats().live, 0U);
        }
}

TEST(VmEventGc, RecursiveCallsKeepAllOuterSsaValuesUntilTheirOwnReturn) {
    for (const unsigned depth : {1U, 8U, 64U}) {
        SCOPED_TRACE(depth);
        Graph g;
        const auto p = g.process(), f = g.sub(s::SubroutineKind::Function, event);
        auto& sub = g.d.subroutines[f.id];
        sub.frame = {{"depth", s::u2(64)}};
        sub.params = {{"depth", s::Direction::Input, s::u2(64), s::kNone, s::FrameVarId{0}}};
        const auto answer = g.slot(s::u2(1));
        const auto fresh = g.emit(f, 0, Op::EventNew, {}, event);
        const auto n = g.emit(f, 0, Op::Fload, {field(0)}, s::u2(64));
        const auto again = g.emit(f, 0, Op::Ne, {n, g.number(0)}, s::u2(1));
        g.emit(f, 0, Op::BrIf, {again, block(1), block(2)});
        const auto next = g.emit(f, 1, Op::Sub, {n, g.number(1)}, s::u2(64));
        const auto inner =
            g.emit(f, 1, Op::Call, {Operand::subroutine(f.asSubroutine()), next}, event);
        g.emit(f, 1, Op::Trigger, {inner});
        g.emit(f, 1, Op::Br, {block(2)});
        g.emit(f, 2, Op::Trigger, {fresh});
        g.emit(f, 2, Op::Ret, {fresh});
        const auto returned =
            g.emit(p, 0, Op::Call, {Operand::subroutine(f.asSubroutine()), g.number(depth)}, event);
        const auto on = g.emit(p, 0, Op::Builtin, {Operand::str("triggered"), returned}, s::u2(1));
        g.emit(p, 0, Op::Store, {answer, on});
        g.emit(p, 0, Op::Ret);
        ASSERT_TRUE(g.valid());
        k::Context context;
        const auto result = kirimasim::vm::run(g.d, context);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(context, answer), 1U);
        EXPECT_LE(context.managedEvents().allocatedObjects(), depth + 1U);
        EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
    }
}

TEST(VmEventGc, DelayedNamedAndMixedTargetsSurvivePressureInBothSchedulingGroups) {
    for (const bool reactive : {false, true})
        for (const bool mixed : {false, true}) {
            SCOPED_TRACE(::testing::Message() << reactive << '/' << mixed);
            Graph g;
            if (reactive) g.d.instances[0].kind = s::InstanceKind::Program;
            const auto issuer = g.process(), waiter = g.process();
            const auto target = g.slot(event), answer = g.slot(s::u2(1));
            const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
            g.churn(helper);
            const auto fresh = g.emit(issuer, 0, Op::EventNew, {}, event);
            g.emit(issuer, 0, Op::Store, {target, fresh});
            g.emit(issuer, 0, Op::TriggerNb, {fresh, g.number(3)});
            g.emit(issuer, 0, Op::WaitDelay, {g.number(1), block(1)});
            g.emit(issuer, 1, Op::Store, {target, g.value(event, 0)});
            for (unsigned i = 0; i < 128; ++i)
                g.emit(issuer, 1, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
            g.emit(issuer, 1, Op::Ret);
            const auto selected = g.emit(waiter, 0, Op::Load, {target}, event);
            std::vector<Operand> items{selected};
            if (mixed) items.push_back(answer);
            items.push_back(block(1));
            g.emit(waiter, 0, mixed ? Op::WaitEvent : Op::WaitNamed, std::move(items));
            g.emit(waiter, 1, Op::Store, {answer, g.value(s::u2(1), 1)});
            g.emit(waiter, 1, Op::Ret);
            ASSERT_TRUE(g.valid());
            k::Context context;
            const auto result = kirimasim::vm::run(g.d, context);
            ASSERT_EQ(result.reason, reactive ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(word(context, answer), 1U);
            EXPECT_EQ(context.scheduler().time(), 3U);
            EXPECT_LE(context.managedEvents().allocatedObjects(), 3U);
            ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
        }
}

TEST(VmEventGc, CancellationInCopyCallbacksStopsRemainingTransfersAndCalleeEffects) {
    for (const bool programExit : {false, true})
        for (const bool output : {false, true})
            for (const bool waitingTask : {false, true}) {
                SCOPED_TRACE(::testing::Message()
                             << programExit << '/' << output << '/' << waitingTask);
                Graph g;
                if (programExit) g.d.instances[0].kind = s::InstanceKind::Program;
                const auto issuer = g.process(), waiter = g.process();
                const auto first = g.slot(s::u2(1)), second = g.slot(s::u2(1));
                const auto effect = g.slot(s::u2(1)), after = g.slot(s::u2(1));
                const auto formalFirst = g.slot(s::u2(1)), formalSecond = g.slot(s::u2(1));
                const auto stop = g.sub(s::SubroutineKind::Function, s::u2(1));
                g.emit(stop, 0, Op::EventNew, {}, event);
                g.emit(stop, 0, Op::EventNew, {}, event);
                g.emit(stop, 0, Op::Syscall,
                       programExit ? std::vector<Operand>{Operand::str("$exit")}
                                   : std::vector<Operand>{Operand::str("$finish"), g.number(0)});
                g.emit(stop, 0, Op::Ret, {g.value(s::u2(1), 1)});
                const auto guard = g.sub(s::SubroutineKind::Guard, s::u2(1));
                g.guard(guard, waiter, stop, output ? first : formalFirst);
                g.emit(waiter, 0, Op::WaitCond,
                       {Operand::subroutine(guard.asSubroutine()), block(1)});
                g.emit(waiter, 1, Op::Ret);
                const auto f =
                    g.sub(waitingTask ? s::SubroutineKind::Task : s::SubroutineKind::Function,
                          s::voidType());
                auto& sub = g.d.subroutines[f.id];
                sub.automatic = false;
                sub.mayWait = waitingTask;
                const auto direction = output ? s::Direction::Output : s::Direction::Input;
                sub.params = {{"first", direction, s::u2(1), formalFirst.asSlot()},
                              {"second", direction, s::u2(1), formalSecond.asSlot()}};
                if (output) {
                    g.emit(f, 0, Op::Store, {formalFirst, g.value(s::u2(1), 1)});
                    g.emit(f, 0, Op::Store, {formalSecond, g.value(s::u2(1), 1)});
                }
                g.emit(f, 0, Op::Store, {effect, g.value(s::u2(1), 1)});
                g.emit(f, 0, Op::Ret);
                g.emit(issuer, 0, Op::WaitDelay, {g.number(0), block(1)});
                std::vector<Operand> args{Operand::subroutine(f.asSubroutine()),
                                          output ? first : g.value(s::u2(1), 1),
                                          output ? second : g.value(s::u2(1), 1)};
                if (waitingTask) args.push_back(block(2));
                g.emit(issuer, 1, waitingTask ? Op::CallTask : Op::Call, std::move(args));
                g.emit(issuer, waitingTask ? 2 : 1, Op::Store, {after, g.value(s::u2(1), 1)});
                g.emit(issuer, waitingTask ? 2 : 1, Op::Ret);
                ASSERT_TRUE(g.valid());
                k::Context context;
                const auto result = kirimasim::vm::run(g.d, context);
                ASSERT_EQ(result.reason, k::EndReason::Finish) << result.error;
                EXPECT_EQ(word(context, output ? second : formalSecond), 0U);
                EXPECT_EQ(word(context, effect), output ? 1U : 0U);
                EXPECT_EQ(word(context, after), 0U);
                EXPECT_LE(context.managedEvents().allocatedObjects(), 2U);
                ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
                EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
            }
}

TEST(VmEventGc, RetainedDescendantKeepsEveryUnreadEventArrayElementDuringCollection) {
    Graph g;
    const auto owner = g.process(), child = g.process();
    g.scoped(owner, {{"events", event, s::kNone, {{-1, 1}}}, {"value", s::u2(64)}});
    auto& descendant = g.d.processes[child.id];
    descendant.kind = s::ProcessKind::General;
    descendant.parent = owner;
    descendant.activation.extents = {s::ActivationScopeId{0}};
    s::FrameVar captured{"value", s::u2(64)};
    captured.home = s::FrameHome::Capture;
    captured.captureSource = s::FrameVarId{1};
    descendant.frame = {captured};
    const auto signal = g.slot(s::u2(1)), answer = g.slot(s::u2(64));
    const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
    g.churn(helper);
    for (std::int64_t e = -1; e <= 1; ++e) {
        const auto fresh = g.emit(owner, 0, Op::EventNew, {}, event);
        g.emit(owner, 0, Op::StoreElem, {field(0), g.value(s::s2(64), e), fresh});
    }
    g.emit(owner, 0, Op::Fstore, {field(1), g.number(19)});
    g.emit(owner, 0, Op::Fork,
           {Operand::process(child.asProcess()), Operand::str("join_none"), block(1)});
    g.emit(owner, 1, Op::Ret);
    g.emit(child, 0, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
    g.emit(child, 0, Op::Store, {signal, g.value(s::u2(1), 1)});
    const auto value = g.emit(child, 0, Op::Fload, {field(0)}, s::u2(64));
    g.emit(child, 0, Op::Store, {answer, value});
    g.emit(child, 0, Op::Ret);
    ASSERT_TRUE(g.valid());
    k::Context context;
    Observer observer{&context, k::SlotId{signal.id}};
    observer.start();
    const auto result = kirimasim::vm::run(g.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    ASSERT_EQ(observer.live.size(), 1U);
    EXPECT_GE(observer.live[0], 3U);
    EXPECT_LE(context.managedEvents().allocatedObjects(), 5U);
    EXPECT_EQ(word(context, answer), 19U);
    EXPECT_EQ(context.activations().stats().live, 0U);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
    context.scheduler().cancelWait(observer.process);
}

TEST(VmEventGc, ReentrantGuardsKeepTheNativeCallersSsaRegionPublished) {
    Graph g;
    const auto issuer = g.process(), second = g.process(), first = g.process();
    const auto a = g.slot(s::u2(1)), b = g.slot(s::u2(1));
    const auto answer = g.slot(s::u2(1)), reached = g.slot(s::u2(1));
    const auto inner = g.sub(s::SubroutineKind::Function, s::u2(1));
    g.churn(inner);
    const auto outer = g.sub(s::SubroutineKind::Function, s::u2(1));
    const auto fresh = g.emit(outer, 0, Op::EventNew, {}, event);
    g.emit(outer, 0, Op::Store, {b, g.value(s::u2(1), 1)});
    g.emit(outer, 0, Op::Builtin, {Operand::str("triggered"), fresh}, s::u2(1));
    g.emit(outer, 0, Op::Ret, {g.value(s::u2(1), 1)});
    const auto guardA = g.sub(s::SubroutineKind::Guard, s::u2(1));
    const auto guardB = g.sub(s::SubroutineKind::Guard, s::u2(1));
    g.guard(guardA, first, outer, a);
    g.guard(guardB, second, inner, b);
    g.emit(first, 0, Op::WaitCond, {Operand::subroutine(guardA.asSubroutine()), block(1)});
    g.emit(first, 1, Op::Store, {answer, g.value(s::u2(1), 1)});
    g.emit(first, 1, Op::Ret);
    g.emit(second, 0, Op::WaitCond, {Operand::subroutine(guardB.asSubroutine()), block(1)});
    g.emit(second, 1, Op::Store, {reached, g.value(s::u2(1), 1)});
    g.emit(second, 1, Op::Ret);
    g.emit(issuer, 0, Op::Store, {a, g.value(s::u2(1), 1)});
    g.emit(issuer, 0, Op::Ret);
    ASSERT_TRUE(g.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(g.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer), 1U);
    EXPECT_EQ(word(context, reached), 1U);
    EXPECT_EQ(context.scheduler().time(), 0U);
    EXPECT_LE(context.managedEvents().allocatedObjects(), 3U);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(VmEventGc, EventInstructionCollectionNeverCollectsTheContainerHeap) {
    Graph g;
    const auto p = g.process();
    s::TypeEntry row;
    row.kind = s::TypeEntryKind::DynArray;
    row.value = s::u2(64);
    g.d.types.push_back(row);
    const auto array = s::dynarrayType(s::TypeId{0});
    const auto answer = g.slot(s::u2(64));
    const auto live =
        g.emit(p, 0, Op::ContainerNew, {g.value(s::s4(64), 4), g.value(array, 0)}, array);
    g.emit(p, 0, Op::EventNew, {}, event);
    g.emit(p, 0, Op::EventNew, {}, event);
    const auto size = g.emit(p, 0, Op::ContainerSize, {live}, s::u2(64));
    g.emit(p, 0, Op::Store, {answer, size});
    g.emit(p, 0, Op::Ret);
    ASSERT_TRUE(g.valid());
    k::Context context;
    const auto result = kirimasim::vm::run(g.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(context, answer), 4U);
    EXPECT_EQ(context.heap().stats().containers, 0U);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(VmEventGc, TerminationAndFinalCallsKeepTheExistingDeferredCleanupPolicy) {
    Graph g;
    const auto p = g.process(), final = g.process();
    g.d.processes[final.id].kind = s::ProcessKind::Final;
    const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
    g.churn(helper);
    const auto answer = g.slot(s::u2(1));
    g.emit(p, 0, Op::EventNew, {}, event);
    g.emit(p, 0, Op::Syscall, {Operand::str("$finish"), g.number(0)});
    g.emit(p, 0, Op::Ret);
    for (unsigned i = 0; i < 16; ++i)
        g.emit(final, 0, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
    g.emit(final, 0, Op::Store, {answer, g.value(s::u2(1), 1)});
    g.emit(final, 0, Op::Ret);
    ASSERT_TRUE(g.valid());
    k::Context context;
    const k::SafePointHook old{[](void*) noexcept {}, &g};
    context.scheduler().setSafePointHook(old);
    const auto result = kirimasim::vm::run(g.d, context);
    ASSERT_EQ(result.reason, k::EndReason::Finish) << result.error;
    EXPECT_EQ(word(context, answer), 1U);
    EXPECT_EQ(context.managedEvents().allocatedObjects(), 33U);
    EXPECT_EQ(context.managedEvents().liveObjects(), 33U);
    const auto restored = context.scheduler().setSafePointHook({});
    EXPECT_EQ(restored.fn, old.fn);
    EXPECT_EQ(restored.frame, old.frame);
    ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(VmEventGc, CreationFailurePreservesPublishedValuesAndReusedRowsNeedNoAllocation) {
    for (const bool reuse : {false, true}) {
        SCOPED_TRACE(reuse);
        Graph g;
        const auto p = g.process();
        const auto arm = g.slot(s::u2(1)), answer = g.slot(s::u2(64));
        const auto target = g.slot(event);
        if (reuse) {
            const auto helper = g.sub(s::SubroutineKind::Function, s::u2(1));
            g.churn(helper);
            g.emit(p, 0, Op::Call, {Operand::subroutine(helper.asSubroutine())}, s::u2(1));
        } else {
            const auto original = g.emit(p, 0, Op::EventNew, {}, event);
            g.emit(p, 0, Op::Store, {target, original});
        }
        g.emit(p, 0, Op::Store, {arm, g.value(s::u2(1), 1)});
        const s::TempId iteration{g.temps(p)++};
        g.blocks(p).resize(3);
        g.blocks(p)[1].params = {{iteration, s::u2(64)}};
        g.emit(p, 0, Op::Br, {block(1, 1), g.number(0)});
        g.emit(p, 1, Op::EventNew, {}, event);
        const auto next = g.emit(p, 1, Op::Add, {Operand::temp(iteration), g.number(1)}, s::u2(64));
        g.emit(p, 1, Op::Store, {answer, next});
        const auto again = g.emit(p, 1, Op::Lt, {next, g.number(64)}, s::u2(1));
        g.emit(p, 1, Op::BrIf, {again, block(1, 1), next, block(2)});
        g.emit(p, 2, Op::Ret);
        ASSERT_TRUE(g.valid());
        Memory memory;
        auto* output = std::tmpfile();
        ASSERT_NE(output, nullptr);
        {
            k::Context context({output, output}, nullptr, &memory);
            Observer observer{&context, k::SlotId{arm.id}, k::kNone, {}, &memory};
            observer.start();
            const auto result = kirimasim::vm::run(g.d, context);
            ASSERT_EQ(result.reason, reuse ? k::EndReason::Quiescent : k::EndReason::ResourceLimit)
                << result.error;
            EXPECT_EQ(observer.live.size(), 1U);
            EXPECT_EQ(word(context, answer), reuse ? 64U : 0U);
            EXPECT_EQ(context.managedEvents().allocatedObjects(), reuse ? 2U : 1U);
            if (!reuse) EXPECT_TRUE(context.managedEvents().valid({word(context, target)}));
            memory.fail = false;
            context.scheduler().cancelWait(observer.process);
            ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
        }
        std::fclose(output);
    }
}
}  // namespace
