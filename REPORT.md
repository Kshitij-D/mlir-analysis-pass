# Dynamic Checking of a Sign Analysis

**Result:** 
`sign-analysis` ignores integer overflow, so it "proves" that values which wrap negative are positive.

## 1. Making the pass transform code

Both passes take a new option, `instrument`. Without it, they print annotated IR as before.
With it, they rewrite the IR so that every proved fact is checked at runtime:

```sh
mlir-opt --load-pass-plugin=build/ZeroAnalysis.dylib \
  --pass-pipeline='builtin.module(sign-analysis{instrument})' in.mlir -o out.mlir
```

The code is in `Instrument.{h,cpp}` (the transformation), the `instrument` branch of `Plugin.cpp`, and `runtime/dataflow_check.c`.

## 2. Turning facts into assertions

Each lattice element is a predicate comparing the value against a constant, using a signed comparison:

| fact | assertion | | fact | assertion |
|---|---|---|---|---|
| negative | `x <s 0` | | nonpositive | `x <=s 0` |
| zero | `x == 0` | | nonzero | `x != 0` |
| one | `x == 1` | | nonnegative | `x >=s 0` |
| positive | `x >s 0` | | top / bottom | (no check) |

The check goes right after the defining operation. For a block argument (a phi), it goes at the top of the block.
Constants are skipped because their checks are trivially true. For example, in `factorial.c` the fact "`%4` (`r * i`) is positive" becomes the code below. SSA names in messages are taken from the IR before instrumentation:

```mlir
%12 = llvm.mul %2, %3 : i32
%13 = llvm.mlir.constant(0 : i32) : i32
%14 = llvm.icmp "sgt" %12, %13 : i32
%15 = llvm.sext %12 : i32 to i64
%16 = llvm.mlir.addressof @__dataflow_msg_0 : !llvm.ptr   // "factorial.c:9:11: in @factorial, %4 was proved positive"
llvm.call @__dataflow_check(%14, %15, %16) : (i1, i64, !llvm.ptr) -> ()
```

The message string records where the fact came from: the first file/line/column in the op's location
(the C source line when the input was compiled with `-g`), the function, and the SSA name.
`__dataflow_check(ok, observed, msg)` prints the message and the observed value when `ok` is false, then aborts.

## 3. Pipeline (`dynamic/build.sh`)

```
clang -g -fwrapv -O0 -Xclang -disable-O0-optnone -emit-llvm | opt -passes=mem2reg
  → mlir-translate --import-llvm --mlir-print-debuginfo
  → mlir-opt sign-analysis{instrument}
  → mlir-translate --mlir-to-llvmir → clang + runtime → executable
```

- **`mem2reg`:** the analysis does not model memory, so without it every value comes from a `load` and is top.
- **`-fwrapv`:** signed overflow is defined behavior here, so a failed check is the analysis's fault, not undefined behavior in the C program.

## 4. Running it

I wrote two small loop programs, `dynamic/factorial.c` and `dynamic/sum.c`, and ran each on a range of inputs:

```
$ ./factorial.instrumented 16
16! = 2004189184
$ ./factorial.instrumented 17
dataflow assertion failed: factorial.c:9:11: in @factorial, %4 was proved positive, but observed -288522240
$ ./sum.instrumented 65535
sum(65535) = 2147450880
$ ./sum.instrumented 65536
dataflow assertion failed: sum.c:8:11: in @sum, %5 was proved positive, but observed -2147450880
```

`factorial.c:9` is `r = r * i;` and `sum.c:8` is `s = s + i;`.
Every input up to the point of overflow passes. The first input that overflows fails.

## 5. Reduced test case

`dynamic/interesting.sh` builds the candidate `.ll` through the whole pipeline, runs it with argument `17`,
and checks that an assertion fires. Running `llvm-reduce` with that test on `factorial.ll` shrank it from 127 lines
to an infinite loop that keeps doubling a value (`dynamic/bug.mlir`):

```mlir
llvm.func @main() -> i32 {
  %0 = llvm.mlir.constant(1 : i32) : i32
  %1 = llvm.mlir.constant(2 : i32) : i32
  llvm.br ^bb1(%0, %0 : i32, i32)
^bb1(%2: i32, %3: i32):            // %2, %3 proved positive
  %4 = llvm.mul %2, %3 : i32       // %4 proved positive
  llvm.br ^bb1(%4, %1 : i32, i32)
}
```

```
dataflow assertion failed: bug.mlir:7:8: in @main, %4 was proved positive, but observed -2147483648
```

The value goes 1, 2, 4, …, 2^30, and then 2^31 wraps to `INT_MIN`.

## 6. The bug

**It was already in the repo. I did not insert it.** `addRule`, `subRule` and `mulRule` in `SignAnalysis.cpp`
use the sign rules for mathematical integers (Pos+Pos=Pos, Pos×Pos=Pos, −Neg=Pos).
But `llvm.add`, `llvm.sub` and `llvm.mul` without `nsw` are modular N-bit arithmetic. Positive + positive
can wrap negative, positive × positive can wrap negative or to zero (65536² = 0 in i32), and `0 − INT_MIN = INT_MIN`.

Nothing static caught this. The only ctest covers `zero-analysis`, and the hand-written `sign-analysis`
examples never overflow. Running the code was what exposed it.

**Fix (not applied):** trust the sign table only for ops with the `nsw` flag. Otherwise, widen any
result that could wrap to top. That covers Pos+Pos, Neg+Neg, negating a value that could be negative, and any product
other than ones involving `0`, `1` or a `Zero` operand.
