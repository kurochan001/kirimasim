# Draft: exponential time in UDP edge-coverage checking

[日本語](slang-udp-coverage-issue-draft.ja.md)

Status: **draft, not posted; human review pending**, revised 2026-10-31.
Posting is on hold at the maintainer’s request (2026-10-31); wait for an explicit instruction to resume.

This updates the 2026-10-24 draft with standalone slang measurements at the
current pinned revision and upstream master.

## Summary

A sequential UDP with one wildcard hold row for each input causes compilation
time to grow rapidly with the number of inputs. A 33-input, 33-row example
exceeds a 15-second external limit with the standalone slang CLI, both at
master `82ed79add4bdc3bc730c00b8fe0c04b1a7714fdb` and at
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`. The rows cover every one-input
edge. No simulation is needed to reproduce the compilation problem.

## Verified environment

- macOS arm64, Apple clang `21.0.0 (clang-2100.3.34.2)`.
- Unmodified upstream source, standalone CMake Release build with
  `SLANG_INCLUDE_TOOLS=ON`, `SLANG_INCLUDE_TESTS=OFF` and mimalloc enabled.
  Existing fetched dependencies were reused through `FETCHCONTENT_SOURCE_DIR_*`.
- Master was fetched and checked on 2026-10-31; the pinned revision was tested
  first, then the same standalone build directory was rebuilt at master.
  Both source worktrees were checked for modifications. The source revisions,
  executable version strings and binary SHA-256 digests are in the evidence.

For a separate checkout, the ordinary standalone build is:

```sh
cmake -S slang -B slang-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSLANG_INCLUDE_TOOLS=ON -DSLANG_INCLUDE_TESTS=OFF
cmake --build slang-build --target slang_driver -j 8
```

## Original reproducer

Save this generator as `make_hold.py`:

```python
from pathlib import Path
import sys

n = int(sys.argv[1])
names = [f'i{i}' for i in range(n)]
rows = []
for i in range(n):
    tokens = ['?'] * n
    tokens[i] = '*'
    rows.append('    ' + ' '.join(tokens) + ' : ? : -;')
source = (
    'primitive hold_all(q, ' + ', '.join(names) + ');\n'
    '  output q;\n  reg q;\n  input ' + ', '.join(names) + ';\n'
    '  table\n' + '\n'.join(rows) + '\n  endtable\nendprimitive\n'
    'module top;\n  wire q;\n  reg [' + str(n - 1) + ':0] d;\n'
    '  hold_all u(q, ' + ', '.join(f'd[{i}]' for i in range(n)) + ');\n'
    'endmodule\n'
)
Path(f'hold_{n}.sv').write_text(source)
```

Generate and compile:

```sh
python3 make_hold.py 33
slang-build/bin/slang --top top hold_33.sv
```

The table specifies hold for each possible edge at each input, for every
state of the other inputs and the current output. This isolates the coverage
check from the simulation behavior of a data-dependent UDP.

## Observed results

Elapsed times below are single runs, with no other compilation running.
The timeout values are externally imposed observation bounds, not claims
that the compiler cannot eventually finish.

- Pinned `e2d102166`: 9 inputs **0.030 s**, 11 inputs **0.284 s**,
  13 inputs **3.217 s**. Each compilation exits 0 with no warnings.
- Master `82ed79add`: 9 inputs **0.034 s**, 11 inputs **0.294 s**,
  13 inputs **3.123 s**. Each compilation exits 0 with no warnings.
- The complete 33-input table exceeds **15 s** on both revisions, without
  producing output before it is stopped.
- The same 33-input table also exceeds **15 s** on both revisions with
  `-Wno-udp-coverage`, and separately with `--max-udp-coverage-notes 0`.
- On master, enabling `-Wudp-coverage` still gives no warning for the complete
  13-input table, and the complete 33-input table exceeds **15 s**. Removing
  the first input's hold row from the 33-input table produces the expected
  `-Wudp-coverage` warning promptly. This checks the diagnostic separately.
- The original data-dependent 33-input `dense_udp_coverage` probe also exceeds
  **15 s** in the standalone CLI at both revisions. Icarus Verilog 13.0 stable
  compiles that original probe in **0.013 s**, exits 0 and prints `1` when run.
  It compiles the complete 33-input hold table in **0.012 s**.

## Localization

The identical function on both tested revisions is
[`checkPrimitiveEdgeCombinations`](https://github.com/MikePopoloski/slang/blob/82ed79add4bdc3bc730c00b8fe0c04b1a7714fdb/source/ast/symbols/MemberSymbols.cpp#L1212),
called unconditionally after table construction by `PrimitiveSymbol::fromSyntax`.
It enumerates concrete rows and queries `BitTrie::contains` for each row.
Its early exit after enough missing examples does not help a completely
covered table, because no missing example is found.

The concrete one-input-edge state space for N inputs has
`6 * N * 3**(N-1)` possibilities. The helper walks concrete states rather than
the compact wildcard rows. The exponential-cost explanation is an inference
from the source, consistent with the measured growth; the formula describes
the state space, not a measured iteration count. Warning suppression and the
note limit do not bypass this loop. A check that scales with compact wildcard
tables while retaining the coverage diagnostic would help. The coverage rule
itself is IEEE 1800-2023 29.6; this report requests preserving it.

Related: [#936](https://github.com/MikePopoloski/slang/issues/936) introduced the
coverage requirement and is closed. On 2026-10-31 repository issue searches for
`UDP`, `primitive`, `coverage` and `exponential` found no matching performance
report; this is not proof that there is no duplicate.

Generative AI assistance: Codex was used to prepare this report and the
original hold-table reproducer. Human review is pending before submission.

## Local evidence

- [Sources, SHA-256 hashes, complete subprocess results and build identities](../measurements/2026-10-31/slang-udp-coverage-standalone.json).
- [Earlier 2026-10-24 observations](../measurements/2026-10-24/udp-edge-coverage-observations.json).
- Local build, compiler logs and measurement driver: `build/slang-udp-20261031/`
  (ignored, not committed). The project's dependency pin is unchanged.
