#!/bin/sh
# C -> LLVM IR -> MLIR -> (instrumented) MLIR -> LLVM IR -> executable.
#   ./build.sh prog.c [pass]     -> prog.instrumented (pass: sign-analysis)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
SRC="$1"; PASS="${2:-sign-analysis}"
BASE="${SRC%.c}"
PLUGIN="$REPO/build/ZeroAnalysis.dylib"
[ -f "$PLUGIN" ] || PLUGIN="$REPO/build/ZeroAnalysis.so"

# -O0 without optnone + mem2reg: keeps values in SSA registers (the analysis
# does not model memory) without letting LLVM fold the arithmetic away.
clang -g -fwrapv -O0 -Xclang -disable-O0-optnone -S -emit-llvm "$SRC" -o - \
  | opt -passes=mem2reg -S -o "$BASE.ll"
mlir-translate --import-llvm --mlir-print-debuginfo "$BASE.ll" -o "$BASE.mlir"
mlir-opt --load-pass-plugin="$PLUGIN" \
         --pass-pipeline="builtin.module($PASS{instrument})" \
         "$BASE.mlir" -o "$BASE.instrumented.mlir"
mlir-translate --mlir-to-llvmir "$BASE.instrumented.mlir" -o "$BASE.instrumented.ll"
clang -O0 -w "$BASE.instrumented.ll" "$REPO/runtime/dataflow_check.c" -o "$BASE.instrumented"
