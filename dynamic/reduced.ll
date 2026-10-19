target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macosx27.0.0"

define i32 @main() {
  br label %1

1:                                                ; preds = %1, %0
  %.01.i = phi i32 [ 1, %0 ], [ %2, %1 ]
  %.0.i = phi i32 [ 1, %0 ], [ 2, %1 ]
  %2 = mul i32 %.01.i, %.0.i
  br label %1
}
