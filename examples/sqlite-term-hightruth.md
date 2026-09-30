# A non-trivial fact found in real SQLite source

This is `zero-analysis` run over the LLVM dialect IR produced from SQLite's
amalgamated `sqlite3.c` (version 3.54.0, built with default configure flags —
notably *without* `-DSQLITE_ENABLE_STAT4`). Reproduction steps are in the
top-level `README.md`.

## The source

`sqlite3.c`, inside `whereLoopOutputAdjust` (in `where.c`):

```c
struct WhereTerm {
  Expr *pExpr;
  WhereClause *pWC;
  LogEst truthProb;
  u16 wtFlags;            /* TERM_xxx bit flags.  See below */
  u16 eOperator;          /* A WO_xx value describing <op> */
  ...
};

#define TERM_HEURTRUTH  0x2000 /* Heuristic truthProb used */
#ifdef SQLITE_ENABLE_STAT4
#  define TERM_HIGHTRUTH  0x4000 /* Term excludes few rows */
#else
#  define TERM_HIGHTRUTH  0      /* Only used with STAT4 */
#endif

  ...
  if( (pTerm->eOperator&(WO_EQ|WO_IS))!=0
   && (pTerm->wtFlags & TERM_HIGHTRUTH)==0  /* tag-20200224-1 */
  ){
```

`pTerm->wtFlags` is a runtime value read out of a `WhereTerm` on the heap —
nothing in this function constrains it, so as far as this analysis (or any
static analysis that doesn't special-case this macro) is concerned it could
be any `u16`. But `TERM_HIGHTRUTH` itself is a compile-time macro that
expands to the literal `0` unless `SQLITE_ENABLE_STAT4` is defined, which it
is not in this build. So `pTerm->wtFlags & TERM_HIGHTRUTH` is `pTerm->wtFlags
& 0` — provably zero **for every possible value of `wtFlags`**, not because
either operand is a small/obviously-zero constant read directly off the
page, but because the AND-with-zero rule fires on the macro-expanded side
while the other operand stays completely unconstrained. That is the
difference between this and "the constant 4 is even": nobody local to this
line wrote a literal `0`, and the reason it's `0` lives in a `#define`
several thousand lines away, gated on a feature macro.

## What the analysis proves

Excerpted from the full annotated output (`llvm.mlir.constant`s renumbered
by MLIR's printer; `%1` is the module's single, CSE'd `i32` zero and `%19`
is `130` i.e. `WO_EQ|WO_IS`):

```mlir
    %1 = llvm.mlir.constant(0 : i32) : i32 // %1 is zero
    ...
    %19 = llvm.mlir.constant(130 : i32) : i32 // %19 is nonzero
    ...
  ^bb27:  // pred: ^bb25
    ...
    %179 = llvm.load %178 {alignment = 4 : i64} : !llvm.ptr -> i16
    %180 = llvm.zext %179 : i16 to i32
    %181 = llvm.and %180, %19 : i32
    %182 = llvm.icmp "ne" %181, %1 : i32
    llvm.cond_br %182, ^bb28, ^bb37
  ^bb28:  // pred: ^bb27
    %183 = llvm.load %30 {alignment = 8 : i64} : !llvm.ptr -> !llvm.ptr
    %184 = llvm.getelementptr inbounds|nuw %183[%1, 3]
        : (!llvm.ptr, i32) -> !llvm.ptr,
          !llvm.struct<"struct.WhereTerm",
              (ptr, ptr, i16, i16, i16, i8, i8, i32, i32,
               struct<"union.anon.18", (ptr)>, i64, i64)>
    %185 = llvm.load %184 {alignment = 2 : i64} : !llvm.ptr -> i16
    %186 = llvm.zext %185 : i16 to i32
    %187 = llvm.and %186, %1 : i32 // %187 is zero
    %188 = llvm.icmp "eq" %187, %1 : i32
    llvm.cond_br %188, ^bb29, ^bb37
```

`%180` is `pTerm->eOperator` (loaded from the struct, unconstrained), ANDed
with the real bitmask `%19` (`130`) — the analysis correctly says nothing
about `%181`, because it genuinely depends on the runtime value of
`eOperator`. Three lines later, `%186` is `pTerm->wtFlags` (also loaded from
the struct, also unconstrained on its own), ANDed with `%1` (`0`) — and the
analysis *does* prove `%187` is zero, purely from the zero operand, without
ever needing to know what `wtFlags` is. That's the AND-with-zero rule
(`ZeroAnalysis.cpp`) doing real work: it tells apart a runtime-dependent
bitmask test from one that is dead by construction, using only the shape of
the operation, not the loaded value.

The practical upshot: `%188 = llvm.icmp "eq" %187, %1` is provably always
true in this build configuration, so the branch to `^bb29` is unconditional
and `^bb37` is unreachable from `^bb28` — a real (if minor) dead-branch fact
about a widely-deployed C codebase, found by a two-rule, ~120-line analysis.
