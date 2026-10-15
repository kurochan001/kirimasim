#!/bin/sh
# Builds the Icarus Verilog development version that the P1 gate compares
# against (TEST_SUITE.md, "比べる相手" and "P1 の合格条件の測り方").
#
#   tools/build_icarus.sh [<install dir>]
#
# The commit is the one pinned by sv-tests. The source is fetched and built
# only; nothing here reads it (clean room, CLAUDE.md). The default install
# directory is $XDG_CACHE_HOME/haremasim/icarus-<commit> (or ~/.cache/...).
# Prints the path of iverilog when done. Building again is skipped if the
# install directory already has it.
#
# Needs git, autoconf, gperf, a C++ compiler, and bison and flex new enough
# for Icarus (on macOS, the Homebrew ones are used when present).

set -eu

COMMIT=04cc493506bd4d75c861cfc9961e2a56517d7cc6
URL=https://github.com/steveicarus/iverilog.git

CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/haremasim
PREFIX=${1:-$CACHE/icarus-$COMMIT}
if [ -x "$PREFIX/bin/iverilog" ]; then
    echo "$PREFIX/bin/iverilog"
    exit 0
fi

for d in /opt/homebrew/opt/bison/bin /opt/homebrew/opt/flex/bin /usr/local/opt/bison/bin \
    /usr/local/opt/flex/bin; do
    if [ -d "$d" ]; then PATH="$d:$PATH"; fi
done
export PATH

SRC=$(mktemp -d "${TMPDIR:-/tmp}/icarus-src.XXXXXX")
trap 'rm -rf "$SRC"' EXIT
git -C "$SRC" init -q
git -C "$SRC" fetch -q --depth 1 "$URL" "$COMMIT"
git -C "$SRC" checkout -q FETCH_HEAD

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
(
    cd "$SRC"
    sh autoconf.sh >/dev/null
    ./configure -q --prefix="$PREFIX"
    make -s -j"$JOBS" >/dev/null
    make -s install >/dev/null
)
echo "$PREFIX/bin/iverilog"
