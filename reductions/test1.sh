#!/bin/sh
# Interestingness test for llvm-reduce (target 1).
#
# Interesting: after converting the .ll on the command line to MLIR and
# running zero-analysis, the output contains an `llvm.and` result that the
# analysis proves "is zero" -- i.e. the AND-with-zero propagation rule fired,
# not just the constant-introduction rule. (zero-analysis annotates a plain
# `llvm.mlir.constant(0)` as zero too, but that alone is the trivial "the
# constant 4 is even" case; requiring the fact to land on an `llvm.and`
# result specifically forces llvm-reduce to keep actual propagation, not
# just a lone zero constant.) The `and` must also still take a function
# argument (`%argN`) as an operand, so llvm-reduce can't satisfy this by
# collapsing everything down to `and i32 0, 0` -- the point is that the
# analysis proves this without ever knowing the argument's value.
#
# Usage: ./test1.sh input.ll ; echo $?   -> prints 0 when interesting.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"

PLUGIN=""
for candidate in "$REPO/build/ZeroAnalysis.so" "$REPO/build/ZeroAnalysis.dylib"; do
  if [ -f "$candidate" ]; then
    PLUGIN="$candidate"
    break
  fi
done
[ -n "$PLUGIN" ] || exit 1

IN="$1"
MLIR="$(mktemp)"
trap 'rm -f "$MLIR"' EXIT

mlir-translate --import-llvm "$IN" >"$MLIR" 2>/dev/null || exit 1

mlir-opt --load-pass-plugin="$PLUGIN" \
         --pass-pipeline='builtin.module(zero-analysis)' \
         "$MLIR" 2>&1 1>/dev/null \
  | grep -Eq 'llvm\.and .*%arg[0-9]+.*// %[A-Za-z0-9_]+ is zero'
