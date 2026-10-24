# UDP evaluation beyond 32 inputs

[日本語](wide-udp-inputs.ja.md). Implemented at `52652eb`, 2026-10-24.

The front and SIR verifier no longer reject every UDP with more than 32
inputs. The shared evaluator and VM transport no longer rely on a fixed-size
input buffer for all UDPs. This follows the existing VALUE_SEMANTICS 5.9 and
SIR_SPEC 4.3/5.5 rules; no semantic amendment, dependency change or diagnostic
suppression is used. IEEE 1800-2023 29.3.2 permits an implementation limit,
while requiring at least ten combinational and nine sequential inputs. It
does not impose a maximum of ten/nine; the old header comment was incorrect.

## Representation and behavior

The existing `udp`/`udp.seq` instructions retain their operand meaning.
Their input count uses the existing positive SIR count representation;
sequential operand-count validation now uses 64-bit arithmetic. Malformed
huge-count SIR cannot make twice the count wrap into a tiny valid argument list.
Compiled edge-column indices use a signed 64-bit field.

The kernel's `udpOutputRead` and `udpStepRead` accept stable input getters.
They normalize Z to X and apply the same compiled rows as the existing pointer
APIs, without allocating an input array. For sequential evaluation, a prefix
view reads current values of already processed terminals and previous values
of later terminals. This preserves the approved port-order rule and avoids
letting an early edge see a future data change. Level rows retain priority;
unlisted changes still produce X.

The VM mechanically supplies values and retains its small local buffer path
for up to 32 inputs. Larger UDPs use the getters. The kernel owns matching and
state-transition semantics. No source-language decision, new scheduler region,
extra process, mutable global state or external ABI change is introduced.
The public header remains C++20. The old pointer APIs remain; C++ callers that
used the removed `kUdpMaxInputs` constant should size transport from
`UdpTable::inputs`. Evaluation itself requires no variable-size scratch allocation.

## Source and independent value evidence

[106 original sources](../../tools/gen_wide_udp_matrix.py) cover 31, 32, 33,
63, 64, 65, 127, 128, 129 and 257 inputs in modules, generate scopes, interface
signal storage, program stimulus and primitive instance arrays. Combinational
rows exercise late columns, zero dominance, X/Z and `b`; sequential rows
exercise initialization, listed/unlisted changes, late data/clock columns,
state hold and recovery. Four delayed instances check initial output and later
delivery; another source checks weak UDP output against a strong driver.

All 106 match their existing-contract model on the new native binary. On the
retained native source `c6d9c3d` (banner `392687e`), 20 pass and 86 are rejected
as unsupported. Both measured references match 25 sources, reject 21 at
compilation, differ on 40 outputs and fail during 20 executions. Do not replace
the approved UDP rules with those failures or mismatches. The
[portable source records](../measurements/2026-10-24/wide-udp-source-validation.json)
retain each source/expected hash and every engine result.

Five new unit tests add independent SIR/VM transport and verifier checks,
wide matching against valsem, decisive boundary columns, level-row priority,
X/Z normalization and prefix-order checks. Direct kernel/reference evaluation
also covers 65,537 inputs; that is not a claim of source compilation at that
size. The malformed-SIR check specifically tests sequential count overflow.

[Four compiled controls](../measurements/2026-10-24/wide-udp-negative-controls.json)
restore the front limit, wrap VM inputs, expose future inputs to an earlier
edge, or omit wide Z normalization. The four earlier UDP unit tests pass for
each corruption. New real source stdout or new wide value assertions detect
each one. No judging self-check is counted. The controls are registered in
`tools/mutants.py`, and the old level-row control follows the refactored helper.

Full local CI passes with native source `52652eb` and the registry/count
follow-up (2026-10-24). Dev/ASan/UBSan each run 3,317 tests: 3,315 pass and
two known skips remain. Release runs 3,318: 3,316 pass and the same skips.
All 136 Tools tests, 106 new source cases and five new unit tests pass.
Release flags, banned features, SPDX, suite files, global state, expectation
protection, README counts, clang-format and clang-tidy pass. Existing suite
bodies/expectations, performance counts and all 29 SIR dumps are unchanged.

## Source compilation boundary

Dense hold-row variants remain a frontend scaling problem. The pinned slang
coverage checker enumerates concrete edge/input combinations, stopping after
enough missing examples or completion. A widely covered early terminal can
make this expensive before a later uncovered combination is reached. The
[original bounded observations](../measurements/2026-10-24/udp-edge-coverage-observations.json)
record completed 3/5/9-input parses and 2-second limits at 13/17/33 inputs for
one original family. Timeouts are measured bounds, not proof of nontermination.
Its 33-input reference runtime values also differ; compile-time observations
must not be mistaken for an approved simulation oracle.

The original 54 dense source variants remain reproducible outside the routine
suite, with their source hashes retained:

```sh
python3 tools/gen_wide_udp_matrix.py \
  --dense-sequential-repro-dir build/udp-coverage-repros
```

They are not passing suite evidence. Runnable cases deliberately test both
listed and unlisted edges; no warning is disabled or dependency patched to
make them pass. A separate dense 33-input probe passes both references and
exceeds the native comparison's 15-second subprocess bound. See the
[upstream issue draft and reproducer](slang-udp-coverage-issue-draft.md).

The source of the pinned checker is
[slang MemberSymbols.cpp](https://github.com/MikePopoloski/slang/blob/3b351a52b15755b896d3b0144ae0432547d86618/source/ast/symbols/MemberSymbols.cpp#L1212).
The helper was also inspected in upstream main; a newer upstream build was
not installed or tested. Complete wide UDP source coverage remains unconfirmed.

## Comparison scope

The expanded 87-item inventory adds passing native/reference combinational and
sequential UDP probes plus the dense compilation boundary. Earlier 84 statuses
and passing judged outputs are unchanged. On the expanded list the retained
baseline has 27/33 gaps and the current native has 25/31 against 13.0/development.
The older 84-item counts remain 24/30. Thus this closes two newly inventoried
mechanisms and exposes a separate compiler limit; it does not close an older
counted gap or establish complete P1 parity. Reference mismatches remain 4/7.
