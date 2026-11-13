# Project size

[日本語](PROJECT_SIZE.ja.md) · This English text is the main version.

A dated record of how large kirimasim is: the lines of its C++ sources by
library, of its tests, tools and documents, and its history. It was measured
on 2026-11-14 at commit `afc7733c8167`, the commit before this document, with
[`tools/project_size.py`](../tools/project_size.py). The numbers describe that
commit only; [how to count again](#how-to-count-again) gives the command for a
later one.

## C++ sources by library

The libraries are those of the dependency table in
[CLAUDE.md](../CLAUDE.md#rules-of-library-dependencies). A line is a newline
character, as `wc -l` counts; "not blank" leaves out lines with only white space.

| Library | Files | Lines | Not blank |
| --- | ---: | ---: | ---: |
| kernel (execution kernel) | 72 | 19,304 | 18,105 |
| front (calling slang, lowering to SIR) | 39 | 17,563 | 16,899 |
| vm (tier 1) | 16 | 10,551 | 10,231 |
| sir (intermediate representation) | 29 | 8,698 | 8,307 |
| valsem (reference value semantics) | 18 | 1,954 | 1,729 |
| driver (the `kirimasim` executable) | 3 | 744 | 708 |
| analysis (empty so far) | 2 | 27 | 17 |
| codegen (empty so far) | 2 | 27 | 17 |
| **Total** | **181** | **58,868** | **56,013** |

## Tests, tools and documents

| Kind | Files | Lines |
| --- | ---: | ---: |
| Unit tests (C++, `tests/unit/`) | 110 | 49,900 |
| Suite tests (`tests/suite/**/*.sv`) | 12,007 | 834,200 |
| Expected outputs of the suite tests | 11,228 | 128,697 |
| Test scripts (Python, `tests/`) | 77 | 11,658 |
| Tools (Python, `tools/`) | 261 | 55,093 |
| Documents in English (`docs/`) | 231 | 38,484 |
| Documents in Japanese (`docs/`) | 231 | 32,820 |
| Specifications, both languages (`docs/spec/`) | 34 | 9,824 |

- 228 of the tools are test generators (`tools/gen_*.py`), which make most of
  the suite tests as matrices of a mechanism ([TEST_SUITE.md](spec/TEST_SUITE.md)).
- `tools/mutants.py` holds 1,355 known mistakes, each of which some test must
  catch ([TEST_SUITE.md](spec/TEST_SUITE.md), negative controls).
- git tracks 25,603 files in all.

## Status at this commit

- Phase P1, the first execution tier; the [README](../README.md#current-status)
  gives the current status.
- The front end is unmodified slang pinned at `8ffdcdd8`
  ([update record](notes/slang-8ffdcdd8-update.md)).
- The dev build runs 13,836 CTest tests, all passing. The counts of tests and
  the comparisons with other simulators, with their dates, are in
  [Recorded validation counts](VALIDATION_COUNTS.md): sv-tests 253 of 344
  (Icarus Verilog 211 on Linux, 210 on macOS) and the P1 population 1,456 of
  1,466.
- History: 1,439 commits, the first on 2026-10-14.
- Issues on GitHub on 2026-11-14, before this document: 45 open, 24 of them
  marked "(slang)" as caused by slang, and 64 closed.

## What is not counted

Files that git does not track; the sources of the dependencies that the build
fetches (slang, GoogleTest and the others, [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md));
and build outputs.

## How to count again

On a clean checkout of the commit to record:

```sh
python3 tools/project_size.py        # the tables above, for HEAD
python3 tools/mutants.py --check     # the number of mutants
```

The issue counts come from GitHub (`gh issue list --state open` and
`--state closed`) and change with every issue.
