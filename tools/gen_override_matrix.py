#!/usr/bin/env python3
"""Generates the procedural continuous assignment matrix test of
TEST_SUITE.md ("機構の行列") in tests/suite/proc/.

  tools/gen_override_matrix.py           # writes the files
  tools/gen_override_matrix.py --check   # exit 1 if a file differs from what it would write

The axes: the target of force and release (a four-state and a two-state
variable written by procedures, a variable a continuous assignment
drives, a net with one driver, a net with two drivers that resolves, a
part and a bit of a net, a concatenation of a variable and a net, a net
of a child instance and a variable of a generate block by hierarchical
names); the steps (force with a right side that reads a variable, a
change of that variable, a procedural write while forced, release read at
once and after the Active region, writes and changes after it); assign
and deassign of a variable, with a force over the assign; force of an x
to both kinds of variable; and the container of the statements (an
initial procedure, a static task, an always procedure and a branch of a
fork).

The expected outputs follow from SCHEDULER_SPEC.md 14 (10.6.1, 10.6.2),
modelled below without reading kirimasim's code: a forced target shows the
force's right side, again whenever what it reads changes; procedural
writes and drivers do not change it; a released net takes its drivers'
value at once; a released variable that a continuous assignment drives,
or that an assign still holds, takes that value when it is evaluated
again in the Active region; any other released variable keeps its value
until the next write. A person checks the model once for all the tests
made from it (TEST_SUITE.md, "人の抜き取り").
"""

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "proc"
NAME = "procedural_continuous_assign"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1


class Target:
    """One target of force and release, and how its value follows."""

    def __init__(self, label, spelling, width, kind, driver=None, shown=None):
        self.label = label
        self.spelling = spelling  # the target as written in force and release
        self.width = width
        self.kind = kind  # "var" (procedural), "driven" (continuous assignment) or "net"
        self.driver = driver  # src -> value, for driven variables and nets
        self.shown = shown or spelling  # what the line shows
        self.mask = (1 << width) - 1


# The force's right side reads src: src ^ 8'hff, cut or repeated to the width.
def force_value(t, src):
    v = src ^ 0xFF
    if t.width > 8:
        v = (v << 8) | src
    return v & t.mask


def force_rhs(t):
    if t.width > 8:
        return "{src ^ 8'hff, src}"
    return "src ^ 8'hff"


TARGETS = [
    Target("var", "v", 8, "var"),
    Target("bit2", "b2", 8, "var"),
    Target("driven var", "cv", 8, "driven", lambda s: (s + 1) & 0xFF),
    Target("net", "w", 8, "net", lambda s: s),
    Target("resolved net", "rw", 8, "net", lambda s: s),
    Target("hier net", "u.n", 8, "net", lambda s: s ^ 0x0F),
    Target("generate var", "gen.g", 8, "var"),
    Target("concat", "{cv2, w2}", 16, "concat",
           lambda s: (((s + 2) & 0xFF) << 8) | (s & 0xFF)),
]
# A part and a bit of a net, shown as the whole net (8 bits): (label,
# spelling, lsb, width).
PARTS = [("net part", "wp[5:2]", 2, 4), ("net bit", "wb[7]", 7, 1)]


def run(container, pad, delay="#1"):
    """The statements of one container and the lines they show."""
    sv, exp = [], []

    # Every target: the same steps, the model below gives the values.
    for t in TARGETS:
        src = 0x12
        sv.append(f"{pad}src = 8'h{src:02x};")
        if t.kind == "var":
            sv.append(f"{pad}{t.spelling} = 8'h34;")
            visible = 0x34
        else:
            visible = t.driver(src)
        lines = []

        def step(tag, value):
            sv.append(f"{pad}{delay} $display(\"{container} {t.label} {tag} %h\", {t.shown});")
            lines.append(f"{container} {t.label} {tag} {value:0{(t.width + 3) // 4}x}")

        step("start", visible)
        sv.append(f"{pad}force {t.spelling} = {force_rhs(t)};")
        visible = force_value(t, src)
        step("forced", visible)
        src = 0x56
        sv.append(f"{pad}src = 8'h{src:02x};")
        visible = force_value(t, src)
        step("follows", visible)
        if t.kind == "var":
            sv.append(f"{pad}{t.spelling} = 8'h78;")
        step("written", visible)
        sv.append(f"{pad}release {t.spelling};")
        # Read at once: a net takes its drivers' value now; a driven
        # variable keeps the forced value until the Active region.
        if t.kind == "net":
            visible = t.driver(src)
        sv.append(f"{pad}$display(\"{container} {t.label} released %h\", {t.shown});")
        lines.append(f"{container} {t.label} released {visible:0{(t.width + 3) // 4}x}")
        if t.kind in ("driven", "concat"):
            # The concatenation holds a driven variable and a net: the net
            # part changes at once, the variable part in the Active region.
            if t.kind == "concat":
                now = (visible & 0xFF00) | (t.driver(src) & 0xFF)
                lines[-1] = f"{container} {t.label} released {now:04x}"
            visible = t.driver(src)
        step("settled", visible)
        src = 0x9A
        sv.append(f"{pad}src = 8'h{src:02x};")
        if t.kind == "var":
            sv.append(f"{pad}{t.spelling} = 8'hbc;")
            visible = 0xBC
        else:
            visible = t.driver(src)
        step("after", visible)
        exp += lines

    # A part and a bit of a net: the other bits keep following the driver.
    for label, spelling, lsb, width in PARTS:
        net = spelling.split("[")[0]
        src = 0x12
        sv.append(f"{pad}src = 8'h{src:02x};")
        mask = ((1 << width) - 1) << lsb
        lines = []

        def show_net(tag, value):
            sv.append(f"{pad}{delay} $display(\"{container} {label} {tag} %h\", {net});")
            lines.append(f"{container} {label} {tag} {value:02x}")

        show_net("start", src)
        sv.append(f"{pad}force {spelling} = {width}'b{'1' * width};")
        show_net("forced", src | mask)
        src = 0x40
        sv.append(f"{pad}src = 8'h{src:02x};")
        show_net("follows", src | mask)
        sv.append(f"{pad}release {spelling};")
        show_net("released", src)
        exp += lines

    # assign and deassign, and a force over the assign.
    lines = []

    def show_a(tag, value):
        sv.append(f"{pad}{delay} $display(\"{container} assign {tag} %h\", av);")
        lines.append(f"{container} assign {tag} {value:02x}")

    sv.append(f"{pad}src = 8'h12;")
    sv.append(f"{pad}av = 8'h11;")
    show_a("start", 0x11)
    sv.append(f"{pad}assign av = src;")
    show_a("assigned", 0x12)
    sv.append(f"{pad}src = 8'h34;")
    show_a("follows", 0x34)
    sv.append(f"{pad}av = 8'h55;")
    show_a("written", 0x34)
    sv.append(f"{pad}force av = 8'haa;")
    show_a("forced", 0xAA)
    sv.append(f"{pad}src = 8'h56;")
    show_a("forced follows", 0xAA)
    sv.append(f"{pad}release av;")
    sv.append(f"{pad}$display(\"{container} assign released %h\", av);")
    lines.append(f"{container} assign released aa")
    show_a("reassigned", 0x56)
    sv.append(f"{pad}deassign av;")
    sv.append(f"{pad}src = 8'h78;")
    show_a("deassigned", 0x56)
    sv.append(f"{pad}av = 8'h99;")
    show_a("written again", 0x99)
    exp += lines

    # An x forced into a four-state and a two-state variable.
    sv.append(f"{pad}force xv = 8'bx;")
    sv.append(f"{pad}force xb = 8'bx;")
    sv.append(f"{pad}{delay} $display(\"{container} x %b %b\", xv, xb);")
    exp.append(f"{container} x xxxxxxxx 00000000")
    sv.append(f"{pad}release xv;")
    sv.append(f"{pad}release xb;")
    return sv, exp


def files():
    sv = (f"/*\n:name: {NAME}\n:description: force and release of variables, nets, parts and "
          "bits of nets, concatenations and hierarchical names, assign and deassign, with "
          "right sides that follow what they read, in several containers\n:tags: proc\n"
          ":type: simulation\n:lrm: 1800-2023 10.6.1 10.6.2 23.6\n:oracle: spec\n"
          ":spec_check: ai\n:fourstate: yes\n:nondeterministic: no\n"
          f":generator: gen_override_matrix.py@{VERSION}\n"
          f":expected_stdout: @{NAME}.expected\n*/\n"
          "// Generated by tools/gen_override_matrix.py; change the generator, not this file.\n"
          "// Icarus Verilog 13.0 and Verilator 5.052 give a released variable that a\n"
          "// continuous assignment drives, or that an assign holds again, its value at once;\n"
          "// 10.6.2 has it evaluated again in its region, so the \"released\" lines of such a\n"
          "// variable here show the forced value (SCHEDULER_SPEC.md 14.3). Icarus 13.0 also\n"
          "// rejects force of a variable that a continuous assignment drives and evaluates\n"
          "// the right side of force and assign only once; Verilator 5.052 keeps no x, and\n"
          "// after a release over an assign, deassign shows the forced value again.\n"
          "module child(input logic [7:0] in);\n  wire [7:0] n;\n  assign n = in ^ 8'h0f;\n"
          "endmodule\n"
          "module top;\n"
          "  logic [7:0] src, v, av, xv;\n  bit [7:0] b2, xb;\n"
          "  logic [7:0] cv, cv2;\n  assign cv = src + 1;\n  assign cv2 = src + 2;\n"
          "  wire [7:0] w, w2, wp, wb;\n  assign w = src;\n  assign w2 = src;\n"
          "  assign wp = src;\n  assign wb = src;\n"
          "  wire [7:0] rw;\n  assign rw = src;\n  assign (weak1, weak0) rw = 8'h0f;\n"
          "  child u (src);\n"
          "  generate\n    if (1) begin : gen\n      logic [7:0] g;\n    end\n  endgenerate\n"
          "  logic go = 1'b0;\n")
    exp = []
    body, e = run("initial", "    ")
    exp += e
    steps = sum(1 for line in body if "#1" in line)
    sv += "  initial begin\n" + "\n".join(body) + "\n"
    sv += f"    run();\n    go = 1'b1;\n    #{steps + 1};\n    fork\n      begin\n"
    fork_body, fork_exp = run("fork", "        ")
    sv += "\n".join(fork_body) + "\n      end\n    join\n  end\n"
    task_body, task_exp = run("task", "    ")
    sv += "  task static run();\n" + "\n".join(task_body) + "\n  endtask\n"
    always_body, always_exp = run("always", "    ")
    sv += "  always @(posedge go) begin\n" + "\n".join(always_body) + "\n  end\nendmodule\n"
    exp += task_exp + always_exp + fork_exp
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
                print(f"gen_override_matrix: {path.relative_to(OUT.parent.parent.parent)} is out "
                      "of date; run tools/gen_override_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
