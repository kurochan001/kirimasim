// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original range-read/wait mechanisms, MOS/switch contract 6/7 and
// VALUE_SEMANTICS 3.11/5. No source-language enabling here.
#include <array>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;
using R = k::StrengthRange;
using E = k::StrengthError;
k::SlotId add(k::Context& ctx, unsigned width = 1, unsigned elements = 1) {
    const auto id = ctx.store().add(k::Domain::Four, width, elements, k::Init::Z);
    EXPECT_EQ(ctx.scheduler().enableStrengths(id, std::vector<R>(std::size_t{width} * elements)),
              E::None);
    return id;
}
void publish(k::Context& ctx, k::SlotBits at, std::span<const R> values) {
    const k::StrengthUpdate update{at, values};
    EXPECT_EQ(ctx.scheduler().publishStrengths({&update, 1}).error, E::None);
}
void publish(k::Context& ctx, k::SlotId id, R value) { publish(ctx, {id, 0, 0, 1}, {&value, 1}); }
struct Wake {
    unsigned calls = 0;
    static kirima_status resume(kirima_context*, void* raw) {
        ++static_cast<Wake*>(raw)->calls;
        return static_cast<kirima_status>(k::Status::Waiting);
    }
};
TEST(StrengthSelection, CapturedSelectionsPreserveEveryIntervalAndFillOnlyMissingBitsWithStrongX) {
    std::vector<R> ranges;
    for (int lo = -7; lo <= 7; ++lo)
        for (int hi = lo; hi <= 7; ++hi)
            ranges.push_back({static_cast<std::int8_t>(lo), static_cast<std::int8_t>(hi)});
    for (const auto width : {1U, 8U, 9U, 33U, 65U, 129U}) {
        k::Context ctx;
        const auto id = add(ctx, width, 3);
        for (unsigned seed = 0; seed < ranges.size(); ++seed) {
            std::array<std::vector<R>, 3> data;
            for (unsigned e = 0; e < 3; ++e) {
                for (unsigned bit = 0; bit < width; ++bit)
                    data[e].push_back(ranges[(seed + bit + 7 * e) % ranges.size()]);
                publish(ctx, {id, e, 0, width}, data[e]);
            }
            for (const auto windowLsb : {0U, width / 2}) {
                const auto windowWidth = width - windowLsb;
                for (const auto selectedWidth : {1U, 9U, 65U, 129U})
                    for (const std::optional<std::int64_t> position :
                         {std::optional<std::int64_t>{},
                          {-130},
                          {-1},
                          {0},
                          {1},
                          {static_cast<std::int64_t>(windowWidth) - 1},
                          {INT64_MIN},
                          {INT64_MAX}})
                        for (const std::optional<std::uint32_t> element :
                             {std::optional<std::uint32_t>{}, {0}, {2}, {3}, {UINT32_MAX}}) {
                            std::vector<k::W4> packed(k::strengthWordCount(selectedWidth) + 1,
                                                      {71, 73});
                            ASSERT_EQ(ctx.strengths().readSelectedPacked(
                                          {id, 0, windowLsb, windowWidth}, element, position,
                                          selectedWidth, packed),
                                      E::None);
                            for (unsigned bit = 0; bit < selectedWidth; ++bit) {
                                R expected{-6, 6};
                                // Independent checked arithmetic, not the kernel intersection
                                // helper.
                                if (element && *element < 3 && position && *position >= -129 &&
                                    *position <= static_cast<std::int64_t>(width)) {
                                    const auto relative = *position + bit;
                                    if (relative >= 0 && relative < windowWidth)
                                        expected = data[*element][windowLsb +
                                                                  static_cast<unsigned>(relative)];
                                }
                                EXPECT_EQ(k::strengthRangeAt(packed, bit),
                                          std::optional<R>(expected));
                            }
                            if (selectedWidth % 8 != 0) {
                                EXPECT_EQ(packed[packed.size() - 2].a >> (8 * (selectedWidth % 8)),
                                          0U);
                                EXPECT_EQ(packed[packed.size() - 2].b >> (8 * (selectedWidth % 8)),
                                          0U);
                            }
                            EXPECT_EQ(packed.back().a, 71U);
                            EXPECT_EQ(packed.back().b, 73U);
                        }
            }
        }
    }
}
TEST(StrengthSelection, InvalidStaticShapesDoNotWriteAnyOutput) {
    k::Context ctx;
    const auto id = add(ctx, 9);
    std::array<k::W4, 2> words{{{71, 73}, {79, 83}}};
    const auto check = [&] {
        EXPECT_EQ(words[0].a, 71U);
        EXPECT_EQ(words[0].b, 73U);
        EXPECT_EQ(words[1].a, 79U);
        EXPECT_EQ(words[1].b, 83U);
    };
    EXPECT_EQ(ctx.strengths().readSelectedPacked({k::SlotId{999}, 0, 0, 0}, 0, 0, 9, words),
              E::InvalidTarget);
    check();
    EXPECT_EQ(ctx.strengths().readSelectedPacked({id, 0, 9, 1}, 0, 0, 9, words), E::InvalidTarget);
    check();
    EXPECT_EQ(ctx.strengths().readSelectedPacked({id, 0, 0, 9}, 0, 0, 0, words), E::InvalidShape);
    check();
    EXPECT_EQ(ctx.strengths().readSelectedPacked({id, 0, 0, 9}, 0, 0, 17, words), E::InvalidShape);
    check();
}
TEST(StrengthWait, StrengthOnlyChangesWakeOnceAndCancelEveryOrdinaryAlternative) {
    for (const auto group : {k::Group::Active, k::Group::Reactive}) {
        k::Context ctx;
        const auto id = add(ctx);
        publish(ctx, id, {3, 3});
        const auto ordinary = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
        Wake wake;
        const auto process = ctx.scheduler().addProcess(&Wake::resume, &wake, group);
        const k::WaitItem item{{ordinary, 0, 0, 0, 1}};
        const k::SlotPart part{id, 0, 0, 0, 1};
        ASSERT_EQ(ctx.scheduler().waitEventStrength(process, {&item, 1}, {&part, 1}), E::None);
        publish(ctx, id, {3, 6});
        const std::uint64_t one = 1;
        ctx.scheduler().write({ordinary, 0, 0, 1}, &one);
        publish(ctx, id, {6, 6});
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(wake.calls, 1U);
    }
}
TEST(StrengthWait, OrdinaryChangesCancelAllStrengthAlternativesAndArrayRows) {
    k::Context ctx;
    const auto id = add(ctx, 3, 3);
    const auto ordinary = ctx.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
    Wake wake;
    const auto process = ctx.scheduler().addProcess(&Wake::resume, &wake, k::Group::Active);
    const k::WaitItem item{{ordinary, 0, 0, 0, 1}};
    const k::SlotPart part{id, 0, 0, 1, 2};
    ASSERT_EQ(ctx.scheduler().waitEventStrength(process, {&item, 1}, {&part, 1}), E::None);
    EXPECT_EQ(ctx.strengths().liveWatches(), 3U);
    const std::uint64_t one = 1;
    ctx.scheduler().write({ordinary, 0, 0, 1}, &one);
    const R range{6, 6};
    publish(ctx, {id, 2, 2, 1}, {&range, 1});
    ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
    EXPECT_EQ(wake.calls, 1U);
    EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
}
TEST(StrengthWait, WaitReplacementCancellationKillAndProcessReuseDropInternalRegistrations) {
    for (unsigned mode = 0; mode < 5; ++mode) {
        k::Context ctx;
        const auto id = add(ctx);
        Wake old, fresh;
        const auto process = ctx.scheduler().addProcess(&Wake::resume, &old, k::Group::Active);
        const k::SlotPart part{id, 0, 0, 0, 1};
        ASSERT_EQ(ctx.scheduler().waitEventStrength(process, {}, {&part, 1}), E::None);
        switch (mode) {
            case 0: ctx.scheduler().cancelWait(process); break;
            case 1: ctx.scheduler().waitDelay(process, 1); break;
            case 2: ctx.scheduler().waitNamed(process, ctx.scheduler().addEvent()); break;
            case 3: ctx.scheduler().kill(process); break;
            case 4:
                ctx.scheduler().kill(process);
                ctx.scheduler().releaseProcess(process);
                EXPECT_EQ(ctx.scheduler().addProcess(&Wake::resume, &fresh, k::Group::Active),
                          process);
                break;
            default: ADD_FAILURE() << "unknown cancellation mode"; break;
        }
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
        publish(ctx, id, {6, 6});
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(old.calls, mode == 1 ? 1U : 0U);
        EXPECT_EQ(fresh.calls, 0U);
    }
}
TEST(StrengthWait, RepeatedRegistrationReusesOneWatchWithoutStaleWakeups) {
    k::Context ctx;
    const auto id = add(ctx);
    Wake wake;
    const auto process = ctx.scheduler().addProcess(&Wake::resume, &wake, k::Group::Active);
    const k::SlotPart part{id, 0, 0, 0, 1};
    for (unsigned i = 0; i < 512; ++i) {
        ASSERT_EQ(ctx.scheduler().waitEventStrength(process, {}, {&part, 1}), E::None);
        publish(ctx, id, i % 2 == 0 ? R{3, 6} : R{6, 6});
        ASSERT_EQ(ctx.scheduler().run(), k::EndReason::Quiescent);
        EXPECT_EQ(wake.calls, i + 1);
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
        EXPECT_EQ(ctx.strengths().watchRows(), 1U);
    }
}
class FailingMemory final : public std::pmr::memory_resource {
public:
    std::size_t remaining = SIZE_MAX;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining == 0) throw std::bad_alloc();
        if (remaining != SIZE_MAX) --remaining;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(ptr, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(StrengthWait, StagedAllocationFailureOrInvalidRangesKeepThePreviousWait) {
    unsigned failures = 0, successes = 0;
    for (unsigned budget = 0; budget < 16; ++budget) {
        FailingMemory memory;
        k::Context ctx(k::OutputDestinations{}, nullptr, &memory);
        const auto id = add(ctx, 3, 3);
        Wake wake;
        const auto process = ctx.scheduler().addProcess(&Wake::resume, &wake, k::Group::Active);
        const k::SlotPart old{id, 0, 1, 0, 1};
        ASSERT_EQ(ctx.scheduler().waitEventStrength(process, {}, {&old, 1}), E::None);
        const k::SlotPart invalid{id, 3, 1, 0, 1};
        EXPECT_EQ(ctx.scheduler().waitEventStrength(process, {}, {&invalid, 1}), E::InvalidTarget);
        EXPECT_EQ(ctx.strengths().liveWatches(), 1U);
        const k::SlotPart next{id, 0, 0, 1, 2};
        memory.remaining = budget;
        const auto status = ctx.scheduler().waitEventStrength(process, {}, {&next, 1});
        memory.remaining = SIZE_MAX;
        if (status == E::ResourceLimit) {
            ++failures;
            EXPECT_EQ(ctx.strengths().liveWatches(), 1U);
            EXPECT_TRUE(ctx.scheduler().ending());
        } else {
            ++successes;
            EXPECT_EQ(status, E::None);
            EXPECT_EQ(ctx.strengths().liveWatches(), 3U);
        }
        ctx.scheduler().cancelWait(process);
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
    }
    EXPECT_GT(failures, 0U);
    EXPECT_GT(successes, 0U);
}
}  // namespace
