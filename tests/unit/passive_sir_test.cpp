// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original typed edge execution matrix; approved MOS/switch contract 3/6/7,
// SIR_SPEC 3/5/7/10, IEEE 1800-2023 4.9.5 and 28.8/28.13/28.14.
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/strength.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/valsem/passive.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using O = s::Operand;
using E = s::StrengthEdgeKind;
constexpr s::SourceLoc loc{0, 6, 3};
constexpr std::array<k::W4, 4> controls{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
struct Model {
    s::Design d;
    explicit Model(E kind = E::Tranif1, bool delayed = false) {
        d.sources = {"passive_network.sv"};
        d.instances = {{"top", "top", s::kNone}};
        s::Slot net{"top.nodes", s::u4(2), s::SlotKind::Wire};
        net.flags = s::kNeedsStrength;
        d.slots.push_back(net);
        d.strengthNodes = {{{s::SlotId{0}, 0, 1, 0, 1}, s::SlotKind::Wire, 0, {}, loc},
                           {{s::SlotId{0}, 0, 1, 1, 1}, s::SlotKind::Wire, 0, {}, loc}};
        addProcess(s::ProcessKind::Cont);
        d.strengthOrigins = {{0, s::ProcessId{0}, {6, 6}, s::StrengthOriginKind::Process, loc}};
        d.processes[0].strengthWrites = {s::StrengthOriginId{0}};
        const bool conditional = s::conditionalStrengthEdge(kind);
        d.strengthEdges = {
            {0, 1, kind, conditional ? s::ProcessId{0} : s::ProcessId{s::kNone}, delayed, loc}};
        if (conditional) {
            d.processes[0].strengthEdgeWrites = {s::StrengthEdgeId{0}};
            std::vector<O> args{edge(), constant(s::u4(1), 1)};
            if (delayed) {
                args.push_back(integer(4));
                args.push_back(integer(3));
            }
            emit(0, delayed ? s::Op::StrengthEdgeDelay : s::Op::StrengthEdge, args);
        }
        emit(0, s::Op::Ret);
    }
    static O edge() { return O::strengthEdge(s::StrengthEdgeId{0}); }
    O constant(s::Type type, std::uint64_t a, std::uint64_t b = 0) {
        const auto id = s::ConstId{static_cast<unsigned>(d.constants.size())};
        d.constants.push_back(
            {type, {a}, s::isFourState(type) ? std::vector{b} : std::vector<std::uint64_t>{}});
        return O::constant(id);
    }
    O integer(std::uint64_t value) { return constant(s::u2(64), value); }
    unsigned addProcess(s::ProcessKind kind) {
        const auto id = static_cast<unsigned>(d.processes.size());
        s::Process p;
        p.name = "top.p" + std::to_string(id);
        p.kind = kind;
        p.loc = loc;
        p.blocks.resize(1);
        d.processes.push_back(std::move(p));
        return id;
    }
    void emit(unsigned p, s::Op op, std::vector<O> args = {}, unsigned block = 0) {
        d.processes[p].blocks.resize(
            std::max<std::size_t>(block + 1, d.processes[p].blocks.size()));
        d.processes[p].blocks[block].instrs.push_back(
            {op, s::voidType(), s::kNone, std::move(args), loc});
    }
    ::testing::AssertionResult valid() const {
        std::string errors;
        for (const auto& error : s::verify(d))
            errors += error.where + ": " + error.message + "\n";
        return errors.empty() ? ::testing::AssertionSuccess()
                              : ::testing::AssertionFailure() << errors;
    }
};

TEST(PassiveSir, StaticTablesControlOwnershipAndTextKeepFormatZero) {
    Model m(E::Rtranif0, true);
    ASSERT_TRUE(m.valid());
    const auto text = s::toText(m.d);
    EXPECT_EQ(text, s::toText(m.d));
    EXPECT_TRUE(text.starts_with("; sir format 0\n"));
    for (const auto* part : {"strength-edge !e0 nodes=(!n0,!n1) kind=rtranif0 owner=@0 delayed",
                             "strength-edge-write=!e0", "strength.edge.delay !e0"})
        EXPECT_NE(text.find(part), std::string::npos) << part;
    Model identity(E::Identity);
    ASSERT_TRUE(identity.valid());
    EXPECT_NE(s::toText(identity.d).find("kind=identity owner=-"), std::string::npos);
}

TEST(PassiveSir, MalformedTablesAndControlInstructionsNeverReachTheKernel) {
    for (unsigned bad = 0; bad < 27; ++bad) {
        SCOPED_TRACE(bad);
        Model m;
        auto& edge = m.d.strengthEdges[0];
        auto& p = m.d.processes[0];
        auto& in = p.blocks[0].instrs[0];
        if (bad == 0) edge.first = 2;
        if (bad == 1) edge.second = UINT32_MAX;
        if (bad == 2) edge.kind = static_cast<E>(200);
        if (bad == 3) edge.loc.line = 0;
        if (bad == 4) edge.loc.file = 1;
        if (bad == 5) edge.owner = s::kNone;
        if (bad == 6) edge.owner = s::ProcessId{1};
        if (bad == 7) p.strengthEdgeWrites.clear();
        if (bad == 8) p.strengthEdgeWrites.push_back(s::StrengthEdgeId{0});
        if (bad == 9) p.strengthEdgeWrites = {s::StrengthEdgeId{1}};
        if (bad == 10) p.kind = s::ProcessKind::Initial;
        if (bad == 11) p.parent = s::Owner::of(s::ProcessId{0});
        if (bad == 12) {
            m.d.strengthNodes[1].component = 1;
        }
        if (bad == 13) in.operands.clear();
        if (bad == 14) in.operands[0].id = 1;
        if (bad == 15) in.operands[0] = O::strengthOrigin(s::StrengthOriginId{0});
        if (bad == 16) in.operands[0].lsb = 1;
        if (bad == 17) in.operands[0].text = "borrowed";
        if (bad == 18) in.operands[1] = m.integer(1);
        if (bad == 19) in.operands[1] = O::slot(s::SlotId{0}, 0, 1);
        if (bad == 20) edge.delayed = true;
        if (bad == 21) {
            in.op = s::Op::StrengthEdgeDelay;
            in.operands.push_back(m.integer(1));
        }
        if (bad == 22) {
            in.op = s::Op::Store;
            in.operands = {O::slot(s::SlotId{0}), Model::edge()};
        }
        if (bad == 23) {
            in.type = s::u2(1);
            in.result = s::TempId{0};
            p.numTemps = 1;
        }
        if (bad == 24) edge.kind = E::Tran;
        if (bad == 25) edge.kind = E::Identity;
        if (bad == 26) m.d.strengthNodes.clear();
        EXPECT_FALSE(m.valid());
        k::Context ctx;
        EXPECT_FALSE(kirimasim::vm::run(m.d, ctx).error.empty());
        EXPECT_FALSE(ctx.strengthDrivers().configured());
    }
    for (unsigned bad = 0; bad < 3; ++bad) {
        Model m(E::Tran);
        if (bad == 0) m.d.strengthEdges[0].delayed = true;
        if (bad == 1) m.d.strengthEdges[0].owner = s::ProcessId{0};
        if (bad == 2) m.d.processes[0].strengthEdgeWrites = {s::StrengthEdgeId{0}};
        EXPECT_FALSE(m.valid());
    }
}

TEST(PassiveSir, PotentialUwireOriginsAreCountedAcrossInitiallyOffDuplicatePaths) {
    Model m;
    m.d.strengthNodes[1].kind = s::SlotKind::Uwire;
    m.d.strengthEdges.push_back({0, 1, E::Identity, s::kNone, false, loc});
    ASSERT_TRUE(m.valid());
    m.d.strengthOrigins.push_back({1, s::ProcessId{0}, {}, s::StrengthOriginKind::Process, loc});
    m.d.processes[0].strengthWrites.push_back(s::StrengthOriginId{1});
    EXPECT_FALSE(m.valid());
    m.d.strengthEdges.pop_back();
    // Conditional edges count even though their initial committed state is off.
    EXPECT_FALSE(m.valid());
}

TEST(PassiveVm, AllIntervalsKindsControlsAndDirectionsMatchIndependentWorlds) {
    unsigned designs = 0;
    for (const auto kind :
         {E::Identity, E::Tran, E::Rtran, E::Tranif0, E::Tranif1, E::Rtranif0, E::Rtranif1})
        for (unsigned code = 0; code < (s::conditionalStrengthEdge(kind) ? 4U : 1U); ++code)
            for (int lo = -7; lo <= 7; ++lo)
                for (int hi = lo; hi <= 7; ++hi)
                    for (const bool reverse : {false, true}) {
                        Model m(kind);
                        const auto logic = controls[code];
                        if (s::conditionalStrengthEdge(kind))
                            m.d.processes[0].blocks[0].instrs[0].operands[1] =
                                m.constant(s::u4(1), logic.a, logic.b);
                        m.d.strengthOrigins[0].node = reverse ? 1 : 0;
                        m.d.strengthOrigins[0].initial = {static_cast<std::int8_t>(lo),
                                                          static_cast<std::int8_t>(hi)};
                        m.d.strengthOrigins.push_back({reverse ? 0U : 1U,
                                                       s::ProcessId{0},
                                                       {-3, -3},
                                                       s::StrengthOriginKind::Process,
                                                       loc});
                        m.d.processes[0].strengthWrites.push_back(s::StrengthOriginId{1});
                        ASSERT_TRUE(m.valid());
                        k::Context ctx;
                        const auto result = kirimasim::vm::run(m.d, ctx);
                        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                        const auto transfer =
                            kind == E::Identity ? v::StrengthTransfer::Identity
                            : (kind == E::Rtran || kind == E::Rtranif0 || kind == E::Rtranif1)
                                ? v::StrengthTransfer::Resistive
                                : v::StrengthTransfer::Nonresistive;
                        const auto control = code == 0   ? v::Logic::Zero
                                             : code == 1 ? v::Logic::One
                                             : code == 2 ? v::Logic::X
                                                         : v::Logic::Z;
                        const auto state =
                            s::conditionalStrengthEdge(kind)
                                ? v::passiveConduction(control,
                                                       (kind == E::Tranif1 || kind == E::Rtranif1))
                                : v::Conduction::On;
                        const auto reference = v::resolvePassive(
                            std::array{v::NetKind::Wire, v::NetKind::Wire},
                            std::array<v::PassiveRoot, 2>{
                                {{reverse ? 1U : 0U, {lo, hi}}, {reverse ? 0U : 1U, {-3, -3}}}},
                            std::array<v::PassiveEdge, 1>{{{0, 1, transfer, state}}});
                        ASSERT_TRUE(reference.has_value());
                        for (unsigned node = 0; node < 2; ++node) {
                            k::StrengthRange actual;
                            ASSERT_EQ(
                                ctx.strengths().read({k::SlotId{0}, 0, node, 1}, {&actual, 1}),
                                k::StrengthError::None);
                            EXPECT_EQ(actual.lo, (*reference)[node].lo);
                            EXPECT_EQ(actual.hi, (*reference)[node].hi);
                        }
                        ++designs;
                    }
    EXPECT_EQ(designs, 4560U);
}

TEST(PassiveVm, ScalarSignednessDomainsAndExplicitZeroDelayUseKernelControlMeaning) {
    for (const auto type : {s::u2(1), s::s2(1), s::u4(1), s::s4(1)})
        for (const auto kind : {E::Tranif0, E::Tranif1, E::Rtranif0, E::Rtranif1})
            for (unsigned code = 0; code < (s::isFourState(type) ? 4U : 2U); ++code) {
                Model m(kind, true);
                auto& args = m.d.processes[0].blocks[0].instrs[0].operands;
                args[1] = m.constant(type, controls[code].a, controls[code].b);
                args[2] = m.integer(0);
                args[3] = m.integer(0);
                ASSERT_TRUE(m.valid());
                k::Context ctx;
                const auto result = kirimasim::vm::run(m.d, ctx);
                ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                const auto state = ctx.strengthDrivers().edge(0);
                ASSERT_TRUE(state.has_value());
                const auto actual = state.value_or(k::StrengthEdgeState{});
                const auto expected =
                    code >= 2 ? v::Conduction::Uncertain
                              : v::passiveConduction(code == 0 ? v::Logic::Zero : v::Logic::One,
                                                     (kind == E::Tranif1 || kind == E::Rtranif1));
                EXPECT_EQ(static_cast<unsigned>(actual.committed), static_cast<unsigned>(expected));
                EXPECT_FALSE(actual.hasPending);
            }
}

TEST(PassiveVm, WideDelaysRejectOverflowAndNormalizeUnknownToZero) {
    for (const bool unknown : {false, true}) {
        Model m(E::Tranif1, true);
        const auto id = s::ConstId{static_cast<unsigned>(m.d.constants.size())};
        m.d.constants.push_back(
            {s::u4(129),
             {0, 1, 0},
             unknown ? std::vector<std::uint64_t>{0, 1, 0} : std::vector<std::uint64_t>{0, 0, 0}});
        m.d.processes[0].blocks[0].instrs[0].operands[2] = O::constant(id);
        ASSERT_TRUE(m.valid());
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        EXPECT_EQ(result.reason, unknown ? k::EndReason::Quiescent : k::EndReason::Error);
        const auto state = ctx.strengthDrivers().edge(0);
        ASSERT_TRUE(state.has_value());
        const auto actual = state.value_or(k::StrengthEdgeState{});
        EXPECT_EQ(actual.committed,
                  unknown ? k::StrengthConduction::On : k::StrengthConduction::Off);
        EXPECT_FALSE(actual.hasPending);
    }
}

TEST(PassiveVm, CapturedConductionTimersKeepDeadlinesAndCarryCurrentData) {
    Model m(E::Tranif1, true);
    const auto variable = [&](const std::string& name) {
        const auto id = s::SlotId{static_cast<unsigned>(m.d.slots.size())};
        m.d.slots.push_back({name, s::u4(1)});
        return id;
    };
    const auto control = variable("top.control"), data = variable("top.data"),
               marker = variable("top.marker");
    std::array<s::SlotId, 7> samples;
    for (unsigned i = 0; i < samples.size(); ++i)
        samples[i] = variable("top.sample" + std::to_string(i));
    const auto root = m.addProcess(s::ProcessKind::Cont);
    const auto issuer = m.addProcess(s::ProcessKind::Initial);
    const auto observer = m.addProcess(s::ProcessKind::Initial);
    const auto value = [&](unsigned p, unsigned block, s::Op op, s::Type type,
                           std::vector<O> args) {
        auto& process = m.d.processes[p];
        process.blocks.resize(std::max<std::size_t>(block + 1, process.blocks.size()));
        const auto temp = s::TempId{process.numTemps++};
        process.blocks[block].instrs.push_back({op, type, temp, std::move(args), loc});
        return O::temp(temp);
    };
    m.d.processes[0].blocks[0].instrs.clear();
    m.d.processes[0].strengthWrites.clear();
    m.d.processes[0].triggers = {{{control}}, {{marker}}};
    const auto captured = value(0, 0, s::Op::Load, s::u4(1), {O::slot(control)});
    value(0, 0, s::Op::Load, s::u4(1), {O::slot(marker)});
    m.emit(0, s::Op::StrengthEdgeDelay, {Model::edge(), captured, m.integer(8), m.integer(6)});
    m.emit(0, s::Op::Ret);
    m.d.strengthOrigins[0].owner = s::ProcessId{root};
    m.d.processes[root].strengthWrites = {s::StrengthOriginId{0}};
    m.d.processes[root].triggers = {{{data}}};
    const auto driven = value(root, 0, s::Op::Load, s::u4(1), {O::slot(data)});
    const auto range = value(root, 0, s::Op::StrengthMake, s::strengthType(1),
                             {driven, m.integer(6), m.integer(6)});
    m.emit(root, s::Op::StrengthDrive, {O::strengthOrigin(s::StrengthOriginId{0}), range});
    m.emit(root, s::Op::Ret);
    m.d.processes[issuer].writes = {{control}, {data}, {marker}};
    struct Update {
        unsigned time;
        s::SlotId slot;
        unsigned a, b;
    };
    const std::array<Update, 15> updates{{{0, control, 0, 0},
                                          {0, data, 1, 0},
                                          {0, marker, 0, 0},
                                          {1, control, 1, 0},
                                          {2, marker, 1, 0},
                                          {3, control, 0, 0},
                                          {4, control, 1, 0},
                                          {6, marker, 0, 0},
                                          {10, control, 0, 0},
                                          {11, data, 0, 0},
                                          {15, control, 1, 1},
                                          {20, control, 1, 0},
                                          {21, data, 1, 0},
                                          {22, marker, 1, 0},
                                          {23, marker, 0, 0}}};
    unsigned block = 0, time = 0;
    for (const auto& update : updates) {
        if (update.time != time) {
            m.emit(issuer, s::Op::WaitDelay,
                   {m.integer(std::uint64_t{2} * (update.time - time)),
                    O::block(s::BlockId{block + 1})},
                   block);
            ++block;
            time = update.time;
        }
        m.emit(issuer, s::Op::Store,
               {O::slot(update.slot), m.constant(s::u4(1), update.a, update.b)}, block);
    }
    m.emit(issuer, s::Op::Ret, {}, block);
    time = 0;
    // Sample strictly between the two possible swapped turn-off deadlines.
    // Every observation is outside the correct conduction commit time.
    const std::array<unsigned, 7> times{12, 18, 24, 27, 38, 44, 50};
    for (unsigned i = 0; i < times.size(); ++i) {
        m.emit(observer, s::Op::WaitDelay,
               {m.integer(times[i] - time), O::block(s::BlockId{i + 1})}, i);
        const auto current =
            value(observer, i + 1, s::Op::Load, s::u4(1), {O::slot(s::SlotId{0}, 1, 1)});
        m.emit(observer, s::Op::Store, {O::slot(samples[i]), current}, i + 1);
        m.d.processes[observer].writes.push_back({samples[i]});
        time = times[i];
    }
    m.emit(observer, s::Op::Ret, {}, static_cast<unsigned>(times.size()));
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    const std::array<k::W4, 7> expected{{{0, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 1}, {1, 0}}};
    for (unsigned i = 0; i < samples.size(); ++i) {
        SCOPED_TRACE(times[i]);
        const auto* words = ctx.store().element(k::SlotId{samples[i].index()}, 0);
        EXPECT_EQ(words[0], expected[i].a);
        EXPECT_EQ(words[1], expected[i].b);
    }
    const auto edge = ctx.strengthDrivers().edge(0);
    ASSERT_TRUE(edge.has_value());
    const auto actual = edge.value_or(k::StrengthEdgeState{});
    EXPECT_EQ(actual.committed, k::StrengthConduction::On);
    EXPECT_FALSE(actual.hasPending);
}
}  // namespace
