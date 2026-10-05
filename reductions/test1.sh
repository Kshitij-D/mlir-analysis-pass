#!/bin/sh
# Interestingness test for llvm-reduce (target 1).
#
# Interesting: after converting the .ll on the command line to MLIR and
# running sign-analysis, the output contains an `llvm.add` result the
# analysis proves "is nonnegative". SignAnalysis.cpp's generic addRule
# table only lands on NonNeg when *both* operands already admit {Zero,
# Pos} simultaneously (hand-verified: NonNeg + Pos tightens to plain Pos,
# not NonNeg) -- so this can only be satisfied by combining two already
# "nonnegative" facts through addRule, not by reading a literal, not by
# mulRule's x*1 fast path, and not by a bare join with no arithmetic.
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
         --pass-pipeline='builtin.module(sign-analysis)' \
         "$MLIR" 2>&1 1>/dev/null \
  | grep -Eq 'llvm\.add .*// %[A-Za-z0-9_]+ is nonnegative'
