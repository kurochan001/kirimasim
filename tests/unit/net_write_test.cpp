// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Independent physical-state traces for approved whole-net contract 2-7.
#include <array>
#include <memory_resource>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using R = k::StrengthRange;
using E = k::StrengthError;
using K = k::NetWriteKind;

k::SlotId point(k::Context& ctx, std::uint32_t width, unsigned drivers = 1) {
    const auto slot = ctx.store().add(k::Domain::Four, width, 1, k::Init::Z);
    EXPECT_EQ(ctx.scheduler().enableStrengths(slot, std::vector<R>(width)), E::None);
    std::vector<k::StrengthNodeSpec> nodes;
    std::vector<k::StrengthOriginSpec> origins;
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        nodes.push_back({{slot, 0, bit, 1}, k::NetKind::Wire, 0});
        for (unsigned driver = 0; driver < drivers; ++driver)
            origins.push_back({bit, driver == 1 ? R{6, 6} : R{-6, -6}});
    }
    EXPECT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, origins), E::None);
    EXPECT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    return slot;
}
std::vector<k::W4> value(unsigned width, unsigned logic) {
    std::vector<k::W4> words((width - 1) / 64 + 1);
    for (unsigned bit = 0; bit < width; ++bit) {
        words[bit / 64].a |= std::uint64_t{logic & 1U} << (bit % 64);
        words[bit / 64].b |= std::uint64_t{logic >> 1U} << (bit % 64);
    }
    return words;
}
std::vector<R> ranges(k::Context& ctx, k::SlotId slot, unsigned width) {
    std::vector<R> out(width);
    EXPECT_EQ(ctx.strengths().read({slot, 0, 0, width}, out), E::None);
    return out;
}
TEST(NetWrites, WholeRangeFourStatesKeepStrengthSnapshotsAndOriginalDrivers) {
    constexpr std::array<R, 4> expected{{{-6, -6}, {6, 6}, {}, {-6, 6}}};
    for (const auto width : {1U, 32U, 33U, 65U, 129U, 257U})
        for (unsigned logic = 0; logic < 4; ++logic) {
            k::Context ctx;
            const auto slot = point(ctx, width);
            const auto bits = value(width, logic);
            const auto result = ctx.scheduler().writeNet({slot}, bits, K::Deposit);
            ASSERT_EQ(result.error, E::None);
            EXPECT_TRUE(result.committed);
            EXPECT_EQ(ranges(ctx, slot, width), std::vector<R>(width, expected[logic]));
            EXPECT_EQ(ctx.strengthDrivers().originCount(), width);
            for (unsigned bit = 0; bit < width; ++bit) {
                const auto origin = ctx.strengthDrivers().origin(bit);
                ASSERT_TRUE(origin.has_value());
                EXPECT_EQ(origin.value_or(k::StrengthOriginState{}).committed, (R{-6, -6}));
            }
            EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
            EXPECT_EQ(ranges(ctx, slot, width), std::vector<R>(width, expected[logic]));
        }
}
TEST(NetWrites, OneChangedBitInvalidatesTheWholePhysicalVectorDeposit) {
    for (const auto width : {33U, 65U, 129U, 257U}) {
        k::Context ctx;
        const auto slot = point(ctx, width);
        ASSERT_EQ(ctx.scheduler().writeNet({slot}, value(width, 1), K::Deposit).error, E::None);
        ASSERT_EQ(ctx.scheduler().driveStrength(0, {6, 6}), E::None);
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        std::vector<R> expected(width, {-6, -6});
        expected[0] = {6, 6};
        EXPECT_EQ(ranges(ctx, slot, width), expected);
        for (unsigned bit = 0; bit < width; ++bit)
            EXPECT_FALSE(ctx.strengthDrivers().deposited(bit).has_value());
    }
}
TEST(NetWrites, ActualDriverChangeClearsDepositEvenWhenResolvedResultIsUnchanged) {
    k::Context ctx;
    const auto slot = point(ctx, 1, 3);
    EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{-6, 6}}));
    ASSERT_EQ(ctx.scheduler().writeNet({slot}, value(1, 1), K::Deposit).error, E::None);
    EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{6, 6}}));
    ASSERT_EQ(ctx.scheduler().driveStrength(2, {6, 6}), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{-6, 6}}));
}
TEST(NetWrites, IdenticalContributionAndCanceledPendingWorkDoNotClearDeposit) {
    k::Context ctx;
    const auto slot = point(ctx, 1);
    ASSERT_EQ(ctx.scheduler().writeNet({slot}, value(1, 1), K::Deposit).error, E::None);
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {-6, -6}), E::None);
    EXPECT_TRUE(ctx.strengthDrivers().deposited(0).has_value());
    ASSERT_EQ(ctx.scheduler().driveStrengthDelayed(0, {6, 6}, 5, 5, 5), E::None);
    ASSERT_EQ(ctx.scheduler().driveStrengthDelayed(0, {-6, -6}, 5, 5, 5), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{6, 6}}));
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {6, 6}), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    ASSERT_EQ(ctx.scheduler().driveStrength(0, {-6, -6}), E::None);
    EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{-6, -6}}));
}
TEST(NetWrites, ForceDominatesDepositAndReleaseUsesCurrentCommittedDrivers) {
    for (const auto width : {1U, 33U, 129U}) {
        k::Context ctx;
        const auto slot = point(ctx, width);
        ASSERT_EQ(ctx.scheduler().writeNet({slot}, value(width, 1), K::Force).error, E::None);
        EXPECT_TRUE(ctx.scheduler().holdsAny(k::Hold::Force, k::ProcessId{UINT32_MAX - 1}));
        ASSERT_EQ(ctx.scheduler().writeNet({slot}, value(width, 0), K::Deposit).error, E::None);
        EXPECT_EQ(ranges(ctx, slot, width), std::vector<R>(width, {6, 6}));
        for (unsigned bit = 0; bit < width; ++bit)
            ASSERT_EQ(ctx.scheduler().driveStrength(bit, {6, 6}), E::None);
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        ASSERT_EQ(ctx.scheduler().writeNet({slot}, {}, K::Release).error, E::None);
        EXPECT_EQ(ranges(ctx, slot, width), std::vector<R>(width, {6, 6}));
        for (unsigned bit = 0; bit < width; ++bit)
            ASSERT_EQ(ctx.scheduler().driveStrength(bit, {-6, -6}), E::None);
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(ranges(ctx, slot, width), std::vector<R>(width, {-6, -6}));
        EXPECT_FALSE(ctx.scheduler().holdsAny(k::Hold::Force, k::ProcessId{UINT32_MAX - 1}));
    }
}
TEST(NetWrites, DelayedDriverCommitInvalidatesDepositAtArrivalAndContinuesUnderForce) {
    for (const bool forced : {false, true}) {
        k::Context ctx;
        const auto slot = point(ctx, 1);
        ASSERT_EQ(ctx.scheduler().driveStrengthDelayed(0, {6, 6}, 5, 5, 5), E::None);
        ASSERT_EQ(
            ctx.scheduler().writeNet({slot}, value(1, 0), forced ? K::Force : K::Deposit).error,
            E::None);
        EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{-6, -6}}));
        EXPECT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(ctx.scheduler().time(), 5U);
        if (forced) {
            EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{-6, -6}}));
            ASSERT_EQ(ctx.scheduler().writeNet({slot}, {}, K::Release).error, E::None);
        }
        EXPECT_EQ(ranges(ctx, slot, 1), (std::vector<R>{{6, 6}}));
    }
}
TEST(NetWrites, PassiveForceUsesTheExistingNetworkAndRejectsDepositsWithoutMutation) {
    for (const auto transfer :
         {k::StrengthTransfer::Nonresistive, k::StrengthTransfer::Resistive}) {
        k::Context ctx;
        const auto a = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        const auto b = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        const R zero{};
        ASSERT_EQ(ctx.scheduler().enableStrengths(a, {&zero, 1}), E::None);
        ASSERT_EQ(ctx.scheduler().enableStrengths(b, {&zero, 1}), E::None);
        const std::array<k::StrengthNodeSpec, 2> nodes{{{{a, 0, 0, 1}}, {{b, 0, 0, 1}}}};
        const k::StrengthOriginSpec origin{0, {-6, -6}};
        const k::StrengthEdgeSpec edge{0, 1, transfer};
        ASSERT_EQ(ctx.scheduler().configureStrengthDrivers(nodes, {&origin, 1}, {&edge, 1}),
                  E::None);
        ASSERT_EQ(ctx.scheduler().startStrengthDrivers(), E::None);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(ctx.scheduler().writeNet({b}, value(1, 1), K::Deposit).error, E::UnsupportedHold);
        EXPECT_EQ(ranges(ctx, b, 1),
                  (std::vector<R>{transfer == k::StrengthTransfer::Nonresistive ? R{-6, -6}
                                                                                : R{-5, -5}}));
        ASSERT_EQ(ctx.scheduler().writeNet({b}, value(1, 1), K::Force).error, E::None);
        EXPECT_EQ(ranges(ctx, b, 1), (std::vector<R>{{6, 6}}));
        EXPECT_EQ(
            ranges(ctx, a, 1),
            (std::vector<R>{transfer == k::StrengthTransfer::Nonresistive ? R{-6, 6} : R{-6, -6}}));
        ASSERT_EQ(ctx.scheduler().writeNet({b}, {}, K::Release).error, E::None);
        EXPECT_EQ(ranges(ctx, a, 1), (std::vector<R>{{-6, -6}}));
        EXPECT_EQ(ranges(ctx, b, 1),
                  (std::vector<R>{transfer == k::StrengthTransfer::Nonresistive ? R{-6, -6}
                                                                                : R{-5, -5}}));
    }
}
TEST(NetWrites, InvalidUnregisteredPartialAndWrongShapesPreserveState) {
    k::Context ctx;
    const auto slot = point(ctx, 33);
    const auto plain = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Zero);
    for (const auto kind : {K::Deposit, K::Force, K::Release}) {
        EXPECT_EQ(ctx.scheduler().writeNet({plain}, value(1, 1), kind).error, E::InvalidTarget);
        EXPECT_EQ(ctx.scheduler().writeNet({slot, 0, 1, 32}, value(33, 1), kind).error,
                  E::InvalidTarget);
        EXPECT_EQ(ctx.scheduler().writeNet({slot, 1, 0, 33}, value(33, 1), kind).error,
                  E::InvalidTarget);
    }
    EXPECT_EQ(ctx.scheduler().writeNet({slot}, {}, K::Force).error, E::InvalidShape);
    EXPECT_EQ(ctx.scheduler().writeNet({slot}, value(33, 1), static_cast<K>(99)).error,
              E::InvalidValue);
    EXPECT_EQ(ranges(ctx, slot, 33), std::vector<R>(33, {-6, -6}));
}
class FailingMemory final : public std::pmr::memory_resource {
public:
    std::size_t remaining = SIZE_MAX;

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        if (remaining != SIZE_MAX) --remaining;
        return std::pmr::new_delete_resource()->allocate(size, alignment);
    }
    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(p, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(NetWrites, PublicationAllocationFailuresDoNotCommitDepositsForcesOrOwnership) {
    unsigned failures = 0, successes = 0;
    for (const auto kind : {K::Deposit, K::Force})
        for (std::size_t budget = 0; budget <= 128; ++budget) {
            FailingMemory memory;
            k::Context ctx({nullptr, nullptr}, nullptr, &memory);
            const auto slot = point(ctx, 33);
            memory.remaining = budget == 128 ? SIZE_MAX : budget;
            const auto result = ctx.scheduler().writeNet({slot}, value(33, 1), kind);
            if (result.error == E::ResourceLimit) {
                ++failures;
                EXPECT_FALSE(result.committed);
                EXPECT_EQ(ranges(ctx, slot, 33), std::vector<R>(33, {-6, -6}));
                EXPECT_FALSE(
                    ctx.scheduler().holdsAny(k::Hold::Force, k::ProcessId{UINT32_MAX - 1}));
                for (unsigned bit = 0; bit < 33; ++bit) {
                    EXPECT_FALSE(ctx.strengthDrivers().forced(bit).has_value());
                    EXPECT_FALSE(ctx.strengthDrivers().deposited(bit).has_value());
                }
            } else {
                ++successes;
                EXPECT_EQ(result.error, E::None);
                EXPECT_TRUE(result.committed);
            }
        }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}
}  // namespace
