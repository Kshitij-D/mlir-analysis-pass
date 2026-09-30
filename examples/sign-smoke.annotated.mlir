module {
  func.func @smoke(%arg0: i32) -> i32 {
    %0 = llvm.mlir.constant(2 : i32) : i32 // %0 is positive
    %1 = llvm.mlir.constant(3 : i32) : i32 // %1 is positive
    %2 = llvm.mlir.constant(-5 : i32) : i32 // %2 is negative
    %3 = llvm.mlir.constant(1 : i32) : i32 // %3 is one
    %4 = llvm.add %0, %1 : i32 // %4 is positive
    %5 = llvm.add %2, %1 : i32
    %6 = llvm.mul %2, %2 : i32 // %6 is positive
    %7 = llvm.sub %0, %1 : i32
    %8 = llvm.mul %arg0, %3 : i32
    %9 = llvm.mul %0, %3 : i32 // %9 is positive
    llvm.return %4 : i32
  }
}
