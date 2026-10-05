; Target 2: sign-analysis's `x * 1` identity-preservation rule in `mulRule`
; (SignAnalysis.cpp), the entire reason SignDomain.h tracks `One` as a
; refinement of `Pos` rather than folding it into plain "positive".
;
; This is airtight, not a coincidence: SignAnalysis's generic sign-product
; table (the 7-condition fallback in mulRule) can only ever *construct* an
; output from Kind::Neg / Kind::Zero / Kind::Pos -- it never calls
; `add(Kind::One)`, so it can *never* produce "is one" as an answer, no
; matter what the inputs are. The *only* code path that can make the result
; "is one" is the early-return special case, `if (lhs.isExactlyOne()) return
; rhs;` / `if (rhs.isExactlyOne()) return lhs;`, handing back an operand that
; was already known to be exactly 1. So finding "is one" annotated on an
; `llvm.mul` result is a direct, unambiguous witness that this specific
; special case fired -- not just that both operands happened to be positive.
define i32 @one_times_one(i32 %noise) {
entry:
  %r = mul i32 1, 1
  %extra = add i32 %noise, %noise
  %sum = add i32 %r, %extra
  ret i32 %sum
}
