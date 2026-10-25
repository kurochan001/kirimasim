// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original end-to-end typed network checks, approved MOS/switch contract
// 1/6/7, SIR_SPEC 3/5/7/10 and IEEE 1800 4.9.5, 28.7/28.16.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/bits.h"
#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/strength.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/valsem/mos.h"
#include "kirimasim/valsem/strength.h"
#include "kirimasim/vm/vm.h"

namespace {
namespace s = kirimasim::sir;
namespace k = kirimasim::kernel;
namespace v = kirimasim::valsem;
using s::Op;
using O = s::Operand;
using R = s::StrengthRange;
constexpr s::SourceLoc loc{0, 12, 5};
constexpr std::array<const char*, 6> kinds{"nmos", "pmos", "rnmos", "rpmos", "cmos", "rcmos"};
constexpr std::array<k::W4, 4> bits{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
struct Model {
    s::Design d;
    Model() {
        d.sources = {"strength_network.sv"};
        d.instances = {{"top", "top", s::kNone}};
    }
    unsigned process(s::ProcessKind kind = s::ProcessKind::Cont) {
        const auto id = static_cast<unsigned>(d.processes.size());
        d.processes.emplace_back();
        auto& p = d.processes.back();
        p.name = "top.p" + std::to_string(id);
        p.kind = kind;
        p.loc = loc;
        p.blocks.resize(1);
        return id;
    }
    s::SlotId net(unsigned width, std::vector<s::ArrayDim> dims = {}, R initial = {}) {
        const auto id = s::SlotId{static_cast<unsigned>(d.slots.size())};
        s::Slot slot;
        slot.name = "top.net" + std::to_string(id.index());
        slot.type = s::u4(width);
        slot.kind = s::SlotKind::Wire;
        slot.flags = s::kNeedsStrength;
        slot.dims = std::move(dims);
        d.slots.push_back(std::move(slot));
        // Component zero groups the independent bank for atomic publication.
        for (unsigned e = 0; e < s::elementCount(d.slot(id)); ++e)
            for (unsigned bit = 0; bit < width; ++bit)
                d.strengthNodes.push_back({{id, e, 1, bit, 1}, s::SlotKind::Wire, 0, initial, loc});
        return id;
    }
    s::SlotId variable(unsigned width) {
        const auto id = s::SlotId{static_cast<unsigned>(d.slots.size())};
        d.slots.push_back({"top.var" + std::to_string(id.index()), s::u4(width)});
        return id;
    }
    s::StrengthOriginId origin(unsigned node, unsigned owner, R initial = {}) {
        const auto id = s::StrengthOriginId{static_cast<unsigned>(d.strengthOrigins.size())};
        d.strengthOrigins.push_back(
            {node, s::ProcessId{owner}, initial, s::StrengthOriginKind::Process, loc});
        d.processes[owner].strengthWrites.push_back(id);
        return id;
    }
    O constant(s::Type type, std::vector<std::uint64_t> a, std::vector<std::uint64_t> b = {}) {
        const auto id = s::ConstId{static_cast<unsigned>(d.constants.size())};
        d.constants.push_back({type, std::move(a), std::move(b)});
        return O::constant(id);
    }
    O integer(std::uint64_t value, unsigned width = 64) { return constant(s::u2(width), {value}); }
    O control(unsigned code) { return constant(s::u4(1), {bits[code].a}, {bits[code].b}); }
    O emit(unsigned p, Op op, s::Type type = s::voidType(), std::vector<O> args = {},
           unsigned block = 0) {
        auto& body = d.processes[p];
        if (body.blocks.size() <= block) body.blocks.resize(block + 1);
        const auto result = type == s::voidType() ? s::kNone : body.numTemps++;
        body.blocks[block].instrs.push_back({op, type, s::TempId{result}, std::move(args), loc});
        return O::temp(s::TempId{result});
    }
    O make(unsigned p, unsigned logic, unsigned one = 6, unsigned zero = 6) {
        return emit(p, Op::StrengthMake, s::strengthType(1),
                    {control(logic), integer(one), integer(zero)});
    }
    void finish(unsigned p) { emit(p, Op::Ret); }
    ::testing::AssertionResult valid() const {
        std::string message;
        for (const auto& e : s::verify(d))
            message += e.where + ": " + e.message + "\n";
        return message.empty() ? ::testing::AssertionSuccess()
                               : ::testing::AssertionFailure() << message;
    }
};
void rejects(const Model& model, const std::string& substring) {
    for (const auto& error : s::verify(model.d))
        if (error.message.find(substring) != std::string::npos) return;
    ADD_FAILURE() << "missing rejection: " << substring;
}
R read(const k::Context& ctx, s::SlotId slot, unsigned element = 0, unsigned bit = 0) {
    k::StrengthRange value;
    EXPECT_EQ(ctx.strengths().read({k::SlotId{slot.index()}, element, bit, 1}, {&value, 1}),
              k::StrengthError::None);
    return {value.lo, value.hi};
}
std::uint64_t count(const k::Context& ctx, const char* name) {
    for (std::uint32_t i = 0; i < ctx.counters().size(); ++i)
        if (ctx.counters().name(k::CounterId{i}) == name)
            return ctx.counters().value(k::CounterId{i});
    return 0;
}
Model simple() {
    Model m;
    const auto p = m.process();
    m.net(1);
    const auto origin = m.origin(0, p);
    const auto value = m.make(p, 1);
    m.emit(p, Op::StrengthDrive, s::voidType(), {O::strengthOrigin(origin), value});
    m.finish(p);
    return m;
}
TEST(StrengthNetworkSir, TablesSensitivityAndThreeInstructionsPrintDeterministicallyInFormatZero) {
    auto m = simple();
    const auto input = m.net(1, {}, {3, 6});
    const auto keeper = m.process();
    m.origin(1, keeper, {3, 6});
    m.finish(keeper);
    m.d.processes[0].strengthTriggers = {{input}};
    auto& code = m.d.processes[0].blocks[0].instrs;
    code.insert(code.begin(),
                {Op::StrengthLoad, s::strengthType(1), s::TempId{1}, {O::slot(input)}, loc});
    m.d.processes[0].numTemps = 2;
    code[2].op = Op::StrengthDriveDelay;
    code[2].operands.insert(code[2].operands.end(), {m.integer(3), m.integer(5), m.integer(7)});
    ASSERT_TRUE(m.valid());
    const auto text = s::toText(m.d);
    EXPECT_EQ(text, s::toText(m.d));
    EXPECT_TRUE(text.starts_with("; sir format 0\n"));
    for (const auto* part :
         {"strength-node !n0", "strength-origin !d0", "strength-trigger=", "strength-writes=!d0",
          "strength.load strength<1>", "strength.drive.delay !d0"})
        EXPECT_NE(text.find(part), std::string::npos) << part;
}
TEST(StrengthNetworkVm, EveryIntervalAndDeviceControlSurvivesLoadMosDriveAndOpposingResolution) {
    for (int lo = -7; lo <= 7; ++lo)
        for (int hi = lo; hi <= 7; ++hi)
            for (unsigned kind = 0; kind < 6; ++kind)
                for (unsigned a = 0; a < 4; ++a)
                    for (unsigned b = 0; b < (kind < 4 ? 1U : 4U); ++b) {
                        Model m;
                        const auto keeper = m.process(), device = m.process();
                        const R inputRange{static_cast<std::int8_t>(lo),
                                           static_cast<std::int8_t>(hi)};
                        const auto input = m.net(1, {}, inputRange), output = m.net(1);
                        m.origin(0, keeper, inputRange);
                        m.finish(keeper);
                        const auto out = m.origin(1, device);
                        // A distinct physical pull-like root opposes the device.
                        m.origin(1, keeper, {-4, -4});
                        m.d.processes[device].strengthTriggers = {{input}};
                        const auto loaded =
                            m.emit(device, Op::StrengthLoad, s::strengthType(1), {O::slot(input)});
                        std::vector<O> operands{O::str(kinds[kind]), loaded, m.control(a)};
                        if (kind >= 4) operands.push_back(m.control(b));
                        const auto transferred = m.emit(device, Op::StrengthMos, s::strengthType(1),
                                                        std::move(operands));
                        m.emit(device, Op::StrengthDrive, s::voidType(),
                               {O::strengthOrigin(out), transferred});
                        m.finish(device);
                        ASSERT_TRUE(m.valid());
                        k::Context ctx;
                        const auto result = kirimasim::vm::run(m.d, ctx);
                        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                        const auto pure =
                            v::mosStrength(static_cast<v::MosKind>(kind), {lo, hi},
                                           static_cast<v::Logic>(a), static_cast<v::Logic>(b));
                        if (!pure.has_value()) {
                            ADD_FAILURE() << "valid device rejected";
                            continue;
                        }
                        const auto expected =
                            v::resolveSignals(v::NetKind::Wire, {*pure, {-4, -4}});
                        EXPECT_EQ(read(ctx, output), (R{static_cast<std::int8_t>(expected.lo),
                                                        static_cast<std::int8_t>(expected.hi)}));
                        EXPECT_EQ(count(ctx, "proc.created"), 2U);
                        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
                    }
}
TEST(StrengthNetworkVm,
     InitialPublishedMetadataIsVisibleToStaticInitBeforeInitialActiveResolution) {
    Model m;
    const auto keeper = m.process(), init = m.process(s::ProcessKind::StaticInit),
               sample = m.process(s::ProcessKind::Initial);
    const auto input = m.net(1, {}, {-6, 6});
    m.origin(0, keeper, {});
    m.finish(keeper);
    for (const auto p : {init, sample}) {
        const auto target = m.variable(1);
        m.d.processes[p].writes = {{target}};
        const auto value = m.emit(p, Op::StrengthLoad, s::strengthType(1), {O::slot(input)});
        const auto projected = m.emit(p, Op::StrengthValue, s::u4(1), {value});
        m.emit(p, Op::Store, s::voidType(), {O::slot(target), projected});
        m.finish(p);
    }
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    EXPECT_EQ(ctx.store().element(k::SlotId{1}, 0)[0], 1U);
    EXPECT_EQ(ctx.store().element(k::SlotId{1}, 0)[1], 1U);
    EXPECT_EQ(ctx.store().element(k::SlotId{2}, 0)[0], 0U);
    EXPECT_EQ(ctx.store().element(k::SlotId{2}, 0)[1], 1U);
    EXPECT_EQ(read(ctx, input), (R{}));
}
TEST(StrengthNetworkVm, StrengthOnlyChangesReevaluateTheDeviceThroughSharedKernelWaits) {
    for (const bool delayed : {false, true}) {
        Model m;
        const auto driver = m.process(), device = m.process(),
                   stimulus = m.process(s::ProcessKind::General);
        const auto input = m.net(1), output = m.net(1);
        const auto source = m.variable(1);
        const auto first = m.origin(0, driver), second = m.origin(1, device);
        // Driver emits varying one strengths using values selected by an ordinary
        // data bit. The net remains 1 while its complete range changes.
        m.d.processes[driver].triggers = {{{source}, s::Edge::Any}};
        const auto selector = m.emit(driver, Op::Load, s::u4(1), {O::slot(source)});
        m.emit(driver, Op::BrIf, s::voidType(),
               {selector, O::block(s::BlockId{1}), O::block(s::BlockId{2})});
        auto one = m.make(driver, 1, 6, 6);
        auto instructions = std::move(m.d.processes[driver].blocks[0].instrs);
        // Move the independent make into its selected branch, retaining unique SSA.
        const auto make = std::move(instructions.back());
        instructions.pop_back();
        m.d.processes[driver].blocks[0].instrs = std::move(instructions);
        m.d.processes[driver].blocks.resize(3);
        m.d.processes[driver].blocks[1].instrs.push_back(make);
        m.emit(driver, Op::StrengthDrive, s::voidType(), {O::strengthOrigin(first), one}, 1);
        m.emit(driver, Op::Ret, s::voidType(), {}, 1);
        const auto weakValue = m.emit(driver, Op::StrengthMake, s::strengthType(1),
                                      {m.control(1), m.integer(3), m.integer(3)}, 2);
        m.emit(driver, Op::StrengthDrive, s::voidType(), {O::strengthOrigin(first), weakValue}, 2);
        m.emit(driver, Op::Ret, s::voidType(), {}, 2);
        m.d.processes[device].strengthTriggers = {{input}};
        const auto loaded = m.emit(device, Op::StrengthLoad, s::strengthType(1), {O::slot(input)});
        const auto passed = m.emit(device, Op::StrengthMos, s::strengthType(1),
                                   {O::str("nmos"), loaded, m.control(1)});
        std::vector<O> propagation{O::strengthOrigin(second), passed};
        if (delayed)
            propagation.insert(propagation.end(), {m.integer(5), m.integer(5), m.integer(5)});
        m.emit(device, delayed ? Op::StrengthDriveDelay : Op::StrengthDrive, s::voidType(),
               std::move(propagation));
        m.finish(device);
        m.d.processes[stimulus].writes = {{source}};
        m.emit(stimulus, Op::Store, s::voidType(), {O::slot(source), m.control(0)});
        m.emit(stimulus, Op::WaitDelay, s::voidType(), {m.integer(1), O::block(s::BlockId{1})});
        m.emit(stimulus, Op::Store, s::voidType(), {O::slot(source), m.control(1)}, 1);
        m.emit(stimulus, Op::Ret, s::voidType(), {}, 1);
        ASSERT_TRUE(m.valid());
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
        EXPECT_EQ(read(ctx, input), (R{6, 6}));
        EXPECT_EQ(read(ctx, output), (R{6, 6}));
        EXPECT_EQ(ctx.scheduler().time(), delayed ? 6U : 1U);
        EXPECT_EQ(count(ctx, "proc.created"), 3U);
        EXPECT_EQ(ctx.strengths().liveWatches(), 0U);
    }
}
TEST(StrengthNetworkSir, MalformedStaticRowsAndForeignOwnersAreRejectedBeforeVmConstruction) {
    for (unsigned mode = 0; mode < 27; ++mode) {
        auto m = simple();
        auto& n = m.d.strengthNodes[0];
        auto& o = m.d.strengthOrigins[0];
        auto& p = m.d.processes[0];
        switch (mode) {
            case 0: n.target.slot = s::SlotId{999}; break;
            case 1: n.target.width = 0; break;
            case 2: n.target.numElems = 0; break;
            case 3: n.target.firstElem = 1; break;
            case 4: n.target.lsb = 1; break;
            case 5: n.kind = s::SlotKind::Tri0; break;
            case 6: n.component = 1; break;
            case 7: n.initial = {-8, 6}; break;
            case 8: n.loc = {}; break;
            case 9: m.d.strengthNodes.push_back(n); break;
            case 10: m.d.strengthNodes.clear(); break;
            case 11: m.d.slots[0].flags = 0; break;
            case 12: m.d.slots[0].kind = s::SlotKind::Var; break;
            case 13: m.d.slots[0].type = s::u2(1); break;
            case 14: m.d.slots[0].flags |= s::kForceTarget; break;
            case 15: o.node = 1; break;
            case 16: o.owner = s::ProcessId{99}; break;
            case 17: o.initial = {6, 3}; break;
            case 18: o.kind = s::StrengthOriginKind::Pull; break;
            case 19: o.loc = {}; break;
            case 20: p.kind = s::ProcessKind::Initial; break;
            case 21: p.strengthWrites.clear(); break;
            case 22: p.strengthWrites.push_back(p.strengthWrites[0]); break;
            case 23: p.blocks[0].instrs[1].operands[0].id = 1; break;
            case 24: m.d.slots[0].kind = static_cast<s::SlotKind>(255); break;
            case 25: n.loc.column = 0; break;
            case 26: o.loc.column = 0; break;
            default: ADD_FAILURE() << "unknown malformed mode"; break;
        }
        EXPECT_FALSE(s::verify(m.d).empty()) << mode;
        k::Context ctx;
        const auto result = kirimasim::vm::run(m.d, ctx);
        EXPECT_EQ(result.reason, k::EndReason::Internal) << mode;
        EXPECT_EQ(ctx.store().size(), 0U) << mode;
        EXPECT_FALSE(ctx.strengthDrivers().configured());
    }
}

TEST(StrengthNetworkSir, ReadOnlyNetworkOwnershipAndCompleteInstructionShapesAreEnforced) {
    for (unsigned mode = 0; mode < 16; ++mode) {
        auto m = simple();
        auto& p = m.d.processes[0];
        auto& drive = p.blocks[0].instrs[1];
        std::string error;
        switch (mode) {
            case 0:
                drive.operands[0].kind = O::Kind::Const;
                error = "owned declared physical origin";
                break;
            case 1:
                drive.operands[0].lsb = 1;
                error = "owned declared physical origin";
                break;
            case 2:
                drive.operands[0].edge = s::Edge::Pos;
                error = "owned declared physical origin";
                break;
            case 3:
                drive.operands.pop_back();
                error = "expects 2 operands";
                break;
            case 4:
                drive.type = s::strengthType(1);
                drive.result = s::TempId{p.numTemps++};
                error = "must not have a result";
                break;
            case 5:
                drive.operands[1] = m.control(1);
                error = "owned declared physical origin";
                break;
            case 6:
                drive.op = Op::StrengthDriveDelay;
                error = "expects 5 operands";
                break;
            case 7:
                p.writes = {{s::SlotId{0}}};
                error = "only the component publisher";
                break;
            case 8:
                drive.op = Op::Store;
                drive.operands[0] = O::slot(s::SlotId{0});
                drive.operands[1] = m.control(1);
                error = "must be read-only";
                break;
            case 9:
                drive.op = Op::Nba;
                drive.operands[0] = O::slot(s::SlotId{0});
                drive.operands[1] = m.control(1);
                error = "must be read-only";
                break;
            case 10:
                drive.op = Op::Syscall;
                drive.operands = {O::str("$readmemh"), O::str("unused.mem"), O::slot(s::SlotId{0})};
                error = "must be read-only";
                break;
            case 11:
                drive.op = Op::Syscall;
                drive.operands = {O::str("$display"), O::strengthOrigin(s::StrengthOriginId{0})};
                error = "origin IDs are not ordinary";
                break;
            case 12:
                p.strengthTriggers = {{s::SlotId{999}}};
                error = "strength sensitivity";
                break;
            case 13:
                m.d.strengthOrigins[0].owner = s::kNone;
                m.d.strengthOrigins[0].kind = s::StrengthOriginKind::Pull;
                m.d.strengthOrigins[0].initial = {5, 5};
                error = "owned process origin";
                break;
            case 14:
                m.d.strengthNodes[0].kind = s::SlotKind::Uwire;
                m.origin(0, 0);
                error = "multiple physical origins";
                break;
            case 15:
                drive.op = Op::RefMake;
                drive.type = s::refType();
                drive.result = s::TempId{p.numTemps++};
                drive.operands = {O::slot(s::SlotId{0})};
                error = "must be read-only";
                break;
            default: ADD_FAILURE() << "unknown instruction rejection"; break;
        }
        rejects(m, error);
    }
}
TEST(StrengthNetworkVm, ScalarDriveDelayUsesNormalizedTicksAndReportsUnrepresentableTimes) {
    for (const auto logic : {0U, 1U, 2U, 3U})
        for (const auto width : {8U, 65U, 129U}) {
            auto m = simple();
            auto& make = m.d.processes[0].blocks[0].instrs[0];
            make.operands[0] = m.control(logic);
            auto& drive = m.d.processes[0].blocks[0].instrs[1];
            drive.op = Op::StrengthDriveDelay;
            const auto delay = [&](unsigned ticks) {
                std::vector<std::uint64_t> words(s::integralWords(width));
                words[0] = ticks;
                return m.constant(s::u2(width), std::move(words));
            };
            drive.operands.insert(drive.operands.end(), {delay(11), delay(7), delay(13)});
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            const std::array<R, 4> expected{R{-6, -6}, R{6, 6}, R{}, R{-6, 6}};
            EXPECT_EQ(read(ctx, s::SlotId{0}), expected[logic]);
            // Initial Z -> Z has no pending propagation.
            const std::array<unsigned, 4> due{7, 11, 0, 7};
            EXPECT_EQ(ctx.scheduler().time(), due[logic]);
        }
    auto m = simple();
    auto& drive = m.d.processes[0].blocks[0].instrs[1];
    drive.op = Op::StrengthDriveDelay;
    const auto huge = m.constant(s::u2(129), {0, 1, 0});
    drive.operands.insert(drive.operands.end(), {huge, m.integer(1), m.integer(1)});
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    EXPECT_EQ(result.reason, k::EndReason::Error);
    EXPECT_EQ(read(ctx, s::SlotId{0}), R{});
}

O signedIndex(Model& m, std::int64_t value, bool unknown = false, bool z = false) {
    std::vector<std::uint64_t> a(3, value < 0 ? UINT64_MAX : 0), b(3);
    a[0] = static_cast<std::uint64_t>(value);
    a[2] &= 1;
    if (unknown) {
        a[0] = z ? 0 : 1;
        b[0] = 1;
    }
    return m.constant(s::s4(129), std::move(a), std::move(b));
}
TEST(StrengthNetworkVm, CapturedArrayAndBitSelectorsRespectBoundsAliasesWideTypesAndPackedTails) {
    const std::array<std::vector<s::ArrayDim>, 3> layouts{
        {{{-3, -1}}, {{4, 2}}, {{-1, -2}, {4, 2}}}};
    constexpr std::array<R, 8> ranges{R{-6, -2}, R{2, 6},   R{-3, 5}, R{},
                                      R{3, 6},   R{-6, -3}, R{0, 4},  R{-4, 0}};
    for (const auto selectedWidth : {1U, 9U, 65U, 129U})
        for (const auto width : {1U, 9U, 65U, 129U})
            for (const auto& layout : layouts)
                for (const bool alias : {false, true})
                    for (unsigned indexMode = 0; indexMode < 10; ++indexMode)
                        for (unsigned positionMode = 0; positionMode < 8; ++positionMode) {
                            Model m;
                            const auto keeper = m.process(), reader = m.process();
                            const auto physical = m.net(width, layout);
                            const auto elements =
                                static_cast<unsigned>(s::elementCount(m.d.slot(physical)));
                            for (unsigned n = 0; n < m.d.strengthNodes.size(); ++n) {
                                auto& node = m.d.strengthNodes[n];
                                const auto value =
                                    ranges[(7 * node.target.firstElem + 3 * node.target.lsb) %
                                           ranges.size()];
                                node.initial = value;
                                m.origin(n, keeper, value);
                            }
                            m.finish(keeper);
                            auto input = physical;
                            if (alias) {
                                auto view = m.d.slot(physical);
                                view.name = "top.alias";
                                view.viewOf = physical;
                                view.flags = 0;
                                for (auto& dim : view.dims)
                                    std::swap(dim.left, dim.right);
                                input = s::SlotId{static_cast<unsigned>(m.d.slots.size())};
                                m.d.slots.push_back(std::move(view));
                            }
                            const auto dims = m.d.slot(input).dims;
                            const auto windowLsb = width > 9 ? width / 2 : 0;
                            const auto windowWidth = width - windowLsb;
                            auto base = O::slot(input, windowLsb, windowWidth);
                            std::vector<O> operands{base};
                            std::optional<unsigned> expectedElement;
                            if (indexMode >= 8) {
                                base.firstElem = indexMode == 8 ? 0 : elements - 1;
                                base.numElems = 1;
                                operands[0] = base;
                                expectedElement = base.firstElem;
                            } else {
                                if (indexMode == 0 || indexMode == 7) expectedElement = 0;
                                if (indexMode == 1) expectedElement = elements - 1;
                                for (unsigned dimension = 0; dimension < dims.size(); ++dimension) {
                                    auto index = indexMode == 1 ? dims[dimension].right
                                                                : dims[dimension].left;
                                    if (dimension == 0 && indexMode == 2)
                                        index = std::min(dims[0].left, dims[0].right) - 1;
                                    if (dimension == 0 && indexMode == 3)
                                        index = std::max(dims[0].left, dims[0].right) + 1;
                                    O captured = signedIndex(m, index,
                                                             dimension == 0 &&
                                                                 (indexMode == 4 || indexMode == 5),
                                                             indexMode == 5);
                                    if (dimension == 0 && indexMode == 6)
                                        captured = m.constant(s::u2(129), {0, 1, 0});
                                    if (indexMode == 7) {
                                        const auto f = s::FrameVarId{static_cast<unsigned>(
                                            m.d.processes[reader].frame.size())};
                                        m.d.processes[reader].frame.push_back(
                                            {"index", s::s4(129)});
                                        m.emit(reader, Op::Fstore, s::voidType(),
                                               {O::frame(f), captured});
                                        captured =
                                            m.emit(reader, Op::Fload, s::s4(129), {O::frame(f)});
                                    }
                                    operands.push_back(captured);
                                }
                            }
                            std::optional<std::int64_t> position;
                            switch (positionMode) {
                                case 0: position = -1; break;
                                case 1: position = 0; break;
                                case 2: position = windowWidth - 1; break;
                                case 3: position = windowWidth; break;
                                case 4: position = INT64_MIN; break;
                                case 5: position = INT64_MAX; break;
                                case 6:
                                case 7: break;
                                default: ADD_FAILURE() << "unknown position mode"; break;
                            }
                            operands.push_back(signedIndex(
                                m, position.value_or(0), !position.has_value(), positionMode == 7));
                            const auto loaded =
                                m.emit(reader, Op::StrengthLoad, s::strengthType(selectedWidth),
                                       std::move(operands));
                            const auto projected =
                                m.emit(reader, Op::StrengthValue, s::u4(selectedWidth), {loaded});
                            const auto sink = m.variable(selectedWidth);
                            m.d.processes[reader].writes = {{sink}};
                            m.emit(reader, Op::Store, s::voidType(), {O::slot(sink), projected});
                            m.finish(reader);
                            ASSERT_TRUE(m.valid());
                            k::Context ctx;
                            const auto result = kirimasim::vm::run(m.d, ctx);
                            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                            const auto* actual =
                                ctx.store().element(k::SlotId{sink.index() - (alias ? 1 : 0)}, 0);
                            for (unsigned bit = 0; bit < selectedWidth; ++bit) {
                                v::Logic expected = v::Logic::X;
                                if (expectedElement && position &&
                                    *position >= -static_cast<std::int64_t>(selectedWidth) &&
                                    *position <= width) {
                                    const auto relative = *position + bit;
                                    if (relative >= 0 && relative < windowWidth) {
                                        const auto range =
                                            ranges[(7 * *expectedElement +
                                                    3 * (windowLsb +
                                                         static_cast<unsigned>(relative))) %
                                                   ranges.size()];
                                        expected = v::valueOf({range.lo, range.hi});
                                    }
                                }
                                EXPECT_EQ(k::bitAt(actual, 2, bit),
                                          expected == v::Logic::One || expected == v::Logic::X);
                                EXPECT_EQ(k::bitAt(actual + 1, 2, bit),
                                          expected == v::Logic::Z || expected == v::Logic::X);
                            }
                            if (selectedWidth % 64 != 0) {
                                const auto tail = 2 * (s::integralWords(selectedWidth) - 1);
                                EXPECT_EQ(actual[tail] >> (selectedWidth % 64), 0U);
                                EXPECT_EQ(actual[tail + 1] >> (selectedWidth % 64), 0U);
                            }
                        }
}
TEST(StrengthNetworkSir, InvalidLoadShapesSelectorsAndBorrowedMetadataAcrossWaitsAreRejected) {
    for (unsigned mode = 0; mode < 10; ++mode) {
        Model m;
        const auto p = m.process(s::ProcessKind::General), keeper = m.process();
        const auto net = m.net(9, {{2, 4}});
        const auto sink = m.variable(9);
        for (unsigned n = 0; n < m.d.strengthNodes.size(); ++n)
            m.origin(n, keeper);
        m.finish(keeper);
        auto base = O::slot(net);
        base.numElems = 1;
        auto loaded = m.emit(p, Op::StrengthLoad, s::strengthType(9), {base});
        auto& in = m.d.processes[p].blocks[0].instrs[0];
        if (mode == 0) in.operands[0].kind = O::Kind::Frame;
        if (mode == 1) in.type = s::u4(9);
        if (mode == 2) in.operands[0].firstElem = 3;
        if (mode == 3) in.operands[0].lsb = 1;
        if (mode == 4) in.operands.push_back(m.constant(s::Type{s::TypeKind::Real, 64}, {0}));
        if (mode == 5) {
            auto value = m.integer(0);
            value.width = 1;
            in.operands.push_back(value);
        }
        if (mode == 6) in.operands.push_back(O::slot(sink));
        if (mode == 7) in.operands[0].numElems = 0;
        if (mode == 8) in.operands.insert(in.operands.end(), {m.integer(0), m.integer(0)});
        if (mode == 9) {
            m.emit(p, Op::WaitDelay, s::voidType(), {m.integer(1), O::block(s::BlockId{1})});
            const auto value = m.emit(p, Op::StrengthValue, s::u4(9), {loaded}, 1);
            m.d.processes[p].writes = {{sink}};
            m.emit(p, Op::Store, s::voidType(), {O::slot(sink), value}, 1);
            m.emit(p, Op::Ret, s::voidType(), {}, 1);
        } else
            m.finish(p);
        EXPECT_FALSE(s::verify(m.d).empty()) << mode;
        if (mode == 9) rejects(m, "after a wait");
    }
}

TEST(StrengthNetworkVm, WideMetadataLoadsUseTheSameInstructionPathInsideNestedFunctions) {
    constexpr std::array<R, 4> ranges{R{-7, -3}, R{3, 7}, R{-5, 4}, R{}};
    for (const auto width : {1U, 9U, 65U, 129U})
        for (const auto depth : {1U, 3U}) {
            Model m;
            const auto keeper = m.process(), reader = m.process();
            const auto input = m.net(width), sink = m.variable(width);
            for (unsigned bit = 0; bit < width; ++bit) {
                const auto range = ranges[bit % ranges.size()];
                m.d.strengthNodes[bit].initial = range;
                m.origin(bit, keeper, range);
            }
            m.finish(keeper);
            const auto value =
                m.emit(reader, Op::Call, s::u4(width), {O::subroutine(s::SubroutineId{depth - 1})});
            m.d.processes[reader].writes = {{sink}};
            m.emit(reader, Op::Store, s::voidType(), {O::slot(sink), value});
            m.finish(reader);
            for (unsigned level = 0; level < depth; ++level) {
                s::Subroutine fn;
                fn.name = "top.f" + std::to_string(level);
                fn.returnType = s::u4(width);
                fn.loc = loc;
                fn.blocks.resize(1);
                if (level == 0) {
                    fn.numTemps = 3;
                    fn.blocks[0].instrs = {
                        {Op::StrengthLoad,
                         s::strengthType(width),
                         s::TempId{0},
                         {O::slot(input)},
                         loc},
                        {Op::StrengthMos,
                         s::strengthType(width),
                         s::TempId{1},
                         {O::str("rnmos"), O::temp(s::TempId{0}), m.control(1)},
                         loc},
                        {Op::StrengthValue,
                         s::u4(width),
                         s::TempId{2},
                         {O::temp(s::TempId{1})},
                         loc},
                        {Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{2})}, loc}};
                } else {
                    fn.numTemps = 1;
                    fn.blocks[0].instrs = {
                        {Op::Call,
                         s::u4(width),
                         s::TempId{0},
                         {O::subroutine(s::SubroutineId{level - 1})},
                         loc},
                        {Op::Ret, s::voidType(), s::kNone, {O::temp(s::TempId{0})}, loc}};
                }
                m.d.subroutines.push_back(std::move(fn));
            }
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            const auto* actual = ctx.store().element(k::SlotId{sink.index()}, 0);
            for (unsigned bit = 0; bit < width; ++bit) {
                const auto range = ranges[bit % ranges.size()];
                const auto expected =
                    v::mosStrength(v::MosKind::Rnmos, {range.lo, range.hi}, v::Logic::One);
                if (!expected.has_value()) {
                    ADD_FAILURE() << "valid reference rejected";
                    continue;
                }
                const auto logic = v::valueOf(*expected);
                EXPECT_EQ(k::bitAt(actual, 2, bit), logic == v::Logic::One || logic == v::Logic::X);
                EXPECT_EQ(k::bitAt(actual + 1, 2, bit),
                          logic == v::Logic::Z || logic == v::Logic::X);
            }
        }
}

TEST(StrengthNetworkSir, ImplicitOriginsStayFixedAndForceThroughAliasesIsRejected) {
    Model m;
    const auto net = m.net(9, {{1, 3}});
    for (unsigned n = 0; n < m.d.strengthNodes.size(); ++n) {
        const auto rank = static_cast<std::int8_t>(n % 2 == 0 ? 5 : -7);
        m.d.strengthOrigins.push_back(
            {n,
             s::kNone,
             {rank, rank},
             n % 2 == 0 ? s::StrengthOriginKind::Pull : s::StrengthOriginKind::Supply,
             loc});
    }
    auto view = m.d.slot(net);
    view.name = "top.alias";
    view.viewOf = net;
    view.flags = 0;
    std::swap(view.dims[0].left, view.dims[0].right);
    m.d.slots.push_back(std::move(view));
    ASSERT_TRUE(m.valid());
    k::Context ctx;
    const auto result = kirimasim::vm::run(m.d, ctx);
    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
    for (unsigned e = 0; e < 3; ++e)
        for (unsigned bit = 0; bit < 9; ++bit) {
            const auto rank = static_cast<std::int8_t>((9 * e + bit) % 2 == 0 ? 5 : -7);
            EXPECT_EQ(read(ctx, net, e, bit), (R{rank, rank}));
        }
    EXPECT_EQ(count(ctx, "proc.created"), 0U);
    m.d.slots[1].flags |= s::kForceTarget;
    rejects(m, "strength net holds");
    k::Context rejected;
    EXPECT_EQ(kirimasim::vm::run(m.d, rejected).reason, k::EndReason::Internal);
    EXPECT_EQ(rejected.store().size(), 0U);
}

TEST(StrengthNetworkSir, CompleteCoverageUniqueTargetsAndInstructionOwnersAreRequired) {
    for (unsigned mode = 0; mode < 4; ++mode) {
        auto m = simple();
        m.d.slots[0].type = s::u4(2);
        m.d.strengthNodes.push_back({{s::SlotId{0}, 0, 1, 1, 1}, s::SlotKind::Wire, 0, {}, loc});
        ASSERT_TRUE(m.valid());
        std::string error;
        if (mode == 0) {
            m.d.strengthNodes.pop_back();
            error = "cover the eligible physical net completely";
        }
        if (mode == 1) {
            m.d.strengthNodes[1].target = m.d.strengthNodes[0].target;
            error = "duplicate physical node target";
        }
        if (mode == 2) {
            m.d.slots[0].type = s::u4(3);
            m.d.strengthNodes.push_back(
                {{s::SlotId{0}, 0, 1, 2, 1}, s::SlotKind::Wire, 2, {}, loc});
            m.d.strengthNodes[1].component = 2;
            error = "component IDs must be dense";
        }
        if (mode == 3) {
            const auto other = m.process();
            const auto foreign = m.origin(0, other);
            m.finish(other);
            ASSERT_TRUE(m.valid());
            m.d.processes[0].blocks[0].instrs[1].operands[0] = O::strengthOrigin(foreign);
            error = "owned declared physical origin";
        }
        rejects(m, error);
        k::Context ctx;
        EXPECT_EQ(kirimasim::vm::run(m.d, ctx).reason, k::EndReason::Internal);
        EXPECT_EQ(ctx.store().size(), 0U);
    }
}
}  // namespace
