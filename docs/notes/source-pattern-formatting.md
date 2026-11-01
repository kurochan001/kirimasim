# Source assignment-pattern formatting

[日本語](source-pattern-formatting.ja.md)

This connects source `%p` and `%0p` under the already approved SYSTEM_TASKS
3.9 (IEEE 1800-2023 21.2.1.6), using the existing shared kernel renderer and
typed SIR `pattern.format` instruction. It adds no dependency, compatibility
mode, instruction opcode or C ABI version.

## Supported values and calls

The source bridge covers integral, real/shortreal and string values, enums,
packed records and first-member packed unions, supported fixed unpacked
arrays/records, and typed dynamic arrays/queues, including supported nested
combinations. Declaration order, packed member offsets, signs, state and enum
names come from slang's elaborated types. `%0p` removes record names recursively.
The existing kernel defines quoting, escapes, decimal values and X/Z spelling;
no independent formatter is added in front or VM. Output has no 1,024-character
cutoff. General field widths, flags and precisions for `%p` remain unsupported;
this bridge accepts `%p` and `%0p`.

Literal-derived integral expressions that slang classifies as implicit strings
use the existing SV string conversion for their presentation. Their raw packed
value remains available for ordinary numeric conversions and monitor comparison.
Thus `%p` of `"ab"` or `{"a","b"}` is quoted, while `%0d` of the same literal
and `%p` of an explicitly integral cast retain integer meaning. Embedded NULs
follow the existing SV string-cast rule, not a new representation policy.

`$display`/`$write`, file and radix forms, `$swrite`, `$sformat`/`$sformatf`,
Postponed strobes/monitors and their file forms share this path. Runtime formats
may be strings or packed values, and packed destinations retain the existing
copyback behavior. Automatic-variable restrictions on postponed calls remain.
Program tests keep their program alive until the queued Postponed output runs;
implicit program exit still follows its existing immediate-finish contract.

## Capture and shared representation

Each actual expression is evaluated once in source order. Constant-format
`%p` arguments are rendered immediately to an immutable string and the format
uses `%s` for that operand. Later arguments cannot change an earlier rendering.
Runtime formats retain a raw scalar, when applicable, and both presentation
strings. The format value itself is captured before evaluating the arguments.

`$sformatf.runtime.pattern` has scope, cell, tick scale and format after its
name. Each subsequent quartet is the original-value PatternId, raw scalar
(or an empty aggregate marker), named string and positional string. An empty
argument uses four empty markers. The verifier checks the prefix, complete
quartets, raw types, layout IDs and both string types. The original-value layout
can differ from the layout used to render an implicit-string presentation.
The VM converts this explicit typed transport to kernel `FormatValue` views;
ordinary numeric/string conversions still use the raw value. Untyped native
`%p` output remains rejected rather than guessed from bits.

Pattern monitors capture actual argument values during the same evaluation
that prepares their output. Container comparisons use detached value snapshots,
recursing through nested containers and comparing integral leaves with case
equality. Comparisons use raw arguments, never rendered text; different values
may print alike. Changes to `$time` alone do not request output. The existing
heap read-capture machinery follows cells and replacement bindings and keeps
observer metadata weak. The older non-pattern monitor path is unchanged.

Legal scalar field reads in active object methods also use this bridge.
Object-property dependency observation remains unsupported, including through
ordinary calls. Formatting an unrelated supported value no longer rejects a
whole design merely because it also allocates objects. Whole class/event/chandle
identities, associative storage and unsupported tagged/unpacked-union storage
remain implementation boundaries; this change does not claim full `%p` or P1.

## Verification and expectation migration

The original [generator](../../tools/gen_pattern_format_matrix.py) covers
12 call contexts and 17 APIs: 196 legal source programs, 4,312 conversions.
The [CLI judge](../../tests/cli/run_pattern_format_tests.py) compares lexical
pattern tokens, retaining strings and escaped names but ignoring field spaces.
It does not put implementation-dependent `%p` spacing into suite stdout goldens.
Source/VM unit checks additionally cover wide enum/packed windows, argument
mutation, aggregate returns/refs, nested/empty/shrunk containers, long output,
raw implicit strings, postponed updates, object boundaries and malformed SIR.

Eight registered compiled controls target mode selection, member offsets,
evaluation count, detached monitor snapshots, four-state comparison, retained
raw scalars, presentation operand types and coexistence with unrelated objects.
Executed results and configuration coverage are in the [validation record](source-pattern-validation.md).
The [expectation-only migration](source-pattern-expectations.md) preserves the
original historical sources and the remaining rejection checks. No P1 population,
external pass rate, original finite Icarus inventory or performance budget is
changed by this source implementation. Slang posting remains on hold.
