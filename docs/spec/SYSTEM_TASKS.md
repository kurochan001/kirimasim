# SYSTEM_TASKS: system tasks and functions

[日本語](SYSTEM_TASKS.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P0 (before P1 starts). Status: **approved (2026-10-15)**. On 2026-10-16, string literals passed to format specifiers (3.1) and the form of reports of unique and priority violations (10.4) were added (approved). The same day, reals with no format specifier, and swapping integer and real formats (3.6), were added (approved). On 2026-10-17, upper-case `%E` and `%G` (3, 3.6) were added (approved). The same day, how `%s` prints zero bytes in the middle of a value (3.5) was added (approved). The same day, the result of `$rtoi` for NaN, infinities and out-of-range values (8) was decided (approved). On 2026-10-18, a `%` with no conversion (3.1) was decided (approved). The same day, the details of the flags `-` and `0` (3.12) were decided (approved). The same day, how the time of the report of `$finish` and `$stop` is written, and when and in what form argument 2 writes its statistics (10.2), were decided (approved). The same day, the value that `?` reads as in `$sscanf` and `$fscanf` (6.3) was written down (approved). On 2026-10-19, `--stop` was referred to from 10.2 (approved). The same day, when standard output is written out, and `--autoflush`, were added to 2 (approved). The same day, a point UNSPECIFIED_BEHAVIOR.md leaves not decided was marked in 7.1 (approved). The same day, the range of `$readmemb` and `$readmemh` without a finish address was added to 7.1 (approved). The same day, the sentence of 11 on `%0t` was brought up to date (approved). On 2026-10-20, descriptors that are not open, `$fopen` with a type not in Table 21-6 and the files of the Context were added to 6.1, and when `$fstrobe` and `$fmonitor` read their descriptor, and how `$fclose` cancels them, to 6.2 (approved). On 2026-10-21, the descriptors input reads, standard input, files that only write, `$fgets` into an integral variable, `$fread`, how `$fscanf` takes characters and `$ungetc` were added to 6.3, and the error `$ferror` gives and when `$feof` is set to 6.4 (approved). On 2026-10-21, the digits of a word, the order of the lower dimensions, what a file that cannot be read does (7.1), and the order of the words $writemem writes (7.2) were set (approved). The same day, the section on `$random` and the `$dist_*` functions was added as 12, and "Not confirmed" became 13 (approved). The same day, what a word out of range of an enumerated type is, and the error it gives (7.1), were added (approved). The same day, `$fread` into memories of unpacked structures was added to 6.3 (approved). The same day, which scope each line of `$stacktrace` names, and the string it returns as a function, were added to 10.3 (approved). The same day, the section on `$urandom`, `$urandom_range`, `randcase` and the seed was added as 13, and "Not confirmed" became 14 (approved). The interpretations and the rules of this document were approved by a human on 2026-10-15.

This document sets what kirimasim's system tasks and system functions do. The test suite passes or fails on standard output, so the details of display formats are set too. The text of the standard (IEEE 1800-2023) is not copied; it is summarized in this document's own words, with the clause numbers it rests on.

- Items marked "checked" were checked by reading the relevant clause of the standard.
- Items marked "interpretation" are not settled by the wording of the standard alone; this document decides how to read them.
- Items marked "rule of this document" are where the standard leaves the choice to the implementation, and this document makes it.
- Some places add, "for reference", the results of running Icarus Verilog 13.0 and Verilator 5.052 (macOS arm64, 2026-10-15). A reference is not grounds.

The meaning and exit codes of execution control (`$finish`, `$stop`, `$exit`, `$fatal`) are set in SCHEDULER_SPEC.md 16. This document covers only the form of the messages they give (10). The random-number system functions (`$random`, `$urandom`, `$urandom_range`, `$dist_*`) are added to this document in P1 (within P1 in the design specification; approved 2026-10-15). Constrained random is in RANDOMIZE_SPEC.md; assertion functions (`$sampled`, `$past` and so on) and controls (`$assertoff` and so on) are in ASSERTION_COVERAGE_SPEC.md; waveforms (`$dump*`) are in the waveform document written in P1.

## 1. Scope and timing

The draft list of the design specification was checked against the standard and put in the table below. The phases are the design specification's stages (1: P1 to P2, 3: P3, 4: P4).

| Group | Names | Standard | Phase |
| --- | --- | --- | --- |
| Display | `$display`, `$write` (and the forms with b, o, h) | 21.2.1 | 1 |
| Delayed display and monitoring | `$strobe`, `$monitor`, `$monitoron`, `$monitoroff` (and the forms with b, o, h) | 21.2.2, 21.2.3 | 1 |
| Formatting to strings | `$swrite` (and the forms with b, o, h), `$sformat`, `$sformatf` | 21.3.3 | 1 |
| Opening and closing files | `$fopen`, `$fclose` | 21.3.1 | 1 |
| Output to files | `$fdisplay`, `$fwrite` (and the forms with b, o, h), `$fflush` | 21.3.2, 21.3.6 | 1 |
| Monitoring to files | `$fstrobe`, `$fmonitor` | 21.3.2 | 3 |
| Reading from files | `$fgets`, `$fscanf`, `$sscanf`, `$fread`, `$fgetc`, `$ungetc` | 21.3.4 | 1 (`$fgetc`, `$ungetc` in 3) |
| Position and status | `$ftell`, `$fseek`, `$rewind`, `$ferror`, `$feof` | 21.3.5 to 21.3.8 | 1 |
| Memories | `$readmemb`, `$readmemh`, `$writememb`, `$writememh` | 21.4, 21.5 | 1 |
| Time | `$time`, `$stime`, `$realtime`, `$timeunit`, `$timeprecision`, `$printtimescale`, `$timeformat` | 20.3, 20.4 | 1 |
| Conversion | `$rtoi`, `$itor`, `$realtobits`, `$bitstoreal`, `$shortrealtobits`, `$bitstoshortreal`, `$signed`, `$unsigned` | 20.5 | 1 |
| Queries | `$bits`, `$isunbounded`, `$typename` | 20.6 | 1 (`$typename` brought forward from 3 on 2026-10-17) |
| Array queries | `$left`, `$right`, `$low`, `$high`, `$increment`, `$size`, `$dimensions`, `$unpacked_dimensions` | 20.7 | 1 |
| Math | `$clog2` and the 20 real functions (`$ln` to `$atanh`) | 20.8 | 1 |
| Bit vectors | `$countbits`, `$countones`, `$onehot`, `$onehot0`, `$isunknown` | 20.9 | 1 |
| Severity | `$fatal`, `$error`, `$warning`, `$info` (at run time and at elaboration) | 20.10, 20.10.1 | 1 |
| Plusargs | `$test$plusargs`, `$value$plusargs` | 21.6 | 1 |
| Other | `$stacktrace` | 20.17.2 | 1 |

Those in the standard but not in the design specification's list (approved by a human 2026-10-15):

- `$system` (20.17.1): runs any shell command. It can break the reproducibility of tests (NFR-2), so by default it is an error, and it runs only with `--allow-system`.
- `$dumplimit` (21.7.1.5) and the `$dumpports` family of extended VCD (21.7.3): covered by WAVEFORM.md.
- The stochastic analysis tasks `$q_*` (20.15) and the PLA tasks (20.16): not supported; calling one is an error. Designs that use them are rare, and the inventory of tests found only tests with their names and no substance.

## 2. Where output goes

- `$display`, `$write`, `$strobe` and `$monitor` write to standard output (checked: 21.2).
- Bit 0 of a multichannel descriptor (mcd) and the file descriptor `32'h8000_0001` (STDOUT) point to the same standard output. `32'h8000_0002` (STDERR) points to standard error (checked: 21.3.1). Whichever path is used, output appears in the order written (rule of this document; they share one buffer).
- Every message of kirimasim itself (diagnostics, severity tasks, the report of `$finish`, the reason for ending) is written to standard error (rule of this document). Standard output holds only what the design wrote. The test suite compares standard output, so messages whose form differs between implementations do not get mixed into what is compared.
- Before writing to standard error, the standard output buffer is flushed (rule of this document), so the order is kept when viewed on a terminal.
- **When standard output is written out (approved 2026-10-19):** standard output goes through the C library's standard output, which DPI-C's `printf` (M7) uses too, so their lines keep their order. It is written out before anything is written to standard error, at `$fflush`, when the first SIGINT is taken, and when the run ends, however it ends. `--autoflush` (CLI.md 2.2) also writes it out after every display task, for watching a run that is slow or may crash.

## 3. Display and formats

### 3.1 Arguments

- `$display` adds a newline at the end and `$write` does not; otherwise they are the same (checked: 21.2.1).
- Every string literal argument is read as a format (the `$display` and `$write` families; `$sformat` is 3.13). Escapes with `\` (5.9.1) and `%` format specifiers are interpreted. `%%` prints one `%` (checked: 21.2.1).
- An expression argument with no format specifier is printed in decimal for `$display` and `$write`, and in binary, octal and hexadecimal for the forms with `b`, `o` and `h`, with the automatic width (3.3). The string type and unpacked arrays of byte are printed as strings. Passing any other unpacked type with no format specifier is an error (checked: 21.2.1.1).
- An empty argument (a leading or trailing comma, or consecutive commas) prints one space. An argument list of a single comma counts as two empty arguments (checked: 21.2.1).
- `$display` with no arguments prints only a newline, and `$write` with no arguments prints nothing (checked: 21.2.1).
- **String literals passed to format specifiers (approved 2026-10-16):** when the format has a specifier that has not yet taken an argument, a following string literal is not a format but the value passed to that specifier (for example `$display("%s", "abc")` prints `abc`). The standard puts, after the format, an expression argument for each `%` specifier (checked: 21.2.1), and a string literal is an expression too, so it is read this way (interpretation; for reference, Icarus 13.0 and Verilator 5.052 do the same, checked 2026-10-16). "Every string literal is read as a format" above means string literals for which no specifier is left to take them.
- **A `%` with no conversion (approved 2026-10-18):** a `%` in a format string with no conversion after it (a trailing `%`, `%5` and so on) is reported as an error of the input (exit code 2; rule of this document; the standard does not decide it, and slang gives a warning; for reference, checked 2026-10-18, Icarus 14.0 devel printed the `%` as it is and Verilator 5.053 devel dropped it).
- An undefined format specifier is an error (checked: 21.2.1.1). If the format is constant, it is an error of elaboration. It is also an error when a specifier other than `%m`, `%l` and `%%` has no argument (checked: 21.2.1). Missing and extra arguments for formats that are not constant (the format of `$sformat` and the like) follow 3.13.

### 3.2 Format specifiers

| Specifier | Meaning | Clause |
| --- | --- | --- |
| `%h` `%x` | Hexadecimal | 21.2.1.1 |
| `%d` | Decimal | 21.2.1.1 |
| `%o` | Octal | 21.2.1.1 |
| `%b` | Binary | 21.2.1.1 |
| `%c` | One character | 21.2.1.1 |
| `%s` | String | 21.2.1.7 |
| `%t` | Time format (`$timeformat`) | 21.2.1.1, 20.4.3 |
| `%m` | The hierarchical name of where it was called (takes no argument) | 21.2.1.5 |
| `%l` | The library binding (takes no argument) | 21.2.1.1 |
| `%p` | The form of an assignment pattern | 21.2.1.6 |
| `%v` | Net strength | 21.2.1.4 |
| `%u` `%z` | Unformatted 2-state and 4-state data | 21.2.1.1 |
| `%e` `%f` `%g` | Real | 21.2.1.1 |

- Upper and lower case mean the same (checked: Table 21-1). But the real `%E` and `%G` print the exponent letter as upper-case `E`, as C's formats do (3.6; approved 2026-10-17).
- The integer formats (`%h`, `%x`, `%d`, `%o`, `%b`, `%c`, `%u`, `%z`) can be used for integral types, enumerated types and packed aggregate types. Not for unpacked aggregate types (checked: 21.2.1.1).
- The hexadecimal letters a to f are printed in lower case even when written `%H` or `%X` (rule of this document; no text found in the standard; for reference, Icarus and Verilator also use lower case). The case of the letters x and z follows 3.4.

### 3.3 Display width

- **Automatic width:** with no width written, the number of digits needed for the largest value the expression can take is found from its width, and the value is printed with that many digits. Decimal replaces leading zeros by spaces; other radixes print leading zeros too (checked: 21.2.1.2). Example: a 32-bit value 10 is `        10` (10 digits) with `%d` and `0000000a` (8 digits) with `%h`. The same for signed values and for parameter values (a note of this document; the rule of the standard has no exception).
- **Decimal digits:** for signed values, the largest number of digits counts the sign too (interpretation; a 32-bit `int` is the 11 characters of `-2147483648`). For reference, Icarus and Verilator print 32-bit signed values in 11 digits.
- **Width 0:** `%0d` and `%0h` print in the smallest width, with no leading spaces or zeros (checked: 21.2.1.2).
- **Explicit width:** a written width is used instead of the automatic width. If the value has fewer digits, it is padded on the left; if more, the field widens. Nothing is truncated. The padding is spaces for decimal and strings, and 0 for hexadecimal, octal and binary (checked: 21.2.1.2). For example, a 32-bit 5 is `005` with `%3h` and `000000000101` with `%12b`. For reference, Verilator follows the standard. Icarus 13.0 ignores a width narrower than the automatic width and prints `00000005`, and pads wide binary, octal and hexadecimal widths with spaces. The test suite's expected values follow the standard.
- The width and precision of real formats are the same as C's formats (3.6).

### 3.4 Printing x and z

Checked (21.2.1.3):

- `%d`: if every bit is x, one lower-case `x`; if every bit is z, one lower-case `z`. If some bits are x, an upper-case `X`; if no bit is x and some are z, an upper-case `Z`. In each case it is right-aligned in the field of the automatic width.
- `%h` and `%o`: for each group of 4 bits (3 for octal), `x` if all are x, `z` if all are z, `X` if some are x, `Z` if none is x and some are z.
- `%b`: one bit at a time, `0`, `1`, `x`, `z`.

### 3.5 Characters and strings

- `%c` prints the low 8 bits of the value as one character (interpretation; the standard says only "an ASCII character"; for reference, Icarus and Verilator also use the low 8 bits).
- `%s` prints an integer value as a sequence of character codes of 8 bits each, the lowest 8 bits being the last character (checked: 21.2.1.7). The automatic width is the width of the value divided by 8, rounded up, in characters; leading zero bytes are not printed as characters but padded with spaces (interpretation; read from "strings are padded with spaces" in 21.2.1.2 together with "leading zeros are not printed" in 21.2.1.7; for reference, Icarus and Verilator do the same). `%0s` adds no spaces. A zero byte in the middle of the value (after the first nonzero byte) is printed as a space by both `%s` and `%0s` (rule of this document; the wording of the standard does not settle it; for reference, on 2026-10-17 Icarus 14.0 devel and Verilator 5.053 devel were checked to print a space too; approved 2026-10-17).
- The string type and unpacked arrays of byte print their contents as they are. Arrays go from the left bound to the right bound (checked: 21.2.1.7).

### 3.6 Reals

- `%e`, `%f` and `%g` are the same as C's formats. Width, precision and the flags `-`, `+`, `0`, space and `#` are handled as in C too (checked: 21.2.1.1). The exponent has at least 2 digits (C's rule).
- `%E` and `%G` are the same as C's `%E` and `%G` (the exponent letter is `E`). `%F` is the same as `%f`. Infinities and NaN are printed `inf`, `-inf` and `nan` even in upper-case formats (the item below). Grounds: the real formats (Table 21-2) have all the features of the C language's formats (checked: 21.2.1.1). For reference (checked 2026-10-17): Icarus 13.0 and its development version print upper case, Verilator 5.052 lower case (approved 2026-10-17).
- The implementation passes the format to C's `snprintf`. The C libraries of macOS and Linux round finite values correctly (not made an exception to NFR-2).
- Infinities are printed `inf` and `-inf`, and NaN `nan` whatever its sign (rule of this document; C's formats may treat the sign of NaN differently by library, so it is made uniform; for reference, Icarus and Verilator do the same).
- **Reals with no format specifier (approved 2026-10-16):** in `$display` and `$write`, a real value with no specifier to take it is printed in the same form as `%g` (rule of this document). The standard does not set the form of a real with no specifier (checked in 21.2.1.1). This matches the form of reals in `%p` (3.9). For reference: Verilator 5.052 prints the same form as `%g` (`1.5`), Icarus 13.0 a form with the fraction padded (`1.50000`) (checked 2026-10-16).
- **Swapping integer and real formats (approved 2026-10-16):** when a real is printed with an integer format (`%d`, `%h`, `%o`, `%b` and so on), the value is converted to a 64-bit signed integer by the rule of 1.5 (VALUE_SEMANTICS.md), and that integer is printed. The automatic width is that of a 64-bit signed value (20 characters for `%d`). When an integer is printed with a real format, it is converted to `real` by the rule of 7.4 and printed (interpretation; the standard says in 21.2.1.1 only that the real formats are used for real and shortreal). For reference: Verilator 5.052 uses this form, and Icarus 13.0 does not apply the automatic width of `%d` (checked 2026-10-16).

### 3.7 `%t`

- `%t` prints a time with the unit, number of digits after the decimal point, suffix string and minimum width set by `$timeformat` (5.4) (checked: 21.2.1.1, 20.4.3).
- The value passed is taken as a time in the time unit of the design element that called it, and converted to the unit of `$timeformat`. It is rounded to the number of digits after the decimal point (the direction of rounding as in SCHEDULER_SPEC.md 1: exactly half goes away from zero; rule of this document).
- The minimum width counts characters including the suffix string, and spaces are added on the left to right-align (checked: the example of 20.4.3).
- **A width written in `%t`:** a width such as `%5t` is used instead of the minimum width of `$timeformat`. `%0t` prints in the minimum width (interpretation; the width rule of 21.2.1.2 sets a width written between `%` and the "radix letter", and whether it applies to `%t` is not settled by the wording; for reference, Icarus and Verilator both read it this way).

### 3.8 `%m` and `%l`

- `%m` prints the hierarchical name of the design element, subroutine, named block or labelled statement that called the task (checked: 21.2.1.5). Unnamed `begin`–`end` and `fork`–`join` are not part of the hierarchy (interpretation; they are not among those the standard lists). Unnamed generate blocks are printed with the names the standard sets (`genblk<n>`, 27.6).
- `%l` prints `<library name>.<cell name>` (checked: 21.2.1.1). Until the library features (chapter 33) exist, the library name is `work` (rule of this document).

### 3.9 `%p` and `%0p`

Checked (21.2.1.6): unpacked structures are printed as named assignment patterns, and arrays as assignment patterns (index labels may be included). Unions print only their first member; tagged unions use the form `tag:value`. Enumerated types print the name (the value of the base type if out of range); strings are quoted. A null handle is `null`. How spaces are placed is left to the implementation.

kirimasim's form (rule of this document):

- Structures: `'{a:1, s:"x"}`. Packed structures too take the same form with member names.
- Arrays (fixed size, dynamic, queues): `'{1, 2, 3}`. Associative arrays: `'{key:value, ...}` (in key order). Empty dynamic arrays and queues: `'{}`.
- The separator is `, ` (a comma and one space). No spaces inside the braces.
- Integral elements are printed in decimal in the smallest width (as `%0d`; x and z are one character by the rule of 3.4). Reals as `%g`.
- A class handle has the form `<class name>@<number>`. The number counts the objects made of that class type in order, and is the same every time for the same input (NFR-2). A chandle is `chandle@<number>`, not a hexadecimal address. Events and virtual interfaces take the same form.
- `%0p`: the positional form with no names (structures too: `'{1, "x"}`).
- There is no limit on the length of the output (the standard allows a limit of 1024 characters or more).

In the test suite, the spaces of `%p` depend on the implementation, so `%p` output is not put in expected values (continuing TEST_SUITE.md's rule).

### 3.10 `%v`

Prints the net's strength in 3 characters (checked: 21.2.1.4). Written in P4 together with the strength model (28.11 to 28.12). Until then, the drive strength is taken as Strong, and `St0`, `St1`, `StX`, `HiZ` are printed (rule of this document; provisional).

### 3.11 `%u` and `%z`

Writes the 2-state (`%u`) or 4-state (`%z`) representation of the value in 32-bit words, least significant word first, in the host's byte order. `%u` makes x and z 0. The 4-state representation of `%z` is the same as VPI's `s_vpi_vecval` (38.15) (checked: 21.2.1.1).

### 3.12 Flags not in the standard

The standard sets nothing for `-` (left-align) and `0` (pad with 0) in integer, string and time formats (checked: 21.2.1.2 sets only the width). kirimasim gives them these meanings, close to C's (rule of this document; details approved 2026-10-18):

- **`-`:** the value as it is printed in the smallest width (as with the width 0) goes at the left of its field, and spaces fill the rest. The field is the written width, or the automatic width when none is written (3.3, 3.5, 3.7). With a 32-bit `int` 5, `%-4d` is `5` and 3 spaces, `%-d` is `5` and 10 spaces, and `%-h` is `5` and 7 spaces. A field narrower than the value widens, as in 3.3. It applies to `%d`, `%h`, `%o`, `%b`, `%c`, `%s` and `%t`, and to values of the string type with `%s`.
- **`0`:** a written width that starts with 0 and is not 0 (`%05d`) pads `%d` on the left with 0 instead of spaces. The sign comes first (`-0003`), and a single `x` or `z` (3.4) is padded too (`0000x`). `%h`, `%o` and `%b` pad with 0 already and are unchanged. It is ignored for `%c`, `%s` and `%t`. `%0d` stays the width 0 (3.3).
- **Both:** `-` wins, as in C.
- **Other flags:** the flags `+`, space and `#`, and a precision, in a format other than `%e`, `%f` and `%g`, are reported as unsupported (CLI.md 5). slang already reports them as errors in string literal formats, so this covers the constant formats of `$sformat` and `$sformatf` that are not literals (3.13). slang also reports a width on `%c` (a `-` included) in a string literal as an error.
- **Warning:** these are formats not in the standard, so when the format is constant, kirimasim gives one warning at elaboration for each format that uses them, at the format's position, naming the first such flag: `<file>:<line>:<column>: warning: format flag '-' with %d is not in IEEE 1800-2023`. A format lowered for several instances warns once. Its ID comes with the scheme of diagnostic IDs (CLI.md 6).
- For reference (checked 2026-10-18): Icarus 14.0 devel prints `%-d` with no width in the automatic width as here, but keeps the leading zeros of `%h`, `%o` and `%b` (`%-8h` of a 32-bit 5 is `00000005`, `%-5b` of a 4-bit 5 is `0101` and a space), and pads `%c` and `%t` with 0 too. Verilator 5.053 devel prints `%-d` and `%-h` with no width in the smallest width, and `%05d` of -3 as `000-3`. With a written width, both left-align `%d` and `%s` as here.
- The test suite does not put the results of these flags in expected values; kirimasim's own tests check them (tests/cli and the kernel's unit tests).

### 3.13 Formatting to strings

- The `$swrite` family, like `$fwrite`, reads every string literal argument as a format. The result goes into the first argument by the rules of assigning a string literal (5.9) (checked: 21.3.3).
- `$sformat` reads only its second argument as a format. The format need not be constant (checked: 21.3.3). `$sformatf` does the same and returns the result as a string.
- In `$sformat` and `$sformatf`, when there are too few or too many arguments for the format's specifiers, a warning is given and the run continues (checked: 21.3.3). A specifier with no argument prints nothing, and extra arguments are dropped (rule of this document). If the format is constant, it is an error at elaboration (checked: 21.3.3 allows making it an error when it is known statically).

## 4. Delayed display and monitoring

- `$strobe` prints with the values at the point after every event of the time it was called has finished, just before time advances (checked: 21.2.2). It is done in the Postponed region of SCHEDULER_SPEC.md.
- `$monitor` prints all its arguments once, in the same form as `$display`, at the end of a time when the value of any argument changed. Changes of `$time`, `$stime` and `$realtime` are not counted. Only one `$monitor` is active at a time; a new call replaces the previous one (checked: 21.2.3).
- The first print is done once at the end of the time `$monitor` was called (interpretation; the standard says it prints on each change and does not say when the first print is; for reference, Icarus and Verilator print once at the time it is called).
- `$monitoroff` stops it and `$monitoron` resumes it. `$monitoron` prints once whether or not a value changed. That print is done at the end of the time `$monitoron` was called (interpretation; the standard says "immediately after the call", but every print of `$monitor` is at the end of the time, so this is made uniform; for reference, Icarus prints at the end of the time and Verilator 5.052 does not print). It is enabled by default (checked: 21.2.3).
- **Order within one time (rule of this document):** in the Postponed region, the prints of `$monitor` and `$fmonitor` come first (`$fmonitor` in the order made, `$monitor` last), then `$strobe` and `$fstrobe` in the order called. The standard does not set this order (4.4.2.9 sets no order within the Postponed region). For reference, Icarus and Verilator both print `$monitor` first. The test suite does not depend on the order of `$monitor` and `$strobe` at the same time.
- An automatic variable as an argument of `$monitor` is an error (checked: 13.3.2; as SIR_SPEC.md 3).

## 5. Time

### 5.1 `$time`, `$stime`, `$realtime`

- `$time` returns the time in the time unit of the calling design element, rounded to a 64-bit integer (checked: 20.3.1). Exactly half goes away from zero (rule of this document; as SCHEDULER_SPEC.md 1).
- `$stime` returns the same value as a 32-bit unsigned integer; if it does not fit in 32 bits, the low 32 bits (checked: 20.3.2).
- `$realtime` returns the same time as a real (checked: 20.3.3). The internal 64-bit time divided by the ratio of units is computed in double precision (rule of this document).

### 5.2 `$timeunit` and `$timeprecision`

They return, as an exponent of a power of 10 (2 to -15), the time unit and precision of: the calling design element with no argument; the design element named by a hierarchical name; the compilation unit for `$unit`; the simulation for `$root` (checked: 20.4.1).

### 5.3 `$printtimescale`

Prints the unit and precision of the same targets as 5.2 in one line, `Time scale of (<name>) is <unit> / <precision>`. For `$unit` and `$root`, the name is `$unit` and `$root` (checked: 20.4.2). A unit has no space between number and symbol, as in `1ns` and `100ps` (checked: the example of 20.4.2). The name is the hierarchical name of the calling design element with no argument, and the name given for a hierarchical-name argument (interpretation).

### 5.4 `$timeformat`

- Its four arguments (unit, digits after the decimal point, suffix string, minimum width) set the form of every later `%t` (checked: 20.4.3).
- The defaults are: the unit is the finest precision in the design, 0 digits, an empty suffix, a minimum width of 20 (checked: Table 20-3). The standard gives the default unit as "the smallest precision of `` `timescale ``". kirimasim uses the smallest precision of the whole design including `timeprecision` declarations (the simulation's time unit) (interpretation).
- A call with the arguments omitted restores the defaults (interpretation; the standard does not say what omission means; to be checked against Icarus 13.0 in P1; not confirmed).

## 6. File input and output

### 6.1 Opening, closing and descriptors

- `$fopen(name)` opens for writing and returns an mcd. `$fopen(name, type)` opens in a form of Table 21-6 and returns a file descriptor (fd). If it cannot open, it returns 0 (checked: 21.3.1).
- An mcd is a 32-bit value with one bit set; bit 0 is standard output, and bit 31 is not used (checked: 21.3.1). kirimasim assigns the lowest free bit (rule of this document).
- An fd is a value with bit 31 set; `32'h8000_0000` to `32'h8000_0002` are standard input, standard output and standard error (checked: 21.3.1). kirimasim assigns the lowest free number from `32'h8000_0003` (rule of this document). Closed numbers are reused (checked: 21.3.1).
- File names are taken relative to the working directory kirimasim was started in (rule of this document).
- `$fclose` cancels the `$fmonitor` and `$fstrobe` of the descriptor (checked: 21.3.1). Input or output on a closed descriptor is an error: a warning is given and nothing is done (rule of this document).
- **Descriptors that are not open (added 2026-10-20, approved):** a descriptor with x or z bits, an fd that is not open, and a bit of an mcd whose file is not open count as not open. Output to one gives the run-time warning `kirima-file-not-open` and writes nothing there; the files of an mcd that are open are still written (rule of this document; for reference, checked 2026-10-20, Icarus 14.0 devel warns too).
- **`$fopen` that returns 0 (added 2026-10-20, approved):** a type not in Table 21-6 returns 0 with the run-time warning `kirima-fopen-mode`. A file that cannot be opened returns 0 with no message, as the design is to check the value (rule of this document; for reference, checked 2026-10-20, Icarus 14.0 devel does the same).
- **The files belong to the Context (added 2026-10-20, approved):** the table of open files is part of the Context (RUNTIME_ABI.md 9.3). When the run ends, however it ends, every open file is written out and closed. Only the immediate end at a second SIGINT (CLI.md 5) can lose what was not written out yet.

### 6.2 Output

- `$fdisplay`, `$fwrite`, `$fstrobe` and `$fmonitor` write to the mcd or fd of their first argument. Otherwise they are the same as the corresponding display tasks (checked: 21.3.2). An mcd can write to several files, one per bit. Any number of `$fmonitor`s can be active at once (checked: 21.3.2).
- `$fflush` flushes the buffer of the descriptor given, or of every open file with no argument (checked: 21.3.6).
- **When `$fstrobe` and `$fmonitor` read their descriptor (added 2026-10-20, approved):** the descriptor is read when the task is called; the other arguments are read in the Postponed region, as for `$strobe` and `$monitor` (4). They write to the files the descriptor named at the call. `$fclose` takes the files it closes out of the pending `$fstrobe`s and the active `$fmonitor`s, without a warning, and a `$fmonitor` left with no file ends. A file opened later under a reused number is not written by them (interpretation: 21.3.1 says that `$fclose` cancels them; that the descriptor is read at the call, and that closing some files of an mcd leaves the others, are rules of this document. For reference, checked 2026-10-20 with Icarus 14.0 devel: it reads the descriptor at the call too, but it wrote a pending `$fstrobe` into a file opened later under the reused number, and dropped the whole print of an `$fstrobe` to an mcd one of whose files was closed).
- `$monitoron` and `$monitoroff` do not affect `$fmonitor` (interpretation: 21.2.3 gives them for `$monitor`; for reference, Icarus 14.0 devel does the same).

### 6.3 Input

- `$fgetc` returns one byte, or -1 on an error or EOF. `$ungetc` pushes back one character (checked: 21.3.4.1).
- `$fgets` reads until the variable is full, a newline (which is included), or EOF, and returns the number of characters read; 0 on an error (checked: 21.3.4.2).
- `$fread` reads 2-state data byte by byte, most significant first. When reading into a memory it follows start and count (checked: 21.3.4.4).
- `$fscanf` and `$sscanf` (checked: 21.3.4.3):
  - White space in the format skips white space in the input. For `$sscanf`, NUL characters count as white space too.
  - An ordinary character must match the same character in the input.
  - A `%` is followed by `*` to suppress the assignment, a maximum width, and the conversion character. Except for `%c`, leading white space of the input is skipped.
  - The set of characters each conversion reads is as in Table 21-7. `%b`, `%o` and `%h` (`%x`) read, besides the digits of their radix, `x`, `X`, `z`, `Z`, `?` and `_` as digits. `?` is the digit z, as `?` in a number is z (VALUE_SEMANTICS.md 4.2), and a lone `?` for `%d` is z in every bit (interpretation; Table 21-7 lists `?` without its value. For reference, checked 2026-10-18: Icarus 13.0 and 14.0 (devel) read it as x). `%d` reads an optional sign (`+` or `-`) followed by decimal digits and `_`, or a single one of `x`, `X`, `z`, `Z`, `?`. **There are no radix prefixes (`0x`, `0o`, `0b`).** Reading `"0xCAFE"` with `%h` reads the 6 digits `0`, `x`, `C`, `A`, `F`, `E`, and a 4-state variable gets `32'h000xcafe`. Reading `"0o777"` with `%o` ends the field at `o`, and the value is 0.
  - `%f`, `%e` and `%g` read C's floating-point form. `%t` reads the same form and converts it by `$timeformat`.
  - A value that does not fit the variable puts its low bits in. If the destination is a real and the value cannot be represented, `+Inf` or `-Inf` is put in.
  - If the format or `$sscanf`'s input string has x or z bits, -1 is returned.
  - The return value is the number of items matched and assigned; -1 (EOF) if the input ran out before the first conversion.
  - What happens when there are too few arguments for the format is not set by the standard. kirimasim makes it an error (rule of this document; at elaboration if the format is constant, otherwise a warning at run time, and the missing ones are not read).
- **Which descriptors are read (added 2026-10-21, approved):** the input functions take an fd. An mcd, an fd that is not open and a descriptor with x or z bits are not open: the run-time warning `kirima-file-not-open` (6.1) is given, and the function returns its error value: -1 for `$fgetc`, `$ungetc`, `$fscanf`, `$ftell`, `$fseek` and `$rewind`, and 0 for `$fgets`, `$fread` and `$feof` (rule of this document; 21.3.8 gives `$feof` only zero or nonzero, and a file that is not open has met no end. For reference, checked 2026-10-21: Icarus 13.0 and 14.0 devel warn too, and their `$feof` returns -1). `$ferror` on one returns the number of EBADF and its text with no warning, since it is the way to ask.
- **Standard input (added 2026-10-21, approved):** `32'h8000_0000` reads kirimasim's standard input (checked: 21.3.1 names it STDIN; for reference, Icarus reads it too). `tools/run_suite_test.py` gives each test an empty standard input (TEST_SUITE.md).
- **Files that only write (added 2026-10-21, approved):** reading a file opened with `w`, `wb`, `a` or `ab` fails with no warning: `$fgetc` returns -1, `$fgets` 0, `$fscanf` -1 and `$fread` 0, and `$ferror` then gives the number of EBADF (rule of this document; for reference, checked 2026-10-21, Icarus gives the same values). The types for update (`r+`, `w+`, `a+` and their `b` forms) read too (interpretation: Table 21-6 calls them reading and writing, although 21.3.4 names only `r` and `r+`).
- **`$fgets` into an integral variable (added 2026-10-21, approved):** it reads at most floor(width / 8) characters (checked: 21.3.4.2 leaves out a partial top byte). They are placed as a string literal is, the last character in the lowest byte, and the bits above them become 0 (rule of this document; for reference, Icarus does the same).
- **`$fread` (added 2026-10-21, approved):** an integral variable takes ceil(width / 8) bytes, the first byte in the most significant place, and the bits above its width are dropped (checked: 21.3.4.4). When the file ends partway, the bytes not read keep their old value, and the number of bytes read is returned (rule of this document; for reference, Icarus does the same). A memory is read element by element, each as an integral variable, from start and for at most count elements (checked: 21.3.4.4). An element that is an unpacked structure is read member by member in declaration order, the members of a nested structure in place, each as an integral variable, and the first member not read whole ends the reading (checked: 21.3.4.4; added 2026-10-21, approved). Structures with a member that is not integral (an array, a real), memories of more than one dimension of structures, and unpacked unions are not supported yet. slang takes no unpacked structure variable as the first argument, only memories of them (checked 2026-10-21).
- **How `$fscanf` takes characters (added 2026-10-21, approved):** `$fscanf` reads as `$sscanf` does, and takes from the file exactly the characters its conversions read; the next read starts at the first character not taken. `%f` on `1e+x`, for example, reads 1.0 and leaves `e+x`, as `$sscanf` does (rule of this document; for reference, checked 2026-10-21, Icarus takes `1e+x` and assigns nothing, as C's `fscanf` may).
- **`$ungetc` (added 2026-10-21, approved):** any number of characters can be pushed back, and the last one pushed is read first (rule of this document; for reference, Icarus does the same with two). It returns 0, or -1 when the descriptor is not open.

### 6.4 Position, status, EOF

- `$ftell`, `$fseek` and `$rewind` are the same as C's `ftell` and `fseek` (checked: 21.3.5). Changing the position discards what `$ungetc` pushed back. `$ftell` counts what was pushed back as not yet read (added 2026-10-21, approved).
- `$ferror` returns the error number of the last input or output and puts a description string in its argument. With no error it returns 0 and empties the string (checked: 21.3.7). The number and string are the OS's `errno` and `strerror` as they are (rule of this document; they can differ by OS, so the test suite looks only at whether it is 0). **Which error (added 2026-10-21, approved):** each fd keeps the error of its most recent operation, none when it succeeded; meeting the end of the file is not an error (as C's `ferror`). The text goes into a string variable, or into a packed variable as a string assignment puts it (21.3.7 asks for at least 640 bits).
- `$feof` returns nonzero if EOF has already been detected (checked: 21.3.8). It returns 1 once a read has met the end of the file, until `$fseek` or `$rewind` (added 2026-10-21, approved; rule of this document, as C's `feof`).

## 7. Reading and writing memories

### 7.1 `$readmemb` and `$readmemh`

Checked (21.4):

- A file can contain only white space, comments (both kinds), and binary or hexadecimal numbers. Numbers have no width or radix. `x`, `z` and `_` may be used.
- Numbers go into the words of successive addresses. An address can be given by `@` followed by a hexadecimal number (no space between `@` and the number).
- With no address arguments, reading goes up from the lowest address of the memory. With a start only, up from there. With start and end, from start to end (down if start is larger). This direction stays the same after an address in the file.
- If an address range is given by the arguments and an address in the file is outside it, it is an error and reading stops. A proposal to ignore addresses out of range instead is not taken (decided 2026-10-19; UNSPECIFIED_BEHAVIOR.md 9).
- If the file has no addresses and the number of words differs from the number of words in the range, a warning is given. Words not given are not changed.
- **When the arguments give no finish address (rule of this document; approved 2026-10-19):** the range ends at the highest address of the memory, and without a start address it begins at the lowest. The two rules above hold for that range too: an address in the file outside it is an error and reading stops, and a file with no addresses whose number of words differs from the number of words in the range gives the warning. Words past the end of the range are not loaded (checked: 21.4 stops loading at the highest address of the memory).
- For 2-state types, x and z become 0. A value out of range of an enumerated type is an error, and reading stops there (21.4.2).
- **Enumerated types (added 2026-10-21, approved):** a word goes into an element as into a memory of the base type (the low digits; x and z as 0 for a 2-state base type), and must then be the value of one of the type's items; one that is not is out of range (interpretation of 21.4.2). It is a run-time error of the design, as text that is no number is: the run ends there, and the words read before stay (rule of this document; for reference, checked 2026-10-21, Icarus 14.0 devel loads such a word and goes on).
- Multidimensional unpacked arrays are read in row order (the rightmost dimension changes fastest), each dimension below the first from its low to its high index. An address points to a word of the topmost dimension (checked: 21.4.3).
- **The digits of a word (added 2026-10-21, approved):** a number with fewer digits than the word leaves 0 above them, and one with more keeps its low digits, as an assignment does (rule of this document; for reference, checked 2026-10-21, Icarus 14.0 devel does the same, and warns of the extra digits).
- Dynamic arrays and queues do not change size. Associative arrays get elements made at the addresses read (21.4.1).
- If the file cannot be opened, it is an error and the memory is not changed (rule of this document). **The kind of error (added 2026-10-21, approved):** that, and text in the file that is no number, address or comment, are run-time errors of the design (CLI.md 5): the run ends there, and the words read before stay (rule of this document; for reference, checked 2026-10-21, Icarus 14.0 devel reports an error and goes on).

### 7.2 `$writememb` and `$writememh`

- A file of the same name is overwritten (checked: 21.5).
- One word per line. Hexadecimal is lower case, with every digit of the word's width including leading zeros. x and z are written by the rule of 3.4 (rule of this document; the standard does not set the form of a line).
- **The order of the words (added 2026-10-21, approved):** the address arguments take the range as `$readmem` takes it, and the words are written in the order `$readmem` reads that range, each dimension below the first from its low to its high index, so that reading the file back with the same arguments gives the same values (rule of this document; 21.5 says the range is written as `$readmem` reads it).
- For unpacked arrays and dynamic arrays no addresses are written; for associative arrays a line `@<address>` precedes each word (checked: 21.5.3). Enumerated types are written as numbers (21.5.2).

## 8. Conversion and queries

- `$rtoi` truncates toward zero. Casts and assignments round (checked: 20.5; rounding is VALUE_SEMANTICS.md 1.5). The truncated integer, like the conversion of VALUE_SEMANTICS.md 7.4, keeps its low bits as two's complement in the width of the result type (`integer`) whatever its size. NaN and infinities make every bit of the result x (rule of this document; approved 2026-10-17; the wording of the standard does not settle it; for reference, checked 2026-10-17, Icarus 14.0 devel gave x for NaN and infinities and `ffffffff` for 1e20, and Verilator 5.053 devel clamped to the ends of the 32-bit range; kirimasim follows the same rule as conversion).
- `$realtobits` and `$bitstoreal`, and `$shortrealtobits` and `$bitstoshortreal`, convert to and from the bit patterns of IEEE 754 double and single precision (checked: 20.5).
- `$signed` and `$unsigned` change only signedness, not width or value (checked: 20.5, 11.7).
- `$bits` takes only an expression or type of a bit-stream type (6.24.3) and returns the number of bits. One 4-state bit counts as 1. The expression is not evaluated. An empty dynamic expression is 0. Passing the name of a dynamic type directly, or an interface class object, is an error (checked: 20.6.2). Passing a real is an error too (real is not among the bit-stream types of 6.24.3).
- `$isunbounded` returns 1 if the value of the parameter is `$` (checked: 20.6.3).
- `$typename` (checked: 20.6.1): typedefs are resolved to the original type, default signedness is removed, the implementation names unnamed structures, unions and enumerated types, enumeration constants get their values, user type names get their package or scope name, and array ranges are written in decimal without sizes. On spaces, the rule (one space between tokens) and the example (`bit [2:0]`) read differently. kirimasim follows the example and puts one space between a keyword or identifier and the packed range after it (interpretation; for reference, Verilator 5.052 gives `logic[7:0]`). The test suite checks only the identity of types (the same type gives the same string). Unnamed enumerated types, structures and unions are named by the namespace where they were declared and their order of appearance in it, as `e$1`, `s$1`, `u$1` (rule of this document; the form of the example of 20.6.1; approved 2026-10-17). The string is made by front from slang's type. The strings of slang's constant evaluation put no space before the packed range (`logic[7:0]`), so they differ from the rule of this section (checked 2026-10-17).
- Array queries (checked: 20.7): dimensions are numbered with the slowest-varying dimension as 1. Integer types (`int` and the like) are taken as one packed dimension `[n-1:0]`. `$dimensions` is the total number of dimensions for an array, 1 for string and simple bit-vector types, and 0 otherwise. If the dimension number is out of range, or the type has `$dimensions` 0, `'x` is returned. Dynamic arrays and queues return their present state (`$left` is 0; when empty, `$right` is -1 and `$increment` -1). The rules of associative arrays are as in 20.7.
- `$clog2` takes its argument as unsigned and returns the base-2 logarithm rounded up to an integer; 0 for 0 (checked: 20.8.1). If the argument has x or z, it returns `'x` (rule of this document).
- The real math functions return the same results as the C functions of Table 20-4 (checked: 20.8.2). kirimasim calls the host's C library. The last digit may differ by library, so this is an exception to NFR-2 (a rule of the design specification). The test suite compares all but `$sqrt`, `$floor` and `$ceil` with a tolerance.
- Bit-vector functions (checked: 20.9): `$countbits` returns the number of bits that match any of the control bits (only the least significant bit of each is used) when the expression is made into a vector of the same width by the streaming operator. `$countones`, `$onehot`, `$onehot0` and `$isunknown` are defined with it. x and z bits do not match `'1`, so `$onehot(8'b0001x000)` is 1.

## 9. Plusargs

Checked (21.6):

- `$test$plusargs(s)` returns nonzero if any plusarg on the command line (with its leading `+` removed) starts with s, and 0 otherwise. When an integral variable is passed, leading NULs are ignored.
- `$value$plusargs("name format", v)` converts the rest of the first plusarg starting with the name by the format (`%d`, `%o`, `%h`, `%x`, `%b`, `%e`, `%f`, `%g`, `%s`; upper case and `%0` forms allowed), puts it in v, and returns nonzero. If none is found it returns 0, leaves v unchanged and gives no warning.
- If the rest is empty, 0 or an empty string is put in. A value narrower than the variable is extended with 0, and a wider one truncated. A negative value counts as wider than the variable. If there is a character that cannot be converted, `'bx` is put in.
- The empty string `""` matches every plusarg (interpretation; it follows from the rule "starts with s"). With no plusargs at all, 0 is returned.

How plusargs are passed is set in CLI.md.

## 10. Form of messages

### 10.1 Severity tasks

`$fatal`, `$error`, `$warning` and `$info` give a message containing the file name and line number (the same as `` `__FILE__ `` and `` `__LINE__ ``), the hierarchical name of the calling scope, the simulation time, and the user's message (checked: 20.10). The user's message is made with the same format as `$display`.

kirimasim's form (rule of this document), one line on standard error:

```text
<file>:<line>: <fatal|error|warning|info>: <user's message> [<hierarchical name> @ <time>]
```

- The time is written as an integer of the simulation's time unit with the unit (for example `@ 15000ps`).
- With no user's message, what follows `<severity>:` is empty.
- Elaboration-time severity tasks (20.10.1) write no time.

### 10.2 Reports of `$finish` and `$stop`

- Argument 0 prints nothing. 1 (the default) prints the time and the place of the call; 2 also prints memory and CPU time (checked: 20.2; SCHEDULER_SPEC.md 16).
- kirimasim's form (rule of this document), on standard error when the call runs: `<file>:<line>: $finish at <time>` (`$stop at` for `$stop`), with the time written as in CLI.md 4 (for example `5 ns`). With 2, one more line at the end of the run, after the line of the reason for ending: `kirimasim: info: statistics ` followed by the JSON object that `--stats=json` writes, with the values once the run has ended (decided 2026-10-18, approved: at the call, the time and reason of the end are not known yet). The memory and CPU time the standard names are not among those values yet.
- `--stop` chooses whether `$stop` ends the run (SCHEDULER_SPEC.md 16.2, CLI.md 2.2; approved 2026-10-19). The line above is written whichever it chooses.
- For `$finish(0)` and `$stop(0)`, the line of the reason for ending of SCHEDULER_SPEC.md 16.2 is not printed either (matching "prints nothing" of 20.2; approved 2026-10-15, with SCHEDULER_SPEC.md amended too). The reason for ending is kept in `--stats=json`.

### 10.3 `$stacktrace`

Called as a task it prints the chain of calls; called as a function it returns the same content as a string. The content is left to the implementation (checked: 20.17.2). kirimasim lists, from the innermost, one line each of `<hierarchical name of the scope> (<file>:<line>)` (rule of this document). Called as a task it writes to standard output (as `$display`; it is output the user asked for). The test suite does not put the content of `$stacktrace` in expected values. **The lines (added 2026-10-21, approved):** the first is where `$stacktrace` is called, with its scope as `%m` gives it; each line after it is the call that led to the one before, naming the task or function that made it, by its hierarchical name, or, for a procedure, the module instance it is in, with the place of the call (rule of this document). A fork branch is a process of its own, which no call led to. Called as a function, it returns the lines with a newline between them and none after the last, so that `$display("%s", $stacktrace)` prints what `$stacktrace;` does (rule of this document).

### 10.4 Reports of unique, unique0 and priority violations (approved 2026-10-16)

The standard leaves to the implementation how a violation is reported after it is settled in the Observed region (checked: 12.4.2.1). kirimasim writes one line per settled violation on standard error (rule of this document).

```text
<file>:<line>: warning: <construct> violation: <content> [<hierarchical name> @ <time>]
```

- `<file>:<line>` is the position of the `if` or `case` with the keyword `unique`, `unique0` or `priority`.
- `<construct>` is one of `unique case`, `unique0 case`, `priority case`, `unique if`, `unique0 if`, `priority if`.
- `<content>` is `no item matches` (case) or `no condition is true` (if) when none applies, and `more than one item matches` (case) or `more than one condition is true` (if) when two or more apply.
- `[<hierarchical name> @ <time>]` has the same form as 10.1 (the time in the form `@ 15000ps`). The hierarchical name is that of the instance containing the statement (inside a named block, including the block's name).
- The severity is warning. It does not affect the exit code (the standard does not make a violation an error). The option that counts `$error` (CLI.md 6) does not count it either.
- How violations are decided, and when they are pending and settled, follow SCHEDULER_SPEC.md 15.

## 11. Effect on the test suite

Approving this document settles these points for the test suite:

- The minimum width of `%0t` (3.7). The wording does not settle it, so the expected values of the test suite repository (kirimasim-tests) do not use `%0t`. kirimasim's own tests in `tests/suite/` follow this reading.
- When an explicit width is narrower than the automatic width (3.3). Icarus 13.0 differs from the standard. Tests whose expected values come from Icarus 13.0 avoid this form (continuing the present rule).
- Flags not in the standard (3.12), the spaces of `%p` (3.9), the numbers of `$ferror` (6.4), the form of severity tasks (10.1) and `$stacktrace` (10.3) are not put in expected values (continuing the present rule).
- The digit sets of `$sscanf` (6.3) are set by the standard, so they may be put in expected values.

## 12. `$random` and the `$dist_*` functions (added 2026-10-21, approved)

This section restates in words the algorithm that Annex N defines by C source code (N.2), so that kirimasim gives the same values without copying that code (UNSPECIFIED_BEHAVIOR.md K3): one author read N.2 and wrote this section, and another wrote the implementation from this section alone. Annex N is normative: where this section and N.2 disagree, N.2 holds. The points N.2 leaves open are settled by rules of this document (approved 2026-10-21), with what Icarus Verilog does for reference.

### 12.1 Sources

- 20.14: `$random`, `$dist_uniform`, `$dist_normal`, `$dist_exponential`, `$dist_poisson`, `$dist_chi_square`, `$dist_t` and `$dist_erlang` return integers, and their algorithm is part of the standard so that every implementation gives the same values; Annex N holds it (checked: 20.14).
- 20.14.1: `$random` takes an optional seed argument and returns a new 32-bit signed value at each call. The seed argument is an integral variable, given a value before the call (checked: 20.14.1).
- 20.14.2: every argument is an integer. For `$dist_exponential`, `$dist_poisson`, `$dist_chi_square`, `$dist_t` and `$dist_erlang`, the mean, the degree of freedom and k_stage are to be greater than 0. The seed is an inout argument: the same seed gives the same result, and only the function updates it. For `$dist_uniform`, start is to be less than end (checked: 20.14.2).
- N.1 maps each function to a routine of N.2, and makes `$random` the uniform distribution over the whole range of the C type `long` (checked: N.1).
- `$random` and `randomize()` are independent (checked: 18.14.3).

### 12.2 Arithmetic

- **Integers.** The seed, the arguments and the results are 32-bit two's complement signed integers. N.2 is written with C `long`, which this section reads as 32 bits: 20.14.1 gives `$random` a 32-bit signed value, which N.1's "whole range of `long`" is only with a 32-bit `long`; the seed update wraps modulo 2^32; and the test of a zero seed (12.4) is made at every draw. For reference, checked 2026-10-21: reading `long` as 64 bits disagrees with Icarus 14.0 devel in 11,738 of about 136,000 results; reading it as 32 bits agrees in all.
- **Reals.** Every real quantity is an IEEE 754 binary64 value, and every operation is rounded to nearest, ties to even. Below, (+), (-), (x) and (/) are those operations, done in the order written; `sqrt` is the correctly rounded square root, `ln` and `exp` the natural logarithm and the exponential of the C library. An integer of this section converted to a real is exact.
- **No fused multiply-add (approved 2026-10-21).** Each product and each sum is rounded by itself, as kirimasim's code is built with `-ffp-contract=off` (CODEGEN_SPEC.md, "Floating point"). C lets a compiler fuse `a*b + c` into one rounding, so N.2 as C does not settle this. Fusing changes results only at the three places marked "fusion point" below. For reference, checked 2026-10-21: Icarus 14.0 devel and 13.0 built for arm64 macOS fuse all three, and so give `$random` a result one greater for 64 of the 2^23 values of M (12.4); what builds for other machines do is not confirmed.
- **trunc(x)** is x rounded toward zero to an integer.

### 12.3 The seed

- **With a seed argument** (`$random(seed)` and every `$dist_*`): the state S is the variable's value when the function is called; the function updates S as 12.4 and 12.5 say, and the final S becomes the variable's value. A function that returns without a draw (the arguments of 12.6 that give 0) leaves S unchanged (checked: 20.14.2, N.2).
- **The width of the seed variable (approved 2026-10-21):** one of 32 bits or more is read through its low 32 bits, and the new seed is written back sign-extended to its width; x and z bits read as 0. A seed variable of fewer than 32 bits is reported as unsupported. (Rule of this document; the standard says only that the seed is an integral variable. For reference, checked 2026-10-21: Icarus 14.0 devel does the same for wider seeds and for x and z, and makes a narrower one an error.)
- **Without a seed argument (approved 2026-10-21):** `$random` uses a seed kept by the Context, one for the whole simulation and shared by every call, which starts at 0; its values are those of `$random(seed)` with a seed starting at 0. (Rule of this document; the standard fixes neither. For reference, Icarus 14.0 devel does the same.)

### 12.4 The draw

A draw D(lo, hi) takes two integers lo < hi, updates S, and gives a real. Every use below has lo < hi, and hi - lo is between 1 and 2^32 - 1.

1. If S = 0, S becomes 259341593.
2. S becomes (69069 * S + 1) mod 2^32, kept as a 32-bit pattern.
3. M is the top 23 bits of the new S, read as an unsigned number: M = floor(S_unsigned / 512), from 0 to 2^23 - 1.
4. F = 1 + M * 2^-23, exact as a real (the binary32 number with sign 0, biased exponent 127 and fraction M).
5. G = F + F * 2^-23, exact.
6. H = G - 1, exact: H = (M + 1) * 2^-23 + M * 2^-46. It is never 0 and never exactly 1; it is above 1 only when M = 2^23 - 1.
7. The value is ((hi - lo) (x) H) (+) lo, with hi - lo exact as a real. (Fusion point 1.)

Two draws have names of their own: U = D(0, 1), which is H, from 2^-23 to 1 + 2^-23 - 2^-46; and W = D(-1, 1), which is 2H - 1, from -1 + 2^-22 to 1 + 2^-22 - 2^-45, never 0. A draw can be above hi (M = 2^23 - 1), so the functions clamp, and the logarithm of a draw U can be positive.

### 12.5 Steps built on draws

Each step takes its draws from the same S, in the order stated.

- **Normal step N(mean, sd)**: (1) draw a1 = W, then a2 = W, and let w = (a1 (x) a1) (+) (a2 (x) a2), each product rounded, then the sum (fusion point 2); (2) if w >= 1 or w = 0, start again at (1) with two new draws; (3) z = a1 (x) sqrt(((-2.0) (x) ln(w)) (/) w), in that order; a2 does not enter z; (4) the value is (z (x) sd) (+) mean, with sd and mean as reals (fusion point 3).
- **Exponential step E(mean)**: draw y = U; the value is (-ln(y)) (x) mean. When M = 2^23 - 1, y is above 1 and the value is slightly negative.
- **Poisson step P(mean)**: L = exp(-mean); j = 0; draw prod = U. While L < prod: j becomes j + 1, a new U is drawn, and prod becomes U (x) prod. The value is j. For a large mean, L is 0 and the loop ends only when prod becomes 0.
- **Chi-square step C(df)**, df > 0: if df is odd, acc = z (x) z with z the value of N(0, 1); if even, acc = 0. Then floor(df / 2) times, acc becomes acc (+) (2 (x) E(1)). The value is acc.
- **Student t step T(df)**, df > 0: y = C(df); den = sqrt(y (/) df); z = N(0, 1), drawn after all the draws of C; the value is z (/) den.
- **Erlang step R(k, mean)**, k > 0: prod = 1.0; k times, prod becomes prod (x) U. The value is ((-mean) (x) ln(prod)) (/) k, with mean and k as reals; ln(0) is minus infinity.

### 12.6 The results

- **round(y)**: if y >= 0, trunc(y (+) 0.5); otherwise -trunc((-y) (+) 0.5). The 0.5 is added in real arithmetic, so that the rounding of that sum counts. A NaN takes the second branch.
- **Values outside 32 bits (approved 2026-10-21):** where N.2 converts a real outside -2^31 .. 2^31 - 1, an infinity or a NaN to a 32-bit integer, which C leaves undefined, kirimasim takes the nearest of -2^31 and 2^31 - 1, and 0 for a NaN. So round gives 2147483647 or -2147483647 there (the magnitude is converted before the sign is applied). (Rule of this document. For reference, checked 2026-10-21: Icarus 14.0 devel and 13.0 give the same.)
- **`$dist_uniform(seed, start, end)`**:
  - If start >= end, the result is start and the seed is unchanged.
  - Otherwise, if end is not 2^31 - 1: e = end + 1 and y = D(start, e). k = trunc(y) if y >= 0, and trunc(y (-) 1) if y < 0. If k < start, k becomes start; if k >= e, k becomes end. The result is k.
  - Otherwise, if start is not -2^31: b = start - 1 and y = D(b, end) (+) 1.0; k as before. If k <= b, k becomes start; if k > end, k becomes end. The result is k.
  - In these two cases k is clamped as a mathematical integer; N.2 converts it to 32 bits first, which leaves its range only in corners near the ends of the 32-bit range, where the clamping gives start or end (interpretation; for reference, Icarus 14.0 devel gives the same).
  - Otherwise (start = -2^31 and end = 2^31 - 1): v = D(-2^31, 2^31 - 1); y = (v (+) 2147483648.0) (/) 4294967295.0; y = (y (x) 4294967296.0) (-) 2147483648.0; k as before, with no clamping. When M = 2^23 - 1, k is 2^31 + 511, which the rule above makes 2147483647.
- **`$random(seed)`** is `$dist_uniform(seed, -2^31, 2^31 - 1)`. `$random` alone does the same with the Context's seed (12.3).
- **`$dist_normal(seed, mean, standard_deviation)`**: round(N(mean, standard_deviation)). Nothing is checked: a standard deviation of 0 gives mean, after the draws.
- **`$dist_exponential(seed, mean)`**: if mean <= 0, 0 and the seed unchanged; otherwise round(E(mean)).
- **`$dist_poisson(seed, mean)`**: if mean <= 0, 0 and the seed unchanged; otherwise P(mean).
- **`$dist_chi_square(seed, degree_of_freedom)`**: if degree_of_freedom <= 0, 0 and the seed unchanged; otherwise round(C(degree_of_freedom)).
- **`$dist_t(seed, degree_of_freedom)`**: if degree_of_freedom <= 0, 0 and the seed unchanged; otherwise round(T(degree_of_freedom)).
- **`$dist_erlang(seed, k_stage, mean)`**: if k_stage <= 0, 0 and the seed unchanged; otherwise round(R(k_stage, mean)); mean is not checked.
- **Arguments out of their range (approved 2026-10-21):** a mean, degree of freedom or k_stage that is 0 or less gives, besides the result 0, the run-time warning `kirima-dist-argument`, once for each place (rule of this document; N.2 prints a warning there, whose wording is not followed).

### 12.7 Limits and checks

- `ln` and `exp` come from the C library, which C does not require to be correctly rounded. A last-bit difference changes an integer result only when a real falls on a rounding or comparison boundary. For reference, checked 2026-10-21 on arm64 macOS: correctly rounded `ln` and `exp` changed none of the results compared with Icarus. Whether some reachable result depends on them is not confirmed.
- Values to check against (from seed = 1 unless said otherwise; each result with the seed after the call):
  - `$random(seed)` from 0: 303379748, -1064739199, -2071669239, -1309649309, 112818957; the seed after the first call is -1844104698.
  - `$random(seed)` from 1: -2147414528 (69070), -1671855048 (475628535), 1129920902 (-1017563188).
  - `$dist_uniform(seed, -5, 5)`: -5 (69070), -4 (475628535), 3 (-1017563188).
  - `$dist_normal(seed, 100, 10)`: 106 (772999773), 76 (2044158073), 102 (797919023).
  - `$dist_exponential(seed, 10)`: 110 (69070), 22 (475628535), 3 (-1017563188).
  - `$dist_poisson(seed, 5)`: 0 (69070), 5 (1662200408), 5 (1059494674).
  - `$dist_chi_square(seed, 3)`: 1 (-417135238), 2 (2044158073), 1 (-1551342684).
  - `$dist_t(seed, 5)`: -6 (2044158073), 0 (584849259), 2 (1692139853).
  - `$dist_erlang(seed, 3, 100)`: 450 (-1017563188), 64 (-473131853), 61 (-505977370).
  - Fusion point 1: `$random(seed)` from -1709352709 gives -1073741185 (Icarus on arm64: -1073741184).
  - Outside 32 bits: `$random(seed)` from -1271221770 gives 2147483647, with the seed -1.
  - A zero seed inside one call: `$dist_normal(seed, 100, 10)` from 1511872763 gives 86 (-2034665166), then 106 (851608272).

## 13. `$urandom`, `$urandom_range`, `randcase` and the seed (added 2026-10-21, approved)

The standard leaves the generator of `$urandom` and the default seed to the implementation (checked: 18.13.1, 18.14.1). This section fixes them by rules of this document (approved 2026-10-21), which are kept across versions (UNSPECIFIED_BEHAVIOR.md K1, K3). The random state of objects, and the methods that seed or read a random state, come in P3 (RANDOMIZE_SPEC.md).

### 13.1 The generator

- Each random number generator (RNG) is SplitMix64: a state of 64 bits. A step adds `0x9e3779b97f4a7c15` to the state, and its value is the new state z after `z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9`, `z = (z ^ (z >> 27)) * 0x94d049bb133111eb` and `z = z ^ (z >> 31)`, all modulo 2^64 (rule of this document; the algorithm published by Steele, Lea and Flood in 2014). It uses integer arithmetic only, so every OS and tier gives the same values.
- A random number of 32 bits is the upper 32 bits of a step's value. The seed an RNG gives a new one is the whole value of a step, which becomes the new RNG's state.

### 13.2 The seed and the RNGs

- **The default seed:** `--seed=<n>` (CLI.md 2.2), a whole number below 2^64, 0 when not given. The version line names it (CLI.md 4).
- **Initialization RNGs:** each module instance and each package has one (checked: 18.14.1). Its state starts as the default seed XOR the 64-bit FNV-1a hash of the bytes of its hierarchical name as `%m` prints it (for a package, its name), so that the instances of one module draw different numbers and a new instance leaves the numbers of the others as they were (rule of this document; 18.14.1 seeds every one with the default seed, and leaves that seed to the implementation).
- **Procedures:** before time 0, each `initial`, `always` (of every kind) and `final` procedure of an instance gets an RNG whose state is the next seed of the instance's initialization RNG, in the order of their declaration in the instance, the procedures of generate blocks in place (checked: 18.14.1, thread stability; the order is a rule of this document). The procedures take their seeds before any static initializer runs. Tasks and functions draw from the RNG of the process that calls them.
- **Fork branches:** when a `fork` starts, each branch, in order, gets an RNG whose state is the next seed of the RNG of the process that forks (checked: 18.14.2, hierarchical seeding).
- **Elsewhere:** a `$urandom` evaluated outside a procedure (in a static variable's initializer, a continuous assignment, or the arguments of `$strobe` and `$monitor`) draws from the initialization RNG of its instance or package (rule of this document; 18.14.1 makes the initialization RNG seed static initializers).

### 13.3 The functions

- **`$urandom`** returns the next random number of 32 bits of the RNG. **`$urandom(seed)`** first gives that RNG the state of the seed's low 32 bits, as an unsigned number, then returns the next one (checked: 18.13.1 makes the seed set the sequence; the state it gives is a rule of this document).
- **`$urandom_range(maxval, minval = 0)`:** when maxval is less than minval, the two are swapped (checked: 18.13.2). With r the next random number of 32 bits and n = maxval - minval + 1 (from 1 to 2^32), the result is minval + floor(r × n / 2^32) (rule of this document).
- **`randcase`** (18.16): the weights are evaluated in the order of their branches, and added as unsigned values at the width of the widest weight (checked: 18.16; the order is a rule of this document). With a sum s of 0, no branch is taken, with no warning (18.16 makes the warning optional). Otherwise r is `$urandom_range(s - 1)` of the process's RNG, and the first branch whose weight, added to those of the branches before it, is greater than r is taken (checked: 18.16 gives the smaller numbers to the earlier branches). A sum wider than 32 bits is not supported yet.
- Not in P1: `srandom`, `get_randstate` and `set_randstate` of processes and objects, `std::randomize`, `shuffle` and `randsequence` (P3).

## 14. Not confirmed, to be written later

- The conversion when a real is printed with an integer format (and the reverse) (3.6).
- The meaning of `$timeformat` with its arguments omitted (5.4), to be checked against Icarus 13.0.
- When `$monitor` first prints and when `$monitoron` prints (4), to be checked against Icarus 13.0 in P1.
- `%v` and the strength model (3.10; P4).
- Details of order at the same time for `$fmonitor`, `$fstrobe`, `$fgetc` and `$ungetc` (P3).
- The waveform tasks (`$dumpfile` and so on) are written in WAVEFORM.md (2026-10-21).
