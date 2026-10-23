// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The negative matrix of the SIR verifier (SIR_SPEC.md 10): every check
// that rejects a malformed design, each reached by one small change to a
// design that verifies. Without it, a check that never fires would go
// unnoticed: front could hand a broken design to the tiers.

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "kirimasim/sir/design.h"
#include "kirimasim/sir/text.h"
#include "kirimasim/sir/verify.h"

namespace {

using namespace kirimasim::sir;

SourceLoc at(std::uint32_t line) { return {0, line, 1}; }

Instr make(Op op, Type t, std::uint32_t r, std::vector<Operand> ops) {
    return {op, t, TempId{r}, std::move(ops), at(2)};
}
Instr none(Op op, std::vector<Operand> ops) { return make(op, voidType(), kNone, std::move(ops)); }

const Type kReal{TypeKind::Real, 0};
const Type kShortReal{TypeKind::ShortReal, 0};
const Type kString{TypeKind::String, 0};
const Type kEvent{TypeKind::Event, 0};
const Type kChandle{TypeKind::Chandle, 0};

// Slots of the playground.
enum : std::uint32_t { sA, sB, sR, sS, sE, sM, sBit, sW, sF };
// Temps that block 0 loads from them, and the first free one.
enum : std::uint32_t { tA, tB, tR, tS, tBit, tFree };
// Constants.
enum : std::uint32_t { c5, c2, c6, cReal, cString, cTrue };
// Frame variables of the playground process.
enum : std::uint32_t { fV, fRef, fRefArr, fRefReal };

Slot slot(std::string name, Type t, std::vector<ArrayDim> dims = {}, SlotKind kind = SlotKind::Var,
          std::uint32_t flags = 0) {
    Slot s;
    s.name = std::move(name);
    s.type = t;
    s.kind = kind;
    s.dims = std::move(dims);
    s.flags = flags;
    return s;
}

/// A design that verifies: slots of every domain, a function f(i4<8> x)
/// returning void, a task t() that may wait, a guard g, a two-input UDP and
/// an initial process whose block 0 loads the slots into temps and ends
/// with ret. A case adds its instruction before that ret.
Design playground() {
    Design d;
    d.sources = {"play.sv"};
    d.instances = {{"top", "top", kNone}};
    d.slots = {slot("top.a", u4(8)),
               slot("top.b", u2(8)),
               slot("top.r", kReal),
               slot("top.s", kString),
               slot("top.e", kEvent),
               slot("top.m", u4(8), {{0, 3}}),
               slot("top.bit", u4(1)),
               slot("top.w", u4(8), {}, SlotKind::Wire),
               slot("top.f", u4(8), {}, SlotKind::Var, kForceTarget)};
    for (std::uint32_t s = 0; s < d.slots.size(); ++s)
        d.names.push_back({d.slots[s].name, SlotId{s}});
    d.constants = {{u4(8), {5}, {0}},      {u2(32), {2}, {}},
                   {u2(32), {6}, {}},      {kReal, {std::bit_cast<std::uint64_t>(1.5)}, {}},
                   {kString, {}, {}, "x"}, {u2(1), {1}, {}}};
    TypeEntry udp;
    udp.kind = TypeEntryKind::Udp;
    udp.name = "and2";
    udp.inputs = 2;
    udp.rows = {{"11", 0, '1'}, {"0?", 0, '0'}, {"?0", 0, '0'}};
    d.types.push_back(udp);

    Subroutine f;
    f.name = "f";
    f.kind = SubroutineKind::Function;
    f.automatic = true;
    f.returnType = voidType();
    f.frame = {{"x", u4(8), kNone, {}, {}}};
    f.params = {{"x", Direction::Input, u4(8), kNone, FrameVarId{0}}};
    f.blocks = {{{}, {none(Op::Ret, {})}}};
    f.loc = at(1);
    d.subroutines.push_back(f);
    Subroutine t;
    t.name = "t";
    t.kind = SubroutineKind::Task;
    t.automatic = true;
    t.mayWait = true;
    t.returnType = voidType();
    t.blocks = {{{}, {none(Op::Ret, {})}}};
    t.loc = at(1);
    d.subroutines.push_back(t);
    Subroutine g;
    g.name = "g";
    g.kind = SubroutineKind::Guard;
    g.automatic = true;
    g.returnType = u2(1);
    g.frameOf = {Owner::Kind::Process, 0};
    g.blocks = {{{}, {make(Op::Ret, voidType(), kNone, {Operand::constant(ConstId{cTrue})})}}};
    g.loc = at(1);
    d.subroutines.push_back(g);

    Process p;
    p.name = "play";
    p.kind = ProcessKind::Initial;
    p.loc = at(1);
    p.numTemps = 40;
    p.writes = {{SlotId{sA}, 0, 0, 0, 0}, {SlotId{sB}, 0, 0, 0, 0}, {SlotId{sR}, 0, 0, 0, 0},
                {SlotId{sS}, 0, 0, 0, 0}, {SlotId{sM}, 0, 0, 0, 0}, {SlotId{sBit}, 0, 0, 0, 0}};
    p.frame = {{"v", u4(8), kNone, {}, {}},
               {"ref", refType(), kNone, {}, u4(8)},
               {"refarr", refType(), kNone, {{0, 3}}, u4(8)},
               {"refreal", refType(), kNone, {}, kReal}};
    p.blocks = {{{},
                 {make(Op::Load, u4(8), tA, {Operand::slot(SlotId{sA})}),
                  make(Op::Load, u2(8), tB, {Operand::slot(SlotId{sB})}),
                  make(Op::Load, kReal, tR, {Operand::slot(SlotId{sR})}),
                  make(Op::Load, kString, tS, {Operand::slot(SlotId{sS})}),
                  make(Op::Load, u4(1), tBit, {Operand::slot(SlotId{sBit})}), none(Op::Ret, {})}}};
    d.processes.push_back(p);
    return d;
}

/// What a case changes in the playground: an instruction added before the
/// ret of block 0, or an edit. (Edits capture nothing, so that copying a
/// case copies no instruction inside a closure.)
struct Change {
    Change(Instr in) : instr(std::move(in)) {}
    template <typename F>
        requires std::is_convertible_v<F, void (*)(Design&)>
    Change(F f) : edit(f) {}

    void apply(Design& d) const {
        if (edit != nullptr) edit(d);
        if (instr) {
            auto& instrs = d.processes[0].blocks[0].instrs;
            instrs.insert(instrs.end() - 1, *instr);
        }
    }

    std::optional<Instr> instr;
    void (*edit)(Design&) = nullptr;
};

Change add(Instr in) { return {std::move(in)}; }

struct Case {
    const char* name;
    const char* expect;  ///< Part of the message the verifier must give.
    Change change;
};

constexpr std::uint32_t N = tFree;

std::vector<Case> cases() {
    const Operand A = Operand::temp(TempId{tA}), B = Operand::temp(TempId{tB}),
                  R = Operand::temp(TempId{tR}), S = Operand::temp(TempId{tS}),
                  Bit = Operand::temp(TempId{tBit});
    const Operand C5 = Operand::constant(ConstId{c5}), C2 = Operand::constant(ConstId{c2}),
                  C6 = Operand::constant(ConstId{c6});
    return {
        // Instructions: the operands each opcode takes (SIR_SPEC.md 5, 7).
        {"source file", "source file out of range",
         [](Design& d) {
             Instr in = none(Op::Store, {Operand::slot(SlotId{sA}), Operand::temp(TempId{tA})});
             in.loc.file = 9;
             add(in).apply(d);
         }},
        {"result without type", "result and result type must come together",
         add(make(Op::Add, voidType(), N, {A, A}))},
        {"result of width 0", "the result has width 0", add(make(Op::Add, u2(0), N, {B, B}))},
        {"frame variable of width 0", "integral frame variable has width 0",
         [](Design& d) { d.processes[0].frame.push_back({"z", u2(0), kNone, {}, {}}); }},
        // A delay is a count of ticks, an integral value.
        {"trigger.nb of a real", "optional integral delay",
         add(none(Op::TriggerNb, {Operand::slot(SlotId{sE}), R}))},
        {"wait.delay of a real", "needs an integral delay",
         [](Design& d) {
             auto& instrs = d.processes[0].blocks[0].instrs;
             instrs.back() =
                 none(Op::WaitDelay, {Operand::temp(TempId{tR}), Operand::block(BlockId{1})});
             d.processes[0].blocks.push_back({{}, {none(Op::Ret, {})}});
         }},
        {"no result", "needs a result", add(make(Op::Add, voidType(), kNone, {A, A}))},
        {"br without block", "needs a block", add(none(Op::Br, {A}))},
        {"store", "bad operands", add(none(Op::Store, {A, A}))},
        {"nba.delay operands", "bad operands", add(none(Op::NbaDelay, {A, A, C2}))},
        {"nba.delay value", "does not fit",
         add(none(Op::NbaDelay, {Operand::slot(SlotId{sA}), R, C2}))},
        {"nba.delay delay", "the delay must be integral",
         add(none(Op::NbaDelay, {Operand::slot(SlotId{sA}), A, R}))},
        {"nba.elem.delay missing delay", "needs one index per dimension",
         add(none(Op::NbaElemDelay, {Operand::slot(SlotId{sM}), C2, A}))},
        {"nba.elem.delay delay", "the delay must be integral",
         add(none(Op::NbaElemDelay, {Operand::slot(SlotId{sM}), C2, A, R}))},
        {"nba.elem.delay value", "value does not fit the element type",
         add(none(Op::NbaElemDelay, {Operand::slot(SlotId{sM}), C2, R, C2}))},
        {"nba.elem.delay index", "indices must be integral",
         add(none(Op::NbaElemDelay, {Operand::slot(SlotId{sM}), R, A, C2}))},
        {"nba.elem.delay frame", "needs a whole unpacked array slot or frame variable",
         add(none(Op::NbaElemDelay, {Operand::frame(FrameVarId{fV}), C2, A, C2}))},
        {"nba.bits.delay missing operands", "needs one index per dimension",
         add(none(Op::NbaBitsDelay, {Operand::slot(SlotId{sA})}))},
        {"nba.bits.delay delay", "the delay must be integral",
         add(none(Op::NbaBitsDelay, {Operand::slot(SlotId{sA}), C2, A, R}))},
        {"nba.bits.delay value", "value must be integral",
         add(none(Op::NbaBitsDelay, {Operand::slot(SlotId{sA}), C2, R, C2}))},
        {"nba.bits.delay position", "indices and the position must be integral",
         add(none(Op::NbaBitsDelay, {Operand::slot(SlotId{sA}), R, A, C2}))},
        {"nba.bits.delay frame", "needs a whole integral slot or frame variable",
         add(none(Op::NbaBitsDelay, {Operand::frame(FrameVarId{fV}), C2, A, C2}))},
        {"drive.delay outside cont", "only a cont process writes with it",
         add(none(Op::DriveDelay, {Operand::slot(SlotId{sW}), A, C2, C2, C2}))},
        {"load.elem of a scalar", "needs a whole unpacked array slot or frame variable",
         add(make(Op::LoadElem, u4(8), N, {Operand::slot(SlotId{sA}), C2}))},
        {"store.elem value", "value does not fit the element type",
         add(none(Op::StoreElem, {Operand::slot(SlotId{sM}), C2, R}))},
        {"store.bits of a real", "needs a whole integral slot or frame variable",
         add(none(Op::StoreBits, {Operand::slot(SlotId{sR}), C2, A}))},
        {"store.bits without value", "needs one index per dimension, a position and a value",
         add(none(Op::StoreBits, {Operand::slot(SlotId{sA}), C2}))},
        {"store.bits domain", "value must be integral, in the slot's domain and not wider",
         add(none(Op::StoreBits, {Operand::slot(SlotId{sB}), C2, A}))},
        {"fload type", "needs a frame variable of the result type that is not an array",
         add(make(Op::Fload, u2(8), N, {Operand::frame(FrameVarId{fV})}))},
        {"ref.make result", "needs a slot or frame variable and a ref result",
         add(make(Op::RefMake, u4(8), N, {Operand::slot(SlotId{sA})}))},
        {"ref.make of a part", "needs a whole slot or frame variable",
         add(make(Op::RefMake, refType(), N, {Operand::slot(SlotId{sA}, 0, 4)}))},
        {"ref.make indices", "needs no index or one per dimension",
         add(make(Op::RefMake, refType(), N, {Operand::slot(SlotId{sM}), C2, C2}))},
        {"load.ref.elem of a non-ref", "needs a ref argument",
         add(make(Op::LoadRefElem, u4(8), N, {Operand::frame(FrameVarId{fV}), C2}))},
        {"load.ref.elem indices", "needs one index per dimension of what it refers to",
         add(make(Op::LoadRefElem, u4(8), N, {Operand::frame(FrameVarId{fRefArr})}))},
        {"load.ref.elem real index", "indices and the position must be integral",
         add(make(Op::LoadRefElem, u4(8), N, {Operand::frame(FrameVarId{fRefArr}), R}))},
        {"load.ref.elem result", "result must have the type of what the ref refers to",
         add(make(Op::LoadRefElem, u2(8), N, {Operand::frame(FrameVarId{fRefArr}), C2}))},
        {"store.ref.bits of a real", "needs a ref to an integral value",
         add(none(Op::StoreRefBits, {Operand::frame(FrameVarId{fRefReal}), C2, A}))},
        {"pow", "base and result must have the same integral type",
         add(make(Op::Pow, u4(8), N, {A, R}))},
        {"and", "operands and result must have the same integral type",
         add(make(Op::And, u4(8), N, {A, B}))},
        {"neg", "operand and result must have the same integral type",
         add(make(Op::Neg, u4(8), N, {B}))},
        {"not", "operand and result must have the same integral type",
         add(make(Op::Not, u4(8), N, {B}))},
        {"eq types", "operands must have one integral, real, string or chandle type",
         add(make(Op::Eq, u4(1), N, {A, B}))},
        {"eq result", "result must be 1 bit in the operands' domain",
         add(make(Op::Eq, u4(8), N, {A, A}))},
        {"weq types", "operands must have one type", add(make(Op::Weq, u4(1), N, {A, B}))},
        {"weq result", "result must be 1 bit in the operands' domain",
         add(make(Op::Weq, u2(1), N, {A, A}))},
        {"match.mask types", "operands must have one type",
         add(make(Op::MatchMask, u2(1), N, {A, B, Operand::str("casez")}))},
        {"ceq types", "operands must have one type", add(make(Op::Ceq, u2(1), N, {A, B}))},
        {"ceq result", "result must be i2<1>", add(make(Op::Ceq, u4(1), N, {A, A}))},
        {"truth of a string", "needs an integral, real or chandle operand and a 1-bit result",
         add(make(Op::Truth, u4(1), N, {S}))},
        {"ror of a real", "needs an integral operand and a 1-bit result in its domain",
         add(make(Op::Ror, u4(1), N, {R}))},
        {"land of strings", "needs integral or real operands and a 1-bit result",
         add(make(Op::Land, u4(1), N, {S, S}))},
        {"shl", "value must have the result type; the amount must be integral",
         add(make(Op::Shl, u4(8), N, {B, C2}))},
        {"resolve", "needs a net kind", add(make(Op::Resolve, u4(8), N, {}))},
        {"select", "needs a 1-bit condition and two values of the result type",
         add(make(Op::Select, u4(8), N, {A, A, A}))},
        {"bitsel", "needs an integral value and index and an unsigned 1-bit result",
         add(make(Op::Bitsel, u4(8), N, {A, C2}))},
        {"partsel position", "needs an integral value, a constant position and a narrower",
         add(make(Op::Partsel, u4(4), N, {A, A}))},
        {"partsel outside", "part is outside the value", add(make(Op::Partsel, u4(4), N, {A, C6}))},
        {"idx.partsel", "needs an integral value and position and an unsigned result",
         add(make(Op::IdxPartsel, u4(4), N, {A, R}))},
        {"concat", "needs at least one operand", add(make(Op::Concat, u4(8), N, {}))},
        {"repl of a string", "needs an integral count and a string",
         add(make(Op::Repl, kString, N, {R, S}))},
        {"repl count", "needs a constant count and an integral value",
         add(make(Op::Repl, u4(16), N, {A, A}))},
        {"repl width", "result width must be count times the value's width",
         add(make(Op::Repl, u4(10), N, {C2, A}))},
        {"zext", "must widen within one domain", add(make(Op::Zext, u4(4), N, {A}))},
        {"trunc", "must narrow within one domain", add(make(Op::Trunc, u4(16), N, {A}))},
        {"to2", "must convert a four-state value to the same two-state type",
         add(make(Op::To2, u2(4), N, {A}))},
        {"to4", "must convert a two-state value to the same four-state type",
         add(make(Op::To4, u4(8), N, {A}))},
        {"itor", "must convert an integral value to a real", add(make(Op::Itor, kReal, N, {S}))},
        {"rtoi", "must convert a real to an integral value", add(make(Op::Rtoi, u4(8), N, {A}))},
        {"rconv", "must convert between real and shortreal",
         add(make(Op::Rconv, kShortReal, N, {A}))},
        {"itos", "must convert an integral value to a string",
         add(make(Op::Itos, kString, N, {R}))},
        {"stoi", "must convert a string to an integral value", add(make(Op::Stoi, u4(8), N, {A}))},
        {"str.get", "needs a string and an index, and gives a two-state byte",
         add(make(Op::StrGet, u4(8), N, {S, C2}))},
        {"trigger", "needs an event slot", add(none(Op::Trigger, {Operand::slot(SlotId{sA})}))},
        {"trigger.nb", "needs an event slot and an optional integral delay",
         add(none(Op::TriggerNb, {Operand::slot(SlotId{sE}), R}))},
        {"disable", "needs a process, a subroutine or a block name", add(none(Op::Disable, {A}))},
        {"scope.enter", "needs a scope and its exit block", add(none(Op::ScopeEnter, {A, A}))},
        {"scope.leave", "needs a scope", add(none(Op::ScopeLeave, {A}))},
        {"disable.task", "needs a task",
         add(none(Op::DisableTask, {Operand::subroutine(SubroutineId{0})}))},
        {"strobe", "needs a postponed process", add(none(Op::Strobe, {A}))},
        {"udp kind", "needs a sequential UDP table",
         add(make(Op::UdpSeq, u4(1), N, {Operand::typeRow(TypeId{0}), Bit, Bit, Bit}))},
        {"drive.delay operands", "needs an integral slot range, a value of its type",
         add(none(Op::DriveDelay, {A, A, C2, C2, C2}))},
        {"block arguments", "block arguments are missing",
         [](Design& d) {
             Operand b = Operand::block(BlockId{0});
             b.count = 2;
             add(none(Op::Br, {b})).apply(d);
         }},
        {"wait without a block", "needs a block", add(none(Op::WaitFork, {}))},
        {"ref argument", "argument 'x' must be a ref",
         [](Design& d) {
             d.subroutines[0].params[0].dir = Direction::Ref;
             add(none(Op::Call, {Operand::subroutine(SubroutineId{0}), Operand::temp(TempId{tA})}))
                 .apply(d);
         }},
        {"array argument", "argument 'x' must be an array of its shape",
         [](Design& d) {
             d.subroutines[0].frame[0].dims = {{0, 3}};
             add(none(Op::Call, {Operand::subroutine(SubroutineId{0}), Operand::temp(TempId{tA})}))
                 .apply(d);
         }},
        {"udp inputs", "needs one value per input",
         add(make(Op::Udp, u4(1), N, {Operand::typeRow(TypeId{0}), Bit}))},
        {"override.start", "needs \"assign\" or \"force\"",
         add(none(Op::OverrideStart, {Operand::str("hold")}))},
        {"store.override outside an override process", "only an override process writes with it",
         add(none(Op::StoreOverride, {Operand::str("force"), Operand::slot(SlotId{sF}), A}))},
        {"report.call kind", "needs \"#0\" or \"final\" and a function",
         add(none(Op::ReportCall, {Operand::str("now"), Operand::subroutine(SubroutineId{0}), A}))},
        {"report.call arguments", "needs a function and one value per argument",
         add(none(Op::ReportCall, {Operand::str("#0"), Operand::subroutine(SubroutineId{0})}))},
        {"report.call argument type", "needs a value of its input's type",
         add(none(Op::ReportCall, {Operand::str("#0"), Operand::subroutine(SubroutineId{0}), B}))},
        {"call", "first operand must be a subroutine", add(none(Op::Call, {A}))},
        {"call arguments", "expects 1 arguments",
         add(none(Op::Call, {Operand::subroutine(SubroutineId{0})}))},
        {"syscall", "first operand must be the system task name", add(none(Op::Syscall, {A}))},
        {"builtin", "first operand must be the method name",
         add(make(Op::Builtin, u2(32), N, {A}))},
        {"wait.event", "needs at least one event and a block", add(none(Op::WaitEvent, {}))},
        {"wait.event events",
         "events must be slot ranges or watches, each with an optional iff guard",
         add(none(Op::WaitEvent, {A, Operand::block(BlockId{0})}))},
        {"wait.cond", "needs a guard", add(none(Op::WaitCond, {A, Operand::block(BlockId{0})}))},
        {"wait.cond of a function", "is not a guard",
         add(none(Op::WaitCond,
                  {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{0})}))},
        {"wait.named", "needs an event slot",
         add(none(Op::WaitNamed, {Operand::slot(SlotId{sA}), Operand::block(BlockId{0})}))},
        {"fork", "needs processes, a join kind and a block", add(none(Op::Fork, {}))},
        {"fork join kind", "join kind must be join, join_any or join_none",
         add(none(Op::Fork, {Operand::process(ProcessId{0}), Operand::str("join_all"),
                             Operand::block(BlockId{0})}))},
        {"fork children", "children must be processes",
         add(none(Op::Fork, {A, Operand::str("join"), Operand::block(BlockId{0})}))},
        {"call.task", "needs a task and a block", add(none(Op::CallTask, {}))},
        {"call.task of a function", "calls only tasks that may wait",
         add(none(Op::CallTask,
                  {Operand::subroutine(SubroutineId{0}), Operand::block(BlockId{0})}))},

        // Tables, subroutines and processes (SIR_SPEC.md 2 to 4, 7, 8).
        {"instance order", "parent must come before the child",
         [](Design& d) { d.instances.push_back({"top.c", "c", InstanceId{2}}); }},
        {"enum names", "enum item name is not unique",
         [](Design& d) {
             TypeEntry e;
             e.kind = TypeEntryKind::Enum;
             e.name = "e";
             e.value = u2(32);
             e.items = {{"A", ConstId{c2}}, {"A", ConstId{c6}}};
             d.types.push_back(e);
         }},
        {"union field", "union field does not start at bit 0",
         [](Design& d) {
             TypeEntry u;
             u.kind = TypeEntryKind::PackedUnion;
             u.name = "u";
             u.value = u4(8);
             u.fields = {{"a", 0, 8, kNone}, {"b", 1, 7, kNone}};
             d.types.push_back(u);
         }},
        {"empty struct", "struct has no members",
         [](Design& d) {
             TypeEntry s;
             s.kind = TypeEntryKind::UnpackedStruct;
             s.name = "s";
             d.types.push_back(s);
         }},
        {"array without dimensions", "array has no dimensions",
         [](Design& d) {
             TypeEntry a;
             a.kind = TypeEntryKind::Array;
             a.name = "a";
             d.types.push_back(a);
         }},
        {"array element row", "element type out of range",
         [](Design& d) {
             TypeEntry a;
             a.kind = TypeEntryKind::Array;
             a.name = "a";
             a.dims = {{0, 1}};
             a.element = TypeId{99};
             d.types.push_back(a);
         }},
        {"udp initial state", "initial state must be 0, 1 or x",
         [](Design& d) {
             d.types[0].sequential = true;
             d.types[0].initial = 'z';
         }},
        {"udp row", "is not a row of this UDP", [](Design& d) { d.types[0].rows[0].inputs = "1"; }},
        {"driver slot", "a driver slot drives a net of its own type",
         [](Design& d) {
             Slot s = slot("top.w.d", u2(8));
             s.driverOf = SlotId{sW};
             d.slots.push_back(s);
         }},
        {"array size", "array is too large",
         [](Design& d) { d.slots[sM].dims = {{0, 5000000000LL}}; }},
        {"string constant words", "string constant has words",
         [](Design& d) { d.constants[cString].aWords = {1}; }},
        {"chandle constant", "a chandle constant is null",
         [](Design& d) { d.constants.push_back({kChandle, {1}, {}}); }},
        {"shortreal constant", "shortreal constant has more than 32 bits",
         [](Design& d) { d.constants.push_back({kShortReal, {std::uint64_t{1} << 40}, {}}); }},
        {"void constant", "constant has no value type",
         [](Design& d) { d.constants.push_back({voidType(), {}, {}}); }},
        {"b words", "wrong number of b words", [](Design& d) { d.constants[c5].bWords = {0, 0}; }},
        {"task value", "a task returns no value",
         [](Design& d) { d.subroutines[1].returnType = u4(8); }},
        {"slot owner", "subroutine out of range",
         [](Design& d) { d.slots[sA].subroutine = SubroutineId{9}; }},
        {"guard result", "a guard returns a 1-bit value",
         [](Design& d) { d.subroutines[2].returnType = u2(8); }},
        {"argument of a static subroutine", "of a static subroutine is in the frame",
         [](Design& d) { d.subroutines[0].automatic = false; }},
        {"argument slot", "argument 'x' slot is invalid",
         [](Design& d) {
             d.subroutines[0].automatic = false;
             d.subroutines[0].params[0].frameVar = kNone;
             d.subroutines[0].params[0].slot = SlotId{sR};
         }},
        {"store.override operands",
         "a slot range marked as a force target and a value of its width",
         [](Design& d) {
             Process o;
             o.name = "hold";
             o.kind = ProcessKind::Override;
             o.loc = at(1);
             o.writes = {{SlotId{sF}, 0, 0, 0, 0}};
             o.blocks = {{{},
                          {none(Op::StoreOverride, {Operand::str("hold"), Operand::slot(SlotId{sF}),
                                                    Operand::constant(ConstId{c5})}),
                           none(Op::Ret, {})}}};
             d.processes.push_back(o);
         }},
        {"guard owner", "a guard needs the process or subroutine whose frame it reads",
         [](Design& d) { d.subroutines[2].frameOf = {}; }},
        {"frame of another", "only guards read another frame",
         [](Design& d) { d.subroutines[0].frameOf = {Owner::Kind::Process, 0}; }},
        {"argument place", "needs exactly one place",
         [](Design& d) { d.subroutines[0].params[0].slot = SlotId{sA}; }},
        {"argument frame variable", "frame variable is invalid",
         [](Design& d) { d.subroutines[0].params[0].frameVar = FrameVarId{7}; }},
        {"postponed process", "a postponed process has no triggers",
         [](Design& d) {
             Process p;
             p.name = "post";
             p.kind = ProcessKind::Postponed;
             p.loc = at(1);
             p.triggers = {{{SlotId{sA}, 0, 0, 0, 0}, Edge::Any}};
             p.blocks = {{{}, {none(Op::Ret, {})}}};
             d.processes.push_back(p);
         }},
        {"forked process kind", "a forked process must be of kind general",
         [](Design& d) {
             Process p;
             p.name = "child";
             p.kind = ProcessKind::Initial;
             p.parent = {Owner::Kind::Process, 0};
             p.loc = at(1);
             p.blocks = {{{}, {none(Op::Ret, {})}}};
             d.processes.push_back(p);
         }},
        {"no blocks", "has no blocks", [](Design& d) { d.subroutines[0].blocks.clear(); }},
        {"branch target", "branch target out of range",
         add(none(Op::Br, {Operand::block(BlockId{9})}))},
        {"temp range", "out of range", add(make(Op::Add, u4(8), 99, {A, A}))},
    };
}

std::string describe(const std::vector<VerifyError>& errors) {
    std::string s;
    for (const auto& e : errors)
        s += "  " + e.where + ": " + e.message + "\n";
    return s;
}

TEST(VerifyMatrix, PlaygroundVerifies) {
    const auto errors = verify(playground());
    EXPECT_TRUE(errors.empty()) << describe(errors);
}

TEST(VerifyMatrix, DelayedNbaTargetsVerifyAndWriteText) {
    for (const Op op : {Op::NbaElemDelay, Op::NbaBitsDelay}) {
        Design d = playground();
        std::vector<Operand> ops{Operand::slot(SlotId{sM}), Operand::constant(ConstId{c2})};
        if (op == Op::NbaBitsDelay) ops.push_back(Operand::constant(ConstId{c2}));
        ops.push_back(Operand::temp(TempId{tA}));
        ops.push_back(Operand::constant(ConstId{c2}));
        add(none(op, std::move(ops))).apply(d);
        EXPECT_TRUE(verify(d).empty()) << describe(verify(d));
        EXPECT_NE(toText(d).find(opName(op)), std::string::npos);
        EXPECT_TRUE(writesSlot(op));
    }
}

TEST(VerifyMatrix, EveryCheckFires) {
    for (const Case& c : cases()) {
        Design d = playground();
        c.change.apply(d);
        const auto errors = verify(d);
        bool found = false;
        for (const auto& e : errors)
            found = found || e.message.find(c.expect) != std::string::npos;
        EXPECT_TRUE(found) << c.name << ": expected \"" << c.expect << "\", got\n"
                           << describe(errors);
    }
}

}  // namespace
