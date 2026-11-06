# AGENTS.md: rules for coding agents working on kirimasim

[日本語](AGENTS.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

This file only points to the rules and says how they apply to a tool other than Claude Code. It does not restate them, so that it cannot drift away from them.

## Where the rules are

Read these two before changing anything. They are the sources of truth: where this file differs from them, they hold.

- [CLAUDE.md](CLAUDE.md): the working rules. Its title names Claude Code, but they bind every coding agent: library dependencies, banned C++ features, build and test commands, where meaning is decided, expected values, and what must not go in the public repository.
- [docs/spec/AI_POLICY.md](docs/spec/AI_POLICY.md): what humans decide, how commits are marked, and the clean room.

For the present state of the project, read [README.md](README.md) and [docs/P1_SUPPORT.md](docs/P1_SUPPORT.md), not a dated note.

## Marking commits

- Add the trailer `Assisted-by: <tool>:<model and version>`, with the name of the tool that did the work: for example `Assisted-by: Codex:GPT-6`. Where CLAUDE.md writes `Claude Code` in this trailer, write your own tool's name.
- Do not add `Co-Authored-By` or `Signed-off-by`.
- Add `Approved-by` or `Reviewed-by` only to a commit that a person has explicitly approved or read. Never infer them.

## What to ask a person first

Show the proposal and its diff, and wait, before changing any of these: the specifications in `docs/spec/`, existing expected values, published text, CI policy, dependencies, or the tools to install.

Push only when a person says so, after running `python3 tools/local_ci.py`, and report its summary.

Do not post to another project's issues or pull requests without a person's go.
