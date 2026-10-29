#!/usr/bin/env python3
# Copyright (c) 2026 The kirimasim Authors
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Check source-default initialization policies without changing other defaults."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import bounded_run  # noqa: E402


MODES = (None, "x", "zero", "one", "random")
NOTE_PREFIX = "kirimasim: note: --init-values="

CHECKER = '''  string policy = "x";
  function automatic void verify(input logic [128:0] value, input int width);
    logic [128:0] mask;
    mask = '1 >> (129 - width);
    value &= mask;
    if (policy == "x") begin
      if (value !== ('x >> (129 - width))) $fatal(1, "expected source-default X");
    end else if (policy == "zero") begin
      if (value !== 0) $fatal(1, "expected source-default zero");
    end else if (policy == "one") begin
      if (value !== mask) $fatal(1, "expected every source-default bit one");
    end else if (policy == "random") begin
      if ($isunknown(value)) $fatal(1, "random initialization must be known");
    end else $fatal(1, "unknown test policy");
  endfunction
'''


@dataclass
class Case:
    name: str
    source: str
    warnings: tuple = ()

    @property
    def stdout(self):
        return self.name + " passed\n"


def source(name, declarations, body, prefix=""):
    return (prefix + "module top;\n" + CHECKER + declarations + "  initial begin\n"
            '    if (!$value$plusargs("policy=%s", policy)) policy = "x";\n'
            + body + f'    $display("{name} passed");\n'
            "  end\nendmodule\n")


def cases():
    return [
        Case("static", source("static", '''  logic single;
  logic [64:0] wide;
  logic [128:0] wider;
  integer signed_word;
  logic [8:0] memory[2:0][4:3];
  logic [8:0] before_initializers;
  logic [8:0] copied_default = before_initializers;
  logic [6:0] explicit_x = 'x, explicit_z = 'z, explicit_value = 7'h25;
  bit [6:0] two_state;
  real real_default;
  string text_default;
  wire [6:0] floating;
  wire [6:0] driven = 7'h17;
  typedef struct packed { logic [8:0] a; logic [5:0] b; } packed_t;
  packed_t packed_value;
  task static retained(input int phase);
    logic [8:0] local_value;
    if (phase == 0) verify(local_value, 9);
    else if (local_value !== 9'h152) $fatal(1, "static local lifetime");
    local_value = 9'h152;
  endtask
''', '''    verify(single, 1); verify(wide, 65); verify(wider, 129);
    verify(signed_word, 32); verify(packed_value, 15);
    foreach (memory[i,j]) verify(memory[i][j], 9);
    verify(before_initializers, 9); verify(copied_default, 9);
    if (copied_default !== before_initializers) $fatal(1, "static initialization order");
    if (explicit_x !== 7'bxxxxxxx || explicit_z !== 7'bzzzzzzz || explicit_value !== 7'h25)
      $fatal(1, "explicit initializer changed");
    if (two_state !== 0 || real_default != 0.0 || text_default != "")
      $fatal(1, "non-four-state default changed");
    if (floating !== 7'bzzzzzzz || driven !== 7'h17) $fatal(1, "net default changed");
    retained(0); retained(1);
''')),
        Case("scopes", source("scopes", '''  saved_interface store();
  if (1) begin : generated
    logic [8:0] value;
  end
''', '''    verify(unit_value, 5); verify(saved_package::value, 7);
    verify(store.value, 9); verify(generated.value, 9);
    verify(saved_class::value, 11);
''', '''logic [4:0] unit_value;
package saved_package;
  logic [6:0] value;
endpackage
interface saved_interface;
  logic [8:0] value;
endinterface
class saved_class;
  static logic [10:0] value;
endclass
''')),
        Case("program", '''program worker;
  logic [8:0] value;
  initial begin
    top.verify(value, 9);
    $display("program passed");
  end
endprogram
module top;
''' + CHECKER + '''  worker test();
  initial begin
    if (!$value$plusargs("policy=%s", policy)) policy = "x";
  end
endmodule
'''),
        Case("automatic", source("automatic", '''  logic [8:0] result;
  task automatic inspect();
    logic [8:0] local_value;
    logic [64:0] words[3];
    verify(local_value, 9);
    foreach (words[i]) verify(words[i], 65);
    local_value = 9'h152;
    words = '{65'h1, 65'h2, 65'h3};
  endtask
  function automatic logic [8:0] recurse(input int depth);
    logic [8:0] local_value;
    logic [8:0] saved = local_value;
    logic [8:0] child;
    verify(local_value, 9);
    if (depth > 0) child = recurse(depth - 1);
    if (local_value !== saved) $fatal(1, "recursive local was overwritten");
    return local_value;
  endfunction
  function automatic logic [8:0] captured_return();
    logic [8:0] snapshot;
    captured_return = 9'h152;
    snapshot = captured_return;
    fork
      begin
        #1;
        if (captured_return !== snapshot || captured_return !== 9'h152)
          $fatal(1, "capture was reinitialized");
      end
    join_none
  endfunction
''', '''    inspect(); inspect();
    result = recurse(3); verify(result, 9);
    fork
      begin
        automatic logic [8:0] fork_value;
        automatic logic [8:0] snapshot = fork_value;
        verify(fork_value, 9);
        #1;
        if (fork_value !== snapshot) $fatal(1, "fork-local lifetime");
      end
    join
    result = captured_return();
    if (result !== 9'h152) $fatal(1, "function return changed");
    #2;
''')),
        Case("capture_defaults", source("capture_defaults", '''  typedef struct {
    logic [3:0] fixed_field = 4'ha;
    logic [8:0] default_field;
  } captured_record_t;
  logic [8:0] scalar_result, copied_scalar;
  captured_record_t record_result, copied_record;
  int completed = 0;
  function automatic void inspect_captured_record(input captured_record_t value);
    if (value.fixed_field !== 4'ha) $fatal(1, "captured member initializer changed");
    verify(value.default_field, 9);
  endfunction
  function automatic logic [8:0] captured_scalar();
    logic [8:0] snapshot = captured_scalar;
    fork
      begin
        #1;
        copied_scalar = captured_scalar;
        verify(copied_scalar, 9);
        if (copied_scalar !== snapshot) $fatal(1, "captured scalar return lifetime");
        completed++;
      end
    join_none
  endfunction
  function static captured_record_t captured_record();
    automatic captured_record_t snapshot = captured_record;
    fork
      begin
        #1;
        copied_record = captured_record;
        inspect_captured_record(copied_record);
        if (copied_record.fixed_field !== snapshot.fixed_field ||
            copied_record.default_field !== snapshot.default_field)
          $fatal(1, "captured aggregate return lifetime");
        completed++;
      end
    join_none
  endfunction
''', '''    scalar_result = captured_scalar(); verify(scalar_result, 9);
    scalar_result = captured_scalar(); verify(scalar_result, 9);
    record_result = captured_record(); inspect_captured_record(record_result);
    record_result = captured_record(); inspect_captured_record(record_result);
    #2;
    if (completed != 4) $fatal(1, "captured return children did not finish");
''')),
        Case("formals", source("formals", '''  logic [8:0] value;
  task automatic automatic_output(output logic [8:0] result);
    verify(result, 9);
    result = 9'h152;
  endtask
  function automatic logic [8:0] automatic_return();
    verify(automatic_return, 9);
  endfunction
  task static static_output(output logic [8:0] result, input int phase);
    if (phase == 0) verify(result, 9);
    if (phase == 1) result = 9'h152;
    if (phase == 2 && result !== 9'h152) $fatal(1, "static output lifetime");
  endtask
  function static logic [8:0] static_return(input int phase);
    if (phase == 0 || phase == 2) verify(static_return, 9);
    if (phase == 1) static_return = 9'h152;
  endfunction
''', '''    value = 'z;
    automatic_output(value);
    if (value !== 9'h152) $fatal(1, "automatic output copyback");
    automatic_output(value);
    value = automatic_return(); verify(value, 9);
    value = automatic_return(); verify(value, 9);
    static_output(value, 0); verify(value, 9);
    static_output(value, 1); static_output(value, 2);
    if (value !== 9'h152) $fatal(1, "static output copyback");
    value = static_return(0); verify(value, 9);
    value = static_return(1);
    if (value !== 9'h152) $fatal(1, "static return copy");
    value = static_return(2); verify(value, 9);
''')),
        Case("records", source("records", '''  typedef struct {
    logic [3:0] fixed_field = 4'ha;
    logic [4:0] default_field;
    bit [2:0] tag = 3'h5;
  } record_t;
  typedef struct {
    logic [6:0] fixed_field = 7'h35;
    record_t nested;
    record_t explicit_nested = '{4'hc, 5'h1b, 3'h6};
    logic [64:0] words[2] = '{65'h12, 65'h15};
  } stored_t;
  stored_t global_record, global_array[2];
  record_t result;
  record_t explicit_record = '{4'bxxxx, 5'bzzzzz, 3'h3};
  function automatic void inspect_record(input record_t value);
    if (value.fixed_field !== 4'ha || value.tag !== 3'h5)
      $fatal(1, "member initializer changed");
    verify(value.default_field, 5);
  endfunction
  task automatic local_record();
    record_t value;
    inspect_record(value);
    value = '{4'h1, 5'h2, 3'h3};
  endtask
  task automatic output_record(output record_t value);
    inspect_record(value);
  endtask
  function automatic record_t return_record();
    inspect_record(return_record);
  endfunction
''', '''    if (global_record.fixed_field !== 7'h35 || global_record.words[0] !== 65'h12 ||
        global_record.words[1] !== 65'h15) $fatal(1, "aggregate member initializer");
    inspect_record(global_record.nested);
    if (global_record.explicit_nested.fixed_field !== 4'hc ||
        global_record.explicit_nested.default_field !== 5'h1b ||
        global_record.explicit_nested.tag !== 3'h6) $fatal(1, "ancestor member initializer");
    foreach (global_array[i]) begin
      inspect_record(global_array[i].nested);
      if (global_array[i].words[1] !== 65'h15) $fatal(1, "array member initializer");
    end
    if (explicit_record.fixed_field !== 4'bxxxx || explicit_record.default_field !== 5'bzzzzz ||
        explicit_record.tag !== 3'h3) $fatal(1, "explicit record initializer");
    local_record(); local_record();
    output_record(result); inspect_record(result);
    output_record(result); inspect_record(result);
    result = return_record(); inspect_record(result);
    result = return_record(); inspect_record(result);
''')),
        Case("const_member", source("const_member", '''  function automatic logic [4:0] const_fn();
    logic [4:0] temporary;
    return temporary;
  endfunction
  typedef struct {
    logic [4:0] explicit_field = const_fn();
    logic [4:0] ordinary;
  } constant_member_t;
  constant_member_t stored, result;
  function automatic void inspect_constant_member(input constant_member_t value);
    if (value.explicit_field !== 5'bxxxxx)
      $fatal(1, "constant member initializer changed");
    verify(value.ordinary, 5);
  endfunction
  task automatic local_constant_member();
    constant_member_t value;
    inspect_constant_member(value);
    value = '{5'h12, 5'h13};
  endtask
  task automatic output_constant_member(output constant_member_t value);
    inspect_constant_member(value);
  endtask
  function automatic constant_member_t return_constant_member();
    inspect_constant_member(return_constant_member);
  endfunction
''', '''    inspect_constant_member(stored);
    local_constant_member(); local_constant_member();
    output_constant_member(result); inspect_constant_member(result);
    output_constant_member(result); inspect_constant_member(result);
    result = return_constant_member(); inspect_constant_member(result);
    result = return_constant_member(); inspect_constant_member(result);
''')),
        Case("containers", source("containers", '''  logic [8:0] values[], copied[], nested[][];
  bit [8:0] binary_values[];
  real real_values[];
  string text_values[];
''', '''    values = new[3];
    foreach (values[i]) verify(values[i], 9);
    values[0] = 9'h152; values[1] = 'x; values[2] = 'z;
    values = new[5](values);
    if (values[0] !== 9'h152 || values[1] !== 9'bxxxxxxxxx || values[2] !== 9'bzzzzzzzzz)
      $fatal(1, "dynamic copy prefix changed");
    verify(values[3], 9); verify(values[4], 9);
    values = new[2](values);
    values = new[4](values);
    if (values[0] !== 9'h152 || values[1] !== 9'bxxxxxxxxx) $fatal(1, "resize prefix changed");
    verify(values[2], 9); verify(values[3], 9);
    copied = values;
    foreach (values[i]) if (copied[i] !== values[i]) $fatal(1, "copy was reinitialized");
    nested = new[2];
    if (nested[0].size() != 0 || nested[1].size() != 0) $fatal(1, "nested default must be empty");
    nested[0] = new[2];
    verify(nested[0][0], 9); verify(nested[0][1], 9);
    binary_values = new[3]; real_values = new[2]; text_values = new[2];
    foreach (binary_values[i]) if (binary_values[i] !== 0) $fatal(1, "2-state fresh element");
    foreach (real_values[i]) if (real_values[i] != 0.0) $fatal(1, "real fresh element");
    foreach (text_values[i]) if (text_values[i] != "") $fatal(1, "string fresh element");
''')),
        Case("fallbacks", source("fallbacks", '''  logic [8:0] queue[$], values[], fixed_values[2];
  logic [8:0] popped, word;
  logic [8:0] left[2] = '{9'h15, 9'h21}, right[2] = '{9'h15, 9'h22}, combined[2];
  int left_int[2] = '{11, 12}, right_int[2] = '{11, 13}, combined_int[2];
  logic select = 1'bx;
  int bad_index = 99;
''', '''    values = new[1];
    if (queue.size() != 0) $fatal(1, "queue must start empty");
    if (queue[bad_index] !== 9'bxxxxxxxxx || values[bad_index] !== 9'bxxxxxxxxx ||
        fixed_values[bad_index] !== 9'bxxxxxxxxx || word[bad_index] !== 1'bx)
      $fatal(1, "invalid read default changed");
    popped = queue.pop_front();
    if (popped !== 9'bxxxxxxxxx) $fatal(1, "empty pop default changed");
    queue.push_back(9'h152); queue.push_back('x); queue.push_back('z);
    if (queue[0] !== 9'h152 || queue[1] !== 9'bxxxxxxxxx || queue[2] !== 9'bzzzzzzzzz)
      $fatal(1, "supplied queue elements changed");
    combined = select ? left : right;
    combined_int = select ? left_int : right_int;
    if (combined[0] !== 9'h15 || combined[1] !== 9'bxxxxxxxxx ||
        combined_int[0] !== 11 || combined_int[1] !== 0)
      $fatal(1, "conditional type default changed");
'''), ("pop from an empty queue",)),
    ]


FINGERPRINT = '''module top;
  logic [128:0] first_value, second_value, dynamic_values[], a, b;
  int initialization_draw = $urandom();
  int before_draw, after_draw, legacy_before, legacy_after;
  function automatic logic [128:0] fresh();
    logic [128:0] local_value;
    return local_value;
  endfunction
  initial begin
    before_draw = $urandom(); legacy_before = $random();
    dynamic_values = new[3];
    a = fresh(); b = fresh();
    after_draw = $urandom(); legacy_after = $random();
    $display("draw %08h %08h %08h %08h %08h", initialization_draw,
             before_draw, after_draw, legacy_before, legacy_after);
    $display("bits %h %h %h %h %h", first_value, second_value, dynamic_values[2], a, b);
  end
endmodule
'''

CONDITIONS = '''module top;
  logic flag;
  int score;
  initial begin
    if (flag) score = 1; else score = 2;
    $display("condition %0d", score);
  end
endmodule
'''


def expected_note(mode):
    return (f"{NOTE_PREFIX}{mode}: 4-state variables with no initial value start as "
            f"{mode}, not x (VALUE_SEMANTICS.md 1.4)")


def split_output(result):
    data, stats = [], []
    for line in result.stdout.splitlines():
        if line.startswith("{"):
            stats.append(json.loads(line))
        else:
            data.append(line)
    return "\n".join(data) + ("\n" if data else ""), stats


def note_ok(stderr, mode):
    lines = stderr.splitlines()
    notes = [line for line in lines if line.startswith(NOTE_PREFIX)]
    if mode in (None, "x"):
        return not notes
    versions = [i for i, line in enumerate(lines) if line.startswith("kirimasim: info: kirimasim ")]
    return (notes == [expected_note(mode)] and len(versions) == 1 and
            versions[0] + 1 < len(lines) and lines[versions[0] + 1] == expected_note(mode))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kirimasim", type=Path, required=True)
    executable = str(parser.parse_args().kirimasim.resolve())
    failures = []
    checks = 0
    with tempfile.TemporaryDirectory(prefix="kirimasim-init-values-") as directory:
        root = Path(directory)

        def run(path, *options):
            return bounded_run.run([executable, *options, str(path)], cwd=root, timeout=30)

        def fail(label, result, detail=""):
            failures.append(f"{label}: code={result.returncode}\n{result.stdout}\n{result.stderr}\n{detail}")

        paths = {}
        for case in cases():
            path = root / f"{case.name}.sv"
            path.write_text(case.source)
            paths[case.name] = path
            default = None
            for mode in MODES:
                options = () if mode is None else (f"--init-values={mode}",)
                result = run(path, "--stats=json", "--seed=37", *options,
                             "+policy=" + (mode or "x"))
                output, stats = split_output(result)
                warnings = re.findall(r"(?m)^.+?:\d+:\d+: warning: ([^\n]+)$", result.stderr)
                if (result.returncode or output != case.stdout or len(stats) != 1 or
                        warnings != list(case.warnings) or not note_ok(result.stderr, mode) or
                        ": error:" in result.stderr or ": fatal:" in result.stderr):
                    fail(f"{case.name} {mode}", result, f"expected {case.stdout!r}, {case.warnings!r}")
                if mode is None:
                    default = (output, stats)
                elif mode == "x" and (output, stats) != default:
                    fail(case.name + " explicit x changes default results or counters", result)
                checks += 1

            implicit = run(path, "--dump-sir")
            explicit = run(path, "--dump-sir", "--init-values=x")
            if (implicit.returncode or explicit.returncode or implicit.stdout != explicit.stdout or
                    "default.init " in implicit.stdout or "default-init" in implicit.stdout or
                    NOTE_PREFIX in implicit.stderr or NOTE_PREFIX in explicit.stderr):
                fail(case.name + " default/x SIR", explicit, implicit.stdout)
            checks += 2

        basic = paths["static"]
        instrumented = []
        for mode in ("zero", "one", "random"):
            result = run(basic, "--dump-sir", f"--init-values={mode}")
            if result.returncode or "default-init" not in result.stdout or NOTE_PREFIX in result.stderr:
                fail("enabled SIR " + mode, result)
            instrumented.append(result.stdout)
            checks += 1
        if len(set(instrumented)) != 1:
            failures.append("non-X policies must share one instrumented SIR")

        for mode in ("x", "zero", "one", "random"):
            result = run(basic, "parse", f"--init-values={mode}")
            if result.returncode or result.stdout or NOTE_PREFIX in result.stderr or ": error:" in result.stderr:
                fail("parse " + mode, result)
            checks += 1

        fingerprint = root / "fingerprint.sv"
        fingerprint.write_text(FINGERPRINT)
        fingerprints = {}
        draws = {}
        for seed in (0, 1, 0):
            for mode in MODES:
                options = () if mode is None else (f"--init-values={mode}",)
                result = run(fingerprint, f"--seed={seed}", *options)
                lines = result.stdout.splitlines()
                valid = (not result.returncode and len(lines) == 2 and
                         re.fullmatch(r"draw(?: [0-9a-f]{8}){5}", lines[0]) and
                         re.fullmatch(r"bits(?: [0-9a-fx]{33}){5}", lines[1]) and
                         note_ok(result.stderr, mode) and ": error:" not in result.stderr)
                if not valid:
                    fail(f"fingerprint {seed} {mode}", result)
                else:
                    values = lines[1].split()[1:]
                    key = (seed, mode)
                    if key in fingerprints and fingerprints[key] != values:
                        fail(f"seed {seed} {mode} is not reproducible", result)
                    fingerprints[key] = values
                    if mode is None:
                        draws[seed] = lines[0]
                    elif draws.get(seed) != lines[0]:
                        fail(f"{mode} changed random or urandom streams", result, draws.get(seed, ""))
                    if mode == "random" and (any(not re.fullmatch(r"[01][0-9a-f]{32}", v) for v in values)
                                              or values[3] == values[4]):
                        fail("random values are unknown, unmasked or reused across entries", result)
                checks += 1
        if fingerprints.get((0, "random")) == fingerprints.get((1, "random")):
            failures.append("different seeds produced the same initialization values")

        condition = root / "conditions.sv"
        condition.write_text(CONDITIONS)
        for mode in MODES:
            options = () if mode is None else (f"--init-values={mode}",)
            result = run(condition, "--cond-x=warn", *options)
            unknown = mode in (None, "x")
            warnings = re.findall(r"(?m)^.+?:\d+:\d+: warning: ([^\n]+)$", result.stderr)
            wanted = ["if condition contains X/Z bits (--cond-x=warn)"] if unknown else []
            summary = re.findall(r"condition X check: (\d+) evaluations at (\d+) source positions",
                                 result.stderr)
            wanted_summary = [("1", "1")] if unknown else []
            output_ok = result.stdout == f"condition {1 if mode == 'one' else 2}\n"
            if mode == "random":
                output_ok = result.stdout in ("condition 1\n", "condition 2\n")
            if (result.returncode or not output_ok or warnings != wanted or summary != wanted_summary or
                    not note_ok(result.stderr, mode)):
                fail("condition diagnostics " + str(mode), result, str(wanted))
            checks += 1

        for first, last in (("random", "x"), ("x", "one"), ("one", "zero"), ("zero", "random")):
            result = run(basic, f"--init-values={first}", f"--init-values={last}", "+policy=" + last)
            if result.returncode or result.stdout != cases()[0].stdout or not note_ok(result.stderr, last):
                fail("last option " + last, result)
            checks += 1
        ordinary = run(basic, "--dump-sir")
        last_x = run(basic, "--dump-sir", "--init-values=random", "--init-values=x")
        if (ordinary.returncode or last_x.returncode or ordinary.stdout != last_x.stdout or
                NOTE_PREFIX in last_x.stderr):
            fail("last x restores ordinary SIR", last_x, ordinary.stdout)
        checks += 2
        for value in ("", "0", "1", "X", "rand", "unknown"):
            for command in ((), ("parse",), ("--dump-sir",)):
                result = run(basic, *command, "--init-values=" + value)
                if (result.returncode != 2 or result.stdout or "--init-values" not in result.stderr or
                        "error:" not in result.stderr or NOTE_PREFIX in result.stderr):
                    fail("invalid option " + repr((command, value)), result)
                checks += 1
        result = bounded_run.run([executable, "--help"], cwd=root, timeout=30)
        if result.returncode or "--init-values=x|zero|one|random" not in result.stdout:
            fail("help", result)
        checks += 1

    print(f"init-values: {checks} checks, {len(failures)} failures")
    for failure in failures:
        print(failure)
    return bool(failures)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (bounded_run.OutputLimit, subprocess.TimeoutExpired, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
