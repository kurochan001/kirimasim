// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "text_v1.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory_resource>
#include <new>
#include <string>
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
                EXPECT_TRUE(c.scheduler().memoryRanOut());
            } else
                EXPECT_EQ(result, k::ClockingError::None);
        }
        EXPECT_EQ(memory.live, 0U);
    }
}
TEST(SampledValues, RegistrationProtectsBothClockingAndSampleStorage) {
    const std::array<std::uint64_t, 4> initial{11U, 0U, 1U, 0U};
    for (const bool sampleFirst : {false, true}) {
        ClockRig r(65, false, k::Group::Active);
        auto& sched = r.c.scheduler();
        const auto current = r.c.store().add(k::Domain::Four, 65, 1, k::Init::X);
        const auto previous = r.c.store().add(k::Domain::Four, 65, 1, k::Init::X);
        const k::SampleHistory history{{{r.clock}, k::Edge::Neg}, previous};
        EXPECT_EQ(sched.addSampledInput({r.source}, previous, initial, history),
                  k::ClockingError::Invalid);
        EXPECT_EQ(sched.addSampledInput({r.source}, current, {}, history),
                  k::ClockingError::Invalid);
        if (sampleFirst) {
            ASSERT_EQ(sched.addSampledInput({r.source}, current, initial, history),
                      k::ClockingError::None);
            const k::ClockingInput collision{{r.source}, {previous}};
            EXPECT_EQ(sched.addClockingBlock({{r.clock}}, r.event, {&collision, 1}),
                      k::ClockingError::Invalid);
            ASSERT_EQ(r.registerBlock(), k::ClockingError::None);
        } else {
            ASSERT_EQ(r.registerBlock(), k::ClockingError::None);
            EXPECT_EQ(sched.addSampledInput({r.target}, current, initial, history),
                      k::ClockingError::Invalid);
            ASSERT_EQ(sched.addSampledInput({r.source}, current, initial, history),
                      k::ClockingError::None);
        }
        for (const auto slot : {previous, current})
            EXPECT_TRUE(std::ranges::equal(initial, std::span(r.c.store().element(slot, 0), 4)));
        EXPECT_EQ(sched.addSampledInput({r.source}, current, initial, history),
                  k::ClockingError::Invalid);
        sched.run();
        EXPECT_EQ(sched.addSampledInput({r.source}, current, initial, {}),
                  k::ClockingError::Invalid);
    }
}
TEST(SampledValues, AllocationFailureLeavesSampleStorageAtItsOriginalValue) {
    for (int budget = 0; budget <= 4; ++budget) {
        ClockingMemory memory;
        {
            k::Context c({stdout, stderr}, nullptr, &memory);
            const auto slot = [&] { return c.store().add(k::Domain::Two, 1, 1, k::Init::Zero); };
            const auto clk = slot(), src = slot(), current = slot(), previous = slot();
            const std::array<std::uint64_t, 1> initial{1};
            const k::SampleHistory history{{{clk}, k::Edge::Pos}, previous};
            memory.remaining = budget;
            const auto result = c.scheduler().addSampledInput({src}, current, initial, history);
            if (budget < 4) {
                EXPECT_EQ(result, k::ClockingError::ResourceLimit);
                EXPECT_TRUE(c.scheduler().memoryRanOut());
                EXPECT_EQ(*c.store().element(current, 0), 0U);
                EXPECT_EQ(*c.store().element(previous, 0), 0U);
            } else {
                EXPECT_EQ(result, k::ClockingError::None);
                EXPECT_EQ(*c.store().element(current, 0), 1U);
                EXPECT_EQ(*c.store().element(previous, 0), 1U);
            }
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
    auto good = clockingDesign();
    good.slots.push_back({"top.alternate", s::s4(32)});
    ASSERT_TRUE(s::verify(good).empty());
    auto otherSource = good;
    otherSource.clockingBlocks.front().inputs.front().source.slot = s::SlotId{4};
    std::string first;
    for (const auto& source : {good, otherSource}) {
        const auto verified = s::verifyDesign(source);
        if (!verified.verified) FAIL() << "fixture did not verify";
        const auto text = s::print1(*verified.verified);
        if (!text) FAIL() << "cannot serialize fixture";
        EXPECT_NE(first, *text);
        first = *text;
        const auto parsed = s::parse1(*text);
        if (!parsed.verified) FAIL() << "cannot read back fixture";
        const auto& design = parsed.verified->design();
        ASSERT_EQ(design.clockingBlocks.size(), 1U);
        const auto& block = design.clockingBlocks.front();
        ASSERT_EQ(block.inputs.size(), 1U);
        EXPECT_EQ(block.inputs.front().source, source.clockingBlocks.front().inputs.front().source);
        EXPECT_EQ(block.inputs.front().target, s::SlotId{2});
        EXPECT_EQ(s::print1(*parsed.verified), text);
    }
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
TEST(SampledValuesSir, PreservesBindingsAndRejectsInvalidStorage) {
    s::Design good;
    good.instances = {{"top", "top", s::kNone}};
    good.slots = {{"top.clk", s::u2(1)},
                  {"top.d", s::u4(65)},
                  {"current", s::u4(65)},
                  {"previous", s::u4(65)}};
    good.names = {{"top.d", s::SlotId{1}}};
    good.constants = {{s::u4(65), {1, 0}, {0, 1}}};
    s::SampledInput row;
    row.sourceDeclaration = 0;
    row.source = {s::SlotId{1}};
    row.current = s::SlotId{2};
    row.initial = s::ConstId{0};
    row.clock = s::SampleClock{{s::SlotId{0}}, s::Edge::Pos, s::SlotId{3}};
    good.sampledInputs.push_back(row);
    for (auto edge : {s::Edge::Pos, s::Edge::Neg}) {
        good.sampledInputs.front().clock = s::SampleClock{{s::SlotId{0}}, edge, s::SlotId{3}};
        auto verified = s::verifyDesign(good);
        if (!verified.verified) FAIL() << "sampled inputs did not verify";
        const auto text = s::print1(*verified.verified);
        if (!text) FAIL() << "cannot serialize sampled inputs";
        auto read = s::parse1(*text);
        if (!read.verified) FAIL() << "cannot read sampled inputs";
        const auto& input = read.verified->design().sampledInputs.at(0);
        EXPECT_EQ(input.source, row.source);
        EXPECT_EQ(input.sourceDeclaration, 0U);
        EXPECT_EQ(input.current, row.current);
        EXPECT_EQ(input.initial, row.initial);
        if (!input.clock) FAIL() << "clock history was lost";
        EXPECT_EQ(input.clock->edge, edge);
        EXPECT_EQ(input.clock->previous, s::SlotId{3});
        EXPECT_EQ(s::print1(*read.verified), text);
    }
    for (unsigned mutation = 0; mutation < 10; ++mutation) {
        auto bad = good;
        auto& input = bad.sampledInputs.front();
        if (!input.clock) FAIL() << "fixture has no history clock";
        switch (mutation) {
            case 0: input.source.slot = s::kNone; break;
            case 1: input.sourceDeclaration = 1; break;
            case 2: input.clock.value().range.slot = s::SlotId{1}; break;
            case 3: input.clock.value().edge = s::Edge::Any; break;
            case 4: input.clock.value().previous = input.current; break;
            case 5: input.initial = s::kNone; break;
            case 6: bad.slots[2].type = s::s4(65); break;
            case 7: input.instance = s::kNone; break;
            case 8: bad.names.push_back({"visible", input.current}); break;
            case 9: input.source.width = 80; break;
            default: FAIL() << "unexpected mutation"; break;
        }
        EXPECT_FALSE(s::verify(bad).empty()) << mutation;
    }
}
}  // namespace
