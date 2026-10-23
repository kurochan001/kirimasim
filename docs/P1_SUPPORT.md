# P1 supported features

[日本語](P1_SUPPORT.ja.md) · This English text is the main version.

The [approved P1 scope](spec/P1_SCOPE.md) now targets Icarus Verilog
13.0-or-broader execution support, advancing the necessary later-phase
mechanisms. This is a target; unsupported forms below remain unimplemented.
See the [comparison progress](notes/p1-icarus-progress.md) for measured gaps.
Later-phase labels below are subject to this scope amendment.

This inventory describes tier 1 as of 2026-10-23, including the execution
controls below. The `b98744a` baseline predates `--autoflush`, `--stop` and
`--max-wall-time`; `dec94ff` introduced bounds diagnostics. Check
`kirimasim --help` for a build's options and `--version` for its revision.

P1 is in progress. “Supported” below means that the listed forms can be
lowered to SIR and run by the interpreter, with tests in this repository.
It does not promise every combination of those features. Acceptance by
`kirimasim parse` only establishes front-end acceptance, not execution
support. The [specifications](spec/SIR_SPEC.md) also describe planned work;
this document lists what is implemented now.

For runnable examples of recent additions, see [P1 features](P1_FEATURES.md).
For build instructions and completion criteria, see the [README](../README.md).

## Language and execution

| Area | Supported forms | Important limits |
| --- | --- | --- |
| Front end | Preprocessing, parsing, type checking and elaboration through unmodified, pinned slang | slang accepting a construct does not make it executable in kirimasim |
| Hierarchy | Modules; `input`, `output`, `inout`; parameters, `localparam`, `defparam`; generate blocks; instance arrays; hierarchical names | Module `ref` ports and part-select connections to `inout` ports are not implemented |
| Shared declarations | Packages, imports, `$unit`, interfaces and modports | Virtual interfaces and classes are not implemented |
| Configuration | Source-level configs, top selection and bindings; specparams, including in specify blocks | Library map files, specify module paths and timing checks are not supported |
| Integral values | Arbitrary-width 2-state and 4-state values, signed and unsigned arithmetic, X/Z propagation and conversions | Practical memory and execution limits still apply |
| Other scalar values | `real`, `shortreal`, `realtime`, `string`, named events, null `chandle` | Non-null handles need DPI-C, which is not implemented |
| Aggregates | Packed/unpacked structures, packed unions, fixed unpacked arrays, multidimensional arrays, static arrays of structures, unpacked array parameters | Dynamic arrays, queues, associative arrays and unpacked unions are not implemented; array slices are not generally assignable |
| Enumerations | Enum values and `first`, `last`, `num`, `name`, `next`, `prev` | Runtime `next(n)` / `prev(n)` counts are converted to 32-bit unsigned values; see the example guide |
| Expressions | Arithmetic, bitwise, logical, reduction and comparison operations; shifts; ternary expressions; concatenation, replication, casts, `inside`, wildcard equality, assignment operators, increments and assignment patterns | Pattern-matching `matches` forms of `if` and the conditional operator are not implemented |
| Selections | Packed bit/part/indexed-part selects, packed dimensions, fixed unpacked-array indexing, structure members | Writes through a runtime select of a packed member or an already selected part are not implemented |
| Streaming | `>>` and `<<` with constant slice sizes; fixed-array and structure operands; unpacking into integral, fixed-array and structure targets, including widths above 64 bits; blocking and nonblocking assignments | No `with` clause, string target or nested streaming target; automatic targets cannot receive an NBA |
| Branches and loops | `if`, `case`, `casez`, `casex`, `case inside`, `randcase`; `unique`, `unique0`, `priority` reports; `for` (including `for (;;)`), `while`, `do`, `repeat`, `forever`, `foreach`, `break`, `continue`, `return` | `randsequence` and `wait_order` are not implemented |
| Procedures | `initial`, `always`, `always_comb`, `always_latch`, `always_ff`, `final` | `program` blocks are not implemented; `final` cannot wait |
| Assignment and delay | Blocking and nonblocking assignment, supported intra-assignment delays, `#` controls, time units/precision and rounding, min/typ/max selection | Whole-array intra-assignment timing, event-controlled NBAs and delayed NBAs to runtime array elements are not implemented |
| Event controls | Signal edges, integral expressions, `iff`, `wait (expr)`, named events, `->`, `->>`, `.triggered` | No writing side effects in event expressions, `iff` or wait conditions; no runtime-indexed named event, named-event `iff`, event-controlled `->>`, or `always_ff` trigger on a computed expression |
| Concurrency | `fork` / `join`, `join_any`, `join_none`, `wait fork`, `disable fork`, disable of supported named blocks and tasks | Fork-local declarations, escaping automatic variables captured by fork branches and disable of a named fork block have unsupported forms |
| Subroutines | Static/automatic functions and tasks, recursion, default arguments, `input`, `output`, `inout`, `ref`, `const ref`; fixed-array and unpacked-structure arguments/results; tasks that wait | No unpacked-structure `ref` argument, array-of-structures argument, automatic array of structures, or array-slice output/inout actual |
| Nets and resolution | `wire`, `tri`, `wand`, `wor`, `tri0`, `tri1`, `supply0`, `supply1`, `uwire`; multiple drivers and drive strengths | No `trireg`, user-defined nettypes or `interconnect` |
| Continuous drivers | Continuous assignments and their rise/fall/turn-off delays, including net declaration assignments | A bare delayed net declaration such as `wire #5 n;` is not implemented; its delay would apply to every driver |
| Primitives | Built-in logic gates and pulls; combinational and sequential UDPs | No MOS/bidirectional switches or UDP instances with unconnected ports |
| Net aliases | Aliases of compatible whole nets | No partial/concatenated/array aliases, incompatible net types, or merging groups already joined separately by aliases or ports |
| Procedural drivers | `assign` / `deassign`, `force` / `release` on supported integral targets | No real/whole-unpacked-array target or force of a net connected through a port |
| Assertions | Immediate `assert`, `assume`, `cover`; deferred immediate assertions (`#0`, `final`) | No concurrent assertions or module-item deferred assertions; a deferred assertion action cannot call a task that may wait |
| Scheduler | All time-slot regions and the reference scheduling algorithm; pending reports in Observed; strobe/monitor output in Postponed | Region infrastructure does not imply support for program blocks, clocking blocks or concurrent assertions |

`randcase` weights wider than 32 bits are not implemented.

## System tasks, functions and methods

The names in this table are grouped by purpose. For display/write/strobe/
monitor families, the `b`, `o` and `h` suffix variants are supported too,
including the file-output forms and `$swrite`.

| Purpose | Supported names / forms |
| --- | --- |
| Console output | `$display`, `$write`, `$strobe`, `$monitor`, `$monitoron`, `$monitoroff` |
| String output | `$swrite`, `$sformat`, `$sformatf`; `$psprintf` as an alias of `$sformatf` |
| Diagnostics and termination | `$fatal`, `$error`, `$warning`, `$info`, `$finish`, `$stop`, `$stacktrace` (task and function forms) |
| Time | `$time`, `$stime`, `$realtime`, `$timeunit`, `$timeprecision`, `$timeformat`, `$printtimescale` |
| Conversions | `$signed`, `$unsigned`, `$rtoi`, `$itor`, `$realtobits`, `$bitstoreal`, `$shortrealtobits`, `$bitstoshortreal` |
| Type and array queries | `$bits`, `$typename`, `$isunbounded`, `$dimensions`, `$unpacked_dimensions`, `$left`, `$right`, `$low`, `$high`, `$increment`, `$size` on supported types |
| Bit queries | `$countbits`, `$countones`, `$onehot`, `$onehot0`, `$isunknown` |
| Integer mathematics | `$clog2` |
| Real mathematics | `$ln`, `$log10`, `$exp`, `$sqrt`, `$pow`, `$floor`, `$ceil`, `$sin`, `$cos`, `$tan`, `$asin`, `$acos`, `$atan`, `$atan2`, `$hypot`, `$sinh`, `$cosh`, `$tanh`, `$asinh`, `$acosh`, `$atanh` |
| String methods | `len`, `getc`, `putc`, `toupper`, `tolower`, `compare`, `icompare`, `substr`, `atoi`, `atohex`, `atooct`, `atobin`, `atoreal`, `itoa`, `hextoa`, `octtoa`, `bintoa`, `realtoa`; character indexing |
| Plusargs | `$test$plusargs`, `$value$plusargs`; command-line `+name` / `+name=value` |
| File management/output | `$fopen`, `$fclose`, `$fflush`, `$fdisplay`, `$fwrite`, `$fstrobe`, `$fmonitor` |
| File input/position | `$fgetc`, `$ungetc`, `$fgets`, `$fscanf`, `$fread`, `$ftell`, `$fseek`, `$rewind`, `$feof`, `$ferror`; STDIN |
| String input | `$sscanf` |
| Memory files | `$readmemb`, `$readmemh`, `$writememb`, `$writememh`, including address ranges |
| Random numbers | `$random`, `$dist_uniform`, `$dist_normal`, `$dist_exponential`, `$dist_poisson`, `$dist_chi_square`, `$dist_t`, `$dist_erlang`; `$urandom`, `$urandom_range` with per-process generators seeded by `--seed` |
| VCD control | `$dumpfile`, `$dumpvars`, `$dumpoff`, `$dumpon`, `$dumpall`, `$dumplimit`, `$dumpflush` |

### Formatting and input limits

- `$strobe` and `$monitor` arguments cannot read automatic variables.
- Output conversions: `%b`, `%o`, `%h` / `%x`, `%d`, `%c`, `%s`, `%e`,
  `%f`, `%g`, `%t`, `%m`, `%l`, `%%`, with widths and supported flags.
  Precision and the `+`, space and `#` flags are for real conversions.
  `%u` and `%z` write raw 32-bit words in native byte order, least
  significant word first, with zero padding above the value's width;
  `%u` clears X/Z. Console, file, postponed and string-format output use
  the same implementation. Packed destinations retain NUL bytes; SV strings
  omit them. `%p` and `%v` are not implemented.
- `$sformat` and `$sformatf` accept runtime string or packed-integral
  formats. Missing or extra values warn and continue; X/Z bits in a packed
  format are not supported. Literal string values after the format remain
  values, even when they contain percent signs.
- `$sscanf` and `$fscanf` accept runtime formats: binary/octal/decimal/hex,
  character/string and real conversions, field widths, assignment suppression
  (`*`) and `%%`. Scan conversions `%t`, `%u`, `%z`, `%v` and `%m` are not
  implemented. An X/Z packed format returns `-1` without reading input;
  insufficient output arguments warn and stop before that conversion.
- `$value$plusargs` accepts runtime formats with one conversion:
  `%d`, `%o`, `%h` / `%x`, `%b`, `%e`, `%f`, `%g`, `%s`, including uppercase
  and `%0` forms. The first matching argument wins. No match leaves the
  output unchanged. Multiple conversions, a leading `%%` prefix and X/Z
  packed formats are not supported.
- `$fread` supports integral destinations and memories; one-dimensional
  memories of unpacked structures may contain nested structures with
  integral leaves. Array/real members, multidimensional structure memories
  and unpacked unions are not supported.
- Memory-file operations accept fixed memories passed through `ref`,
  including forwarding, waiting calls, automatic storage and references
  to array rows. Writemem also accepts `const ref`. See
  [system tasks](spec/SYSTEM_TASKS.md) and [examples](P1_FEATURES.md).

## Command line and waveforms

| Purpose | Implemented interface |
| --- | --- |
| Run / inspect | `kirimasim [options] <files> [+plusargs]`, `kirimasim parse`, `--help`, `--version` |
| Source input | `-f`, `+incdir+`, `+define+`, `--top` through slang |
| Time selection | `--timescale=<unit>/<precision>`, `--mintypmax=min\|typ\|max` |
| Simulation limits | `--max-time`, `--max-time-ok`, `--max-zero-delay`, `--max-slot-events`, `--max-steps`, `--max-call-depth` |
| Live output | `--autoflush` flushes stdout after each display/write/strobe/monitor, including file tasks directed to stdout; default off |
| Stop policy | `--stop=finish\|ignore\|error`; default `finish`; `ignore` resumes the caller, `error` ends with code 1 and reason `stop`; `$stop(2)` requests statistics at the eventual end |
| Wall-clock limit | `--max-wall-time=<seconds>`; whole seconds, 0 disables; compilation excluded; ends with code 4 / `wall_time_limit` and runs `final`; `--max-time-ok` does not apply |
| Random seed | `--seed=<n>` |
| Inspection | `--dump-sir`, `--stats=json` |
| Bounds diagnostics (from `dec94ff`) | `--bounds-check=off\|warn` (default `off`): warn once per source position for invalid packed selections/fixed-array indices and print final access counts; normal read/write/NBA values are preserved |
| VCD output | `--wave=<file>.vcd`, `--wave-scope=<pattern>[,<pattern>...]`, `--wave-glitches`; 4-state waveforms, also controlled by the dump tasks above |

`--max-scc-iter` is accepted but has no effect until P2's static islands.
Other options described in the [CLI specification](spec/CLI.md) are not
automatically implemented: for example, `--cond-x`, `--init-values` and
`--max-procs` are not current driver options. Use `--help` to inspect a build.
FST output is not implemented. The wall-clock limit is checked before events
and at VM branches, including static initialization; it does not preempt
a blocking host operation. Final procedures finish without that deadline.

## Remaining work and scope

| Status | Features |
| --- | --- |
| Still pending in P1 | FST; `%p`; resolved-strength `%v`; the unsupported `$fread` structure shapes above; remaining gaps in the P1 forms listed here |
| P2 | Tier 2 C++ generation, static-island analysis and further RTL performance work |
| P3 | Classes/UVM, dynamic arrays, queues, associative arrays, constrained randomization, covergroups, concurrent assertions and sampled-value functions, clocking blocks, program blocks, virtual interfaces, `let`, VPI and DPI-C |
| P4 | MOS and bidirectional switches |
| Outside current scope / deferred | VHDL/mixed-language and analog/mixed-signal designs; SDF annotation and encrypted IP; specify module paths/timing checks; user-defined nettypes, `interconnect`, `trireg`; legacy PLI; library map files; checkers, `randsequence`, an interactive debugger and a standalone Python API |

There is no compiled-simulation backend yet: all executable features in
this inventory run in tier 1. Python integration is planned through cocotb
over VPI in P3. P1 completion also requires the correctness and performance
gates in the README; the size of this list alone does not establish completion.

## Evidence and maintenance

- [Simulation suite](../tests/suite/): executable examples and expectations,
  grouped under `values`, `proc`, `sched`, `cont`, `nets`, `hier` and others.
- [Unit tests](../tests/unit/): value semantics, scheduler and runtime checks.
- [Unsupported-form matrix](../tools/gen_unsupported_matrix.py) and its
  [CLI fixtures](../tests/cli/unsupported_matrix/): specific rejected forms
  and source-position diagnostics. This is more precise than a keyword list.
- Mechanism matrices for [streaming](../tools/gen_stream_matrix.py),
  [enum steps](../tools/gen_enum_step_matrix.py),
  [runtime scans](../tools/gen_runtime_scan_matrix.py),
  [string formatting](../tools/gen_runtime_string_format_matrix.py),
  [plusarg formats](../tools/gen_runtime_plusargs_matrix.py),
  [structure memories](../tools/gen_fread_struct_matrix.py),
  [reference memories](../tools/gen_ref_memfile_matrix.py),
  [binary output](../tests/cli/run_binary_format_tests.py) and
  [bounds diagnostics](../tools/gen_bounds_check_matrix.py).
- Implementation: [lowering](../src/front/), [interpreter](../src/vm/),
  [runtime](../src/kernel/) and [driver](../src/driver/main.cpp).

When support changes, update both language versions, the README summary and
the relevant tests. Keep examples in `P1_FEATURES` and normative behavior in
`docs/spec/`; a planned specification item is not evidence of implementation.
