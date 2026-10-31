# Class and process inventory, 2026-10-31

[日本語](icarus-class-process-inventory-2026-10-31.ja.md). Measured against native
`7eab335e9a71ae08dbaaf834223669703a9e0a3d` on macOS arm64.

The finite comparison now has 116 original probes. The previous 95 sources,
flags and expected outputs are unchanged; 21 class/process probes are appended.
Every saved row includes its exact source and SHA-256, clause references,
expected output bytes and both engines' outcomes. The complete results and
actual version strings are in [the measurement](../measurements/2026-10-31/icarus13-class-process-inventory.json).
This is an inventory, not a claim of complete coverage or P1 completion.

## What the added probes establish

Icarus 13.0 stable runs 18 of the 21 new probes as proposed. Native runs two
and reports unsupported forms with exit 2 for the other 19. Sixteen added
probes are therefore confirmed gaps against this finite 13.0 comparison:

- Seven class mechanisms: handle assignment/aliasing, shallow copy,
  nonvirtual inheritance, `this` property selection, declaration initialization,
  a task method that waits, and a static property shared through instances.
  They fall under the [approved class contract](class-runtime-proposal.md).
- Seven named-fork disable forms: join, join_any, join_none, an external
  hierarchical disable, an event waiter, retained descendants of an ended
  branch, and a waiting task call (9.3.2, 9.6.2; SCHEDULER_SPEC 10.4/10.7).
- Two always_ff expression forms: a computed XOR edge and a runtime bit
  selection (9.2.2.4, 9.4.2). The SIR representation must retain the existing
  single-event and write restrictions while monitoring expressions.

Both engines pass the controls for `disable fork` reaching a descendant of
an ended child, and `wait fork` waiting only for direct children. Icarus rejects
class-handle comparison, `wait_order`, and a nonblocking event trigger with an
event control. Class assignment and shallow copy are tested separately from
handle comparison, so the unsupported comparison does not mask their support.
These three rejected forms stay in the standards backlog and are not newly
required by this measurement alone.

Across all 116 probes, 21 are Icarus-passing native gaps (including the existing
`wire real` extension, inventoried separately). The original four reference
output mismatches remain four; they are not adopted as native oracles.
The original 95 probes still have the same five gaps. Adding probes has exposed
more of the existing required work; it has not regressed native execution.

## Next boundaries

Class implementation must cover the approved ownership, construction, dispatch,
copy and lifetime mechanisms together; accepting only a constructor is not
completion. Named-fork disable must identify the relevant child activations
and retained descendants even after a join_any/join_none continuation or a
branch return; attaching a sequential scope only to the parent is insufficient.
VPI, timing checks and the remaining container/network/I/O forms still need
inventory. Linux and Icarus 14.0 results for this new cohort are not confirmed;
repeat the same saved sources and runner with those engines to check them.
No existing suite expectation or specification is changed by this inventory.
