# Owned strength SSA values and MOS execution

[日本語](strength-ssa-execution.ja.md). Status: implemented and checked,
2026-10-26. This connects internal SIR values; source MOS/switch support remains work.

## Grounds and boundary

The approved [MOS/switch contract](mos-switch-proposal.md) sections 1/2/7 and its
[normative mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC 4.1/5.1/10,
VALUE_SEMANTICS 5 and IEEE 1800-2023 28.7/28.9/28.13/28.14 are the grounds.
The implementation and tests are original. No external simulator implementation
or tests were read. The [pure operations](mos-strength-operations.md) remain the
kernel's scalar device semantics, cross-checked against `valsem`.

A new non-language type `strength<w>` owns one complete interval per bit. It has
positive width, no descriptor and no arithmetic signedness. It is SSA metadata,
not a SystemVerilog storage type or handle. The current verifier admits it as an
instruction result or block parameter, and rejects it in slots, constants,
frames, refs, type descriptors and subroutine parameter/return interfaces.
Only the dedicated operations and typed branch arguments may consume it;
ordinary arithmetic, conversions, selects, system tasks and builtins cannot
silently interpret its bytes as four-state values or handles. Existing SSA
use-before-definition, dominance and no-use-across-wait rules apply unchanged.

Three operations are connected end to end:

- `strength.make`: a captured four-state value of width w, two constant ranks
  0..7, and an optional captured two-state side mask of width 2w, producing
  `strength<w>`. Known signed/unsigned ranks of any representable width are
  accepted; negative or X/Z ranks are rejected. Low mask bits enable the one
  side, high bits the zero side, including unaligned halves at widths 33/65/129.
- `strength.mos`: one of `nmos`, `pmos`, `rnmos`, `rpmos`, `cmos`, `rcmos`, a
  matching strength value, and one scalar control (two for CMOS), producing
  the same strength type. Both state domains and either control signedness
  are accepted. Device names are decoded once; the VM delegates interval
  behavior to the existing shared-kernel MOS operation.
- `strength.value`: a complete strength value to unsigned `i4<w>` through the
  existing projection. This is the explicit point where endpoints are discarded.

The other contract operations (`strength.load`, `strength.drive` and its delayed
form) require net/driver stores and owned registrations and are not implemented
by this checkpoint. No front-end source construct is enabled here.

## Transport and shared kernel

`kernel/strength_value.h` exposes C++20 nonallocating, nonthrowing packed-value
operations, independent of SIR, slang and LLVM. Each W4-sized storage word holds
eight signed lo bytes in `a` and eight hi bytes in `b`, with the least significant
byte first by explicit shifts. These fields are endpoint storage, not aval/bval.
No host byte-order reinterpretation, pointer or GC-root encoding is used.

The APIs validate complete input ranges, shapes, ranks and kinds before changing
output. Output must be disjoint from inputs. Successful operations zero unused
tail bytes/bits and leave surplus output words alone. Interval validation includes
the last bit even for disabled devices. The public-header probe and a standalone
compile with exceptions and RTTI disabled check C++20 compatibility.

The VM allocates `ceil(w/8)` words for each strength temp, instead of the ordinary
integral `ceil(w/64)`, with checked cumulative temp offsets. It snapshots inputs
and writes a distinct owned result; branch transfer copies every word and reads
all arguments before writing any parameter. Nested function execution uses the
same typed temp layout. No source callbacks or scheduling occur in these pure
instructions. VM input snapshots currently allocate; optimizing their transport
and measuring complete source-MOS performance remain work.

The format stays 0 and C function-table ABI stays unchanged. Text printing is
deterministic and includes the new types and op names. There is still no parser;
print/parse/print roundtrip guarantees remain the P2 task rather than evidence
claimed from repeated printing.

## Validation

`0ec3cc57` adds 13 original tests. Kernel checks cover all 120 valid intervals
at 32 packed positions (3,840 cases), 1,408 make/rank/side vectors across eleven
widths, and 12,384 packed-MOS comparisons against the independent point/control
reference. Tail clearing, untouched surplus words, invalid ranks/shapes/endpoints
and failure before partial output are checked. SIR cases reject metadata in
language storage/interfaces and ordinary consumers, malformed ranks/masks/kinds,
wrong projection types, decorated SSA operands and wait-crossing uses.

The VM runs 2,034 hand-built designs: 1,920 width/device/mask/control cases,
96 signed/state-domain controls, 12 parallel branch-copy cases and six nested
function cases with ordinary/scoped input fields. Both swapped parameters are
observed. All 67 focused cases pass, including all 29 unchanged existing dumps.

Nine [compiled controls](../measurements/2026-10-26/strength-sir-controls.json)
are caught: endpoint loss, a misaligned side half, incomplete prevalidation,
loss of X/Z projection, wrong VM word count, wrong p-device decoding, a syscall
accepting metadata, truncated branch copies and a missing mask-width check.
Every full mutated build succeeds, including the C++20 probe, then actual tests
fail. Compilation failures are not detections. Baseline and restored production
pass all 13 new cases; all 677 registered controls apply.

The [full local CI](../measurements/2026-10-26/strength-sir-validation.json)
passes all 21 checks: dev/ASan/UBSan 5,152/5,154, release 5,153/5,155, zero
failures and two known skips each. Existing dumps, PerfCount, static checks and
generated counts pass. The implementation-only commit passes the manual
expected-change check; no existing oracle changed. README counts are generated
from the fresh complete dev JUnit. These are local macOS arm64 results.

## Remaining execution work

The [subsequent kernel checkpoint](strength-store-publication.md) now provides
published interval storage, internal notices and atomic range/value publication.
Connect statically identified net ranges and captured indices through SIR
load/drive; provide physical driver IDs and origin identity. Add per-scalar inertial scheduling, the passive
world-preserving solver, force/release, port mappings and strength-aware formatting.
Primitive arrays must retain their scalar pending-state ownership. Preserve ordinary
initialization and apply the separately approved MOS startup policy at the real
scheduler connection. Existing expected source files and performance budgets remain
protected.

No new source-level Icarus comparison, external dataset run, Linux result or speed
comparison is claimed. The 21 historical Icarus-13-passing probe gaps remain work
and do not enumerate the whole missing language. P1/full Icarus coverage remains
incomplete. This work neither approves nor enables the separate managed-event
instruction-safepoint amendment.
