// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original execution checks: SIR_SPEC 3.2.1/7/8, RUNTIME_ABI 4/6,
// automatic-capture contract 5/6/7 and IEEE 1800-2023 6.8/15.5.
#include <array>
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
const s::Type event{s::TypeKind::Event, 0};
Operand block(unsigned n) { return Operand::block(s::BlockId{n}); }
Operand field(unsigned n) { return Operand::frame(s::FrameVarId{n}); }
struct Model {
    s::Design d;
    Model() {
        d.sources = {"vm_event.sv"};
        d.instances = {{"top", "top", s::kNone}};
        process();
    }
    unsigned process() {
        s::Process p;
        p.name = "top.p" + std::to_string(d.processes.size());
        p.loc = {0, 2, 3};
        d.processes.push_back(std::move(p));
        return static_cast<unsigned>(d.processes.size() - 1);
    }
    Operand slot(s::Type type, std::vector<s::ArrayDim> dims = {}) {
        s::Slot value;
        value.name = "top.v" + std::to_string(d.slots.size());
        value.type = type;
        value.dims = std::move(dims);
        value.flags = s::kObservable;
        const auto id = s::SlotId{static_cast<std::uint32_t>(d.slots.size())};
        d.slots.push_back(std::move(value));
        return Operand::slot(id);
    }
    Operand constant(s::Type type, std::uint64_t value) {
        const auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({type, {value}, {}});
        return Operand::constant(id);
    }
    Operand ticks(std::uint64_t value) { return constant(s::u2(64), value); }
    Operand null() { return constant(event, 0); }
    Operand emit(unsigned process, unsigned bb, Op op, std::vector<Operand> args = {},
                 s::Type type = s::voidType()) {
        auto& p = d.processes[process];
        if (p.blocks.size() <= bb) p.blocks.resize(bb + 1);
        const auto id = s::TempId{type == s::voidType() ? s::kNone : p.numTemps++};
        p.blocks[bb].instrs.push_back({op, type, id, std::move(args), {0, 7, 5}});
        return Operand::temp(id);
    }
    void store(unsigned p, unsigned b, const Operand& target, const Operand& value) {
        emit(p, b, target.kind == Operand::Kind::Frame ? Op::Fstore : Op::Store, {target, value});
    }
    Operand triggered(unsigned p, unsigned b, const Operand& value) {
        return emit(p, b, Op::Builtin, {Operand::str("triggered"), value}, s::u2(1));
    }
    void wait(unsigned p, unsigned b, std::uint64_t delay, unsigned next) {
        emit(p, b, Op::WaitDelay, {ticks(delay), block(next)});
    }
    ::testing::AssertionResult valid() {
        for (auto& p : d.processes)
            for (unsigned i = 0; i < d.slots.size(); ++i)
                p.writes.push_back({s::SlotId{i}});
        const auto errors = s::verify(d);
        if (errors.empty()) return ::testing::AssertionSuccess();
        std::string out;
        for (const auto& e : errors)
            out += e.where + ": " + e.message + "\n";
        return ::testing::AssertionFailure() << out;
    }
};
std::uint64_t word(const k::Context& ctx, const Operand& slot, unsigned element = 0) {
    return *ctx.store().element(k::SlotId{slot.id}, element);
}
std::uint64_t count(const k::Context& ctx, const char* name) {
    const auto& counters = ctx.counters();
    for (std::uint32_t i = 0; i < counters.size(); ++i)
        if (counters.name(k::CounterId{i}) == name) return counters.value(k::CounterId{i});
    return 0;
}
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file != nullptr) std::fclose(file);
    }
    std::string text() {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string out;
        std::array<char, 512> buffer{};
        while (const auto size = std::fread(buffer.data(), 1, buffer.size(), file))
            out.append(buffer.data(), size);
        return out;
    }
};

TEST(VmEvent, FreshSsaValuesAndLoadedLegacySelectionsUseTypedOperations) {
    Model m;
    const auto legacy = m.slot(event, {{3, 2}}), answer = m.slot(s::u2(1), {{0, 3}});
    const auto fresh = m.emit(0, 0, Op::EventNew, {}, event);
    const auto other = m.emit(0, 0, Op::EventNew, {}, event);
    m.emit(0, 0, Op::Trigger, {fresh});
    const auto selected = m.emit(0, 0, Op::LoadElem, {legacy, m.ticks(2)}, event);
    m.emit(0, 0, Op::Trigger, {selected});
    std::vector<Operand> values{fresh, other, selected, m.null()};
    for (unsigned i = 0; i < values.size(); ++i) {
        const auto on = m.triggered(0, 0, values[i]);
        m.emit(0, 0, Op::StoreElem, {answer, m.ticks(i), on});
    }
    m.emit(0, 0, Op::Trigger, {m.null()});
    m.emit(0, 0, Op::TriggerNb, {m.null(), m.ticks(UINT64_MAX)});
    m.emit(0, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    for (unsigned i = 0; i < 4; ++i)
        EXPECT_EQ(word(ctx, answer, i), i == 0 || i == 2 ? 1U : 0U);
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    EXPECT_EQ(ctx.managedEvents().allocatedObjects(), 2U);
}

TEST(VmEvent, TypedSlotsAndOrdinaryOrScopedArraysSurviveRegionCollectionAndRelease) {
    unsigned cases = 0;
    for (unsigned home = 0; home < 3; ++home)
        for (const auto& dims : {std::vector<s::ArrayDim>{}, std::vector<s::ArrayDim>{{3, 1}},
                                 std::vector<s::ArrayDim>{{-1, 1}}}) {
            SCOPED_TRACE(::testing::Message() << home << '/' << dims.size() << '/' << cases);
            Model m;
            const auto answer = m.slot(s::u2(1));
            Operand holder;
            if (home == 0)
                holder = m.slot(event, dims);
            else {
                s::FrameVar row{"event", event, s::kNone, dims};
                if (home == 2) {
                    row.home = s::FrameHome::Scoped;
                    row.activationScope = s::ActivationScopeId{0};
                    m.d.processes[0].activation.root = s::ActivationScopeId{0};
                    m.d.processes[0].activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
                }
                m.d.processes[0].frame.push_back(std::move(row));
                holder = field(0);
            }
            const auto index =
                m.ticks(dims.empty() ? 0 : static_cast<std::uint64_t>(dims[0].right));
            const auto fresh = m.emit(0, 0, Op::EventNew, {}, event);
            const auto put = [&](unsigned bb, const Operand& value) {
                if (dims.empty())
                    m.store(0, bb, holder, value);
                else
                    m.emit(0, bb, Op::StoreElem, {holder, index, value});
            };
            put(0, fresh);
            m.wait(0, 0, 1, 1);
            const auto read = dims.empty()
                                  ? m.emit(0, 1, home == 0 ? Op::Load : Op::Fload, {holder}, event)
                                  : m.emit(0, 1, Op::LoadElem, {holder, index}, event);
            m.emit(0, 1, Op::Trigger, {read});
            m.store(0, 1, answer, m.triggered(0, 1, read));
            put(1, m.null());
            m.wait(0, 1, 1, 2);
            m.emit(0, 2, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_EQ(word(ctx, answer), 1U);
            EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
            EXPECT_EQ(ctx.activations().stats().live, 0U);
            ++cases;
        }
    EXPECT_EQ(cases, 9U);
}

TEST(VmEvent, PendingTypedNbaAndNamedOrMixedWaitRetainTheSelectedObject) {
    for (const bool reactive : {false, true})
        for (const bool mixed : {false, true}) {
            Model m;
            if (reactive) m.d.instances[0].kind = s::InstanceKind::Program;
            const auto target = m.slot(event), signal = m.slot(s::u2(1));
            const auto answer = m.slot(s::u2(1));
            const auto waiter = m.process();
            const auto fresh = m.emit(0, 0, Op::EventNew, {}, event);
            m.store(0, 0, target, fresh);
            m.emit(0, 0, Op::TriggerNb, {fresh, m.ticks(2)});
            m.wait(0, 0, 1, 1);
            m.store(0, 1, target, m.null());
            m.emit(0, 1, Op::Ret);
            const auto selected = m.emit(waiter, 0, Op::Load, {target}, event);
            std::vector<Operand> items{selected};
            if (mixed) items.push_back(signal);
            items.push_back(block(1));
            m.emit(waiter, 0, mixed ? Op::WaitEvent : Op::WaitNamed, std::move(items));
            m.store(waiter, 1, answer, m.constant(s::u2(1), 1));
            m.emit(waiter, 1, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, reactive ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(word(ctx, answer), 1U);
            EXPECT_EQ(ctx.scheduler().time(), 2U);
            EXPECT_EQ(count(ctx, "sched.events.nba"), reactive ? 0U : 1U);
            EXPECT_EQ(count(ctx, "sched.events.re_nba"), reactive ? 1U : 0U);
            // Program completion suppresses later VM safe points (16.2).
            if (!reactive) EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
            ASSERT_EQ(ctx.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
        }
}

TEST(VmEvent, NullNamedAndMixedWaitsArePositionedDesignErrors) {
    for (const bool mixed : {false, true}) {
        Model m;
        std::vector<Operand> args{m.null()};
        if (mixed) args.push_back(m.slot(s::u2(1)));
        args.push_back(block(1));
        m.emit(0, 0, mixed ? Op::WaitEvent : Op::WaitNamed, std::move(args));
        m.emit(0, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context ctx({capture.file, capture.file});
        const auto result = kirimasim::vm::run(m.d, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Error) << result.error;
        EXPECT_EQ(capture.text(), "vm_event.sv:7: error: wait on a null event [top @ 0ns]\n");
    }
}

TEST(VmEvent, TriggeredMonitorsCaptureActualSelectedObjectsAndReplaceOldDependencies) {
    for (const bool dynamic : {false, true}) {
        Model m;
        const auto events = m.slot(event, {{0, 1}}), index = m.slot(s::u2(64));
        const auto answer = m.slot(s::u2(1));
        const auto waiter = m.process();
        const auto a = m.emit(0, 0, Op::EventNew, {}, event);
        const auto b = m.emit(0, 0, Op::EventNew, {}, event);
        m.emit(0, 0, Op::StoreElem, {events, m.ticks(0), a});
        m.emit(0, 0, Op::StoreElem, {events, m.ticks(1), b});
        m.wait(0, 0, 1, 1);
        if (dynamic) m.store(0, 1, index, m.ticks(1));
        auto first = events;
        first.numElems = 1;
        auto second = first;
        second.firstElem = 1;
        m.emit(0, 1, Op::Trigger, {first});
        m.emit(0, 1, Op::Trigger, {second});
        m.emit(0, 1, Op::Ret);
        s::Subroutine guard;
        guard.kind = s::SubroutineKind::Guard;
        guard.name = "top.guard";
        guard.frameOf = s::Owner::of(s::ProcessId{waiter});
        guard.returnType = s::u2(1);
        guard.loc = {0, 1, 1};
        std::vector<s::Instr> ops;
        if (dynamic) ops.push_back({Op::Load, s::u2(64), s::TempId{0}, {index}, {0, 1, 1}});
        const auto which = dynamic ? Operand::temp(s::TempId{0}) : m.ticks(1);
        ops.push_back({Op::LoadElem, event, s::TempId{1}, {events, which}, {0, 1, 1}});
        ops.push_back({Op::Builtin,
                       s::u2(1),
                       s::TempId{2},
                       {Operand::str("triggered"), Operand::temp(s::TempId{1})},
                       {0, 1, 1}});
        ops.push_back({Op::Ret, s::voidType(), s::kNone, {Operand::temp(s::TempId{2})}, {0, 1, 1}});
        guard.blocks.push_back({{}, std::move(ops)});
        guard.numTemps = 3;
        m.d.subroutines.push_back(std::move(guard));
        m.emit(waiter, 0, Op::WaitCond, {Operand::subroutine(s::SubroutineId{0}), block(1)});
        m.store(waiter, 1, answer, m.constant(s::u2(1), 1));
        m.emit(waiter, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(ctx, answer), 1U);
        EXPECT_EQ(count(ctx, "wait.monitor_evals"), dynamic ? 3U : 2U);
    }
}

// A trigger targets an event without reading its handle as a value
// (SCHEDULER_SPEC 9.4/9.5). Rebinding that target must not reevaluate a
// wait whose called function only triggers it. Hand-built SIR keeps the
// trigger's slot operand explicit, independently of front's temporaries.
TEST(VmEvent, TriggerOnlyCalleeTargetsAreNotMonitorReads) {
    for (const bool nonblocking : {false, true})
        for (const bool watch : {false, true}) {
            SCOPED_TRACE(::testing::Message() << nonblocking << '/' << watch);
            Model m;
            const auto target = m.slot(event);
            const auto enabled = m.slot(s::u2(1)), done = m.slot(s::u2(1));
            const auto waiter = m.process();
            m.store(0, 0, target, m.emit(0, 0, Op::EventNew, {}, event));
            m.wait(0, 0, 1, 1);
            m.store(0, 1, target, m.emit(0, 1, Op::EventNew, {}, event));
            m.wait(0, 1, 1, 2);
            m.store(0, 2, enabled, m.constant(s::u2(1), 1));
            m.emit(0, 2, Op::Ret);

            s::Subroutine fn;
            fn.name = "top.trigger_only";
            fn.returnType = s::u2(1);
            fn.numTemps = 1;
            fn.blocks.resize(1);
            fn.blocks[0].instrs = {
                {nonblocking ? Op::TriggerNb : Op::Trigger,
                 s::voidType(),
                 s::kNone,
                 {target},
                 {0, 1, 1}},
                {Op::Load, s::u2(1), s::TempId{0}, {enabled}, {0, 1, 1}},
                {Op::Ret, s::voidType(), s::kNone, {Operand::temp(s::TempId{0})}, {0, 1, 1}}};
            m.d.subroutines.push_back(std::move(fn));
            s::Subroutine guard;
            guard.name = "top.guard";
            guard.kind = watch ? s::SubroutineKind::Watch : s::SubroutineKind::Guard;
            guard.frameOf = s::Owner::of(s::ProcessId{waiter});
            guard.returnType = s::u2(1);
            guard.numTemps = 1;
            guard.blocks.resize(1);
            guard.blocks[0].instrs = {
                {Op::Call,
                 s::u2(1),
                 s::TempId{0},
                 {Operand::subroutine(s::SubroutineId{0})},
                 {0, 1, 1}},
                {Op::Ret, s::voidType(), s::kNone, {Operand::temp(s::TempId{0})}, {0, 1, 1}}};
            m.d.subroutines.push_back(std::move(guard));
            m.emit(waiter, 0, watch ? Op::WaitEvent : Op::WaitCond,
                   {Operand::subroutine(s::SubroutineId{1}), block(1)});
            m.store(waiter, 1, done, m.constant(s::u2(1), 1));
            m.emit(waiter, 1, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            EXPECT_EQ(word(ctx, done), 1U);
            EXPECT_EQ(ctx.scheduler().time(), 2U);
            EXPECT_EQ(count(ctx, "wait.monitor_evals"), 2U);
        }
}

TEST(VmEvent, BoundaryReuseNeedsNoContainerMutationAndRestoresThePreviousHook) {
    Model m;
    for (unsigned i = 0; i < 128; ++i) {
        m.emit(0, i, Op::EventNew, {}, event);
        m.wait(0, i, 1, i + 1);
    }
    m.emit(0, 128, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    unsigned visits = 0;
    const k::SafePointHook old{[](void* raw) noexcept { ++*static_cast<unsigned*>(raw); }, &visits};
    ctx.scheduler().setSafePointHook(old);
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(ctx.scheduler().time(), 128U);
    EXPECT_EQ(ctx.managedEvents().allocatedObjects(), 1U);
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    const auto restored = ctx.scheduler().setSafePointHook({});
    EXPECT_EQ(restored.fn, old.fn);
    EXPECT_EQ(restored.frame, old.frame);
}

TEST(VmEvent, NullDeferredTargetsIgnoreUnrepresentableAndRelativeOverflowDelays) {
    for (unsigned targetKind = 0; targetKind < 3; ++targetKind)
        for (const bool normalizedOverflow : {false, true}) {
            Model m;
            const auto target = m.slot(event);
            if (targetKind == 0) m.store(0, 0, target, m.null());
            if (targetKind == 1) m.store(0, 0, target, m.emit(0, 0, Op::EventNew, {}, event));
            m.wait(0, 0, 1, 1);
            const auto value = m.emit(0, 1, Op::Load, {target}, event);
            auto delay = m.ticks(UINT64_MAX);
            if (normalizedOverflow) {
                const auto id = s::ConstId{static_cast<std::uint32_t>(m.d.constants.size())};
                m.d.constants.push_back({s::u2(65), {0, 1}, {}});
                delay = Operand::constant(id);
            }
            m.emit(0, 1, Op::TriggerNb, {value, delay});
            m.emit(0, 1, Op::Ret);
            ASSERT_TRUE(m.valid());
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context ctx({capture.file, capture.file});
            const auto result = kirimasim::vm::run(m.d, ctx);
            EXPECT_EQ(result.reason,
                      targetKind == 0 ? k::EndReason::Quiescent : k::EndReason::Error)
                << result.error;
            EXPECT_EQ(ctx.scheduler().time(), 1U);
            EXPECT_EQ(capture.text(), targetKind == 0 ? ""
                                                      : "vm_event.sv:7: error: the delay takes the "
                                                        "time past the largest time [top @ 1ns]\n");
        }
}

TEST(VmEvent, QuiescentReturnDetachesTypedWaitsAndMonitorCallbacksBeforeFramesDie) {
    for (const bool managed : {false, true})
        for (unsigned kind = 0; kind < 3; ++kind) {
            Model m;
            const auto target = m.slot(event), answer = m.slot(s::u2(1));
            if (managed) m.store(0, 0, target, m.emit(0, 0, Op::EventNew, {}, event));
            m.emit(0, 0, Op::Ret);
            const auto waiter = m.process();
            if (kind < 2) {
                const auto value = m.emit(waiter, 0, Op::Load, {target}, event);
                m.emit(waiter, 0, kind == 0 ? Op::WaitNamed : Op::WaitEvent, {value, block(1)});
            } else {
                s::Subroutine guard;
                guard.name = "top.guard";
                guard.kind = s::SubroutineKind::Guard;
                guard.frameOf = s::Owner::of(s::ProcessId{waiter});
                guard.returnType = s::u2(1);
                guard.loc = {0, 1, 1};
                guard.numTemps = 2;
                guard.blocks = {{{},
                                 {{Op::Load, event, s::TempId{0}, {target}, {0, 1, 1}},
                                  {Op::Builtin,
                                   s::u2(1),
                                   s::TempId{1},
                                   {Operand::str("triggered"), Operand::temp(s::TempId{0})},
                                   {0, 1, 1}},
                                  {Op::Ret,
                                   s::voidType(),
                                   s::kNone,
                                   {Operand::temp(s::TempId{1})},
                                   {0, 1, 1}}}}};
                m.d.subroutines.push_back(std::move(guard));
                m.emit(waiter, 0, Op::WaitCond,
                       {Operand::subroutine(s::SubroutineId{0}), block(1)});
            }
            m.store(waiter, 1, answer, m.constant(s::u2(1), 1));
            m.emit(waiter, 1, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            const auto before = count(ctx, "wait.monitor_evals");
            ASSERT_EQ(ctx.scheduler().triggerValue({word(ctx, target)}),
                      k::ManagedEventError::None);
            ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(word(ctx, answer), 0U);
            EXPECT_EQ(count(ctx, "wait.monitor_evals"), before);
            ASSERT_EQ(ctx.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
        }
}

struct RootObserver {
    k::ProcessId process = k::kNone;
    bool waiting = false, seen = false;
    std::size_t live = 0;
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<RootObserver*>(frame);
        auto& ctx = k::fromC(raw);
        if (!self.waiting) {
            self.waiting = true;
            ctx.scheduler().waitDelay(self.process, 1);
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        self.seen = true;
        self.live = ctx.managedEvents().liveObjects();
        return static_cast<kirima_status>(k::Status::Done);
    }
};
TEST(VmEvent, AChildThatReadsOnlyAnotherFieldRetainsEveryUnreadEventElement) {
    Model m;
    const auto answer = m.slot(s::u2(64));
    s::FrameVar events{"events", event, s::kNone, {{-1, 1}}};
    events.home = s::FrameHome::Scoped;
    events.activationScope = s::ActivationScopeId{0};
    s::FrameVar value{"value", s::u2(64)};
    value.home = s::FrameHome::Scoped;
    value.activationScope = s::ActivationScopeId{0};
    m.d.processes[0].frame = {events, value};
    m.d.processes[0].activation.root = s::ActivationScopeId{0};
    m.d.processes[0].activation.scopes = {{s::kNone, {s::FrameVarId{0}, s::FrameVarId{1}}}};
    const auto child = m.process();
    auto& p = m.d.processes[child];
    p.kind = s::ProcessKind::General;
    p.parent = s::Owner::of(s::ProcessId{0});
    p.activation.extents = {s::ActivationScopeId{0}};
    value.home = s::FrameHome::Capture;
    value.activationScope = s::kNone;
    value.captureSource = s::FrameVarId{1};
    p.frame = {value};
    for (std::int64_t e = -1; e <= 1; ++e) {
        const auto fresh = m.emit(0, 0, Op::EventNew, {}, event);
        m.emit(0, 0, Op::StoreElem,
               {field(0), m.constant(s::s2(64), static_cast<std::uint64_t>(e)), fresh});
    }
    m.store(0, 0, field(1), m.ticks(19));
    m.emit(0, 0, Op::Fork,
           {Operand::process(s::ProcessId{child}), Operand::str("join_none"), block(1)});
    m.emit(0, 1, Op::Ret);
    m.wait(child, 0, 2, 1);
    m.store(child, 1, answer, m.emit(child, 1, Op::Fload, {field(0)}, s::u2(64)));
    m.emit(child, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    RootObserver observer;
    observer.process =
        ctx.scheduler().addProcess(&RootObserver::resume, &observer, k::Group::Active);
    ctx.scheduler().start(observer.process);
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    ASSERT_TRUE(observer.seen);
    EXPECT_EQ(observer.live, 3U);
    EXPECT_EQ(word(ctx, answer), 19U);
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    EXPECT_EQ(ctx.activations().stats().live, 0U);
}

TEST(VmEvent, InputCopiesAndFunctionReturnsShareTheObjectAcrossCallsAndWaits) {
    for (const bool scoped : {false, true}) {
        Model m;
        const auto answer = m.slot(s::u2(1));
        m.d.processes[0].frame = {{"saved", event}};
        s::Subroutine function;
        function.name = "top.identity";
        function.kind = s::SubroutineKind::Function;
        function.automatic = true;
        function.returnType = event;
        function.loc = {0, 2, 3};
        function.frame = {{"input", event}};
        if (scoped) {
            function.frame[0].home = s::FrameHome::Scoped;
            function.frame[0].activationScope = s::ActivationScopeId{0};
            function.activation.root = s::ActivationScopeId{0};
            function.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
        }
        function.params = {{"input", s::Direction::Input, event, s::kNone, s::FrameVarId{0}}};
        function.numTemps = 1;
        function.blocks = {
            {{},
             {{Op::Fload, event, s::TempId{0}, {field(0)}, {0, 3, 1}},
              {Op::Trigger, s::voidType(), s::kNone, {Operand::temp(s::TempId{0})}, {0, 3, 1}},
              {Op::Ret, s::voidType(), s::kNone, {Operand::temp(s::TempId{0})}, {0, 3, 1}}}}};
        m.d.subroutines.push_back(std::move(function));
        const auto fresh = m.emit(0, 0, Op::EventNew, {}, event);
        const auto returned =
            m.emit(0, 0, Op::Call, {Operand::subroutine(s::SubroutineId{0}), fresh}, event);
        m.store(0, 0, answer, m.triggered(0, 0, returned));
        m.store(0, 0, field(0), returned);
        m.wait(0, 0, 1, 1);
        const auto saved = m.emit(0, 1, Op::Fload, {field(0)}, event);
        const auto again =
            m.emit(0, 1, Op::Call, {Operand::subroutine(s::SubroutineId{0}), saved}, event);
        m.store(0, 1, answer, m.triggered(0, 1, again));
        m.emit(0, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(ctx, answer), 1U);
        EXPECT_EQ(ctx.managedEvents().allocatedObjects(), 1U);
        EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    }
}

TEST(VmEvent, EventReturnAndOutputCopybackKeepDistinctObjectsAndCapturedHomes) {
    for (const bool scoped : {false, true}) {
        Model m;
        const auto answer = m.slot(s::u2(1), {{0, 1}});
        m.d.processes[0].frame = {{"output", event}, {"returned", event}};
        s::Subroutine function;
        function.name = "top.make";
        function.kind = s::SubroutineKind::Function;
        function.automatic = true;
        function.returnType = event;
        function.loc = {0, 2, 3};
        function.frame = {{"output", event}};
        if (scoped) {
            function.frame[0].home = s::FrameHome::Scoped;
            function.frame[0].activationScope = s::ActivationScopeId{0};
            function.activation.root = s::ActivationScopeId{0};
            function.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
        }
        function.params = {{"output", s::Direction::Output, event, s::kNone, s::FrameVarId{0}}};
        function.numTemps = 2;
        function.blocks = {
            {{},
             {{Op::EventNew, event, s::TempId{0}, {}, {0, 3, 1}},
              {Op::Fstore,
               s::voidType(),
               s::kNone,
               {field(0), Operand::temp(s::TempId{0})},
               {0, 3, 1}},
              {Op::EventNew, event, s::TempId{1}, {}, {0, 3, 1}},
              {Op::Ret, s::voidType(), s::kNone, {Operand::temp(s::TempId{1})}, {0, 3, 1}}}}};
        m.d.subroutines.push_back(std::move(function));
        const auto returned =
            m.emit(0, 0, Op::Call, {Operand::subroutine(s::SubroutineId{0}), field(0)}, event);
        m.store(0, 0, field(1), returned);
        m.wait(0, 0, 1, 1);
        const auto output = m.emit(0, 1, Op::Fload, {field(0)}, event);
        const auto saved = m.emit(0, 1, Op::Fload, {field(1)}, event);
        m.emit(0, 1, Op::Trigger, {output});
        m.emit(0, 1, Op::StoreElem, {answer, m.ticks(0), m.triggered(0, 1, saved)});
        m.emit(0, 1, Op::Trigger, {saved});
        m.emit(0, 1, Op::StoreElem, {answer, m.ticks(1), m.triggered(0, 1, saved)});
        m.emit(0, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(word(ctx, answer, 0), 0U);
        EXPECT_EQ(word(ctx, answer, 1), 1U);
        EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
        EXPECT_EQ(ctx.activations().stats().live, 0U);
    }
}

TEST(VmEvent, WaitingTaskCopiesEveryEventArrayElementBackToItsCaller) {
    Model m;
    const auto answer = m.slot(s::u2(1), {{0, 2}});
    m.d.processes[0].frame = {{"output", event, s::kNone, {{-1, 1}}}};
    s::Subroutine task;
    task.name = "top.make_array";
    task.kind = s::SubroutineKind::Task;
    task.automatic = task.mayWait = true;
    task.returnType = s::voidType();
    task.loc = {0, 2, 3};
    task.frame = {{"output", event, s::kNone, {{-1, 1}}}};
    task.params = {{"output", s::Direction::Output, event, s::kNone, s::FrameVarId{0}}};
    task.numTemps = 3;
    task.blocks.resize(2);
    for (unsigned i = 0; i < 3; ++i) {
        const auto index = m.constant(s::s2(64), static_cast<std::uint64_t>(i) - 1);
        task.blocks[0].instrs.push_back({Op::EventNew, event, s::TempId{i}, {}, {0, 3, 1}});
        task.blocks[0].instrs.push_back({Op::StoreElem,
                                         s::voidType(),
                                         s::kNone,
                                         {field(0), index, Operand::temp(s::TempId{i})},
                                         {0, 3, 1}});
    }
    task.blocks[0].instrs.push_back(
        {Op::WaitDelay, s::voidType(), s::kNone, {m.ticks(1), block(1)}, {0, 3, 1}});
    task.blocks[1].instrs.push_back({Op::Ret, s::voidType(), s::kNone, {}, {0, 3, 1}});
    m.d.subroutines.push_back(std::move(task));
    m.emit(0, 0, Op::CallTask, {Operand::subroutine(s::SubroutineId{0}), field(0), block(1)});
    for (unsigned i = 0; i < 3; ++i) {
        const auto index = m.constant(s::s2(64), static_cast<std::uint64_t>(i) - 1);
        const auto value = m.emit(0, 1, Op::LoadElem, {field(0), index}, event);
        m.emit(0, 1, Op::Trigger, {value});
        m.emit(0, 1, Op::StoreElem, {answer, m.ticks(i), m.triggered(0, 1, value)});
    }
    m.emit(0, 1, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(ctx.scheduler().time(), 1U);
    for (unsigned i = 0; i < 3; ++i)
        EXPECT_EQ(word(ctx, answer, i), 1U);
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
}

TEST(VmEvent, SettledDeferredCallUsesCapturedEventAfterIssuerReturnsAndSourceIsReplaced) {
    Model m;
    const auto answer = m.slot(s::u2(1));
    m.d.processes[0].frame = {{"source", event}};
    s::Subroutine action;
    action.name = "top.action";
    action.kind = s::SubroutineKind::Function;
    action.automatic = true;
    action.returnType = s::voidType();
    action.loc = {0, 2, 3};
    action.frame = {{"input", event}};
    action.params = {{"input", s::Direction::Input, event, s::kNone, s::FrameVarId{0}}};
    action.numTemps = 2;
    action.blocks = {
        {{},
         {{Op::Fload, event, s::TempId{0}, {field(0)}, {0, 3, 1}},
          {Op::Trigger, s::voidType(), s::kNone, {Operand::temp(s::TempId{0})}, {0, 3, 1}},
          {Op::Builtin,
           s::u2(1),
           s::TempId{1},
           {Operand::str("triggered"), Operand::temp(s::TempId{0})},
           {0, 3, 1}},
          {Op::Store, s::voidType(), s::kNone, {answer, Operand::temp(s::TempId{1})}, {0, 3, 1}},
          {Op::Ret, s::voidType(), s::kNone, {}, {0, 3, 1}}}}};
    m.d.subroutines.push_back(std::move(action));
    const auto original = m.emit(0, 0, Op::EventNew, {}, event);
    m.store(0, 0, field(0), original);
    const auto captured = m.emit(0, 0, Op::Fload, {field(0)}, event);
    m.emit(0, 0, Op::ReportCall,
           {Operand::str("#0"), Operand::subroutine(s::SubroutineId{0}), captured});
    const auto replacement = m.emit(0, 0, Op::EventNew, {}, event);
    m.store(0, 0, field(0), replacement);
    m.emit(0, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(word(ctx, answer), 1U);
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
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
struct ArmTransferFailure {
    Memory* memory;
    k::SlotId arm;
    k::ProcessId process = k::kNone;
    std::vector<k::SlotId> outputs;
    std::vector<std::uint64_t> initial;
    unsigned visits = 0;
    static int guard(kirima_context*, void* frame) {
        auto& self = *static_cast<ArmTransferFailure*>(frame);
        ++self.visits;
        self.memory->fail = true;
        return 0;
    }
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<ArmTransferFailure*>(frame);
        auto& context = k::fromC(raw);
        for (const auto slot : self.outputs)
            self.initial.push_back(*context.store().element(slot, 0));
        const k::WaitItem item{{self.arm}, k::Edge::Any, &guard, &self};
        context.scheduler().waitEvent(self.process, {&item, 1});
        return static_cast<kirima_status>(k::Status::Waiting);
    }
};

TEST(VmEvent, TransferResourceFailuresStopBeforeCalleeEffectsOrPartialCopyback) {
    for (unsigned path = 0; path < 5; ++path) {
        SCOPED_TRACE(path);
        const bool output = path >= 3, waiting = path == 1 || path == 4;
        Model m;
        const auto armSlot = m.slot(s::u2(1)), answer = m.slot(s::u2(1));
        std::vector<Operand> targets;
        s::Subroutine sub;
        sub.name = "top.transfer";
        sub.kind = waiting ? s::SubroutineKind::Task : s::SubroutineKind::Function;
        sub.automatic = true;
        sub.mayWait = waiting;
        sub.returnType = s::voidType();
        sub.loc = {0, 2, 3};
        sub.blocks.resize(path == 4 ? 2 : 1);
        std::vector<Operand> args{Operand::subroutine(s::SubroutineId{0})};
        for (unsigned i = 0; i < 3; ++i) {
            sub.frame.push_back({"value" + std::to_string(i), event});
            sub.params.push_back({"value" + std::to_string(i),
                                  output ? s::Direction::Output : s::Direction::Input, event,
                                  s::kNone, s::FrameVarId{i}});
            if (output) {
                targets.push_back(m.slot(event));
                args.push_back(targets.back());
                sub.blocks[0].instrs.push_back({Op::EventNew, event, s::TempId{i}, {}, {0, 3, 1}});
                sub.blocks[0].instrs.push_back({Op::Fstore,
                                                s::voidType(),
                                                s::kNone,
                                                {field(i), Operand::temp(s::TempId{i})},
                                                {0, 3, 1}});
            } else {
                args.push_back(m.emit(0, 0, Op::EventNew, {}, event));
            }
        }
        sub.numTemps = output ? 3 : 0;
        if (output) {
            if (path == 4)
                sub.blocks[0].instrs.push_back(
                    {Op::WaitDelay, s::voidType(), s::kNone, {m.ticks(1), block(1)}, {0, 3, 1}});
            sub.blocks.back().instrs.push_back({Op::Store,
                                                s::voidType(),
                                                s::kNone,
                                                {armSlot, m.constant(s::u2(1), 1)},
                                                {0, 3, 1}});
        } else {
            sub.blocks[0].instrs.push_back(
                {Op::Store, s::voidType(), s::kNone, {answer, m.constant(s::u2(1), 1)}, {0, 3, 1}});
        }
        sub.blocks.back().instrs.push_back({Op::Ret, s::voidType(), s::kNone, {}, {0, 3, 1}});
        m.d.subroutines.push_back(std::move(sub));
        if (path == 2) {
            args.insert(args.begin(), Operand::str("#0"));
            m.emit(0, 0, Op::ReportCall, args);
        }
        if (!output) m.store(0, 0, armSlot, m.constant(s::u2(1), 1));
        if (path != 2) {
            if (waiting) args.push_back(block(1));
            m.emit(0, 0, waiting ? Op::CallTask : Op::Call, args);
        }
        const unsigned continuation = waiting ? 1 : 0;
        if (output) m.store(0, continuation, answer, m.constant(s::u2(1), 1));
        m.emit(0, continuation, Op::Ret);
        ASSERT_TRUE(m.valid());
        Memory memory;
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context ctx({capture.file, capture.file}, nullptr, &memory);
        ArmTransferFailure failure{&memory, k::SlotId{armSlot.id}};
        for (const auto& target : targets)
            failure.outputs.push_back(k::SlotId{target.id});
        failure.process =
            ctx.scheduler().addProcess(&ArmTransferFailure::resume, &failure, k::Group::Active);
        ctx.scheduler().start(failure.process);
        const auto result = kirimasim::vm::run(m.d, ctx);
        EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
        EXPECT_EQ(failure.visits, 1U);
        EXPECT_EQ(word(ctx, answer), 0U);
        ASSERT_EQ(failure.initial.size(), targets.size());
        for (unsigned i = 0; i < targets.size(); ++i)
            EXPECT_EQ(word(ctx, targets[i]), failure.initial[i]);
        EXPECT_EQ(capture.text(), "vm_event.sv:7: error: event resource limit [top @ " +
                                      std::string(path == 4 ? "1" : "0") + "ns]\n");
        ctx.scheduler().cancelWait(failure.process);
        memory.fail = false;
        ASSERT_EQ(ctx.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    }
}

TEST(VmEvent, EventCreationResourceFailureReturnsTheExistingLimitAndRestoresHooks) {
    Model m;
    m.emit(0, 0, Op::EventNew, {}, event);
    m.emit(0, 0, Op::Ret);
    ASSERT_TRUE(m.valid());
    Memory memory;
    Capture capture;
    ASSERT_NE(capture.file, nullptr);
    k::Context ctx({capture.file, capture.file}, nullptr, &memory);
    const k::SafePointHook old{[](void*) noexcept {}, &memory};
    ctx.scheduler().setSafePointHook(old);
    memory.fail = true;
    const auto result = kirimasim::vm::run(m.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::ResourceLimit) << result.error;
    EXPECT_EQ(ctx.managedEvents().liveObjects(), 0U);
    const auto restored = ctx.scheduler().setSafePointHook({});
    EXPECT_EQ(restored.fn, old.fn);
    EXPECT_EQ(restored.frame, old.frame);
}
}  // namespace
