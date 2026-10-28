# Draft: A read of a checker variable through a hierarchical name is rejected

Status: **draft, not posted**, 2026-10-28. English only. This is a draft of a
report to slang. It asks whether the `CheckerHierarchical` diagnostic is meant
to cover reads as well as writes. It does not ask to relax a write check.

## Observed setup

- Unmodified slang commit `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  (master, 2026-10-27), pinned by kirimasim.
- kirimasim `a2420294775a`, Linux x86_64 (WSL2), Clang 20.1.8.
- Observed through kirimasim, which uses slang's driver. A standalone slang
  tool was not built for this note.
- Icarus Verilog 13.0 and Verilator 5.050 do not implement checkers, so there
  is no simulator oracle for this case.

## Reproducer

Save as `checker_read.sv`:

```systemverilog
checker c(output int v);
  int x;
  assign v = 1;
  assign x = 2;
endchecker

module top;
  int v;
  c u(.v(v));
  initial #1 $display("x=%0d", u.x);
endmodule
```

Run:

```sh
kirimasim checker_read.sv
```

Observed:

```text
checker_read.sv:10:33: error: cannot access checker members hierarchically
checker_read.sv:9:5: note: declared here
```

## What I expected

17.7.1 says that referencing a checker variable using its hierarchical name
**in assignments** is illegal. The example in that subclause shows
`wire x = my_check.a;` and `my_check.a = y;`, both assignments. The reproducer
above only reads `u.x` as an argument of `$display`; it does not assign to it.

The kirimasim test suite also keeps the write forms as illegal-design tests
(`test_illegal_checker_hierarchical_write`,
`test_illegal_checker_hierarchical_read`), so the distinction between a read in
an expression and a read/write in an assignment is already part of its model.

If the intent is that all hierarchical references to a checker variable are
illegal, the wording of 17.7.1 may still be "in assignments"; please confirm
which reading is intended.

## Localization

- `scripts/diagnostics.txt`: `CheckerHierarchical` ("cannot access checker
  members hierarchically").
- `source/ast/Lookup.cpp` emits the diagnostic when a member of a checker is
  reached through a hierarchical name.
- The check currently does not appear to distinguish the read context from the
  assignment context.

## Duplicate search

A search of the upstream issue tracker for "checker hierarchical" and
"checker member hierarchical" on 2026-10-28 found no matching issue. The search
is not proof that no duplicate exists.

## Open questions

1. Does 17.7.1's "in assignments" limit the rule to assignment targets, or is
   it shorthand for every hierarchical reference?
2. If reads are legal, which contexts should remain errors (assignment target,
   `force`, an event control, a port actual)?
3. Should the diagnostic be split into a read form and a write form?

No issue or comment has been sent.
