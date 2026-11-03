# SVG diagrams

[日本語](README.ja.md)

These original diagrams explain existing kirimasim behavior and boundaries.
The SVG files are the editable sources. They use text, paths and shapes, with
their own styles and a white background, so they display without a Mermaid
renderer, external fonts or scripts.

- [Debugging stages](debugging-stages.svg): the checks and diagnostic branches
  in the [Debugging Guide](../DEBUGGING.md).
- [VPI lifecycle](vpi-lifecycle.svg): compile-time registration and checks,
  followed by the mode-dependent runtime boundary in the
  [VPI execution note](../notes/vpi-execution.md).
- [Assignment capture](assignment-capture.svg): frame-owned snapshots versus
  independent pending operations, as described in
  [Architecture](../ARCHITECTURE.md#assignment-capture-and-suspension).

Maintain the `.svg` and `.ja.svg` versions together. Keep the same connections,
ownership and scope in both languages. Edit labels and coordinates directly;
keep the root `viewBox`, arrow-line/head coordinates, accessible `title`/`desc`
and the embedding document's alternative text consistent. Check the displayed
text and edges after editing, including the Japanese font fallback.

The diagrams summarize the linked prose and approved contracts. Their arrows
show processing order or ownership; the assignment diagram has no time scale
or scheduler-region ordering. Keep the source/type limits and exceptional
cases in the accompanying text.
