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
     ": sir::isFourState(vt) ? constant(vt, maskOf(vt.width), maskOf(vt.width))",
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
     "    std::sort(pendingOrder_.begin(), pendingOrder_.end());\n", ""),
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
     "emitVoid(Op::Syscall, {Operand::str(\"$error\"), Operand::str(b_.scope)}, sl);",
     "(void)sl;"),
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
     "copyArray(to.id, {}, from.id, {}, nonblocking, sl);", "(void)from;"),
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
     "            terminate(Op::Br, {Operand::block(*n->exit)}, sl);\n            return;\n        }\n"
     "        case ast::StatementKind::WaitFork",
     "            return;\n        }\n        case ast::StatementKind::WaitFork"),
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
     "                                         lowerPorts(i);\n                                         lowerInstance(i);",
     "                                         lowerInstance(i);"),
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
