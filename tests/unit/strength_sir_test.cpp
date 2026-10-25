// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Original strength SSA checks: approved MOS/switch contract 7, SIR_SPEC
// 4.1/5.1/10 and IEEE 1800-2023 28.7/28.9/28.13/28.14.
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
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
using s::Operand;
constexpr std::array<const char*, 6> names{"nmos", "pmos", "rnmos", "rpmos", "cmos", "rcmos"};
constexpr std::array<k::W4, 4> bits{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
Operand temp(unsigned id) { return Operand::temp(s::TempId{id}); }
Operand bb(unsigned id, unsigned count = 0) { return Operand::block(s::BlockId{id}, count); }
struct Model {
    s::Design d;
    unsigned width;
    explicit Model(unsigned w = 17) : width(w) {
        d.sources = {"strength_value.sv"};
        d.instances = {{"top", "top", s::kNone}};
        d.slots = {{"top.result", s::u4(w)}};
        d.processes.resize(1);
        d.processes[0].name = "top.initial";
        d.processes[0].writes = {{s::SlotId{0}}};
    }
    Operand constant(s::Type type, std::vector<std::uint64_t> a,
                     std::vector<std::uint64_t> b = {}) {
        const auto id = s::ConstId{static_cast<std::uint32_t>(d.constants.size())};
        d.constants.push_back({type, std::move(a), std::move(b)});
        return Operand::constant(id);
    }
    Operand rank(unsigned r) { return constant(s::u2(3), {r}); }
    Operand control(unsigned logic, s::Type type = s::u4(1)) {
        return constant(type, {bits[logic].a},
                        s::isFourState(type) ? std::vector<std::uint64_t>{bits[logic].b}
                                             : std::vector<std::uint64_t>{});
    }
    Operand data(unsigned seed = 0) {
        std::vector<std::uint64_t> a((width + 63) / 64), b(a.size());
        for (unsigned bit = 0; bit < width; ++bit) {
            const auto value = bits[(bit + seed) % 4];
            a[bit / 64] |= value.a << (bit % 64);
            b[bit / 64] |= value.b << (bit % 64);
        }
        return constant(s::u4(width), std::move(a), std::move(b));
    }
    Operand side() {
        std::vector<std::uint64_t> a((2 * width + 63) / 64);
        for (unsigned bit = 0; bit < width; ++bit) {
            if (bit % 3 != 0) a[bit / 64] |= std::uint64_t{1} << (bit % 64);
            if (bit % 5 != 0) a[(width + bit) / 64] |= std::uint64_t{1} << ((width + bit) % 64);
        }
        return constant(s::u2(2 * width), std::move(a));
    }
    Operand emit(unsigned block, Op op, s::Type type, std::vector<Operand> args = {}) {
        auto& p = d.processes[0];
        if (p.blocks.size() <= block) p.blocks.resize(block + 1);
        const auto result = type == s::voidType() ? s::kNone : p.numTemps++;
        p.blocks[block].instrs.push_back({op, type, s::TempId{result}, std::move(args), {0, 4, 7}});
        return temp(result);
    }
    Operand make(bool masked = false) {
        std::vector<Operand> args{data(), rank(7), rank(3)};
        if (masked) args.push_back(side());
        return emit(0, Op::StrengthMake, s::strengthType(width), std::move(args));
    }
    void finish(const Operand& value, unsigned block = 0) {
        const auto projected = emit(block, Op::StrengthValue, s::u4(width), {value});
        emit(block, Op::Store, s::voidType(), {Operand::slot(s::SlotId{0}), projected});
        emit(block, Op::Ret, s::voidType());
    }
    ::testing::AssertionResult valid() const {
        std::string message;
        for (const auto& e : s::verify(d))
            message += e.where + ": " + e.message + "\n";
        return message.empty() ? ::testing::AssertionSuccess()
                               : ::testing::AssertionFailure() << message;
    }
};
void rejects(const Model& m, const std::string& message) {
    const auto errors = s::verify(m.d);
    for (const auto& e : errors)
        if (e.message.find(message) != std::string::npos) return;
    ADD_FAILURE() << "missing rejection: " << message;
}
TEST(StrengthSir, TypesAndThreeOpsPrintDeterministicallyWithoutChangingVersion) {
    Model m;
    const auto made = m.make(true);
    const auto mos = m.emit(0, Op::StrengthMos, s::strengthType(m.width),
                            {Operand::str("rcmos"), made, m.control(3), m.control(0)});
    m.finish(mos);
    ASSERT_TRUE(m.valid());
    const auto text = s::toText(m.d);
    EXPECT_EQ(text, s::toText(m.d));
    EXPECT_NE(text.find("strength.make strength<17>"), std::string::npos);
    EXPECT_NE(text.find("strength.mos strength<17> \"rcmos\""), std::string::npos);
    EXPECT_NE(text.find("strength.value i4<17>"), std::string::npos);
    EXPECT_TRUE(text.starts_with("; sir format 0\n"));
    EXPECT_EQ(s::toString(s::strengthType(129)), "strength<129>");
    EXPECT_FALSE(s::isIntegral(s::strengthType(129)));
    EXPECT_FALSE(s::isSigned(s::strengthType(129)));
    EXPECT_FALSE(s::isFourState(s::strengthType(129)));
    EXPECT_EQ(s::otherDomain(s::strengthType(129)), s::strengthType(129));
}

TEST(StrengthSir, StorageConstantsDescriptorsAndCallInterfacesRejectMetadata) {
    for (unsigned home = 0; home < 9; ++home) {
        SCOPED_TRACE(home);
        Model m;
        m.finish(m.make());
        const auto type = s::strengthType(17);
        if (home == 0) m.d.slots[0].type = type;
        if (home == 1) m.constant(type, {0});
        if (home == 2) m.d.processes[0].frame = {{"metadata", type}};
        if (home == 3) m.d.processes[0].frame = {{"reference", s::refType(), s::kNone, {}, type}};
        if (home >= 4 && home <= 7) {
            s::Subroutine fn;
            fn.name = "top.function";
            fn.automatic = true;
            fn.loc = {0, 1, 1};
            fn.returnType = home == 4 ? type : s::voidType();
            fn.blocks = {{{}, {{Op::Ret, s::voidType(), s::kNone, {}, {0, 1, 1}}}}};
            if (home >= 5) {
                fn.frame = {{"arg", type}};
                fn.params = {{"arg", home == 6 ? s::Direction::Ref : s::Direction::Input, type,
                              s::kNone, s::FrameVarId{0}}};
                if (home == 7) fn.frame[0].target = type;
            }
            m.d.subroutines.push_back(std::move(fn));
        }
        if (home == 8) {
            s::TypeEntry t;
            t.kind = s::TypeEntryKind::Queue;
            t.value = type;
            m.d.types.push_back(t);
        }
        rejects(m, "strength is SSA-only metadata");
    }
}

TEST(StrengthSir, EveryOrdinaryConsumerAndUndeclaredProducerRejectMetadata) {
    for (const auto op : {Op::Add, Op::Eq, Op::Truth, Op::Select, Op::To2, Op::Zext, Op::Store,
                          Op::Syscall, Op::Builtin, Op::WaitNamed, Op::Ret}) {
        SCOPED_TRACE(s::opName(op));
        Model m;
        const auto made = m.make();
        std::vector<Operand> args{made};
        if (op == Op::Syscall || op == Op::Builtin)
            args.insert(args.begin(), Operand::str("$display"));
        m.emit(0, op, s::voidType(), args);
        m.emit(0, Op::Ret, s::voidType());
        rejects(m, "strength metadata is not an ordinary operand");
    }
    for (const auto op : {Op::Load, Op::Call, Op::Syscall, Op::Select, Op::Add}) {
        Model m;
        m.emit(0, op, s::strengthType(17), {});
        m.emit(0, Op::Ret, s::voidType());
        rejects(m, "only strength.make/mos/load produce strength metadata");
    }
}

TEST(StrengthSir, MakeRanksMasksAndMosKindsControlsAreVerified) {
    for (unsigned bad = 0; bad < 19; ++bad) {
        SCOPED_TRACE(bad);
        Model m;
        const auto made = m.make(true);
        auto& in = m.d.processes[0].blocks[0].instrs[0];
        if (bad == 0) in.type.width = 0;
        if (bad == 1) in.type.detail = s::TypeId{0};
        if (bad == 2) in.type = s::u4(17);
        if (bad == 3) in.operands.clear();
        if (bad == 4) in.operands.push_back(m.rank(1));
        if (bad == 5) in.operands[0] = m.rank(6);
        if (bad == 6) in.operands[1] = m.constant(s::u2(4), {8});
        if (bad == 7) in.operands[1] = m.constant(s::s2(3), {7});
        if (bad == 8) in.operands[1] = m.constant(s::u4(3), {6}, {1});
        if (bad == 9) in.operands[1] = m.constant(s::u2(129), {6, 1, 0});
        if (bad == 10) in.operands[1] = m.constant(s::u4(129), {6, 0, 0}, {0, 1, 0});
        if (bad == 11) in.operands[3] = m.constant(s::u4(34), {0}, {0});
        if (bad == 12) in.operands[3] = m.constant(s::u2(33), {0});
        if (bad == 13) in.operands[0] = Operand::slot(s::SlotId{0});
        if (bad >= 14) {
            const auto mos = m.emit(
                0, Op::StrengthMos, s::strengthType(17),
                {Operand::str(bad == 14 ? "tran" : "cmos"), made, m.control(0), m.control(1)});
            auto& gate = m.d.processes[0].blocks[0].instrs[1];
            if (bad == 15) gate.operands.pop_back();
            if (bad == 16) gate.operands[2] = m.rank(1);
            if (bad == 17) gate.operands[1] = m.data();
            if (bad == 18) gate.operands[1].lsb = 1;
            m.finish(mos);
        } else
            m.finish(made);
        EXPECT_FALSE(m.valid());
    }
    Model wideRank;
    const auto made = wideRank.make();
    wideRank.d.processes[0].blocks[0].instrs[0].operands[1] =
        wideRank.constant(s::u4(129), {7, 0, 0}, {0, 0, 0});
    wideRank.finish(made);
    EXPECT_TRUE(wideRank.valid());
}

TEST(StrengthSir, ProjectionWidthWholeOperandsAndNoSsaAcrossWaitAreVerified) {
    for (unsigned bad = 0; bad < 4; ++bad) {
        Model m;
        const auto made = m.make();
        if (bad == 3) m.emit(0, Op::WaitDelay, s::voidType(), {m.constant(s::u2(64), {1}), bb(1)});
        m.finish(made, bad == 3 ? 1 : 0);
        auto& in = m.d.processes[0].blocks[bad == 3 ? 1 : 0].instrs[bad == 3 ? 0 : 1];
        if (bad == 0) in.type = s::s4(17);
        if (bad == 1) in.type = s::u4(16);
        if (bad == 2) in.operands[0].width = 16;
        if (bad == 3)
            rejects(m, "is used after a wait");
        else
            EXPECT_FALSE(m.valid());
    }
}

TEST(StrengthVm, MakeMosAndProjectionUseTheIndependentReferenceAtEveryBit) {
    unsigned designs = 0;
    for (const unsigned width : {1U, 7U, 8U, 9U, 32U, 33U, 63U, 64U, 65U, 129U})
        for (unsigned kind = 0; kind < 6; ++kind)
            for (unsigned first = 0; first < 4; ++first)
                for (unsigned second = 0; second < 4; ++second)
                    for (const bool masked : {false, true}) {
                        Model m(width);
                        const auto made = m.make(masked);
                        const auto output = m.emit(
                            0, Op::StrengthMos, s::strengthType(width),
                            kind >= 4 ? std::vector<Operand>{Operand::str(names[kind]), made,
                                                             m.control(first), m.control(second)}
                                      : std::vector<Operand>{Operand::str(names[kind]), made,
                                                             m.control(first)});
                        m.finish(output);
                        ASSERT_TRUE(m.valid());
                        k::Context ctx;
                        const auto result = kirimasim::vm::run(m.d, ctx);
                        ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                        const auto* words = ctx.store().element(k::SlotId{0}, 0);
                        for (unsigned at = 0; at < width; ++at) {
                            const auto input = v::driverSignal({static_cast<v::Logic>(at % 4),
                                                                masked && at % 3 == 0 ? 0 : 7,
                                                                masked && at % 5 == 0 ? 0 : 3});
                            const auto range = v::mosStrength(static_cast<v::MosKind>(kind), input,
                                                              static_cast<v::Logic>(first),
                                                              static_cast<v::Logic>(second));
                            if (!range.has_value()) {
                                FAIL() << "missing validated value";
                            }
                            const auto logic = v::valueOf(*range);
                            EXPECT_EQ((words[std::size_t{2} * (at / 64)] >> (at % 64)) & 1,
                                      logic == v::Logic::One || logic == v::Logic::X);
                            EXPECT_EQ((words[std::size_t{2} * (at / 64) + 1] >> (at % 64)) & 1,
                                      logic == v::Logic::Z || logic == v::Logic::X);
                        }
                        ++designs;
                    }
    EXPECT_EQ(designs, 1920U);
}

TEST(StrengthVm, ParallelBranchCopiesKeepEveryPackedWordAndDoNotCrossWaits) {
    for (const unsigned width : {1U, 8U, 9U, 65U, 129U, 257U})
        for (const bool indirect : {false, true}) {
            Model m(width);
            const auto a = m.make(), b = m.emit(0, Op::StrengthMake, s::strengthType(width),
                                                {m.data(1), m.rank(3), m.rank(7)});
            const auto zero = m.constant(s::u2(1), {0}), one = m.constant(s::u2(1), {1});
            auto& p = m.d.processes[0];
            p.numTemps = 9;
            m.d.slots.push_back({"top.other", s::u4(width)});
            p.writes.push_back({s::SlotId{1}});
            p.blocks.resize(indirect ? 4 : 3);
            m.emit(0, Op::Br, s::voidType(), {bb(1, 3), a, zero, b});
            p.blocks[1].params = {{s::TempId{2}, s::strengthType(width)},
                                  {s::TempId{3}, s::u2(1)},
                                  {s::TempId{4}, s::strengthType(width)}};
            p.blocks[1].instrs = {
                {Op::BrIf,
                 s::voidType(),
                 s::kNone,
                 indirect ? std::vector<Operand>{temp(3), bb(2, 2), temp(2), temp(4), bb(3)}
                          : std::vector<Operand>{temp(3), bb(2, 2), temp(2), temp(4), bb(1, 3),
                                                 temp(4), one, temp(2)},
                 {0, 4, 7}}};
            if (indirect)
                p.blocks[3].instrs = {{Op::Br,
                                       s::voidType(),
                                       s::kNone,
                                       {bb(1, 3), temp(4), one, temp(2)},
                                       {0, 4, 7}}};
            p.blocks[2].params = {{s::TempId{5}, s::strengthType(width)},
                                  {s::TempId{6}, s::strengthType(width)}};
            p.blocks[2].instrs = {
                {Op::StrengthValue, s::u4(width), s::TempId{7}, {temp(5)}, {0, 4, 7}},
                {Op::Store,
                 s::voidType(),
                 s::kNone,
                 {Operand::slot(s::SlotId{0}), temp(7)},
                 {0, 4, 7}},
                {Op::StrengthValue, s::u4(width), s::TempId{8}, {temp(6)}, {0, 4, 7}},
                {Op::Store,
                 s::voidType(),
                 s::kNone,
                 {Operand::slot(s::SlotId{1}), temp(8)},
                 {0, 4, 7}},
                {Op::Ret, s::voidType(), s::kNone, {}, {0, 4, 7}}};
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            const auto* words = ctx.store().element(k::SlotId{0}, 0);
            for (unsigned at = 0; at < width; ++at) {
                const auto value = bits[(at + 1) % 4];
                EXPECT_EQ((words[std::size_t{2} * (at / 64)] >> (at % 64)) & 1, value.a);
                EXPECT_EQ((words[std::size_t{2} * (at / 64) + 1] >> (at % 64)) & 1, value.b);
                const auto* other = ctx.store().element(k::SlotId{1}, 0);
                const auto original = bits[at % 4];
                EXPECT_EQ((other[std::size_t{2} * (at / 64)] >> (at % 64)) & 1, original.a);
                EXPECT_EQ((other[std::size_t{2} * (at / 64) + 1] >> (at % 64)) & 1, original.b);
            }
        }
}
TEST(StrengthVm, ScalarControlsUseBothStateDomainsAndEitherSignedness) {
    for (const auto type : {s::u2(1), s::s2(1), s::u4(1), s::s4(1)})
        for (unsigned kind = 0; kind < 6; ++kind)
            for (unsigned first = 0; first < 2; ++first)
                for (unsigned second = 0; second < 2; ++second) {
                    Model m(9);
                    const auto made = m.make();
                    std::vector<Operand> args{Operand::str(names[kind]), made,
                                              m.control(first, type)};
                    if (kind >= 4) args.push_back(m.control(second, type));
                    const auto mos = m.emit(0, Op::StrengthMos, s::strengthType(9), args);
                    m.finish(mos);
                    ASSERT_TRUE(m.valid());
                    k::Context ctx;
                    const auto result = kirimasim::vm::run(m.d, ctx);
                    ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
                    const auto* output = ctx.store().element(k::SlotId{0}, 0);
                    for (unsigned at = 0; at < 9; ++at) {
                        const auto input = v::driverSignal({static_cast<v::Logic>(at % 4), 7, 3});
                        const auto range = v::mosStrength(static_cast<v::MosKind>(kind), input,
                                                          static_cast<v::Logic>(first),
                                                          static_cast<v::Logic>(second));
                        if (!range.has_value()) {
                            FAIL() << "missing reference result";
                        }
                        const auto logic = v::valueOf(*range);
                        EXPECT_EQ((output[0] >> at) & 1,
                                  logic == v::Logic::One || logic == v::Logic::X);
                        EXPECT_EQ((output[1] >> at) & 1,
                                  logic == v::Logic::Z || logic == v::Logic::X);
                    }
                }
}
TEST(StrengthVm, NestedFunctionTempsLeaveCallerStrengthValuesIntact) {
    for (const auto width : {9U, 65U, 129U})
        for (const bool scoped : {false, true}) {
            Model m(width);
            m.d.slots.push_back({"top.returned", s::u4(width)});
            m.d.processes[0].writes.push_back({s::SlotId{1}});
            const auto caller = m.make(), actual = m.data(1), inner = m.data(2);
            const auto seven = m.rank(7), three = m.rank(3);
            s::Subroutine fn;
            fn.name = "top.identity";
            fn.automatic = true;
            fn.returnType = s::u4(width);
            fn.frame = {{"input", s::u4(width)}};
            fn.params = {{"input", s::Direction::Input, s::u4(width), s::kNone, s::FrameVarId{0}}};
            fn.loc = {0, 2, 1};
            if (scoped) {
                fn.frame[0].home = s::FrameHome::Scoped;
                fn.frame[0].activationScope = s::ActivationScopeId{0};
                fn.activation.root = s::ActivationScopeId{0};
                fn.activation.scopes = {{s::kNone, {s::FrameVarId{0}}}};
            }
            fn.numTemps = 4;
            fn.blocks = {{{},
                          {{Op::Fload,
                            s::u4(width),
                            s::TempId{0},
                            {Operand::frame(s::FrameVarId{0})},
                            {0, 2, 1}},
                           {Op::StrengthMake,
                            s::strengthType(width),
                            s::TempId{1},
                            {temp(0), seven, three},
                            {0, 2, 1}},
                           {Op::Call,
                            s::u4(width),
                            s::TempId{2},
                            {Operand::subroutine(s::SubroutineId{1})},
                            {0, 2, 1}},
                           {Op::StrengthValue, s::u4(width), s::TempId{3}, {temp(1)}, {0, 2, 1}},
                           {Op::Ret, s::voidType(), s::kNone, {temp(3)}, {0, 2, 1}}}}};
            m.d.subroutines.push_back(std::move(fn));
            s::Subroutine nested;
            nested.name = "top.nested";
            nested.automatic = true;
            nested.returnType = s::u4(width);
            nested.loc = {0, 3, 1};
            nested.numTemps = 2;
            nested.blocks = {
                {{},
                 {{Op::StrengthMake,
                   s::strengthType(width),
                   s::TempId{0},
                   {inner, three, seven},
                   {0, 3, 1}},
                  {Op::StrengthValue, s::u4(width), s::TempId{1}, {temp(0)}, {0, 3, 1}},
                  {Op::Ret, s::voidType(), s::kNone, {temp(1)}, {0, 3, 1}}}}};
            m.d.subroutines.push_back(std::move(nested));
            const auto returned = m.emit(0, Op::Call, s::u4(width),
                                         {Operand::subroutine(s::SubroutineId{0}), actual});
            m.emit(0, Op::Store, s::voidType(), {Operand::slot(s::SlotId{1}), returned});
            m.finish(caller);
            ASSERT_TRUE(m.valid());
            k::Context ctx;
            const auto result = kirimasim::vm::run(m.d, ctx);
            ASSERT_EQ(result.reason, k::EndReason::Quiescent) << result.error;
            for (unsigned slot = 0; slot < 2; ++slot) {
                const auto* words = ctx.store().element(k::SlotId{slot}, 0);
                for (unsigned at = 0; at < width; ++at) {
                    const auto value = bits[(at + slot) % 4];
                    EXPECT_EQ((words[std::size_t{2} * (at / 64)] >> (at % 64)) & 1, value.a);
                    EXPECT_EQ((words[std::size_t{2} * (at / 64) + 1] >> (at % 64)) & 1, value.b);
                }
            }
        }
}
}  // namespace
