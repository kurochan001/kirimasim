# Source warning options

[日本語](source-warning-options.ja.md). Implemented under CLI.md 2.1.

`kirimasim -Wwidth-trunc design.sv` and `kirimasim parse -Wwidth-trunc design.sv`
now pass warning options to slang. Previously these direct arguments were
reported as unknown options, although the same options already reached slang
inside an argument file. The default diagnostic configuration is unchanged.

`parseArgs` forwards each complete `-W` argument in source order. Slang owns
the names, groups, enable/suppression/error behavior and unknown-name diagnostics.
A bare `-W` is an input error. There is no second warning catalog in the driver.
Runtime diagnostics keep their existing explicit controls; this does not add
controls for the planned kirima-prefixed runtime diagnostic names.

The generator `tools/gen_slang_warning_matrix.py` makes 12 original cases.
`cli/slang_warnings` checks them in parse/simulation, directly/in an argument
file, and before/after the source, plus empty-name and help checks: 99 checks.
It checks exit status, stdout, severity, diagnostic group and positioned width
conversion. Two compiled mutations discard options or reverse their order.
No existing expected outputs or source fixtures are changed.

## Compatibility boundary found during this work

`--compat` remains outside the direct driver interface. The existing argument
file path exposes an inconsistency at native `7eab335e`, pinned slang `e2d102166`:
`--compat=all` lets an initial block and an always_ff both write a variable.
Parsing succeeds, but execution reports an internal SIR verifier fault (exit 3)
because SIR still enforces the strict write restriction. Default and vcs modes
reject the source with exit 2. The [original source and six observations](../measurements/2026-10-31/slang-compat-sir-boundary.json)
are observations of the defect, not expected results for a regression test.

Compatibility implementation must reconcile those explicit allowances with
SIR_SPEC 7.1/10 and the approved CLI policy before it is called complete.
This change does not amend that contract or enable a new compatibility mode.
