#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Generate original aggregate-return lifetime checks (IEEE 6.21/13.4.1)."""

import argparse
from dataclasses import dataclass
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


@dataclass(frozen=True)
class Leaf:
    path: str
    kind: str = "logic"
    width: int = 9
    initial: str | None = None

    def value(self, second=False):
        if self.kind == "real":
            return "-7.5" if second else "3.25"
        if self.kind == "string":
            return '"later value"' if second else '"first value"'
        high = 1 << (self.width - 1)
        value = (high ^ 0xa5) if second else (high | 0x152)
        return f"{self.width}'h{value & ((1 << self.width) - 1):x}"

    def compare(self, left, right, message):
        unequal = "!=" if self.kind in ("real", "string") else "!=="
        return f'if ({left} {unequal} {right}) $fatal(1, "{message}");\n'

    def default(self, expression):
        if self.initial is not None:
            return self.compare(expression, self.initial, "member initializer")
        if self.kind in ("bit", "real", "string"):
            return self.compare(expression, '""' if self.kind == "string" else "0",
                                "source default")
        return (f'if (policy == "x") begin\n'
                + self.compare(expression, f"{self.width}'bx", "source default x")
                + 'end else if (policy == "zero") begin\n'
                + self.compare(expression, f"{self.width}'h0", "source default zero")
                + 'end else if (policy == "one") begin\n'
                + self.compare(expression, f"{self.width}'h{(1 << self.width) - 1:x}",
                               "source default one")
                + f'end else if ($isunknown({expression})) $fatal(1, "source default random");\n')


def shapes():
    return {
        "array": ("typedef logic [8:0] value_t[2:1][0:1];\n",
                  [Leaf(f"[{i}][{j}]") for i in (2, 1) for j in (0, 1)]),
        "record": ("typedef struct { logic [64:0] wide; bit [31:0] known; real number; "
                   "string text; logic flag; } value_t;\n",
                   [Leaf(".wide", width=65), Leaf(".known", "bit", 32),
                    Leaf(".number", "real"), Leaf(".text", "string"), Leaf(".flag", width=1)]),
        "record_array": ("typedef struct { logic [128:0] wide; logic [8:0] words[1:0]; } item_t;\n"
                         "typedef item_t value_t[0:1];\n",
                         [leaf for i in (0, 1) for leaf in
                          (Leaf(f"[{i}].wide", width=129), Leaf(f"[{i}].words[1]"),
                           Leaf(f"[{i}].words[0]"))]),
        "array_record": ("typedef struct { logic [31:0] words[-1:0]; } inner_t;\n"
                         "typedef struct { inner_t nested; logic flag; } value_t;\n",
                         [Leaf(".nested.words[-1]", width=32),
                          Leaf(".nested.words[0]", width=32), Leaf(".flag", width=1)]),
        "initialized": ("typedef struct { logic [8:0] bits=9'h12; string text=\"ready\"; "
                        "logic [64:0] tail; } value_t;\n",
                        [Leaf(".bits", initial="9'h12"),
                         Leaf(".text", "string", initial='"ready"'), Leaf(".tail", width=65)]),
    }


def source(scope, shape, lifetime, exit_style, entry="process"):
    declaration, leaves = shapes()[shape]
    qualifier = "" if lifetime == "implicit" else lifetime + " "
    function = f"function {qualifier}value_t retained(input int phase);\nvalue_t nested;\n"
    for phase, second in ((1, False), (3, True)):
        function += f"if (phase == {phase}) begin\n"
        for i, leaf in enumerate(leaves):
            if second or i % 2 == 0:
                function += f"retained{leaf.path} = {leaf.value(second)};\n"
        function += "end\n"
    function += "if (phase == 4) nested = retained(1);\n"
    if exit_style == "explicit":
        function += "return retained;\n"
    function += "endfunction\n"

    caller = ('string policy="x";\nvalue_t original, first, saved, again, changed, recursive;\n'
              'initial begin\nif (!$value$plusargs("policy=%s",policy)) policy="x";\n'
              'original=retained(0);\n')
    for leaf in leaves:
        caller += leaf.default("original" + leaf.path)
    caller += "first=retained(1); saved=first;\n"
    for i, leaf in enumerate(leaves):
        if i % 2 == 0:
            caller += leaf.compare("first" + leaf.path, leaf.value(), "first assignment")
    caller += f"first{leaves[0].path} = {leaves[0].value(True)};\nagain=retained(2);\n"
    for i, leaf in enumerate(leaves):
        if lifetime == "automatic":
            caller += leaf.default("again" + leaf.path)
        else:
            expected = leaf.value() if i % 2 == 0 else "original" + leaf.path
            caller += leaf.compare("again" + leaf.path, expected, "static return retention")
    caller += "changed=retained(3);\n"
    for i, leaf in enumerate(leaves):
        caller += leaf.compare("changed" + leaf.path, leaf.value(True), "later assignment")
        if i % 2 == 0:
            caller += leaf.compare("saved" + leaf.path, leaf.value(), "detached return value")
    caller += "recursive=retained(4);\n"
    for i, leaf in enumerate(leaves):
        if lifetime == "automatic":
            caller += leaf.default("recursive" + leaf.path)
        else:
            expected = leaf.value() if i % 2 == 0 else leaf.value(True)
            caller += leaf.compare("recursive" + leaf.path, expected, "recursive static return")
    caller += '$display("aggregate lifetime passed");\nend\n'

    if entry == "declaration":
        caller = caller.replace("value_t original, first,", "value_t original=retained(0), first,")
        caller = caller.replace("original=retained(0);\n", "")
    definitions = declaration + function
    if scope == "package":
        return ("package owner;\n" + definitions + "endpackage\nmodule top;\n"
                "import owner::*;\n" + caller + "endmodule\n", 1)
    if scope == "unit":
        return definitions + "module top;\n" + caller + "endmodule\n", 1
    kind = "module" if scope == "generate" else scope
    body = definitions + caller
    if scope == "generate":
        body = "if (1) begin: generated\n" + body + "end\n"
    return (f"{kind} subject;\n" + body + f"end{kind}\n"
            "module top; subject left(); subject right(); endmodule\n", 2)


def cases():
    for scope, shape, lifetime, exit_style, entry in itertools.product(
            ("module", "interface", "program", "generate", "package", "unit"),
            shapes(), ("implicit", "static", "automatic"), ("fallthrough", "explicit"),
            ("process", "declaration")):
        name = "-".join((scope, shape, lifetime, exit_style, entry))
        text, lines = source(scope, shape, lifetime, exit_style, entry)
        yield name, text, lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", required=True)
    parser.add_argument("--only", default="", help="substring of the generated case name")
    parser.add_argument("--record", type=Path)
    args = parser.parse_args()
    executable = str(Path(args.kirimasim).resolve())
    failures = []
    observations = []
    sources = {}
    selected = [c for c in cases() if args.only in c[0]]
    if not selected:
        parser.error("no generated cases selected")
    with tempfile.TemporaryDirectory(prefix="kirimasim-return-lifetime-") as tmp:
        root = Path(tmp)
        for name, text, lines in selected:
            path = root / (name + ".sv")
            path.write_text(text)
            sources[name] = hashlib.sha256(text.encode()).hexdigest()
            for policy in ("x", "zero", "one", "random"):
                try:
                    result = bounded_run.run(
                        [executable, "--seed=37", "--init-values=" + policy,
                         "+policy=" + policy, path.name], cwd=root, timeout=30)
                    passed = (result.returncode == 0 and
                              result.stdout == "aggregate lifetime passed\n" * lines)
                    observations.append({"case": name, "policy": policy,
                                         "exit": result.returncode, "passed": passed})
                    if not passed:
                        failures.append(f"{name} {policy}: exit={result.returncode}\n"
                                        + (result.stdout + result.stderr)[:4096])
                except (subprocess.TimeoutExpired, bounded_run.OutputLimit) as error:
                    observations.append({"case": name, "policy": policy, "passed": False,
                                         "error": type(error).__name__})
                    failures.append(f"{name} {policy}: {error}")
    if args.record:
        args.record.write_text(json.dumps({"sources": sources, "results": observations},
                                          indent=2) + "\n")
    print(f"aggregate return lifetime: {len(sources)} sources, "
          f"{len(observations)} checks, {len(failures)} failures")
    for failure in failures[:12]:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
