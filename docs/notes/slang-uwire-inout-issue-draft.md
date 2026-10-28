# Draft: `uwire` is rejected as an `inout` port

Status: **draft, not posted**, 2026-10-28. English only. This is a draft of a
report to slang. It asks whether slang's `InOutUWirePort` / `InOutUWireConn`
diagnostics are based on a clause that I have not found. It does not ask to
suppress a warning.

## Observed setup

- Unmodified slang commit `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  (master, 2026-10-27), pinned by kirimasim.
- kirimasim `a2420294775a`, Linux x86_64 (WSL2), Clang 20.1.8.
- Observed through kirimasim, which uses slang's driver. A standalone slang
  tool was not built for this note.
- Icarus Verilog 13.0 and Verilator 5.050 were used only to compare behavior.

## Reproducer

Save as `uwire_inout.sv`:

```systemverilog
module uw_cell(w, d);
  inout w;
  input d;
  uwire w;
  assign w = d;
endmodule

module top;
  uwire n;
  logic d = 1'b1;
  uw_cell u(n, d);
  initial #1 $display("%b", n);
endmodule
```

Run:

```sh
kirimasim uwire_inout.sv
```

Observed (two errors; the third is a consequence of the first):

```text
uwire_inout.sv:2:9: error: 'uwire' port 'w' cannot have direction 'inout'
uwire_inout.sv:11:13: error: 'uwire' net 'n' cannot be connected to 'inout' port
uwire_inout.sv:5:10: error: 'uwire' net 'w' cannot have multiple drivers
```

Icarus Verilog 13.0 and Verilator 5.050 both compile and run the same file and
print `1`.

## What I expected

I could not find a rule that forbids an `inout` port of `uwire` net type.
6.6.2 restricts `uwire` in two ways that I can see: it is an error to connect
one bit to more than one driver, and it is an error to connect it to a
bidirectional terminal of a bidirectional pass switch. 23.3.3.6 asks for a
warning when the two sides of a `uwire` port are not merged into one net; it
does not forbid the port. 23.3.3.3 allows an `inout` port to connect to a net.

The design above has one driver, the internal `assign w = d`, and the port is
the only path to the parent net. If the standard does forbid this, please point
to the clause; if it does not, the two diagnostics are too strict.

## Localization

- `scripts/diagnostics.txt`: `InOutUWirePort` ("'uwire' port '{}' cannot have
  direction 'inout'") and `InOutUWireConn` ("'uwire' net '{}' cannot be
  connected to 'inout' port").
- `source/ast/symbols/PortSymbols.cpp` emits `InOutUWirePort` for ANSI and
  non-ANSI ports.
- `source/ast/expressions/MiscExpressions.cpp` emits `InOutUWireConn` when an
  `inout` port connection drives a `uwire`.
- `CHANGELOG.md` says "Correctly disallow `inout` uwire ports" (an older
  entry). This note questions the basis of that change.

## Duplicate search

A search of the upstream issue tracker for "uwire inout" and "uwire port" on
2026-10-28 found no matching issue. The search is not proof that no duplicate
exists.

## Open questions

1. Is there a clause outside 6.6.2 and 23.3.3 that forbids `inout` on a
   `uwire` port?
2. If not, should `InOutUWirePort` and `InOutUWireConn` be removed, or limited
   to the case where the connection would create a second driver?
3. Does the answer change for a `uwire` port with a delay or a strength?

No issue or comment has been sent.
