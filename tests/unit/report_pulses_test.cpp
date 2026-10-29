// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original cancellation-notice checks for CLI 2.2 / SCHEDULER_SPEC 12.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using Kind = k::PulseKind;
using Error = k::StrengthError;
using Range = k::StrengthRange;
using Conduction = k::StrengthConduction;

struct Notice {
    Kind kind = Kind::Drive;
    std::uint32_t id = 0;
    std::uint64_t time = 0;
    bool pending = false;
};
struct Notices {
    const k::Context* context = nullptr;
    std::array<Notice, 16> values{};
    unsigned used = 0;
    bool wrongTime = false;
    static void record(void* raw, Kind kind, std::uint32_t id, std::uint64_t time) noexcept {
        auto& self = *static_cast<Notices*>(raw);
        bool pending = false;
        if (self.context != nullptr) {
            self.wrongTime = self.wrongTime || self.context->scheduler().time() != time;
            if (kind == Kind::StrengthDrive) {
                const auto state = self.context->strengthDrivers().origin(id);
                pending = state && state->hasPending;
            } else if (kind == Kind::StrengthEdge) {
                const auto state = self.context->strengthDrivers().edge(id);
                pending = state && state->hasPending;
            }
        }
        if (self.used < self.values.size()) self.values[self.used] = {kind, id, time, pending};
        ++self.used;
    }
    k::PulseHook hook() { return {record, this}; }
};

struct Fixture {
    k::Context context;
    Kind kind;
    k::SlotId target{0};
    std::uint32_t driver = 0;
    explicit Fixture(Kind kind) : kind(kind) {
        auto& scheduler = context.scheduler();
        if (kind == Kind::Drive) {
            target = context.store().add(k::Domain::Four, 65, 1, k::Init::Zero);
            scheduler.addDelayedDriver();
            driver = scheduler.addDelayedDriver();
            return;
        }
        const auto first = context.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        const auto second = context.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        const Range initial{};
        EXPECT_EQ(scheduler.enableStrengths(first, {&initial, 1}), Error::None);
        EXPECT_EQ(scheduler.enableStrengths(second, {&initial, 1}), Error::None);
        const std::array<k::StrengthNodeSpec, 2> nodes{{{{first, 0, 0, 1}}, {{second, 0, 0, 1}}}};
        const k::StrengthOriginSpec origin{0, {3, 3}};
        const k::StrengthEdgeSpec edge{0, 1, k::StrengthTransfer::Nonresistive, Conduction::Off};
        EXPECT_EQ(scheduler.configureStrengthDrivers(nodes, {&origin, 1}, {&edge, 1}), Error::None);
        EXPECT_EQ(scheduler.startStrengthDrivers(), Error::None);
        target = kind == Kind::StrengthDrive ? first : second;
    }
    Error drive(unsigned value, std::uint64_t delay, bool immediate = false) {
        auto& scheduler = context.scheduler();
        if (kind == Kind::Drive) {
            const std::array<std::uint64_t, 4> words{value == 0 ? 0U : 1U, value == 2 ? 1U : 0U,
                                                     value == 0 ? 0U : 1U, value == 2 ? 1U : 0U};
            scheduler.driveDelayed(driver, {target, 0, 0, 65}, words.data(), delay, delay, delay);
            return Error::None;
        }
        if (kind == Kind::StrengthDrive) {
            const Range range = value == 0 ? Range{3, 3} : value == 1 ? Range{6, 6} : Range{-6, -6};
            return immediate ? scheduler.driveStrength(0, range)
                             : scheduler.driveStrengthDelayed(0, range, delay, delay, delay);
        }
        const auto state = value == 0   ? Conduction::Off
                           : value == 1 ? Conduction::On
                                        : Conduction::Uncertain;
        return immediate ? scheduler.setStrengthEdge(0, state)
                         : scheduler.setStrengthEdgeDelayed(0, state, delay, delay);
    }
};
struct Action {
    std::uint64_t time = 0;
    unsigned value = 0;
    std::uint64_t delay = 0;
    bool immediate = false;
};
struct Script {
    Fixture& fixture;
    std::vector<Action> actions;
    k::ProcessId process{0};
    std::size_t next = 0;
    unsigned errors = 0;
    static kirima_status resume(kirima_context*, void* raw) {
        auto& self = *static_cast<Script*>(raw);
        auto& scheduler = self.fixture.context.scheduler();
        if (self.next < self.actions.size() && self.actions[self.next].time != scheduler.time()) {
            scheduler.waitDelay(self.process, self.actions[self.next].time - scheduler.time());
            return static_cast<kirima_status>(k::Status::Waiting);
        }
        while (self.next < self.actions.size() &&
               self.actions[self.next].time == scheduler.time()) {
            const auto& action = self.actions[self.next++];
            if (self.fixture.drive(action.value, action.delay, action.immediate) != Error::None)
                ++self.errors;
        }
        if (self.next == self.actions.size()) return static_cast<kirima_status>(k::Status::Done);
        scheduler.waitDelay(self.process, self.actions[self.next].time - scheduler.time());
        return static_cast<kirima_status>(k::Status::Waiting);
    }
    void start(k::Group group = k::Group::Active) {
        auto& scheduler = fixture.context.scheduler();
        process = scheduler.addProcess(resume, this, group);
        scheduler.start(process);
    }
};

TEST(ReportPulses, HookReplacementReturnsThePreviousContextLocalObserver) {
    Fixture first{Kind::Drive}, second{Kind::Drive};
    Notices a{&first.context}, b{&first.context}, c{&second.context};
    auto& scheduler = first.context.scheduler();
    const auto none = scheduler.setPulseHook(a.hook());
    EXPECT_EQ(none.fn, nullptr);
    EXPECT_EQ(none.frame, nullptr);
    const auto previous = scheduler.setPulseHook(b.hook());
    EXPECT_EQ(previous.fn, &Notices::record);
    EXPECT_EQ(previous.frame, &a);
    second.context.scheduler().setPulseHook(c.hook());
    EXPECT_EQ(first.drive(1, 5), Error::None);
    EXPECT_EQ(first.drive(0, 5), Error::None);
    EXPECT_EQ(a.used, 0U);
    EXPECT_EQ(b.used, 1U);
    EXPECT_EQ(c.used, 0U);
    const auto replaced = scheduler.setPulseHook(previous);
    EXPECT_EQ(replaced.frame, &b);
    EXPECT_EQ(first.drive(1, 5), Error::None);
    EXPECT_EQ(first.drive(0, 5), Error::None);
    EXPECT_EQ(a.used, 1U);
    EXPECT_EQ(b.used, 1U);
    EXPECT_EQ(second.drive(1, 5), Error::None);
    EXPECT_EQ(second.drive(0, 5), Error::None);
    EXPECT_EQ(c.used, 1U);
}

TEST(ReportPulses, DifferentPendingValuesReportOnceAndIdenticalValuesKeepTheirDeadline) {
    for (const auto kind : {Kind::Drive, Kind::StrengthDrive, Kind::StrengthEdge}) {
        for (const auto group : {k::Group::Active, k::Group::Reactive}) {
            SCOPED_TRACE(::testing::Message()
                         << static_cast<unsigned>(kind) << '/' << static_cast<unsigned>(group));
            Fixture fixture{kind};
            Notices notices{&fixture.context};
            fixture.context.scheduler().setPulseHook(notices.hook());
            Script script{fixture, {{0, 1, 5}, {1, 1, 99}, {2, 2, 5}, {3, 0, 99}, {9, 1, 2}}};
            script.start(group);
            ASSERT_EQ(fixture.context.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(script.errors, 0U);
            ASSERT_EQ(notices.used, 2U);
            EXPECT_FALSE(notices.wrongTime);
            for (unsigned i = 0; i < 2; ++i) {
                EXPECT_EQ(notices.values[i].kind, kind);
                EXPECT_EQ(notices.values[i].id, kind == Kind::Drive ? fixture.driver : 0U);
                EXPECT_EQ(notices.values[i].time, i + 2U);
            }
            if (kind != Kind::Drive) {
                EXPECT_TRUE(notices.values[0].pending);   // Replacement was already installed.
                EXPECT_FALSE(notices.values[1].pending);  // Reversion already canceled it.
            }
            EXPECT_EQ(fixture.context.scheduler().time(), 11U);
        }
    }
}

TEST(ReportPulses, UncanceledPropagationAndNbasDoNotNotify) {
    for (const auto kind : {Kind::Drive, Kind::StrengthDrive, Kind::StrengthEdge}) {
        Fixture fixture{kind};
        Notices notices{&fixture.context};
        fixture.context.scheduler().setPulseHook(notices.hook());
        Script script{fixture, {{0, 1, 5}, {2, 1, 99}, {6, 1, 99}}};
        script.start();
        ASSERT_EQ(fixture.context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(script.errors, 0U);
        EXPECT_EQ(notices.used, 0U);
        EXPECT_EQ(fixture.context.scheduler().time(), 6U);
        const auto idle = [](kirima_context*, void*) -> kirima_status {
            return static_cast<kirima_status>(k::Status::Done);
        };
        const auto process =
            fixture.context.scheduler().addProcess(idle, nullptr, k::Group::Active);
        const auto slot = fixture.context.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        const std::uint64_t one = 1, zero = 0;
        fixture.context.scheduler().scheduleNba(process, {slot, 0, 0, 1}, &one, 2);
        fixture.context.scheduler().scheduleNba(process, {slot, 0, 0, 1}, &zero, 1);
        ASSERT_EQ(fixture.context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(notices.used, 0U);
    }
}

TEST(ReportPulses, ImmediateStrengthChangesReportOnlyDifferentPendingValuesAfterMutation) {
    for (const auto kind : {Kind::StrengthDrive, Kind::StrengthEdge}) {
        Fixture fixture{kind};
        Notices notices{&fixture.context};
        fixture.context.scheduler().setPulseHook(notices.hook());
        Script script{
            fixture,
            {{0, 1, 5}, {1, 0, 0, true}, {2, 1, 5}, {3, 1, 0, true}, {4, 2, 5}, {5, 0, 0, true}}};
        script.start();
        ASSERT_EQ(fixture.context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(script.errors, 0U);
        ASSERT_EQ(notices.used, 2U);
        EXPECT_EQ(notices.values[0].time, 1U);
        EXPECT_EQ(notices.values[1].time, 5U);
        EXPECT_FALSE(notices.values[0].pending);
        EXPECT_FALSE(notices.values[1].pending);
    }
}

TEST(ReportPulses, SideOnlyReversionCancelsACompleteDriverState) {
    k::Context context;
    const auto target = context.store().add(k::Domain::Four, 1, 1, k::Init::X);
    const auto side = context.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    const auto driver = context.scheduler().addDelayedDriver();
    Notices notices{&context};
    context.scheduler().setPulseHook(notices.hook());
    const std::array<std::uint64_t, 2> x{1, 1};
    const k::DelayedSideUpdate enabled{{side, 0, 0, 1}, true}, disabled{{side, 0, 0, 1}, false};
    context.scheduler().driveDelayed(driver, {target, 0, 0, 1}, x.data(), 5, 5, 5, {&enabled, 1});
    context.scheduler().driveDelayed(driver, {target, 0, 0, 1}, x.data(), 99, 99, 99,
                                     {&enabled, 1});
    EXPECT_EQ(notices.used, 0U);
    context.scheduler().driveDelayed(driver, {target, 0, 0, 1}, x.data(), 5, 5, 5, {&disabled, 1});
    ASSERT_EQ(notices.used, 1U);
    EXPECT_EQ(notices.values[0].kind, Kind::Drive);
    EXPECT_EQ(notices.values[0].id, driver);
    ASSERT_EQ(context.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(notices.used, 1U);
    EXPECT_EQ(context.store().element(side, 0)[0], 0U);
}

TEST(ReportPulses, InvalidStrengthInputsDoNotReportOrCancelThePendingValue) {
    for (const auto kind : {Kind::StrengthDrive, Kind::StrengthEdge}) {
        Fixture fixture{kind};
        Notices notices{&fixture.context};
        auto& scheduler = fixture.context.scheduler();
        scheduler.setPulseHook(notices.hook());
        ASSERT_EQ(fixture.drive(1, 5), Error::None);
        EXPECT_EQ(scheduler.driveStrengthDelayed(UINT32_MAX, {6, 6}, 2, 2, 2),
                  Error::InvalidTarget);
        EXPECT_EQ(scheduler.setStrengthEdgeDelayed(UINT32_MAX, Conduction::Off, 2, 2),
                  Error::InvalidTarget);
        EXPECT_EQ(scheduler.driveStrengthDelayed(0, {8, 8}, 2, 2, 2), Error::InvalidValue);
        EXPECT_EQ(scheduler.driveStrength(0, {8, 8}), Error::InvalidValue);
        EXPECT_EQ(scheduler.setStrengthEdgeDelayed(0, static_cast<Conduction>(99), 2, 2),
                  Error::InvalidValue);
        EXPECT_EQ(scheduler.setStrengthEdge(0, static_cast<Conduction>(99)), Error::InvalidValue);
        EXPECT_EQ(notices.used, 0U);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(notices.used, 0U);
    }
}

TEST(ReportPulses, OverflowedReplacementDoesNotNotify) {
    for (const auto kind : {Kind::Drive, Kind::StrengthDrive, Kind::StrengthEdge}) {
        Fixture fixture{kind};
        Notices notices{&fixture.context};
        fixture.context.scheduler().setPulseHook(notices.hook());
        constexpr auto at = std::numeric_limits<std::uint64_t>::max() - 2;
        Script script{fixture, {{at, 1, 1}, {at, 2, 3}}};
        script.start();
        ASSERT_EQ(fixture.context.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(script.errors, kind == Kind::Drive ? 0U : 1U);
        EXPECT_TRUE(fixture.context.scheduler().takeOverflow());
        EXPECT_EQ(notices.used, 0U);
    }
}

TEST(ReportPulses, EndingWithPendingUpdatesDoesNotReportCleanupAsAPulse) {
    for (const auto kind : {Kind::Drive, Kind::StrengthDrive, Kind::StrengthEdge}) {
        Notices notices;
        {
            Fixture fixture{kind};
            notices.context = &fixture.context;
            fixture.context.scheduler().setPulseHook(notices.hook());
            ASSERT_EQ(fixture.drive(1, 5), Error::None);
            fixture.context.scheduler().requestEnd(k::EndReason::Finish, 0);
            EXPECT_EQ(fixture.context.scheduler().run(), k::EndReason::Finish);
            EXPECT_EQ(notices.used, 0U);
        }
        EXPECT_EQ(notices.used, 0U);
    }
}
}  // namespace
