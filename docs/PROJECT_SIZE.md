# Project size

[日本語](PROJECT_SIZE.ja.md) · This English text is the main version.

This snapshot records the source, tests, tools, documents and history at
`v0.3.0` (commit `f0abaff21ee9`, 2026-11-27). [November 14 snapshot](https://github.com/kurochan001/kirimasim/blob/v0.3.0/docs/PROJECT_SIZE.md)

## C++ sources by library

The library names follow the [dependency table](../CLAUDE.md#rules-of-library-dependencies).
Headers shared at the root of `src/` have their own row. A line is a newline, as
`wc -l` counts. The last column excludes lines with only white space.

| Library | Files | Lines | Not blank |
| --- | ---: | ---: | ---: |
| kernel | 134 | 27,562 | 26,177 |
| front | 42 | 18,683 | 17,976 |
| vm | 19 | 11,737 | 11,387 |
| sir | 42 | 10,337 | 9,810 |
| valsem | 18 | 1,954 | 1,729 |
| driver | 12 | 1,963 | 1,899 |
| analysis | 13 | 1,393 | 1,313 |
| codegen | 6 | 1,608 | 1,548 |
| Shared (`src/`) | 1 | 50 | 48 |
| **Total** | **287** | **75,287** | **71,887** |

## Tests, tools and documents

| Kind | Files | Lines |
| --- | ---: | ---: |
| Unit tests (C++, `tests/unit/`) | 111 | 52,172 |
| Suite tests (`tests/suite/**/*.sv`) | 12,175 | 845,198 |
| Expected outputs of the suite tests | 11,395 | 129,916 |
| Test scripts (Python, `tests/`) | 87 | 14,294 |
| Tools (Python, `tools/`) | 272 | 57,594 |
| Documents in English (`docs/`) | 239 | 40,551 |
| Documents in Japanese (`docs/`) | 240 | 34,864 |
| Specifications, both languages (`docs/spec/`) | 34 | 10,885 |

The release has 235 test generators and 26,141 tracked files. Its history has 1,751
commits, beginning on 2026-10-14.

## Status at this commit

P3 is complete. All 14,040 dev tests and 14,041 Release tests passed.
[Release validation](VALIDATION.md) · [P3 exit measurements](BENCHMARKS.md#p3-performance-targets)

## What is not counted

The count excludes untracked files, dependencies downloaded during the build,
and build outputs. [Third-party notices](../THIRD_PARTY_NOTICES.md)

## How to count again

Run the current counter on a clean checkout. It reads the tracked files in the
working tree. The snapshot above uses this counter on the files at `v0.3.0`.

```sh
python3 tools/project_size.py
```
