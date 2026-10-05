# llvm-reduce reductions

Three `llvm-reduce` runs, each driven by an interestingness test that
converts its candidate `.ll` to MLIR (`mlir-translate --import-llvm`) and
checks whether one of this repo's passes computes a specific, non-trivial
fact about it — not just "some constant has a known sign/zero-ness," which
would be the "the constant 4 is even" triviality the assignment warns
against. Each test targets a different mechanism in the analyses, and each
is constructed so that only that mechanism can make the grep pattern match
(reasoning is in each script's header comment).

| # | Pass | Mechanism exercised | What the test greps for |
|---|---|---|---|
| 1 | `zero-analysis` | `ZeroAnalysis.cpp`'s AND-with-zero propagation rule | an `llvm.and` result proved `zero`, where one operand is still a live function argument (not foldable to a lone constant) |
| 2 | `sign-analysis` | `mulRule`'s `x*1` identity-preservation fast path | an `llvm.mul` result annotated `is one` — the generic sign-product table can *only* construct `Neg`/`Zero`/`Pos`, never `One`, so this can only come from the fast path handing back an already-`One` operand |
| 3 | `sign-analysis` | the lattice `join()` itself, across control flow | a block **argument** (former phi node) annotated `is nonpositive`, from merging a `Neg` edge and a `Zero` edge — a fact no single transfer rule produces, only MLIR's dataflow framework joining the two incoming edges |

Reproduce any of them:

```sh
cd reductions
./testN.sh inputN.ll ; echo $?        # confirm the *un*-reduced input is interesting (prints 0)
llvm-reduce --test=./testN.sh inputN.ll   # writes reduced.ll
mlir-translate --import-llvm reduced.ll \
  | mlir-opt --load-pass-plugin=../build/ZeroAnalysis.dylib \
             --pass-pipeline='builtin.module(zero-analysis)' \  # or sign-analysis, see table
             - 2>&1 1>/dev/null            # the annotated listing, i.e. reducedN.mlir
```

(`ZeroAnalysis.dylib` on macOS, `ZeroAnalysis.so` on Linux/WSL2 — see the
top-level README.)

## Results

**Target 1** — `input1.ll` has a dead-code-laden function; `and i32 %flags, 0`
survives because `%flags` must remain a free argument for the test to keep
matching. Reduced to:

```llvm
define i32 @check_flag(i32 %flags) {
entry:
  %masked = and i32 %flags, 0
  ret i32 %masked
}
```

**Target 2** — reduced straight to the essence of the claim, `1 * 1`:

```llvm
define i32 @one_times_one() {
entry:
  %r = mul i32 1, 1
  ret i32 %r
}
```

**Target 3** — reduced to a two-predecessor merge; llvm-reduce also folded
the branch condition to a literal `i1 false` along the way (interesting in
its own right — the dead edge is *not* pruned from the join in this
analysis, since `DeadCodeAnalysis`'s reachability pruning and the LLVM
dialect's `cond_br` don't eliminate it here, so the join still runs over
both predecessors):

```llvm
define i32 @pick_sign() {
entry:
  br i1 false, label %merge, label %else

else:
  br label %merge

merge:
  %y = phi i32 [ 0, %else ], [ -3, %entry ]
  ret i32 %y
}
```

Full annotated output for each is in `reducedN.mlir`.
