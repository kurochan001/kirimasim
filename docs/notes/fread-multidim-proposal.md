# Multidimensional binary memory input: proposed contract

[日本語](fread-multidim-proposal.ja.md). Proposed 2026-10-24; **not approved
or implemented**. SYSTEM_TASKS 6.3 defines binary input for a memory, but
does not map a multidimensional memory to the scalar start/count arguments.
[P1_SCOPE](../spec/P1_SCOPE.md) requires review of incomplete semantic rules
before implementation. No dependency or installation is proposed.

## Proposed amendment to SYSTEM_TASKS 6.3

- Keep the existing one-dimensional rule: start is an actual declared
  index, defaults to the lower numerical bound, and increases numerically.
- For a fixed integral memory with two or more remaining unpacked dimensions,
  define a linear address from zero through the product of their sizes minus
  one. Each dimension traverses its lower numerical index to its higher one,
  with the rightmost dimension varying fastest, regardless of declaration
  direction. Start addresses a scalar integral leaf in this linear space;
  count counts those leaves, not rows or bytes.
- Default start is zero and default count allows the remaining memory to be
  filled. A start outside the linear address space or nonpositive count reads
  no data, returns zero and leaves memory unchanged. End at the memory bound,
  count exhaustion or the first element not read whole. Retain the existing
  argument typing/conversion and descriptor rules; do not alter one-dimensional
  behavior, including existing invalid-input diagnostics.
- Read each integral leaf with the existing shared binary-input operation:
  most significant byte first, ceil(width/8) bytes, truncation above the leaf
  width, and old bits retained for bytes not supplied at EOF. Return the total
  bytes read. Stop after a partial leaf. Preserve ordinary element-write
  notifications and two/four-state destination conversion.
- Capture an accepted selected-memory prefix before descriptor, start and
  count arguments, then evaluate those arguments once left to right, as in
  VALUE_SEMANTICS 3.1. Use the remaining dimensions of that selected memory;
  an outer actual index is not a flattened start address.
- Use the same contract for slot, automatic-frame and actual-ref storage.
  Extend front traversal and existing typed element operations; do not add
  source-level address semantics to VM or introduce a new SIR/kernel opcode.
  Existing SIR storage limits continue to apply.
- Dynamic-array/queue resizing and multidimensional structure memories remain
  separate incomplete mechanisms. This amendment does not remove them from
  the P1 target or relax strict slang acceptance.

The multidimensional address space above is an **explicit project
interpretation**, not wording quoted from IEEE 1800-2023. Sections 7.4.4 and
21.3.4.4 were checked locally: the former gives dimension significance and
the latter defines byte/element input, but the checked binary-input clause
does not explicitly resolve a multidimensional scalar start address.

## Original observations and required validation

Installed Icarus compiler/runtime both report `14.0 (devel)
(s20260301-411-gca6b8d376)`. Original byte-memory probes with dimensions
`[0:1][0:2]`, `[1:0][2:0]`, `[2:3][5:7]`, `[3:2][7:5]` and
`[-1:0][-2:0]` read six scalar elements in increasing numerical index order.
For each shape, start 1 reads five leaves; start 2/count 2 reads two leaves
at linear offsets 2 and 3, including across a row boundary. An omitted-start
argument before an explicit count is rejected by that compiler, although
21.3.4.4 permits the form; strict slang acceptance is retained. Exact Icarus
13.0 behavior is unconfirmed. Neither simulator's source or tests were read.

After approval, add independent byte-level model matrices for ascending,
descending, nonzero and negative ranges, two/three dimensions, one-dimensional
regression controls, widths across byte and machine-word boundaries, two/four
states, partial EOF, start/count boundaries and side effects. Cover shared
storage paths, references and waits, and compile deliberate traversal/count
mutations to prove that actual output assertions detect them. Add the measured
gap to the comparison inventory without changing existing probes or expectations.
