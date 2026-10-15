// Tests for the SIR data structures, the text writer and the verifier.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "haremasim/sir/design.h"
#include "haremasim/sir/text.h"
#include "haremasim/sir/verify.h"

namespace {

using namespace haremasim::sir;

SourceLoc at(std::uint32_t line, std::uint32_t col = 1) { return {0, line, col}; }

Instr make(Op op, Type t, TempId r, std::vector<Operand> ops, std::uint32_t line) {
    return {op, t, r, std::move(ops), at(line)};
}

Constant constU4(std::uint32_t w, std::uint64_t a, std::uint64_t b = 0) {
    return {u4(w), {a}, {b}};
}

/// The counter of SIR_SPEC.md section 9:
///   always @(posedge clk) if (!rst_n) q <= '0; else q <= q + 1;
Design counter() {
    Design d;
    d.sources = {"counter.sv"};
    d.instances = {{"top", "counter", kNone}};
    d.slots = {
        {"top.clk", u4(1), Domain::Four, 0, SlotKind::Var, {}, kObservable, 0, kNone},
        {"top.rst_n", u4(1), Domain::Four, 0, SlotKind::Var, {}, kObservable, 0, kNone},
        {"top.q", u4(8), Domain::Four, 0, SlotKind::Var, {}, kObservable, 0, kNone},
    };
    d.names = {{"top.clk", 0}, {"top.rst_n", 1}, {"top.q", 2}};
    d.constants = {constU4(8, 0), constU4(8, 1)};
    Process p;
    p.name = "ff_q";
    p.kind = ProcessKind::FfPlain;
    p.triggers = {{0, 0, 0, Edge::Pos}};
    p.loc = at(3);
    p.numTemps = 5;
    p.blocks.resize(3);
    p.blocks[0].instrs = {
        make(Op::Load, u4(1), 0, {Operand::slot(1)}, 3),
        make(Op::Lnot, u4(1), 1, {Operand::temp(0)}, 3),
        make(Op::Truth, u4(1), 2, {Operand::temp(1)}, 3),
        make(Op::BrIf, voidType(), kNone,
             {Operand::temp(2), Operand::block(1), Operand::block(2)}, 3),
    };
    p.blocks[1].instrs = {
        make(Op::Nba, voidType(), kNone, {Operand::slot(2), Operand::constant(0)}, 4),
        make(Op::Ret, voidType(), kNone, {}, 4),
    };
    p.blocks[2].instrs = {
        make(Op::Load, u4(8), 3, {Operand::slot(2)}, 5),
        make(Op::Add, u4(8), 4, {Operand::temp(3), Operand::constant(1)}, 5),
        make(Op::Nba, voidType(), kNone, {Operand::slot(2), Operand::temp(4)}, 5),
        make(Op::Ret, voidType(), kNone, {}, 5),
    };
    d.processes.push_back(std::move(p));
    return d;
}

bool mentions(const std::vector<VerifyError>& errors, const std::string& text) {
    for (const auto& e : errors)
        if (e.message.find(text) != std::string::npos) return true;
    return false;
}

TEST(Sir, CounterVerifies) {
    const auto errors = verify(counter());
    for (const auto& e : errors)
        ADD_FAILURE() << e.where << ": " << e.message;
    EXPECT_TRUE(errors.empty());
}

TEST(Sir, CounterText) {
    const std::string expected = R"(; sir format 0
source 0 "counter.sv"
instance 0 top def=counter parent=-
slot %top.clk : i4<1> var domain=four inst=0 flags(observable)
slot %top.rst_n : i4<1> var domain=four inst=0 flags(observable)
slot %top.q : i4<8> var domain=four inst=0 flags(observable)

process @ff_q kind=ff-plain inst=0 trigger=posedge(%top.clk) {  ; 0:3:1
bb0:
  %t0 = load i4<1> %top.rst_n  ; 0:3:1
  %t1 = lnot i4<1> %t0  ; 0:3:1
  %t2 = truth i4<1> %t1  ; 0:3:1
  br.if %t2, bb1, bb2  ; 0:3:1
bb1:
  nba %top.q, const i4<8> 8'h00  ; 0:4:1
  ret  ; 0:4:1
bb2:
  %t3 = load i4<8> %top.q  ; 0:5:1
  %t4 = add i4<8> %t3, const i4<8> 8'h01  ; 0:5:1
  nba %top.q, %t4  ; 0:5:1
  ret  ; 0:5:1
}
)";
    EXPECT_EQ(toText(counter()), expected);
}

TEST(Sir, TextIsDeterministic) { EXPECT_EQ(toText(counter()), toText(counter())); }

TEST(Sir, ConstantText) {
    EXPECT_EQ(constantText(constU4(4, 0b1010)), "4'ha");
    // 4'b1x0z: bit 3 = 1, bit 2 = x, bit 1 = 0, bit 0 = z.
    EXPECT_EQ(constantText(constU4(4, 0b1100, 0b0101)), "4'b1x0z");
    EXPECT_EQ(constantText({u2(9), {0x1ff}, {}}), "9'h1ff");
}

TEST(Sir, MissingTerminator) {
    Design d = counter();
    d.processes[0].blocks[1].instrs.pop_back();
    EXPECT_TRUE(mentions(verify(d), "does not end with a terminator"));
}

TEST(Sir, TerminatorInTheMiddle) {
    Design d = counter();
    auto& instrs = d.processes[0].blocks[1].instrs;
    instrs.insert(instrs.begin(), make(Op::Ret, voidType(), kNone, {}, 4));
    EXPECT_TRUE(mentions(verify(d), "is not the last instruction"));
}

TEST(Sir, UnreachableBlock) {
    Design d = counter();
    d.processes[0].blocks.push_back({{make(Op::Ret, voidType(), kNone, {}, 6)}});
    EXPECT_TRUE(mentions(verify(d), "unreachable"));
}

TEST(Sir, UseBeforeDefinition) {
    Design d = counter();
    // bb1 does not see %t3, which is defined in bb2.
    auto& instrs = d.processes[0].blocks[1].instrs;
    instrs[0] = make(Op::Nba, voidType(), kNone, {Operand::slot(2), Operand::temp(3)}, 4);
    EXPECT_TRUE(mentions(verify(d), "not defined before use"));
}

TEST(Sir, TempDefinedTwice) {
    Design d = counter();
    d.processes[0].blocks[2].instrs[1].result = 3;
    EXPECT_TRUE(mentions(verify(d), "defined twice"));
}

TEST(Sir, WaitInFlipFlop) {
    Design d = counter();
    auto& instrs = d.processes[0].blocks[1].instrs;
    instrs.back() = make(Op::WaitDelay, voidType(), kNone,
                         {Operand::constant(1), Operand::block(2)}, 4);
    EXPECT_TRUE(mentions(verify(d), "wait.delay in a ff-plain process"));
}

TEST(Sir, WaitInInitialIsAllowed) {
    Design d = counter();
    d.processes[0].kind = ProcessKind::Initial;
    d.processes[0].triggers.clear();
    auto& instrs = d.processes[0].blocks[1].instrs;
    instrs.back() = make(Op::WaitDelay, voidType(), kNone,
                         {Operand::constant(1), Operand::block(2)}, 4);
    EXPECT_TRUE(verify(d).empty());
}

TEST(Sir, AlwaysFfNeedsAnEventControl) {
    Design d = counter();
    d.processes[0].kind = ProcessKind::Ff;
    d.processes[0].triggers.clear();
    EXPECT_TRUE(mentions(verify(d), "exactly one event control"));
}

TEST(Sir, StoreTypeMismatch) {
    Design d = counter();
    // A 1-bit value stored into the 8-bit q.
    d.processes[0].blocks[2].instrs[2] =
        make(Op::Nba, voidType(), kNone, {Operand::slot(2), Operand::temp(0)}, 5);
    EXPECT_TRUE(mentions(verify(d), "does not fit"));
}

TEST(Sir, AddNeedsMatchingTypes) {
    Design d = counter();
    d.processes[0].blocks[2].instrs[1].type = u4(9);
    EXPECT_TRUE(mentions(verify(d), "same integral type"));
}

TEST(Sir, SlotRangeOutOfBounds) {
    Design d = counter();
    d.processes[0].blocks[2].instrs[0] = make(Op::Load, u4(4), 3, {Operand::slot(2, 6, 4)}, 5);
    EXPECT_TRUE(mentions(verify(d), "slot range is invalid"));
}

TEST(Sir, PartOfASlotIsUnsigned) {
    Design d = counter();
    d.slots[2].type = s4(8);
    // Bits [3:0] of a signed slot read as unsigned (like a part-select).
    d.processes[0].blocks[2].instrs[0] = make(Op::Load, u4(4), 3, {Operand::slot(2, 0, 4)}, 5);
    d.processes[0].blocks[2].instrs[1] =
        make(Op::Zext, u4(8), 4, {Operand::temp(3)}, 5);
    const auto errors = verify(d);
    EXPECT_FALSE(mentions(errors, "load"));
}

TEST(Sir, ConstantHighBitsMustBeZero) {
    Design d = counter();
    d.constants[0] = constU4(8, 0x100);
    EXPECT_TRUE(mentions(verify(d), "bits above the width"));
}

TEST(Sir, MissingSourceLocation) {
    Design d = counter();
    d.processes[0].blocks[0].instrs[0].loc = {};
    EXPECT_TRUE(mentions(verify(d), "no source location"));
}

TEST(Sir, DuplicateSlotName) {
    Design d = counter();
    d.slots[1].name = "top.clk";
    EXPECT_TRUE(mentions(verify(d), "not unique"));
}

TEST(Sir, DomainMustMatchType) {
    Design d = counter();
    d.slots[0].domain = Domain::Two;
    EXPECT_TRUE(mentions(verify(d), "domain does not match"));
    d.slots[0].flags |= kProvenNoX;
    EXPECT_FALSE(mentions(verify(d), "domain does not match"));
}

}  // namespace
