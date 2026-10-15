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
        {"top.clk", u4(1), SlotKind::Var, {}, kObservable, 0, kNone, kNone, 0},
        {"top.rst_n", u4(1), SlotKind::Var, {}, kObservable, 0, kNone, kNone, 0},
        {"top.q", u4(8), SlotKind::Var, {}, kObservable, 0, kNone, kNone, 0},
    };
    d.names = {{"top.clk", 0}, {"top.rst_n", 1}, {"top.q", 2}};
    d.constants = {constU4(8, 0), constU4(8, 1)};
    Process p;
    p.name = "ff_q";
    p.kind = ProcessKind::FfPlain;
    p.triggers = {{{0, 0, 0, 0, 0}, Edge::Pos}};
    p.writes = {{2, 0, 0, 0, 0}};
    p.loc = at(3);
    p.numTemps = 5;
    p.blocks.resize(3);
    p.blocks[0].instrs = {
        make(Op::Load, u4(1), 0, {Operand::slot(1)}, 3),
        make(Op::Lnot, u4(1), 1, {Operand::temp(0)}, 3),
        make(Op::Truth, u4(1), 2, {Operand::temp(1)}, 3),
        make(Op::BrIf, voidType(), kNone, {Operand::temp(2), Operand::block(1), Operand::block(2)},
             3),
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
precision 1ns
source 0 "counter.sv"
instance 0 "top" def="counter" parent=-
slot %s0 "top.clk" : i4<1> var inst=0 flags(observable)
slot %s1 "top.rst_n" : i4<1> var inst=0 flags(observable)
slot %s2 "top.q" : i4<8> var inst=0 flags(observable)

process @0 "ff_q" kind=ff-plain inst=0 trigger=posedge(%s0) writes=%s2 {  ; 0:3:1
bb0:
  %t0 = load i4<1> %s1  ; 0:3:1
  %t1 = lnot i4<1> %t0  ; 0:3:1
  %t2 = truth i4<1> %t1  ; 0:3:1
  br.if %t2, bb1, bb2  ; 0:3:1
bb1:
  nba %s2, const i4<8> 8'h00  ; 0:4:1
  ret  ; 0:4:1
bb2:
  %t3 = load i4<8> %s2  ; 0:5:1
  %t4 = add i4<8> %t3, const i4<8> 8'h01  ; 0:5:1
  nba %s2, %t4  ; 0:5:1
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
    d.processes[0].blocks.push_back({{}, {make(Op::Ret, voidType(), kNone, {}, 6)}});
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
    instrs.back() =
        make(Op::WaitDelay, voidType(), kNone, {Operand::constant(1), Operand::block(2)}, 4);
    EXPECT_TRUE(mentions(verify(d), "wait.delay in a ff-plain process"));
}

TEST(Sir, WaitInInitialIsAllowed) {
    Design d = counter();
    d.processes[0].kind = ProcessKind::Initial;
    d.processes[0].triggers.clear();
    auto& instrs = d.processes[0].blocks[1].instrs;
    instrs.back() =
        make(Op::WaitDelay, voidType(), kNone, {Operand::constant(1), Operand::block(2)}, 4);
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

TEST(Sir, MatchMaskTakesTwoValuesAndAKind) {
    // VALUE_SEMANTICS.md 4.2: casez and casex matching gives i2<1>.
    Design d = counter();
    auto& instrs = d.processes[0].blocks[2].instrs;
    d.processes[0].numTemps = 6;
    const auto match = [](Type t, const char* kind) {
        return make(Op::MatchMask, t, 5,
                    {Operand::temp(3), Operand::constant(1), Operand::str(kind)}, 5);
    };
    instrs.insert(instrs.begin() + 1, match(u2(1), "casez"));
    EXPECT_TRUE(verify(d).empty());
    EXPECT_NE(toText(d).find("match.mask i2<1> %t3, const i4<8> 8'h01, \"casez\""),
              std::string::npos);
    instrs[1] = match(u2(1), "case");
    EXPECT_TRUE(mentions(verify(d), "casez or casex"));
    instrs[1] = match(u4(1), "casex");
    EXPECT_TRUE(mentions(verify(d), "result must be i2<1>"));
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
    d.processes[0].blocks[2].instrs[1] = make(Op::Zext, u4(8), 4, {Operand::temp(3)}, 5);
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

TEST(Sir, TimePrecision) {
    // The global time precision is printed as a unit and kept within
    // 1fs..100s (SCHEDULER_SPEC.md 1).
    Design d = counter();
    d.timePrecision = -11;
    EXPECT_TRUE(verify(d).empty());
    EXPECT_NE(toText(d).find("\nprecision 10ps\n"), std::string::npos);
    d.timePrecision = -16;
    EXPECT_TRUE(mentions(verify(d), "time precision"));
    d.timePrecision = 3;
    EXPECT_TRUE(mentions(verify(d), "time precision"));
}

TEST(Sir, DomainFollowsTheType) {
    // The domain is not stored; it follows from the declared type (SIR_SPEC.md 3.2).
    EXPECT_EQ(domainOf(u2(8)), Domain::Two);
    EXPECT_EQ(domainOf(s4(8)), Domain::Four);
    EXPECT_EQ(domainOf({TypeKind::ShortReal, 0}), Domain::Real);
    EXPECT_EQ(domainOf({TypeKind::String, 0}), Domain::Handle);
}

// ---- M3b: block parameters, frames, arrays, tables (SIR_SPEC.md 2888a21) ----

Constant constS2(std::uint32_t w, std::uint64_t a) { return {s2(w), {a}, {}, {}}; }

/// SIR_SPEC.md section 9, second example:
///   initial for (int i = 0; i < 3; i++) #5 q = i;
/// i is a frame variable because it lives across the wait.
Design loopWithWait() {
    Design d = counter();
    d.processes.clear();
    d.constants = {constS2(32, 0), constS2(32, 3), {u2(64), {5}, {}, {}}, constS2(32, 1)};
    Process p;
    p.name = "counter.initial0";
    p.kind = ProcessKind::Initial;
    p.writes = {{2, 0, 0, 0, 0}};
    p.frame = {{"i", s2(32), kNone}};
    p.loc = at(1);
    p.numTemps = 6;
    p.blocks.resize(5);
    p.blocks[0].instrs = {
        make(Op::Fstore, voidType(), kNone, {Operand::frame(0), Operand::constant(0)}, 1),
        make(Op::Br, voidType(), kNone, {Operand::block(1)}, 1),
    };
    p.blocks[1].instrs = {
        make(Op::Fload, s2(32), 0, {Operand::frame(0)}, 1),
        make(Op::Lt, u2(1), 1, {Operand::temp(0), Operand::constant(1)}, 1),
        make(Op::BrIf, voidType(), kNone, {Operand::temp(1), Operand::block(2), Operand::block(4)},
             1),
    };
    p.blocks[2].instrs = {
        make(Op::WaitDelay, voidType(), kNone, {Operand::constant(2), Operand::block(3)}, 1),
    };
    p.blocks[3].instrs = {
        make(Op::Fload, s2(32), 2, {Operand::frame(0)}, 1),
        make(Op::Trunc, u2(8), 3, {Operand::temp(2)}, 1),
        make(Op::To4, u4(8), 4, {Operand::temp(3)}, 1),
        make(Op::Store, voidType(), kNone, {Operand::slot(2), Operand::temp(4)}, 1),
        make(Op::Add, s2(32), 5, {Operand::temp(2), Operand::constant(3)}, 1),
        make(Op::Fstore, voidType(), kNone, {Operand::frame(0), Operand::temp(5)}, 1),
        make(Op::Br, voidType(), kNone, {Operand::block(1)}, 1),
    };
    p.blocks[4].instrs = {make(Op::Ret, voidType(), kNone, {}, 1)};
    d.processes.push_back(std::move(p));
    return d;
}

/// A loop without waits whose counter is a block parameter:
///   bb0: br bb1(0)   bb1(%t0): if (%t0 < 4) { br bb1(%t0 + 1) } else ret
Design loopWithParam() {
    Design d = counter();
    d.processes.clear();
    d.constants = {constS2(32, 0), constS2(32, 4), constS2(32, 1)};
    Process p;
    p.name = "counter.initial0";
    p.kind = ProcessKind::Initial;
    p.loc = at(1);
    p.numTemps = 3;
    p.blocks.resize(4);
    p.blocks[0].instrs = {
        make(Op::Br, voidType(), kNone, {Operand::block(1, 1), Operand::constant(0)}, 1),
    };
    p.blocks[1].params = {{0, s2(32)}};
    p.blocks[1].instrs = {
        make(Op::Lt, u2(1), 1, {Operand::temp(0), Operand::constant(1)}, 1),
        make(Op::BrIf, voidType(), kNone, {Operand::temp(1), Operand::block(2), Operand::block(3)},
             1),
    };
    p.blocks[2].instrs = {
        make(Op::Add, s2(32), 2, {Operand::temp(0), Operand::constant(2)}, 1),
        make(Op::Br, voidType(), kNone, {Operand::block(1, 1), Operand::temp(2)}, 1),
    };
    p.blocks[3].instrs = {make(Op::Ret, voidType(), kNone, {}, 1)};
    d.processes.push_back(std::move(p));
    return d;
}

void expectClean(const Design& d) {
    const auto errors = verify(d);
    for (const auto& e : errors)
        ADD_FAILURE() << e.where << ": " << e.message;
    EXPECT_TRUE(errors.empty());
}

TEST(Sir, LoopWithWaitVerifies) { expectClean(loopWithWait()); }

TEST(Sir, LoopWithParamVerifies) { expectClean(loopWithParam()); }

TEST(Sir, LoopText) {
    const std::string text = toText(loopWithWait());
    EXPECT_NE(text.find("process @0 \"counter.initial0\" kind=initial inst=0 writes=%s2 {"),
              std::string::npos)
        << text;
    EXPECT_NE(text.find("  frame %f0 \"i\" : s2<32>\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  fstore %f0, const s2<32> 32'h00000000"), std::string::npos) << text;
    EXPECT_NE(text.find("  wait.delay const i2<64> 64'h0000000000000005, bb3"), std::string::npos)
        << text;

    const std::string params = toText(loopWithParam());
    EXPECT_NE(params.find("  br bb1(const s2<32> 32'h00000000)"), std::string::npos) << params;
    EXPECT_NE(params.find("bb1(%t0: s2<32>):\n"), std::string::npos) << params;
    EXPECT_NE(params.find("  br bb1(%t2)"), std::string::npos) << params;
}

TEST(Sir, TempUsedAfterAWait) {
    Design d = loopWithWait();
    // Use %t0 (read before the wait) after the wait instead of reading again.
    d.processes[0].blocks[3].instrs[1] = make(Op::Trunc, u2(8), 3, {Operand::temp(0)}, 1);
    EXPECT_TRUE(mentions(verify(d), "temp %t0 is used after a wait"));
}

TEST(Sir, BlockArgumentCount) {
    Design d = loopWithParam();
    d.processes[0].blocks[2].instrs[1] = make(Op::Br, voidType(), kNone, {Operand::block(1)}, 1);
    EXPECT_TRUE(mentions(verify(d), "bb1 takes 1 arguments"));
}

TEST(Sir, BlockArgumentType) {
    Design d = loopWithParam();
    d.constants[0] = {u2(32), {0}, {}, {}};
    EXPECT_TRUE(mentions(verify(d), "argument 0 of bb1 has the wrong type"));
}

TEST(Sir, EntryBlockHasNoParameters) {
    Design d = loopWithParam();
    d.processes[0].blocks[0].params = {{2, s2(32)}};
    EXPECT_TRUE(mentions(verify(d), "entry block has parameters"));
}

TEST(Sir, BlockAfterAWaitTakesNoArguments) {
    Design d = loopWithWait();
    d.processes[0].blocks[3].params = {{0, s2(32)}};
    d.processes[0].blocks[1].instrs[0] = make(Op::Fload, s2(32), 1, {Operand::frame(0)}, 1);
    EXPECT_TRUE(mentions(verify(d), "the block after a wait takes no arguments"));
}

TEST(Sir, FrameVariableOutOfRange) {
    Design d = loopWithWait();
    d.processes[0].blocks[0].instrs[0] =
        make(Op::Fstore, voidType(), kNone, {Operand::frame(1), Operand::constant(0)}, 1);
    EXPECT_TRUE(mentions(verify(d), "frame variable out of range"));
}

TEST(Sir, WriteOutsideTheWriteSet) {
    Design d = loopWithWait();
    d.processes[0].writes.clear();
    EXPECT_TRUE(mentions(verify(d), "outside the write set"));
}

TEST(Sir, AlwaysCombWritesAreExclusive) {
    // Rule 6: an always_comb writes q, and so does the ff-plain process.
    Design d = counter();
    Process c;
    c.name = "comb_q";
    c.kind = ProcessKind::Comb;
    c.writes = {{2, 0, 0, 4, 4}};
    c.loc = at(7);
    c.blocks.resize(1);
    c.blocks[0].instrs = {
        make(Op::Store, voidType(), kNone, {Operand::slot(2, 4, 4), Operand::constant(2)}, 7),
        make(Op::Ret, voidType(), kNone, {}, 7),
    };
    d.constants.push_back({u4(4), {0}, {0}, {}});
    d.processes.push_back(std::move(c));
    EXPECT_TRUE(mentions(verify(d), "also writes"));
    // Disjoint bits are fine.
    d.processes[0].writes = {{2, 0, 0, 0, 4}};
    d.processes[0].blocks[1].instrs[0] =
        make(Op::Nba, voidType(), kNone, {Operand::slot(2, 0, 4), Operand::constant(2)}, 4);
    d.processes[0].blocks[2].instrs[2] =
        make(Op::Nba, voidType(), kNone, {Operand::slot(2, 0, 4), Operand::constant(2)}, 5);
    expectClean(d);
}

TEST(Sir, TwoContinuousAssignmentsToAVariable) {
    Design d = counter();
    d.processes.clear();
    for (int i = 0; i < 2; ++i) {
        Process c;
        c.name = "assign" + std::to_string(i);
        c.kind = ProcessKind::Cont;
        c.writes = {{2, 0, 0, 0, 0}};
        c.loc = at(8);
        c.blocks.resize(1);
        c.blocks[0].instrs = {
            make(Op::Store, voidType(), kNone, {Operand::slot(2), Operand::constant(1)}, 8),
            make(Op::Ret, voidType(), kNone, {}, 8),
        };
        d.processes.push_back(std::move(c));
    }
    EXPECT_TRUE(mentions(verify(d), "also writes"));
    d.slots[2].kind = SlotKind::Wire;  // nets may have several drivers
    expectClean(d);
}

/// mem is logic [7:0] mem [0:3]; the process reads mem[i] and writes it back.
Design memory() {
    Design d = counter();
    d.processes.clear();
    d.slots.push_back({"top.mem", u4(8), SlotKind::Var, {{0, 3}}, 0, 0, kNone, kNone, 0});
    d.constants.push_back(constS2(32, 2));  // #2
    Process p;
    p.name = "rw";
    p.kind = ProcessKind::Initial;
    p.writes = {{3, 0, 0, 0, 0}};
    p.loc = at(9);
    p.numTemps = 1;
    p.blocks.resize(1);
    p.blocks[0].instrs = {
        make(Op::LoadElem, u4(8), 0, {Operand::slot(3), Operand::constant(2)}, 9),
        make(Op::StoreElem, voidType(), kNone,
             {Operand::slot(3), Operand::constant(2), Operand::temp(0)}, 9),
        make(Op::Ret, voidType(), kNone, {}, 9),
    };
    d.processes.push_back(std::move(p));
    return d;
}

TEST(Sir, ArrayElementsVerify) {
    expectClean(memory());
    EXPECT_NE(toText(memory()).find("slot %s3 \"top.mem\" : i4<8> [0:3] var inst=0\n"),
              std::string::npos);
}

TEST(Sir, OneIndexPerDimension) {
    Design d = memory();
    d.processes[0].blocks[0].instrs[0] = make(
        Op::LoadElem, u4(8), 0, {Operand::slot(3), Operand::constant(2), Operand::constant(2)}, 9);
    EXPECT_TRUE(mentions(verify(d), "one index per dimension"));
}

TEST(Sir, WholeArrayIsNotAValue) {
    Design d = memory();
    d.processes[0].blocks[0].instrs[0] = make(Op::Load, u4(8), 0, {Operand::slot(3)}, 9);
    EXPECT_TRUE(mentions(verify(d), "result type differs"));
    // One element with a constant index is a value.
    Operand one = Operand::slot(3);
    one.firstElem = 2;
    one.numElems = 1;
    d.processes[0].blocks[0].instrs[0] = make(Op::Load, u4(8), 0, {one}, 9);
    expectClean(d);
    EXPECT_NE(toText(d).find("load i4<8> %s3{2}"), std::string::npos);
}

TEST(Sir, ElementRangeOutOfBounds) {
    Design d = memory();
    Operand bad = Operand::slot(3);
    bad.firstElem = 3;
    bad.numElems = 2;
    d.processes[0].blocks[0].instrs[0] = make(Op::Load, u4(8), 0, {bad}, 9);
    EXPECT_TRUE(mentions(verify(d), "slot range is invalid"));
}

/// A function f(input logic [7:0] a) returning a + 1, and a task that may
/// wait; the process calls both.
Design calls() {
    Design d = counter();
    d.processes.clear();
    Subroutine f;
    f.name = "top.f";
    f.kind = SubroutineKind::Function;
    f.automatic = true;
    f.returnType = u4(8);
    f.frame = {{"a", u4(8), kNone}};
    f.params = {{"a", Direction::Input, u4(8), kNone, 0}};
    f.loc = at(10);
    f.numTemps = 2;
    f.blocks.resize(1);
    f.blocks[0].instrs = {
        make(Op::Fload, u4(8), 0, {Operand::frame(0)}, 10),
        make(Op::Add, u4(8), 1, {Operand::temp(0), Operand::constant(1)}, 10),
        make(Op::Ret, voidType(), kNone, {Operand::temp(1)}, 10),
    };
    Subroutine t;
    t.name = "top.t";
    t.kind = SubroutineKind::Task;
    t.automatic = true;
    t.mayWait = true;
    t.returnType = voidType();
    t.loc = at(11);
    t.blocks.resize(2);
    d.constants.push_back({u2(64), {1}, {}, {}});  // #2
    t.blocks[0].instrs = {
        make(Op::WaitDelay, voidType(), kNone, {Operand::constant(2), Operand::block(1)}, 11),
    };
    t.blocks[1].instrs = {make(Op::Ret, voidType(), kNone, {}, 11)};
    d.subroutines = {std::move(f), std::move(t)};
    Process p;
    p.name = "caller";
    p.kind = ProcessKind::Initial;
    p.writes = {{2, 0, 0, 0, 0}};
    p.loc = at(12);
    p.numTemps = 1;
    p.blocks.resize(2);
    p.blocks[0].instrs = {
        make(Op::Call, u4(8), 0, {Operand::subroutine(0), Operand::constant(0)}, 12),
        make(Op::Store, voidType(), kNone, {Operand::slot(2), Operand::temp(0)}, 12),
        make(Op::CallTask, voidType(), kNone, {Operand::subroutine(1), Operand::block(1)}, 12),
    };
    p.blocks[1].instrs = {make(Op::Ret, voidType(), kNone, {}, 12)};
    d.processes.push_back(std::move(p));
    return d;
}

TEST(Sir, CallsVerify) {
    expectClean(calls());
    const std::string text = toText(calls());
    EXPECT_NE(text.find("subroutine @0 \"top.f\" function automatic inst=0 ret=i4<8> {"),
              std::string::npos)
        << text;
    EXPECT_NE(text.find("  param input \"a\" : i4<8> -> %f0\n"), std::string::npos) << text;
    EXPECT_NE(text.find("  call.task @1, bb1"), std::string::npos) << text;
}

TEST(Sir, CallArgumentType) {
    Design d = calls();
    d.processes[0].blocks[0].instrs[0] =
        make(Op::Call, u4(8), 0, {Operand::subroutine(0), Operand::constant(2)}, 12);
    EXPECT_TRUE(mentions(verify(d), "argument 'a' has the wrong type"));
    d.processes[0].blocks[0].instrs[0] =
        make(Op::Call, u4(8), 0, {Operand::subroutine(0), Operand::slot(2)}, 12);
    EXPECT_TRUE(mentions(verify(d), "argument 'a' must be a value"));
}

TEST(Sir, WaitingTaskNeedsCallTask) {
    Design d = calls();
    d.processes[0].blocks[0].instrs[2] =
        make(Op::Call, voidType(), kNone, {Operand::subroutine(1)}, 12);
    d.processes[0].blocks[0].instrs.push_back(
        make(Op::Br, voidType(), kNone, {Operand::block(1)}, 12));
    EXPECT_TRUE(mentions(verify(d), "called with call.task"));
}

TEST(Sir, CallTaskInAlwaysComb) {
    Design d = calls();
    d.processes[0].kind = ProcessKind::Comb;
    EXPECT_TRUE(mentions(verify(d), "call.task in a comb process"));
}

TEST(Sir, WaitInAFunction) {
    Design d = calls();
    d.subroutines[1].kind = SubroutineKind::Function;
    d.subroutines[1].mayWait = false;
    d.processes[0].blocks[0].instrs[2] = make(Op::Br, voidType(), kNone, {Operand::block(1)}, 12);
    EXPECT_TRUE(mentions(verify(d), "wait.delay in a function"));
}

TEST(Sir, ReturnValueType) {
    Design d = calls();
    d.subroutines[0].blocks[0].instrs[2] =
        make(Op::Ret, voidType(), kNone, {Operand::constant(2)}, 10);
    EXPECT_TRUE(mentions(verify(d), "needs a value of the return type"));
}

TEST(Sir, IffGuardBelongsToTheWaiter) {
    // @(posedge clk iff en): the guard reads the waiting process's frame.
    Design d = calls();
    Subroutine g;
    g.name = "caller.iff0";
    g.kind = SubroutineKind::Guard;
    g.returnType = u4(1);
    g.frameOf = {Owner::Kind::Process, 0};
    g.loc = at(13);
    g.numTemps = 1;
    g.blocks.resize(1);
    g.blocks[0].instrs = {
        make(Op::Load, u4(1), 0, {Operand::slot(1)}, 13),
        make(Op::Ret, voidType(), kNone, {Operand::temp(0)}, 13),
    };
    d.subroutines.push_back(std::move(g));
    d.processes[0].blocks[0].instrs[2] =
        make(Op::WaitEvent, voidType(), kNone,
             {Operand::event(0, Edge::Pos), Operand::subroutine(2), Operand::block(1)}, 12);
    expectClean(d);
    EXPECT_NE(toText(d).find("wait.event posedge %s0 iff @2, bb1"), std::string::npos);
    d.subroutines[2].frameOf = {Owner::Kind::Subroutine, 1};
    EXPECT_TRUE(mentions(verify(d), "reads another frame"));
}

TEST(Sir, ForkChildrenBelongToTheParent) {
    Design d = calls();
    Process child;
    child.name = "caller.fork0";
    child.kind = ProcessKind::General;
    child.parent = {Owner::Kind::Process, 0};
    child.loc = at(14);
    child.blocks.resize(1);
    child.blocks[0].instrs = {make(Op::Ret, voidType(), kNone, {}, 14)};
    d.processes.push_back(std::move(child));
    d.processes[0].blocks[0].instrs[2] =
        make(Op::Fork, voidType(), kNone,
             {Operand::process(1), Operand::str("join"), Operand::block(1)}, 12);
    expectClean(d);
    d.processes[1].parent = {Owner::Kind::Process, 1};
    EXPECT_TRUE(mentions(verify(d), "has another parent"));
}

TEST(Sir, ForkJoinNoneIsNotAWait) {
    Design d = calls();
    Process child;
    child.name = "f.fork0";
    child.kind = ProcessKind::General;
    child.parent = {Owner::Kind::Subroutine, 0};
    child.loc = at(14);
    child.blocks.resize(1);
    child.blocks[0].instrs = {make(Op::Ret, voidType(), kNone, {}, 14)};
    d.processes.push_back(std::move(child));
    // A function may fork join_none (13.4.4), but not join.
    auto& fb = d.subroutines[0].blocks;
    fb.resize(2);
    fb[1].instrs = {fb[0].instrs[2]};
    fb[0].instrs[2] = make(Op::Fork, voidType(), kNone,
                           {Operand::process(1), Operand::str("join_none"), Operand::block(1)}, 10);
    expectClean(d);
    fb[0].instrs[2].operands[1].text = "join";
    EXPECT_TRUE(mentions(verify(d), "fork in a function"));
}

TEST(Sir, MonitorOfAnAutomaticVariable) {
    Design d = loopWithWait();
    d.processes[0].blocks[3].instrs.insert(
        d.processes[0].blocks[3].instrs.begin(),
        make(Op::Syscall, voidType(), kNone, {Operand::str("$monitor"), Operand::frame(0)}, 1));
    EXPECT_TRUE(mentions(verify(d), "may not refer to an automatic variable"));
}

TEST(Sir, EnumItemsHaveTheBaseType) {
    Design d = counter();
    d.constants.push_back({u2(2), {0}, {}, {}});  // #2
    d.constants.push_back({u2(2), {1}, {}, {}});  // #3
    TypeEntry e;
    e.kind = TypeEntryKind::Enum;
    e.name = "state_t";
    e.value = u2(2);
    e.items = {{"IDLE", 2}, {"RUN", 3}};
    d.types.push_back(e);
    expectClean(d);
    EXPECT_NE(toText(d).find("type !0 enum \"state_t\" : i2<2> { \"IDLE\" = 2'h0, \"RUN\" = "
                             "2'h1 }"),
              std::string::npos);
    d.types[0].items[1].value = 0;  // an i4<8> constant
    EXPECT_TRUE(mentions(verify(d), "does not have the base type"));
}

TEST(Sir, PackedFieldsInsideTheVector) {
    Design d = counter();
    TypeEntry t;
    t.kind = TypeEntryKind::PackedStruct;
    t.name = "pair_t";
    t.value = u4(8);
    t.fields = {{"hi", 4, 4, kNone}, {"lo", 0, 4, kNone}};
    d.types.push_back(t);
    expectClean(d);
    d.types[0].fields[0].width = 5;
    EXPECT_TRUE(mentions(verify(d), "outside the vector"));
}

TEST(Sir, ConcatWidth) {
    Design d = counter();
    d.processes[0].numTemps = 6;
    auto& instrs = d.processes[0].blocks[2].instrs;
    // {q, q} is 16 bits wide, not 8.
    instrs.insert(instrs.begin() + 1,
                  make(Op::Concat, u4(16), 5, {Operand::temp(3), Operand::temp(3)}, 5));
    expectClean(d);
    instrs[1].type = u4(8);
    EXPECT_TRUE(mentions(verify(d), "as wide as the operands together"));
}

TEST(Sir, RealAndStringConstants) {
    EXPECT_EQ(constantText({{TypeKind::Real, 0}, {0x3ff8000000000000ULL}, {}, {}}), "1.5");
    EXPECT_EQ(constantText({{TypeKind::ShortReal, 0}, {0x3e800000ULL}, {}, {}}), "0.25");
    EXPECT_EQ(constantText({{TypeKind::String, 0}, {}, {}, "a\"b\x01"}), "\"a\\\"b\\x01\"");
    Design d = counter();
    d.constants.push_back({{TypeKind::Real, 0}, {0, 1}, {}, {}});
    EXPECT_TRUE(mentions(verify(d), "exactly one word"));
}

}  // namespace
