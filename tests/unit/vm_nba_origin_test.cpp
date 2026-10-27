// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original SIR/native integration: finished callback origins keep typed frame
// roots until readiness/discard. Source event-NBA is not enabled by this test.
#include <array>
#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
kirima_status done(kirima_context*, void*) { return KIRIMA_DONE; }

s::Design frameContainer() {
    s::Design d;
    d.sources = {"nba-origin.sv"};
    d.instances = {{"top", "top", s::kNone}};
    d.slots.push_back({"top.output", s::u2(32)});
    s::TypeEntry type;
    type.kind = s::TypeEntryKind::Queue;
    type.name = "integers";
    type.value = s::u2(32);
    d.types.push_back(std::move(type));
    const auto queue = s::queueType(s::TypeId{0});
    for (auto n : {11U, 23U}) {
        s::Constant constant;
        constant.type = s::u2(32);
        constant.aWords = {n};
        d.constants.push_back(std::move(constant));
    }
    s::Process process;
    process.name = "top.initial";
    process.loc = {0, 1, 1};
    process.numTemps = 1;
    s::FrameVar var;
    var.name = "local";
    var.type = queue;
    process.frame.push_back(std::move(var));
    process.blocks.resize(1);
    process.blocks[0].instrs = {
        {s::Op::ContainerPack,
         queue,
         s::TempId{0},
         {s::Operand::constant(s::ConstId{0}), s::Operand::constant(s::ConstId{1})},
         {0, 1, 1}},
        {s::Op::Fstore,
         s::voidType(),
         s::kNone,
         {s::Operand::frame(s::FrameVarId{0}), s::Operand::temp(s::TempId{0})},
         {0, 1, 1}},
        {s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}},
    };
    d.processes.push_back(std::move(process));
    // A second VM entry sweeps the ended origin while its control is pending.
    // A single VM initial would leave it in the running-frame deferral path.
    s::Process other;
    other.name = "top.other";
    other.loc = {0, 2, 1};
    other.blocks.resize(1);
    other.blocks[0].instrs = {{s::Op::Ret, s::voidType(), s::kNone, {}, {0, 2, 1}}};
    d.processes.push_back(std::move(other));
    return d;
}
struct Observer {
    k::ProcessId self{0}, origin{1};
    k::SlotId output;
    k::EventId event;
    k::NbaEventTicket control;
    unsigned phase = 0, releases = 0;
    bool discard = false;
    static void release(void* raw) noexcept { ++static_cast<Observer*>(raw)->releases; }
    static kirima_status resume(kirima_context* context, void* raw) {
        auto& f = *static_cast<Observer*>(raw);
        auto& c = k::fromC(context);
        auto& scheduler = c.scheduler();
        if (f.phase == 0) {
            const std::uint64_t value = 42;
            const k::NbaWrite write{{.slot = f.output}, {&value, 1}};
            const auto payload = scheduler.captureNba(f.origin, {&write, 1});
            EXPECT_TRUE(payload);
            const k::W4 one{1, 0};
            const std::array items{
                k::NbaEventItem{k::NamedValueWaitItem{k::EventValue::legacy(f.event)}}};
            const auto result = scheduler.registerNbaEvent(
                f.origin, payload.ticket, {1, false, true, {&one, 1}}, items, {&release, &f});
            EXPECT_TRUE(result);
            f.control = result.ticket;
        } else if (f.phase == 1) {
            EXPECT_TRUE(scheduler.finished(f.origin));
            EXPECT_TRUE(scheduler.hasCallbackOrigins(f.origin));
            EXPECT_GT(c.heap().stats().containers, 0U);
            EXPECT_GE(c.heap().stats().cells, 2U);
            EXPECT_EQ(f.releases, 0U);
            scheduler.releaseProcess(f.origin);
            EXPECT_NE(scheduler.addProcess(done, nullptr, k::Group::Active), f.origin);
            if (f.discard)
                EXPECT_EQ(scheduler.discardNbaEvent(f.control), k::NbaError::None);
            else
                scheduler.trigger(f.event);
            EXPECT_FALSE(scheduler.hasCallbackOrigins(f.origin));
            EXPECT_EQ(f.releases, 1U);
        } else {
            EXPECT_EQ(c.heap().stats().containers, 0U);
            EXPECT_EQ(c.heap().stats().cells, 0U);
            const auto first = scheduler.addProcess(done, nullptr, k::Group::Active);
            const auto second = scheduler.addProcess(done, nullptr, k::Group::Active);
            EXPECT_TRUE(first == f.origin || second == f.origin);
            EXPECT_EQ(c.store().element(f.output, 0)[0], f.discard ? 0U : 42U);
            ++f.phase;
            return KIRIMA_DONE;
        }
        ++f.phase;
        scheduler.waitDelay(f.self, 1);
        return KIRIMA_WAITING;
    }
};
TEST(VmNbaOrigin, FinishedFrameRootsSurviveUntilLastControlAndAreReclaimedAtTheNextBoundary) {
    for (bool discard : {false, true}) {
        const auto design = frameContainer();
        const auto errors = s::verify(design);
        ASSERT_TRUE(errors.empty()) << (errors.empty() ? "" : errors[0].message);
        Observer observer;
        k::Context context;
        observer.discard = discard;
        observer.output = k::SlotId{0};
        observer.event = context.scheduler().addEvent();
        observer.self =
            context.scheduler().addProcess(&Observer::resume, &observer, k::Group::Active);
        ASSERT_EQ(observer.self, k::ProcessId{0});
        context.scheduler().start(observer.self);
        const auto result = kirimasim::vm::run(design, context);
        EXPECT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_TRUE(result.error.empty());
        EXPECT_EQ(observer.phase, 3U);
        EXPECT_EQ(observer.releases, 1U);
        EXPECT_EQ(context.scheduler().nbaEventStats().live, 0U);
    }
}
}  // namespace
