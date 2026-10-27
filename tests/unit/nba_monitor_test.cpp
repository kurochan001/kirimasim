// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Approved event-NBA contract: persistent monitors share ordinary value rules.
// Independent kernel tests; no source event-NBA is claimed here.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "fuzz.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/valsem/logic.h"

namespace {
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
kirima_status done(kirima_context*, void*) { return KIRIMA_DONE; }
struct World {
    unsigned cleanups = 0;
    k::Context c;
    k::ProcessId origin;
    k::SlotId output;
    explicit World(k::Group group = k::Group::Active,
                   std::pmr::memory_resource* memory = std::pmr::new_delete_resource())
        : c({stdout, stderr}, nullptr, memory),
          origin(c.scheduler().addProcess(done, nullptr, group)),
          output(c.store().add(k::Domain::Two, 32, 1, k::Init::Zero)) {}
    ~World() { c.scheduler().cancelNbaEvents(); }
    static void release(void* raw) noexcept { ++static_cast<World*>(raw)->cleanups; }
    k::SlotId slot(unsigned width = 1, k::Domain domain = k::Domain::Two, unsigned elems = 1) {
        return c.store().add(domain, width, elems, k::Init::Zero);
    }
    k::NbaCapture capture(std::uint64_t value = 42) {
        const k::NbaWrite write{{.slot = output}, {&value, 1}};
        return c.scheduler().captureNba(origin, {&write, 1});
    }
    k::NbaEventRegistration arm(unsigned count, std::span<const k::NbaEventItem> items,
                                std::uint64_t value = 42) {
        const auto payload = capture(value);
        EXPECT_TRUE(payload);
        const k::W4 word{count, 0};
        return c.scheduler().registerNbaEvent(origin, payload.ticket, {32, false, true, {&word, 1}},
                                              items, {&release, this});
    }
    std::uint64_t value() { return c.store().element(output, 0)[0]; }
};
struct Copy {
    k::SlotId source;
    unsigned calls = 0, element = 0;
    std::vector<unsigned>* trace = nullptr;
    unsigned tag = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<Copy*>(data);
        ++f.calls;
        if (f.trace != nullptr) f.trace->push_back(f.tag);
        auto& c = k::fromC(raw);
        std::copy_n(c.store().element(f.source, f.element), c.store().layout(f.source).elemWords,
                    out);
    }
};
k::MonitorSpec monitor(Copy& f, k::Domain domain = k::Domain::Two, unsigned width = 1,
                       k::Edge edge = k::Edge::Any) {
    k::MonitorSpec result;
    result.slots = {{f.source}};
    result.eval = &Copy::eval;
    result.frame = &f;
    result.domain = domain;
    result.width = width;
    result.edge = edge;
    return result;
}
struct Guard {
    bool accept = true;
    unsigned calls = 0;
    std::vector<unsigned>* trace = nullptr;
    unsigned tag = 0;
    static int run(kirima_context*, void* data) {
        auto& f = *static_cast<Guard*>(data);
        ++f.calls;
        if (f.trace != nullptr) f.trace->push_back(f.tag);
        return f.accept;
    }
};
TEST(NbaMonitor, IssuerWaitCancellationAndKillDoNotRemoveTheMonitorOrCreateChildren) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        World w(group);
        auto& s = w.c.scheduler();
        const auto source = w.slot();
        Copy f{source};
        auto spec = monitor(f);
        s.waitMonitor(w.origin, spec);
        k::Limits limits;
        limits.maxProcs = 0;
        s.setLimits(limits);
        const std::array items{k::NbaEventItem{spec}};
        ASSERT_TRUE(w.arm(2, items));
        EXPECT_EQ(f.calls, 2U);  // each owner samples its own initial value
        s.cancelWait(w.origin);
        s.kill(w.origin);
        s.releaseProcess(w.origin);
        EXPECT_TRUE(s.hasCallbackOrigins(w.origin));
        const std::uint64_t one = 1, zero = 0;
        s.write({.slot = source}, &one);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        s.write({.slot = source}, &zero);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(f.calls, 4U);
        EXPECT_EQ(w.cleanups, 1U);
        EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
        EXPECT_EQ(s.waitListEntries(), 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), 42U);
        EXPECT_EQ(w.c.counters().value(w.c.counters().add("proc.created")), 1U);
    }
}
TEST(NbaMonitor, DirectAndMultipleExpressionItemsRunInOrderAndDecrementOnce) {
    for (bool monitorFirst : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        const auto source = w.slot();
        std::vector<unsigned> trace;
        Copy a{source, 0, 0, &trace, 2}, b{source, 0, 0, &trace, 4};
        Guard direct{true, 0, &trace, 1}, ga{true, 0, &trace, 3}, gb{false, 0, &trace, 5};
        auto ma = monitor(a), mb = monitor(b);
        ma.guard = &Guard::run;
        ma.guardFrame = &ga;
        mb.guard = &Guard::run;
        mb.guardFrame = &gb;
        const k::WaitItem slot{{source}, k::Edge::Any, &Guard::run, &direct};
        std::vector<k::NbaEventItem> items;
        if (monitorFirst)
            items = {ma, slot, mb};
        else
            items = {slot, ma, mb};
        ASSERT_TRUE(w.arm(2, items));
        EXPECT_EQ(trace, (std::vector<unsigned>{2, 4}));
        trace.clear();
        const std::uint64_t one = 1, zero = 0;
        s.write({.slot = source}, &one);
        const auto expected = monitorFirst ? std::vector<unsigned>{2, 3, 1, 4, 5}
                                           : std::vector<unsigned>{1, 2, 3, 4, 5};
        EXPECT_EQ(trace, expected);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        trace.clear();
        s.write({.slot = source}, &zero);
        EXPECT_EQ(trace, expected);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), 42U);
    }
}
TEST(NbaMonitor, FalseIffStillUpdatesPreviousValueAndUnchangedWritesDoNotReevaluate) {
    World w;
    auto& s = w.c.scheduler();
    const auto source = w.slot();
    Copy f{source};
    Guard guard{false};
    auto spec = monitor(f);
    spec.guard = &Guard::run;
    spec.guardFrame = &guard;
    const std::array items{k::NbaEventItem{spec}};
    ASSERT_TRUE(w.arm(2, items));
    const std::uint64_t one = 1, zero = 0;
    s.write({.slot = source}, &one);
    s.write({.slot = source}, &one);
    EXPECT_EQ(f.calls, 2U);
    EXPECT_EQ(guard.calls, 1U);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    guard.accept = true;
    s.write({.slot = source}, &zero);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.write({.slot = source}, &one);
    EXPECT_EQ(f.calls, 4U);
    EXPECT_EQ(guard.calls, 3U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
TEST(NbaMonitor, FourStateExpressionEdgesUseTheSharedReferenceForEveryTransition) {
    constexpr std::array values{v::Logic::Zero, v::Logic::One, v::Logic::Z, v::Logic::X};
    for (auto edge : {k::Edge::Any, k::Edge::Pos, k::Edge::Neg, k::Edge::Both})
        for (unsigned from = 0; from < 4; ++from)
            for (unsigned to = 0; to < 4; ++to) {
                World w;
                auto& s = w.c.scheduler();
                const auto source = w.slot(1, k::Domain::Four);
                const std::array<std::uint64_t, 2> before{from & 1U, from >> 1U},
                    after{to & 1U, to >> 1U};
                s.write({.slot = source}, before.data());
                Copy f{source};
                const std::array items{k::NbaEventItem{monitor(f, k::Domain::Four, 1, edge)}};
                ASSERT_TRUE(w.arm(1, items));
                s.write({.slot = source}, after.data());
                const bool pos = v::isPosedge(values[from], values[to]),
                           neg = v::isNegedge(values[from], values[to]);
                const bool hit = edge == k::Edge::Any   ? from != to
                                 : edge == k::Edge::Pos ? pos
                                 : edge == k::Edge::Neg ? neg
                                                        : pos || neg;
                EXPECT_EQ(s.nbaEventStats().live, hit ? 0U : 1U);
                EXPECT_EQ(s.run(), k::EndReason::Quiescent);
                EXPECT_EQ(w.value(), hit ? 42U : 0U);
            }
}
TEST(NbaMonitor, WideExpressionChangeIncludesEveryValueAndUnknownPlaneWord) {
    for (auto domain : {k::Domain::Two, k::Domain::Four})
        for (auto width : {65U, 129U}) {
            World w;
            auto& s = w.c.scheduler();
            const auto source = w.slot(width, domain);
            Copy f{source};
            const std::array items{k::NbaEventItem{monitor(f, domain, width)}};
            ASSERT_TRUE(w.arm(domain == k::Domain::Four ? 2 : 1, items));
            const std::array<std::uint64_t, 2> one{1, 0}, unknown{1, 1};
            s.write({.slot = source, .lsb = width - 1, .width = 1}, one.data());
            if (domain == k::Domain::Four) {
                EXPECT_EQ(s.nbaEventStats().live, 1U);
                s.write({.slot = source, .lsb = width - 1, .width = 1}, unknown.data());
            }
            EXPECT_EQ(s.nbaEventStats().live, 0U);
            EXPECT_EQ(s.run(), k::EndReason::Quiescent);
            EXPECT_EQ(w.value(), 42U);
        }
}

struct EventExpr {
    k::SlotId handle;
    unsigned calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<EventExpr*>(data);
        ++f.calls;
        auto& c = k::fromC(raw);
        const auto result = c.scheduler().triggeredValue({c.store().element(f.handle, 0)[0]});
        EXPECT_TRUE(result);
        out[0] = result.value;
    }
};
TEST(NbaMonitor, DynamicManagedEventReadsReattachAndReleaseThePreviousObject) {
    World w;
    auto& s = w.c.scheduler();
    auto first = w.c.managedEvents().create(), second = w.c.managedEvents().create();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto old = k::EventValue::managed(first.value.token()),
               next = k::EventValue::managed(second.value.token());
    const auto handle = w.slot(64, k::Domain::Handle);
    s.write({.slot = handle}, &old.bits);
    EventExpr f{handle};
    k::MonitorSpec spec;
    spec.slots = {{handle}};
    spec.eval = &EventExpr::eval;
    spec.frame = &f;
    spec.domain = k::Domain::Two;
    spec.edge = k::Edge::Pos;
    spec.captureEvents = true;
    const std::array items{k::NbaEventItem{spec}};
    ASSERT_TRUE(w.arm(2, items));
    first.value.reset();
    EXPECT_EQ(w.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_TRUE(w.c.managedEvents().valid({old.bits}));
    EXPECT_EQ(s.triggerValue(old), k::ManagedEventError::None);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.write({.slot = handle}, &next.bits);
    EXPECT_EQ(w.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_FALSE(w.c.managedEvents().valid({old.bits}));
    second.value.reset();
    EXPECT_EQ(w.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_TRUE(w.c.managedEvents().valid({next.bits}));
    const auto calls = f.calls;
    EXPECT_EQ(s.triggerValue(old), k::ManagedEventError::InvalidToken);
    EXPECT_EQ(f.calls, calls);
    EXPECT_EQ(s.triggerValue(next), k::ManagedEventError::None);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(w.c.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_FALSE(w.c.managedEvents().valid({next.bits}));
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
TEST(NbaMonitor, NamedAndTriggeredExpressionItemsShareOneNotification) {
    World w;
    auto& s = w.c.scheduler();
    const auto event = s.addEvent();
    const auto value = k::EventValue::legacy(event);
    const auto handle = w.slot(64, k::Domain::Handle);
    s.write({.slot = handle}, &value.bits);
    EventExpr f{handle};
    k::MonitorSpec spec;
    spec.slots = {{handle}};
    spec.eval = &EventExpr::eval;
    spec.frame = &f;
    spec.domain = k::Domain::Two;
    spec.captureEvents = true;
    const std::array<k::NbaEventItem, 2> items{k::NamedValueWaitItem{value}, spec};
    ASSERT_TRUE(w.arm(2, items));
    s.trigger(event);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    s.trigger(event);
    EXPECT_EQ(f.calls, 3U);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}

struct HeapExpr {
    k::HeapTypeId type;
    k::HeapLayoutId layout;
    k::ContainerToken container;
    std::int64_t index = 0;
    unsigned calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<HeapExpr*>(data);
        ++f.calls;
        auto& heap = k::fromC(raw).heap();
        const auto cell = heap.cell(f.type, f.container, f.index);
        ASSERT_TRUE(cell);
        k::W4 value{};
        ASSERT_TRUE(heap.readCell(f.layout, cell.value, {&value, 1}));
        out[0] = value.a;
        out[1] = value.b;
    }
};
struct Pair {
    k::HeapTypeId type;
    k::HeapLayoutId layout;
    k::ContainerToken container;
    explicit Pair(k::Context& c) {
        const std::array words{k::HeapWord{}};
        const auto l = c.heap().addLayout(words);
        EXPECT_TRUE(l);
        layout = l.value;
        const auto t = c.heap().addType({k::ContainerKind::Queue, layout, 0});
        EXPECT_TRUE(t);
        type = t.value;
        const std::array values{k::W4{10, 0}, k::W4{20, 0}};
        const auto value = c.heap().fromValues(type, values);
        EXPECT_TRUE(value);
        container = value.value;
    }
};
k::MonitorSpec heapMonitor(HeapExpr& f) {
    k::MonitorSpec spec;
    spec.eval = &HeapExpr::eval;
    spec.frame = &f;
    spec.domain = k::Domain::Four;
    spec.width = 64;
    spec.heap = true;
    return spec;
}
TEST(NbaMonitor, AtomicHeapSwapCoalescesAllRootsAndExpressionItems) {
    for (bool watchSecond : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        Pair pair(w.c);
        HeapExpr a{pair.type, pair.layout, pair.container, 0},
            b{pair.type, pair.layout, pair.container, 1};
        std::vector<k::NbaEventItem> items{heapMonitor(a)};
        if (watchSecond) items.push_back(heapMonitor(b));
        ASSERT_TRUE(w.arm(2, items));
        ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
        EXPECT_EQ(s.nbaEventStats().pendingNotices, 1U);
        s.flushHeapChanges();
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        EXPECT_EQ(a.calls, 2U);
        EXPECT_EQ(b.calls, watchSecond ? 2U : 0U);
        ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
        s.flushHeapChanges();
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.nbaEventStats().noticeRecords, 1U);
        EXPECT_EQ(s.nbaEventStats().pendingNotices, 0U);
        EXPECT_EQ(s.waitListEntries(), 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), 42U);
    }
}

TEST(NbaMonitor, NamedItemsDoNotReevaluateUnrelatedSlotOnlyExpressions) {
    World w;
    auto& s = w.c.scheduler();
    const auto event = s.addEvent();
    const auto source = w.slot();
    Copy f{source};
    const std::array<k::NbaEventItem, 2> items{k::NamedValueWaitItem{k::EventValue::legacy(event)},
                                               monitor(f)};
    ASSERT_TRUE(w.arm(2, items));
    s.trigger(event);
    EXPECT_EQ(f.calls, 1U);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    const std::uint64_t one = 1;
    s.write({.slot = source}, &one);
    EXPECT_EQ(f.calls, 2U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
struct InitialSlotWrite {
    k::SlotId source;
    unsigned calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<InitialSlotWrite*>(data);
        auto& c = k::fromC(raw);
        out[0] = c.store().element(f.source, 0)[0];
        if (++f.calls == 1) {
            const std::uint64_t one = 1;
            c.scheduler().write({.slot = f.source}, &one);
        }
    }
};
TEST(NbaMonitor, BothOwnersAttachStaticDependenciesBeforeInitialCallbacksWriteThem) {
    for (bool independent : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        InitialSlotWrite f{w.slot()};
        k::MonitorSpec spec;
        spec.slots = {{f.source}};
        spec.eval = &InitialSlotWrite::eval;
        spec.frame = &f;
        spec.domain = k::Domain::Two;
        if (independent) {
            const std::array items{k::NbaEventItem{spec}};
            ASSERT_TRUE(w.arm(1, items));
            EXPECT_EQ(s.nbaEventStats().live, 0U);
        } else
            s.waitMonitor(w.origin, spec);
        EXPECT_EQ(f.calls, 2U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        if (!independent) EXPECT_TRUE(s.finished(w.origin));
        EXPECT_EQ(w.value(), independent ? 42U : 0U);
    }
}
struct InitialHeapWrite {
    HeapExpr read;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<InitialHeapWrite*>(data);
        HeapExpr::eval(raw, &f.read, out);
        if (f.read.calls == 1) {
            auto& heap = k::fromC(raw).heap();
            const auto cell = heap.cell(f.read.type, f.read.container, 0);
            ASSERT_TRUE(cell);
            const k::W4 next{11, 0};
            ASSERT_TRUE(heap.writeCell(f.read.layout, cell.value, {&next, 1}));
        }
    }
};
TEST(NbaMonitor, BothOwnersObserveHeapWritesToReadsCapturedDuringInitialization) {
    for (bool independent : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        Pair pair(w.c);
        InitialHeapWrite f{{pair.type, pair.layout, pair.container, 0}};
        auto spec = heapMonitor(f.read);
        spec.eval = &InitialHeapWrite::eval;
        spec.frame = &f;
        if (independent) {
            const std::array items{k::NbaEventItem{spec}};
            ASSERT_TRUE(w.arm(1, items));
            EXPECT_EQ(s.nbaEventStats().live, 0U);
        } else
            s.waitMonitor(w.origin, spec);
        EXPECT_EQ(f.read.calls, 2U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        if (!independent) EXPECT_TRUE(s.finished(w.origin));
        EXPECT_EQ(w.value(), independent ? 42U : 0U);
    }
}
struct NestedRegistration {
    World* world;
    k::SlotId source;
    unsigned calls = 0;
    static void wideZero(kirima_context*, void*, std::uint64_t* out) { std::fill_n(out, 10, 0); }
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<NestedRegistration*>(data);
        ++f.calls;
        out[0] = k::fromC(raw).store().element(f.source, 0)[0];
        if (f.calls == 2) {
            for (unsigned i = 0; i < 64; ++i) {
                k::MonitorSpec child;
                child.eval = &wideZero;
                child.width = 257;
                const std::array items{k::NbaEventItem{child}};
                EXPECT_TRUE(f.world->arm(1, items, 1000 + i));
            }
        }
    }
};
TEST(NbaMonitor, NestedInitializationsCannotOverwriteTheCurrentEvaluationBuffer) {
    World w;
    auto& s = w.c.scheduler();
    NestedRegistration f{&w, w.slot()};
    k::MonitorSpec spec;
    spec.slots = {{f.source}};
    spec.eval = &NestedRegistration::eval;
    spec.frame = &f;
    spec.domain = k::Domain::Two;
    const std::array items{k::NbaEventItem{spec}};
    ASSERT_TRUE(w.arm(2, items));
    const std::uint64_t one = 1, zero = 0;
    s.write({.slot = f.source}, &one);
    EXPECT_EQ(s.nbaEventStats().live, 65U);
    s.write({.slot = f.source}, &zero);
    EXPECT_EQ(s.nbaEventStats().live, 64U);
    EXPECT_EQ(f.calls, 3U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
    EXPECT_EQ(w.cleanups, 65U);
}
struct OwnedState {
    World* world = nullptr;
    k::NbaEventTicket ticket;
    unsigned calls = 0, deleted = 0, bodyCompleted = 0;
    bool atInitialization = false;
};
struct OwnedFrame {
    OwnedState& state;
    static void release(void* raw) noexcept {
        auto* f = static_cast<OwnedFrame*>(raw);
        ++f->state.deleted;
        delete f;
    }
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<OwnedFrame*>(data);
        auto& state = f.state;
        auto& scheduler = k::fromC(raw).scheduler();
        ++state.calls;
        if ((state.atInitialization && state.calls == 1) ||
            (!state.atInitialization && state.calls == 2)) {
            if (state.atInitialization)
                scheduler.cancelNbaEvents();
            else
                EXPECT_EQ(scheduler.discardNbaEvent(state.ticket), k::NbaError::None);
            EXPECT_TRUE(scheduler.hasCallbackOrigins(state.world->origin));
            EXPECT_EQ(state.deleted, 0U);
            const auto before = scheduler.nbaStorageStats().live;
            EXPECT_EQ(before, 1U);
            ++state.bodyCompleted;  // read the context after cancellation, before returning
        }
        out[0] = state.calls;
    }
};
TEST(NbaMonitor, SelfCancellationKeepsTheMonitorContextAliveThroughCallbackReturn) {
    for (bool initialization : {false, true}) {
        OwnedState state;
        World w;
        state.world = &w;
        state.atInitialization = initialization;
        auto& s = w.c.scheduler();
        const auto source = w.slot();
        auto* frame = new OwnedFrame{state};
        k::MonitorSpec spec;
        spec.slots = {{source}};
        spec.eval = &OwnedFrame::eval;
        spec.frame = frame;
        spec.domain = k::Domain::Two;
        const std::array items{k::NbaEventItem{spec}};
        const auto payload = w.capture();
        ASSERT_TRUE(payload);
        const k::W4 one{1, 0};
        const auto result =
            s.registerNbaEvent(w.origin, payload.ticket, {1, false, true, {&one, 1}}, items,
                               {&OwnedFrame::release, frame});
        ASSERT_TRUE(result);
        state.ticket = result.ticket;
        if (!initialization) {
            const std::uint64_t changed = 1;
            s.write({.slot = source}, &changed);
        }
        EXPECT_EQ(state.bodyCompleted, 1U);
        EXPECT_EQ(state.deleted, 1U);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.waitListEntries(), 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), 0U);
    }
}
struct DynamicHeap {
    k::SlotId handle, index;
    k::HeapTypeId type;
    k::HeapLayoutId layout;
    unsigned calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<DynamicHeap*>(data);
        auto& c = k::fromC(raw);
        ++f.calls;
        const k::ContainerToken token{c.store().element(f.handle, 0)[0]};
        const auto cell = c.heap().cell(f.type, token, c.store().element(f.index, 0)[0]);
        ASSERT_TRUE(cell);
        k::W4 value{};
        ASSERT_TRUE(c.heap().readCell(f.layout, cell.value, {&value, 1}));
        out[0] = value.a;
        out[1] = value.b;
    }
};
TEST(NbaMonitor, HeapReadsReattachOnIndexAndHandleReplacementEvenWithoutAValueChange) {
    World w;
    auto& s = w.c.scheduler();
    Pair pair(w.c);
    DynamicHeap f{w.slot(64, k::Domain::Handle), w.slot(32), pair.type, pair.layout};
    s.write({.slot = f.handle}, &pair.container.bits);
    k::MonitorSpec spec;
    spec.slots = {{f.handle}, {f.index}};
    spec.eval = &DynamicHeap::eval;
    spec.frame = &f;
    spec.domain = k::Domain::Four;
    spec.width = 64;
    spec.heap = true;
    const std::array items{k::NbaEventItem{spec}};
    ASSERT_TRUE(w.arm(4, items));
    const std::uint64_t nextIndex = 1;
    s.write({.slot = f.index}, &nextIndex);  // 10 -> 20
    EXPECT_EQ(f.calls, 2U);
    k::W4 changed{99, 0};
    ASSERT_TRUE(w.c.heap().write(pair.type, pair.container, 0, {&changed, 1}));
    s.flushHeapChanges();
    EXPECT_EQ(f.calls, 2U);
    changed.a = 21;
    ASSERT_TRUE(w.c.heap().write(pair.type, pair.container, 1, {&changed, 1}));
    s.flushHeapChanges();
    EXPECT_EQ(f.calls, 3U);
    auto copy = w.c.heap().copy(pair.type, pair.container);
    ASSERT_TRUE(copy);
    s.write({.slot = f.handle}, &copy.value.bits);  // 21 -> 21, but dependencies change
    EXPECT_EQ(f.calls, 4U);
    changed.a = 22;
    ASSERT_TRUE(w.c.heap().write(pair.type, pair.container, 1, {&changed, 1}));
    s.flushHeapChanges();
    EXPECT_EQ(f.calls, 4U);
    for (auto n : {23U, 24U}) {
        changed.a = n;
        ASSERT_TRUE(w.c.heap().write(pair.type, copy.value, 1, {&changed, 1}));
        s.flushHeapChanges();
    }
    EXPECT_EQ(f.calls, 6U);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
struct ActivationExpr {
    k::ActivationBinding binding;
    unsigned lsb = 0, width = 65, calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<ActivationExpr*>(data);
        ++f.calls;
        std::array<k::W4, 2> words{};
        ASSERT_EQ(k::fromC(raw).activations().read(
                      f.binding, 0, {words.data(), (f.width + 63) / 64}, f.lsb, f.width),
                  k::ActivationError::None);
        for (std::size_t i = 0; i < (f.width + 63) / 64; ++i) {
            out[2 * i] = words[i].a;
            out[2 * i + 1] = words[i].b;
        }
    }
    k::MonitorSpec spec(bool capture) {
        k::MonitorSpec out;
        out.eval = &eval;
        out.frame = this;
        out.width = width;
        out.activation.capture = capture;
        out.activation.bindings = {binding};
        if (!capture) out.activation.parts = {{binding, 0, 1, 0, 65}};
        return out;
    }
};
TEST(NbaMonitor, ActivationNotificationsCoalesceItemsAndRetainTypedExtentsUntilCompletion) {
    for (bool capture : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        const std::array fields{k::ActivationField{k::ActivationKind::Four, 65, {}}};
        const auto layout = w.c.activations().addLayout(fields);
        ASSERT_TRUE(layout);
        auto owner = w.c.activations().create(layout.value);
        ASSERT_TRUE(owner);
        const auto binding = w.c.activations().bind(owner.value, 0);
        ASSERT_TRUE(binding);
        ActivationExpr a{binding.value}, b{binding.value, 64, 1};
        const std::array<k::NbaEventItem, 2> items{a.spec(capture), b.spec(capture)};
        ASSERT_TRUE(w.arm(2, items));
        owner.value.reset();
        EXPECT_EQ(w.c.activations().stats().live, 1U);
        const std::array one{k::W4{1, 0}, k::W4{1, 0}}, zero{k::W4{0, 0}, k::W4{0, 0}};
        ASSERT_EQ(s.writeActivation(binding.value, 0, one, 0, 65), k::ActivationError::None);
        EXPECT_EQ(a.calls, 2U);
        EXPECT_EQ(b.calls, 2U);
        EXPECT_EQ(s.nbaEventStats().live, 1U);
        ASSERT_EQ(s.writeActivation(binding.value, 0, zero, 0, 65), k::ActivationError::None);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(w.c.activations().stats().live, 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), 42U);
    }
}
class Memory final : public std::pmr::memory_resource {
public:
    std::optional<unsigned> remaining;
    std::size_t liveBytes = 0;

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        if (remaining) {
            if (*remaining == 0) throw std::bad_alloc{};
            --*remaining;
        }
        auto* p = std::pmr::new_delete_resource()->allocate(size, alignment);
        liveBytes += size;
        return p;
    }
    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override {
        liveBytes -= size;
        std::pmr::new_delete_resource()->deallocate(p, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(NbaMonitor, AllocationFailuresDistinguishRejectedOwnershipFromAcceptedRuntimeFailure) {
    unsigned pre = 0, post = 0, success = 0;
    for (unsigned failAfter = 0; failAfter < 32; ++failAfter) {
        Memory memory;
        {
            World w(k::Group::Active, &memory);
            auto& s = w.c.scheduler();
            Pair pair(w.c);
            HeapExpr frame{pair.type, pair.layout, pair.container};
            const std::array items{k::NbaEventItem{heapMonitor(frame)}};
            const auto payload = w.capture();
            ASSERT_TRUE(payload);
            const k::W4 one{1, 0};
            memory.remaining = failAfter;
            const auto result =
                s.registerNbaEvent(w.origin, payload.ticket, {1, false, true, {&one, 1}}, items,
                                   {&World::release, &w});
            memory.remaining.reset();
            if (!result) {
                ++pre;
                EXPECT_EQ(result.error, k::NbaError::ResourceLimit);
                EXPECT_EQ(w.cleanups, 0U);
                EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
                EXPECT_EQ(s.discardNba(payload.ticket), k::NbaError::None);
            } else if (s.reason() == k::EndReason::ResourceLimit) {
                ++post;
                EXPECT_EQ(w.cleanups, 1U);
                EXPECT_EQ(s.nbaEventStats().live, 0U);
                EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
                EXPECT_EQ(s.waitListEntries(), 0U);
            } else {
                ++success;
                EXPECT_EQ(s.discardNbaEvent(result.ticket), k::NbaError::None);
                EXPECT_EQ(w.cleanups, 1U);
            }
            EXPECT_EQ(w.value(), 0U);
            const auto old = w.c.heap().setChangeHook({});
            EXPECT_EQ(old.fn, nullptr);
        }
        EXPECT_EQ(memory.liveBytes, 0U);
    }
    EXPECT_GT(pre, 0U);
    EXPECT_GT(post, 0U);
    EXPECT_GT(success, 0U);
}
TEST(NbaMonitor, FailureQueuingACommittedHeapChangeEndsWithoutPublishingTheNBA) {
    Memory memory;
    World w(k::Group::Active, &memory);
    auto& s = w.c.scheduler();
    Pair pair(w.c);
    HeapExpr frame{pair.type, pair.layout, pair.container};
    const std::array items{k::NbaEventItem{heapMonitor(frame)}};
    ASSERT_TRUE(w.arm(1, items));
    memory.remaining = 0;
    ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
    memory.remaining.reset();
    EXPECT_EQ(s.reason(), k::EndReason::ResourceLimit);
    EXPECT_EQ(s.run(), k::EndReason::ResourceLimit);
    EXPECT_EQ(w.value(), 0U);
    EXPECT_EQ(w.cleanups, 1U);
    EXPECT_EQ(s.nbaEventStats().pendingNotices, 0U);
    EXPECT_EQ(s.waitListEntries(), 0U);
}
TEST(NbaMonitor, CompletedMonitorAndNoticeRowsAreReusedWithoutStaleDependencies) {
    World w;
    auto& s = w.c.scheduler();
    Pair pair(w.c);
    HeapExpr frame{pair.type, pair.layout, pair.container};
    const std::array items{k::NbaEventItem{heapMonitor(frame)}};
    for (unsigned i = 0; i < 200; ++i) {
        ASSERT_TRUE(w.arm(1, items, i + 1));
        ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
        s.flushHeapChanges();
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_EQ(s.nbaEventStats().pendingNotices, 0U);
        EXPECT_EQ(s.nbaEventStats().noticeRecords, 1U);
        EXPECT_EQ(s.waitListEntries(), 0U);
        EXPECT_EQ(s.run(), k::EndReason::Quiescent);
        EXPECT_EQ(w.value(), i + 1);
    }
    EXPECT_EQ(w.cleanups, 200U);
}
TEST(NbaMonitor, SkippedCountsDoNotValidateOrEvaluateMonitorDescriptors) {
    World w;
    auto& s = w.c.scheduler();
    k::MonitorSpec invalid;
    invalid.slots = {{k::SlotId{k::kNone}}};
    const std::array items{k::NbaEventItem{invalid}};
    ASSERT_TRUE(w.arm(0, items));
    EXPECT_EQ(w.cleanups, 1U);
    EXPECT_EQ(s.nbaEventStats().records, 0U);
    EXPECT_EQ(s.waitListEntries(), 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
struct ReenterNotice {
    World* world;
    Pair* pair;
    HeapExpr replacement;
    bool entered = false;
    static int guard(kirima_context*, void* raw) {
        auto& f = *static_cast<ReenterNotice*>(raw);
        auto& s = f.world->c.scheduler();
        if (s.nbaEventStats().pendingNotices != 0) {
            ADD_FAILURE() << "notice row must be released before invoking callbacks";
            return 0;
        }
        EXPECT_FALSE(f.entered);
        f.entered = true;
        s.cancelNbaEvents();
        const std::array items{k::NbaEventItem{heapMonitor(f.replacement)}};
        EXPECT_TRUE(f.world->arm(1, items, 71));
        EXPECT_TRUE(f.world->c.heap().swapValues(f.pair->type, f.pair->container, 0, 1));
        EXPECT_EQ(s.nbaEventStats().pendingNotices, 1U);
        EXPECT_EQ(s.nbaEventStats().noticeRecords, 1U);
        return 0;
    }
};
TEST(NbaMonitor, NoticeCallbacksCanCancelAllAndReuseTheCurrentNoticeRow) {
    World w;
    auto& s = w.c.scheduler();
    Pair pair(w.c);
    ReenterNotice frame{&w, &pair, {pair.type, pair.layout, pair.container}};
    HeapExpr old{pair.type, pair.layout, pair.container};
    auto first = heapMonitor(old);
    first.guard = &ReenterNotice::guard;
    first.guardFrame = &frame;
    const std::array items{k::NbaEventItem{first}};
    ASSERT_TRUE(w.arm(1, items, 41));
    Guard stale;
    auto second = heapMonitor(old);
    second.guard = &Guard::run;
    second.guardFrame = &stale;
    const std::array next{k::NbaEventItem{second}};
    ASSERT_TRUE(w.arm(1, next, 42));
    ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
    s.flushHeapChanges();
    EXPECT_TRUE(frame.entered);
    EXPECT_EQ(stale.calls, 0U);
    EXPECT_EQ(w.cleanups, 3U);
    EXPECT_EQ(s.nbaEventStats().pendingNotices, 0U);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 71U);
}
struct ReleaseInitialPin {
    k::ManagedEventPin* pin;
    k::ManagedEventToken token;
    unsigned calls = 0;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<ReleaseInitialPin*>(data);
        ++f.calls;
        auto& c = k::fromC(raw);
        f.pin->reset();
        EXPECT_EQ(c.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_TRUE(c.managedEvents().valid(f.token));
        out[0] = 0;
    }
    static void zero(kirima_context*, void*, std::uint64_t* out) { out[0] = 0; }
};
TEST(NbaMonitor, BothOwnersRetainEveryStaticEventBeforeTheFirstInitialization) {
    for (bool independent : {false, true}) {
        World w;
        auto& s = w.c.scheduler();
        auto event = w.c.managedEvents().create();
        ASSERT_TRUE(event);
        const auto token = event.value.token();
        ReleaseInitialPin frame{&event.value, token};
        k::MonitorSpec first;
        first.domain = k::Domain::Two;
        first.eval = &ReleaseInitialPin::eval;
        first.frame = &frame;
        k::MonitorSpec second;
        second.domain = k::Domain::Two;
        second.eval = &ReleaseInitialPin::zero;
        second.eventValues = {k::EventValue::managed(token)};
        if (independent) {
            const std::array items{k::NbaEventItem{first}, k::NbaEventItem{second}};
            ASSERT_TRUE(w.arm(1, items));
            s.cancelNbaEvents();
        } else {
            const std::array monitors{first, second};
            s.waitAny(w.origin, {}, monitors);
            s.cancelWait(w.origin);
        }
        EXPECT_EQ(frame.calls, 1U);
        EXPECT_EQ(w.c.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_FALSE(w.c.managedEvents().valid(token));
    }
}
struct XorRead {
    k::SlotId x, y;
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        const auto& f = *static_cast<XorRead*>(data);
        const auto& store = k::fromC(raw).store();
        out[0] = store.element(f.x, 0)[0] ^ store.element(f.y, 0)[0];
    }
};
TEST(NbaMonitor, RandomizedMixedItemsMatchAnIndependentNotificationModel) {
    const auto seed = kirimasim::test::fuzzSeed(0x8a5bd21cU);
    SCOPED_TRACE(kirimasim::test::fuzzSetting(seed));
    std::mt19937_64 random(seed);
    for (auto group : {k::Group::Active, k::Group::Reactive})
        for (std::uint64_t round = 0; round < 20 * kirimasim::test::fuzzScale(); ++round) {
            World w(group);
            auto& s = w.c.scheduler();
            Pair pair(w.c);
            const auto x = w.slot(), y = w.slot();
            const auto event = s.addEvent();
            constexpr unsigned controls = 12, itemCount = 6;
            std::array<std::array<Guard, itemCount>, controls> guards;
            std::array<std::array<unsigned, itemCount>, controls> guardCalls{};
            std::array<Copy, controls> copies;
            std::array<XorRead, controls> expressions;
            std::array<std::array<HeapExpr, 2>, controls> heaps;
            std::array<unsigned, controls> remaining;
            std::array<bool, controls> ready{};
            for (unsigned i = 0; i < controls; ++i) {
                copies[i].source = x;
                expressions[i] = {x, y};
                heaps[i] = {HeapExpr{pair.type, pair.layout, pair.container, 0},
                            HeapExpr{pair.type, pair.layout, pair.container, 1}};
                auto copy = monitor(copies[i]);
                copy.edge = i % 3 == 0 ? k::Edge::Any : i % 3 == 1 ? k::Edge::Pos : k::Edge::Neg;
                copy.guard = &Guard::run;
                copy.guardFrame = &guards[i][1];
                auto compound = copy;
                compound.edge = k::Edge::Any;
                compound.slots = {{x}, {y}};
                compound.eval = &XorRead::eval;
                compound.frame = &expressions[i];
                compound.guardFrame = &guards[i][3];
                auto a = heapMonitor(heaps[i][0]), b = heapMonitor(heaps[i][1]);
                a.guard = b.guard = &Guard::run;
                a.guardFrame = &guards[i][4];
                b.guardFrame = &guards[i][5];
                const std::array<k::NbaEventItem, itemCount> items{
                    k::WaitItem{{x}, k::Edge::Any, &Guard::run, &guards[i][0]},
                    copy,
                    k::NamedValueWaitItem{k::EventValue::legacy(event), &Guard::run, &guards[i][2]},
                    compound,
                    a,
                    b};
                remaining[i] = 1 + random() % 9;
                ASSERT_TRUE(w.arm(remaining[i], items, 1000 + i));
            }
            std::uint64_t vx = 0, vy = 0;
            std::array<std::uint64_t, 2> values{10, 20};
            for (unsigned notice = 0; notice < 80; ++notice) {
                const auto oldX = vx, oldY = vy;
                const auto oldValues = values;
                const unsigned operation = random() % 5, element = random() % 2;
                const std::uint64_t nextValue = random() % 23;
                if (operation == 0) vx = nextValue % 2;
                if (operation == 1) vy = nextValue % 2;
                if (operation == 3) std::swap(values[0], values[1]);
                if (operation == 4) values[element] = nextValue;
                unsigned live = 0;
                for (unsigned i = 0; i < controls; ++i) {
                    for (auto& guard : guards[i])
                        guard.accept = (random() & 1U) != 0;
                    const bool edge = i % 3 == 0 || (i % 3 == 1 ? vx == 1 : vx == 0);
                    const std::array<bool, itemCount> changed{oldX != vx,
                                                              oldX != vx && edge,
                                                              operation == 2,
                                                              (oldX ^ oldY) != (vx ^ vy),
                                                              oldValues[0] != values[0],
                                                              oldValues[1] != values[1]};
                    bool accepted = false;
                    if (remaining[i] != 0) {
                        for (unsigned item = 0; item < itemCount; ++item)
                            if (changed[item]) {
                                ++guardCalls[i][item];
                                accepted = guards[i][item].accept || accepted;
                            }
                        if (accepted && --remaining[i] == 0) ready[i] = true;
                    }
                    live += remaining[i] != 0;
                }
                if (operation == 0) s.write({.slot = x}, &vx);
                if (operation == 1) s.write({.slot = y}, &vy);
                if (operation == 2) s.trigger(event);
                if (operation == 3)
                    ASSERT_TRUE(w.c.heap().swapValues(pair.type, pair.container, 0, 1));
                if (operation == 4) {
                    const k::W4 word{nextValue, 0};
                    ASSERT_TRUE(w.c.heap().write(pair.type, pair.container, element, {&word, 1}));
                }
                s.flushHeapChanges();
                EXPECT_EQ(s.nbaEventStats().live, live);
                for (unsigned i = 0; i < controls; ++i)
                    for (unsigned item = 0; item < itemCount; ++item)
                        EXPECT_EQ(guards[i][item].calls, guardCalls[i][item]);
            }
            std::uint64_t expected = 0;
            for (unsigned i = 0; i < controls; ++i)
                if (ready[i]) expected = 1000 + i;
            EXPECT_EQ(s.run(), k::EndReason::Quiescent);
            EXPECT_EQ(w.value(), expected);
            EXPECT_EQ(w.cleanups, controls);
            EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
            EXPECT_EQ(s.waitListEntries(), 0U);
        }
}
TEST(NbaMonitor, RealExpressionsUseOneWordWithZeroIntegralWidth) {
    World w;
    auto& s = w.c.scheduler();
    Copy frame{w.slot(64, k::Domain::Real)};
    Guard guard;
    guard.accept = false;
    auto spec = monitor(frame, k::Domain::Real, 0);
    spec.guard = &Guard::run;
    spec.guardFrame = &guard;
    const std::array items{k::NbaEventItem{spec}};
    ASSERT_TRUE(w.arm(1, items));
    const auto first = std::bit_cast<std::uint64_t>(2.5),
               second = std::bit_cast<std::uint64_t>(6.25);
    s.write({.slot = frame.source}, &first);
    EXPECT_EQ(guard.calls, 1U);
    EXPECT_EQ(s.nbaEventStats().live, 1U);
    guard.accept = true;
    s.write({.slot = frame.source}, &first);
    EXPECT_EQ(guard.calls, 1U);
    s.write({.slot = frame.source}, &second);
    EXPECT_EQ(guard.calls, 2U);
    EXPECT_EQ(s.nbaEventStats().live, 0U);
    EXPECT_EQ(s.run(), k::EndReason::Quiescent);
    EXPECT_EQ(w.value(), 42U);
}
TEST(NbaMonitor, InvalidDescriptorsRollBackBeforeTransferringCleanupOwnership) {
    for (unsigned variant = 0; variant < 6; ++variant) {
        World w;
        auto& s = w.c.scheduler();
        Copy frame{w.slot()};
        auto spec = monitor(frame);
        if (variant == 0) spec.eval = nullptr;
        if (variant == 1) spec.untilTrue = true;
        if (variant == 2) spec.domain = k::Domain::Handle;
        if (variant == 3) spec.width = 0;
        if (variant == 4) spec.slots = {{k::SlotId{k::kNone}}};
        if (variant == 5) spec.events = {k::EventId{k::kNone}};
        const auto capture = w.capture();
        ASSERT_TRUE(capture);
        const k::W4 one{1, 0};
        const std::array items{k::NbaEventItem{spec}};
        const auto result = s.registerNbaEvent(
            w.origin, capture.ticket, {1, false, true, {&one, 1}}, items, {&World::release, &w});
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error,
                  variant == 4 ? k::NbaError::InvalidTarget : k::NbaError::InvalidValue);
        EXPECT_EQ(frame.calls, 0U);
        EXPECT_EQ(w.cleanups, 0U);
        EXPECT_EQ(s.nbaEventStats().live, 0U);
        EXPECT_FALSE(s.hasCallbackOrigins(w.origin));
        EXPECT_EQ(s.waitListEntries(), 0U);
        EXPECT_EQ(s.discardNba(capture.ticket), k::NbaError::None);
    }
}
struct ComputedContainer {
    World* world;
    HeapExpr input;
    bool nested;
    unsigned calls = 0;
    k::ContainerToken temporary;
    static void child(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<ComputedContainer*>(data);
        const k::W4 initial{3, 0};
        const auto made = k::fromC(raw).heap().fromValues(f.input.type, {&initial, 1});
        ASSERT_TRUE(made);
        f.temporary = made.value;
        out[0] = 0;
    }
    static void eval(kirima_context* raw, void* data, std::uint64_t* out) {
        auto& f = *static_cast<ComputedContainer*>(data);
        auto& c = k::fromC(raw);
        if (++f.calls > 5) {
            // Bound the detector even if work-object writes cause a loop.
            c.scheduler().cancelNbaEvents();
            c.scheduler().cancelWait(f.world->origin);
            out[0] = out[1] = 0;
            return;
        }
        if (f.nested) {
            k::MonitorSpec childSpec;
            childSpec.domain = k::Domain::Two;
            childSpec.eval = &child;
            childSpec.frame = &f;
            const std::array items{k::NbaEventItem{childSpec}};
            ASSERT_TRUE(f.world->arm(100, items));
        } else {
            std::uint64_t ignored = 0;
            child(raw, data, &ignored);
        }
        ASSERT_EQ(c.heap().size(f.input.type, f.temporary).value, 1U);
        const k::W4 appended{5, 0};
        ASSERT_TRUE(c.heap().push(f.input.type, f.temporary, false, {&appended, 1}));
        HeapExpr::eval(raw, &f.input, out);
    }
};
TEST(NbaMonitor, BothOwnersIgnoreWorkObjectsFromTheirOwnAndNestedEvaluations) {
    for (bool independent : {false, true})
        for (bool nested : {false, true}) {
            World w;
            auto& s = w.c.scheduler();
            Pair pair(w.c);
            ComputedContainer frame{&w, {pair.type, pair.layout, pair.container}, nested};
            auto spec = heapMonitor(frame.input);
            spec.eval = &ComputedContainer::eval;
            spec.frame = &frame;
            if (independent) {
                const std::array items{k::NbaEventItem{spec}};
                ASSERT_TRUE(w.arm(1, items));
            } else
                s.waitMonitor(w.origin, spec);
            EXPECT_EQ(frame.calls, 1U);
            const k::W4 next{11, 0};
            ASSERT_TRUE(w.c.heap().write(pair.type, pair.container, 0, {&next, 1}));
            s.flushHeapChanges();
            EXPECT_EQ(frame.calls, 2U);
            EXPECT_EQ(s.run(), k::EndReason::Quiescent);
            EXPECT_EQ(w.value(), independent ? 42U : 0U);
        }
}
struct AllocationObserver {
    k::Heap* heap;
    k::HeapTypeId type;
    k::HeapLayoutId layout;
    unsigned containers = 0, cells = 0;
    static void allocated(void* raw, k::HeapRoot root) noexcept {
        auto& f = *static_cast<AllocationObserver*>(raw);
        if (root.kind == k::HeapRoot::Kind::Container) {
            ++f.containers;
            EXPECT_TRUE(f.heap->size(f.type, {root.bits}));
        } else {
            ++f.cells;
            k::W4 value{};
            EXPECT_TRUE(f.heap->readCell(f.layout, {root.bits}, {&value, 1}));
        }
    }
};
TEST(NbaMonitor, AllocationObserversSeeCommittedObjectsAndNoRolledBackTokens) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 24; ++budget) {
        Memory memory;
        {
            World w(k::Group::Active, &memory);
            Pair pair(w.c);
            AllocationObserver observer{&w.c.heap(), pair.type, pair.layout};
            const auto previous =
                w.c.heap().setAllocationHook({&AllocationObserver::allocated, &observer});
            memory.remaining = budget;
            const std::array values{k::W4{3, 0}, k::W4{5, 0}};
            const auto made = w.c.heap().fromValues(pair.type, values);
            memory.remaining.reset();
            w.c.heap().setAllocationHook(previous);
            if (made) {
                ++successes;
                EXPECT_EQ(observer.containers, 1U);
                EXPECT_EQ(observer.cells, 2U);
            } else {
                ++failures;
                EXPECT_EQ(made.status.error, k::HeapError::ResourceLimit);
                EXPECT_EQ(observer.containers, 0U);
                EXPECT_EQ(observer.cells, 0U);
            }
        }
        EXPECT_EQ(memory.liveBytes, 0U);
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}
struct CancelIndependent {
    static int guard(kirima_context* raw, void*) {
        k::fromC(raw).scheduler().cancelNbaEvents();
        return 1;
    }
};
TEST(NbaMonitor, OrdinaryCallbacksCannotSkipOtherWaitersWhenRemovingIndependentEntries) {
    for (auto group : {k::Group::Active, k::Group::Reactive})
        for (bool named : {false, true}) {
            World w(group);
            auto& s = w.c.scheduler();
            const auto source = w.slot();
            const auto event = s.addEvent();
            Copy independent{source}, first{source}, second{source};
            const auto make = [&](Copy& f) {
                auto spec = monitor(f);
                if (named) {
                    spec.slots.clear();
                    spec.events = {event};
                }
                return spec;
            };
            const std::array items{k::NbaEventItem{make(independent)}};
            ASSERT_TRUE(w.arm(100, items));
            const auto a = s.addProcess(done, nullptr, group);
            const auto b = s.addProcess(done, nullptr, group);
            auto firstSpec = make(first);
            firstSpec.guard = &CancelIndependent::guard;
            s.waitMonitor(a, firstSpec);
            s.waitMonitor(b, make(second));
            const std::uint64_t one = 1;
            if (named) {
                w.c.store().element(source, 0)[0] = one;
                s.trigger(event);
            } else
                s.write({.slot = source}, &one);
            EXPECT_EQ(first.calls, 2U);
            EXPECT_EQ(second.calls, 2U);
            EXPECT_EQ(s.nbaEventStats().live, 0U);
            EXPECT_EQ(s.run(), k::EndReason::Quiescent);
            EXPECT_TRUE(s.finished(a));
            EXPECT_TRUE(s.finished(b));
            EXPECT_EQ(w.value(), 0U);
        }
}
}  // namespace
