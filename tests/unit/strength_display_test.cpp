// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Original subscription and hold-bridge checks for MOS/switch contract 5/7/8.
#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/format.h"
#include "kirimasim/kernel/strength_value.h"
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {
namespace k = kirimasim::kernel;
using R = k::StrengthRange;
struct Display {
    k::SlotId slot;
    unsigned bit = 0;
    std::vector<R> values;
    bool valid = true;
    static kirima_status resume(kirima_context* raw, void* frame) {
        auto& self = *static_cast<Display*>(frame);
        auto& ctx = k::fromC(raw);
        const k::SlotBits at{self.slot, 0, self.bit, 1};
        R range;
        self.valid &= ctx.scheduler().inPostponed();
        self.valid &= ctx.scheduler().trackDisplayStrength(at);
        self.valid &= ctx.scheduler().trackDisplayStrength(at);  // Duplicate reads own one watch.
        self.valid &= ctx.strengths().read(at, {&range, 1}) == k::StrengthError::None;
        self.values.push_back(range);
        return KIRIMA_DONE;
    }
    static kirima_status tick(kirima_context*, void*) { return KIRIMA_DONE; }
};
TEST(StrengthDisplay, SelectedIntervalChangesArePostponedAndDependenciesAreReplacedAndDetached) {
    for (const bool file : {false, true}) {
        k::Context ctx;
        auto& scheduler = ctx.scheduler();
        const auto slot = ctx.store().add(k::Domain::Four, 2, 1, k::Init::Z);
        ASSERT_EQ(scheduler.enableStrengths(slot, std::array<R, 2>{}), k::StrengthError::None);
        const std::array<k::StrengthNodeSpec, 2> nodes{{{{slot, 0, 0, 1}}, {{slot, 0, 1, 1}}}};
        const std::array<k::StrengthOriginSpec, 2> origins{{{0, {3, 3}}, {1, {3, 3}}}};
        ASSERT_EQ(scheduler.configureStrengthDrivers(nodes, origins, {}), k::StrengthError::None);
        ASSERT_EQ(scheduler.startStrengthDrivers(), k::StrengthError::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        Display first{slot}, second{slot, 1};
        const auto p = scheduler.addProcess(&Display::resume, &first, k::Group::Active);
        const auto q = scheduler.addProcess(&Display::resume, &second, k::Group::Active);
        const auto clock = scheduler.addProcess(&Display::tick, nullptr, k::Group::Active);
        if (file)
            scheduler.addFileMonitor(p, {}, ctx.files().targets({1, 0}), {.strength = true});
        else
            scheduler.setMonitor(p, {}, {.strength = true});
        scheduler.start(clock);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        ASSERT_EQ(first.values, (std::vector<R>{{3, 3}}));
        EXPECT_EQ(ctx.strengths().liveWatches(), 1U);
        ASSERT_EQ(scheduler.driveStrength(1, {6, 6}), k::StrengthError::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(first.values.size(), 1U);  // An unrelated bit never evaluates the display.
        ASSERT_EQ(scheduler.driveStrength(0, {3, 6}), k::StrengthError::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(first.values, (std::vector<R>{{3, 3}, {3, 6}}));
        // Reinstall after selection changes. The old selected bit loses its watch.
        first.bit = 1;
        if (file)
            scheduler.addFileMonitor(q, {}, ctx.files().targets({1, 0}), {.strength = true});
        else
            scheduler.setMonitor(q, {}, {.strength = true});
        scheduler.start(clock);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(second.values, (std::vector<R>{{6, 6}}));
        EXPECT_EQ(ctx.strengths().liveWatches(), file ? 2U : 1U);
        scheduler.kill(p);
        EXPECT_EQ(ctx.strengths().liveWatches(), 1U);
        ASSERT_EQ(scheduler.driveStrength(0, {5, 5}), k::StrengthError::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(second.values.size(), 1U);
        ASSERT_EQ(scheduler.driveStrength(1, {7, 7}), k::StrengthError::None);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(second.values, (std::vector<R>{{6, 6}, {7, 7}}));
        scheduler.kill(q);
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
        EXPECT_TRUE(first.valid && second.valid);
    }
}
TEST(StrengthDisplay, HoldBridgeRetainsPerBitOwnerAndReleaseReadsCurrentRoots) {
    k::Context ctx;
    auto& scheduler = ctx.scheduler();
    const auto slot = ctx.store().add(k::Domain::Four, 65, 1, k::Init::Z);
    scheduler.markHeld(slot, true, true, true);
    ASSERT_EQ(scheduler.enableStrengths(slot, std::vector<R>(65)), k::StrengthError::None);
    std::vector<k::StrengthNodeSpec> nodes;
    std::vector<k::StrengthOriginSpec> roots;
    for (unsigned bit = 0; bit < 65; ++bit) {
        nodes.push_back({{slot, 0, bit, 1}});
        roots.push_back({bit, {3, 3}});
    }
    ASSERT_EQ(scheduler.configureStrengthDrivers(nodes, roots, {}), k::StrengthError::None);
    ASSERT_EQ(scheduler.startStrengthDrivers(), k::StrengthError::None);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    const auto p = scheduler.addProcess(&Display::tick, nullptr, k::Group::Active);
    const auto q = scheduler.addProcess(&Display::tick, nullptr, k::Group::Active);
    const std::array<std::uint64_t, 4> ones{UINT64_MAX, 0, 1, 0}, zeros{};
    scheduler.hold(k::Hold::Force, p, {slot, 0, 1, 0, 65});
    scheduler.writeHeld(k::Hold::Force, p, {slot, 0, 0, 65}, ones.data());
    scheduler.hold(k::Hold::Force, q, {slot, 0, 1, 64, 1});
    scheduler.writeHeld(k::Hold::Force, q, {slot, 0, 64, 1}, zeros.data());
    scheduler.writeHeld(k::Hold::Force, p, {slot, 0, 0, 65}, ones.data());
    const auto lowerForce = ctx.strengthDrivers().forced(63);
    const auto upperForce = ctx.strengthDrivers().forced(64);
    ASSERT_TRUE(lowerForce);
    ASSERT_TRUE(upperForce);
    if (lowerForce && upperForce) {
        EXPECT_EQ(lowerForce->a, 1U);
        EXPECT_EQ(upperForce->a, 0U);
    }
    ASSERT_EQ(scheduler.driveStrength(64, {-5, -5}), k::StrengthError::None);
    ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
    scheduler.release(k::Hold::Force, {slot, 0, 1, 64, 1});
    R actual;
    ASSERT_EQ(ctx.strengths().read({slot, 0, 64, 1}, {&actual, 1}), k::StrengthError::None);
    EXPECT_EQ(actual, (R{-5, -5}));  // Synchronous, no extra settle/run.
    EXPECT_FALSE(ctx.strengthDrivers().forced(64));
    EXPECT_TRUE(ctx.strengthDrivers().forced(63));
    scheduler.release(k::Hold::Force, {slot, 0, 1, 0, 65});
    for (unsigned bit = 0; bit < 65; ++bit)
        EXPECT_FALSE(ctx.strengthDrivers().forced(bit));
}
}  // namespace
