# Recent P1 features: examples and limits

[日本語](P1_FEATURES.ja.md) · This English text is the main version.

These examples describe tier 1 as of 2026-10-23. P1 is still in progress;
see the [README](../README.md) for the full supported range and remaining work.
After a [development build](../README.md#building), save one example at a time as
`demo.sv` and run it from the repository root:

```sh
build/dev/src/driver/kirimasim demo.sv
```

The output blocks below show the design's standard output. kirimasim's own
messages go to standard error.

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
Runtime formats for `$sformat`, `$sformatf` and `$value$plusargs` remain
unsupported. See [system tasks, section 6.3](spec/SYSTEM_TASKS.md#63-input)
and [SIR implementation limits](spec/SIR_SPEC.md).

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
