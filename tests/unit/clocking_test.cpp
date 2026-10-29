// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory_resource>
#include <new>
#include <vector>

namespace {
namespace k = kirimasim::kernel;
namespace s = kirimasim::sir;
struct ClockRig {
    k::Context c;
    k::SlotId clock, source, target, secondSource, secondTarget;
    k::EventId event;
    k::ProcessId writer, reader;
    unsigned phase = 0, reads = 0, width;
    bool nba;
    std::vector<std::uint64_t> expected;
    explicit ClockRig(unsigned bits, bool nonblocking, k::Group group)
        : clock(c.store().add(k::Domain::Two, 1, 1, k::Init::Zero)),
          source(c.store().add(k::Domain::Four, bits, 1, k::Init::X)),
          target(c.store().add(k::Domain::Four, bits, 1, k::Init::X)),
          secondSource(c.store().add(k::Domain::Two, 32, 1, k::Init::Zero)),
          secondTarget(c.store().add(k::Domain::Two, 32, 1, k::Init::Zero)),
          event(c.scheduler().addEvent()), width(bits), nba(nonblocking) {
        writer = c.scheduler().addProcess(write, this, group);
        reader = c.scheduler().addProcess(read, this, k::Group::Active);
    }
    static kirima_status write(kirima_context*, void* raw) {
        auto& r = *static_cast<ClockRig*>(raw);
        auto& sched = r.c.scheduler();
        if (r.phase == 5) return KIRIMA_DONE;
        // Independent reference: raw storage on entry to this time slot.
        const auto* before = r.c.store().element(r.source, 0);
        const auto words = std::size_t{k::wordsFor(r.width)} * 2;
        r.expected.assign(before, before + words);
        std::vector<std::uint64_t> value(words);
        for (std::size_t word = 0; word < k::wordsFor(r.width); ++word) {
            value[word * 2] = (r.phase & 1U) != 0 ? ~std::uint64_t{0} : 0;
            value[word * 2 + 1] = (r.phase & 2U) != 0 ? ~std::uint64_t{0} : 0;
        }
        const std::uint64_t zero = 0, one = 1, data = r.phase + 1;
        sched.write({r.clock}, &zero);
        sched.write({r.source}, value.data());
        sched.write({r.secondSource}, &data);
        if (r.nba)
            sched.scheduleNba(r.writer, {r.clock}, &one, 0);
        else
            sched.write({r.clock}, &one);
        ++r.phase;
        sched.waitDelay(r.writer, 17);
        return KIRIMA_WAITING;
    }
    static kirima_status read(kirima_context*, void* raw) {
        auto& r = *static_cast<ClockRig*>(raw);
        if (r.reads != 0) {
            EXPECT_TRUE(
                std::equal(r.expected.begin(), r.expected.end(), r.c.store().element(r.target, 0)));
            EXPECT_EQ(*r.c.store().element(r.secondTarget, 0), r.phase - 1);
            EXPECT_EQ(r.c.scheduler().time(), (r.phase - 1) * 17);
        }
        if (r.reads++ == 5) return KIRIMA_DONE;
        r.c.scheduler().waitNamed(r.reader, r.event);
        return KIRIMA_WAITING;
    }
    k::ClockingError registerBlock() {
        const std::array inputs{k::ClockingInput{{source}, {target}},
                                k::ClockingInput{{secondSource}, {secondTarget}}};
        return c.scheduler().addClockingBlock({{clock}, k::Edge::Pos}, event, inputs);
    }
};
TEST(Clocking, SnapshotPublicationAcrossWidthsValuesAndEvaluationSets) {
    for (const auto width : {1U, 32U, 65U, 129U})
        for (const auto group : {k::Group::Active, k::Group::Reactive})
            for (const bool nba : {false, true}) {
                SCOPED_TRACE(width);
                SCOPED_TRACE(nba);
                SCOPED_TRACE(static_cast<int>(group));
                ClockRig r(width, nba, group);
                ASSERT_EQ(r.registerBlock(), k::ClockingError::None);
                r.c.scheduler().start(r.reader);
                r.c.scheduler().start(r.writer);
                r.c.scheduler().run();
                EXPECT_EQ(r.reads, 6U);
                EXPECT_EQ(r.c.scheduler().time(), 85U);
            }
}
TEST(Clocking, NoStartupEdgeOrQuiescenceRootAndContextIsolation) {
    ClockRig a(65, false, k::Group::Active), b(129, true, k::Group::Reactive);
    ASSERT_EQ(a.registerBlock(), k::ClockingError::None);
    ASSERT_EQ(b.registerBlock(), k::ClockingError::None);
    a.c.scheduler().start(a.reader);
    a.c.scheduler().run();
    EXPECT_EQ(a.reads, 1U);
    EXPECT_EQ(a.c.scheduler().time(), 0U);
    a.c.scheduler().kill(a.reader);
    const std::array<std::uint64_t, 4> one{~std::uint64_t{0}, 0, 1, 0};
    const std::uint64_t edge = 1;
    a.c.scheduler().write({a.source}, one.data());
    a.c.scheduler().write({a.clock}, &edge);
    a.c.scheduler().run();  // A tier can drain callbacks again at the same time.
    EXPECT_EQ(*a.c.store().element(a.target, 0), ~std::uint64_t{0});
    EXPECT_EQ(a.c.store().element(a.target, 0)[1], ~std::uint64_t{0});
    EXPECT_EQ(b.reads, 0U);
    EXPECT_EQ(*b.c.store().element(b.target, 0), ~std::uint64_t{0});
}
TEST(Clocking, InvalidRegistrationDoesNotInstallWatchers) {
    ClockRig r(65, false, k::Group::Active);
    auto& sched = r.c.scheduler();
    const k::ClockingInput valid{{r.source}, {r.target}};
    auto bad = valid;
    bad.source.slot = k::SlotId{10000};
    EXPECT_EQ(sched.addClockingBlock({{r.clock}}, r.event, {&bad, 1}), k::ClockingError::Invalid);
    bad = valid;
    bad.source.lsb = 65;
    bad.source.width = 1;
    EXPECT_EQ(sched.addClockingBlock({{r.clock}}, r.event, {&bad, 1}), k::ClockingError::Invalid);
    bad = valid;
    bad.target = bad.source;
    EXPECT_EQ(sched.addClockingBlock({{r.clock}}, r.event, {&bad, 1}), k::ClockingError::Invalid);
    const std::array duplicate{valid, valid};
    EXPECT_EQ(sched.addClockingBlock({{r.clock}}, r.event, duplicate), k::ClockingError::Invalid);
    ASSERT_EQ(r.registerBlock(), k::ClockingError::None);
    EXPECT_EQ(r.registerBlock(), k::ClockingError::Invalid);
    sched.run();
    EXPECT_EQ(sched.addClockingBlock({{r.clock}}, sched.addEvent(), {}), k::ClockingError::Invalid);
}
class ClockingMemory final : public std::pmr::memory_resource {
public:
    int remaining = -1;
    std::size_t live = 0;

private:
    void* do_allocate(std::size_t size, std::size_t align) override {
        if (remaining == 0) throw std::bad_alloc();
        if (remaining > 0) --remaining;
        void* result = std::pmr::new_delete_resource()->allocate(size, align);
        ++live;
        return result;
    }
    void do_deallocate(void* p, std::size_t size, std::size_t align) override {
        --live;
        std::pmr::new_delete_resource()->deallocate(p, size, align);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(Clocking, AllocationFailureRollsBackEveryRegistrationStage) {
    for (int fail = 0; fail < 4; ++fail) {
        ClockingMemory memory;
        {
            k::Context c({stdout, stderr}, nullptr, &memory);
            const auto clk = c.store().add(k::Domain::Two, 1, 1, k::Init::Zero);
            const auto src = c.store().add(k::Domain::Four, 129, 1, k::Init::X);
            const auto dst = c.store().add(k::Domain::Four, 129, 1, k::Init::X);
            const auto event = c.scheduler().addEvent();
            const k::ClockingInput input{{src}, {dst}};
            const auto before = memory.live;
            memory.remaining = fail;
            const auto result = c.scheduler().addClockingBlock({{clk}}, event, {&input, 1});
            if (fail < 3) {
                EXPECT_EQ(result, k::ClockingError::ResourceLimit);
                EXPECT_EQ(memory.live, before);
                EXPECT_EQ(c.scheduler().reason(), k::EndReason::ResourceLimit);
            } else
                EXPECT_EQ(result, k::ClockingError::None);
        }
        EXPECT_EQ(memory.live, 0U);
    }
}
s::Design clockingDesign() {
    s::Design d;
    d.instances = {{"top", "top", s::kNone}};
    d.slots = {{"top.clk", s::u2(1)},
               {"top.d", s::s4(32)},
               {"top.cb.d", s::s4(32)},
               {"top.cb.event", {s::TypeKind::Event, 0}}};
    s::ClockingBlock block;
    block.clock = {s::SlotId{0}};
    block.edge = s::Edge::Pos;
    block.event = s::SlotId{3};
    block.inputs.push_back({{s::SlotId{1}}, s::SlotId{2}});
    d.clockingBlocks.push_back(block);
    return d;
}
TEST(ClockingSir, VerifiesBoundsTypesOwnershipAndReadOnlyDestinations) {
    const auto good = clockingDesign();
    ASSERT_TRUE(s::verify(good).empty());
    for (unsigned mutation = 0; mutation < 10; ++mutation) {
        auto d = good;
        auto& block = d.clockingBlocks.front();
        switch (mutation) {
            case 0: block.clock.slot = s::SlotId{99}; break;
            case 1: block.inputs.front().source.width = 33; break;
            case 2: d.slots[2].type = s::u2(32); break;
            case 3: block.instance = s::InstanceId{99}; break;
            case 4: block.inputs.push_back(block.inputs.front()); break;
            case 5: block.event = s::SlotId{0}; break;
            case 6: block.inputs.front().target = s::SlotId{1}; break;
            case 7: block.edge = static_cast<s::Edge>(99); break;
            case 8: d.slots[2].type = s::u4(32); break;
            case 9: d.clockingBlocks.push_back(block); break;
            default: FAIL() << "unexpected mutation"; break;
        }
        EXPECT_FALSE(s::verify(d).empty()) << mutation;
    }
}
}  // namespace
