; Target 1: sign-analysis's generic `addRule` table correctly combining two
; operands that are *themselves* already combined lattice facts (not atomic
; constants), producing a third combined fact -- distinct from target 2
; (a single-operand fast-path bypass in `mulRule`) and target 3 (a bare
; `join()` with no arithmetic at all).
;
; %a and %b are each independently merged to `NonNeg` ("could be 0 or
; positive") by their own if/else. Hand-verified against addRule's 7-case
; table in SignAnalysis.cpp: `NonNeg + NonNeg` lands on `NonNeg` exactly,
; but `NonNeg + Pos` (one side a plain, non-combined positive) tightens all
; the way to `Pos` instead -- so a single real branch merge is not enough to
; make the final `add`'s result "nonnegative"; *both* operands have to carry
; the combined fact into the add. That can't be satisfied by constant
; introduction alone, by `mulRule`'s identity fast path (wrong op), or by a
; join with no following arithmetic.
define i32 @sum_of_two_merges(i1 %c1, i1 %c2) {
entry:
  br i1 %c1, label %t1, label %f1
t1:
  br label %m1
f1:
  br label %m1
m1:
  %a = phi i32 [ 0, %t1 ], [ 5, %f1 ]
  br i1 %c2, label %t2, label %f2
t2:
  br label %m2
f2:
  br label %m2
m2:
  %b = phi i32 [ 0, %t2 ], [ 7, %f2 ]
  %sum = add i32 %a, %b
  ret i32 %sum
}
