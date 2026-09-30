module {
  func.func @smoke(%arg0: i32) -> i32 {
    %c2 = llvm.mlir.constant(2 : i32) : i32
    %c3 = llvm.mlir.constant(3 : i32) : i32
    %cm5 = llvm.mlir.constant(-5 : i32) : i32
    %c1 = llvm.mlir.constant(1 : i32) : i32

    // Pos + Pos -> Pos (unambiguous): should annotate "positive"
    %add_pos = llvm.add %c2, %c3 : i32

    // Neg + Pos -> Top (genuinely ambiguous): should be unannotated
    %add_ambig = llvm.add %cm5, %c3 : i32

    // Neg * Neg -> Pos: should annotate "positive"
    %mul_pos = llvm.mul %cm5, %cm5 : i32

    // Pos - Pos(bigger magnitude unknown to us) -> Top via sub's negate+add path,
    // but here both are exact atoms: 2 - 3 -> Neg+Pos ambiguity -> Top, unannotated
    %sub_ambig = llvm.sub %c2, %c3 : i32

    // x * 1 -> should preserve arg0's own state (Top here, since arg0 is unconstrained,
    // so still unannotated -- this just proves it's not accidentally forced to "positive")
    %mul_identity = llvm.mul %arg0, %c1 : i32

    // c2 * 1 -> should preserve "positive" through the identity rule
    %mul_identity_const = llvm.mul %c2, %c1 : i32

    llvm.return %add_pos : i32
  }
}
