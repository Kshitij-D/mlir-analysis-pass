#!/bin/sh
# Interestingness test for llvm-reduce (target 3).
#
# Interesting: after converting the .ll on the command line to MLIR and
# running sign-analysis, the output contains a block *argument* (a
# control-flow merge point, i.e. what used to be a phi node in LLVM IR)
# that the analysis proves "is nonpositive". No single transfer rule in
# SignAnalysis.cpp ever produces that fact directly from one incoming
# value -- it can only come from MLIR's dataflow framework joining two
# differently-signed values arriving along different edges (one exactly
# negative, one exactly zero) via SignState::join() in SignDomain.h. This
# is checked on an "// argument:" line specifically (Annotate.cpp's prefix
# for block arguments), not on the result of any further arithmetic, so it
# can only be explained by the cross-edge join itself.
#
# Usage: ./test3.sh input.ll ; echo $?   -> prints 0 when interesting.
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
  | grep -Eq '// argument: %[A-Za-z0-9_]+ is nonpositive'
