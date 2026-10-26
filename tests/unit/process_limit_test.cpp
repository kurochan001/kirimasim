// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// CLI 2.7 and SCHEDULER_SPEC 10.8/16.2: admission counts unfinished
// runtime children, independent of static processes and record retention.
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"

namespace {
namespace k = kirimasim::kernel;

kirima_status done(kirima_context*, void*) { return KIRIMA_DONE; }

void capacity(k::Context& context, std::uint64_t count) {
    k::Limits limits;
    limits.maxProcs = count;
    context.scheduler().setLimits(limits);
}

k::ProcessId parent(k::Context& context) {
    return context.scheduler().addProcess(done, nullptr, k::Group::Active);
}

k::ProcessId child(k::Context& context, k::ProcessId owner) {
    return context.scheduler().addChildProcess(done, nullptr, owner);
}

std::uint64_t created(k::Context& context) {
    return context.counters().value(context.counters().add("proc.created"));
}

TEST(ProcessLimit, PublicDefaultAndUnlimitedConfiguration) {
    EXPECT_EQ(k::Limits{}.maxProcs, 1000000U);
    k::Context c;
    capacity(c, 0);
    const auto p = parent(c);
    for (unsigned i = 0; i < 128; ++i)
        ASSERT_NE(child(c, p), k::kNone);
    EXPECT_FALSE(c.scheduler().ending());
    EXPECT_EQ(created(c), 129U);
}

TEST(ProcessLimit, StaticAndProgramInitialProcessesDoNotConsumeCapacity) {
    k::Context c;
    capacity(c, 1);
    const auto p = parent(c);
    for (unsigned i = 0; i < 64; ++i) {
        ASSERT_NE(parent(c), k::kNone);
        const auto program = c.scheduler().addProgram();
        ASSERT_NE(c.scheduler().addProgramInitial(done, nullptr, program), k::kNone);
    }
    EXPECT_NE(child(c, p), k::kNone);
    EXPECT_FALSE(c.scheduler().ending());
}

TEST(ProcessLimit, RejectedChildHasNoRecordCounterOrCallback) {
    k::Context c;
    capacity(c, 1);
    const auto p = parent(c);
    const auto first = child(c, p);
    ASSERT_NE(first, k::kNone);
    const auto before = created(c);
    bool invoked = false;
    const auto callback = [](kirima_context*, void* frame) -> kirima_status {
        *static_cast<bool*>(frame) = true;
        return KIRIMA_DONE;
    };
    EXPECT_EQ(c.scheduler().addChildProcess(callback, &invoked, p), k::kNone);
    EXPECT_EQ(created(c), before);
    EXPECT_FALSE(invoked);
    EXPECT_EQ(c.scheduler().reason(), k::EndReason::ResourceLimit);
    EXPECT_EQ(c.scheduler().endNote(),
              "resource limit reached: 1 live processes made at run time (--max-procs=1)");
    EXPECT_TRUE(c.scheduler().error().empty());
}

TEST(ProcessLimit, NormalCompletionReturnsCapacityBeforeRecordsAreReleased) {
    k::Context c;
    capacity(c, 1);
    const auto p = parent(c);
    for (unsigned i = 0; i < 32; ++i) {
        const auto f = child(c, p);
        ASSERT_NE(f, k::kNone);
        c.scheduler().ended(f);
        c.scheduler().ended(f);
        EXPECT_TRUE(c.scheduler().finished(f));
    }
    EXPECT_EQ(created(c), 33U);
    const auto live = child(c, p);
    ASSERT_NE(live, k::kNone);
    EXPECT_EQ(child(c, p), k::kNone);
}

TEST(ProcessLimit, KillReturnsCapacityExactlyOnce) {
    k::Context c;
    capacity(c, 1);
    const auto p = parent(c);
    const auto first = child(c, p);
    ASSERT_NE(first, k::kNone);
    c.scheduler().kill(first);
    c.scheduler().kill(first);
    c.scheduler().ended(first);
    ASSERT_NE(child(c, p), k::kNone);
    EXPECT_EQ(child(c, p), k::kNone);
}

TEST(ProcessLimit, ReusedStaticRecordDoesNotReleaseAnotherChildsCapacity) {
    k::Context c;
    capacity(c, 1);
    const auto p = parent(c);
    const auto first = child(c, p);
    c.scheduler().ended(first);
    c.scheduler().releaseProcess(first);
    const auto helper = parent(c);
    ASSERT_EQ(helper, first);
    ASSERT_NE(child(c, p), k::kNone);
    c.scheduler().ended(helper);
    EXPECT_EQ(child(c, p), k::kNone);
}

TEST(ProcessLimit, EndedAncestorDoesNotCountWhileItsDescendantRemains) {
    k::Context c;
    capacity(c, 2);
    const auto p = parent(c);
    const auto first = child(c, p);
    ASSERT_NE(first, k::kNone);
    ASSERT_NE(child(c, first), k::kNone);
    c.scheduler().ended(first);
    ASSERT_NE(child(c, p), k::kNone);
    EXPECT_EQ(child(c, p), k::kNone);
}

TEST(ProcessLimit, TimeAndNamedEventWaitersRemainLive) {
    for (const bool named : {false, true}) {
        k::Context c;
        capacity(c, 1);
        const auto p = parent(c);
        const auto f = child(c, p);
        ASSERT_NE(f, k::kNone);
        if (named)
            c.scheduler().waitNamed(f, c.scheduler().addEvent());
        else
            c.scheduler().waitDelay(f, 100);
        EXPECT_EQ(child(c, p), k::kNone);
        EXPECT_FALSE(c.scheduler().finished(f));
    }
}

TEST(ProcessLimit, ProgramCompletionReturnsAllItsDescendantsCapacity) {
    k::Context c;
    capacity(c, 2);
    const auto first = c.scheduler().addProgram();
    const auto second = c.scheduler().addProgram();
    const auto a = c.scheduler().addProgramInitial(done, nullptr, first);
    const auto b = c.scheduler().addProgramInitial(done, nullptr, second);
    const auto x = child(c, a);
    ASSERT_NE(x, k::kNone);
    const auto y = child(c, x);
    ASSERT_NE(y, k::kNone);
    c.scheduler().exitProgram(a);
    EXPECT_TRUE(c.scheduler().finished(x));
    EXPECT_TRUE(c.scheduler().finished(y));
    ASSERT_NE(child(c, b), k::kNone);
    ASSERT_NE(child(c, b), k::kNone);
    EXPECT_EQ(child(c, b), k::kNone);
}

TEST(ProcessLimit, CapacityAndTerminationBelongToEachContext) {
    k::Context first, second;
    capacity(first, 1);
    capacity(second, 1);
    const auto a = parent(first), b = parent(second);
    const auto x = child(first, a);
    ASSERT_NE(x, k::kNone);
    ASSERT_NE(child(second, b), k::kNone);
    first.scheduler().kill(x);
    ASSERT_NE(child(first, a), k::kNone);
    EXPECT_EQ(child(second, b), k::kNone);
    EXPECT_EQ(second.scheduler().reason(), k::EndReason::ResourceLimit);
    EXPECT_FALSE(first.scheduler().ending());
}
}  // namespace
