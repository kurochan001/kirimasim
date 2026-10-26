// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original component checks for RUNTIME_ABI 6's approved event-only amendment.
// Explicit collection here does not enable or prove VM instruction safepoints.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "event_temporaries.h"
#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using kirimasim::vm::detail::EventTemporaries;

k::ManagedEventError collect(k::Context& context, const EventTemporaries& temps) {
    const std::array providers{temps.provider()};
    return context.managedEvents().collect(providers);
}
k::ManagedEventToken create(k::Context& context, k::W4& word) {
    auto value = context.managedEvents().create();
    EXPECT_TRUE(value);
    if (!value) return {};
    const auto token = value.value.token();
    word = {token.bits, 0};
    return token;
}

TEST(EventTemporaries, TracesDeclaredWordsAndNeverGuessesOpaqueBits) {
    k::Context context;
    std::vector<k::W4> words(12, {19, 31});
    EventTemporaries temps{words};
    const std::array<std::uint32_t, 3> offsets{1, 4, 9};
    std::array<k::ManagedEventToken, 3> kept;
    k::ManagedEventToken numeric;
    {
        EventTemporaries::Region region{temps, 0, words.size(), offsets};
        for (unsigned i = 0; i < offsets.size(); ++i) {
            EXPECT_EQ(words[offsets[i]].a, 0U);
            EXPECT_EQ(words[offsets[i]].b, 0U);
            kept[i] = create(context, words[offsets[i]]);
        }
        numeric = create(context, words[0]);
        words[2] = words[0];  // Numeric/ref/string-shaped aliases are not roots.
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        for (const auto token : kept)
            EXPECT_TRUE(context.managedEvents().valid(token));
        EXPECT_FALSE(context.managedEvents().valid(numeric));
        EXPECT_EQ(context.managedEvents().liveObjects(), kept.size());
    }
    for (const auto offset : offsets) {
        EXPECT_EQ(words[offset].a, 0U);
        EXPECT_EQ(words[offset].b, 0U);
    }
    EXPECT_EQ(words[0].a, numeric.bits);
    EXPECT_EQ(words[2].a, numeric.bits);
    EXPECT_EQ(words[3].a, 19U);
    EXPECT_EQ(words[3].b, 31U);
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(EventTemporaries, NestedCallbackKeepsCallerRootsAcrossVectorRelocation) {
    k::Context context;
    std::vector<k::W4> words(16);
    EventTemporaries temps{words};
    const std::array<std::uint32_t, 2> offsets{0, 3};
    std::array<k::ManagedEventToken, 2> outer;
    std::array<k::ManagedEventToken, 2> inner;
    {
        EventTemporaries::Region caller{temps, 2, 8, offsets};
        outer[0] = create(context, words[2]);
        outer[1] = create(context, words[5]);
        const auto capacity = words.capacity();
        words.resize(capacity + 2048);  // Force relocation while the caller is published.
        ASSERT_GT(words.capacity(), capacity);
        {
            EventTemporaries::Region callback{temps, 16, 8, offsets};
            inner[0] = create(context, words[16]);
            inner[1] = create(context, words[19]);
            // An event-free recursive body must leave the callback/caller visible.
            EventTemporaries::Region numeric{temps, 24, 100, {}};
            ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
            for (const auto token : outer)
                EXPECT_TRUE(context.managedEvents().valid(token));
            for (const auto token : inner)
                EXPECT_TRUE(context.managedEvents().valid(token));
            EXPECT_EQ(context.managedEvents().liveObjects(), 4U);
        }
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        for (const auto token : outer)
            EXPECT_TRUE(context.managedEvents().valid(token));
        for (const auto token : inner)
            EXPECT_FALSE(context.managedEvents().valid(token));
        EXPECT_EQ(context.managedEvents().liveObjects(), 2U);
    }
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
}

TEST(EventTemporaries, RecursiveRegionsRetireInOrderWithNoNativePointersIntoWords) {
    k::Context context;
    std::vector<k::W4> words;
    EventTemporaries temps{words};
    const std::array<std::uint32_t, 1> offsets{2};
    const auto descend = [&](auto&& recurse, std::size_t depth) -> void {
        words.resize((depth + 1) * 7);
        EventTemporaries::Region region{temps, depth * 7, 7, offsets};
        const auto token = create(context, words[depth * 7 + 2]);
        if (depth < 127) recurse(recurse, depth + 1);
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        EXPECT_EQ(context.managedEvents().liveObjects(), depth + 1);
        EXPECT_TRUE(context.managedEvents().valid(token));
        EXPECT_EQ(words[depth * 7 + 2].a, token.bits);
    };
    descend(descend, 0);
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
    EXPECT_EQ(context.managedEvents().allocatedObjects(), 128U);
}

TEST(EventTemporaries, UnwindAndRepeatedReentryDropOnlyInactiveEventWords) {
    k::Context context;
    std::vector<k::W4> words(64, {27, 13});
    EventTemporaries temps{words};
    const std::array<std::uint32_t, 1> offsets{1};
    k::ManagedEventToken previous;
    for (unsigned iteration = 0; iteration < 1024; ++iteration) {
        SCOPED_TRACE(iteration);
        try {
            EventTemporaries::Region region{temps, 0, 8, offsets};
            EXPECT_EQ(words[1].a, 0U);
            const auto token = create(context, words[1]);
            EXPECT_NE(token, previous);
            previous = token;
            {
                EventTemporaries::Region callback{temps, 8, 8, offsets};
                create(context, words[9]);
                ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
                EXPECT_EQ(context.managedEvents().liveObjects(), 2U);
                if (iteration % 2 == 0) throw iteration;
            }
            ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
            EXPECT_EQ(context.managedEvents().liveObjects(), 1U);
        } catch (unsigned caught) {
            EXPECT_EQ(caught, iteration);
        }
        EXPECT_EQ(words[1].a, 0U);
        EXPECT_EQ(words[9].a, 0U);
        EXPECT_EQ(words[2].a, 27U);
        EXPECT_EQ(words[2].b, 13U);
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        EXPECT_FALSE(context.managedEvents().valid(previous));
        EXPECT_EQ(context.managedEvents().liveObjects(), 0U);
        EXPECT_EQ(context.managedEvents().allocatedObjects(), 2U);
    }
}

TEST(EventTemporaries, StaleTypedRootAbortsTheWholeSweep) {
    k::Context context;
    std::vector<k::W4> words(8);
    EventTemporaries temps{words};
    auto stale = create(context, words[4]);
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    const std::array<std::uint32_t, 2> offsets{1, 3};
    EventTemporaries::Region region{temps, 0, 8, offsets};
    const auto current = create(context, words[1]);
    const auto unrooted = create(context, words[4]);
    words[3] = {stale.bits, 0};
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::InvalidToken);
    EXPECT_TRUE(context.managedEvents().valid(current));
    EXPECT_TRUE(context.managedEvents().valid(unrooted));
    EXPECT_EQ(context.managedEvents().liveObjects(), 2U);
    words[3] = {};
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_TRUE(context.managedEvents().valid(current));
    EXPECT_FALSE(context.managedEvents().valid(unrooted));
}

TEST(EventTemporaries, MalformedRegionsNeverSweepAPartialRootSet) {
    for (unsigned shape = 0; shape < 7; ++shape) {
        SCOPED_TRACE(shape);
        k::Context context;
        std::vector<k::W4> words(16);
        EventTemporaries temps{words};
        const auto held = create(context, words[0]);
        const std::array<std::uint32_t, 1> outerOffsets{1};
        EventTemporaries::Region outer{temps, 0, 4, outerOffsets};
        std::array<std::uint32_t, 2> offsets{0, 2};
        std::size_t base = 4, size = 4;
        switch (shape) {
            case 0: base = std::numeric_limits<std::size_t>::max(); break;
            case 1: size = std::numeric_limits<std::size_t>::max(); break;
            case 2: offsets[1] = 4; break;
            case 3: offsets[1] = 0; break;
            case 4: offsets = {2, 1}; break;
            case 5: base = 2; break;  // Aliases a still-active caller region.
            case 6: break;            // The published vector is truncated below.
            default: FAIL() << "unknown malformed region shape";
        }
        {
            EventTemporaries::Region bad{temps, base, size, offsets};
            if (shape == 6) words.resize(4);
            ASSERT_EQ(collect(context, temps), k::ManagedEventError::InvalidBackend);
            EXPECT_TRUE(context.managedEvents().valid(held));
            EXPECT_EQ(context.managedEvents().liveObjects(), 1U);
        }
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        EXPECT_FALSE(context.managedEvents().valid(held));
    }
}

TEST(EventTemporaries, LegacyNullAndInFlightPinsKeepTheirDifferentOwnershipRules) {
    k::Context context;
    std::vector<k::W4> words(4);
    EventTemporaries temps{words};
    const std::array<std::uint32_t, 2> offsets{0, 1};
    auto pending = context.managedEvents().create();
    ASSERT_TRUE(pending);
    const auto token = pending.value.token();
    {
        EventTemporaries::Region region{temps, 0, 4, offsets};
        words[0] = {k::EventValue::legacy(k::EventId{0}).bits, 0};
        words[1] = {};
        ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
        EXPECT_TRUE(context.managedEvents().valid(token));
    }
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_TRUE(context.managedEvents().valid(token));
    pending.value.reset();
    ASSERT_EQ(collect(context, temps), k::ManagedEventError::None);
    EXPECT_FALSE(context.managedEvents().valid(token));
}
}  // namespace
