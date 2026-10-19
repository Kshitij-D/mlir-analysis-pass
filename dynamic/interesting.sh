#!/bin/sh
# llvm-reduce interestingness test: the candidate .ll, instrumented by
# sign-analysis{instrument}, compiled and run with argument 17, must trip a
# dataflow assertion (i.e. the analysis must still prove something false).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
PLUGIN="$REPO/build/ZeroAnalysis.dylib"
[ -f "$PLUGIN" ] || PLUGIN="$REPO/build/ZeroAnalysis.so"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT

mlir-translate --import-llvm "$1" -o "$T/a.mlir" 2>/dev/null || exit 1
mlir-opt --load-pass-plugin="$PLUGIN" \
  --pass-pipeline='builtin.module(sign-analysis{instrument})' \
  "$T/a.mlir" -o "$T/b.mlir" 2>/dev/null || exit 1
mlir-translate --mlir-to-llvmir "$T/b.mlir" -o "$T/b.ll" 2>/dev/null || exit 1
clang -O0 -w "$T/b.ll" "$REPO/runtime/dataflow_check.c" -o "$T/a.out" 2>/dev/null || exit 1
# perl alarm: a reduced loop may no longer terminate.
perl -e 'alarm 5; exec @ARGV' "$T/a.out" 17 2>"$T/err" >/dev/null
grep -q 'dataflow assertion failed' "$T/err"
