// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original ownership checks for RUNTIME_ABI 6 and automatic-capture 5/6/8.
// Explicit registry collection here does not enable VM instruction collection.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "event_transfers.h"
#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using kirimasim::vm::detail::EventTransfers;

struct Memory : std::pmr::memory_resource {
    std::size_t calls = 0, liveBytes = 0, failAt = SIZE_MAX;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (calls++ == failAt) throw std::bad_alloc();
        void* out = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        liveBytes += bytes;
        return out;
    }
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        liveBytes -= bytes;
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
k::ManagedEventToken create(k::Context& context) {
    auto pin = context.managedEvents().create();
    EXPECT_TRUE(pin);
    return pin ? pin.value.token() : k::ManagedEventToken{};
}
std::vector<k::W4> words(std::span<const k::ManagedEventToken> tokens) {
    std::vector<k::W4> out;
    for (const auto token : tokens)
        out.push_back({token.bits, 0});
    return out;
}

TEST(EventTransfers, ScalarNullAndLegacyNeedNoAllocationAndReleaseAtScopeExit) {
    k::Context context;
    Memory memory;
    memory.failAt = 0;
    const auto token = create(context);
    const std::array<k::W4, 3> input{
        {{0, 0}, {k::EventValue::legacy(k::EventId{3}).bits, 0}, {token.bits, 0}}};
    {
        EventTransfers owner{context.managedEvents(), &memory};
        ASSERT_EQ(owner.retain(input), k::ManagedEventError::None);
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_TRUE(context.managedEvents().valid(token));
        EXPECT_EQ(owner.size(), 1U);
        EXPECT_EQ(memory.calls, 0U);
    }
    ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_FALSE(context.managedEvents().valid(token));
    EXPECT_EQ(memory.liveBytes, 0U);
}

TEST(EventTransfers, TypedArrayParametersOwnSnapshotsWithoutGuessingOpaqueAliases) {
    k::Context context;
    std::array<k::ManagedEventToken, 9> tokens;
    for (auto& token : tokens)
        token = create(context);
    std::vector<std::vector<k::W4>> args{
        words(std::span(tokens).first(2)), words(std::span(tokens).subspan(2, 3)),
        words(std::span(tokens).subspan(5, 2)), words(std::span(tokens).subspan(7, 2))};
    const std::array<std::uint32_t, 2> eventParameters{1, 3};
    {
        EventTransfers owner{context.managedEvents()};
        ASSERT_EQ(owner.retainArguments(args, eventParameters), k::ManagedEventError::None);
        // A callback can replace every source/argument buffer after capture.
        args.clear();
        args.resize(1024, {{41, 73}});
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        for (unsigned i = 0; i < tokens.size(); ++i)
            EXPECT_EQ(context.managedEvents().valid(tokens[i]), (i >= 2 && i <= 4) || i >= 7);
        EXPECT_EQ(owner.size(), 5U);
        EXPECT_EQ(context.managedEvents().liveObjects(), 5U);
    }
    ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(EventTransfers, NestedTransfersAndRuntimePinsReleaseIndependentOwnership) {
    k::Context context;
    const auto outer = create(context), inner = create(context);
    auto pending = context.managedEvents().create();
    ASSERT_TRUE(pending);
    const auto delayed = pending.value.token();
    {
        EventTransfers caller{context.managedEvents()};
        ASSERT_EQ(caller.retain(std::array{k::W4{outer.bits, 0}}), k::ManagedEventError::None);
        {
            EventTransfers callback{context.managedEvents()};
            ASSERT_EQ(callback.retain(std::array{k::W4{inner.bits, 0}, k::W4{outer.bits, 0}}),
                      k::ManagedEventError::None);
            ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 3U);
        }
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_TRUE(context.managedEvents().valid(outer));
        EXPECT_FALSE(context.managedEvents().valid(inner));
        EXPECT_TRUE(context.managedEvents().valid(delayed));
        caller.clear();
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_FALSE(context.managedEvents().valid(outer));
        EXPECT_TRUE(context.managedEvents().valid(delayed));
    }
    pending.value.reset();
    ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(EventTransfers, StaleValuesAndMalformedPlansRollBackOnlyTheNewBatch) {
    for (unsigned shape = 0; shape < 4; ++shape) {
        SCOPED_TRACE(shape);
        k::Context context;
        const auto stale = create(context);
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        const auto old = create(context), fresh = create(context);
        EventTransfers owner{context.managedEvents()};
        ASSERT_EQ(owner.retain(std::array{k::W4{old.bits, 0}}), k::ManagedEventError::None);
        std::vector<std::vector<k::W4>> args{{{fresh.bits, 0}}, {{stale.bits, 0}}};
        std::array<std::uint32_t, 2> plan{0, 1};
        if (shape == 1) plan = {0, 2};
        if (shape == 2) plan = {0, 0};
        if (shape == 3) {
            plan = {1, 0};
            args[1] = {{fresh.bits, 0}};
        }
        const auto original = args;
        EXPECT_EQ(owner.retainArguments(args, plan), shape == 0
                                                         ? k::ManagedEventError::InvalidToken
                                                         : k::ManagedEventError::InvalidBackend);
        EXPECT_EQ(owner.size(), 1U);
        for (unsigned i = 0; i < args.size(); ++i)
            EXPECT_EQ(args[i][0].a, original[i][0].a);
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        EXPECT_TRUE(context.managedEvents().valid(old));
        EXPECT_FALSE(context.managedEvents().valid(fresh));
    }
}

TEST(EventTransfers, SpillAllocationFailureRollsBackEveryPartOfTheArgumentBatch) {
    for (const bool previous : {false, true})
        for (const std::size_t failAt : {0U, 1U, 2U}) {
            SCOPED_TRACE(::testing::Message() << previous << '/' << failAt);
            k::Context context;
            Memory memory;
            memory.failAt = failAt;
            std::array<k::ManagedEventToken, 10> tokens;
            for (auto& token : tokens)
                token = create(context);
            {
                EventTransfers owner{context.managedEvents(), &memory};
                if (previous)
                    ASSERT_EQ(owner.retain(std::array{k::W4{tokens[0].bits, 0}}),
                              k::ManagedEventError::None);
                const std::vector<std::vector<k::W4>> args{words(std::span(tokens).subspan(1, 3)),
                                                           words(std::span(tokens).subspan(4))};
                EXPECT_EQ(owner.retainArguments(args, std::array<std::uint32_t, 2>{0, 1}),
                          k::ManagedEventError::ResourceLimit);
                EXPECT_EQ(owner.size(), previous ? 1U : 0U);
                ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
                for (unsigned i = 0; i < tokens.size(); ++i)
                    EXPECT_EQ(context.managedEvents().valid(tokens[i]), previous && i == 0);
            }
            EXPECT_EQ(memory.liveBytes, 0U);
        }
}

TEST(EventTransfers, RepeatedExceptionalTransfersReuseRowsAndNeverRetainInactiveValues) {
    k::Context context;
    for (unsigned i = 0; i < 1024; ++i) {
        const std::array tokens{create(context), create(context), create(context)};
        try {
            EventTransfers owner{context.managedEvents()};
            ASSERT_EQ(owner.retain(words(tokens)), k::ManagedEventError::None);
            ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 3U);
            throw i;
        } catch (unsigned caught) {
            EXPECT_EQ(caught, i);
        }
        ASSERT_EQ(context.managedEvents().collect(), k::ManagedEventError::None);
        for (const auto token : tokens)
            EXPECT_FALSE(context.managedEvents().valid(token));
        EXPECT_EQ(context.managedEvents().allocatedObjects(), 3U);
    }
}
}  // namespace
