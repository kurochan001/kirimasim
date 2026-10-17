#!/usr/bin/env python3
"""Negative controls for the implementation (TEST_SUITE.md): puts one known
mistake at a time into the sources, rebuilds, runs the tests, and checks
that some test fails. A mistake that no test catches is a hole in the
tests.

  tools/mutants.py [--preset dev] [--only <name>]

A mistake counts as caught when some test fails, or when a test that ran
on the unchanged sources is skipped as unsupported: a mistake can turn a
supported construct into an unsupported one rather than give a wrong
value.

Each mistake is a text replacement that must match exactly once. The source
file is always restored, even when the run is interrupted; the script
refuses to start when src/ has uncommitted changes, so that restoring can
never lose work. At the end it rebuilds the unchanged sources. The exit code
is 1 if a mistake survives or does not apply.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# (name, file, text, replacement). Each one breaks a rule that a spec fixes.
MUTANTS = [
    ("kernel and4: x & 0 is x", "src/kernel/include/kirimasim/kernel/w4.h",
     "(~x.a & ~x.b) | (~y.a & ~y.b);  // either side is a known 0",
     "(~x.a & ~x.b) & (~y.a & ~y.b);  // either side is a known 0"),
    ("kernel or4: x | 1 is x", "src/kernel/include/kirimasim/kernel/w4ops.h",
     "(~x.a & ~x.b) & (~y.a & ~y.b);  // both sides are known 0",
     "(~x.a & ~x.b) | (~y.a & ~y.b);  // both sides are known 0"),
    ("kernel casex compares x", "src/kernel/include/kirimasim/kernel/w4ops.h",
     "if (ignoreX) ignored |= x.b | y.b;", "if (ignoreX) ignored |= 0;"),
    ("valsem casez looks at one side", "src/valsem/value.cpp",
     "if (ignored(a) || ignored(b)) continue;", "if (ignored(a)) continue;"),
    ("valsem 1 & x is 1", "src/valsem/logic.cpp",
     "/* 0 */ {Zero, Zero, Zero, Zero},\n        /* 1 */ {Zero, One, X, X},",
     "/* 0 */ {Zero, Zero, Zero, Zero},\n        /* 1 */ {Zero, One, One, X},"),
    ("edge: x -> 1 is no posedge", "src/kernel/include/kirimasim/kernel/edge.h",
     "(from >= 2 && to == 1)", "(from == 2 && to == 1)"),
    ("scheduler: NBA in the Inactive region", "src/kernel/sim.cpp",
     "counters_.increment(cNbaScheduled_);\n"
     "    const Region r = processes_[p].group == Group::Active ? Region::Nba : Region::ReNba;",
     "counters_.increment(cNbaScheduled_);\n"
     "    const Region r = processes_[p].group == Group::Active ? Region::Inactive : Region::ReNba;"),
    ("scheduler: no flush point on an event wake-up", "src/kernel/sim.cpp",
     "proc.flushOnResume = true;", "proc.flushOnResume = false;"),
    ("vm: an x condition is true", "src/vm/vm.cpp",
     "return (c.a & ~c.b & 1U) != 0;", "return (c.a & 1U) != 0;"),
    ("format: %d automatic width one too wide", "src/kernel/format.cpp",
     "return decimal(powerOfTwo(width, 1)).size();",
     "return decimal(powerOfTwo(width, 1)).size() + 1;"),
    ("front: always_comb is sensitive to what it writes", "src/front/lower.cpp",
     "skip = skip || sir::covers(s, w, r);", "skip = skip || false;"),
    ("front: a process ignores what its body reads (assign, always @*)", "src/front/lower.cpp",
     "if (!dup) proc().triggers.push_back({r, sir::Edge::Any});",
     "if (false) proc().triggers.push_back({r, sir::Edge::Any});"),
    ("sir: tri1 and supply1 start at z", "src/sir/types.cpp",
     "case SlotKind::Supply1: return InitValue::One;", "case SlotKind::Supply1: return InitValue::Z;"),
    ("front: an operand is extended by its own sign", "src/front/lower_expr.cpp",
     "if (c.conversionKind == ast::ConversionKind::Propagated && sir::isIntegral(v.type) &&",
     "if (false && sir::isIntegral(v.type) &&"),
    ("front: next() of a value that is no member is 0 in 4-state", "src/front/lower_expr.cpp",
     ": sir::isFourState(vt) ? allXConstant(vt)",
     ": sir::isFourState(vt) ? constant(vt, 0)"),
    ("front: an increment steps a real by its smallest step", "src/front/lower_expr.cpp",
     "const Val one = coerce(constant(sir::s2(32), 1), rt, sl);",
     "const Val one = constant(rt, 1);"),
    ("front: a string method evaluates the index of its string twice",
     "src/front/lower_expr.cpp",
     "const Val self = place ? loadPlace(*place, sl) : lowerExpr(*selfExpr);",
     "const Val self = lowerExpr(*selfExpr);"),
    ("front: an assignment operator evaluates the index of its target twice",
     "src/front/lower_expr.cpp", "b_.lvalueRefs.push_back(loadPlace(p, sl));",
     "b_.lvalueRefs.push_back(lowerExpr(a.left()));"),
    ("front: a concatenation target fills its parts from the least significant one",
     "src/front/lower_expr.cpp", "{all.op, constant(sir::u2(32), lsb).op}, sl);",
     "{all.op, constant(sir::u2(32), p.width - lsb - w).op}, sl);"),
    ("vm: $fatal does not end the run", "src/vm/vm.cpp",
     "sched_.requestEnd(kernel::EndReason::Fatal, static_cast<int>(value(ops[2]).a));",
     "(void)value(ops[2]);"),
    ("front: a named block is not a scope of the messages", "src/front/lower_stmt.cpp",
     "b_.scope += \".\" + std::string(b.blockSymbol->name);", "b_.scope += \"\";"),
    ("front: an increment evaluates the index of its target twice", "src/front/lower_expr.cpp",
     "const Val old = coerce(loadPlace(p, sl), rt, sl);",
     "const Val old = coerce(lowerExpr(u.operand()), rt, sl);"),
    ("kernel: tri0 has no pull", "src/kernel/include/kirimasim/kernel/strength.h",
     "if (kind == NetKind::Tri0) range(-5, -5);", "if (false) range(-5, -5);"),
    ("kernel: a wand at equal strengths gives x", "src/kernel/include/kirimasim/kernel/strength.h",
     "if (sides == 1 || (sides == 3 && wand)) {", "if (sides == 1 || (sides == 3 && false)) {"),
    ("front: a weak drive strength counts as strong", "src/front/lower.cpp",
     "case ast::DriveStrength::Weak: return 3;", "case ast::DriveStrength::Weak: return 6;"),
    ("front: drivers that meet through a port between nets resolve with a strong port",
     "src/front/lower.cpp", "if (through != ps.end()) {", "if (false) {"),
    ("vm: an unknown condition picks the first string", "src/vm/vm.cpp",
     "temp(in.result) = str(1) == str(2) ? v(1) : W4{0, 0};  // 0 is \"\"",
     "temp(in.result) = v(1);"),
    ("vm: a violation report names the process's instance", "src/vm/vm.cpp",
     "                line += ops[2].text;\n                line += \" @ \";",
     "                line += d_.instances[p.instance].scopeName;\n                line += \" @ \";"),
    ("scheduler: reports confirmed in the order they were first held", "src/kernel/sim.cpp",
     "    // Reactive region; final ones wait for the Postponed region (15.2).\n    sortPending();\n",
     "    // Reactive region; final ones wait for the Postponed region (15.2).\n"),
    ("front: a merged net keeps the external kind", "src/front/lower.cpp",
     "slot.kind = mergedKind(*innerKind, slot.kind);", "slot.kind = slot.kind;"),
    ("front: packages get their rows and initializers after the instances",
     "src/front/lower.cpp", "    declarePackages(root);\n", ""),
    ("kernel: $countbits takes z for x", "src/kernel/include/kirimasim/kernel/w4ops.h",
     "if (zs) chosen |= ~x.a & x.b;", "if (zs) chosen |= x.a & x.b;"),
    ("front: && || and -> never skip their right operand", "src/front/lower_expr.cpp",
     "const Val stop = emit(Op::Ceq, sir::u2(1), {t.op, constant(t.type, decider).op}, sl);",
     "const Val stop = constant(sir::u2(1), decider - decider);"),
    ("sir: a range of every bit does not cover the whole slot", "src/sir/types.cpp",
     "r.width == 0 ? s.type.width : r.width", "r.width"),
    ("front: a member is written at bit 0 of its structure", "src/front/lower_expr.cpp",
     "        base.lsb = lsb;", "        base.lsb = 0;"),
    ("front: a pattern fills an array from its right bound", "src/front/lower_expr.cpp",
     "indices.push_back(d.left + static_cast<std::int64_t>(k) * step);",
     "indices.push_back(d.right - static_cast<std::int64_t>(k) * step);"),
    ("front: an assert without else does not call $error", "src/front/lower_stmt.cpp",
     "               s.assertionKind == ast::AssertionKind::Assume) {\n"
     "        emitVoid(Op::Syscall, {Operand::str(\"$error\"), Operand::str(b_.scope)}, sl);",
     "               s.assertionKind == ast::AssertionKind::Assume) {\n        (void)sl;"),
    ("front: a deferred assert without an action does not call $error", "src/front/lower_stmt.cpp",
     "        if (call == nullptr)\n"
     "            emitVoid(Op::Syscall, {Operand::str(\"$error\"), Operand::str(b_.scope)}, sl);",
     "        if (call == nullptr)\n            (void)sl;"),
    ("vm: ->> triggers at once", "src/vm/vm.cpp",
     "sched_.triggerNb(f.kernelId, *e, t.b != 0 ? 0 : t.a);", "sched_.trigger(*e);"),
    ("kernel: triggered stays true after its time slot", "src/kernel/sim.cpp",
     "return events_[e].triggeredAt == time_;", "return events_[e].triggeredAt <= time_;"),
    ("front: an event array element counts from the right bound", "src/front/lower_expr.cpp",
     "o.firstElem = elementNumber(slot, indices);",
     "o.firstElem = static_cast<std::uint32_t>(sir::elementCount(slot)) - 1 -\n"
     "                      elementNumber(slot, indices);"),
    ("front: every member of a structure starts at its first leaf", "src/front/lower.cpp",
     "        n += leafCount(g->getType());", "        n += 0;"),
    ("front: members take no default values", "src/front/lower_struct.cpp",
     "if (const ast::Expression* init = f->getInitializer())",
     "if (const ast::Expression* init = nullptr)"),
    ("front: a structure copy skips array members", "src/front/lower_struct.cpp",
     "        copyArray(storageBase(dst), fixedIndices(dst, sl), storageBase(src), fixedIndices(src, sl),\n"
     "                  out, sl);",
     "        (void)src;"),
    ("front: a structure pattern counts one leaf per member", "src/front/lower_struct.cpp",
     "            off += leafCount(ft);", "            off += 1;"),
    ("vm: the exponent of ** has the width of the result", "src/vm/vm.cpp",
     "sir::isSigned(rt), t(1).width,", "sir::isSigned(rt), w,"),
    ("front: inside compares integral members with ==", "src/front/lower_expr.cpp",
     "    const Val m = lowerExpr(e);\n    const bool integralPair = sir::isIntegral(v.type) && "
     "sir::isIntegral(m.type);",
     "    const Val m = lowerExpr(e);\n    const bool integralPair = false;"),
    ("front: an inside range has no upper bound", "src/front/lower_expr.cpp",
     "        bound(vr.right(), Op::Le);", "        (void)vr;"),
    ("front: disable of a named block does nothing", "src/front/lower_stmt.cpp",
     "        if (!n.exit) n.exit = newBlock();\n        terminate(Op::Br, {Operand::block(*n.exit)}, sl);\n"
     "        return;",
     "        return;"),
    ("front: << slices a stream from the left", "src/front/lower_expr.cpp",
     "    return reverseBlocks(stream, s.getSliceSize(), true, sl);",
     "    return reverseBlocks(stream, s.getSliceSize(), false, sl);"),
    ("front: unpacking takes the right end of a wider source", "src/front/lower_expr.cpp",
     "{src.op, constant(sir::u2(32), src.type.width - w).op}, sl);",
     "{src.op, constant(sir::u2(32), 0).op}, sl);"),
    ("front: an automatic string starts from a constant of words", "src/front/lower_stmt.cpp",
     "                if (t.kind == sir::TypeKind::String) return stringConstant(\"\");\n", ""),
    ("front: a constant made a string keeps its zero bytes", "src/front/lower_expr.cpp",
     "        if (byte && *byte != 0) out.push_back(static_cast<char>(*byte));",
     "        if (byte) out.push_back(static_cast<char>(*byte));"),
    ("front: %m prints nothing", "src/front/lower_expr.cpp",
     "ops.push_back(Operand::str(withScope(text, b_.scope)));",
     "ops.push_back(Operand::str(withScope(text, \"\")));"),
    ("vm: nba.delay updates in the current time step", "src/vm/vm.cpp",
     "nbaSlot(f, o.id, o.firstElem, o.lsb, o.width, v(1), d.b != 0 ? 0 : d.a);",
     "nbaSlot(f, o.id, o.firstElem, o.lsb, o.width, v(1), 0 * d.a);"),
    ("vm: $test$plusargs wants the whole plusarg", "src/vm/vm.cpp",
     "found = found || std::string_view(a).starts_with(prefix);",
     "found = found || std::string_view(a) == prefix;"),
    ("front: elements of an array of instances get no connections", "src/front/lower.cpp",
     "                                                             lowerPorts(i);\n"
     "                                                             lowerInstance(i);",
     "                                                             lowerInstance(i);"),
    ("front: an unnamed block hides the scope around it", "src/front/lower.cpp",
     "        if (s.kind == ast::SymbolKind::StatementBlock && s.name.empty()) continue;",
     "        if (s.kind == ast::SymbolKind::StatementBlock && s.name.empty()) break;"),
    ("front: an array method ignores its with clause", "src/front/lower_expr.cpp",
     "        if (with != nullptr) {\n            iterators_[item] = v;",
     "        if (false && with != nullptr) {\n            iterators_[item] = v;"),
    ("kernel: %E and %G write a small e", "src/kernel/format.cpp",
     "p.spec.cformat += format[i] == 'E' || format[i] == 'G' ? format[i] : conv;",
     "p.spec.cformat += conv;"),
    ("front: an array element is written as soon as it is computed", "src/front/lower_expr.cpp",
     "        out.emplace_back(elementPlace(s, indices), v);\n        return;",
     "        storePlace(elementPlace(s, indices), v, false, sl);\n        return;"),
    ("front: aggregate == looks at the last pair only", "src/front/lower_expr.cpp",
     "        all = bitOp(Op::And, all, pair(i), sl);", "        all = pair(i);"),
    ("front: a constant index into an array of structures is 0", "src/front/lower_struct.cpp",
     "            return withIndex(std::move(*base), *c);",
     "            return withIndex(std::move(*base), 0 * *c);"),
    ("vm: $sscanf drops the first value it reads", "src/vm/vm.cpp",
     "                for (std::size_t i = 0; i < scanValues_.size() && 3 + i < ops.size(); ++i)",
     "                for (std::size_t i = 1; i < scanValues_.size() && 3 + i < ops.size(); ++i)"),
    ("front: $sscanf assigns one target more than its count", "src/front/lower_expr.cpp",
     "        const Val above = emit(Op::Gt, sir::u2(1),",
     "        const Val above = emit(Op::Ge, sir::u2(1),"),
    ("kernel: %c skips white space", "src/kernel/scan.cpp",
     "        if (s.conv != 'c') skipSpace();", "        skipSpace();"),
    ("front: a stream into an array is put at the right end", "src/front/lower_expr.cpp",
     "                          {st.op, constant(sir::u2(all.width - st.type.width), 0).op}, sl);",
     "                          {constant(sir::u2(all.width - st.type.width), 0).op, st.op}, sl);"),
    ("vm: automatic arrays share their elements", "src/vm/vm.cpp",
     "                l.size += sir::elementCount(frame[i]) * l.chunks[i];",
     "                l.size += l.chunks[i];"),
    ("vm: a ref to an element reads the first element", "src/vm/vm.cpp",
     ": readSlot(refSlot(r) + static_cast<sir::SlotId>(k), refElem(r), 0, 0));",
     ": readSlot(refSlot(r) + static_cast<sir::SlotId>(k), 0, 0, 0));"),
    ("vm: an output array is not copied back", "src/vm/vm.cpp",
     "                for (std::size_t e = 0; e * c < outs[i].size(); ++e)",
     "                for (std::size_t e = 0; e * c < 0; ++e)"),
    ("front: an automatic array keeps its elements over a new scope entry",
     "src/front/lower_stmt.cpp",
     "                    for (Place& e : places)\n                        stores.emplace_back(std::move(e), initial(t));",
     "                    (void)places;"),
    ("front: === on chandles means !==", "src/front/lower_expr.cpp",
     "        const Op eq = op == Op::Eq || op == Op::Ceq ? Op::Eq : Op::Ne;",
     "        const Op eq = op == Op::Eq ? Op::Eq : Op::Ne;"),
    ("vm: the b, o and h forms print decimal", "src/vm/vm.cpp",
     "                spec.conv = info.radix;", "                spec.conv = 'd';"),
    ("kernel: a real %t is truncated", "src/kernel/format.cpp",
     "digits[digits.size() - drop] >= '5';", "digits[digits.size() - drop] >= ':';"),
    ("vm: $timeformat does nothing", "src/vm/vm.cpp",
     "                timeFormat_ = tf;", "                (void)tf;"),
    ("front: a specparam is not a constant in a delay", "src/front/lower_expr.cpp",
     "    ast::EvalContext ctx(*evalSymbol_, ast::EvalFlags::SpecparamsAllowed);",
     "    ast::EvalContext ctx(*evalSymbol_);"),
    ("front: a specify block of specparams is reported", "src/front/lower.cpp",
     "                    onlyParams = onlyParams && m.kind == ast::SymbolKind::Specparam;",
     "                    onlyParams = false;"),
    ("front: $bits of a hierarchical name is unsupported", "src/front/lower_expr.cpp",
     "                if (name == \"$bits\" && c.arguments().size() == 1 &&",
     "                if (false && c.arguments().size() == 1 &&"),
    ("sir: a driver starts at z", "src/sir/types.cpp",
     "        if (p.kind != ProcessKind::Cont) continue;\n        for (const SlotRange& w : p.writes)",
     "        continue;\n        for (const SlotRange& w : p.writes)"),
    ("front: an alias gives each net its own slot", "src/front/lower.cpp",
     "        if (member.kind == ast::SymbolKind::NetAlias) aliasNets(member.as<ast::NetAliasSymbol>());",
     "        if (member.kind == ast::SymbolKind::NetAlias) (void)member;"),
    ("kernel: a delayed driver keeps an update of another value", "src/kernel/sim.cpp",
     "    if (d.pending && !std::equal(value, value + words, d.value.begin())) {\n"
     "        d.pending = false;",
     "    if (d.pending && !std::equal(value, value + words, d.value.begin())) {\n"
     "        d.pending = true;"),
    ("kernel: a change to x takes the rise delay", "src/kernel/sim.cpp",
     "delay = !b ? (a ? rise : fall) : (a ? std::min({rise, fall, off}) : off);",
     "delay = !b ? (a ? rise : fall) : (a ? rise : off);"),
    ("kernel: a vector falling to 0 takes the rise delay", "src/kernel/sim.cpp",
     "    } else if (!all(now.data(), false, false) && all(value, false, false)) {",
     "    } else if (false) {"),
    ("front: a continuous assignment ignores its delay", "src/front/lower.cpp",
     "        if (delay != nullptr) b_.delays = delaysOf(*delay, sl);",
     "        if (false) b_.delays = delaysOf(*delay, sl);"),
    ("kernel: a UDP's level rows do not decide", "src/kernel/include/kirimasim/kernel/udp.h",
     "            if (detail::udpLevel(r, v, t.inputs, t.inputs))\n"
     "                return r.output == kUdpKeep ? state : r.output;",
     "            if (false)\n                return r.output == kUdpKeep ? state : r.output;"),
    ("kernel: an unlisted UDP change keeps the state", "src/kernel/include/kirimasim/kernel/udp.h",
     "    return edge >= 0 ? static_cast<std::uint8_t>(edge) : kUdpX;",
     "    return edge >= 0 ? static_cast<std::uint8_t>(edge) : state;"),
    ("kernel: a UDP input z is not read as x", "src/kernel/include/kirimasim/kernel/udp.h",
     "inline std::uint8_t udpKnown(std::uint8_t v) { return v >= kUdpX ? kUdpX : v; }",
     "inline std::uint8_t udpKnown(std::uint8_t v) { return v; }"),
    ("valsem: p takes a fall from x", "src/valsem/udp.cpp",
     "case 'p': return (from == L::Zero && to != L::Zero) || (from == L::X && to == L::One);",
     "case 'p': return (from == L::Zero && to != L::Zero) || (from == L::X && to == L::Zero);"),
    ("front: a UDP forgets the inputs it saw", "src/front/lower.cpp",
     "                emitVoid(sir::Op::Store, {sir::Operand::slot(prev), now.op}, sl);",
     "                (void)now;"),
    ("front: a UDP ignores its initial statement", "src/front/lower.cpp",
     "        if (init != 'x') {", "        if (false) {"),
    ("front: a fork in a task belongs to a process", "src/front/lower_stmt.cpp",
     "        b_.isSub ? sir::Owner::Kind::Subroutine : sir::Owner::Kind::Process;",
     "        sir::Owner::Kind::Process;"),
    ("kernel: writes to a held slot ignore the hold", "src/kernel/sim.cpp",
     "if (heldOf_.empty() || heldOf_[s] == kNotHeld) return writeRaw(s, elem, lsb, width, value);",
     "return writeRaw(s, elem, lsb, width, value);"),
    ("kernel: a released net keeps the forced value", "src/kernel/sim.cpp",
     "        if (h.net)\n            showHeld(part.slot, e, lsb, width);",
     "        if (false)\n            showHeld(part.slot, e, lsb, width);"),
    ("kernel: a released driven variable changes at once", "src/kernel/sim.cpp",
     "if (level == Hold::Force && (other || h.driven)) {",
     "if (false) {"),
    ("kernel: assign shows over force", "src/kernel/sim.cpp",
     "const std::uint64_t* from = h.owner[1][bit] != kNoHolder   ? h.value[1].data() + at\n"
     "                                    : h.owner[0][bit] != kNoHolder ? h.value[0].data() + at",
     "const std::uint64_t* from = h.owner[0][bit] != kNoHolder   ? h.value[0].data() + at\n"
     "                                    : h.owner[1][bit] != kNoHolder ? h.value[1].data() + at"),
    ("vm: a force does not follow its right side", "src/vm/vm.cpp",
     "        o.level = level;\n        waitTriggers(o, d_.processes[ops[1].id]);",
     "        o.level = level;"),
    ("front: a gate passes an input z through", "src/front/lower.cpp",
     "const auto known = [&](const Val& a) { return op2(sir::Op::Xor, a, constant(bit, 0)); };",
     "const auto known = [&](const Val& a) { return a; };"),
    ("front: a three-state gate gives z for an x control", "src/front/lower.cpp",
     "{off.op, constant(bit, 0, 1).op, constant(bit, 1, 1).op}, sl)",
     "{off.op, constant(bit, 0, 1).op, constant(bit, 0, 1).op}, sl)"),
    ("front: L and H drive both sides", "src/front/lower.cpp",
     "const Val unknown = inv(op2(sir::Op::Or, on, off));",
     "const Val unknown = op2(sir::Op::And, on, off);"),
    ("front: a pull drives strong", "src/front/lower.cpp",
     "if (!ds.second) strength.first = 5;", "if (!ds.second) strength.first = 6;"),
    ("vm: resolve ignores a driver's side slot", "src/vm/vm.cpp",
     "                d.one = side & kernel::mask(w);", "                d.one = ~side;"),
    ("vm: wait (expr) does not wait on what a function reads", "src/vm/vm.cpp",
     "                if (in.op == Op::Call) collectReads(",
     "                if (false) collectReads("),
    ("vm: wait (expr) misses the triggers of ev.triggered", "src/vm/vm.cpp",
     "                    m.events.push_back(static_cast<kernel::EventId>(h - 1));",
     "                    (void)h;"),
    ("vm: a guard in a task reads the process's frame", "src/vm/vm.cpp",
     "        std::vector<W4>& vars = f.calls.empty() ? f.vars : f.calls.back().vars;",
     "        std::vector<W4>& vars = f.vars;"),
    ("vm: wait (expr) waits when it is true", "src/vm/vm.cpp",
     "        if (!now || *now) return now ? std::optional<bool>(false) : std::nullopt;",
     "        if (!now) return std::nullopt;"),
    ("front: a delay known when it runs is not scaled", "src/front/lower_expr.cpp",
     "    if (scale == 0) return v;\n    return emit(Op::Mul, t64,",
     "    if (true) return v;\n    return emit(Op::Mul, t64,"),
    ("front: a real delay known when it runs is rounded to the unit", "src/front/lower_expr.cpp",
     "        scale = ts.precision - precision();", "        scale = ts.unit - precision();"),
    ("front: a parameter's element at a constant index is loaded", "src/front/lower_expr.cpp",
     "        if (base != nullptr && base->kind == ast::SymbolKind::Parameter)\n",
     "        if (false)\n"),
    ("front: a parameter's slot gets its first element everywhere", "src/front/lower.cpp",
     "            o.firstElem = static_cast<std::uint32_t>(i);\n            o.numElems = 1;",
     "            o.firstElem = 0;\n            o.numElems = 1;"),
    ("kernel: the monitor does not run after a change", "src/kernel/sim.cpp",
     "                monitorDue_ = true;", "                (void)0;"),
    ("kernel: $monitoron does not run the monitor", "src/kernel/sim.cpp",
     "    if (on) monitorDue_ = true;", "    (void)on;"),
    ("vm: $monitoron does not start the monitor afresh", "src/vm/vm.cpp",
     "                if (monitor_ != nullptr) freshVars(*monitor_);", "                (void)0;"),
    ("front: $monitor compares $time", "src/front/lower_expr.cpp",
     '                    if (n == "$time" || n == "$stime" || n == "$realtime") continue;',
     "                    (void)n;"),
    ("front: $monitor shows values that did not change", "src/front/lower_expr.cpp",
     "            if (changed) show = bitOp(Op::Or, show, *changed, sl);",
     "            show = constant(sir::u2(1), 1);"),
    ("vm: disable leaves the processes the activation started", "src/vm/vm.cpp",
     "            killChildren(g, children);", "            (void)children;"),
    ("vm: disable does not wake another process", "src/vm/vm.cpp",
     "            sched_.start(g.kernelId);", "            (void)g;"),
    ("front: a block disabled from outside is not a scope", "src/front/lower_stmt.cpp",
     "                    disabledFromOutside_.insert(sym);", "                    (void)sym;"),
    ("front: return leaves its scopes active", "src/front/lower_sub.cpp",
     "    leaveScopes(0, sl);", "    (void)sl;"),
    ("front: a deferred action reads its values when it runs", "src/front/lower_stmt.cpp",
     "            substitutions_[passed[i]] = emit(Op::Fload, values[i].type,",
     "            (void)emit(Op::Fload, values[i].type,"),
    ("kernel: a final assertion is confirmed in the Observed region", "src/kernel/sim.cpp",
     "            if (r.final) {", "            if (false) {"),
    ("kernel: a flush point keeps the pending queue", "src/kernel/sim.cpp",
     "    if (proc.pending.empty()) return;", "    return;"),
    ("front: $typename keeps the default signing", "src/front/type_name.cpp",
     "if (p.isSigned != ast::PredefinedIntegerType::isDefaultSigned(p.integerKind))",
     "if (true)"),
    ("front: anonymous types are numbered across kinds", "src/front/type_name.cpp",
     "        if (k == letter) ++n;", "        ++n;"),
    ("front: a structure is not named by its typedef", "src/front/type_name.cpp",
     "if (const ast::TypeAliasType* a = aliasDeclaring(t))",
     "if (const ast::TypeAliasType* a = nullptr)"),
    ("front: an output structure is not copied back", "src/front/lower_sub.cpp",
     "        copyAggregate(structOuts[k].first, structOuts[k].second, *structOutTypes[k], stores, sl);",
     "        (void)k;"),
    ("front: a static function's $return slots are left as they are", "src/front/lower_sub.cpp",
     "        if (b_.retSlots) {", "        if (false) {"),
    ("front: an inout structure is not copied in", "src/front/lower_sub.cpp",
     "            if (prm.dir != sir::Direction::Output) {\n                Stores stores;",
     "            if (prm.dir == sir::Direction::Input) {\n                Stores stores;"),
    ("kernel: a bit without its 1 side keeps its strength",
     "src/kernel/include/kirimasim/kernel/strength.h",
     "((drivers[d].one >> bit) & 1U) != 0 ? drivers[d].s1 : 0;",
     "((drivers[d].one >> bit) & 1U) != 0 ? drivers[d].s1 : drivers[d].s1;"),
    ("front: a program instance runs like a module", "src/front/lower.cpp",
     "    reportProgram(inst);\n", "    (void)inst;\n"),
    ("front: an anonymous program is not reported", "src/front/lower.cpp",
     "        report(s.location, \"anonymous program\");", "        (void)s;"),
    ("front: a join whose branches start processes starts none", "src/front/lower_stmt.cpp",
     "if (b.blockKind != ast::StatementBlockKind::JoinAll || spawns != before) spawn();",
     "if (b.blockKind != ast::StatementBlockKind::JoinAll) spawn();"),
    ("front: a function that forks starts no process", "src/front/lower_stmt.cpp",
     "if (sub.subroutineKind == ast::SubroutineKind::Task || mayFork(sub)) spawn();",
     "if (sub.subroutineKind == ast::SubroutineKind::Task) spawn();"),
    ("vm: the record of a finished process is never released", "src/vm/vm.cpp",
     "if (f.released || !f.finished || !f.children.empty() || f.reports != 0) return;",
     "if (f.released || f.finished || !f.children.empty() || f.reports != 0) return;"),
    ("vm: a process is released while its children are kept", "src/vm/vm.cpp",
     "if (f.released || !f.finished || !f.children.empty() || f.reports != 0) return;",
     "if (f.released || !f.finished || f.reports != 0) return;"),
    ("vm: a task call tells its children by their place", "src/vm/vm.cpp",
     "        c.children = f.births;\n",
     "        c.children = static_cast<std::uint32_t>(f.children.size());\n"),
    ("vm: a block tells its children by their place", "src/vm/vm.cpp",
     "static_cast<std::uint32_t>(f.syncCalls.size()), f.births});",
     "static_cast<std::uint32_t>(f.syncCalls.size()),\n"
     "                             static_cast<std::uint32_t>(f.children.size())});"),
    ("vm: a process is released while the kernel holds its reports", "src/vm/vm.cpp",
     "if (&f == running_ || sched_.hasPending(f.kernelId)) {", "if (&f == running_) {"),
    ("vm: a function's result wider than 64 bits keeps its low chunk", "src/vm/vm.cpp",
     "if (in.result != sir::kNone && chunksOf(in.type) > 1) wide = std::move(retWide_);",
     "(void)retWide_;"),
    ("vm: a net wider than 64 bits resolves its low chunk", "src/vm/vm.cpp",
     "for (std::uint32_t k = 0; k < c; ++k) {\n                drivers_.clear();",
     "for (std::uint32_t k = 0; k < 1; ++k) {\n                drivers_.clear();"),
    ("vm: %h shows the low chunk of a wide value", "src/vm/vm.cpp",
     "        for (const W4& c : chunks(o)) {\n            words.push_back(c.a);",
     "        for (const W4& c : std::vector<W4>{value(o)}) {\n            words.push_back(c.a);"),
    ("vm: $countbits counts the low chunk", "src/vm/vm.cpp",
     "chunksOf(ct) > 1\n                        ? kernel::wide::countBits",
     "false\n                        ? kernel::wide::countBits"),
    ("vm: force writes the low chunk", "src/vm/vm.cpp",
     "        std::vector<W4> v((w + 63) / 64);\n        readOperand(value, v.data()",
     "        std::vector<W4> v(1);\n        readOperand(value, v.data()"),
    ("vm: $sscanf writes the low chunk of a wide target", "src/vm/vm.cpp",
     "        if (c == 1 || !sir::isIntegral(t)) {\n            *out = scannedWord(v, t);",
     "        if (true) {\n            *out = scannedWord(v, t);"),
    ("vm: a ref to a wide value reads one word", "src/vm/vm.cpp",
     "            const std::uint32_t c = chunksOf(vt);\n            if (c == 1) return false;",
     "            const std::uint32_t c = chunksOf(vt);\n            if (c >= 1) return false;"),
    ("vm: a ref into a frame steps one word per element", "src/vm/vm.cpp",
     "at += std::size_t{*el} * chunksOf(fv.type);",
     "at += *el;"),
    ("kernel: a scanned value keeps its low chunk", "src/kernel/scan.cpp",
     "if (out.a != 0 || out.b != 0) acc.push_back(out);",
     "(void)out;"),
    ("front: a wide default is x in its low word", "src/front/lower.cpp",
     "    const std::size_t words = (t.width + 63) / 64;\n    return constantWords(",
     "    return constant(t, ~std::uint64_t{0}, ~std::uint64_t{0});\n    const std::size_t words = (t.width + 63) / 64;\n    return constantWords("),
    ("front: a wide constant keeps its low word", "src/front/lower_expr.cpp",
     "if (bit.value == 1 || x) a[i / 64] |= std::uint64_t{1} << (i % 64);",
     "if ((bit.value == 1 || x) && i < 64) a[i / 64] |= std::uint64_t{1} << (i % 64);"),
    ("kernel: %s prints a zero byte inside a value as it is", "src/kernel/format.cpp",
     "bytes.push_back(c == 0 ? ' ' : static_cast<char>(c));",
     "bytes.push_back(static_cast<char>(c));"),
    ("kernel: NaN converts to 0 in a four-state integer", "src/kernel/wide.cpp",
     "        if (fourState) allX(r, w);", "        (void)fourState;"),
    ("kernel: a real past 64 bits converts to 0", "src/kernel/wide.cpp",
     "words[static_cast<std::size_t>(i) / 64] |= std::uint64_t{1} << (i % 64);", "(void)i;"),
    ("front: a constant real to integer takes slang's value", "src/front/lower_expr.cpp",
     "            if (op.type->isFloating() && e.type->isIntegral()) return true;\n", ""),
    ("kernel: $rtoi rounds like a conversion", "src/kernel/wide.cpp",
     "    const double v = truncate ? std::trunc(d) : std::round(d);",
     "    const double v = std::round(d);"),
    ("vm: $rtoi gives 0 for NaN", "src/vm/vm.cpp",
     "                                               sir::isFourState(rt), true);",
     "                                               false, true);"),
    ("kernel: a reused place counts its waits from 0", "src/kernel/sim.cpp",
     "        proc.gen = processes_[id].gen + 1;", "        proc.gen = 0;"),
    ("kernel: held reports go by place, not creation", "src/kernel/sim.cpp",
     "        return processes_[a].serial < processes_[b].serial;", "        return a < b;"),
]


def run(cmd: list[str]) -> int:
    return subprocess.run(cmd, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                          check=False).returncode


def run_tests(cmd: list[str]) -> tuple[int, set[str]]:
    """The exit code of ctest and the names of the tests it skipped."""
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, check=False)
    skipped = set(re.findall(r"Test +#\d+: (\S+) \.*\*+Skipped", p.stdout))
    return p.returncode, skipped


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--preset", default="dev")
    ap.add_argument("--only", default="", help="only mutants whose name contains this")
    args = ap.parse_args()

    if run(["git", "diff", "--quiet", "--", "src"]) != 0:
        print("mutants: src/ has uncommitted changes; commit or stash them first",
              file=sys.stderr)
        return 2
    build = ["cmake", "--build", "--preset", args.preset]
    test = ["ctest", "--preset", args.preset, "-j8"]
    if run(build) != 0:
        print("mutants: the unchanged sources do not build", file=sys.stderr)
        return 2
    rc, skipped_before = run_tests(test)
    if rc != 0:
        print("mutants: tests fail on the unchanged sources", file=sys.stderr)
        return 2

    bad = 0
    for name, rel, old, new in MUTANTS:
        if args.only not in name:
            continue
        path = ROOT / rel
        original = path.read_text(encoding="utf-8")
        if original.count(old) != 1:
            print(f"DOES NOT APPLY  {name} ({rel})")
            bad += 1
            continue
        try:
            path.write_text(original.replace(old, new), encoding="utf-8")
            if run(build) != 0:
                print(f"DOES NOT BUILD  {name}")
                bad += 1
                continue
            rc, skipped = run_tests(test)
            caught = rc != 0 or not skipped <= skipped_before
        finally:
            path.write_text(original, encoding="utf-8")
        print(f"{'caught' if caught else 'SURVIVED':14}  {name}")
        bad += 0 if caught else 1
    if run(build) != 0:
        print("mutants: the unchanged sources do not build", file=sys.stderr)
        return 2
    print(f"mutants: {bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
