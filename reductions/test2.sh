#!/bin/sh
# Interestingness test for llvm-reduce (target 2).
#
# Interesting: after converting the .ll on the command line to MLIR and
# running sign-analysis, the output contains an `llvm.mul` result the
# analysis proves "is one". SignAnalysis.cpp's generic sign-product table
# can only ever construct Neg/Zero/Pos as an answer (never "one"); the only
# way the result can be "one" is the `isExactlyOne()` fast path in
# `mulRule` handing back an operand that was already exactly 1 untouched.
# So this grep pattern can only match if that specific rule fired -- it is
# not satisfiable by plain constant introduction or the generic table alone.
#
# Usage: ./test2.sh input.ll ; echo $?   -> prints 0 when interesting.
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
  | grep -Eq 'llvm\.mul .*// %[A-Za-z0-9_]+ is one'
