// llvm-reduce'd from factorial.c; trips sign-analysis{instrument}.
llvm.func @main() -> i32 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  %1 = llvm.mlir.constant(2 : i32) : i32
  llvm.br ^bb1(%0, %0 : i32, i32)
^bb1(%2: i32, %3: i32):
  %4 = llvm.mul %2, %3 : i32
  llvm.br ^bb1(%4, %1 : i32, i32)
}
