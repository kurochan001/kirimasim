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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/kernel/context.h"
#include "kirimasim/sir/design.h"
#include "kirimasim/sir/verify.h"
#include "kirimasim/vm/vm.h"

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
Value slotValue(const kirimasim::kernel::Context& c, SlotId s, Type t) {
    const std::uint64_t* p = c.store().element(kirimasim::kernel::SlotId{s.index()}, 0);
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

TEST(VmBlockArgument, EveryChunkPassesAndArgumentsAreReadFirst) {
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
                    const auto r = kirimasim::vm::run(d, c);
                    ASSERT_EQ(r.reason, kirimasim::kernel::EndReason::Quiescent) << r.error;
                    const Value x = slotValue(c, SlotId{0}, t), y = slotValue(c, SlotId{1}, t);
                    EXPECT_EQ(x.a, b.a);
                    EXPECT_EQ(x.b, b.b);
                    EXPECT_EQ(y.a, a.a);
                    EXPECT_EQ(y.b, a.b);
                    ++cases;
                }
            }
        }
    }
    EXPECT_EQ(cases, 5 * (2 * 3 + 2) * 2);
}

}  // namespace
