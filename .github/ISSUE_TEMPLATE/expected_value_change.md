---
name: Expected value change
about: Propose changing an existing test expectation or PerfCount budget
labels: expected-value
---

<!-- Please do not paste text, figures, or example code from the IEEE 1800
     standard or code written by others, and do not include confidential
     information from your employer. Cite section numbers and use your own
     words and your own minimal examples. -->

## Which expectation

<!-- Test file and field (for example, :expected_stdout:), or the
     tests/perf_counts/expected.json entry. -->

## Current and proposed value

- Current:
- Proposed:

## Reason

<!-- Why the current value is wrong or out of date. For behavior, give the
     IEEE 1800 section number and explain in your own words. For PerfCount,
     explain what changed the counts. -->

## Evidence

<!-- Outputs of other simulators (name and version), or other evidence. -->

Changes to existing expectations are made in their own commit, with no
implementation changes under `src/` (see CONTRIBUTING.md).
