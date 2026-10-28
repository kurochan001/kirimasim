# Draft: `defparam` is rejected for a program parameter and for a named-block parameter

Status: **draft, not posted**, 2026-10-28. English only. This is a draft of a
report to slang. It asks whether two parameter scopes are classified correctly.
It does not ask to change the rule that a `localparam` cannot be a `defparam`
target.

## Observed setup

- Unmodified slang commit `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  (master, 2026-10-27), pinned by kirimasim.
- kirimasim `a2420294775a`, Linux x86_64 (WSL2), Clang 20.1.8.
- Observed through kirimasim, which uses slang's driver. A standalone slang
  tool was not built for this note.
- Icarus Verilog 13.0 was used for the program-parameter case.

## Reproducer 1: a program parameter

Save as `defparam_prog.sv`:

```systemverilog
program prg #(parameter int Q = 1) ();
  initial #1 $display("Q=%0d", Q);
endprogram

module top;
  prg p();
  defparam p.Q = 9;
endmodule
```

Run:

```sh
kirimasim defparam_prog.sv
```

Observed:

```text
defparam_prog.sv:7:12: error: cannot reference program item from outside of a program
```

Icarus Verilog 13.0 compiles the file and prints `Q=9`.

## Reproducer 2: a parameter in a named block

Save as `defparam_param.sv`:

```systemverilog
module leaf;
  parameter int P = 0;
endmodule

module top;
  leaf u();
  initial begin : blk
    parameter int BP = 1;
    #1 $display("P=%0d BP=%0d", u.P, BP);
  end
  defparam u.P = 5;
  defparam top.blk.BP = 6;
endmodule
```

Observed:

```text
defparam_param.sv:12:12: error: 'BP' is a localparam and so cannot be the target of a defparam
```

## What I expected

23.10.1 allows a `defparam` to change a value parameter of a module, interface
or program instance through its hierarchical name. The `illegal reference to a
program item` rule of 24.3 is about program **signals** (variables and nets);
a parameter is not a signal. The first reproducer should therefore be legal.

23.10.2 says that a parameter declared in a named block, task or function can
be redefined directly by a `defparam`. `BP` is declared in a named block and is
not a `localparam` in the sense of a parameter that cannot be overridden; the
second reproducer should be legal.

## Localization

- `scripts/diagnostics.txt`: `IllegalReferenceToProgramItem` and the
  `defparam` target diagnostic for a `localparam`.
- The first is emitted where a hierarchical name resolves into a program; the
  check needs to distinguish a parameter from a program signal.
- The second is emitted where the `defparam` target is classified. The
  parameter in a named block is being classified as a `localparam`; 23.10.2
  says it can be a `defparam` target.
- The exact source locations have not been localized beyond the diagnostic
  text.

## Duplicate search

A search of the upstream issue tracker for "defparam program" and
"defparam parameter block" on 2026-10-28 found no matching issue. The search is
not proof that no duplicate exists.

## Open questions

1. Does the program-signal rule of 24.3 intentionally include parameters?
2. Should a parameter in a named block be a `parameter` or a `localparam` for
   the purpose of a `defparam` target? 23.10.2 says it can be redefined by a
   `defparam`.
3. Does the same apply to a parameter in a task or function (the second
   reproducer uses a named block)?

No issue or comment has been sent.
