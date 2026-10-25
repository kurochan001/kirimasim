// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original registry/real-scheduler checks of automatic-capture contract 5/6/8.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/managed_event.h"

namespace {
namespace k = kirimasim::kernel;
using Error = k::ManagedEventError;
using Token = k::ManagedEventToken;

struct Roots {
    std::vector<Token> values;
    Error result = Error::None;
    static Error enumerate(const void* frame, k::ManagedEventRoots& sink) noexcept {
        const auto& self = *static_cast<const Roots*>(frame);
        for (auto token : self.values)
            sink.add(token);
        return self.result;
    }
    k::ManagedEventRootProvider provider() const { return {this, enumerate}; }
};
Error collect(k::ManagedEvents& store, const Roots& roots) {
    const std::array providers{roots.provider()};
    return store.collect(providers);
}

TEST(ManagedEvent, NullMalformedAndExpiredTokensNeverResolveAsObjects) {
    k::Context context;
    auto& store = context.managedEvents();
    EXPECT_EQ(store.resolve(Token{}).error, Error::Null);
    EXPECT_EQ(store.retain(Token{}).error, Error::Null);
    for (auto bits : {std::uint64_t{1}, std::uint64_t{1} << 32, UINT64_MAX}) {
        EXPECT_EQ(store.resolve(Token{bits}).error, Error::InvalidToken);
        EXPECT_EQ(store.retain(Token{bits}).error, Error::InvalidToken);
    }
    auto first = store.create();
    ASSERT_TRUE(first);
    const auto stale = first.value.token();
    const auto physical = store.resolve(first.value).value;
    first.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_FALSE(store.valid(stale));
    EXPECT_EQ(store.resolve(stale).error, Error::InvalidToken);
    auto next = store.create();
    ASSERT_TRUE(next);
    EXPECT_EQ(store.resolve(next.value).value, physical);
    EXPECT_NE(next.value.token(), stale);
    EXPECT_EQ(store.retain(stale).error, Error::InvalidToken);
}

TEST(ManagedEvent, TypedHoldersAndDuplicateRootsRetainOnePhysicalObject) {
    k::Context context;
    auto& store = context.managedEvents();
    auto owner = store.create();
    ASSERT_TRUE(owner);
    const auto token = owner.value.token();
    Roots roots{{token, token, {}}};
    owner.value.reset();
    ASSERT_EQ(collect(store, roots), Error::None);
    EXPECT_TRUE(store.valid(token));
    EXPECT_EQ(store.liveObjects(), 1U);
    EXPECT_FALSE(store.collectionEligible());
    roots.values.clear();
    store.rootsChanged();
    EXPECT_TRUE(store.collectionEligible());
    ASSERT_EQ(collect(store, roots), Error::None);
    EXPECT_EQ(store.liveObjects(), 0U);
    EXPECT_FALSE(store.valid(token));
}

TEST(ManagedEvent, PinsShareIdentityMoveOnceAndRemainIndependentOfTypedHolders) {
    k::Context context;
    auto& store = context.managedEvents();
    auto first = store.create();
    ASSERT_TRUE(first);
    const auto token = first.value.token();
    auto second = store.retain(first.value);
    ASSERT_TRUE(second);
    auto third = store.retain(token);
    ASSERT_TRUE(third);
    first.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_TRUE(store.valid(token));
    k::ManagedEventPin moved = std::move(second.value);
    EXPECT_FALSE(second.value);
    EXPECT_TRUE(moved);
    third.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_TRUE(store.valid(token));
    moved.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_FALSE(store.valid(token));
}

TEST(ManagedEvent, ContextIdentityIsCarriedByCppPins) {
    k::Context first, second;
    auto pin = first.managedEvents().create();
    auto unrelated = second.managedEvents().create();
    ASSERT_TRUE(pin);
    ASSERT_TRUE(unrelated);
    EXPECT_EQ(second.managedEvents().retain(pin.value).error, Error::InvalidToken);
    EXPECT_EQ(second.managedEvents().resolve(pin.value).error, Error::InvalidToken);
    ASSERT_EQ(first.managedEvents().reset(), Error::None);
    EXPECT_FALSE(pin.value);
    EXPECT_TRUE(unrelated.value);
    EXPECT_EQ(second.managedEvents().liveObjects(), 1U);
}

TEST(ManagedEvent, ResetInvalidatesOldPinsWithoutRewindingGenerations) {
    k::Context context;
    auto& store = context.managedEvents();
    auto first = store.create();
    ASSERT_TRUE(first);
    const auto stale = first.value.token();
    auto second = store.retain(first.value);
    ASSERT_TRUE(second);
    ASSERT_EQ(store.reset(), Error::None);
    EXPECT_FALSE(first.value);
    EXPECT_FALSE(second.value);
    auto next = store.create();
    ASSERT_TRUE(next);
    EXPECT_NE(next.value.token(), stale);
    first.value.reset();
    second.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_TRUE(next.value);
    EXPECT_EQ(store.liveObjects(), 1U);
}

TEST(ManagedEvent, FailedRootEnumerationConservativelyKeepsEveryObject) {
    k::Context context;
    auto& store = context.managedEvents();
    auto first = store.create(), second = store.create();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto a = first.value.token(), b = second.value.token();
    first.value.reset();
    second.value.reset();
    Roots roots{{a, Token{UINT64_MAX}}};
    EXPECT_EQ(collect(store, roots), Error::InvalidToken);
    EXPECT_TRUE(store.valid(a));
    EXPECT_TRUE(store.valid(b));
    EXPECT_TRUE(store.collectionEligible());
    roots.values = {a};
    roots.result = Error::ResourceLimit;
    EXPECT_EQ(collect(store, roots), Error::ResourceLimit);
    EXPECT_TRUE(store.valid(b));
    roots.result = Error::None;
    ASSERT_EQ(collect(store, roots), Error::None);
    EXPECT_TRUE(store.valid(a));
    EXPECT_FALSE(store.valid(b));
}

TEST(ManagedEvent, RootEnumerationRejectsReentrantMutationAndCollection) {
    k::Context context;
    auto& store = context.managedEvents();
    auto owner = store.create();
    ASSERT_TRUE(owner);
    struct Frame {
        k::ManagedEvents* store;
        Token token;
        bool checked = false;
    } frame{&store, owner.value.token()};
    const k::ManagedEventRootProvider provider{
        &frame, [](const void* opaque, k::ManagedEventRoots& sink) noexcept {
            auto& f = *const_cast<Frame*>(static_cast<const Frame*>(opaque));
            f.checked = f.store->create().error == Error::Busy &&
                        f.store->retain(f.token).error == Error::Busy &&
                        f.store->collect() == Error::Busy && f.store->reset() == Error::Busy &&
                        f.store->setBackend({}) == Error::Busy;
            return sink.add(f.token);
        }};
    owner.value.reset();
    ASSERT_EQ(store.collect(std::span{&provider, 1}), Error::None);
    EXPECT_TRUE(frame.checked);
    EXPECT_TRUE(store.valid(frame.token));
}

TEST(ManagedEvent, ReusedSchedulerObjectsClearTriggeredStateAndStaleWaitQueues) {
    k::Context context;
    auto& store = context.managedEvents();
    auto& scheduler = context.scheduler();
    auto owner = store.create();
    ASSERT_TRUE(owner);
    const auto physical = store.resolve(owner.value).value;
    const auto waiter = scheduler.addProcess(
        [](kirima_context*, void*) { return static_cast<kirima_status>(k::Status::Done); }, nullptr,
        k::Group::Active);
    scheduler.trigger(physical);
    EXPECT_TRUE(scheduler.triggered(physical));
    scheduler.waitNamed(waiter, physical);
    EXPECT_EQ(scheduler.waitListEntries(), 1U);
    scheduler.cancelWait(waiter);
    owner.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_EQ(scheduler.waitListEntries(), 0U);
    auto next = store.create();
    ASSERT_TRUE(next);
    EXPECT_EQ(store.resolve(next.value).value, physical);
    EXPECT_FALSE(scheduler.triggered(physical));
}

TEST(ManagedEvent, ReclaimClearsDetachedMonitorReferencesBeforeObjectReuse) {
    k::Context context;
    auto& store = context.managedEvents();
    auto& scheduler = context.scheduler();
    auto owner = store.create();
    ASSERT_TRUE(owner);
    const auto event = store.resolve(owner.value).value;
    const auto process = scheduler.addProcess(
        [](kirima_context*, void*) { return static_cast<kirima_status>(k::Status::Done); }, nullptr,
        k::Group::Active);
    k::MonitorSpec monitor;
    monitor.domain = k::Domain::Two;
    monitor.events = {event};
    monitor.eval = [](kirima_context*, void*, std::uint64_t* value) { value[0] = 0; };
    scheduler.waitMonitor(process, monitor);
    EXPECT_EQ(scheduler.waitListEntries(), 1U);
    scheduler.cancelWait(process);
    owner.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_EQ(scheduler.waitListEntries(), 0U);
}

TEST(ManagedEvent, ReclaimingManagedObjectsDoesNotResetLegacyStaticEvents) {
    k::Context context;
    auto& scheduler = context.scheduler();
    auto& store = context.managedEvents();
    const auto legacy = scheduler.addEvent();
    scheduler.trigger(legacy);
    auto pin = store.create();
    ASSERT_TRUE(pin);
    EXPECT_NE(store.resolve(pin.value).value, legacy);
    pin.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_TRUE(scheduler.triggered(legacy));
    EXPECT_EQ(k::eventOfHandle(k::eventHandle(legacy)), legacy);
}

TEST(ManagedEvent, RuntimePinProtectsConcreteDelayedTargetAfterIssuerReturns) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context context;
        auto& store = context.managedEvents();
        auto& scheduler = context.scheduler();
        auto owner = store.create();
        ASSERT_TRUE(owner);
        const auto target = store.resolve(owner.value).value;
        auto pending = store.retain(owner.value);
        ASSERT_TRUE(pending);
        struct Frame {
            k::EventId target;
            bool woke = false;
            k::ProcessId issuer = k::kNone;
        } frame{target};
        const auto waiter = scheduler.addProcess(
            [](kirima_context*, void* opaque) {
                static_cast<Frame*>(opaque)->woke = true;
                return static_cast<kirima_status>(k::Status::Done);
            },
            &frame, group);
        scheduler.waitNamed(waiter, target);
        const auto issuer = scheduler.addProcess(
            [](kirima_context* c, void* opaque) {
                auto& scheduler = k::fromC(c).scheduler();
                const auto& frame = *static_cast<Frame*>(opaque);
                scheduler.triggerNb(frame.issuer, frame.target, 5);
                return static_cast<kirima_status>(k::Status::Done);
            },
            &frame, group);
        frame.issuer = issuer;
        scheduler.start(issuer);
        owner.value.reset();
        ASSERT_EQ(store.collect(), Error::None);
        auto unrelated = store.create();
        ASSERT_TRUE(unrelated);
        EXPECT_NE(store.resolve(unrelated.value).value, target);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_TRUE(frame.woke);
        EXPECT_TRUE(scheduler.triggered(target));
        EXPECT_FALSE(scheduler.triggered(store.resolve(unrelated.value).value));
        pending.value.reset();
        ASSERT_EQ(store.collect(), Error::None);
        EXPECT_EQ(store.liveObjects(), 1U);
    }
}

TEST(ManagedEvent, FreshAndReusedObjectsRemainUntriggeredAtTheLargestTime) {
    for (auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context context;
        auto& store = context.managedEvents();
        auto& scheduler = context.scheduler();
        struct Frame {
            k::ProcessId process = k::kNone;
            k::EventId legacy = k::kNone, reused = k::kNone;
            k::ManagedEventPin owner;
            unsigned phase = 0;
            bool correct = false;
        } frame;
        frame.legacy = scheduler.addEvent();
        auto original = store.create();
        ASSERT_TRUE(original);
        frame.reused = store.resolve(original.value).value;
        frame.owner = std::move(original.value);
        scheduler.trigger(frame.reused);
        frame.process = scheduler.addProcess(
            [](kirima_context* c, void* opaque) {
                auto& frame = *static_cast<Frame*>(opaque);
                auto& context = k::fromC(c);
                auto& store = context.managedEvents();
                auto& scheduler = context.scheduler();
                if (frame.phase++ == 0) {
                    scheduler.waitDelay(frame.process, UINT64_MAX);
                    return static_cast<kirima_status>(k::Status::Waiting);
                }
                frame.correct = scheduler.time() == UINT64_MAX &&
                                !scheduler.triggered(frame.legacy) &&
                                !scheduler.triggered(frame.reused);
                scheduler.trigger(frame.reused);
                frame.correct = frame.correct && scheduler.triggered(frame.reused);
                frame.owner.reset();
                frame.correct = frame.correct && store.collect() == Error::None;
                auto next = store.create();
                if (!next) return static_cast<kirima_status>(k::Status::Error);
                const auto event = store.resolve(next.value).value;
                frame.correct =
                    frame.correct && event == frame.reused && !scheduler.triggered(event);
                scheduler.trigger(event);
                frame.correct = frame.correct && scheduler.triggered(event);
                return static_cast<kirima_status>(k::Status::Done);
            },
            &frame, group);
        scheduler.start(frame.process);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_TRUE(frame.correct);
    }
}

Error collectActivationEvents(k::Context& context) {
    const std::array providers{context.activations().eventRootProvider()};
    return context.managedEvents().collect(providers);
}
void writeEvent(k::ActivationStore& store, k::ActivationBinding binding, Token token,
                std::uint32_t element = 0) {
    const std::array<k::W4, 1> value{{{token.bits, 0}}};
    ASSERT_EQ(store.write(binding, element, value), k::ActivationError::None);
}

TEST(ManagedEvent, RetainedAutomaticScopePublishesAllUnreadEventArrayFieldsOnlyOnce) {
    k::Context context;
    auto& activations = context.activations();
    auto& events = context.managedEvents();
    const auto layout = activations.addLayout(
        std::vector<k::ActivationField>{{k::ActivationKind::Event, 64, {{-1, 0}}},
                                        {k::ActivationKind::Event, 64, {}},
                                        {k::ActivationKind::Two, 64, {}}});
    ASSERT_TRUE(layout);
    auto scope = activations.create(layout.value);
    ASSERT_TRUE(scope);
    const auto array = activations.bind(scope.value, 0).value;
    const auto scalar = activations.bind(scope.value, 1).value;
    const auto integer = activations.bind(scope.value, 2).value;
    std::array<Token, 4> tokens;
    for (std::uint32_t i = 0; i < 4; ++i) {
        auto event = events.create();
        ASSERT_TRUE(event);
        tokens[i] = event.value.token();
        writeEvent(activations,
                   i < 2    ? array
                   : i == 2 ? scalar
                            : integer,
                   tokens[i], i < 2 ? i : 0);
    }
    auto descendant = activations.retain(array);
    ASSERT_TRUE(descendant);
    scope.value.reset();
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    EXPECT_EQ(events.liveObjects(), 3U);
    for (std::size_t i = 0; i < 3; ++i)
        EXPECT_TRUE(events.valid(tokens[i]));
    EXPECT_FALSE(events.valid(tokens[3]));
    activations.acknowledgeEventRoots();
    descendant.value.reset();
    EXPECT_TRUE(activations.eventRootsChanged());
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    EXPECT_EQ(events.liveObjects(), 0U);
}

TEST(ManagedEvent, EventRootChangesAreIndependentOfHeapRootAcknowledgement) {
    k::Context context;
    auto& activations = context.activations();
    auto& events = context.managedEvents();
    const auto layout =
        activations.addLayout(std::vector<k::ActivationField>{{k::ActivationKind::Event, 64, {}}});
    ASSERT_TRUE(layout);
    auto scope = activations.create(layout.value);
    ASSERT_TRUE(scope);
    const auto binding = activations.bind(scope.value, 0).value;
    auto first = events.create(), second = events.create();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    const auto old = first.value.token(), next = second.value.token();
    writeEvent(activations, binding, old);
    first.value.reset();
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    activations.acknowledgeRoots();
    activations.acknowledgeEventRoots();
    writeEvent(activations, binding, next);
    EXPECT_FALSE(activations.rootsChanged());
    EXPECT_TRUE(activations.eventRootsChanged());
    activations.acknowledgeRoots();
    EXPECT_TRUE(activations.eventRootsChanged());
    second.value.reset();
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    EXPECT_FALSE(events.valid(old));
    EXPECT_TRUE(events.valid(next));
    activations.acknowledgeEventRoots();
    writeEvent(activations, binding, next);
    EXPECT_FALSE(activations.eventRootsChanged());
}

TEST(ManagedEvent, AutomaticRootPublisherPreservesLegacyStaticEventAssociations) {
    k::Context context;
    auto& activations = context.activations();
    auto& events = context.managedEvents();
    const auto legacy = context.scheduler().addEvent();
    context.scheduler().trigger(legacy);
    const auto layout =
        activations.addLayout(std::vector<k::ActivationField>{{k::ActivationKind::Event, 64, {}}});
    ASSERT_TRUE(layout);
    auto scope = activations.create(layout.value);
    ASSERT_TRUE(scope);
    writeEvent(activations, activations.bind(scope.value, 0).value, Token{k::eventHandle(legacy)});
    auto unrelated = events.create();
    ASSERT_TRUE(unrelated);
    const auto token = unrelated.value.token();
    unrelated.value.reset();
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    EXPECT_FALSE(events.valid(token));
    EXPECT_TRUE(context.scheduler().triggered(legacy));
}

TEST(ManagedEvent, EventFieldsRejectUnknownBitsWrongWidthsAndPartialWrites) {
    k::Context context;
    auto& activations = context.activations();
    EXPECT_FALSE(
        activations.addLayout(std::vector<k::ActivationField>{{k::ActivationKind::Event, 32, {}}}));
    const auto layout =
        activations.addLayout(std::vector<k::ActivationField>{{k::ActivationKind::Event, 64, {}}});
    ASSERT_TRUE(layout);
    auto scope = activations.create(layout.value);
    ASSERT_TRUE(scope);
    const auto field = activations.bind(scope.value, 0).value;
    const std::array<k::W4, 1> unknown{{{7, 1}}};
    EXPECT_EQ(activations.write(field, 0, unknown), k::ActivationError::InvalidValue);
    const std::array<k::W4, 1> value{{{7, 0}}};
    EXPECT_EQ(activations.write(field, 0, value, 1, 1), k::ActivationError::OutOfRange);
    const auto readonly = activations.bind(scope.value, 0, true).value;
    EXPECT_EQ(activations.write(readonly, 0, value), k::ActivationError::ReadOnly);
    std::array<k::W4, 1> read;
    ASSERT_EQ(activations.read(field, 0, read), k::ActivationError::None);
    EXPECT_EQ(read[0].a, 0U);
    EXPECT_EQ(read[0].b, 0U);
}

TEST(ManagedEvent, InvalidEventFieldStopsCollectionUntilItsAssociationIsCleared) {
    k::Context context;
    auto& activations = context.activations();
    auto& events = context.managedEvents();
    const auto layout =
        activations.addLayout(std::vector<k::ActivationField>{{k::ActivationKind::Event, 64, {}}});
    ASSERT_TRUE(layout);
    auto scope = activations.create(layout.value);
    ASSERT_TRUE(scope);
    const auto field = activations.bind(scope.value, 0).value;
    auto first = events.create();
    ASSERT_TRUE(first);
    writeEvent(activations, field, first.value.token());
    ASSERT_EQ(events.reset(), Error::None);
    auto unrelated = events.create();
    ASSERT_TRUE(unrelated);
    const auto token = unrelated.value.token();
    unrelated.value.reset();
    EXPECT_EQ(collectActivationEvents(context), Error::InvalidToken);
    EXPECT_TRUE(events.valid(token));
    writeEvent(activations, field, {});
    ASSERT_EQ(collectActivationEvents(context), Error::None);
    EXPECT_FALSE(events.valid(token));
}

struct Backend {
    std::uint32_t created = 0, cleared = 0;
    Error failure = Error::None;
    k::ManagedEventBackend api() {
        return {
            this,
            [](void* opaque) noexcept -> k::ManagedEventResult<k::EventId> {
                auto& self = *static_cast<Backend*>(opaque);
                if (self.failure != Error::None) return {k::kNone, self.failure};
                return {k::EventId{self.created++}};
            },
            [](void* opaque, k::EventId) noexcept { ++static_cast<Backend*>(opaque)->cleared; }};
    }
};
class Memory final : public std::pmr::memory_resource {
public:
    int remaining = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc{};
        --remaining;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

TEST(ManagedEvent, AllocationFailuresPublishNoPartialEventAndPreserveExistingPins) {
    for (int budget = 0; budget < 8; ++budget) {
        Memory memory;
        memory.remaining = budget;
        Backend backend;
        k::ManagedEvents store{&memory};
        ASSERT_EQ(store.setBackend(backend.api()), Error::None);
        std::vector<k::ManagedEventPin> pins;
        for (int i = 0; i < 32; ++i) {
            auto created = store.create();
            if (!created) {
                EXPECT_EQ(created.error, Error::ResourceLimit);
                EXPECT_EQ(store.liveObjects(), pins.size());
                EXPECT_EQ(backend.created, pins.size());
                for (const auto& pin : pins)
                    EXPECT_TRUE(pin);
                break;
            }
            pins.push_back(std::move(created.value));
        }
        pins.clear();
        memory.remaining = 0;
        ASSERT_EQ(store.collect(), Error::None);
        EXPECT_EQ(store.liveObjects(), 0U);
        EXPECT_EQ(backend.cleared, backend.created);
        if (backend.created != 0) {
            auto reused = store.create();
            ASSERT_TRUE(reused);
            EXPECT_EQ(backend.created, store.allocatedObjects());
        }
    }
}

TEST(ManagedEvent, BackendFailureRollsBackTheRowAndBackendCannotBeReplacedAfterUse) {
    Backend backend;
    k::ManagedEvents store;
    EXPECT_EQ(store.create().error, Error::InvalidBackend);
    ASSERT_EQ(store.setBackend(backend.api()), Error::None);
    backend.failure = Error::ResourceLimit;
    EXPECT_EQ(store.create().error, Error::ResourceLimit);
    EXPECT_EQ(store.allocatedObjects(), 0U);
    EXPECT_EQ(store.liveObjects(), 0U);
    EXPECT_EQ(backend.created, 0U);
    backend.failure = Error::None;
    auto pin = store.create();
    ASSERT_TRUE(pin);
    EXPECT_EQ(store.setBackend(backend.api()), Error::InvalidBackend);
    pin.value.reset();
    ASSERT_EQ(store.collect(), Error::None);
    EXPECT_EQ(store.setBackend(backend.api()), Error::InvalidBackend);
}

TEST(ManagedEvent, RepeatedLifetimesReuseBoundedObjectsWithoutStaleAliases) {
    Backend backend;
    k::ManagedEvents store;
    ASSERT_EQ(store.setBackend(backend.api()), Error::None);
    Token previous;
    for (int i = 0; i < 10000; ++i) {
        auto pin = store.create();
        ASSERT_TRUE(pin);
        EXPECT_NE(pin.value.token(), previous);
        EXPECT_FALSE(store.valid(previous));
        previous = pin.value.token();
        pin.value.reset();
        ASSERT_EQ(store.collect(), Error::None);
    }
    EXPECT_EQ(store.allocatedObjects(), 1U);
    EXPECT_EQ(backend.created, 1U);
    EXPECT_EQ(backend.cleared, 10000U);
    EXPECT_EQ(store.retiredObjects(), 0U);
}

}  // namespace
