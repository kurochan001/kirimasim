# WAVEFORM: waveform output

[日本語](WAVEFORM.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

**Phase scope amendment (approved 2026-10-23):** [P1_SCOPE](P1_SCOPE.md)
advances the Icarus-required subset of later-phase or out-of-scope work into
P1. Existing semantic rules remain; missing details need review before code.
Phase labels below describe the original plan, subject to this amendment.

Priority: P1. Status: **approved (2026-10-21)**. The same day, what the implementation found the text did not say was added: the sizes of reals and events and how a reference is spelled (2), how the header is laid out (3), the control tasks in the state they set, `$dumpvars` with levels alone and naming what is not dumped, the texts of the warnings and the comment of `$dumplimit`, a file that cannot be opened, the end of a run cut short (4), the forms of `--wave` and `--wave-scope` and what they do without `--wave` (5), and how the tests read a dump (6) (approved).

This document sets what kirimasim writes as waveforms: the 4-state VCD file of 21.7.1 and 21.7.2, written by the dump tasks of the design or by the command line's `--wave` (CLI.md 2.2). The text of the standard is not copied; it is summarized in this document's own words, with clause numbers. "Checked", "interpretation", "rule of this document" and "for reference" mean what they mean in SYSTEM_TASKS.md.

## 1. Scope

- **In P1:** the 4-state VCD (21.7.1, 21.7.2) with `$dumpfile`, `$dumpvars`, `$dumpoff`, `$dumpon`, `$dumpall`, `$dumplimit` and `$dumpflush`, and `--wave=<file>.vcd` with `--wave-scope` and `--wave-glitches` (CLI.md 2.2).
- **Later:** FST (`--wave=<file>.fst`) needs libfstapi, a new dependency, which a person decides on before it comes (CLAUDE.md); until then `.fst` is an error of the command line. The extended VCD of 21.7.3 and 21.7.4 (`$dumpports` and the rest) is reported as unsupported.

## 2. What is dumped

- **Scopes (interpretation of 21.7.2.3 and 21.7.5):** module instances (scope type `module`), named `begin` and `fork` blocks (`begin`, `fork`), and tasks and functions that hold static variables (`task`, `function`). A named generate block is a `begin` scope (rule of this document; the 4-state VCD has no type for it). An unpacked structure variable is a `fork` scope whose members are its variables (checked: 21.7.5).
- **Variables and nets:** the nets and the static variables of those scopes, of the types of Table 21-11 and their 1364 equivalents. Unpacked arrays and automatic variables are not dumped (checked: 21.7.5); nor are strings, chandles, class handles and other dynamic data, which have no VCD type (rule of this document), nor parameters (rule of this document: the 4-state VCD can only list them with a value, outside the value changes).
- **Types (checked: 21.7.5, 21.7.2.3):** `bit`, `logic` and packed arrays and structures are `reg` of their whole width; `int` and `enum` are `integer` 32 (an enum with a base type takes that type's mapping); `byte`, `shortint` and `longint` are `reg` 8, 16 and 64; `integer` is `integer`, `time` is `time`; `real` and `shortreal` are `real`, `realtime` is `realtime`, all of size 64 (rule of this document: 21.7.2.3 makes the size the number of bits, and Table 21-11 gives none for them; added 2026-10-21, approved); nets take their kind (`wire`, `tri`, `wand`, `wor`, `triand`, `trior`, `tri0`, `tri1`, `supply0`, `supply1`), `uwire` as `wire` (checked: 21.7.2.3); `event` is `event` of size 1 (rule of this document, added 2026-10-21, approved).
- **Reference:** the variable's name, with `[msb:lsb]` when it was declared with one packed range, as declared, and `[width-1:0]` for the other integral types wider than one bit (rule of this document). The range follows the name after a space, as in `$var reg 4 ! a [3:0] $end` (added 2026-10-21, approved).
- **Order and identifier codes (rule of this document):** scopes depth first in the order of elaboration, and in each scope its nets and variables in the order of declaration, then its sub-scopes. Identifier codes are numbers in base 94 written with the characters `!` to `~` (33 to 126), most significant first, given in that order from `!`. Names that are one net after port collapsing (VALUE_SEMANTICS.md 5) share one code (checked: 21.7.2.3 allows it).

## 3. The file

- **Header (checked: 21.7.2.1, 21.7.2.3):** `$date`, the date and time the run started, which the driver gives to the Context (RUNTIME_ABI.md 9.3 item 8), in UTC as `YYYY-MM-DD hh:mm:ss UTC` (rule of this document); `$version`, `kirimasim <version>` and, on its own line, the `$dumpfile` call as written in the source, or `--wave=<file>` (checked: 21.7.2.3 asks for the call as written); `$timescale`, the simulation's precision, such as `1 ps`; the scopes and variables; `$enddefinitions`. The text of `$date`, `$version` and `$timescale` goes on lines of its own, each after a tab (rule of this document, added 2026-10-21, approved).
- **Times:** `#<n>` in units of the simulation's precision (checked: 21.7.2.1).
- **Values (checked: 21.7.2.2):** a 1-bit variable as `0`, `1`, `x` or `z` followed by its code, with no space; a wider one as `b` and its bits in the shortest form of Tables 21-8 and 21-9, a space and its code; a real as `r` and the number in the form `%.16g`, a space and its code. An event is `1` and its code at each time it is triggered.

## 4. When values are written

- **Start (checked: 21.7.1.3):** `$dumpvars` makes dumping start at the end of the time slot in which it runs: the header, the time, and a `$dumpvars` section with every dumped variable's value then, events left out.
- **Changes (UNSPECIFIED_BEHAVIOR.md D3, approved 2026-10-19):** at the end of each later time slot, the time and the variables whose value differs from the value last written; with `--wave-glitches`, every change within the time slot, in order, each written as it happens.
- **The control tasks (rule of this document):** `$dumpoff`, `$dumpon` and `$dumpall` take effect at the end of the time slot in which they run, after its changes. `$dumpoff` writes a `$dumpoff` section with every variable as x (reals are left out, as they have no x), and no change is written until `$dumpon`, which writes a `$dumpon` section with the values then (checked: 21.7.1.3). `$dumpall` writes a `$dumpall` section with the values then (checked: 21.7.1.4). Before `$dumpvars`, they do nothing. `$dumpoff` while dumping is off, `$dumpon` while it is on, and `$dumpall` while it is off do nothing (rule of this document, added 2026-10-21, approved).
- **`$dumpvars` arguments (checked: 21.7.1.2):** with none, every scope. With `levels` and scopes or variables: a scope with 0 is dumped with every scope below it, with n with n - 1 levels of module instances below it (named blocks, tasks, functions and generate blocks go with their module); a variable is dumped alone. Several `$dumpvars` in one time slot add up. With levels alone, the top modules are dumped with those levels; a scope or variable named that is not dumped (an unpacked array, a string; 2) adds nothing (rule of this document, added 2026-10-21, approved). **A later `$dumpvars` (rule of this document):** the standard asks all of them to run at one time; one at a later time is ignored with the run-time warning `kirima-dumpvars-late`, `$dumpvars: dumping started at an earlier time; the call is ignored` (the text added 2026-10-21, approved).
- **`$dumpfile` (checked: 21.7.1.1):** names the file, `dump.vcd` when no `$dumpfile` ran. A later `$dumpfile` before dumping starts replaces the name; one after it started is ignored with the run-time warning `kirima-dumpfile-late`, `$dumpfile: dumping has started; the call is ignored` (rule of this document; the text added 2026-10-21, approved). The file is created when dumping starts, overwriting one of the same name, relative to the directory kirimasim was started in. A file that cannot be opened gives the warning `kirimasim: warning: the dump file "<name>" cannot be opened (<reason>); nothing is dumped`, and the run goes on (rule of this document, added 2026-10-21, approved).
- **`$dumplimit` (checked: 21.7.1.5):** when the file reaches that many bytes, dumping stops and a comment says the limit was reached. The size is looked at after the lines of each time slot are written, so the file can pass it by those lines; the comment is `$comment the dump limit of <n> bytes was reached $end` (rule of this document, added 2026-10-21, approved).
- **`$dumpflush` (checked: 21.7.1.6):** writes out what is buffered.
- **The end:** the file is written out and closed when the run ends, however it ends (as files, SYSTEM_TASKS.md 6.1); a final time line is not added (rule of this document). What changed after the end of the last time slot, in a time slot the end cut short or in a final procedure, is written then, as at the end of a time slot, and a `$dumpvars` of a time slot cut short starts the dump then (rule of this document, added 2026-10-21, approved).

## 5. `--wave`

- `--wave=<file>.vcd` dumps as if `$dumpfile("<file>.vcd"); $dumpvars;` ran at time 0 before every process; the design's `$dumpfile` and `$dumpvars` are then ignored, with one run-time warning `kirima-dump-command-line` for each place, `<task>: --wave sets what is dumped; the call is ignored`, and its `$dumpoff`, `$dumpon`, `$dumpall`, `$dumplimit` and `$dumpflush` still act (rule of this document; the text added 2026-10-21, approved). A file whose name does not end in `.vcd` is an error of the command line (added 2026-10-21, approved).
- `--wave-scope=<pattern>[,<pattern>...]` keeps the scopes whose hierarchical name, or an enclosing scope's, matches one of the patterns, where `*` matches any characters and `?` one (rule of this document). Without it, every scope. An empty pattern is an error of the command line. Without `--wave`, the patterns choose among what the design's `$dumpvars` dumps (added 2026-10-21, approved).
- `--wave-glitches` (CLI.md 2.2): see 4. Without `--wave`, it applies to the design's own dump (added 2026-10-21, approved).

## 6. Effect on the test suite

- The test tools compare a VCD file without its `$date` section, which changes with every run. The tests of the suite do it inside the design: after `$dumpflush`, it reads its dump back and prints it without the line of `$date` and the line `kirimasim <version>` of `$version`, which changes with every release (`tools/gen_dump_matrix.py`; added 2026-10-21, approved).

## 7. Not confirmed, to be written later

- FST (P1, with the decision on libfstapi).
- The extended VCD of 21.7.3 (P4, with strengths).
- What a dump of the variables of a program, an interface or a class gives (P3).
