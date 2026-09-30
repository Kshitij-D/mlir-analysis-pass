# MLIR out-of-tree dataflow analysis template

A starting point for writing MLIR dataflow analyses as a loadable `mlir-opt`
plugin, with no LLVM source tree required and nothing to patch upstream.

It currently contains two analyses over integer values in the LLVM dialect:

- **`zero-analysis`** — is this value known to be zero, or known nonzero?
  Two transfer rules.
- **`sign-analysis`** — is this value known negative, zero, positive, or
  (a refinement of positive) known to be exactly the literal `1`? A handful
  of transfer rules, dispatched by a small op-name → rule table.

Both are meant to be read and then replaced: the point of the repo is the
scaffolding around an analysis (the pass, the solver setup, the annotated
printer), not these two specific domains.

Jump to [**a non-trivial fact found in real SQLite source**](#a-worked-example-a-non-trivial-fact-in-real-sqlite-source)
if you just want to see output without building anything.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

That is the whole procedure on Linux, macOS, and WSL2. There is no platform
flag to set and no path to edit. `CMakeLists.txt` finds MLIR by asking
whichever `llvm-config` is on your `PATH` where its CMake package lives, so if
`mlir-opt` runs, the build should configure.

To build against a specific MLIR instead:

```sh
cmake -S . -B build -DMLIR_DIR=/path/to/prefix/lib/cmake/mlir
```

You need an LLVM built with MLIR enabled and plugins enabled
(`-DLLVM_ENABLE_PROJECTS=mlir -DLLVM_ENABLE_PLUGINS=ON`; both are ordinary on
Linux and macOS). Distribution packages work: on Debian and Ubuntu that is
`libmlir-dev` alongside `llvm-dev`. On macOS, Homebrew's `llvm` is the easy
route if it ships `mlir-opt` for your version; otherwise build LLVM yourself.
The configure step diagnoses the cases it can detect — no MLIR found, plugins
disabled in the host LLVM, or an `mlir-opt` on `PATH` whose version does not
match what you are building against.

Both passes are built into one plugin module, `ZeroAnalysis.{so,dylib}` (the
name predates `sign-analysis`; nothing requires renaming it, see "What is
where" below for how to if you want to).

## Running

```sh
./run.sh input.mlir
```

`run.sh` runs `zero-analysis` and locates the plugin whatever it is called on
your platform. For `sign-analysis`, or to see both, invoke `mlir-opt`
directly:

```sh
mlir-opt --load-pass-plugin=build/ZeroAnalysis.so \
         --pass-pipeline='builtin.module(zero-analysis)' \
         input.mlir -o /dev/null

mlir-opt --load-pass-plugin=build/ZeroAnalysis.so \
         --pass-pipeline='builtin.module(sign-analysis)' \
         input.mlir -o /dev/null

# Or both in one solver run, one after the other:
mlir-opt --load-pass-plugin=build/ZeroAnalysis.so \
         --pass-pipeline='builtin.module(zero-analysis,sign-analysis)' \
         input.mlir -o /dev/null
```

using `build/ZeroAnalysis.dylib` on macOS. Each pass leaves the IR unchanged
and writes it to stdout as usual; the annotated view goes to stderr, so the
two streams can be redirected independently. Annotations are comments, so the
annotated listing is still valid MLIR. Values at top or bottom are left
unannotated, so that what prints is exactly what was proved.

## Getting an input

Any LLVM-dialect `.mlir` file works. From C or C++ source, `mlir-translate`
imports straight from LLVM IR:

```sh
clang -S -emit-llvm -o - input.c | mlir-translate --import-llvm > input.mlir
```

### Reproducing the SQLite example below

The worked example further down was produced from SQLite's amalgamated
source (a single-file build of the whole library — this is what the
[SQLite download page](https://www.sqlite.org/download.html) calls the
"autoconf" or "amalgamation" tarball, e.g. `sqlite-autoconf-3540000.tar.gz`):

```sh
curl -LO https://www.sqlite.org/2025/sqlite-autoconf-3540000.tar.gz
tar xf sqlite-autoconf-3540000.tar.gz
cd sqlite-autoconf-3540000

clang -S -emit-llvm -o - sqlite3.c | mlir-translate --import-llvm > sqlite3.mlir

mlir-opt --load-pass-plugin=/path/to/build/ZeroAnalysis.dylib \
         --pass-pipeline='builtin.module(zero-analysis)' \
         sqlite3.mlir -o /dev/null 2> sqlite3.annotated.mlir
```

No project-specific flags — default `configure`/build settings, no `-D`s.
That last point matters for the example: it means `SQLITE_ENABLE_STAT4` is
*not* defined, which is precisely what makes the fact below true. `sqlite3.c`
compiles to roughly 360,000 lines of annotated MLIR and takes about 2.5
seconds to analyze on a laptop. A generated IR dump that size doesn't belong
in a pedagogical project's history, so `.gitignore` now excludes it from
future commits — regenerate it with the recipe above rather than expecting
to find it checked in. What *is* checked in is the small, curated excerpt
below, plus the exact source and IR lines it came from, under `examples/`.

## A worked example: a non-trivial fact in real SQLite source

"The constant 4 is even" is trivial: it only requires reading a literal.
Here is a fact `zero-analysis` proves about real, unmodified SQLite source
that is not trivial in that sense — it depends on an AND whose *result* is
provably zero even though *neither operand is a literal at that line*, one
side being a runtime value loaded from a heap struct that the analysis never
otherwise constrains.

Full writeup with more context: [`examples/sqlite-term-hightruth.md`](examples/sqlite-term-hightruth.md).

In `where.c`'s `whereLoopOutputAdjust`:

```c
struct WhereTerm {
  ...
  u16 wtFlags;            /* TERM_xxx bit flags.  See below */
  ...
};

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

`pTerm->wtFlags` is unconstrained as far as the analysis is concerned — it's
a value loaded from memory that nothing else in the function pins down. But
`TERM_HIGHTRUTH` is a macro that expands to the literal `0` in a default
(non-`STAT4`) build, several thousand lines away from this call site. The
resulting IR (excerpted; `%1` is the function's single, CSE'd `i32` zero,
`%19` is `130` i.e. `WO_EQ|WO_IS`):

```mlir
    %1 = llvm.mlir.constant(0 : i32) : i32 // %1 is zero
    ...
    %19 = llvm.mlir.constant(130 : i32) : i32 // %19 is nonzero
    ...
    %180 = llvm.zext %179 : i16 to i32      // pTerm->eOperator, unconstrained
    %181 = llvm.and %180, %19 : i32         // eOperator & (WO_EQ|WO_IS): genuinely unknown
    %182 = llvm.icmp "ne" %181, %1 : i32
    llvm.cond_br %182, ^bb28, ^bb37
  ^bb28:
    ...
    %186 = llvm.zext %185 : i16 to i32      // pTerm->wtFlags, unconstrained
    %187 = llvm.and %186, %1 : i32 // %187 is zero
    %188 = llvm.icmp "eq" %187, %1 : i32
    llvm.cond_br %188, ^bb29, ^bb37
```

Two structurally identical lines, two different answers, and the analysis
gets both right without knowing either loaded value: `%181` stays
unannotated (genuinely depends on `eOperator` at runtime), while `%187` is
proven zero from the *other* operand alone. The practical upshot: in a
default, non-STAT4 build, `%188` is always true, so the branch to `^bb29` in
this snippet is unconditional and `^bb37` is unreachable from `^bb28` — a
real, if minor, dead-branch fact about a widely deployed C codebase, found by
an analysis with exactly one rule for AND and about 120 lines of code total.

## What is where

| File | |
|---|---|
| `ZeroDomain.h` | The zero/nonzero abstract domain: four lattice elements and their join. |
| `ZeroAnalysis.{h,cpp}` | The zero/nonzero transfer function: two rules, plus a default. |
| `SignDomain.h` | The sign abstract domain: nine lattice elements (including "exactly 1") and their join. |
| `SignAnalysis.{h,cpp}` | The sign transfer function: constant introduction plus an op-name → rule table for `add`/`sub`/`mul`. |
| `Annotate.{h,cpp}` | Prints IR with a comment on each value. Domain-agnostic, shared by both passes. |
| `Plugin.cpp` | Both passes, their solver setup, and the `mlir-opt` entry point. |
| `cmake/RunTest.cmake` | The test runner. |
| `examples/` | Small, checked-in inputs and annotated outputs, referenced above. |

To build a different analysis, copy the shape of `SignDomain.h` +
`SignAnalysis.{h,cpp}`, or replace `ZeroDomain.h` + `ZeroAnalysis.cpp`
in place. To rename the whole plugin, rename the files, the `zero`/`sign`
namespaces, and the corresponding places in `CMakeLists.txt` and
`Plugin.cpp`.

## Tests

`test/zero.mlir` exercises every `zero-analysis` transfer rule.
`test/zero.expected` lists facts that must appear in the output, and — with
a leading `!` — facts that must not. The negative checks are the ones that
matter: an unsound transfer function still produces plausible-looking
output, and only a test that pins down what the analysis must *not* claim
will catch it. `sign-analysis` does not yet have an equivalent `ctest`
entry — `examples/sign-smoke.mlir` and its annotated output exercise every
rule by hand in the meantime (constant sign, the ambiguous `Neg+Pos` and
`Pos-Pos` cases, `Neg*Neg`, and the `x*1`/`1*x` identity that motivated
adding `One` to the domain).

Note that MLIR's printer renumbers SSA values, so the checks are written
against operation text rather than the names in `zero.mlir`. After adding or
reordering operations, regenerate with `./run.sh test/zero.mlir`.

## How the analyses work

`Plugin.cpp` loads three analyses into one solver for each pass.
`DeadCodeAnalysis` supplies reachability — without it the solver must assume
every branch is taken — and `SparseConstantPropagation` resolves branch
conditions on its behalf. These are prerequisites for a precise result, not
optional extras. `ZeroAnalysis`/`SignAnalysis` then propagate their facts
through operations and block arguments until the solver reaches a fixed
point, which is when the pass queries it.

**`zero-analysis`** has two rules, one of each kind an analysis needs:

- **Constants** are zero or nonzero as written. This is the only rule that
  does not consult its operands, and without some rule of this kind there
  would be no facts to propagate at all.
- **`x & y` is zero if either operand is zero**, because a zero operand
  clears every bit. Note what this does not say: two nonzero operands prove
  nothing, since `1 & 2` is `0`.

**`sign-analysis`** has the same constant rule (refined to recognize the
literal `1` specifically), plus `add`/`sub`/`mul`, dispatched through a
name → rule table rather than an if/else chain (there are enough of them
that a chain would just be the table spelled out as control flow).
Multiplying by exactly `1` is special-cased to hand back the other operand's
state untouched — the entire reason the domain tracks `One` as distinct from
general `Pos` — and `Neg + Pos` (and `Pos - Pos`) is the genuinely ambiguous
case that has to join all the way to `Top`, since sign alone can't do better
without tracking magnitude.

Everything else is unknown in both passes. That is always sound, just
imprecise. Values reaching either analysis from outside — function
arguments, and results of any operation without a rule — start at top. Each
domain's bottom element means "not yet proved reachable"; the solver starts
everything there and raises it as facts arrive, which is what makes the
fixed-point iteration terminate.

### A known limitation, visible on real code

Both analyses are intraprocedural and neither models memory: `llvm.load`
always yields top. In a memory-heavy real program like SQLite, most integer
values trace back to a load within one or two operations, so most
arithmetic sees at least one top operand — and top pollutes further
arithmetic almost immediately (`Top + anything-that-could-be-positive` is
itself genuinely ambiguous, since an unconstrained value could be large
negative). Concretely: across all ~6,400 `add`/`sub`/`mul` instructions in
the SQLite build described above, zero produce an annotated (non-top)
result — every real fact recovered there is a constant, or (for
`zero-analysis`) an AND against one. `examples/sign-smoke.mlir` demonstrates
the arithmetic rules firing on hand-written input instead, since real,
load-heavy code essentially never gives them a non-top operand to work
with. Neither pass refines facts on branch conditions either, so a value
tested against zero is not known nonzero on the taken edge — that, and
modeling `llvm.load` against whatever else is known (e.g. a value known
never stored to be negative), are the natural next extensions.

## Notes on portability

Most of the platform-specific knowledge lives in `CMakeLists.txt`, next to the
code it affects. The parts worth knowing about:

**The plugin's file name differs.** It is `ZeroAnalysis.dylib` on macOS and
`ZeroAnalysis.so` on Linux and WSL2. Nothing in this project spells that out:
CMake is asked via `$<TARGET_FILE:ZeroAnalysis>`, and `run.sh` probes for both.

**Linking a plugin on macOS needs special flags.** The plugin deliberately
leaves its MLIR symbols undefined, to be resolved from the `mlir-opt` process
that loads it. On macOS that requires `-undefined dynamic_lookup`, which
`include(HandleLLVMOptions)` supplies. The same include also matches LLVM's
RTTI and exception settings, which differ between distribution packages and
local builds and cause link errors or silent ODR violations when they are
wrong. That is also why `project()` enables C: `HandleLLVMOptions` probes flags
with the C compiler and fails if none is configured.

**A plugin only loads into the LLVM it was built against.** The version is
recorded at compile time and checked at load time, so a mismatch is a clear
error rather than a crash. The configure step warns about it earlier still, by
comparing against the `mlir-opt` it finds.

**The test suite needs no shell.** `cmake/RunTest.cmake` is a CMake script
rather than a shell script, so `ctest` depends on nothing the build did not
already require.

**Under WSL2, build on the Linux filesystem.** A tree under `/mnt/c` is
slow enough to be noticeable and does not reliably carry execute bits.
`.gitattributes` forces LF endings, which keeps `run.sh` working when a
repository is cloned by a Windows git and built inside WSL2.
