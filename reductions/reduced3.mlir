module attributes {dlti.dl_spec = #dlti.dl_spec<!llvm.ptr = dense<64> : vector<4xi64>, i1 = dense<8> : vector<2xi64>, i8 = dense<8> : vector<2xi64>, i16 = dense<16> : vector<2xi64>, i32 = dense<32> : vector<2xi64>, i64 = dense<[32, 64]> : vector<2xi64>, f16 = dense<16> : vector<2xi64>, f64 = dense<64> : vector<2xi64>, f128 = dense<128> : vector<2xi64>, "dlti.endianness" = "little">, llvm.module_asm = [], llvm.target_triple = ""} {
  llvm.func @pick_sign() -> i32 {
    %0 = llvm.mlir.constant(-3 : i32) : i32 // %0 is negative
    %1 = llvm.mlir.constant(false) : i1 // %1 is zero
    %2 = llvm.mlir.constant(0 : i32) : i32 // %2 is zero
    llvm.cond_br %1, ^bb2(%0 : i32), ^bb1
  ^bb1:  // pred: ^bb0
    llvm.br ^bb2(%2 : i32)
  ^bb2(%3: i32):  // 2 preds: ^bb0, ^bb1
    // argument: %3 is nonpositive
    llvm.return %3 : i32
  }
}
