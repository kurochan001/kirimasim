# Config reset and the pinned elaborator

[日本語](config-reset-dependency-gap.ja.md)

Measured on 2026-11-03. This is a dependency diagnostic, not a production fix.

## Confirmed boundary

Four external Chapter 33×27 cross cases disagree after `use #()` resets a
target instance that also has defparam assignments. IEEE 1800-2023 §33.4.3
gives the reset precedence. The unchanged expected W/N/MODE/TAG determine
the generated shape; a sibling remains a separate control.

The [standalone probe](../measurements/2026-11-03/config-parameter-probe.cpp)
uses only the pinned slang library and its public AST APIs. It does not link
kirimasim front, SIR, kernel or VM. Across the 41 original inputs, its parameter
queries have [37 matches and the same four mismatches](../measurements/2026-11-03/config-parameter-elaboration.json).
The input hashes and requested symbols are recorded. This localizes those four
wrong values to dependency elaboration before kirimasim lowering.

At pin `e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`, the empty configuration
assignment path clears the assignment map. It leaves no configuration-origin
entry to suppress a later defparam override. Named empty resets use an entry
with no replacement expression, preserving that origin. This source inspection
is confined to the MIT-licensed dependency permitted by the project policy.

A fresh [standalone run on the subsequently adopted unmodified d02b7cda pin](../measurements/2026-11-03/config-parameter-elaboration-d02b7cda.json) also records 37 matches and the same four differences. The approved label-cancellation implementation does not alter this dependency gap. The experimental patch above remains separate from production.

## Isolated diagnostic patch

The [experimental patch](../measurements/2026-11-03/slang-config-reset-diagnostic.patch)
marks every overridable parameter as reset by the configuration, using the
existing named-reset representation. It is applied to a separate object and
a copy of the static library, leaving the production dependency and simulator
unchanged. The same parameter probe then has
[41 matches and zero mismatches](../measurements/2026-11-03/config-parameter-diagnostic-patch.json).
That checks parameter elaboration only: it is not a full simulation, an upstream
acceptance, a dependency-pin update or a passing production regression.

Production integration remains open. Keep slang unmodified at the approved pin,
retain the four failures, and prepare an upstream-compatible fix or an approved
dependency policy change. A future pin requires current-binary execution of all
41 config cases and full local CI; never replace the expected values with the
observed defparam values. No issue or pull request has been sent upstream.

## Reproduce

The probe takes a source, explicit config top and hierarchical parameter names:

```sh
config-parameter-probe input.sv cfg:config \
  cfg_top.target.W cfg_top.target.N cfg_top.target.MODE cfg_top.target.TAG
```

Build it with the include paths and defines in the dev compilation database,
linking the existing `libsvlang.a` and the dependency libraries in the driver's
Ninja link command. For the isolated experiment, compile only the patched
`ParameterBuilder.cpp` using its own compile-database command, replace that
object in a copy of the archive and relink this probe. Do not replace a live
build's dependency archive. Records retain source, input, library and patch hashes.
