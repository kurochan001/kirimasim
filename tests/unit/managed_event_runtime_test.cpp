// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original operation/pin matrix for automatic-capture contract 5/6/8.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using Error = k::ManagedEventError;
using Value = k::EventValue;

Value value(const k::ManagedEventPin& pin) { return Value::managed(pin.token()); }
struct Receiver {
    unsigned wakes = 0;
    std::uint64_t lastTime = 0;
    k::ProcessId process = k::kNone;
    static kirima_status resume(kirima_context* c, void* raw) {
        auto& receiver = *static_cast<Receiver*>(raw);
        ++receiver.wakes;
        receiver.lastTime = k::fromC(c).scheduler().time();
        return static_cast<kirima_status>(k::Status::Done);
    }
    void add(k::Context& context, k::Group group = k::Group::Active) {
        process = context.scheduler().addProcess(resume, this, group);
    }
};
std::uint64_t count(const k::Counters& counters, const char* name) {
    for (std::uint32_t i = 0; i < counters.size(); ++i)
        if (counters.name(k::CounterId{i}) == name) return counters.value(k::CounterId{i});
    return 0;
}

TEST(ManagedEventRuntime, UnifiedValuesRejectStaleAndUnversionedManagedAliases) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    const auto legacy = scheduler.addEvent();
    EXPECT_EQ(scheduler.resolveEventValue(Value::legacy(legacy)).value, legacy);
    auto owned = events.create();
    ASSERT_TRUE(owned);
    const auto saved = value(owned.value);
    const auto physical = events.resolve(owned.value).value;
    EXPECT_EQ(scheduler.resolveEventValue(saved).value, physical);
    EXPECT_EQ(scheduler.resolveEventValue(Value::legacy(physical)).error, Error::InvalidToken);
    EXPECT_EQ(scheduler.resolveEventValue({UINT64_MAX}).error, Error::InvalidToken);
    EXPECT_EQ(scheduler.resolveEventValue({}).error, Error::Null);
    EXPECT_EQ(scheduler.triggerValue({}), Error::None);
    EXPECT_EQ(scheduler.triggerNbValue(k::kNone, {}, UINT64_MAX), Error::None);
    const auto null = scheduler.triggeredValue({});
    ASSERT_TRUE(null);
    EXPECT_FALSE(null.value);
    owned.value.reset();
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(scheduler.triggerValue(saved), Error::InvalidToken);
    EXPECT_EQ(scheduler.triggeredValue(saved).error, Error::InvalidToken);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(scheduler.time(), 0U);
}

TEST(ManagedEventRuntime, NamedWaitRetainsTheEvaluatedObjectRatherThanTheCurrentVariable) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context context;
        auto& scheduler = context.scheduler();
        auto& events = context.managedEvents();
        auto first = events.create(), second = events.create();
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        const auto original = value(first.value), replacement = value(second.value);
        Value variable = original;
        Receiver receiver;
        receiver.add(context, group);
        ASSERT_EQ(scheduler.waitNamedValue(receiver.process, variable), Error::None);
        variable = replacement;
        first.value.reset();
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_TRUE(events.valid({original.bits}));
        ASSERT_EQ(scheduler.triggerValue(variable), Error::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(receiver.wakes, 0U);
        ASSERT_EQ(scheduler.triggerValue(original), Error::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(receiver.wakes, 1U);
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_FALSE(events.valid({original.bits}));
    }
}

TEST(ManagedEventRuntime, InvalidNamedOrMonitorValuesDoNotReplaceAnExistingWait) {
    for (int kind = 0; kind < 3; ++kind) {
        k::Context context;
        auto& scheduler = context.scheduler();
        auto& events = context.managedEvents();
        auto owned = events.create();
        ASSERT_TRUE(owned);
        const auto saved = value(owned.value);
        Receiver receiver;
        receiver.add(context);
        ASSERT_EQ(scheduler.waitNamedValue(receiver.process, saved), Error::None);
        owned.value.reset();
        if (kind < 2) {
            const auto input = kind == 0 ? Value{} : Value{UINT64_MAX};
            EXPECT_EQ(scheduler.waitNamedValue(receiver.process, input),
                      kind == 0 ? Error::Null : Error::InvalidToken);
        } else {
            k::MonitorSpec monitor;
            monitor.eventValues = {{UINT64_MAX}};
            monitor.eval = [](kirima_context*, void*, std::uint64_t* out) { out[0] = 0; };
            EXPECT_EQ(scheduler.waitAnyValues(receiver.process, {}, {&monitor, 1}, {}),
                      Error::InvalidToken);
        }
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_TRUE(events.valid({saved.bits}));
        ASSERT_EQ(scheduler.triggerValue(saved), Error::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(receiver.wakes, 1U);
    }
}

TEST(ManagedEventRuntime, MixedSignalAndNamedItemsShareOneWaitAndKeepPerItemIff) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        for (bool signalWins : {false, true}) {
            k::Context context;
            auto& scheduler = context.scheduler();
            auto& events = context.managedEvents();
            const auto signal = context.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
            auto a = events.create(), b = events.create();
            ASSERT_TRUE(a);
            ASSERT_TRUE(b);
            const auto av = value(a.value), bv = value(b.value);
            Receiver receiver;
            receiver.add(context, group);
            bool permit = false;
            const std::array named{
                k::NamedValueWaitItem{
                    av, [](kirima_context*, void* raw) { return *static_cast<bool*>(raw) ? 1 : 0; },
                    &permit},
                k::NamedValueWaitItem{bv}};
            const k::WaitItem item{{signal, 0, 1, 0, 1}, k::Edge::Pos};
            ASSERT_EQ(scheduler.waitAnyValues(receiver.process, {&item, 1}, {}, named),
                      Error::None);
            a.value.reset();
            b.value.reset();
            ASSERT_EQ(events.collect(), Error::None);
            ASSERT_EQ(scheduler.triggerValue(av), Error::None);
            EXPECT_EQ(receiver.wakes, 0U);
            if (signalWins) {
                const std::uint64_t one = 1;
                scheduler.write({signal, 0, 0, 1}, &one);
            } else {
                ASSERT_EQ(scheduler.triggerValue(bv), Error::None);
            }
            ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
            EXPECT_EQ(receiver.wakes, 1U);
            ASSERT_EQ(events.collect(), Error::None);
            EXPECT_EQ(events.liveObjects(), 0U);
        }
    }
}

TEST(ManagedEventRuntime, DeferredTargetsSurviveIssuerReturnAndProcessRecordReuseInBothGroups) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        for (auto delay : {std::uint64_t{0}, std::uint64_t{5}, UINT64_MAX}) {
            k::Context context;
            auto& scheduler = context.scheduler();
            auto& events = context.managedEvents();
            struct Frame {
                k::ManagedEventPin owner;
                Value target;
                k::ProcessId issuer = k::kNone, recycler = k::kNone;
                std::uint64_t delay = 0;
                k::Group opposite = k::Group::Active;
                unsigned phase = 0;
                bool scheduled = false, held = false, reused = false;
            } frame;
            auto owned = events.create();
            ASSERT_TRUE(owned);
            frame.target = value(owned.value);
            frame.owner = std::move(owned.value);
            frame.delay = delay;
            frame.opposite = group == k::Group::Active ? k::Group::Reactive : k::Group::Active;
            Receiver receiver;
            receiver.add(context);
            scheduler.waitNamed(receiver.process, scheduler.resolveEventValue(frame.target).value);
            frame.issuer = scheduler.addProcess(
                [](kirima_context* c, void* raw) {
                    auto& f = *static_cast<Frame*>(raw);
                    auto& context = k::fromC(c);
                    f.scheduled = context.scheduler().triggerNbValue(f.issuer, f.target, f.delay) ==
                                  Error::None;
                    f.owner.reset();
                    f.held = context.managedEvents().collect() == Error::None &&
                             context.managedEvents().valid({f.target.bits});
                    return static_cast<kirima_status>(k::Status::Done);
                },
                &frame, group);
            scheduler.start(frame.issuer);
            if (delay != 0) {
                frame.recycler = scheduler.addProcess(
                    [](kirima_context* c, void* raw) {
                        auto& f = *static_cast<Frame*>(raw);
                        auto& context = k::fromC(c);
                        auto& scheduler = context.scheduler();
                        if (f.phase++ == 0) {
                            scheduler.waitDelay(f.recycler, 1);
                            return static_cast<kirima_status>(k::Status::Waiting);
                        }
                        scheduler.releaseProcess(f.issuer);
                        const auto replacement =
                            scheduler.addProcess(Receiver::resume, nullptr, f.opposite);
                        f.reused = replacement == f.issuer &&
                                   context.managedEvents().valid({f.target.bits});
                        return static_cast<kirima_status>(k::Status::Done);
                    },
                    &frame, k::Group::Active);
                scheduler.start(frame.recycler);
            }
            ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
            EXPECT_TRUE(frame.scheduled);
            EXPECT_TRUE(frame.held);
            if (delay != 0) EXPECT_TRUE(frame.reused);
            EXPECT_EQ(receiver.wakes, 1U);
            EXPECT_EQ(scheduler.time(), delay);
            EXPECT_EQ(receiver.lastTime, delay);
            EXPECT_EQ(count(context.counters(),
                            group == k::Group::Active ? "sched.events.nba" : "sched.events.re_nba"),
                      1U);
            EXPECT_EQ(count(context.counters(),
                            group == k::Group::Active ? "sched.events.re_nba" : "sched.events.nba"),
                      0U);
            ASSERT_EQ(events.collect(), Error::None);
            EXPECT_EQ(events.liveObjects(), 0U);
        }
    }
}

TEST(ManagedEventRuntime, CancellationAndPendingRowReuseRejectOldQueuedSerials) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    Receiver issuer, receiver;
    issuer.add(context);
    receiver.add(context);
    auto first = events.create();
    ASSERT_TRUE(first);
    const auto stale = value(first.value);
    ASSERT_EQ(scheduler.triggerNbValue(issuer.process, stale, 1), Error::None);
    first.value.reset();
    scheduler.cancelManagedEventOperations();
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_FALSE(events.valid({stale.bits}));
    auto second = events.create();
    ASSERT_TRUE(second);
    const auto current = value(second.value);
    ASSERT_EQ(scheduler.waitNamedValue(receiver.process, current), Error::None);
    ASSERT_EQ(scheduler.triggerNbValue(issuer.process, current, 3), Error::None);
    second.value.reset();
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 1U);
    EXPECT_EQ(scheduler.time(), 3U);
    EXPECT_EQ(receiver.lastTime, 3U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, LateTriggerOwnsItsObjectUntilTheNotificationHasFinished) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    struct Frame {
        k::ManagedEventPin owner;
        Value target;
        k::EventId physical = k::kNone;
        bool queued = false, held = false, deferred = false;
    } frame;
    auto owned = events.create();
    ASSERT_TRUE(owned);
    frame.target = value(owned.value);
    frame.physical = events.resolve(owned.value).value;
    frame.owner = std::move(owned.value);
    Receiver waiter, monitor;
    waiter.add(context);
    monitor.add(context);
    scheduler.waitNamed(waiter.process, frame.physical);
    k::MonitorSpec spec;
    spec.domain = k::Domain::Two;
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& context = k::fromC(c);
        f.queued = context.scheduler().triggerValue(f.target) == Error::None;
        f.deferred = !context.scheduler().triggered(f.physical);
        f.owner.reset();
        f.held = context.managedEvents().collect() == Error::None &&
                 context.managedEvents().valid({f.target.bits});
        out[0] = 0;
    };
    scheduler.waitMonitor(monitor.process, spec);
    EXPECT_TRUE(frame.queued);
    EXPECT_TRUE(frame.held);
    EXPECT_TRUE(frame.deferred);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(waiter.wakes, 1U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, NamedGuardMayCancelItselfCollectAndGrowBothTables) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    auto owner = events.create();
    ASSERT_TRUE(owner);
    struct Frame {
        Value target;
        k::ProcessId process;
        bool valid = false;
    } frame{value(owner.value), k::kNone};
    Receiver receiver;
    receiver.add(context);
    frame.process = receiver.process;
    const k::NamedValueWaitItem item{
        frame.target,
        [](kirima_context* c, void* raw) {
            auto& f = *static_cast<Frame*>(raw);
            auto& context = k::fromC(c);
            context.scheduler().cancelWait(f.process);
            std::vector<k::ManagedEventPin> temporary;
            for (int i = 0; i < 64; ++i) {
                auto event = context.managedEvents().create();
                if (!event) return 0;
                temporary.push_back(std::move(event.value));
                context.scheduler().addProcess(Receiver::resume, nullptr, k::Group::Active);
            }
            f.valid = context.managedEvents().collect() == Error::None &&
                      context.managedEvents().valid({f.target.bits});
            return 1;
        },
        &frame};
    ASSERT_EQ(scheduler.waitAnyValues(receiver.process, {}, {}, {&item, 1}), Error::None);
    owner.value.reset();
    ASSERT_EQ(scheduler.triggerValue(frame.target), Error::None);
    EXPECT_TRUE(frame.valid);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 0U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

struct Watch {
    std::array<Value, 2> values;
    k::SlotId selected = k::kNone;
    unsigned evaluations = 0;
    bool valid = true;
    static void eval(kirima_context* c, void* raw, std::uint64_t* out) {
        auto& watch = *static_cast<Watch*>(raw);
        auto& context = k::fromC(c);
        ++watch.evaluations;
        const auto index = *context.store().element(watch.selected, 0);
        const auto result = context.scheduler().triggeredValue(watch.values[index]);
        watch.valid = watch.valid && static_cast<bool>(result);
        out[0] = result.value ? 1 : 0;
    }
};
TEST(ManagedEventRuntime, DynamicEventReadsReattachWithoutReevaluatingOnTheOldObject) {
    for (int domains = 0; domains < 4; ++domains) {
        k::Context context;
        auto& scheduler = context.scheduler();
        auto& events = context.managedEvents();
        Watch watch;
        std::array<k::ManagedEventPin, 2> owners;
        for (unsigned i = 0; i < 2; ++i) {
            if ((domains & (1 << i)) != 0) {
                watch.values[i] = Value::legacy(scheduler.addEvent());
            } else {
                auto created = events.create();
                ASSERT_TRUE(created);
                watch.values[i] = value(created.value);
                owners[i] = std::move(created.value);
            }
        }
        watch.selected = context.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        Receiver receiver;
        receiver.add(context);
        k::MonitorSpec spec;
        spec.captureEvents = true;
        spec.slots = {{watch.selected, 0, 1, 0, 1}};
        spec.domain = k::Domain::Two;
        spec.untilTrue = true;
        spec.eval = Watch::eval;
        spec.frame = &watch;
        scheduler.waitMonitor(receiver.process, spec);
        EXPECT_EQ(watch.evaluations, 1U);
        const std::uint64_t one = 1;
        scheduler.write({watch.selected, 0, 0, 1}, &one);
        EXPECT_EQ(watch.evaluations, 2U);
        ASSERT_EQ(scheduler.triggerValue(watch.values[0]), Error::None);
        EXPECT_EQ(watch.evaluations, 2U);
        owners[0].reset();
        owners[1].reset();
        ASSERT_EQ(events.collect(), Error::None);
        if ((domains & 1) == 0) EXPECT_FALSE(events.valid({watch.values[0].bits}));
        if ((domains & 2) == 0) EXPECT_TRUE(events.valid({watch.values[1].bits}));
        ASSERT_EQ(scheduler.triggerValue(watch.values[1]), Error::None);
        EXPECT_EQ(watch.evaluations, 3U);
        EXPECT_TRUE(watch.valid);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(receiver.wakes, 1U);
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_EQ(events.liveObjects(), 0U);
    }
}

TEST(ManagedEventRuntime, StaticValueDependenciesDeduplicateAndPinWithoutExecutedReads) {
    k::Context context;
    auto& events = context.managedEvents();
    auto& scheduler = context.scheduler();
    auto owner = events.create();
    ASSERT_TRUE(owner);
    struct Frame {
        k::EventId event;
        unsigned calls = 0;
    } frame{events.resolve(owner.value).value};
    const auto saved = value(owner.value);
    Receiver receiver;
    receiver.add(context);
    k::MonitorSpec spec;
    spec.domain = k::Domain::Two;
    spec.untilTrue = true;
    spec.eventValues = {saved, saved};
    spec.events = {frame.event};
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        ++f.calls;
        out[0] = k::fromC(c).scheduler().triggered(f.event) ? 1 : 0;
    };
    scheduler.waitMonitor(receiver.process, spec);
    owner.value.reset();
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_TRUE(events.valid({saved.bits}));
    ASSERT_EQ(scheduler.triggerValue(saved), Error::None);
    EXPECT_EQ(frame.calls, 2U);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 1U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, SelfCancellationKeepsOldDependenciesPinnedThroughTheCallback) {
    k::Context context;
    auto& events = context.managedEvents();
    auto& scheduler = context.scheduler();
    auto first = events.create(), second = events.create();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    struct Frame {
        Value a, b;
        k::ProcessId process;
        unsigned calls = 0;
        bool held = false;
    } frame{value(first.value), value(second.value), k::kNone};
    Receiver receiver;
    receiver.add(context);
    frame.process = receiver.process;
    k::MonitorSpec spec;
    spec.captureEvents = true;
    spec.domain = k::Domain::Two;
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& context = k::fromC(c);
        if (f.calls++ != 0) {
            context.scheduler().cancelWait(f.process);
            f.held = context.managedEvents().collect() == Error::None &&
                     context.managedEvents().valid({f.b.bits});
        }
        const auto a = context.scheduler().triggeredValue(f.a);
        const auto b = context.scheduler().triggeredValue(f.b);
        f.held = f.calls == 1 ? (a && b) : f.held && a && b;
        out[0] = 0;
    };
    scheduler.waitMonitor(receiver.process, spec);
    first.value.reset();
    second.value.reset();
    ASSERT_EQ(scheduler.triggerValue(frame.a), Error::None);
    EXPECT_TRUE(frame.held);
    EXPECT_EQ(frame.calls, 2U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 0U);
}

TEST(ManagedEventRuntime, CancelDuringInitialEvaluationCannotPublishANewRegistration) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    struct Frame {
        k::ManagedEventPin owner;
        Value target;
        bool held = false;
    } frame;
    auto created = events.create();
    ASSERT_TRUE(created);
    frame.target = value(created.value);
    frame.owner = std::move(created.value);
    Receiver receiver;
    receiver.add(context);
    k::MonitorSpec spec;
    spec.captureEvents = true;
    spec.domain = k::Domain::Two;
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& context = k::fromC(c);
        const auto read = context.scheduler().triggeredValue(f.target);
        f.owner.reset();
        context.scheduler().cancelManagedEventOperations();
        f.held = read && context.managedEvents().collect() == Error::None &&
                 context.managedEvents().valid({f.target.bits});
        out[0] = 0;
    };
    scheduler.waitMonitor(receiver.process, spec);
    EXPECT_TRUE(frame.held);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_FALSE(events.valid({frame.target.bits}));
    EXPECT_EQ(scheduler.triggerValue(frame.target), Error::InvalidToken);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 0U);
}

TEST(ManagedEventRuntime, InitialCallbackReplacingAWaitKeepsTheNewBindingAndPins) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    struct Frame {
        k::ManagedEventPin oldOwner, newOwner;
        Value oldValue, newValue;
        k::ProcessId process = k::kNone;
        bool replaced = false;
    } frame;
    auto old = events.create(), next = events.create();
    ASSERT_TRUE(old);
    ASSERT_TRUE(next);
    frame.oldValue = value(old.value);
    frame.newValue = value(next.value);
    frame.oldOwner = std::move(old.value);
    frame.newOwner = std::move(next.value);
    Receiver receiver;
    receiver.add(context);
    frame.process = receiver.process;
    const k::NamedValueWaitItem named{frame.oldValue};
    k::MonitorSpec spec;
    spec.domain = k::Domain::Two;
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& scheduler = k::fromC(c).scheduler();
        scheduler.cancelWait(f.process);
        f.replaced = scheduler.waitNamedValue(f.process, f.newValue) == Error::None;
        f.oldOwner.reset();
        f.newOwner.reset();
        out[0] = 0;
    };
    ASSERT_EQ(scheduler.waitAnyValues(receiver.process, {}, {&spec, 1}, {&named, 1}), Error::None);
    EXPECT_TRUE(frame.replaced);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_FALSE(events.valid({frame.oldValue.bits}));
    EXPECT_TRUE(events.valid({frame.newValue.bits}));
    ASSERT_EQ(scheduler.triggerValue(frame.newValue), Error::None);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(receiver.wakes, 1U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, ActualTerminationReleasesPendingWaitAndMonitorPins) {
    for (auto reason : {k::EndReason::Finish, k::EndReason::Error, k::EndReason::ResourceLimit,
                        k::EndReason::Interrupted, k::EndReason::TimeLimit, k::EndReason::Fatal,
                        k::EndReason::ZeroTimeLimit, k::EndReason::WallTimeLimit,
                        k::EndReason::Stop, k::EndReason::Internal}) {
        k::Context context;
        auto& events = context.managedEvents();
        auto& scheduler = context.scheduler();
        auto named = events.create(), watched = events.create(), pending = events.create();
        ASSERT_TRUE(named);
        ASSERT_TRUE(watched);
        ASSERT_TRUE(pending);
        Receiver a, b, issuer;
        a.add(context);
        b.add(context);
        issuer.add(context);
        ASSERT_EQ(scheduler.waitNamedValue(a.process, value(named.value)), Error::None);
        k::MonitorSpec spec;
        spec.eventValues = {value(watched.value)};
        spec.domain = k::Domain::Two;
        spec.eval = [](kirima_context*, void*, std::uint64_t* out) { out[0] = 0; };
        scheduler.waitMonitor(b.process, spec);
        ASSERT_EQ(scheduler.triggerNbValue(issuer.process, value(pending.value), 7), Error::None);
        named.value.reset();
        watched.value.reset();
        pending.value.reset();
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_EQ(events.liveObjects(), 3U);
        volatile std::sig_atomic_t interrupt = 1;
        if (reason == k::EndReason::Interrupted)
            scheduler.setInterruptFlag(&interrupt);
        else
            scheduler.requestEnd(reason, 0);
        EXPECT_EQ(scheduler.run(), reason);
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_EQ(events.liveObjects(), 0U);
        EXPECT_EQ(a.wakes + b.wakes, 0U);
    }
}

TEST(ManagedEventRuntime, TimeAdvanceDoesNotReevaluateOrWakeATriggeredExpression) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    auto owned = events.create();
    ASSERT_TRUE(owned);
    const auto saved = value(owned.value);
    ASSERT_EQ(scheduler.triggerValue(saved), Error::None);
    struct Frame {
        Value target;
        unsigned evaluations = 0;
    } frame{saved};
    Receiver waiter;
    waiter.add(context);
    k::MonitorSpec monitor;
    monitor.captureEvents = true;
    monitor.domain = k::Domain::Two;
    monitor.edge = k::Edge::Neg;
    monitor.frame = &frame;
    monitor.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        ++f.evaluations;
        out[0] = k::fromC(c).scheduler().triggeredValue(f.target).value ? 1 : 0;
    };
    scheduler.waitMonitor(waiter.process, monitor);
    struct Clock {
        k::ProcessId process = k::kNone;
        unsigned phase = 0;
    } clock;
    clock.process = scheduler.addProcess(
        [](kirima_context* c, void* raw) {
            auto& f = *static_cast<Clock*>(raw);
            if (f.phase++ == 0) {
                k::fromC(c).scheduler().waitDelay(f.process, 1);
                return static_cast<kirima_status>(k::Status::Waiting);
            }
            return static_cast<kirima_status>(k::Status::Done);
        },
        &clock, k::Group::Active);
    scheduler.start(clock.process);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(frame.evaluations, 1U);
    EXPECT_EQ(waiter.wakes, 0U);
    EXPECT_FALSE(scheduler.triggeredValue(saved).value);
    scheduler.cancelWait(waiter.process);
}

TEST(ManagedEventRuntime, NestedNoncapturingMonitorDoesNotPolluteTheOuterReadSet) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    auto a = events.create(), b = events.create();
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    struct Frame {
        Value a, b;
        k::ProcessId inner;
        bool valid = false;
    } frame{value(a.value), value(b.value), k::kNone};
    Receiver outer, inner;
    outer.add(context);
    inner.add(context);
    frame.inner = inner.process;
    k::MonitorSpec monitor;
    monitor.captureEvents = true;
    monitor.domain = k::Domain::Two;
    monitor.frame = &frame;
    monitor.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& scheduler = k::fromC(c).scheduler();
        f.valid = static_cast<bool>(scheduler.triggeredValue(f.a));
        k::MonitorSpec nested;
        nested.domain = k::Domain::Two;
        nested.frame = raw;
        nested.eval = [](kirima_context* c, void* raw, std::uint64_t* value) {
            auto& f = *static_cast<Frame*>(raw);
            f.valid = f.valid && static_cast<bool>(k::fromC(c).scheduler().triggeredValue(f.b));
            value[0] = 0;
        };
        scheduler.waitMonitor(f.inner, nested);
        out[0] = 0;
    };
    scheduler.waitMonitor(outer.process, monitor);
    EXPECT_TRUE(frame.valid);
    b.value.reset();
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_FALSE(events.valid({frame.b.bits}));
    EXPECT_TRUE(events.valid({frame.a.bits}));
    scheduler.cancelWait(outer.process);
    scheduler.cancelWait(inner.process);
}

TEST(ManagedEventRuntime, ProgramCompletionCancelsWaitsAndAlreadyOwnedPendingTriggers) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    struct Frame {
        k::ManagedEventPin owner;
        Value target;
        k::ProcessId initial = k::kNone, child = k::kNone;
        Receiver receiver;
        bool registered = false;
    } frame;
    auto owned = events.create();
    ASSERT_TRUE(owned);
    frame.target = value(owned.value);
    frame.owner = std::move(owned.value);
    const auto program = scheduler.addProgram();
    frame.initial = scheduler.addProgramInitial(
        [](kirima_context* c, void* raw) {
            auto& f = *static_cast<Frame*>(raw);
            auto& context = k::fromC(c);
            auto& scheduler = context.scheduler();
            f.child = scheduler.addChildProcess(Receiver::resume, &f.receiver, f.initial);
            f.registered = scheduler.waitNamedValue(f.child, f.target) == Error::None &&
                           scheduler.triggerNbValue(f.initial, f.target, 7) == Error::None;
            f.owner.reset();
            return static_cast<kirima_status>(k::Status::Done);
        },
        &frame, program);
    scheduler.start(frame.initial);
    EXPECT_EQ(scheduler.run(), k::EndReason::Finish);
    EXPECT_TRUE(frame.registered);
    EXPECT_EQ(frame.receiver.wakes, 0U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, OverflowDropsThePendingPinWhileNullSchedulesNothing) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    struct Frame {
        k::ManagedEventPin owner;
        Value target;
        k::ProcessId process = k::kNone;
        unsigned phase = 0;
        bool correct = false;
    } frame;
    auto owned = events.create();
    ASSERT_TRUE(owned);
    frame.target = value(owned.value);
    frame.owner = std::move(owned.value);
    frame.process = scheduler.addProcess(
        [](kirima_context* c, void* raw) {
            auto& f = *static_cast<Frame*>(raw);
            auto& context = k::fromC(c);
            auto& scheduler = context.scheduler();
            if (f.phase++ == 0) {
                scheduler.waitDelay(f.process, 7);
                return static_cast<kirima_status>(k::Status::Waiting);
            }
            const auto error = scheduler.triggerNbValue(f.process, f.target, UINT64_MAX);
            const bool overflow = scheduler.takeOverflow();
            const auto null = scheduler.triggerNbValue(f.process, {}, UINT64_MAX);
            f.correct = error == Error::None && overflow && null == Error::None &&
                        !scheduler.takeOverflow();
            f.owner.reset();
            f.correct = f.correct && context.managedEvents().collect() == Error::None &&
                        context.managedEvents().liveObjects() == 0;
            return static_cast<kirima_status>(k::Status::Done);
        },
        &frame, k::Group::Active);
    scheduler.start(frame.process);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_TRUE(frame.correct);
    EXPECT_EQ(scheduler.time(), 7U);
}

TEST(ManagedEventRuntime, DeliveryKeepsItsPinWhenTheCallbackCancelsAndReusesTheRecord) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    auto owned = events.create();
    ASSERT_TRUE(owned);
    struct Frame {
        Value first;
        k::ProcessId issuer = k::kNone;
        Receiver next;
        bool held = false, scheduled = false;
    } frame{value(owned.value)};
    Receiver issuer, original;
    issuer.add(context);
    original.add(context);
    frame.issuer = issuer.process;
    frame.next.add(context);
    const k::NamedValueWaitItem item{
        frame.first,
        [](kirima_context* c, void* raw) {
            auto& f = *static_cast<Frame*>(raw);
            auto& context = k::fromC(c);
            auto& scheduler = context.scheduler();
            scheduler.cancelManagedEventOperations();
            auto next = context.managedEvents().create();
            if (!next) return 0;
            f.scheduled =
                scheduler.waitNamedValue(f.next.process, value(next.value)) == Error::None &&
                scheduler.triggerNbValue(f.issuer, value(next.value), 0) == Error::None;
            next.value.reset();
            f.held = context.managedEvents().collect() == Error::None &&
                     context.managedEvents().valid({f.first.bits});
            return 1;
        },
        &frame};
    ASSERT_EQ(scheduler.waitAnyValues(original.process, {}, {}, {&item, 1}), Error::None);
    ASSERT_EQ(scheduler.triggerNbValue(issuer.process, frame.first, 1), Error::None);
    owned.value.reset();
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_TRUE(frame.held);
    EXPECT_TRUE(frame.scheduled);
    EXPECT_EQ(original.wakes, 0U);
    EXPECT_EQ(frame.next.wakes, 1U);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

class Memory final : public std::pmr::memory_resource {
public:
    int remaining = -1;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc{};
        if (remaining > 0) --remaining;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(ManagedEventRuntime, NamedWaitStagingFailuresPreserveTheOldBinding) {
    for (int budget = 0; budget < 6; ++budget) {
        Memory memory;
        k::Context context({nullptr, nullptr}, nullptr, &memory);
        auto& events = context.managedEvents();
        auto& scheduler = context.scheduler();
        auto old = events.create(), next = events.create();
        ASSERT_TRUE(old);
        ASSERT_TRUE(next);
        const auto ov = value(old.value), nv = value(next.value);
        Receiver receiver;
        receiver.add(context);
        ASSERT_EQ(scheduler.waitNamedValue(receiver.process, ov), Error::None);
        old.value.reset();
        memory.remaining = budget;
        const auto result = scheduler.waitNamedValue(receiver.process, nv);
        EXPECT_TRUE(result == Error::None || result == Error::ResourceLimit);
        next.value.reset();
        ASSERT_EQ(events.collect(), Error::None);
        const auto retained = result == Error::None ? nv : ov;
        EXPECT_TRUE(events.valid({retained.bits}));
        memory.remaining = -1;
        ASSERT_EQ(scheduler.triggerValue(retained), Error::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(receiver.wakes, 1U);
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_EQ(events.liveObjects(), 0U);
    }
}
TEST(ManagedEventRuntime, MonitorAllocationFailureLeavesNoPublishedPinOrCallback) {
    for (int budget = 0; budget < 8; ++budget) {
        Memory memory;
        k::Context context({nullptr, nullptr}, nullptr, &memory);
        auto& events = context.managedEvents();
        auto& scheduler = context.scheduler();
        auto created = events.create();
        ASSERT_TRUE(created);
        Receiver receiver;
        receiver.add(context);
        k::MonitorSpec spec;
        spec.eventValues = {value(created.value)};
        spec.domain = k::Domain::Two;
        spec.eval = [](kirima_context*, void*, std::uint64_t* out) { out[0] = 0; };
        memory.remaining = budget;
        const auto result = scheduler.waitAnyValues(receiver.process, {}, {&spec, 1}, {});
        EXPECT_TRUE(result == Error::None || result == Error::ResourceLimit);
        created.value.reset();
        scheduler.cancelWait(receiver.process);
        ASSERT_EQ(events.collect(), Error::None);
        EXPECT_EQ(events.liveObjects(), 0U);
        EXPECT_EQ(receiver.wakes, 0U);
    }
}
TEST(ManagedEventRuntime, FirstCaptureAllocationFailureRemainsAResourceError) {
    Memory memory;
    k::Context context({nullptr, nullptr}, nullptr, &memory);
    auto& scheduler = context.scheduler();
    auto created = context.managedEvents().create();
    ASSERT_TRUE(created);
    struct Frame {
        Value target;
        Error first = Error::None, second = Error::None;
    } frame{value(created.value)};
    Receiver receiver;
    receiver.add(context);
    k::MonitorSpec spec;
    spec.captureEvents = true;
    spec.domain = k::Domain::Two;
    spec.frame = &frame;
    spec.eval = [](kirima_context* c, void* raw, std::uint64_t* out) {
        auto& f = *static_cast<Frame*>(raw);
        auto& scheduler = k::fromC(c).scheduler();
        f.first = scheduler.triggeredValue(f.target).error;
        f.second = scheduler.triggeredValue({UINT64_MAX}).error;
        out[0] = 0;
    };
    memory.remaining = 0;
    scheduler.waitMonitor(receiver.process, spec);
    EXPECT_EQ(frame.first, Error::ResourceLimit);
    EXPECT_EQ(frame.second, Error::ResourceLimit);
    EXPECT_EQ(scheduler.run(), k::EndReason::ResourceLimit);
    created.value.reset();
    EXPECT_EQ(context.managedEvents().collect(), Error::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(ManagedEventRuntime, PendingAllocationFailureRollsBackAndFreedRowsNeedNoAllocation) {
    Memory memory;
    k::Context context({nullptr, nullptr}, nullptr, &memory);
    auto& scheduler = context.scheduler();
    auto& events = context.managedEvents();
    Receiver issuer;
    issuer.add(context);
    auto first = events.create();
    ASSERT_TRUE(first);
    memory.remaining = 0;
    EXPECT_EQ(scheduler.triggerNbValue(issuer.process, value(first.value), 7),
              Error::ResourceLimit);
    first.value.reset();
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
    EXPECT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(scheduler.time(), 0U);
    memory.remaining = -1;
    auto next = events.create();
    ASSERT_TRUE(next);
    ASSERT_EQ(scheduler.triggerNbValue(issuer.process, value(next.value), 1), Error::None);
    scheduler.cancelManagedEventOperations();
    memory.remaining = 0;
    EXPECT_EQ(scheduler.triggerNbValue(issuer.process, value(next.value), 2), Error::None);
    next.value.reset();
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    ASSERT_EQ(events.collect(), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEventRuntime, UnrepresentableDelaysDoNotScheduleNullOrRetainTargets) {
    k::Context context;
    auto& scheduler = context.scheduler();
    Receiver issuer;
    issuer.add(context);
    auto created = context.managedEvents().create();
    ASSERT_TRUE(created);
    const auto managed = value(created.value);
    const auto legacy = Value::legacy(scheduler.addEvent());
    EXPECT_EQ(scheduler.triggerNbValue(k::kNone, {}, std::nullopt), Error::None);
    EXPECT_FALSE(scheduler.takeOverflow());
    for (const auto target : {managed, legacy}) {
        EXPECT_EQ(scheduler.triggerNbValue(issuer.process, target, std::nullopt), Error::None);
        EXPECT_TRUE(scheduler.takeOverflow());
        EXPECT_FALSE(scheduler.takeOverflow());
    }
    EXPECT_EQ(scheduler.triggerNbValue(k::kNone, managed, std::nullopt), Error::InvalidToken);
    EXPECT_FALSE(scheduler.takeOverflow());
    EXPECT_EQ(scheduler.triggerNbValue(issuer.process, {UINT64_MAX}, std::nullopt),
              Error::InvalidToken);
    EXPECT_FALSE(scheduler.takeOverflow());
    created.value.reset();
    ASSERT_EQ(context.managedEvents().collect(), Error::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    EXPECT_EQ(scheduler.time(), 0U);
    EXPECT_EQ(count(context.counters(), "sched.events.nba"), 0U);
    EXPECT_EQ(count(context.counters(), "sched.events.re_nba"), 0U);
}
}  // namespace
