# ASSERTION_COVERAGE_SPEC: assertions and coverage

[日本語](ASSERTION_COVERAGE_SPEC.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

**Reader context:** Immediate assertions and concurrent assertions are different
execution features; use the [current support
inventory](../P1_SUPPORT.md#system-tasks-functions-and-methods) to identify the
implemented subset.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](../notes/phase-renumbering.md)).

The first procedural sampled-value functions are in P2. Broader concurrent assertion and coverage support is planned for P5. [Current phase assignments](../notes/p3-p7-roadmap-revision.md)

## Assertion operators and when they are evaluated

### Procedural sampled-value functions in P2

IEEE 1800-2023 16.5.1 and 16.9.3 define the sample and the comparison. The first subset accepts packed integral static variables and nets, constant selections, integral constants, and pure built-in integral operators over these operands. It supports declaration initializers that are elaboration-time constants. Other operand forms receive a positioned diagnostic.

- `$sampled(e)` evaluates the expression from the sampled values of its operands. At time zero a static variable contributes its declaration initializer if present, otherwise its type default. A net contributes its type default. At each later simulation time all operands come from the Preponed sample. Further Active, NBA or Reactive iterations at that time read the same sample. This function needs no clock.
- `$stable(e)` and `$changed(e)` compare that sample with the sample at the most recent **strictly earlier simulation time** with a qualifying clock edge. Before such an edge exists they compare with the default sampled expression. Compare all bits including X and Z. The results are complementary one-bit values. Repeated edges at one simulation time contribute one history sample. History advances even when control flow skips every call at an edge.
- The first history clocks are a single posedge or negedge on a statically bound one-bit signal, given explicitly or inferred from the enclosing procedural event control under 16.14.6. History belongs to the elaborated signal and clock bindings in their instance. Compound and guarded clocks, further clock inference, and the other sampled-value functions are deferred.

The draft SIR records each sampled input's instance, source declaration and fixed packed range, typed default sample, and internal current-sample slot. A clocked input additionally records the clock range, edge and internal previous-sample slot. The verifier checks source and destination bounds, types, clock width, and ownership of the internal destinations. Format 1 preserves the records through readback. The scheduler owns sampling and history. Expressions use their ordinary typed value operations on these samples.

## The coverage model

Not written yet (to be written in the phase the priority table gives).

## The JSON format of coverage

Not written yet (to be written in the phase the priority table gives).
