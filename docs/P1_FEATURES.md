# Recent P1 features: examples and limits

[日本語](P1_FEATURES.ja.md) · This English text is the main version.

These examples describe tier 1 as of 2026-10-23. P1 is still in progress;
see the [P1 support inventory](P1_SUPPORT.md) for the supported range and limits,
and the [README](../README.md) for the remaining work.
After a [development build](../README.md#building), save one example at a time as
`demo.sv` and run it from the repository root:

```sh
build/dev/src/driver/kirimasim demo.sv
```

The output blocks below show the design's standard output. kirimasim's own
messages go to standard error.

## Delayed NBAs with runtime targets

Delayed NBAs now support runtime fixed-array indices, packed bit and part
positions, and concatenations of those targets. The right value, target
and delay are captured when issued; later changes do not redirect the
update. Only valid bits of a partial selection are scheduled. Updates use
the issuing thread's NBA/Re-NBA set and survive its task/fork returning.
Automatic targets remain illegal; event-controlled NBAs, whole-array
timing and delayed targets through refs or string characters remain
unsupported.

```systemverilog
`timescale 1ns/1ns
module top;
  logic [64:0] words[2];
  int index = 1;
  logic [64:0] data = 65'h10000000000000001;
  initial begin
    words[0] = 0; words[1] = 0;
    words[index] <= #2 data;
    index = 0; data = 0;
    #3; $display("%h %h", words[0], words[1]);
  end
endmodule
```

```text
00000000000000000 10000000000000001
```


## Programs and the reactive region set

Program initial procedures and continuous assignments run in Reactive.
Their #0 delays resume in Re-Inactive, and NBAs update in Re-NBA. Calls
and fork branches inherit the executing thread's group and program origin,
including code declared in modules, packages or interfaces. In this example,
the design's NBA is processed before the program samples the input.

The last initial of each program ends its remaining descendants. Once all
programs with initials end, an implicit finish runs final procedures and
prevents the module's later `future` line. `$exit` explicitly ends the
originating program's initials and descendants; it is ignored outside a
program-initial thread. Empty programs do not force finish. Anonymous-program
subroutines are accepted, while slang keeps restrictions on outside references.
Classes, clocking blocks, concurrent assertions and program VCD scopes are
not enabled by this feature.

```systemverilog
module top;
  logic clk = 0;
  int data = 0;
  initial begin #2; data = 5; data <= 7; clk = 1; #10; $display("future"); end
  worker p(clk, data);
endmodule
program worker(input logic clk, input int data);
  int sampled = 0;
  initial begin
    @(posedge clk);
    $display("sample %0d", data);
    sampled <= data;
    #0; $display("before Re-NBA %0d", sampled);
    #1; $display("after Re-NBA %0d", sampled);
  end
  final $display("program final");
endprogram
```

```text
sample 7
before Re-NBA 0
after Re-NBA 7
program final
```

## Writing unformatted binary words

`%u` writes 2-state data and `%z` writes 4-state data, in complete 32-bit
words, least significant word first, in native byte order. `%u` clears
X/Z; `%z` writes the VPI-style value and unknown masks. Padding above the
value's width is zero, including for signed values. Uppercase forms work too.

```systemverilog
module top;
  logic [3:0] value = 4'b1xz0;
  string format_value, result;
  int fd;
  initial begin
    fd = $fopen("binary.bin", "wb");
    $fwrite(fd, "%u%z", value, value);
    $display("bytes=%0d", $ftell(fd));
    $fclose(fd);
    format_value = "%z";
    result = $sformatf(format_value, value);
    $display("string length=%0d", result.len());
  end
endmodule
```

```text
bytes=12
string length=2
```

Files and packed destinations of `$swrite`/`$sformat` keep every byte.
SV strings cannot contain NUL, so string destinations and `$sformatf`
omit zero bytes. To retain binary data, use a file or a packed destination.
Binary input formats in `$sscanf` and `$fscanf` use the same word encoding;
see the input example below.

## Reading unformatted binary words

`%u`/`%z` read least-significant native 32-bit words first. Omitted and
zero field width use the destination bit width rounded up to words;
positive width is transfer bits, not bytes. Bits above that width are
masked before zero extension or truncation. `%z` retains X/Z in four-state
targets; two-state targets clear unknown bits. These formats are not
plusarg conversions.

Raw whitespace and NUL are payload, including packed `$sscanf` input.
Explicit whitespace in a format still skips normally. Only complete fields
assign; a short field consumes available bytes, preserves its target, and
sets file EOF. A complete field does not look ahead to detect EOF. Width
zero is the approved project rule and differs from measured Icarus behavior.

```systemverilog
module top;
  logic [3:0] written = 4'b1xz0, loaded;
  logic [32:0] wide = '1;
  string fmt;
  int fd, count;
  initial begin
    fd = $fopen("binary.bin", "wb");
    $fwrite(fd, "%z", written);
    $fclose(fd);
    fd = $fopen("binary.bin", "rb");
    fmt = "%z";
    count = $fscanf(fd, fmt, loaded);
    $display("read=%0d value=%b bytes=%0d eof=%0d", count, loaded, $ftell(fd), $feof(fd));
    $fclose(fd);
    count = $sscanf(32'h44444444, "%4u", wide);
    $display("read=%0d low nibble=%h", count, wide);
  end
endmodule
```

```text
read=1 value=1xz0 bytes=8 eof=0
read=1 low nibble=000000004
```

## Loading memories through reference arguments

Readmem/writemem tasks accept fixed memories through `ref`, including
automatic memories, forwarded references, waiting tasks and references to
rows of multidimensional arrays. Writemem also accepts `const ref`.
Address ranges, enumeration checks and X/Z conversion use the same kernel
as direct calls.

```systemverilog
module top;
  logic [7:0] memory[2:4];
  int fd;
  task automatic load(ref logic [7:0] m[2:4]);
    $readmemh("input.mem", m);
  endtask
  function automatic void save(const ref logic [7:0] m[2:4]);
    $writememh("output.mem", m);
  endfunction
  initial begin
    fd = $fopen("input.mem", "w");
    $fwrite(fd, "12 34 56\n");
    $fclose(fd);
    load(memory);
    save(memory);
    $display("%h %h %h", memory[2], memory[3], memory[4]);
  end
endmodule
```

```text
12 34 56
```

## Seeing output while a design waits

`--autoflush` flushes stdout after each display/write/strobe/monitor task,
including file tasks directed to stdout. It defaults off and does not
change the text or scheduling. A reader can see this prompt before the
design waits for standard input:

```systemverilog
module top;
  int character;
  initial begin
    $write("ready> ");
    character = $fgetc(32'h8000_0000);
    $display("read=%0d", character);
  end
endmodule
```

```sh
printf x | build/dev/src/driver/kirimasim --autoflush demo.sv
```

```text
ready> read=120
```

## Choosing what `$stop` does

```systemverilog
module top;
  initial begin
    $display("before");
    $stop(1);
    $display("after");
  end
  final $display("final");
endmodule
```

With `--stop=ignore`, the output is:

```text
before
after
final
```

The default `--stop=finish` omits `after` and exits with code 0;
`--stop=error` does the same with code 1. Both keep the reason `stop`.
Ignored calls still print their call-site report when their argument is
1 or 2; argument 2 requests statistics at the eventual end of the run.

## Limiting wall-clock time

```systemverilog
module top;
  int count;
  initial forever count++;
  final $display("final");
endmodule
```

```sh
build/dev/src/driver/kirimasim --max-wall-time=1 --max-steps=0 demo.sv
```

The output is `final`, and the exit code is 4, with reason `wall_time_limit`.
The budget is a whole number of seconds from the start of simulation;
compilation is excluded and 0 disables it. `--max-time-ok` does not change
that code. The kernel checks before events, and tier 1 checks at branches,
including static initialization. This does not preempt a blocking host
operation. Final procedures run to completion without the expired budget.

## Enum steps computed at run time

`next(n)` and `prev(n)` accept a variable count and follow declaration order,
wrapping around the ends. Calling them returns a value and leaves the receiver
unchanged.

```systemverilog
module top;
  typedef enum bit [7:0] {IDLE = 1, RUN = 5, DONE = 9} state_t;
  state_t state, advanced, previous;
  int unsigned steps = 2;
  initial begin
    state = IDLE;
    advanced = state.next(steps);
    previous = state.prev(steps);
    $display("next=%s prev=%s original=%s",
             advanced.name(), previous.name(), state.name());
  end
endmodule
```

```text
next=DONE prev=RUN original=IDLE
```

The count is converted to 32-bit `int unsigned` before stepping, including for
negative or wider arguments. Zero keeps a valid current member; a value outside
the enum's members returns the base type's default initial value. The receiver
and count are evaluated once each, left to right. See
[value semantics, section 7.7](spec/VALUE_SEMANTICS.md#77-enumerated-types).

## Streaming into targets wider than 64 bits

Streaming assignment targets support packed values, fixed unpacked arrays and
structures beyond 64 bits. This example splits 128 bits into bytes, reverses
their order with `<<8`, and then schedules a nonblocking assignment.

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  typedef logic [7:0] byte_t;
  logic [127:0] payload;
  byte_t bytes [0:15];
  initial begin
    payload = 128'h00112233445566778899aabbccddeeff;
    {>>{bytes}} = payload;
    $display("forward %h %h", bytes[0], bytes[15]);
    {<<8{bytes}} = payload;
    $display("reverse %h %h", bytes[0], bytes[15]);
    {>>{bytes}} <= payload;
    payload = '0;
    #1;
    $display("nba %h %h", bytes[0], bytes[15]);
  end
endmodule
```

```text
forward 00 ff
reverse ff 00
nba 00 ff
```

The nonblocking assignment captures the right-hand value before `payload`
changes. Four-state destinations retain X and Z; two-state destinations convert
them to zero. Arrays follow foreach order and structures follow member
declaration order. Slice sizes must be constant. Automatic variables cannot be
nonblocking assignment targets. Dynamic containers and `with` remain P3 work;
nested streaming targets are not supported yet. See
[value semantics, section 8](spec/VALUE_SEMANTICS.md#8-not-confirmed-to-be-written-later)
for the handling of a final short slice.

## Scan formats computed at run time

`$sscanf` and `$fscanf` accept formats held in strings or packed integral values,
including array elements and function results. This example creates
`scan-input.txt` in the working directory.

```systemverilog
module top;
  string formats [2];
  int value, matched, fd;
  initial begin
    formats[0] = "%d";
    formats[1] = "%h";
    for (int i = 0; i < 2; i++) begin
      matched = $sscanf("10", formats[i], value);
      $display("string %0d %0d", matched, value);
    end
    fd = $fopen("scan-input.txt", "w");
    if (fd == 0) $fatal(1, "cannot open scan-input.txt for writing");
    $fwrite(fd, "10 10");
    $fclose(fd);
    fd = $fopen("scan-input.txt", "r");
    if (fd == 0) $fatal(1, "cannot open scan-input.txt for reading");
    for (int i = 0; i < 2; i++) begin
      matched = $fscanf(fd, formats[i], value);
      $display("file %0d %0d", matched, value);
    end
    $fclose(fd);
  end
endmodule
```

```text
string 1 10
string 1 16
file 1 10
file 1 16
```

The return value counts successfully assigned outputs; remaining outputs keep
their values. An X/Z-containing packed format returns -1 without assigning
outputs or consuming file input. A runtime format with too few outputs warns
and stops before the missing conversion. An unsupported conversion or a
conversion incompatible with its output type ends the run with a diagnostic
and exit code 2. Constant formats are still checked at elaboration.

Supported scan conversions are `%b`, `%o`, `%d`, `%h`/`%x`, `%c`, `%s` and
`%f`/`%e`/`%g`, with field widths, assignment suppression and `%%`.
See [system tasks, section 6.3](spec/SYSTEM_TASKS.md#63-input)
and [SIR implementation limits](spec/SIR_SPEC.md).

## String formats computed at run time

`$sformat` and `$sformatf` accept a runtime string or packed format, including
function results. Only the format argument is interpreted; a string value
containing `%` is printed as data.

```systemverilog
module top;
  string fmt, written, returned;
  initial begin
    for (int i = 0; i < 2; i++) begin
      fmt = i == 0 ? "%0d" : "%04h";
      $sformat(written, fmt, 10);
      returned = $sformatf(fmt, 10);
      $display("%s|%s", written, returned);
    end
  end
endmodule
```

```text
10|10
000a|000a
```

Numeric, string and real conversions use the existing format rules. `%t` uses
the caller's time unit and `$timeformat`; `%m` and `%l` use the calling scope
and cell. Missing values print nothing and extra values are ignored, with one
warning per call site. All supplied values are evaluated, including extra ones.
Invalid formats and incompatible types end with a diagnostic and exit code 2.
X/Z-containing packed formats and `%p` remain unsupported. Constant formats
retain elaboration checks. See [system tasks, section 3.13](spec/SYSTEM_TASKS.md#313-formatting-to-strings).

## Plusarg formats computed at run time

`$value$plusargs` accepts a runtime string or packed integral format, including
function results. Here the same call site reads the first `+count=42` argument
as decimal and then hexadecimal.

```systemverilog
module top;
  string fmt;
  int value, matched;
  initial begin
    for (int i = 0; i < 2; i++) begin
      fmt = i == 0 ? "count=%d" : "count=%h";
      matched = $value$plusargs(fmt, value);
      $display("%0d %0d", matched, value);
    end
  end
endmodule
```

Run with `build/dev/src/driver/kirimasim demo.sv +count=42 +count=99`:

```text
1 42
1 66
```

The first matching plusarg wins. If none matches, the call returns 0 and leaves
the output unchanged; functions in its output indices are not called. Supported
formats and conversions follow [system tasks, section 9](spec/SYSTEM_TASKS.md#9-plusargs).
Invalid runtime formats and incompatible output types end with a diagnostic and
exit code 2. Packed formats containing X/Z bits and formats with `%%` before
the conversion remain unsupported.

## A for loop with no condition

An omitted condition in `for (;;)` keeps the loop running until control leaves
it, for example through `break`.

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  int ticks;
  initial begin
    ticks = 0;
    for (;;) begin
      #1;
      ticks++;
      if (ticks == 3) break;
    end
    $display("ticks %0d", ticks);
  end
endmodule
```

```text
ticks 3
```

For loops that do not advance simulation time, `--max-steps=<n>` bounds execution.
See the [CLI specification](spec/CLI.md) for limits and exit codes.

## Runtime bounds warnings

`--bounds-check=warn` reports indices that are out of range or contain X/Z,
including a part-select that only partly overlaps the value.

```systemverilog
module top;
  logic [7:0] value = 8'ha5;
  logic [3:0] selected;
  int index = 6;
  initial begin
    for (int repeat_count = 0; repeat_count < 2; repeat_count++) begin
      selected = value[index +: 4];
      $display("%b", selected);
    end
  end
endmodule
```

```text
xx10
xx10
```

Run with `build/dev/src/driver/kirimasim --bounds-check=warn demo.sv`.
Standard error gives one warning at the select's position, then a summary
counting two invalid accesses at one source position. The same program with
`--bounds-check=off` (the default) has the same design output and event counts.
The check covers packed reads/writes, fixed unpacked arrays, automatic/ref
storage and NBA targets. Warnings are counted per read/write access; a compound
update can include both a read and a write. See [CLI.md](spec/CLI.md) and
[value semantics, section 3.11](spec/VALUE_SEMANTICS.md).

## Minimum, typical and maximum delays

```systemverilog
module top;
  timeunit 1ns;
  timeprecision 1ns;
  initial begin
    #(2:5:9);
    $display("delay %0d", $time);
  end
endmodule
```

Run this example with `--mintypmax=min`, `--mintypmax=typ` or
`--mintypmax=max` to print `delay 2`, `delay 5` or `delay 9`, respectively.
The default is `typ`. See the [CLI specification](spec/CLI.md) and
[scheduler specification](spec/SCHEDULER_SPEC.md).
