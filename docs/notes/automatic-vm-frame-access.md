# VM value access for shared automatic storage

[日本語](automatic-vm-frame-access.ja.md). Status: access-path migration,
2026-10-25; automatic environment construction is still in progress.

## Contract and change

This implements the access transport required by sections 2–4/6–8 of the
approved [automatic capture contract](automatic-capture-proposal.md), mapped
to SIR_SPEC 7/8 and RUNTIME_ABI 4/6 in the
[approval record](p1-execution-contracts-2026-10-25.md). IEEE 1800-2023
6.21, 9.3.2 and 13.5.2 are the language references. No external implementation,
external test or new dependency is used. This does not select a physical
frame layout, formatVersion 1 or a new C ABI.

Previously `frameVar`, `frameElem` and `refWord` returned mutable native word
references. Scalar operations, wide operations, memory files, file input,
scanning, seed updates and call marshalling wrote those words directly.
Replacing just scalar assignments with shared bindings would leave the other
paths writing unrelated or unobserved words.

`detail::FrameValue` is a short-lived transport view of one value/element.
It can borrow ordinary frame words or use an immutable kernel activation
binding. It owns neither storage nor a lease and keeps no mutable value
cache. The bound variant reads through `ActivationStore::read` and writes
through `Scheduler::writeActivation`, with complete wide values and explicit
selected bit ranges. Kernel code still defines normalization, identity,
permission checks and observation. The ordinary path keeps word copies and
uses the existing kernel wide-selection primitives, with no allocation in
the view itself. Reference payloads retain both halves of their W4 encoding.

All existing instruction/file/scan/argument paths now read values or commit
values through the shared access interface. A clipped packed write commits
only the covered bits. `$fread` builds only the read-byte range and commits it
once; it does not read the rest of the destination to manufacture a write.
Whole arrays are transported element by element, preserving declaration order.
`bindArgs`, `readOuts` and `writeOuts` take the registered logical activation.
Both waiting and synchronous calls copy outputs back before retiring the
callee, exactly once. The task and deferred-call entry paths register before
copy-in. Registration alone does not yet construct an automatic root.

## Verification and limits

Seven original tests exercise 270 ordinary/bound selection cases against
an independent bit-by-bit model, full reference payloads, invalid buffers and
ranges, readonly access, selected read/write footprints, complete wide-value
notification in Active and Reactive groups, two-state normalization, and
reused-scope generation rejection. A hand-built SIR matrix runs 24 cases of
ordinary/ref array access, clipped packed writes and scalar copies, with
unrelated wide frame rows to check logical/physical row translation.
Five new compiled-control definitions target notifications, wide chunks,
element identity, readonly rights and selected reads. The two existing controls
for wait-ref lookup and `$fread` byte ordering are relocated to the same faults
in the new access paths; no expected output or existing suite body is changed.
Final validation and actual compiled-control outcomes are recorded separately
after they run.

The VM's SIR-to-place resolver currently selects ordinary word views only.
The [new SIR homes](automatic-sir-metadata.md) remain explicitly rejected;
front emits none. The bound access variant is exercised directly by the new
tests, not by newly accepted source programs. This checkpoint closes no
Icarus inventory gap and does not establish P1 completion.

Next, resolve scoped/captured homes to their immutable bindings, construct
root/block/child environments transactionally, preserve full lexical extents,
and release executing owners independently of control frames. References need
their own immutable scoped origin and rights, rather than a pointer to a
parent's current binding table. Guard/watch dependencies must resolve against
the waiter's actual logical activation. Declaration initialization, parameter
binding initialization, copyback, return/disable/program cleanup and typed
roots must all work before removing the execution gate or enabling front
capture. Managed events and other field kinds still need their real providers.
