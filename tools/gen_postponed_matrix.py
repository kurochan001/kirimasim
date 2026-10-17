#!/usr/bin/env python3
"""Generates the $strobe and $monitor matrix test of TEST_SUITE.md ("機構の行列")
in tests/suite/proc/.

  tools/gen_postponed_matrix.py           # writes the files
  tools/gen_postponed_matrix.py --check   # exit 1 if a file differs from what it would write

$strobe shows its arguments at the end of the time slot it is called in,
once per call (21.2.2). $monitor shows its arguments at the end of the
time slot it is called in, and then at the end of every time slot after
which one of them is not what it showed last; $time is not compared. One
$monitor is in effect at a time; a new one replaces it. $monitoroff stops
it, and $monitoron shows the arguments once more, changed or not, at the
end of that time slot (21.2.3, SYSTEM_TASKS.md 4). The b, o and h forms
show values without a specifier in that radix (SYSTEM_TASKS.md 3.1).

The axes: the container that calls them (an initial and an always
procedure, a fork branch, a static and an automatic task, a function),
what the arguments read (a variable, a vector, an array element at a
constant and at a variable index, a net, an expression, a function call,
a string, a real, a value with x bits, a hierarchical name, a package
variable), and how the values change in the time slot of a strobe (a
blocking assignment, a nonblocking one, one after #0) and between the
monitor's time slots (a change, a change of an element it does not show,
a change of the index, a change undone in the same slot, changes while
monitoring is off, two changes in one slot, none, and none with
$monitoroff and $monitoron).

The expected outputs follow from the rules above. A person checks them
once for all the tests made from them (TEST_SUITE.md, "人の抜き取り").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "postponed_matrix"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

CONTAINERS = ["initial", "always", "fork branch", "static task", "automatic task", "function"]
ARGS = ("v, vp, arr[1], arr[k], w, v + vp, dbl(), s, r, x4, c.q, pk::pv")
FORMAT = "v=%0d vp=%h a1=%0d ak=%0d w=%h e=%0d d=%0d s=%s r=%0.1f x=%b q=%0d p=%0d"
STROBE_STEP = 2   # the strobe phase: container i calls $strobe at 1 + 2i
MONITOR_START = 20
WINDOW = 12       # the monitor phase: container i calls $monitor at 20 + 12i


def xor1(bits):
    """x4 ^ 4'b0001 on a text of four 0, 1, x."""
    last = bits[3]
    return bits[:3] + {"0": "1", "1": "0", "x": "x"}[last]


def initial_state():
    return {"v": 1, "vp": 0x10, "arr": [0, 0, 0, 0], "k": 0, "s": "a", "r": 1.5,
            "x4": "10x1", "q": 0, "pv": 0}


def render(st):
    return (f"v={st['v']} vp={st['vp']:02x} a1={st['arr'][1]} ak={st['arr'][st['k']]} "
            f"w={(st['vp'] + 1) & 0xff:02x} e={st['v'] + st['vp']} d={2 * st['v']} "
            f"s={st['s']} r={st['r']:.1f} x={st['x4']} q={st['q']} p={st['pv']}")


def strobe_changes(st):
    """The driver's changes in the time slot of a strobe: blocking,
    nonblocking and after #0; the strobe shows them all."""
    st["v"] += 1
    st["vp"] = (st["vp"] + 1) & 0xff
    st["arr"][1] += 1


# The driver's steps in a monitor window: (offset, SystemVerilog, change of
# the model; None for $monitoroff and $monitoron).
def monitor_steps():
    def inc_v(st):
        st["v"] += 1

    def inc_a2(st):
        st["arr"][2] += 1

    def flip_k(st):
        st["k"] = 0 if st["k"] == 2 else 2

    def glitch(st):
        pass

    def off(st):
        st["x4"] = xor1(st["x4"])

    def two(st):
        st["s"] += "!"
        st["r"] += 0.5

    def far(st):
        st["q"] = (st["q"] + 1) & 0xf
        st["pv"] += 1

    return [
        (1, "v = v + 1;", inc_v, None),
        (2, "arr[2] = arr[2] + 1;", inc_a2, None),
        (3, "k = (k == 2) ? 0 : 2;", flip_k, None),
        (4, "vp = vp + 1; vp = vp - 1;", glitch, None),
        (5, "$monitoroff; x4 = x4 ^ 4'b0001;", off, "off"),
        (6, "$monitoron;", None, "on"),
        (7, "s = {s, \"!\"}; r = r + 0.5;", two, None),
        (8, "c.q = c.q + 1; pk::pv = pk::pv + 1;", far, None),
        (9, ";", None, None),
        (10, "$monitoroff; $monitoron;", None, "on"),
        (11, ";", None, None),
    ]


def files():
    st = initial_state()
    exp = []
    # The strobe phase.
    driver = []
    now = 0
    for i, c in enumerate(CONTAINERS):
        t = 1 + STROBE_STEP * i
        driver.append(f"    #{t - now} v = v + 1;\n    vp <= vp + 1;\n    #0 arr[1] = arr[1] + 1;")
        now = t
        strobe_changes(st)
        exp += [f"{c} strobe {t}: {render(st)}"] * 2
        if c == "initial":
            exp.append(f"{c} strobeh {t}: {st['v']:08x}")
    # The monitor phase: one window per container, and one for $monitorh.
    windows = CONTAINERS + ["monitorh"]
    for i, c in enumerate(windows):
        base = MONITOR_START + WINDOW * i
        on = True
        shown = None

        def show(t, force=False):
            nonlocal shown
            line = st["v"] if c == "monitorh" else render(st)
            if force or line != shown:
                if c == "monitorh":
                    exp.append(f"{c} {t}: {st['v']:08x}")
                else:
                    exp.append(f"{c} monitor {t}: {render(st)}")
                shown = line

        show(base, True)
        for off, text, change, ctl in monitor_steps():
            t = base + off
            driver.append(f"    #{t - now} {text}")
            now = t
            if change is not None:
                change(st)
            if ctl == "off":
                on = False
                continue
            if ctl == "on":
                on = True
                show(t, True)
                continue
            if on:
                show(t)
    end = MONITOR_START + WINDOW * len(windows)

    def strobe(c):
        return [f"$strobe(\"{c} strobe %0t: {FORMAT}\", $time, {ARGS});"] * 2

    def monitor(c):
        return [f"$monitor(\"{c} monitor %0t: {FORMAT}\", $time, {ARGS});"]

    sv = (f"/*\n:name: {NAME}\n:description: $strobe shows the values at the end of its time "
          "slot, once per call; $monitor shows them when it is called and after every time slot "
          "that changes one of them, not for $time, glitches or elements it does not show, and "
          "once more at $monitoron; called from procedures, a fork branch, tasks and a function, "
          "reading variables, parts, elements, nets, expressions, function calls, strings, reals, "
          "x, hierarchical names and package variables\n:tags: proc\n:type: simulation\n"
          ":lrm: 1800-2023 21.2.2 21.2.3\n:oracle: spec\n:spec_check: ai\n:fourstate: yes\n"
          ":nondeterministic: no\n"
          f":generator: gen_postponed_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_postponed_matrix.py; change the generator, not this file.\n"
          "// No time slot has both a $strobe and a $monitor line: the order of the two in one\n"
          "// time slot is not the standard's (SYSTEM_TASKS.md 4). Icarus Verilog 13.0 takes\n"
          "// only simple signals as $monitor arguments. Verilator 5.052 reads x as 0, and its\n"
          "// $monitor shows nothing in the time slot of the call or of $monitoron and shows\n"
          "// changes of whole variables, such as elements it does not show.\n"
          "package pk;\n  int pv;\nendpackage\n"
          "module child;\n  logic [3:0] q = 0;\nendmodule\n"
          "module top;\n"
          "  int v = 1;\n  logic [7:0] vp = 8'h10;\n  int arr[4];\n  int k = 0;\n"
          "  wire [7:0] w = vp + 1;\n  string s = \"a\";\n  real r = 1.5;\n"
          "  logic [3:0] x4 = 4'b10x1;\n  logic go_s = 0, go_m = 0;\n  child c();\n"
          "  function int dbl();\n    return 2 * v;\n  endfunction\n")
    pad = "    "
    sv += ("  task static ts_s();\n" + "".join(f"{pad}{x}\n" for x in strobe("static task")) +
           "  endtask\n")
    sv += ("  task automatic ta_s();\n" +
           "".join(f"{pad}{x}\n" for x in strobe("automatic task")) + "  endtask\n")
    sv += ("  function void f_s();\n" + "".join(f"{pad}{x}\n" for x in strobe("function")) +
           "  endfunction\n")
    sv += ("  task static ts_m();\n" + "".join(f"{pad}{x}\n" for x in monitor("static task")) +
           "  endtask\n")
    sv += ("  task automatic ta_m();\n" +
           "".join(f"{pad}{x}\n" for x in monitor("automatic task")) + "  endtask\n")
    sv += ("  function void f_m();\n" + "".join(f"{pad}{x}\n" for x in monitor("function")) +
           "  endfunction\n")
    sv += ("  always @(posedge go_s) begin\n" +
           "".join(f"{pad}{x}\n" for x in strobe("always")) + "  end\n")
    sv += ("  always @(posedge go_m) begin\n" +
           "".join(f"{pad}{x}\n" for x in monitor("always")) + "  end\n")
    # The calls, each container at its time.
    main = []
    now = 0
    for i, c in enumerate(CONTAINERS):
        t = 1 + STROBE_STEP * i
        main.append(f"    #{t - now};")
        now = t
        if c == "initial":
            main += [f"    {x}" for x in strobe(c)]
            main.append("    $strobeh(\"initial strobeh %0t: \", $time, v);")
        elif c == "always":
            main.append("    go_s = 1;")
        elif c == "fork branch":
            main.append("    fork\n      begin\n" +
                        "".join(f"        {x}\n" for x in strobe(c)) + "      end\n    join")
        elif c == "static task":
            main.append("    ts_s();")
        elif c == "automatic task":
            main.append("    ta_s();")
        else:
            main.append("    f_s();")
    for i, c in enumerate(windows):
        t = MONITOR_START + WINDOW * i
        main.append(f"    #{t - now};")
        now = t
        if c == "initial":
            main += [f"    {x}" for x in monitor(c)]
        elif c == "always":
            main.append("    go_m = 1;")
        elif c == "fork branch":
            main.append("    fork\n      begin\n" +
                        "".join(f"        {x}\n" for x in monitor(c)) + "      end\n    join")
        elif c == "static task":
            main.append("    ts_m();")
        elif c == "automatic task":
            main.append("    ta_m();")
        elif c == "function":
            main.append("    f_m();")
        else:
            main.append("    $monitorh(\"monitorh %0t: \", $time, v);")
    main.append(f"    #{end - now} $finish;")
    sv += "  initial begin\n" + "\n".join(main) + "\n  end\n"
    sv += "  initial begin\n" + "\n".join(driver) + "\n  end\nendmodule\n"
    return {f"{NAME}.sv": sv, f"{NAME}.expected": "\n".join(exp) + "\n"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_postponed_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_postponed_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
