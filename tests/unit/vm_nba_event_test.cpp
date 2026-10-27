// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original SIR/native tests of the approved event-NBA execution contract.
// These exercise the VM bridge, not source-language lowering.
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/kernel/urandom.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
using s::Op;
using s::Operand;
const s::Type event{s::TypeKind::Event, 0};
Operand block(unsigned n) { return Operand::block(s::BlockId{n}); }
Operand field(unsigned n) { return Operand::frame(s::FrameVarId{n}); }
s::NbaValueId captured(unsigned n) { return s::NbaValueId{n}; }
struct Model {
    s::Design d;
    explicit Model(bool program = false) {
        d.sources = {"vm-nba-event.sv"};
        d.instances = {
            {"top", "top", s::kNone, program ? s::InstanceKind::Program : s::InstanceKind::Module}};
        for (unsigned i = 0; i < 2; ++i) {
            s::Process p;
            p.name = "top.initial" + std::to_string(i);
            p.loc = {0, i + 1, 1};
            d.processes.push_back(std::move(p));
        }
    }
    Operand constant(s::Type type, std::vector<std::uint64_t> a,
                     std::vector<std::uint64_t> b = {}) {
        if (s::isFourState(type) && b.empty()) b.resize(a.size(), 0);
        auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({type, std::move(a), std::move(b)});
        return Operand::constant(id);
    }
    Operand scalar(s::Type type, std::uint64_t n) {
        std::vector<std::uint64_t> a(s::isIntegral(type) ? (type.width + 63) / 64 : 1, 0);
        a[0] = n;
        return constant(type, std::move(a));
    }
    Operand ticks(std::uint64_t n) { return scalar(s::u2(64), n); }
    Operand slot(s::Type type, std::vector<s::ArrayDim> dims = {}) {
        auto id = s::SlotId{static_cast<std::uint32_t>(d.slots.size())};
        s::Slot value;
        value.name = "top.value" + std::to_string(id.index());
        value.type = type;
        value.dims = std::move(dims);
        d.slots.push_back(std::move(value));
        return Operand::slot(id);
    }
    template <class Body>
    Operand emitIn(Body& body, unsigned bb, Op op, std::vector<Operand> args, s::Type type) {
        if (body.blocks.size() <= bb) body.blocks.resize(bb + 1);
        auto id = s::TempId{type == s::voidType() ? s::kNone : body.numTemps++};
        body.blocks[bb].instrs.push_back({op, type, id, std::move(args), {0, 7, 3}});
        return Operand::temp(id);
    }
    Operand emit(unsigned process, unsigned bb, Op op, std::vector<Operand> args = {},
                 s::Type type = s::voidType()) {
        return emitIn(d.processes[process], bb, op, std::move(args), type);
    }
    Operand subEmit(unsigned sub, Op op, std::vector<Operand> args = {},
                    s::Type type = s::voidType()) {
        return emitIn(d.subroutines[sub], 0, op, std::move(args), type);
    }
    unsigned function(s::Type result, s::Type privateType = s::voidType()) {
        s::Subroutine sub;
        sub.name = "top.callback" + std::to_string(d.subroutines.size());
        sub.returnType = result;
        sub.automatic = true;
        sub.loc = {0, 4, 1};
        if (privateType != s::voidType()) sub.frame.push_back({"work", privateType});
        d.subroutines.push_back(std::move(sub));
        return static_cast<unsigned>(d.subroutines.size() - 1);
    }
    s::SubroutineId freshGuard() {
        const auto id = function(s::u2(1), s::u2(32));
        const auto old = subEmit(id, Op::Fload, {field(0)}, s::u2(32));
        const auto first = subEmit(id, Op::Eq, {old, scalar(s::u2(32), 0)}, s::u2(1));
        const auto next = subEmit(id, Op::Add, {old, scalar(s::u2(32), 1)}, s::u2(32));
        subEmit(id, Op::Fstore, {field(0), next});
        subEmit(id, Op::Ret, {first});
        return s::SubroutineId{id};
    }
    s::SubroutineId freshWatch(const Operand& clock, s::Type type) {
        const auto id = function(type, type);
        const auto old = subEmit(id, Op::Fload, {field(0)}, type);
        const auto now = subEmit(id, Op::Load, {clock}, type);
        subEmit(id, Op::Fstore, {field(0), scalar(type, 1)});
        const auto value = subEmit(id, Op::Xor, {old, now}, type);
        subEmit(id, Op::Ret, {value});
        return s::SubroutineId{id};
    }
    Operand layout(s::NbaLayout value) {
        auto id = s::NbaLayoutId{static_cast<std::uint32_t>(d.nbaLayouts.size())};
        d.nbaLayouts.push_back(std::move(value));
        return Operand::nbaLayout(id);
    }
    void wait(unsigned process, unsigned bb, std::uint64_t ticks, unsigned next) {
        emit(process, bb, Op::WaitDelay, {this->ticks(ticks), block(next)});
    }
    ::testing::AssertionResult valid() {
        for (auto& p : d.processes) {
            p.writes.clear();
            for (unsigned i = 0; i < d.slots.size(); ++i)
                p.writes.push_back({s::SlotId{i}});
        }
        const auto errors = s::verify(d);
        if (errors.empty()) return ::testing::AssertionSuccess();
        std::string out;
        for (const auto& e : errors)
            out += e.where + ": " + e.message + "\n";
        return ::testing::AssertionFailure() << out;
    }
};
std::vector<std::uint64_t> words(const k::Context& c, const Operand& slot, unsigned elem = 0) {
    const auto& store = c.store();
    const auto n = store.layout(k::SlotId{slot.id}).elemWords;
    const auto* p = store.element(k::SlotId{slot.id}, elem);
    return {p, p + n};
}
std::uint64_t counter(const k::Context& c, const char* name) {
    const auto& counters = c.counters();
    for (std::uint32_t i = 0; i < counters.size(); ++i)
        if (counters.name(k::CounterId{i}) == name) return counters.value(k::CounterId{i});
    return 0;
}
struct Capture {
    std::FILE* file = std::tmpfile();
    ~Capture() {
        if (file != nullptr) std::fclose(file);
    }
    std::string text() {
        EXPECT_EQ(std::fflush(file), 0);
        EXPECT_EQ(std::fseek(file, 0, SEEK_SET), 0);
        std::string result;
        char buffer[512];
        while (const auto n = std::fread(buffer, 1, sizeof buffer, file))
            result.append(buffer, n);
        return result;
    }
};

TEST(VmNbaEvent, FreshCallbacksRetainOriginAndDoNotResumeOrCreateProcesses) {
    unsigned cases = 0;
    for (bool program : {false, true})
        for (unsigned form = 0; form < 4; ++form)
            for (bool ended : {false, true}) {
                SCOPED_TRACE(::testing::Message() << program << ':' << form << ':' << ended);
                Model m(program);
                const auto out = m.slot(s::u4(129)), clock = m.slot(s::u2(129));
                const auto named = m.slot(event), proof = m.slot(s::u2(129));
                const auto payload = m.constant(s::u4(129), {7, UINT64_MAX, 1});
                const auto saved = m.constant(s::u2(129), {99, 5, 1});
                m.d.processes[0].frame.push_back({"issuer", s::u2(129)});
                m.emit(0, 0, Op::Store, {out, m.scalar(s::u4(129), 0)});
                m.emit(0, 0, Op::Store, {clock, m.scalar(s::u2(129), 0)});
                m.emit(0, 0, Op::Fstore, {field(0), saved});
                const auto eventValue = m.emit(0, 0, Op::Load, {named}, event);
                const auto guard = m.freshGuard(), watch = m.freshWatch(clock, s::u2(129));
                s::NbaLayout layout;
                layout.values = {s::u4(129), event};
                layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
                if (form == 0 || form == 3)
                    layout.events.push_back(
                        s::NbaSlotEvent{{s::SlotId{clock.id}}, s::Edge::Any, guard});
                if (form == 1 || form == 3)
                    layout.events.push_back(s::NbaNamedEvent{captured(1), guard});
                if (form == 2 || form == 3)
                    layout.events.push_back(s::NbaWatchEvent{watch, s::Edge::Any, guard});
                m.emit(0, 0, Op::NbaEvent,
                       {m.layout(std::move(layout)), m.scalar(s::s4(65), 2), payload, eventValue});
                if (ended) {
                    m.emit(0, 0, Op::Ret);
                } else {
                    // An ordinary wait must not replace independent callback state.
                    if (form == 1 || form == 3)
                        m.emit(0, 0, Op::WaitNamed, {named, block(1)});
                    else
                        m.wait(0, 0, 1, 1);
                    const auto value = m.emit(0, 1, Op::Fload, {field(0)}, s::u2(129));
                    m.emit(0, 1, Op::Store, {proof, value});
                    m.wait(0, 1, 2, 2);
                    m.emit(0, 2, Op::Ret);
                }
                m.wait(1, 0, 1, 1);
                if (form == 1) {
                    m.emit(1, 1, Op::Trigger, {named});
                    m.emit(1, 1, Op::Trigger, {named});
                } else {
                    m.emit(1, 1, Op::Store, {clock, m.scalar(s::u2(129), 1)});
                    m.emit(1, 1, Op::Store, {clock, m.scalar(s::u2(129), 0)});
                }
                if (form == 3) m.emit(1, 1, Op::Trigger, {named});
                m.wait(1, 1, 2, 2);  // Keep a program alive through its Re-NBA.
                m.emit(1, 2, Op::Ret);
                ASSERT_TRUE(m.valid());
                k::Context c;
                const auto result = kirimasim::vm::run(m.d, c);
                ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                    << result.error;
                EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{7, 0, UINT64_MAX, 0, 1, 0}));
                if (!ended) EXPECT_EQ(words(c, proof), (std::vector<std::uint64_t>{99, 5, 1}));
                EXPECT_EQ(counter(c, "proc.created"), 2U);
                EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
                EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
                ++cases;
            }
    EXPECT_EQ(cases, 16U);
}

TEST(VmNbaEvent, RegistrationPrecedesTheNextInstructionAndKeepsTheIssuerRunning) {
    for (bool program : {false, true}) {
        Model m(program);
        const auto out = m.slot(s::u2(32)), named = m.slot(event), proof = m.slot(s::u2(32));
        s::NbaLayout layout;
        layout.values = {s::u2(32), event};
        layout.events = {s::NbaNamedEvent{captured(1), m.freshGuard()}};
        layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
        const auto object = m.emit(0, 0, Op::Load, {named}, event);
        m.emit(0, 0, Op::NbaEvent,
               {m.layout(std::move(layout)), m.scalar(s::u2(129), 2), m.scalar(s::u2(32), 42),
                object});
        m.emit(0, 0, Op::Trigger, {named});
        m.emit(0, 0, Op::Trigger, {named});
        m.emit(0, 0, Op::Store, {proof, m.scalar(s::u2(32), 99)});
        m.emit(0, 0, Op::Ret);
        m.wait(1, 0, 1, 1);
        m.emit(1, 1, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
            << result.error;
        EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
        EXPECT_EQ(words(c, proof), (std::vector<std::uint64_t>{99}));
        EXPECT_EQ(counter(c, "proc.created"), 2U);
    }
}

TEST(VmNbaEvent, SkippedCountsDoNotEnterOrInitializeCallbacks) {
    unsigned cases = 0;
    for (bool program : {false, true})
        for (unsigned shape = 0; shape < 10; ++shape) {
            Model m(program);
            const auto out = m.slot(s::u2(32));
            Operand repeat;
            switch (shape) {
                case 0: repeat = m.scalar(s::u2(1), 0); break;
                case 1: repeat = m.scalar(s::s2(1), 1); break;
                case 2: repeat = m.scalar(s::s2(32), UINT32_MAX); break;
                case 3: repeat = m.constant(s::s2(65), {0, 1}); break;
                case 4: repeat = m.constant(s::s4(129), {0, 0, 1}); break;
                case 5: repeat = m.constant(s::u4(1), {0}, {1}); break;
                case 6: repeat = m.constant(s::u4(1), {1}, {1}); break;
                case 7: repeat = m.constant(s::u4(65), {4, 0}, {0, 1}); break;
                case 8: repeat = m.constant(s::u4(129), {4, 0, 0}, {0, 1, 0}); break;
                default: repeat = m.constant(s::u2(129), {0, 0, 0}); break;
            }
            const auto leaf = m.function(s::u2(1));
            m.subEmit(leaf, Op::Ret, {m.scalar(s::u2(1), 0)});
            const auto watch = m.function(s::u2(1));
            // With maxCallDepth=0, even one callback initialization would fail.
            const auto value =
                m.subEmit(watch, Op::Call, {Operand::subroutine(s::SubroutineId{leaf})}, s::u2(1));
            m.subEmit(watch, Op::Ret, {value});
            s::NbaLayout layout;
            layout.values = {s::u2(32)};
            layout.events = {s::NbaWatchEvent{s::SubroutineId{watch}}};
            layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
            m.emit(0, 0, Op::NbaEvent,
                   {m.layout(std::move(layout)), repeat, m.scalar(s::u2(32), 42)});
            m.emit(0, 0, Op::Ret);
            m.wait(1, 0, 1, 1);
            m.emit(1, 1, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context c;
            kirimasim::vm::RunOptions options;
            options.maxCallDepth = 0;
            const auto result = kirimasim::vm::run(m.d, c, options);
            ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
            EXPECT_EQ(counter(c, "proc.created"), 2U);
            EXPECT_EQ(c.scheduler().nbaEventStats().records, 0U);
            ++cases;
        }
    EXPECT_EQ(cases, 20U);
}

TEST(VmNbaEvent, WidePositiveCountsAreNeitherTruncatedNorSaturated) {
    for (bool program : {false, true})
        for (unsigned width : {65U, 129U}) {
            Model m(program);
            const auto out = m.slot(s::u2(32)), named = m.slot(event);
            std::vector<std::uint64_t> count((width + 63) / 64, 0);
            count.back() = 1;
            s::NbaLayout layout;
            layout.values = {s::u2(32), event};
            layout.events = {s::NbaNamedEvent{captured(1), m.freshGuard()}};
            layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
            const auto object = m.emit(0, 0, Op::Load, {named}, event);
            m.emit(0, 0, Op::NbaEvent,
                   {m.layout(std::move(layout)), m.constant(s::u2(width), std::move(count)),
                    m.scalar(s::u2(32), 42), object});
            m.emit(0, 0, Op::Ret);
            m.wait(1, 0, 1, 1);
            m.emit(1, 1, Op::Trigger, {named});
            m.emit(1, 1, Op::Trigger, {named});
            m.wait(1, 1, 1, 2);
            m.emit(1, 2, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context c;
            const auto result = kirimasim::vm::run(m.d, c);
            ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{0}));
            EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
            EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        }
}

TEST(VmNbaEvent, WatchCallbacksRetainEveryHighValueAndUnknownBit) {
    unsigned cases = 0;
    for (bool program : {false, true})
        for (unsigned width : {65U, 129U})
            for (unsigned mode = 0; mode < 3; ++mode) {
                Model m(program);
                const auto type = mode == 0 ? s::u2(width) : s::u4(width);
                const auto clock = m.slot(type), out = m.slot(s::u2(32));
                const auto watch = m.function(type);
                const auto now = m.subEmit(watch, Op::Load, {clock}, type);
                m.subEmit(watch, Op::Ret, {now});
                const auto zero = m.scalar(type, 0);
                std::vector<std::uint64_t> high((width + 63) / 64, 0);
                high.back() = 1;
                const auto change =
                    mode == 2 ? m.constant(type, std::vector<std::uint64_t>(high.size(), 0), high)
                              : m.constant(type, high);
                s::NbaLayout layout;
                layout.values = {s::u2(32)};
                layout.events = {s::NbaWatchEvent{s::SubroutineId{watch}}};
                layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
                m.emit(0, 0, Op::Store, {clock, zero});
                m.emit(
                    0, 0, Op::NbaEvent,
                    {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), m.scalar(s::u2(32), 42)});
                m.emit(0, 0, Op::Ret);
                m.wait(1, 0, 1, 1);
                m.emit(1, 1, Op::Store, {clock, change});
                m.emit(1, 1, Op::Store, {clock, zero});
                m.wait(1, 1, 1, 2);
                m.emit(1, 2, Op::Ret);
                ASSERT_TRUE(m.valid());
                k::Context c;
                const auto result = kirimasim::vm::run(m.d, c);
                ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                    << result.error;
                EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
                ++cases;
            }
    EXPECT_EQ(cases, 12U);
}

TEST(VmNbaEvent, CallbacksContinueTheOriginRngWithoutSeedingOrDrawingFromTheTrigger) {
    unsigned cases = 0;
    for (bool program : {false, true})
        for (bool ended : {false, true})
            for (bool nested : {false, true})
                for (std::uint64_t seed : {0ULL, 19ULL, UINT64_MAX}) {
                    SCOPED_TRACE(::testing::Message() << program << ended << nested << seed);
                    Model m(program);
                    k::Rng initialization{k::initialState(seed, "top")};
                    auto origin = initialization.child(), emitter = initialization.child();
                    const auto first = origin.draw(), second = origin.draw();
                    const auto third = origin.draw(), fourth = origin.draw(), fifth = origin.draw();
                    ASSERT_NE(third, fourth);
                    const auto out = m.slot(s::u2(32)), named = m.slot(event);
                    const auto before = m.slot(s::u2(32)), after = m.slot(s::u2(32));
                    const auto resumed = m.slot(s::u2(32)), triggerProof = m.slot(s::u2(32));
                    const auto leaf = m.function(s::u2(1));
                    const auto draw =
                        m.subEmit(leaf, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                    const auto isThird =
                        m.subEmit(leaf, Op::Eq, {draw, m.scalar(s::u2(32), third)}, s::u2(1));
                    const auto isFourth =
                        m.subEmit(leaf, Op::Eq, {draw, m.scalar(s::u2(32), fourth)}, s::u2(1));
                    const auto accepted = m.subEmit(leaf, Op::Or, {isThird, isFourth}, s::u2(1));
                    m.subEmit(leaf, Op::Ret, {accepted});
                    const auto guard = m.function(s::u2(1));
                    const auto called = m.subEmit(
                        guard, Op::Call, {Operand::subroutine(s::SubroutineId{leaf})}, s::u2(1));
                    m.subEmit(guard, Op::Ret, {called});
                    const auto a = m.emit(0, 0, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                    m.emit(0, 0, Op::Store, {before, a});
                    const auto object = m.emit(0, 0, Op::Load, {named}, event);
                    s::NbaLayout layout;
                    layout.values = {s::u2(32), event};
                    layout.events = {s::NbaNamedEvent{captured(1), s::SubroutineId{guard}}};
                    layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
                    m.emit(0, 0, Op::NbaEvent,
                           {m.layout(std::move(layout)), m.scalar(s::u2(32), 2),
                            m.scalar(s::u2(32), 42), object});
                    const auto b = m.emit(0, 0, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                    m.emit(0, 0, Op::Store, {after, b});
                    if (ended) {
                        m.emit(0, 0, Op::Ret);
                    } else {
                        m.wait(0, 0, 2, 1);
                        const auto c =
                            m.emit(0, 1, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                        m.emit(0, 1, Op::Store, {resumed, c});
                        m.emit(0, 1, Op::Ret);
                    }
                    m.wait(1, 0, 1, 1);
                    if (nested) {
                        const auto task = m.function(s::voidType());
                        m.d.subroutines[task].kind = s::SubroutineKind::Task;
                        m.subEmit(task, Op::Trigger, {named});
                        m.subEmit(task, Op::Trigger, {named});
                        const auto c =
                            m.subEmit(task, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                        m.subEmit(task, Op::Store, {triggerProof, c});
                        m.subEmit(task, Op::Ret);
                        m.emit(1, 1, Op::Call, {Operand::subroutine(s::SubroutineId{task})});
                    } else {
                        m.emit(1, 1, Op::Trigger, {named});
                        m.emit(1, 1, Op::Trigger, {named});
                        const auto c =
                            m.emit(1, 1, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
                        m.emit(1, 1, Op::Store, {triggerProof, c});
                    }
                    m.wait(1, 1, 2, 2);
                    m.emit(1, 2, Op::Ret);
                    ASSERT_TRUE(m.valid());
                    k::Context context;
                    kirimasim::vm::RunOptions options;
                    options.seed = seed;
                    options.maxCallDepth = 1;  // Only the real source call consumes depth.
                    const auto result = kirimasim::vm::run(m.d, context, options);
                    ASSERT_EQ(result.reason,
                              program ? k::EndReason::Finish : k::EndReason::Quiescent)
                        << result.error;
                    EXPECT_EQ(words(context, out), (std::vector<std::uint64_t>{42}));
                    EXPECT_EQ(words(context, before), (std::vector<std::uint64_t>{first}));
                    EXPECT_EQ(words(context, after), (std::vector<std::uint64_t>{second}));
                    if (!ended)
                        EXPECT_EQ(words(context, resumed), (std::vector<std::uint64_t>{fifth}));
                    EXPECT_EQ(words(context, triggerProof),
                              (std::vector<std::uint64_t>{emitter.draw()}));
                    EXPECT_EQ(counter(context, "proc.created"), 2U);
                    ++cases;
                }
    EXPECT_EQ(cases, 24U);
}

TEST(VmNbaEvent, OneControlOwnsOrderedWholeElementBitAndReferenceWrites) {
    for (bool program : {false, true}) {
        Model m(program);
        const auto wide = s::u4(129);
        const auto whole = m.slot(wide), bits = m.slot(wide), reference = m.slot(wide);
        const auto array = m.slot(s::u2(32), {{3, 2}, {-1, 0}});
        const auto named = m.slot(event), rhs = m.slot(wide), index = m.slot(s::u2(64));
        const auto zero = m.scalar(wide, 0), payload = m.constant(wide, {7, 0, 1});
        for (const auto& target : {whole, bits, reference})
            m.emit(0, 0, Op::Store, {target, zero});
        m.emit(0, 0, Op::Store, {rhs, payload});
        m.emit(0, 0, Op::Store, {index, m.ticks(2)});
        const auto frozen = m.emit(0, 0, Op::Load, {rhs}, wide);
        const auto selected = m.emit(0, 0, Op::Load, {index}, s::u2(64));
        const auto ref = m.emit(0, 0, Op::RefMake, {reference}, s::refType());
        const auto object = m.emit(0, 0, Op::Load, {named}, event);
        s::NbaLayout layout;
        layout.values = {wide,     s::u2(64),    s::s2(64), s::u2(32), s::s2(64),
                         s::u4(9), s::refType(), event,     s::s2(64)};
        layout.events = {s::NbaNamedEvent{captured(7), m.freshGuard()}};
        layout.targets = {
            s::NbaSlotTarget{{s::SlotId{whole.id}}, captured(0)},
            s::NbaElementTarget{s::SlotId{array.id}, {captured(1), captured(2)}, captured(3)},
            s::NbaSlotTarget{{s::SlotId{array.id}, 1, 1}, captured(3)},
            s::NbaBitsTarget{{s::SlotId{bits.id}, 0, 0, 4, 8}, {}, captured(4), captured(5)},
            s::NbaRefTarget{captured(6), 0, wide, captured(0)},
            s::NbaRefBitsTarget{captured(6), 0, wide, captured(8), captured(5)},
        };
        m.emit(0, 0, Op::NbaEvent,
               {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), frozen, selected,
                m.scalar(s::s2(64), 0), m.scalar(s::u2(32), 99),
                m.scalar(s::s2(64), UINT64_MAX - 1), m.constant(s::u4(9), {0x1ad}, {0x102}), ref,
                object, m.scalar(s::s2(64), 127)});
        m.emit(0, 0, Op::Store, {rhs, zero});
        m.emit(0, 0, Op::Store, {index, m.ticks(3)});
        m.emit(0, 0, Op::Ret);
        m.wait(1, 0, 1, 1);
        m.emit(1, 1, Op::Trigger, {named});
        m.emit(1, 1, Op::Trigger, {named});
        m.wait(1, 1, 1, 2);
        m.emit(1, 2, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
            << result.error;
        EXPECT_EQ(words(c, whole), (std::vector<std::uint64_t>{7, 0, 0, 0, 1, 0}));
        EXPECT_EQ(words(c, bits), (std::vector<std::uint64_t>{0x6b0, 0x400, 0, 0, 0, 0}));
        EXPECT_EQ(words(c, reference),
                  (std::vector<std::uint64_t>{7, 0, std::uint64_t{1} << 63, 0, 0, 1}));
        for (unsigned i = 0; i < 4; ++i)
            EXPECT_EQ(words(c, array, i),
                      (std::vector<std::uint64_t>{i == 1 || i == 3 ? 99U : 0U}));
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
    }
}

TEST(VmNbaEvent, HeapSnapshotsStayRootedAndDestinationsOwnIndependentCopies) {
    for (bool program : {false, true}) {
        Model m(program);
        const auto wide = s::u4(129);
        s::TypeEntry row;
        row.kind = s::TypeEntryKind::Queue;
        row.name = "words";
        row.value = wide;
        m.d.types.push_back(row);
        const auto queue = s::queueType(s::TypeId{0});
        const s::Type real{s::TypeKind::Real, 0}, string{s::TypeKind::String, 0};
        const auto source = m.slot(queue), a = m.slot(queue), b = m.slot(queue);
        const auto named = m.slot(event), realOut = m.slot(real), stringOut = m.slot(string);
        const auto first = m.constant(wide, {7, 0, 1}), second = m.constant(wide, {11, 3, 0});
        const auto changed = m.constant(wide, {99, 0, 0});
        const auto packed = m.emit(0, 0, Op::ContainerPack, {first, second}, queue);
        m.emit(0, 0, Op::Store, {source, packed});
        const auto snapshot = m.emit(0, 0, Op::ContainerSnapshot, {packed}, queue);
        const auto object = m.emit(0, 0, Op::Load, {named}, event);
        const auto text = m.constant(string, {});
        m.d.constants[text.id].bytes = "owned snapshot";
        s::NbaLayout layout;
        layout.values = {queue, event, real, string};
        layout.events = {s::NbaNamedEvent{captured(1), m.freshGuard()}};
        layout.targets = {s::NbaSlotTarget{{s::SlotId{a.id}}, captured(0)},
                          s::NbaSlotTarget{{s::SlotId{b.id}}, captured(0)},
                          s::NbaSlotTarget{{s::SlotId{realOut.id}}, captured(2)},
                          s::NbaSlotTarget{{s::SlotId{stringOut.id}}, captured(3)}};
        m.emit(0, 0, Op::NbaEvent,
               {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), snapshot, object,
                m.scalar(real, std::bit_cast<std::uint64_t>(2.5)), text});
        const auto ref = m.emit(0, 0, Op::RefMake, {source}, s::refType(queue.detail));
        const auto cell = m.emit(0, 0, Op::ContainerCell,
                                 {Operand::typeRow(queue.detail), ref, m.ticks(0)}, s::refType());
        m.emit(0, 0, Op::StoreRef, {cell, changed});
        m.emit(0, 0, Op::Store, {source, m.scalar(queue, 0)});
        m.emit(0, 0, Op::Ret);
        m.wait(1, 0, 1, 1);
        m.emit(1, 1, Op::Trigger, {named});
        m.emit(1, 1, Op::Trigger, {named});
        m.wait(1, 1, 1, 2);
        const auto dest = m.emit(1, 2, Op::RefMake, {a}, s::refType(queue.detail));
        const auto destCell =
            m.emit(1, 2, Op::ContainerCell, {Operand::typeRow(queue.detail), dest, m.ticks(0)},
                   s::refType());
        m.emit(1, 2, Op::StoreRef, {destCell, changed});
        m.emit(1, 2, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
            << result.error;
        const k::ContainerToken aToken{words(c, a)[0]}, bToken{words(c, b)[0]};
        EXPECT_NE(aToken, bToken);
        const auto type = c.heap().actualType(aToken);
        ASSERT_TRUE(type);
        for (unsigned destination = 0; destination < 2; ++destination)
            for (unsigned index = 0; index < 2; ++index) {
                k::W4 data[3]{};
                ASSERT_TRUE(
                    c.heap().read(type.value, destination == 0 ? aToken : bToken, index, data));
                const auto expected = index == 1 ? second : destination == 0 ? changed : first;
                for (unsigned chunk = 0; chunk < 3; ++chunk) {
                    EXPECT_EQ(data[chunk].a, m.d.constants[expected.id].aWords[chunk]);
                    EXPECT_EQ(data[chunk].b, 0U);
                }
            }
        EXPECT_EQ(words(c, source), (std::vector<std::uint64_t>{0}));
        EXPECT_EQ(c.heap().stats().containers, 2U);
        EXPECT_EQ(c.heap().stats().cells, 4U);
        EXPECT_EQ(words(c, realOut),
                  (std::vector<std::uint64_t>{std::bit_cast<std::uint64_t>(2.5)}));
        EXPECT_EQ(c.strings().get(words(c, stringOut)[0]), "owned snapshot");
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
    }
}

TEST(VmNbaEvent, CallbackContextsDoNotBorrowTheReturnedAutomaticTaskFrame) {
    for (bool program : {false, true}) {
        Model m(program);
        const auto out = m.slot(s::u2(32)), named = m.slot(event), proof = m.slot(s::u2(32));
        const auto guard = m.freshGuard();
        const auto task = m.function(s::voidType(), s::u2(32));
        m.d.subroutines[task].kind = s::SubroutineKind::Task;
        const auto object = m.subEmit(task, Op::Load, {named}, event);
        m.subEmit(task, Op::Fstore, {field(0), m.scalar(s::u2(32), 99)});
        s::NbaLayout layout;
        layout.values = {s::u2(32), event};
        layout.events = {s::NbaNamedEvent{captured(1), guard}};
        layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
        m.subEmit(
            task, Op::NbaEvent,
            {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), m.scalar(s::u2(32), 42), object});
        m.subEmit(task, Op::Ret);
        m.emit(0, 0, Op::Call, {Operand::subroutine(s::SubroutineId{task})});
        m.emit(0, 0, Op::Store, {proof, m.scalar(s::u2(32), 72)});
        m.emit(0, 0, Op::Ret);
        m.wait(1, 0, 1, 1);
        m.emit(1, 1, Op::Trigger, {named});
        m.emit(1, 1, Op::Trigger, {named});
        m.wait(1, 1, 1, 2);
        m.emit(1, 2, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
            << result.error;
        EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
        EXPECT_EQ(words(c, proof), (std::vector<std::uint64_t>{72}));
        EXPECT_EQ(counter(c, "proc.created"), 2U);
    }
}

TEST(VmNbaEvent, InvalidActualReferencesProduceDesignErrorsBeforePublishing) {
    for (unsigned shape = 0; shape < 4; ++shape) {
        Model m;
        const auto out = m.slot(shape == 1 ? s::u2(64) : s::u2(32)), named = m.slot(event);
        Operand home = out;
        if (shape == 0) {
            m.d.processes[0].frame.push_back({"automatic", s::u2(32)});
            home = field(0);
        }
        if (shape == 2) m.d.slots[out.id].kind = s::SlotKind::Wire;
        const auto reference = m.emit(0, 0, Op::RefMake, {home}, s::refType());
        const auto object = m.emit(0, 0, Op::Load, {named}, event);
        s::NbaLayout layout;
        layout.values = {s::refType(), s::u2(32), event};
        layout.events = {s::NbaNamedEvent{captured(2)}};
        layout.targets = {
            s::NbaRefTarget{captured(0), shape == 3 ? UINT32_MAX : 0, s::u2(32), captured(1)}};
        m.emit(0, 0, Op::NbaEvent,
               {m.layout(std::move(layout)), m.scalar(s::u2(32), 1), reference,
                m.scalar(s::u2(32), 42), object});
        m.emit(0, 0, Op::Ret);
        m.emit(1, 0, Op::Ret);
        ASSERT_TRUE(m.valid());
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        k::Context c(k::OutputDestinations{capture.file, capture.file});
        const auto result = kirimasim::vm::run(m.d, c);
        EXPECT_EQ(result.reason, k::EndReason::Error) << shape << ':' << result.error;
        EXPECT_NE(capture.text().find("event-NBA"), std::string::npos);
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
    }
}

TEST(VmNbaEvent, DuplicateItemsEvaluateBothGuardsInOrderAndCountOneNotification) {
    for (bool program : {false, true}) {
        Model m(program);
        k::Rng initialization{k::initialState(0, "top")};
        auto rng = initialization.child();
        const auto first = rng.draw(), second = rng.draw(), third = rng.draw(), fourth = rng.draw();
        const auto fifth = rng.draw();
        const auto out = m.slot(s::u2(32)), named = m.slot(event), between = m.slot(s::u2(32));
        const auto proof = m.slot(s::u2(32));
        std::vector<s::NbaEventItem> events;
        for (unsigned item = 0; item < 2; ++item) {
            const auto guard = m.function(s::u2(1));
            const auto draw = m.subEmit(guard, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
            const auto a = m.subEmit(
                guard, Op::Eq, {draw, m.scalar(s::u2(32), item == 0 ? first : second)}, s::u2(1));
            const auto b = m.subEmit(
                guard, Op::Eq, {draw, m.scalar(s::u2(32), item == 0 ? third : fourth)}, s::u2(1));
            const auto accepted = m.subEmit(guard, Op::Or, {a, b}, s::u2(1));
            m.subEmit(guard, Op::Ret, {accepted});
            events.push_back(s::NbaNamedEvent{captured(1), s::SubroutineId{guard}});
        }
        s::NbaLayout layout;
        layout.values = {s::u2(32), event};
        layout.events = std::move(events);
        layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
        const auto object = m.emit(0, 0, Op::Load, {named}, event);
        m.emit(
            0, 0, Op::NbaEvent,
            {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), m.scalar(s::u2(32), 42), object});
        m.wait(0, 0, 3, 1);
        const auto next = m.emit(0, 1, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
        m.emit(0, 1, Op::Store, {proof, next});
        m.emit(0, 1, Op::Ret);
        m.wait(1, 0, 1, 1);
        m.emit(1, 1, Op::Trigger, {named});
        m.wait(1, 1, 1, 2);
        const auto old = m.emit(1, 2, Op::Load, {out}, s::u2(32));
        m.emit(1, 2, Op::Store, {between, old});
        m.emit(1, 2, Op::Trigger, {named});
        m.wait(1, 2, 2, 3);
        m.emit(1, 3, Op::Ret);
        ASSERT_TRUE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
            << result.error;
        EXPECT_EQ(words(c, between), (std::vector<std::uint64_t>{0}));
        EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
        EXPECT_EQ(words(c, proof), (std::vector<std::uint64_t>{fifth}));
    }
}

TEST(VmNbaEvent, EmptyAndInvalidDestinationsStillEvaluateTheControl) {
    for (bool program : {false, true})
        for (bool empty : {false, true}) {
            Model m(program);
            const auto array = m.slot(s::u2(32), {{0, 1}}), bits = m.slot(s::u4(16));
            const auto named = m.slot(event), proof = m.slot(s::u2(32));
            const auto guard = m.function(s::u2(1));
            m.subEmit(guard, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
            m.subEmit(guard, Op::Ret, {m.scalar(s::u2(1), 1)});
            const auto object = m.emit(0, 0, Op::Load, {named}, event);
            s::NbaLayout layout;
            layout.values = {s::u2(32), s::u4(65), s::u4(4), event};
            layout.events = {s::NbaNamedEvent{captured(3), s::SubroutineId{guard}}};
            if (!empty)
                layout.targets = {
                    s::NbaElementTarget{s::SlotId{array.id}, {captured(1)}, captured(0)},
                    s::NbaBitsTarget{{s::SlotId{bits.id}}, {}, captured(1), captured(2)}};
            const auto zero = m.scalar(s::u4(16), 0);
            m.emit(0, 0, Op::Store, {bits, zero});
            m.emit(0, 0, Op::NbaEvent,
                   {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), m.scalar(s::u2(32), 42),
                    m.constant(s::u4(65), {0, 0}, {0, 1}), m.scalar(s::u4(4), 7), object});
            m.wait(0, 0, 2, 1);
            const auto value = m.emit(0, 1, Op::Syscall, {Operand::str("$urandom")}, s::u2(32));
            m.emit(0, 1, Op::Store, {proof, value});
            m.emit(0, 1, Op::Ret);
            m.wait(1, 0, 1, 1);
            m.emit(1, 1, Op::Trigger, {named});
            m.emit(1, 1, Op::Trigger, {named});
            m.wait(1, 1, 2, 2);
            m.emit(1, 2, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Rng initialization{k::initialState(0, "top")};
            auto rng = initialization.child();
            static_cast<void>(rng.draw());
            static_cast<void>(rng.draw());
            k::Context c;
            const auto result = kirimasim::vm::run(m.d, c);
            ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(words(c, proof), (std::vector<std::uint64_t>{rng.draw()}));
            EXPECT_EQ(words(c, array, 0), (std::vector<std::uint64_t>{0}));
            EXPECT_EQ(words(c, array, 1), (std::vector<std::uint64_t>{0}));
            EXPECT_EQ(words(c, bits), (std::vector<std::uint64_t>{0, 0}));
            EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
            EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
        }
}

TEST(VmNbaEvent, HeapReadCaptureWorksWithFreshCallbacksAndCalledFunctions) {
    for (bool program : {false, true})
        for (bool called : {false, true}) {
            Model m(program);
            s::TypeEntry row;
            row.kind = s::TypeEntryKind::Queue;
            row.name = "values";
            row.value = s::u2(32);
            m.d.types.push_back(row);
            const auto queue = s::queueType(s::TypeId{0});
            const auto source = m.slot(queue), out = m.slot(s::u2(32));
            const auto leaf = m.function(s::u2(64));
            const auto value = m.subEmit(leaf, Op::Load, {source}, queue);
            const auto size = m.subEmit(leaf, Op::ContainerSize, {value}, s::u2(64));
            m.subEmit(leaf, Op::Ret, {size});
            auto watch = leaf;
            if (called) {
                watch = m.function(s::u2(64));
                const auto value = m.subEmit(
                    watch, Op::Call, {Operand::subroutine(s::SubroutineId{leaf})}, s::u2(64));
                m.subEmit(watch, Op::Ret, {value});
            }
            s::NbaLayout layout;
            layout.values = {s::u2(32)};
            layout.events = {s::NbaWatchEvent{s::SubroutineId{watch}}};
            layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
            m.emit(0, 0, Op::NbaEvent,
                   {m.layout(std::move(layout)), m.scalar(s::u2(32), 2), m.scalar(s::u2(32), 42)});
            m.emit(0, 0, Op::Ret);
            m.wait(1, 0, 1, 1);
            const auto ref = m.emit(1, 1, Op::RefMake, {source}, s::refType(queue.detail));
            for (unsigned n : {7U, 11U})
                m.emit(1, 1, Op::QueuePushBack,
                       {Operand::typeRow(queue.detail), ref, m.scalar(s::u2(32), n)});
            m.wait(1, 1, 1, 2);
            m.emit(1, 2, Op::Ret);
            ASSERT_TRUE(m.valid());
            k::Context c;
            const auto result = kirimasim::vm::run(m.d, c);
            ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(words(c, out), (std::vector<std::uint64_t>{42}));
            EXPECT_EQ(c.heap().stats().containers, 1U);
            EXPECT_EQ(c.heap().stats().cells, 2U);
            EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
        }
}
TEST(VmNbaEvent, BoundsWarningsShareTheExistingLocationRuleAndPreserveWrites) {
    for (bool program : {false, true})
        for (bool warnings : {false, true}) {
            Model m(program);
            const auto array = m.slot(s::u2(32), {{0, 1}}), bits = m.slot(s::u4(16));
            const auto refBits = m.slot(s::u4(16)), named = m.slot(event);
            const auto object = m.emit(0, 0, Op::Load, {named}, event);
            const auto ref = m.emit(0, 0, Op::RefMake, {refBits}, s::refType());
            const auto zero = m.scalar(s::u4(16), 0);
            m.emit(0, 0, Op::Store, {bits, zero});
            m.emit(0, 0, Op::Store, {refBits, zero});
            s::NbaLayout layout;
            layout.values = {s::u2(32), s::u4(65),    s::s2(64), s::u4(4),
                             event,     s::refType(), s::s2(64)};
            layout.events = {s::NbaNamedEvent{captured(4)}};
            layout.targets = {
                s::NbaElementTarget{s::SlotId{array.id}, {captured(1)}, captured(0)},
                s::NbaBitsTarget{{s::SlotId{bits.id}, 0, 0, 4, 8}, {}, captured(2), captured(3)},
                s::NbaRefBitsTarget{captured(5), 0, s::u4(16), captured(6), captured(3)}};
            m.emit(0, 0, Op::NbaEvent,
                   {m.layout(std::move(layout)), m.scalar(s::u2(32), 1), m.scalar(s::u2(32), 42),
                    m.constant(s::u4(65), {0, 0}, {0, 1}), m.scalar(s::s2(64), UINT64_MAX - 1),
                    m.scalar(s::u4(4), 11), object, ref, m.scalar(s::s2(64), 15)});
            m.emit(0, 0, Op::Ret);
            m.wait(1, 0, 1, 1);
            m.emit(1, 1, Op::Trigger, {named});
            m.wait(1, 1, 1, 2);
            m.emit(1, 2, Op::Ret);
            ASSERT_TRUE(m.valid());
            Capture capture;
            ASSERT_NE(capture.file, nullptr);
            k::Context c(k::OutputDestinations{capture.file, capture.file});
            kirimasim::vm::RunOptions options;
            options.boundsCheck = warnings;
            const auto result = kirimasim::vm::run(m.d, c, options);
            ASSERT_EQ(result.reason, program ? k::EndReason::Finish : k::EndReason::Quiescent)
                << result.error;
            EXPECT_EQ(words(c, array, 0), (std::vector<std::uint64_t>{0}));
            EXPECT_EQ(words(c, array, 1), (std::vector<std::uint64_t>{0}));
            EXPECT_EQ(words(c, bits), (std::vector<std::uint64_t>{0x20, 0}));
            EXPECT_EQ(words(c, refBits), (std::vector<std::uint64_t>{0x8000, 0}));
            const auto text = capture.text();
            if (warnings) {
                const auto first = text.find("--bounds-check=warn");
                ASSERT_NE(first, std::string::npos);
                EXPECT_EQ(text.find("--bounds-check=warn", first + 1), std::string::npos);
                EXPECT_NE(text.find("3 invalid accesses at 1 source positions"), std::string::npos);
            } else {
                EXPECT_TRUE(text.empty());
            }
        }
}
struct Memory : std::pmr::memory_resource {
    std::uint64_t calls = 0, failAt = 0, bytes = 0;
    void* do_allocate(std::size_t size, std::size_t align) override {
        if (++calls == failAt) throw std::bad_alloc{};
        auto* p = std::pmr::new_delete_resource()->allocate(size, align);
        bytes += size;
        return p;
    }
    void do_deallocate(void* p, std::size_t size, std::size_t align) override {
        bytes -= size;
        std::pmr::new_delete_resource()->deallocate(p, size, align);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
TEST(VmNbaEvent, ContextAllocationFailuresReleasePublishedAndUnpublishedOwners) {
    Model m;
    s::TypeEntry row;
    row.kind = s::TypeEntryKind::Queue;
    row.name = "values";
    row.value = s::u4(129);
    m.d.types.push_back(row);
    const auto queue = s::queueType(s::TypeId{0});
    const auto source = m.slot(queue), out = m.slot(queue);
    const auto initial = m.constant(s::u4(129), {7, 0, 1});
    const auto packed = m.emit(0, 0, Op::ContainerPack, {initial, initial}, queue);
    m.emit(0, 0, Op::Store, {source, packed});
    const auto snapshot = m.emit(0, 0, Op::ContainerSnapshot, {packed}, queue);
    const auto watch = m.function(s::u2(64));
    const auto q = m.subEmit(watch, Op::Load, {source}, queue);
    const auto size = m.subEmit(watch, Op::ContainerSize, {q}, s::u2(64));
    m.subEmit(watch, Op::Ret, {size});
    s::NbaLayout layout;
    layout.values = {queue};
    layout.events = {s::NbaWatchEvent{s::SubroutineId{watch}, s::Edge::Any, m.freshGuard()}};
    layout.targets = {s::NbaSlotTarget{{s::SlotId{out.id}}, captured(0)}};
    m.emit(0, 0, Op::NbaEvent, {m.layout(std::move(layout)), m.scalar(s::u2(32), 1), snapshot});
    m.emit(0, 0, Op::Ret);
    m.wait(1, 0, 1, 1);
    const auto ref = m.emit(1, 1, Op::RefMake, {source}, s::refType(queue.detail));
    m.emit(1, 1, Op::QueuePushBack, {Operand::typeRow(queue.detail), ref, initial});
    m.wait(1, 1, 1, 2);
    m.emit(1, 2, Op::Ret);
    ASSERT_TRUE(m.valid());
    std::uint64_t allocations = 0, failed = 0;
    for (std::uint64_t failAt = 0; failAt <= allocations; ++failAt) {
        Memory memory;
        memory.failAt = failAt;
        Capture capture;
        ASSERT_NE(capture.file, nullptr);
        {
            k::Context c(k::OutputDestinations{capture.file, capture.file}, nullptr, &memory);
            const auto result = kirimasim::vm::run(m.d, c);
            if (failAt == 0) {
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                allocations = memory.calls;
                ASSERT_GT(allocations, 0U);
                ASSERT_LT(allocations, 2000U);
                const k::ContainerToken value{words(c, out)[0]};
                const auto type = c.heap().actualType(value);
                ASSERT_TRUE(type);
                ASSERT_TRUE(c.heap().size(type.value, value));
                EXPECT_EQ(c.heap().size(type.value, value).value, 2U);
            } else {
                EXPECT_GE(memory.calls, failAt);
                EXPECT_EQ(result.reason, k::EndReason::ResourceLimit)
                    << failAt << ':' << result.error << ':' << capture.text();
                ++failed;
            }
            EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
            EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
        }
        EXPECT_EQ(memory.bytes, 0U) << failAt;
    }
    EXPECT_EQ(failed, allocations);
    RecordProperty("context_allocation_failure_points", allocations);
}
TEST(VmNbaEvent, MalformedLayoutsStopBeforeReadingCaptureStorage) {
    for (unsigned shape = 0; shape < 4; ++shape) {
        Model m;
        std::vector<Operand> args;
        if (shape != 0) args = {Operand::nbaLayout(s::NbaLayoutId{0}), m.scalar(s::u2(32), 1)};
        if (shape == 2) args[0] = m.scalar(s::u2(32), 0);
        if (shape == 3) m.d.nbaLayouts.push_back({});
        m.emit(0, 0, Op::NbaEvent, std::move(args));
        m.emit(0, 0, Op::Ret);
        m.emit(1, 0, Op::Ret);
        ASSERT_FALSE(m.valid());
        k::Context c;
        const auto result = kirimasim::vm::run(m.d, c);
        EXPECT_EQ(result.reason, k::EndReason::Internal);
        EXPECT_NE(result.error.find("invalid event-NBA SIR"), std::string::npos);
        EXPECT_EQ(c.scheduler().nbaEventStats().live, 0U);
        EXPECT_EQ(c.scheduler().nbaStorageStats().live, 0U);
    }
}
}  // namespace
