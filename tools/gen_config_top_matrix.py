#!/usr/bin/env python3
"""Generates the matrix tests of the top taken from a config in the
sources, of TEST_SUITE.md ("Mechanism matrix"), in tests/suite/hier/.

  tools/gen_config_top_matrix.py           # writes the files
  tools/gen_config_top_matrix.py --check   # exit 1 if a file differs from what it would write

In a single-pass tool, a config is given by putting its source among the
files read, and the cell its design statement names is the top, whatever
other cells are left uninstantiated (33.5.4). Without --top, one config
in the sources gives the top; with more than one, the standard does not
say which is meant, so kirimasim stops with an error at the second and
asks for --top (CLI.md 2.1, UNSPECIFIED_BEHAVIOR.md N9). --top names a
module or a config, a module first when both have the name, and
<name>:config names the config.

The axes: the rule of the config (none beyond the design statement, a
default liblist, a cell clause, an instance clause, parameters set by an
instance clause, two top cells); where the config stands (before or
after the modules); how the top is chosen (no --top, --top naming the
config, --top naming a module); two configs; a config with the name of a
module. Every file has a module that nothing instantiates, which must
not run when a config gives the top. The design statement names its
cells with and without the library (work, 33.3.1, 33.4.1.1).

Parameters are set with "use #(...)", the form of the examples of
33.4.3; the syntax of 33.4.1 writes the assignments without "#( )".

Each top prints the kind and the parameter of its instances from one
initial procedure, read through hierarchical names of their localparams,
so that the output does not depend on the order of processes. The
expected outputs follow from the rules above, computed below. A person
checks them once for all the tests made from them (TEST_SUITE.md,
"Sampling by people").
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "tests" / "suite" / "hier"
PREFIX = "config_top_"
# The version of the rules below (TEST_SUITE.md, :generator:). Raise it
# whenever they change.
VERSION = 1

LEAVES = """module leaf_a #(parameter int P = 1);
  localparam string KIND = "leaf_a";
endmodule
module leaf_b #(parameter int P = 1);
  localparam string KIND = "leaf_b";
endmodule
module top;
  leaf_a u1();
  leaf_a u2();
  initial $display("u1=%s P=%0d u2=%s P=%0d", u1.KIND, u1.P, u2.KIND, u2.P);
endmodule
module top2;
  initial #1 $display("top2 ran");
endmodule
module stray;
  initial $display("stray ran");
endmodule"""

# (case, description, configs as (name, design, rules), whether the
# configs come after the modules, :top_module: or None, (u1 kind, u1 P,
# u2 kind, u2 P) or None for the error, whether top2 runs)
CASES = [
    ("design_only", "a config with only a design statement gives the top",
     [("cfg", "top", [])], False, None, ("leaf_a", 1, "leaf_a", 1), False),
    ("default_liblist", "a config with a default liblist gives the top",
     [("cfg", "work.top", ["default liblist work;"])], False, None,
     ("leaf_a", 1, "leaf_a", 1), False),
    ("cell_use", "a cell clause of the config binds both instances",
     [("cfg", "work.top", ["cell leaf_a use leaf_b;"])], False, None,
     ("leaf_b", 1, "leaf_b", 1), False),
    ("instance_use", "an instance clause of the config binds one instance",
     [("cfg", "top", ["instance top.u2 use leaf_b;"])], False, None,
     ("leaf_a", 1, "leaf_b", 1), False),
    ("instance_parameter", "an instance clause of the config sets a parameter",
     [("cfg", "work.top", ["instance top.u1 use #(.P(7));"])], False, None,
     ("leaf_a", 7, "leaf_a", 1), False),
    ("two_cells", "a design statement with two cells gives two tops",
     [("cfg", "top work.top2", [])], False, None, ("leaf_a", 1, "leaf_a", 1), True),
    ("after_modules", "a config after the modules gives the top",
     [("cfg", "top", ["cell leaf_a use leaf_b;"])], True, None,
     ("leaf_b", 1, "leaf_b", 1), False),
    ("named_config", "--top naming the config gives the same top",
     [("cfg", "top", ["cell leaf_a use leaf_b;"])], False, "cfg",
     ("leaf_b", 1, "leaf_b", 1), False),
    ("named_module", "--top naming a module leaves the config unused",
     [("cfg", "top", ["cell leaf_a use leaf_b;"])], False, "top",
     ("leaf_a", 1, "leaf_a", 1), False),
    ("same_name", "a config with the name of a module gives the top",
     [("top", "work.top", ["cell leaf_a use leaf_b;"])], False, None,
     ("leaf_b", 1, "leaf_b", 1), False),
    ("same_name_module", "--top with the name of a module and of a config takes the module",
     [("top", "work.top", ["cell leaf_a use leaf_b;"])], False, "top",
     ("leaf_a", 1, "leaf_a", 1), False),
    ("same_name_config", "--top with <name>:config takes the config of that name",
     [("top", "work.top", ["cell leaf_a use leaf_b;"])], False, "top:config",
     ("leaf_b", 1, "leaf_b", 1), False),
    ("two_configs_named", "--top chooses one of two configs",
     [("cfg_a", "top", ["cell leaf_a use leaf_b;"]),
      ("cfg_b", "top", ["instance top.u2 use leaf_b;"])], False, "cfg_b",
     ("leaf_a", 1, "leaf_b", 1), False),
    ("two_configs", "two configs and no --top: the standard does not say which is meant",
     [("cfg_a", "top", ["cell leaf_a use leaf_b;"]),
      ("cfg_b", "top", ["instance top.u2 use leaf_b;"])], False, None, None, False),
]


def clauses(configs):
    """The clauses a test rests on: the top (33.5.4) and the design
    statement (33.4.1.1), and those of the rules it uses."""
    rules = " ".join(r for _, _, rs in configs for r in rs)
    found = {"33.5.4", "33.4.1.1"}
    for word, numbers in (("liblist", ["33.4.1.5"]), ("cell ", ["33.4.1.4", "33.4.1.6"]),
                          ("instance ", ["33.4.1.3", "33.4.1.6"]), ("#(", ["33.4.3"])):
        if word in rules:
            found.update(numbers)
    return sorted(found, key=lambda c: [int(n) for n in c.split(".")])


def config_text(name, design, rules):
    return "\n".join([f"config {name};", f"  design {design};"] +
                     [f"  {r}" for r in rules] + ["endconfig"])


def files():
    out = {}
    for case, description, configs, after, top, result, top2 in CASES:
        name = PREFIX + case
        header = [
            "/*",
            f":name: {name}",
            f":description: {description}",
            ":tags: hierarchy configurations",
            ":type: simulation" if result is not None else ":type: elaboration",
            ":lrm: 1800-2023 " + " ".join(clauses(configs)),
            ":oracle: spec",
            ":spec_check: ai",
            ":fourstate: no",
            ":nondeterministic: no",
            f":generator: gen_config_top_matrix.py@{VERSION}",
        ]
        if top is not None:
            header.append(f":top_module: {top}")
        body_configs = "\n".join(config_text(*c) for c in configs)
        comment = ["*/",
                   "// Generated by tools/gen_config_top_matrix.py; change the generator, not "
                   "this file."]
        if result is None:
            header.append(f":should_fail_because: {description}")
            header.append(":expected_error: {where}: error: a second config "
                          f"'{configs[1][0]}' in the sources, after '{configs[0][0]}'; name the "
                          "one to use with --top")
        else:
            header.append(f":expected_stdout: @{name}.expected")
        parts = [body_configs, LEAVES] if not after else [LEAVES, body_configs]
        text = "\n".join(header + comment + parts) + "\n"
        if result is None:
            # The error stands at the second config's name, after "config ".
            second = f"config {configs[1][0]};"
            line = text.splitlines().index(second) + 1
            text = text.replace("{where}", f"{line}:8")
        out[f"{name}.sv"] = text
        if result is not None:
            k1, p1, k2, p2 = result
            expected = [f"u1={k1} P={p1} u2={k2} P={p2}"]
            if top2:
                expected.append("top2 ran")
            out[f"{name}.expected"] = "\n".join(expected) + "\n"
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    bad = 0
    for name, text in files().items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                print(f"gen_config_top_matrix: {path.relative_to(OUT.parent.parent.parent)} is "
                      "out of date; run tools/gen_config_top_matrix.py", file=sys.stderr)
                bad += 1
        else:
            path.write_text(text, encoding="utf-8")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
