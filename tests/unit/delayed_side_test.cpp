// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original independent kernel and hand-built SIR tests of the approved
// complete-state inertial contract (SCHEDULER_SPEC 12; SIR_SPEC 5.4).
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;

struct DelayedSequence {
    k::ProcessId process{0};
    k::SlotId primary{0}, sides{0};
    unsigned width = 1, step = 0;
    std::uint32_t driver = 0;
    std::vector<unsigned> samples, notifications;
    bool grown = false;

    unsigned read(k::Context& ctx) const {
        const auto* value = ctx.store().element(primary, 0);
        const auto* mask = ctx.store().element(sides, 0);
        return (static_cast<unsigned>(value[0] & 1) << 3) |
               (static_cast<unsigned>(value[1] & 1) << 2) |
               (static_cast<unsigned>(k::bitAt(mask, 1, width - 1)) << 1) |
               static_cast<unsigned>(k::bitAt(mask, 1, 2 * width - 1));
    }
    void drive(k::Context& ctx, bool one, bool zero, bool reverse) {
        std::array<k::DelayedSideUpdate, 2> captured{
            {{{sides, 0, width - 1, 1}, one}, {{sides, 0, 2 * width - 1, 1}, zero}}};
        if (reverse) std::reverse(captured.begin(), captured.end());
        std::array<std::uint64_t, 2> x{1, 1};
        ctx.scheduler().driveDelayed(driver, {primary, 0, 0, 1}, x.data(), 3, 3, 3, captured);
        // Registration owns both buffers, including target metadata.
        x = {0, 0};
        for (auto& item : captured) {
            item.at.lsb = 0;
            item.value = false;
        }
    }
    static std::int32_t guard(kirima_context* context, void* frame) {
        auto& self = *static_cast<DelayedSequence*>(frame);
        auto& ctx = k::fromC(context);
        self.notifications.push_back(self.read(ctx));
        if (!self.grown) {
            self.grown = true;
            for (unsigned i = 0; i < 4096; ++i)
                ctx.scheduler().addDelayedDriver();
        }
        return 0;  // Keep all three subscriptions live.
    }
    static kirima_status idle(kirima_context*, void*) {
        return static_cast<kirima_status>(k::Status::Done);
    }
    static kirima_status resume(kirima_context* context, void* frame) {
        auto& self = *static_cast<DelayedSequence*>(frame);
        auto& ctx = k::fromC(context);
        auto& scheduler = ctx.scheduler();
        std::uint64_t wait = 0;
        switch (self.step++) {
            case 0:
                self.drive(ctx, true, false, false);
                wait = 1;
                break;
            case 1:
                self.samples.push_back(self.read(ctx));
                wait = 1;
                break;
            case 2:
                self.drive(ctx, true, false, true);
                wait = 1;
                break;
            case 3:
                self.samples.push_back(self.read(ctx));
                wait = 1;
                break;
            case 4:
                self.drive(ctx, false, true, false);
                wait = 1;
                break;
            case 5:
                self.drive(ctx, true, false, true);
                wait = 3;
                break;
            case 6:
                self.samples.push_back(self.read(ctx));
                wait = 1;
                break;
            case 7:
                self.drive(ctx, false, true, false);
                wait = 1;
                break;
            case 8:
                self.drive(ctx, false, true, true);
                wait = 2;
                break;
            case 9:
                self.samples.push_back(self.read(ctx));
                return static_cast<kirima_status>(k::Status::Done);
            default: ADD_FAILURE() << self.step; return static_cast<kirima_status>(k::Status::Done);
        }
        scheduler.waitDelay(self.process, wait);
        return static_cast<kirima_status>(k::Status::Waiting);
    }
};

TEST(DelayedSide, OwnsCompleteStatesKeepsDeadlinesAndPublishesBeforeReentrantCallbacks) {
    for (const unsigned width : {1U, 33U, 65U, 129U, 257U}) {
        SCOPED_TRACE(width);
        k::Context ctx;
        auto& scheduler = ctx.scheduler();
        DelayedSequence sequence;
        sequence.width = width;
        sequence.primary = ctx.store().add(k::Domain::Four, 1, 1, k::Init::Z);
        sequence.sides = ctx.store().add(k::Domain::Two, 2 * width, 1, k::Init::Zero);
        sequence.driver = scheduler.addDelayedDriver();
        const auto observer =
            scheduler.addProcess(&DelayedSequence::idle, nullptr, k::Group::Active);
        const std::array<k::WaitItem, 3> watched{
            {{{sequence.primary}, k::Edge::Any, &DelayedSequence::guard, &sequence},
             {{sequence.sides, 0, 0, width - 1, 1},
              k::Edge::Any,
              &DelayedSequence::guard,
              &sequence},
             {{sequence.sides, 0, 0, 2 * width - 1, 1},
              k::Edge::Any,
              &DelayedSequence::guard,
              &sequence}}};
        scheduler.waitEvent(observer, watched);
        sequence.process =
            scheduler.addProcess(&DelayedSequence::resume, &sequence, k::Group::Reactive);
        scheduler.start(sequence.process);
        ASSERT_EQ(scheduler.run(), k::EndReason::Quiescent);
        EXPECT_EQ(sequence.samples, (std::vector<unsigned>{4, 14, 14, 13}));
        EXPECT_EQ(sequence.notifications, (std::vector<unsigned>{14, 14, 13, 13}));
        EXPECT_TRUE(sequence.grown);
    }
}

struct DelayedSir {
    s::Design design;
    explicit DelayedSir(unsigned width = 65, bool zero = false) {
        design.sources = {"delayed-side.sv"};
        design.instances = {{"top", "top", s::kNone}};
        design.slots = {{"top.driver", s::u4(1)}, {"top.sides", s::u2(2 * width)}};
        design.constants = {{s::u4(1), {1}, {1}},
                            {s::u2(64), {zero ? 0U : 3U}, {}},
                            {s::u2(1), {1}, {}},
                            {s::u2(1), {0}, {}}};
        s::Process p;
        p.name = "top.gate";
        p.kind = s::ProcessKind::Cont;
        p.loc = {0, 1, 1};
        p.writes = {{s::SlotId{0}},
                    {s::SlotId{1}, 0, 0, width - 1, 1},
                    {s::SlotId{1}, 0, 0, 2 * width - 1, 1}};
        p.blocks.resize(1);
        p.blocks[0].instrs = {
            {s::Op::DriveDelay,
             s::voidType(),
             s::kNone,
             {s::Operand::slot(s::SlotId{0}), s::Operand::constant(s::ConstId{0}),
              s::Operand::constant(s::ConstId{1}), s::Operand::constant(s::ConstId{1}),
              s::Operand::constant(s::ConstId{1}), s::Operand::slot(s::SlotId{1}, width - 1, 1),
              s::Operand::constant(s::ConstId{2}), s::Operand::slot(s::SlotId{1}, 2 * width - 1, 1),
              s::Operand::constant(s::ConstId{3})},
             {0, 1, 1}},
            {s::Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}}};
        design.processes.push_back(std::move(p));
    }
};

std::string errors(const s::Design& design) {
    std::string out;
    for (const auto& error : s::verify(design))
        out += error.message + '\n';
    return out;
}

TEST(DelayedSide, HandBuiltSirTransportsOwnedZeroAndNonzeroDelayFields) {
    for (const unsigned width : {1U, 33U, 65U, 129U, 257U})
        for (const bool zero : {false, true})
            for (const bool high : {false, true}) {
                DelayedSir program(width, zero);
                program.design.constants[2].aWords = {high ? 1U : 0U};
                program.design.constants[3].aWords = {high ? 0U : 1U};
                ASSERT_EQ(errors(program.design), "");
                EXPECT_NE(s::toText(program.design).find("drive.delay"), std::string::npos);
                k::Context ctx;
                const auto result = kirimasim::vm::run(program.design, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                const auto* value = ctx.store().element(k::SlotId{0}, 0);
                EXPECT_EQ(value[0], 1U);
                EXPECT_EQ(value[1], 1U);
                const auto* sides = ctx.store().element(k::SlotId{1}, 0);
                for (unsigned bit = 0; bit < 2 * width; ++bit)
                    EXPECT_EQ(k::bitAt(sides, 1, bit), bit == (high ? width - 1 : 2 * width - 1))
                        << bit;
            }
}

TEST(DelayedSide, VerifierRejectsMalformedOwnedFieldsAndMissingWriteCoverage) {
    for (unsigned broken = 0; broken < 12; ++broken) {
        DelayedSir program;
        auto& d = program.design;
        auto& p = d.processes[0];
        auto& in = p.blocks[0].instrs[0];
        ASSERT_EQ(errors(d), "");
        switch (broken) {
            case 0: in.operands.pop_back(); break;
            case 1: in.operands[5] = s::Operand::constant(s::ConstId{2}); break;
            case 2: d.constants[2].type = s::u2(2); break;
            case 3: in.operands[7] = in.operands[5]; break;
            case 4: in.operands[5] = s::Operand::slot(s::SlotId{0}, 0, 1); break;
            case 5: d.slots[1].dims = {{0, 1}}; break;
            case 6: p.writes.pop_back(); break;
            case 7: in.operands[5].lsb = 130; break;
            case 8: in.operands[6] = s::Operand::str("bad"); break;
            case 9: p.kind = s::ProcessKind::Initial; break;
            case 10:
                p.frame.push_back({"bad", s::u2(1)});
                in.operands[5] = s::Operand::frame(s::FrameVarId{0});
                break;
            case 11:
                d.constants[2].type = s::u4(1);
                d.constants[2].bWords = {1};
                break;
            default: ADD_FAILURE() << broken; break;
        }
        EXPECT_FALSE(errors(d).empty()) << broken;
    }
}
}  // namespace
