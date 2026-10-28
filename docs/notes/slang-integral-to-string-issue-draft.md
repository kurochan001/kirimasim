# Draft: Integral-to-string implicit conversion is rejected in filename contexts

Status: **draft, not posted**, 2026-10-28. English only. This is a draft of a
report to slang. It asks where the conversion for a filename or plusargs key
should happen. It does not ask for a general relaxation of the implicit
conversion rules.

## Observed setup

- Unmodified slang commit `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`
  (master, 2026-10-27), pinned by kirimasim.
- kirimasim `a2420294775a`, Linux x86_64 (WSL2), Clang 20.1.8.
- Observed through kirimasim, which uses slang's driver. A standalone slang
  tool was not built for this note.
- Icarus Verilog 13.0 and Verilator 5.050 were used only to compare behavior.

## Reproducer

Save as `int_string.sv`:

```systemverilog
module top;
  bit [63:0] name = "tmp.txt";
  int fd;
  initial begin
    fd = $fopen(name, "w");
    $display("fd=%0d", fd);
    if (fd) $fclose(fd);
  end
endmodule
```

Run:

```sh
kirimasim int_string.sv
```

Observed:

```text
int_string.sv:5:17: error: no implicit conversion from 'bit[63:0]' to 'string'; explicit conversion exists, are you missing a cast?
```

Icarus Verilog 13.0 and Verilator 5.050 accept the file and call `$fopen`.

The same diagnostic appears for a plusargs key stored in a packed vector and
for the filename argument of `$dumpvars`/`$dumpports`.

## What I expected

21.3.1 says the filename of `$fopen` may be a string literal, a `string`, or an
integral type containing a character string. 21.6 uses the same forms for the
key of `$value$plusargs`. 21.7.1 describes the filename of the VCD tasks in the
same way. The argument is therefore valid in these system tasks even though a
general assignment from an integral to a `string` is not an implicit
conversion.

There are two possible fixes:

1. Let the system task's argument binding accept an integral in these
   positions and convert it to a string, without changing the general
   conversion rules.
2. Allow the implicit conversion generally, if the standard intends that.

The first is narrower and matches the wording of 21.3.1.

## Localization

- `source/ast/expressions/ConversionExpression.cpp`: `convertAssignment`
  reports `NoImplicitConversion` when the target and source are cast compatible
  but not assignment compatible.
- The system task argument binding for `$fopen`, `$value$plusargs` and the VCD
  tasks is in the system subroutine definitions; I have not localized the
  exact place where an integral filename could be accepted.
- `scripts/diagnostics.txt`: `NoImplicitConversion`.

## Duplicate search

A search of the upstream issue tracker for "implicit conversion string" and
"fopen string" on 2026-10-28 found no matching issue. The search is not proof
that no duplicate exists.

## Open questions

1. Should the conversion be part of the general implicit conversion rules, or
   only of the filename and plusargs argument binding?
2. Which contexts are covered by 21.3.1 besides `$fopen` (for example
   `$readmemh`, `$writememh`, `$fdisplay` with a file descriptor)?
3. Should an integral with a non-printable byte or an early NUL be an error?

No issue or comment has been sent.
