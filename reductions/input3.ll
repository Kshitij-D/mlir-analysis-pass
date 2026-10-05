; Target 3: the lattice JOIN itself, firing across control flow -- not a
; transfer rule in ZeroAnalysis.cpp/SignAnalysis.cpp at all, but MLIR's
; SparseForwardDataFlowAnalysis framework automatically merging the two
; edges into a phi/block-argument by calling SignState::join() directly.
;
; %y is -3 on one incoming edge and 0 on the other: neither value alone is
; "nonpositive" (one is exactly negative, the other exactly zero), but the
; merge point has to be sound for *either* predecessor, so it's annotated
; the smallest fact that covers both -- `NonPos` -- straight off
; SignDomain.h's Hasse diagram, with no transfer rule involved. This is
; checked against the block argument itself (an Annotate.cpp "// argument:"
; line), not a value that's had any further arithmetic applied, so it can
; only be explained by the cross-edge join and not by, say, `addRule`
; combining two already-combined facts.
define i32 @pick_sign(i1 %cond, i32 %noise) {
entry:
  %junk = add i32 %noise, 1
  br i1 %cond, label %then, label %else
then:
  br label %merge
else:
  br label %merge
merge:
  %y = phi i32 [ -3, %then ], [ 0, %else ]
  %combined = add i32 %y, %junk
  ret i32 %combined
}
