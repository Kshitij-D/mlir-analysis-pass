; Target 1: zero-analysis's AND-with-zero rule.
;
; %masked is provably zero no matter what %flags is at runtime: the analysis
; has to combine "nothing is known about %flags" (top, it's a function
; argument) with "this operand is exactly 0" through the `and` rule, rather
; than simply reading a literal off %masked itself. This mirrors the real
; finding in SQLite's whereLoopOutputAdjust (see
; ../examples/sqlite-term-hightruth.md): a runtime value ANDed with a
; statically-zero mask. mlir-translate materializes the immediate `0` below
; as its own `llvm.mlir.constant`, so after import this has the same shape
; as the SQLite case -- an `and` whose own text has no visible "this is
; zero" marker, unlike `%masked = and i32 %flags, %flags` which is not
; interesting under this rule at all.
define i32 @check_flag(i32 %flags, i32 %other) {
entry:
  %masked = and i32 %flags, 0
  %noise = add i32 %other, %other
  %cmp = icmp eq i32 %masked, 0
  %result = zext i1 %cmp to i32
  %sum = add i32 %result, %noise
  ret i32 %sum
}
