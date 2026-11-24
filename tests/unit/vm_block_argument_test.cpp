// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Block arguments (SIR_SPEC.md 5.1) in tier 1: a branch gives each
// parameter the whole of its argument, every chunk of a value wider than
// 64 bits, and reads all the arguments before it writes any parameter, so
// that a branch passing a block's own parameters back to it in another
// order (a parallel copy) loses nothing.
//
// The axes: the width (64, 65, 100, 128, 200), two- and four-state, signed
// and unsigned, the upper chunk (known, with x, with z), and the branch
// that passes the parameters back (br, br.if). The design:
//
//   bb0:                br bb1(A, 0, B)
//   bb1(%p, %n, %q):    %d = ceq %n, 1
//                       br.if %d, bb2(%p, %q), bb1(%q, 1, %p)   ; or bb3
//   bb3:                br bb1(%q, 1, %p)                      ; br only
//   bb2(%x, %y):        store top.x, %x; store top.y, %y; ret
//
// It ends with top.x = B and top.y = A. The narrow %n between the wide
// parameters checks that each argument lands at its own parameter.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text_v1.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/valsem/value.h"
#include "kirimasim/vm/vm.h"
#include "verified_run.h"

namespace {

using namespace kirimasim::sir;

enum class Upper { Known, X, Z };
enum class Back { BrIf, Br };

struct Value {
    std::vector<std::uint64_t> a, b;
};

Instr make(Op op, Type t, std::uint32_t r, std::vector<Operand> ops) {
    return {op, t, TempId{r}, std::move(ops), {0, 1, 1}};
}
Instr none(Op op, std::vector<Operand> ops) { return make(op, voidType(), kNone, std::move(ops)); }

/// A value of the type whose chunks all differ, from `seed`; the top
/// chunk holds x or z in some of its bits for Upper::X and Upper::Z.
Value pattern(Type t, Upper upper, std::uint64_t seed) {
    const std::uint32_t chunks = (t.width + 63) / 64;
    Value v;
    for (std::uint32_t k = 0; k < chunks; ++k) {
        v.a.push_back(seed ^ (0x0123456789abcdefULL * (k + 1)));
        v.b.push_back(0);
    }
    const std::uint32_t top = t.width - 64 * (chunks - 1);
    const std::uint64_t mask = top == 64 ? ~0ULL : (1ULL << top) - 1;
    // The highest eight bits of the value (the top bit alone at 65).
    const std::uint64_t unknown = (0xffULL << (top > 8 ? top - 8 : 0)) & mask;
    v.a.back() &= mask;
    if (upper == Upper::X) {
        v.a.back() |= unknown;
        v.b.back() |= unknown;
    } else if (upper == Upper::Z) {
        v.a.back() &= ~unknown;
        v.b.back() |= unknown;
    }
    if (!isFourState(t)) v.b.clear();
    return v;
}

/// The value of slot s after the run.
Value slotValue(const kirimasim::kernel::Context& c, SlotId s, Type t, std::uint32_t element = 0) {
    const std::uint64_t* p = c.store().element(kirimasim::kernel::SlotId{s.index()}, element);
    const std::size_t chunks = (t.width + 63) / 64;
    Value v;
    for (std::size_t k = 0; k < chunks; ++k) {
        if (isFourState(t)) {
            v.a.push_back(p[2 * k]);
            v.b.push_back(p[2 * k + 1]);
        } else {
            v.a.push_back(p[k]);
        }
    }
    return v;
}

Design swapDesign(Type t, Back back, const Value& a, const Value& b) {
    Design d;
    d.sources = {"block_argument.sv"};
    d.instances = {{"top", "top", kNone}};
    d.slots = {{"top.x", t, SlotKind::Var, {}, kObservable, InstanceId{0}, kNone, kNone, 0},
               {"top.y", t, SlotKind::Var, {}, kObservable, InstanceId{0}, kNone, kNone, 0}};
    d.constants = {{t, a.a, a.b}, {t, b.a, b.b}, {u2(8), {0}, {}}, {u2(8), {1}, {}}};
    enum : std::uint32_t { cA, cB, c0, c1 };
    enum : std::uint32_t { tP, tN, tQ, tX, tY, tD };
    const Operand A = Operand::constant(ConstId{cA}), B = Operand::constant(ConstId{cB});
    const Operand zero = Operand::constant(ConstId{c0}), one = Operand::constant(ConstId{c1});
    const Operand P = Operand::temp(TempId{tP}), N = Operand::temp(TempId{tN}),
                  Q = Operand::temp(TempId{tQ});

    Process p;
    p.name = "top.initial0";
    p.writes = {{SlotId{0}}, {SlotId{1}}};
    p.numTemps = 6;
    p.blocks.resize(back == Back::Br ? 4 : 3);
    p.blocks[0].instrs = {none(Op::Br, {Operand::block(BlockId{1}, 3), A, zero, B})};
    p.blocks[1].params = {{TempId{tP}, t}, {TempId{tN}, u2(8)}, {TempId{tQ}, t}};
    std::vector<Operand> brIf = {Operand::temp(TempId{tD}), Operand::block(BlockId{2}, 2), P, Q};
    if (back == Back::BrIf)
        brIf.insert(brIf.end(), {Operand::block(BlockId{1}, 3), Q, one, P});
    else
        brIf.push_back(Operand::block(BlockId{3}));
    p.blocks[1].instrs = {make(Op::Ceq, u2(1), tD, {N, one}), none(Op::BrIf, brIf)};
    p.blocks[2].params = {{TempId{tX}, t}, {TempId{tY}, t}};
    p.blocks[2].instrs = {none(Op::Store, {Operand::slot(SlotId{0}), Operand::temp(TempId{tX})}),
                          none(Op::Store, {Operand::slot(SlotId{1}), Operand::temp(TempId{tY})}),
                          none(Op::Ret, {})};
    if (back == Back::Br)
        p.blocks[3].instrs = {none(Op::Br, {Operand::block(BlockId{1}, 3), Q, one, P})};
    d.processes.push_back(std::move(p));
    return d;
}

std::string describe(Type t, Upper upper, Back back) {
    return std::string(isFourState(t) ? "four-state " : "two-state ") +
           (isSigned(t) ? "signed " : "unsigned ") + std::to_string(t.width) + " bits, upper " +
           (upper == Upper::Known ? "known"
            : upper == Upper::X   ? "x"
                                  : "z") +
           ", back by " + (back == Back::BrIf ? "br.if" : "br");
}

enum class FirstOperand { Constant, LaterParameter, Slot, Frame, AlignedSlotRange };

Design operandTypeDesign(Type t, Upper upper, FirstOperand first) {
    const Value a = pattern(t, upper, 0x112233445566778aULL);
    const Value b = pattern(t, Upper::Known, 0x99aabbccddeeff01ULL);
    Design d;
    d.sources = {"operand_types.sv"};
    d.instances = {{"top", "top", kNone}};
    d.constants = {{t, a.a, a.b}, {t, b.a, b.b}};
    d.slots = {{"top.different", u2(1), SlotKind::Var, {}, kObservable, InstanceId{0}},
               {"top.same", u2(1), SlotKind::Var, {}, kObservable, InstanceId{0}},
               {"top.source", t, SlotKind::Var, {}, kObservable, InstanceId{0}}};
    Process p;
    p.name = "top.initial0";
    p.writes = {{SlotId{0}}, {SlotId{1}}};
    p.blocks.resize(3);
    const auto A = Operand::constant(ConstId{0});
    const auto B = Operand::constant(ConstId{1});
    Operand input = A;
    std::uint32_t next = 0;
    if (first != FirstOperand::Constant) {
        input = Operand::temp(TempId{next++});
        if (first == FirstOperand::LaterParameter) {
            p.blocks[0].instrs = {none(Op::Br, {Operand::block(BlockId{2}, 1), A})};
            p.blocks[2].params = {{TempId{0}, t}};
        } else {
            const bool frame = first == FirstOperand::Frame;
            auto source = frame ? Operand::frame(FrameVarId{0}) : Operand::slot(SlotId{2});
            if (first == FirstOperand::AlignedSlotRange) {
                const unsigned slotWidth = t.width + 128;
                d.slots[2].type = isFourState(t) ? u4(slotWidth) : u2(slotWidth);
                source = Operand::slot(SlotId{2}, 64, t.width);
            }
            if (frame)
                p.frame = {{"source", t}};
            else {
                p.writes.push_back({SlotId{2}});
            }
            p.blocks[0].instrs = {none(frame ? Op::Fstore : Op::Store, {source, A}),
                                  none(Op::Br, {Operand::block(BlockId{2})})};
            p.blocks[2].instrs = {make(frame ? Op::Fload : Op::Load, t, 0, {source})};
        }
    } else {
        p.blocks[0].instrs = {none(Op::Br, {Operand::block(BlockId{2})})};
    }
    // Block 2 defines the type used by block 1. Its execution precedes
    // block 1 even though constructor traversal visits it afterwards.
    p.blocks[2].instrs.push_back(none(Op::Br, {Operand::block(BlockId{1})}));
    const auto different = next++;
    const auto same = next++;
    p.blocks[1].instrs = {
        make(Op::Ceq, u2(1), different, {input, B}), make(Op::Ceq, u2(1), same, {input, A}),
        none(Op::Store, {Operand::slot(SlotId{0}), Operand::temp(TempId{different})}),
        none(Op::Store, {Operand::slot(SlotId{1}), Operand::temp(TempId{same})}),
        none(Op::Ret, {})};
    p.numTemps = next;
    d.processes.push_back(std::move(p));
    return d;
}

void checkOperandTypes() {
    // The normal lookup/cache equality is checked at each executed
    // instruction in debug builds, including Slot/Frame and empty operands.
    for (const unsigned width : {1U, 63U, 64U, 65U, 127U, 128U, 129U, 255U, 256U, 257U, 512U}) {
        for (const Type t : {u2(width), s2(width), u4(width), s4(width)}) {
            for (const Upper upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(t) && upper != Upper::Known) continue;
                for (const auto first :
                     {FirstOperand::Constant, FirstOperand::LaterParameter, FirstOperand::Slot,
                      FirstOperand::Frame, FirstOperand::AlignedSlotRange}) {
                    if (first == FirstOperand::AlignedSlotRange && isSigned(t)) continue;
                    SCOPED_TRACE(::testing::Message()
                                 << width << '/' << static_cast<int>(t.kind) << '/'
                                 << static_cast<int>(upper) << '/' << static_cast<int>(first));
                    const auto design = operandTypeDesign(t, upper, first);
                    const auto errors = verify(design);
                    ASSERT_TRUE(errors.empty()) << errors.front().message;
                    for (unsigned repeat = 0; repeat < 2; ++repeat) {
                        kirimasim::kernel::Context context;
                        const auto result = kirimasim::test::runVerified(design, context);
                        ASSERT_EQ(result.reason, kirimasim::kernel::EndReason::Quiescent)
                            << result.error;
                        EXPECT_EQ(slotValue(context, SlotId{0}, u2(1)).a,
                                  std::vector<std::uint64_t>{0});
                        EXPECT_EQ(slotValue(context, SlotId{1}, u2(1)).a,
                                  std::vector<std::uint64_t>{1});
                    }
                }
            }
        }
    }
}

Design calculationDesign(Type inputType, Upper upper, FirstOperand first, Type resultType,
                         bool nba) {
    auto d = operandTypeDesign(inputType, upper, first);
    auto& p = d.processes[0];
    const auto input = p.blocks[1].instrs[0].operands[0];
    const auto result = p.numTemps - 2;
    const auto observed = result + 1;
    d.slots[0].type = resultType;
    d.slots[1].type = resultType;
    const std::vector<std::uint64_t> zeros((resultType.width + 63) / 64);
    d.constants.push_back(
        {resultType, zeros, isFourState(resultType) ? zeros : std::vector<std::uint64_t>{}});
    p.blocks[0].instrs.insert(
        p.blocks[0].instrs.begin(),
        none(Op::Store, {Operand::slot(SlotId{0}), Operand::constant(ConstId{2})}));
    p.blocks[1].instrs = {
        make(Op::Xor, resultType, result, {input, Operand::constant(ConstId{1})}),
        none(nba ? Op::Nba : Op::Store, {Operand::slot(SlotId{0}), Operand::temp(TempId{result})}),
        make(Op::Load, resultType, observed, {Operand::slot(SlotId{0})}),
        none(Op::Store, {Operand::slot(SlotId{1}), Operand::temp(TempId{observed})}),
        none(Op::Ret, {})};
    return d;
}

void expectValue(const kirimasim::kernel::Context& context, SlotId slot, Type type,
                 kirimasim::valsem::Value expected, std::uint32_t element = 0) {
    namespace v = kirimasim::valsem;
    if (!isFourState(type)) expected = v::to2(expected);
    Value words;
    v::toWords(expected, words.a, words.b);
    if (!isFourState(type)) words.b.clear();
    const auto actual = slotValue(context, slot, type, element);
    EXPECT_EQ(actual.a, words.a);
    EXPECT_EQ(actual.b, words.b);
}

void checkScalarCalculations() {
    namespace v = kirimasim::valsem;
    using Binary = v::Value (*)(const v::Value&, const v::Value&);
    struct Operation {
        Op op;
        Binary reference;
        bool comparison = false;
    };
    const std::array<Operation, 11> operations{{{Op::Add, v::add},
                                                {Op::Sub, v::sub},
                                                {Op::Mul, v::mul},
                                                {Op::Div, v::div},
                                                {Op::Mod, v::mod},
                                                {Op::And, v::bitAnd},
                                                {Op::Or, v::bitOr},
                                                {Op::Xor, v::bitXor},
                                                {Op::Eq, v::eq, true},
                                                {Op::Lt, v::lt, true},
                                                {Op::Select, nullptr}}};
    // VALUE_SEMANTICS 3 and SCHEDULER_SPEC 13. The reference works on bits;
    // the same operations cross the 64-bit boundary and all operand homes.
    for (const auto width : {1U, 7U, 32U, 63U, 64U, 65U, 129U, 257U})
        for (const Type type : {u2(width), s2(width), u4(width), s4(width)})
            for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(type) && upper != Upper::Known) continue;
                const auto a = pattern(type, upper, 0x112233445566778aULL);
                const auto b = pattern(type, Upper::Known, 0x99aabbccddeeff01ULL);
                const auto left = v::fromWords(a.a, a.b, width, isSigned(type));
                const auto right = v::fromWords(b.a, b.b, width, isSigned(type));
                const auto condition = v::filled(upper == Upper::Known ? v::Logic::One
                                                 : upper == Upper::X   ? v::Logic::X
                                                                       : v::Logic::Z,
                                                 1, false);
                for (const auto first : {FirstOperand::Constant, FirstOperand::LaterParameter,
                                         FirstOperand::Slot, FirstOperand::Frame})
                    for (const bool nba : {false, true})
                        for (const auto operation : operations) {
                            SCOPED_TRACE(::testing::Message()
                                         << width << '/' << static_cast<int>(type.kind) << '/'
                                         << static_cast<int>(upper) << '/'
                                         << static_cast<int>(first) << '/' << nba << '/'
                                         << opName(operation.op));
                            const auto resultType =
                                operation.comparison ? (isFourState(type) ? u4(1) : u2(1)) : type;
                            auto d = calculationDesign(type, upper, first, resultType, nba);
                            auto& instruction = d.processes[0].blocks[1].instrs[0];
                            instruction.op = operation.op;
                            if (operation.op == Op::Select) {
                                std::vector<std::uint64_t> ca, cb;
                                v::toWords(condition, ca, cb);
                                d.constants.push_back({u4(1), ca, cb});
                                instruction.operands.insert(instruction.operands.begin(),
                                                            Operand::constant(ConstId{3}));
                            }
                            const auto expected = operation.reference
                                                      ? operation.reference(left, right)
                                                      : v::select(condition, left, right);
                            const auto errors = verify(d);
                            ASSERT_TRUE(errors.empty()) << errors.front().message;
                            kirimasim::kernel::Context context;
                            const auto result = kirimasim::test::runVerified(d, context);
                            ASSERT_EQ(result.reason, kirimasim::kernel::EndReason::Quiescent)
                                << result.error;
                            expectValue(context, SlotId{0}, resultType, expected);
                            expectValue(context, SlotId{1}, resultType,
                                        nba ? v::fromUint(0, resultType.width, isSigned(resultType))
                                            : expected);
                        }
            }
}

void checkScalarMixedBlocks() {
    namespace v = kirimasim::valsem;
    for (const auto width : {1U, 7U, 32U, 64U, 65U, 129U})
        for (const Type type : {u2(width), s2(width), u4(width), s4(width)})
            for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(type) && upper != Upper::Known) continue;
                const auto a = pattern(type, upper, 0x112233445566778aULL);
                const auto b = pattern(type, Upper::Known, 0x99aabbccddeeff01ULL);
                const auto left = v::fromWords(a.a, a.b, width, isSigned(type));
                const auto right = v::fromWords(b.a, b.b, width, isSigned(type));
                for (const auto first : {FirstOperand::Constant, FirstOperand::LaterParameter,
                                         FirstOperand::Slot, FirstOperand::Frame})
                    for (const bool nba : {false, true})
                        for (const auto op : {Op::Div, Op::Mod}) {
                            SCOPED_TRACE(::testing::Message()
                                         << width << '/' << static_cast<int>(type.kind) << '/'
                                         << static_cast<int>(upper) << '/'
                                         << static_cast<int>(first) << '/' << nba << '/'
                                         << opName(op));
                            auto d = calculationDesign(type, upper, first, type, nba);
                            auto& p = d.processes[0];
                            auto& instructions = p.blocks[1].instrs;
                            const auto prefix = instructions[0].result;
                            const auto divided = TempId{p.numTemps++};
                            const auto inverted = TempId{p.numTemps++};
                            instructions[1].operands[1] = Operand::temp(inverted);
                            instructions.insert(
                                instructions.begin() + 1,
                                {make(op, type, divided.index(),
                                      {Operand::temp(prefix), Operand::constant(ConstId{1})}),
                                 make(Op::Not, type, inverted.index(), {Operand::temp(divided)})});
                            auto xored = v::bitXor(left, right);
                            if (!isFourState(type)) xored = v::to2(xored);
                            auto dividedValue =
                                op == Op::Div ? v::div(xored, right) : v::mod(xored, right);
                            // Each instruction publishes its declared type,
                            // including two-state division by zero before not.
                            if (!isFourState(type)) dividedValue = v::to2(dividedValue);
                            const auto expected = v::bitNot(dividedValue);
                            const auto errors = verify(d);
                            ASSERT_TRUE(errors.empty()) << errors.front().message;
                            kirimasim::kernel::Context context;
                            const auto result = kirimasim::test::runVerified(d, context);
                            ASSERT_EQ(result.reason, kirimasim::kernel::EndReason::Quiescent)
                                << result.error;
                            expectValue(context, SlotId{0}, type, expected);
                            expectValue(context, SlotId{1}, type,
                                        nba ? v::fromUint(0, width, isSigned(type)) : expected);
                        }
            }
}

void checkScalarArrays() {
    namespace v = kirimasim::valsem;
    struct Index {
        Constant value;
        std::int64_t position;
        bool valid;
    };
    const std::vector<Index> indices{{{s2(64), {static_cast<std::uint64_t>(-2)}, {}}, -2, false},
                                     {{s2(64), {UINT64_MAX}, {}}, -1, true},
                                     {{s2(64), {0}, {}}, 0, true},
                                     {{s2(64), {1}, {}}, 1, true},
                                     {{s2(64), {2}, {}}, 2, false},
                                     {{u2(65), {1, 0}, {}}, 1, true},
                                     {{u2(65), {0, 1}, {}}, 0, false},
                                     {{s2(65), {UINT64_MAX, 1}, {}}, -1, true},
                                     {{s4(64), {1}, {1}}, 0, false},
                                     {{s4(64), {0}, {1}}, 0, false}};
    const std::vector<std::vector<ArrayDim>> shapes{{{-1, 1}}, {{1, -1}}, {{-1, 1}, {4, 3}}};
    // SIR_SPEC 4.2 and VALUE_SEMANTICS 3.11: declaration-order cells,
    // invalid-index defaults and writes, with NBA observation before/after time advances.
    for (const auto width : {1U, 7U, 32U, 64U, 65U})
        for (const Type type : {u2(width), s2(width), u4(width), s4(width)})
            for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(type) && upper != Upper::Known) continue;
                const auto data = pattern(type, upper, 0x112233445566778aULL);
                const auto payload = v::fromWords(data.a, data.b, width, isSigned(type));
                const auto zero = v::fromUint(0, width, isSigned(type));
                const auto invalid = v::filled(isFourState(type) ? v::Logic::X : v::Logic::Zero,
                                               width, isSigned(type));
                for (const auto& dims : shapes) {
                    std::vector<std::vector<std::int64_t>> cells;
                    for (unsigned i = 0; i < 3; ++i) {
                        const auto first =
                            dims[0].left + (dims[0].left < dims[0].right ? i : -std::int64_t{i});
                        if (dims.size() == 1)
                            cells.push_back({first});
                        else
                            for (const auto second : {4, 3})
                                cells.push_back({first, second});
                    }
                    for (const auto& index : indices)
                        for (const bool frame : {false, true})
                            for (const bool nba : {false, true}) {
                                if (frame && nba) continue;
                                for (const bool parameter : {false, true})
                                    for (const bool bounds : {false, true}) {
                                        SCOPED_TRACE(::testing::Message()
                                                     << width << '/' << static_cast<int>(type.kind)
                                                     << '/' << static_cast<int>(upper) << '/'
                                                     << dims.size() << '/' << dims[0].left << '/'
                                                     << index.position << '/' << index.valid << '/'
                                                     << index.value.type.width << '/' << frame
                                                     << '/' << nba << '/' << parameter << '/'
                                                     << bounds);
                                        Design d;
                                        d.sources = {"scalar_arrays.sv"};
                                        d.instances = {{"top", "top", kNone}};
                                        const std::vector<std::uint64_t> zeros((width + 63) / 64);
                                        d.constants = {{type, data.a, data.b},
                                                       {type, zeros,
                                                        isFourState(type)
                                                            ? zeros
                                                            : std::vector<std::uint64_t>{}},
                                                       index.value,
                                                       {u2(64), {1}, {}}};
                                        d.slots = {{"top.observed", type},
                                                   {"top.final", type},
                                                   {"top.array", type, SlotKind::Var, dims}};
                                        Process process;
                                        process.name = "top.initial0";
                                        process.blocks.resize(3);
                                        process.writes = {{SlotId{0}}, {SlotId{1}}};
                                        if (!frame) process.writes.push_back({SlotId{2}});
                                        process.frame = {{"padding", u4(129)},
                                                         {"array", type, kNone, dims}};
                                        const auto home = frame ? Operand::frame(FrameVarId{1})
                                                                : Operand::slot(SlotId{2});
                                        const auto coordinate = [&](std::int64_t value) {
                                            const auto id = ConstId{
                                                static_cast<std::uint32_t>(d.constants.size())};
                                            d.constants.push_back(
                                                {s2(64), {static_cast<std::uint64_t>(value)}, {}});
                                            return Operand::constant(id);
                                        };
                                        const auto selection = [&](const Operand& first) {
                                            std::vector<Operand> operands{home, first};
                                            if (dims.size() == 2) operands.push_back(coordinate(3));
                                            return operands;
                                        };
                                        for (const auto& cell : cells) {
                                            std::vector<Operand> operands{home};
                                            for (const auto value : cell)
                                                operands.push_back(coordinate(value));
                                            operands.push_back(Operand::constant(ConstId{1}));
                                            process.blocks[0].instrs.push_back(
                                                none(Op::StoreElem, operands));
                                        }
                                        std::uint32_t next = 0;
                                        auto at = Operand::constant(ConstId{2});
                                        if (parameter) {
                                            at = Operand::temp(TempId{next++});
                                            process.blocks[1].params = {
                                                {at.asTemp(), index.value.type}};
                                            process.blocks[0].instrs.push_back(
                                                none(Op::Br, {Operand::block(BlockId{1}, 1),
                                                              Operand::constant(ConstId{2})}));
                                        } else {
                                            process.blocks[0].instrs.push_back(
                                                none(Op::Br, {Operand::block(BlockId{1})}));
                                        }
                                        auto write = selection(at);
                                        write.push_back(Operand::constant(ConstId{0}));
                                        process.blocks[1].instrs.push_back(
                                            none(nba ? Op::NbaElem : Op::StoreElem, write));
                                        const auto load = [&](unsigned block,
                                                              const std::vector<Operand>& operands,
                                                              SlotId target) {
                                            const auto temp = next++;
                                            auto& code = process.blocks[block].instrs;
                                            code.push_back(
                                                make(Op::LoadElem, type, temp, operands));
                                            code.push_back(
                                                none(Op::Store, {Operand::slot(target),
                                                                 Operand::temp(TempId{temp})}));
                                        };
                                        load(1, selection(at), SlotId{0});
                                        process.blocks[1].instrs.push_back(
                                            none(Op::WaitDelay, {Operand::constant(ConstId{3}),
                                                                 Operand::block(BlockId{2})}));
                                        load(2, selection(Operand::constant(ConstId{2})),
                                             SlotId{1});
                                        for (const auto& cell : cells) {
                                            const auto target =
                                                SlotId{static_cast<std::uint32_t>(d.slots.size())};
                                            d.slots.push_back(
                                                {"top.cell" + std::to_string(target.index()),
                                                 type});
                                            process.writes.push_back({target});
                                            std::vector<Operand> operands{home};
                                            for (const auto value : cell)
                                                operands.push_back(coordinate(value));
                                            load(2, operands, target);
                                        }
                                        process.blocks[2].instrs.push_back(none(Op::Ret, {}));
                                        process.numTemps = next;
                                        d.processes.push_back(std::move(process));
                                        const auto errors = verify(d);
                                        ASSERT_TRUE(errors.empty()) << errors.front().message;
                                        kirimasim::kernel::Context context;
                                        kirimasim::vm::RunOptions options;
                                        options.boundsCheck = bounds;
                                        const auto result =
                                            kirimasim::test::runVerified(d, context, options);
                                        ASSERT_EQ(result.reason,
                                                  kirimasim::kernel::EndReason::Quiescent)
                                            << result.error;
                                        expectValue(context, SlotId{0}, type,
                                                    index.valid ? (nba ? zero : payload) : invalid);
                                        expectValue(context, SlotId{1}, type,
                                                    index.valid ? payload : invalid);
                                        for (unsigned i = 0; i < cells.size(); ++i) {
                                            const bool selected =
                                                index.valid && cells[i][0] == index.position &&
                                                (cells[i].size() == 1 || cells[i][1] == 3);
                                            const auto expected = selected ? payload : zero;
                                            expectValue(context, SlotId{3 + i}, type, expected);
                                            if (!frame)
                                                expectValue(context, SlotId{2}, type, expected, i);
                                        }
                                    }
                            }
                }
            }
}

void checkScalarConcatenations() {
    namespace v = kirimasim::valsem;
    const std::vector<std::vector<unsigned>> shapes{
        {1},       {7},         {64},        {65},           {1, 1},
        {1, 31},   {31, 1},     {32, 32},    {1, 64},        {64, 1},
        {1, 6, 1}, {7, 25, 32}, {32, 32, 1}, {1, 7, 24, 32}, {32, 32, 32, 33}};
    // VALUE_SEMANTICS 7.1: left-to-right packed bits, regardless of input
    // signedness, with a four-state result if any input is four-state.
    for (const auto& widths : shapes) {
        unsigned total = 0;
        for (const auto width : widths)
            total += width;
        for (unsigned domains = 0; domains < (1U << widths.size()); ++domains)
            for (const bool signedInputs : {false, true})
                for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
                    if (domains == 0 && upper != Upper::Known) continue;
                    const auto resultType = domains != 0 ? u4(total) : u2(total);
                    std::vector<Type> types;
                    std::vector<Constant> constants;
                    std::vector<v::Value> parts;
                    for (unsigned i = 0; i < widths.size(); ++i) {
                        const bool four = (domains & (1U << i)) != 0;
                        const auto type = four ? (signedInputs ? s4(widths[i]) : u4(widths[i]))
                                               : (signedInputs ? s2(widths[i]) : u2(widths[i]));
                        const auto value = pattern(type, four ? upper : Upper::Known,
                                                   0x112233445566778aULL + i * 0x1234567ULL);
                        types.push_back(type);
                        constants.push_back({type, value.a, value.b});
                        parts.push_back(v::fromWords(value.a, value.b, widths[i], signedInputs));
                    }
                    const auto expected = v::concat(parts);
                    for (const auto home : {FirstOperand::Constant, FirstOperand::LaterParameter,
                                            FirstOperand::Slot, FirstOperand::Frame})
                        for (const bool nba : {false, true}) {
                            SCOPED_TRACE(::testing::Message()
                                         << total << '/' << widths.size() << '/' << widths.front()
                                         << '/' << widths.back() << '/' << domains << '/'
                                         << signedInputs << '/' << static_cast<int>(upper) << '/'
                                         << static_cast<int>(home) << '/' << nba);
                            Design d;
                            d.sources = {"scalar_concat.sv"};
                            d.instances = {{"top", "top", kNone}};
                            d.constants = constants;
                            const auto zero =
                                ConstId{static_cast<std::uint32_t>(d.constants.size())};
                            const std::vector<std::uint64_t> zeros((total + 63) / 64);
                            d.constants.push_back(
                                {resultType, zeros,
                                 domains != 0 ? zeros : std::vector<std::uint64_t>{}});
                            d.slots = {{"top.final", resultType}, {"top.observed", resultType}};
                            Process process;
                            process.name = "top.initial0";
                            process.writes = {{SlotId{0}}, {SlotId{1}}};
                            process.frame = {{"padding", u4(129)}};
                            process.blocks.resize(2);
                            auto& entry = process.blocks[0].instrs;
                            entry.push_back(none(
                                Op::Store, {Operand::slot(SlotId{0}), Operand::constant(zero)}));
                            std::uint32_t next = 0;
                            std::vector<Operand> arguments, operands;
                            for (unsigned i = 0; i < widths.size(); ++i) {
                                auto input = Operand::constant(ConstId{i});
                                if (home == FirstOperand::Slot || home == FirstOperand::Frame) {
                                    const bool frame = home == FirstOperand::Frame;
                                    Operand place;
                                    if (frame) {
                                        const auto id = FrameVarId{
                                            static_cast<std::uint32_t>(process.frame.size())};
                                        process.frame.push_back(
                                            {"part" + std::to_string(i), types[i]});
                                        place = Operand::frame(id);
                                    } else {
                                        const auto id =
                                            SlotId{static_cast<std::uint32_t>(d.slots.size())};
                                        d.slots.push_back(
                                            {"top.part" + std::to_string(i), types[i]});
                                        process.writes.push_back({id});
                                        place = Operand::slot(id);
                                    }
                                    entry.push_back(
                                        none(frame ? Op::Fstore : Op::Store, {place, input}));
                                    const auto temp = next++;
                                    entry.push_back(make(frame ? Op::Fload : Op::Load, types[i],
                                                         temp, {place}));
                                    input = Operand::temp(TempId{temp});
                                }
                                if (home != FirstOperand::Constant) {
                                    arguments.push_back(input);
                                    const auto parameter = TempId{next++};
                                    process.blocks[1].params.push_back({parameter, types[i]});
                                    input = Operand::temp(parameter);
                                }
                                operands.push_back(input);
                            }
                            std::vector<Operand> branch{Operand::block(
                                BlockId{1}, static_cast<std::uint32_t>(arguments.size()))};
                            branch.insert(branch.end(), arguments.begin(), arguments.end());
                            entry.push_back(none(Op::Br, branch));
                            const auto result = next++, observed = next++;
                            process.blocks[1].instrs = {
                                make(Op::Concat, resultType, result, operands),
                                none(nba ? Op::Nba : Op::Store,
                                     {Operand::slot(SlotId{0}), Operand::temp(TempId{result})}),
                                make(Op::Load, resultType, observed, {Operand::slot(SlotId{0})}),
                                none(Op::Store,
                                     {Operand::slot(SlotId{1}), Operand::temp(TempId{observed})}),
                                none(Op::Ret, {})};
                            process.numTemps = next;
                            d.processes.push_back(std::move(process));
                            const auto errors = verify(d);
                            ASSERT_TRUE(errors.empty()) << errors.front().message;
                            kirimasim::kernel::Context context;
                            const auto run = kirimasim::test::runVerified(d, context);
                            ASSERT_EQ(run.reason, kirimasim::kernel::EndReason::Quiescent)
                                << run.error;
                            expectValue(context, SlotId{0}, resultType, expected);
                            expectValue(context, SlotId{1}, resultType,
                                        nba ? v::fromUint(0, total, false) : expected);
                        }
                }
    }
}

void checkScalarSelections() {
    namespace v = kirimasim::valsem;
    for (const auto width : {1U, 7U, 32U, 63U, 64U, 65U, 129U})
        for (const Type type : {u2(width), s2(width), u4(width), s4(width)})
            for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(type) && upper != Upper::Known) continue;
                const auto a = pattern(type, upper, 0x112233445566778aULL);
                const auto value = v::fromWords(a.a, a.b, width, isSigned(type));
                for (const auto position :
                     {std::int64_t{-1}, std::int64_t{0}, std::int64_t{width / 2},
                      std::int64_t{width - 1}, std::int64_t{width}})
                    for (const bool part : {false, true}) {
                        if (part && (position < 0 || position >= width)) continue;
                        const auto count =
                            part ? std::min(7U, width - static_cast<unsigned>(position)) : 1U;
                        const Type resultType = isFourState(type) ? u4(count) : u2(count);
                        auto d =
                            calculationDesign(type, upper, FirstOperand::Slot, resultType, false);
                        d.constants.push_back({s2(64), {static_cast<std::uint64_t>(position)}, {}});
                        auto& instruction = d.processes[0].blocks[1].instrs[0];
                        instruction.op = part ? Op::Partsel : Op::Bitsel;
                        instruction.operands[1] = Operand::constant(ConstId{3});
                        const auto expected = v::bits(value, position, count);
                        const auto errors = verify(d);
                        ASSERT_TRUE(errors.empty()) << errors.front().message;
                        for (const bool bounds : {false, true}) {
                            SCOPED_TRACE(::testing::Message() << width << '/' << position << '/'
                                                              << count << '/' << bounds);
                            kirimasim::kernel::Context context;
                            kirimasim::vm::RunOptions options;
                            options.boundsCheck = bounds;
                            const auto result = kirimasim::test::runVerified(d, context, options);
                            ASSERT_EQ(result.reason, kirimasim::kernel::EndReason::Quiescent)
                                << result.error;
                            expectValue(context, SlotId{0}, resultType, expected);
                        }
                    }
            }
}

void checkScalarUnknownIndices() {
    namespace v = kirimasim::valsem;
    const std::array<Constant, 5> indices{{{s4(64), {1}, {1}},
                                           {s4(64), {0}, {1}},
                                           {u2(65), {3, 0}, {}},
                                           {u2(65), {3, 1}, {}},
                                           {s2(65), {UINT64_MAX, 1}, {}}}};
    for (const auto& index : indices) {
        auto d = calculationDesign(u4(32), Upper::Known, FirstOperand::Constant, u4(1), false);
        d.constants.push_back(index);
        auto& instruction = d.processes[0].blocks[1].instrs[0];
        instruction.op = Op::Bitsel;
        instruction.operands[1] = Operand::constant(ConstId{3});
        const auto& source = d.constants[0];
        const auto expected = v::bitSelect(
            v::fromWords(source.aWords, source.bWords, 32, false),
            v::fromWords(index.aWords, index.bWords, index.type.width, isSigned(index.type)));
        ASSERT_TRUE(verify(d).empty());
        for (const bool bounds : {false, true}) {
            kirimasim::kernel::Context context;
            kirimasim::vm::RunOptions options;
            options.boundsCheck = bounds;
            const auto result = kirimasim::test::runVerified(d, context, options);
            ASSERT_EQ(result.reason, kirimasim::kernel::EndReason::Quiescent) << result.error;
            expectValue(context, SlotId{0}, u4(1), expected);
        }
    }
}

// A following store observes each intermediate. A step limit may stop
// anywhere in the decoded address calculation, before any or all stores.
void checkScaledOffsets() {
    namespace v = kirimasim::valsem;
    for (const auto type : {u2(8), u4(8), u4(64)}) {
        for (const auto upper : {Upper::Known, Upper::X, Upper::Z}) {
            const auto input = pattern(u4(8), upper, 5);
            Design d;
            d.sources = {"scaled_offset.sv"};
            d.instances = {{"top", "top", kNone}};
            const std::vector<std::uint64_t> known(isFourState(type) ? 1 : 0, 0);
            d.constants = {{u4(8), input.a, input.b}, {type, {251}, known}, {type, {13}, known}};
            Process p;
            p.name = "top.initial0";
            p.numTemps = 4;
            p.blocks.resize(1);
            auto& ins = p.blocks[0].instrs;
            const auto tmp = [](unsigned i) { return Operand::temp(TempId{i}); };
            ins = {make(Op::Zext, u4(8), 0, {Operand::constant(ConstId{0})}),
                   make(isFourState(type) ? Op::Zext : Op::To2, type, 1, {tmp(0)}),
                   make(Op::Sub, type, 2, {tmp(1), Operand::constant(ConstId{1})}),
                   make(Op::Mul, type, 3, {tmp(2), Operand::constant(ConstId{2})})};
            for (unsigned i = 0; i < 3; ++i) {
                const SlotId id{i};
                d.slots.push_back({"top.v" + std::to_string(i),
                                   type,
                                   SlotKind::Var,
                                   {},
                                   kObservable,
                                   InstanceId{0}});
                p.writes.push_back({id});
                ins.push_back(none(Op::Store, {Operand::slot(id), tmp(i + 1)}));
            }
            ins.push_back(none(Op::Ret, {}));
            d.processes.push_back(std::move(p));
            const auto errors = verify(d);
            ASSERT_TRUE(errors.empty()) << errors.front().message;
            auto index = v::zext(v::fromWords(input.a, input.b, 8, false), type.width, false);
            if (!isFourState(type)) index = v::to2(index);
            const auto delta = v::sub(index, v::fromUint(251, type.width, false));
            const std::array values{index, delta,
                                    v::mul(delta, v::fromUint(13, type.width, false))};
            for (unsigned limit = 0; limit <= 8; ++limit) {
                SCOPED_TRACE(::testing::Message() << type.width << '/' << isFourState(type) << '/'
                                                  << static_cast<int>(upper) << '/' << limit);
                kirimasim::kernel::Context context;
                kirimasim::vm::RunOptions options;
                options.maxSteps = limit;
                const auto result = kirimasim::test::runVerified(d, context, options);
                const bool stopped = limit && limit < 8;
                EXPECT_EQ(result.reason, stopped ? kirimasim::kernel::EndReason::ZeroTimeLimit
                                                 : kirimasim::kernel::EndReason::Quiescent);
                EXPECT_EQ(context.counters().value(context.counters().add("vm.instrs")),
                          stopped ? limit + 1 : 8);
                for (unsigned i = 0; i < values.size(); ++i)
                    expectValue(context, SlotId{i}, type,
                                limit && limit < 5 + i ? v::filled(v::Logic::X, type.width, false)
                                                       : values[i]);
            }
        }
    }
}

void checkScalarInstructionLimits() {
    namespace v = kirimasim::valsem;
    const auto d = calculationDesign(u4(32), Upper::Known, FirstOperand::Constant, u4(32), false);
    for (const auto limit : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U}) {
        kirimasim::kernel::Context context;
        kirimasim::vm::RunOptions options;
        options.maxSteps = limit;
        const auto result = kirimasim::test::runVerified(d, context, options);
        const bool stopped = limit != 0 && limit < 8;
        EXPECT_EQ(result.reason, stopped ? kirimasim::kernel::EndReason::ZeroTimeLimit
                                         : kirimasim::kernel::EndReason::Quiescent)
            << result.error;
        EXPECT_EQ(context.counters().value(context.counters().add("vm.instrs")),
                  stopped ? limit + 1 : 8);
        const auto& a = d.constants[0];
        const auto& b = d.constants[1];
        const auto expected = v::bitXor(v::fromWords(a.aWords, a.bWords, 32, false),
                                        v::fromWords(b.aWords, b.bWords, 32, false));
        // store-zero, br, br, xor, store-result, load, store-observed, ret.
        expectValue(context, SlotId{0}, u4(32),
                    limit != 0 && limit < 5 ? v::fromUint(0, 32, false) : expected);
        expectValue(context, SlotId{1}, u4(32),
                    limit != 0 && limit < 7 ? v::filled(v::Logic::X, 32, false) : expected);
    }
}

TEST(VmBlockArgument, EveryChunkPassesAndArgumentsAreReadFirst) {
    ASSERT_NO_FATAL_FAILURE(checkOperandTypes());
    ASSERT_NO_FATAL_FAILURE(checkScalarCalculations());
    ASSERT_NO_FATAL_FAILURE(checkScalarMixedBlocks());
    ASSERT_NO_FATAL_FAILURE(checkScalarArrays());
    ASSERT_NO_FATAL_FAILURE(checkScalarConcatenations());
    ASSERT_NO_FATAL_FAILURE(checkScalarSelections());
    ASSERT_NO_FATAL_FAILURE(checkScalarUnknownIndices());
    ASSERT_NO_FATAL_FAILURE(checkScalarInstructionLimits());
    ASSERT_NO_FATAL_FAILURE(checkScaledOffsets());
    int cases = 0;
    for (const std::uint32_t w : {64U, 65U, 100U, 128U, 200U}) {
        for (const Type t : {u4(w), s4(w), u2(w), s2(w)}) {
            for (const Upper upper : {Upper::Known, Upper::X, Upper::Z}) {
                if (!isFourState(t) && upper != Upper::Known) continue;
                for (const Back back : {Back::BrIf, Back::Br}) {
                    SCOPED_TRACE(describe(t, upper, back));
                    const Value a = pattern(t, upper, 0x112233445566778aULL);
                    const Value b = pattern(t, Upper::Known, 0x99aabbccddeeff01ULL);
                    const Design d = swapDesign(t, back, a, b);
                    const auto errors = verify(d);
                    ASSERT_TRUE(errors.empty()) << errors.front().message;
                    kirimasim::kernel::Context c;
                    const auto r = kirimasim::test::runVerified(d, c);
                    ASSERT_EQ(r.reason, kirimasim::kernel::EndReason::Quiescent) << r.error;
                    const Value x = slotValue(c, SlotId{0}, t), y = slotValue(c, SlotId{1}, t);
                    EXPECT_EQ(x.a, b.a);
                    EXPECT_EQ(x.b, b.b);
                    EXPECT_EQ(y.a, a.a);
                    EXPECT_EQ(y.b, a.b);

                    const auto first = verifyDesign(d);
                    if (!first.verified) FAIL() << "fixture did not verify";
                    const auto text = print1(*first.verified);
                    if (!text) FAIL() << "cannot serialize fixture";
                    const auto parsed = parse1(*text);
                    if (!parsed.verified) FAIL() << "cannot read back fixture";
                    kirimasim::kernel::Context readback;
                    const auto rerun = kirimasim::vm::run(*parsed.verified, readback);
                    ASSERT_EQ(rerun.reason, r.reason) << rerun.error;
                    const auto readbackX = slotValue(readback, SlotId{0}, t);
                    const auto readbackY = slotValue(readback, SlotId{1}, t);
                    EXPECT_EQ(readbackX.a, x.a);
                    EXPECT_EQ(readbackX.b, x.b);
                    EXPECT_EQ(readbackY.a, y.a);
                    EXPECT_EQ(readbackY.b, y.b);
                    ++cases;
                }
            }
        }
    }
    EXPECT_EQ(cases, 5 * (2 * 3 + 2) * 2);
}

}  // namespace
