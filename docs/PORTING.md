# Porting a Simulation to kirimasim

[日本語](PORTING.ja.md) · The English version is authoritative when the versions differ.

This guide moves a source-based simulation workflow from Icarus Verilog,
Verilator or Lyra to kirimasim. Source baseline: `5e8ab597`; checked on
**2026-10-29**. P1 is unfinished. Shared option names do not establish full
command-line or language compatibility; consult [P1 Support](P1_SUPPORT.md)
for the exact source forms you need.

Build kirimasim as described in the [User Manual](USER_MANUAL.md).
The runnable examples below use a development build and the repository root
as their working directory. Other tools are optional comparison tools;
record their installed versions before using their results.

## Inventory the existing workflow

Record the top name, ordered source list, include directories, macro values,
module libraries, parameter settings, timescales, runtime arguments and data
files. Also identify work performed outside SystemVerilog: a C++ harness,
VPI/DPI modules, cocotb, build scripts or a project configuration.

For each tool, retain its version output and the exact compile and run
commands. For kirimasim, start with:

```sh
build/dev/src/driver/kirimasim --version
build/dev/src/driver/kirimasim --help
```

Separate frontend settings from runtime settings. `-DCOUNTER_WIDTH=8` defines
a preprocessing macro; `+cycles=6` is read by the running design. Neither is
an implicit override of a module parameter.

## Translate the execution workflow

### From Icarus Verilog

An Icarus compiler/runtime workflow for the supplied counter is:

```sh
iverilog -g2012 -s tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -o counter.vvp docs/examples/user_manual/counter.sv \
  docs/examples/user_manual/tb.sv
vvp counter.vvp +cycles=6
```

With kirimasim, compile and run in one invocation:

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv +cycles=6
```

Replace Icarus top selection `-s tb` with `--top=tb`. Remove the compiled
output argument `-o counter.vvp` and the separate `vvp` invocation; kirimasim
does not consume VVP files or produce reusable compiled models. It uses an
IEEE 1800-2023 frontend; removing `-g2012` does not select a 2012 language mode.
The documented compiler and runtime forms are in the
[Icarus flags](https://steveicarus.github.io/iverilog/usage/command_line_flags.html)
and [VVP flags](https://steveicarus.github.io/iverilog/usage/vvp_flags.html).

For `$stop` policy, the counterpart of a noninteractive `vvp -n` run is
kirimasim's default `--stop=finish`; use `--stop=error` to make `$stop` fail,
as with `vvp -N`. These map the `$stop` policy only. In kirimasim, SIGINT
still has its own interruption behavior and exit code 130.

### From Verilator

For a SystemVerilog testbench that supplies its own clock and stimulus,
the corresponding Verilator binary workflow is:

```sh
verilator --binary --timing --top-module tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv
./obj_dir/Vtb +cycles=6
```

Use the kirimasim invocation above. Change `--top-module tb` to `--top=tb`;
the compiler-build flags and generated executable step are replaced by
kirimasim's source execution. The upstream
[argument reference](https://verilator.org/guide/latest/exe_verilator.html)
documents this binary workflow and its options.

A workflow using a C++ wrapper needs additional work. kirimasim has no
Verilated-model interface, `eval()` loop or replacement for host C++ source
arguments. Identify the wrapper's reset, clock, stimulus and checking behavior
and express what can be supported in a SystemVerilog testbench. VPI/DPI-C and
cocotb integration remain unavailable. See the upstream
[model connection documentation](https://verilator.org/guide/latest/connecting.html)
for the interface whose use must be identified.

### From Lyra

The [official README at `6069311ba779`](https://github.com/hankhsu1996/lyra/blob/6069311ba779a8d90a8f970d3b46c6ba13a05b30/README.md),
checked directly on 2026-10-29, describes a source-run form of this shape:

```text
lyra run --top top design.sv
```

The kirimasim command shape is:

```text
kirimasim --top=top design.sv
```

These are illustrative forms, not a verified Lyra execution example.
The README documents forwarding frontend arguments to slang; check the
installed `lyra --help` for the accepted forms. Lyra is not installed in the
validation environment; its executable version, this command's execution
and the full frontend-option mapping are **unconfirmed**. Run the same small
original design on the installed build to verify them. Translate settings
from `lyra.toml` into explicit sources and options; kirimasim has no Lyra
project reader or `run`, `check`, `build`, `emit cpp` or `dump` subcommands.
`--dump-sir` produces kirimasim's own diagnostic representation.

## Translate inputs explicitly

- **Includes:** use `-I<dir>` or `-I <dir>`, or the existing `+incdir+<dir>`.
  kirimasim searches the including file's directory first, then added paths
  in user order. Preserve ordering when headers share a name.
- **Macros:** use `-DNAME=value` with an explicit value for shared commands.
  kirimasim's bare `-DNAME` means `1`; `-DNAME=` is empty. Its pinned
  frontend keeps the first predefinition of a repeated name. Avoid depending
  on identical defaults or duplicate-definition behavior in another tool.
- **Module libraries:** use `-y<dir>`, `-y <dir>` or `--libdir=<dir>`.
  `+libext+.custom` adds an extension while retaining `.v` and `.sv`.
  In kirimasim, library paths and include paths have separate roles.
  Verilator's documented `-y` also searches includes; supply an explicit
  `-I` when moving a command that relied on that behavior.
- **File lists:** use `-f <file>` with frontend arguments. kirimasim resolves
  its paths from the process working directory. It has no direct `-F`
  spelling; rewrite file-directory-relative lists for the chosen working
  directory. Keep runtime options and runtime plusargs on the actual command
  line, outside frontend lists. Do not copy shell commands into a list.
- **Tops and parameters:** select `--top=<name>` explicitly. Icarus `-P`
  and Verilator `-G` are not kirimasim driver options. Use a source wrapper
  with supported parameter overrides when necessary; a macro works only if
  the source actually uses that macro.
- **Data files:** relative `$fopen` and `$readmem*` paths use the working
  directory. Include and module-library search paths do not relocate data.

For example, load `counter.sv` through a module library:

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -y docs/examples/user_manual docs/examples/user_manual/tb.sv +cycles=6
```

The [Reference Manual](REFERENCE_MANUAL.md#input-options) is the source for
accepted forms and kirimasim's lookup rules. The linked Verilator argument
reference documents its `-f`, `-F` and `-y` behavior; no cross-tool file-list
parser equivalence is implied.

## Check frontend acceptance, lowering and execution

Run each stage separately while isolating a problem:

```sh
build/dev/src/driver/kirimasim parse --top=tb \
  -f docs/examples/user_manual/files.f
build/dev/src/driver/kirimasim --dump-sir --top=tb \
  -f docs/examples/user_manual/files.f > migrated.sir
build/dev/src/driver/kirimasim --max-time=100ns --max-wall-time=5 \
  --stats=json --wave=migrated.vcd --top=tb \
  -f docs/examples/user_manual/files.f > migrated.out 2> migrated.err
```

`parse` checks the frontend; it does not prove that every construct can be
lowered or executed. The SIR dump cannot be run as an input model. Inspect
the first positioned error, the output files and the actual exit code.
Successful simulation of this file-list example prints the default
`width=4 cycles=4`, creates VCD and ends with reason `finish`.

`--max-time` bounds simulation time; `--max-wall-time` bounds elapsed
simulation seconds at safe points, excluding compilation. A bound is a
termination policy, not proof that a test passed. Use `--max-time-ok` only
when an intentional `time_limit` is a successful outcome for your script.

## Compare the design's behavior

The counter commands with width 8 and six cycles should print these design
lines in Icarus, Verilator and kirimasim:

```text
width=8 cycles=6
t=20 ns q=1
t=30 ns q=2
t=40 ns q=3
t=50 ns q=4
t=60 ns q=5
t=70 ns q=6
```

Tool banners and termination reports are separate from these design lines;
their destinations and wording can differ. kirimasim normally puts its
banner and diagnostics on stderr. With `--stats=json`, its last stdout line
is a JSON object, so do not parse the whole stdout stream as one JSON document.

Before treating a mismatch as a simulator defect, align timescales and
precision, reset/initialization, four-state expectations, scheduling and
race assumptions, random seeding and file inputs. kirimasim preserves
four-state values by default; compare the exact policy of the other build
and its options. Equal numeric seeds do not establish identical random
streams across tools. Waveform hierarchy, identifiers and intermediate
transitions also need not be byte-identical.

Make self-checks fail with `$fatal`; kirimasim's `$error` alone does not force
a nonzero exit code. `$finish` / `$stop` arguments are reporting levels,
not OS exit codes. See the reference's
[termination contract](REFERENCE_MANUAL.md#termination-and-exit-codes).

## Keep compatibility evidence scoped

The supplied counter workflow is checked with kirimasim source `5e8ab597`,
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)` and Verilator
`5.052 2026-09-05 rev vUNKNOWN-built20260905`. This establishes the displayed
design samples for those commands, not general parity or Icarus 13.0 parity.
The external documentation links were checked on 2026-10-29; their moving
pages may describe a different build from an installed executable.

Lyra execution remains unconfirmed as explained above. Wider language
coverage must be checked in [P1 Support](P1_SUPPORT.md) and the dated
[Icarus comparison](notes/icarus-13-comparison.md). Class objects, broader
clocking, concurrent assertions, coverage, VPI/DPI-C and FST output remain
unsupported or incomplete; shared input flags do not enable those features.
Record versions, original inputs, options, stdout, stderr and exit codes
when reporting a new compatibility observation.
