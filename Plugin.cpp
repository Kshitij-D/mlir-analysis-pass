//===- Plugin.cpp - Pass definition and plugin entry point ----------------===//
//
// Scaffolding: wires the analysis into a pass and exposes it to mlir-opt.
//
//===----------------------------------------------------------------------===//

#include "Annotate.h"
#include "Instrument.h"
#include "SignAnalysis.h"
#include "ZeroAnalysis.h"

#include "mlir/Analysis/DataFlow/ConstantPropagationAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

struct ZeroAnalysisPass
    : PassWrapper<ZeroAnalysisPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ZeroAnalysisPass)

  ZeroAnalysisPass() = default;
  ZeroAnalysisPass(const ZeroAnalysisPass &other) : PassWrapper(other) {}

  Option<bool> instrument{
      *this, "instrument",
      llvm::cl::desc("Instead of printing facts, insert a runtime check for "
                     "each one (see Instrument.h)"),
      llvm::cl::init(false)};

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  StringRef getArgument() const final { return "zero-analysis"; }

  StringRef getDescription() const final {
    return "Determine which integer values are known zero or known nonzero";
  }

  void runOnOperation() override {
    DataFlowConfig config;
    config.setInterprocedural(false);

    DataFlowSolver solver(config);
    // DeadCodeAnalysis supplies reachability, without which the solver must
    // assume every branch is taken; SparseConstantPropagation resolves branch
    // conditions for it.  Both are prerequisites, not extras.
    solver.load<dataflow::DeadCodeAnalysis>();
    solver.load<dataflow::SparseConstantPropagation>();
    solver.load<zero::ZeroAnalysis>();

    if (failed(solver.initializeAndRun(getOperation()))) {
      getOperation().emitError("zero analysis failed to reach a fixed point");
      return signalPassFailure();
    }

    // Query states only now that the solver has converged.
    if (instrument) {
      zero::instrument(getOperation(), [&](Value value) -> std::optional<zero::Fact> {
        const auto *lattice = solver.lookupState<zero::ZeroLattice>(value);
        if (!lattice)
          return std::nullopt;
        switch (lattice->getValue().kind) {
        case zero::Kind::Zero:
          return zero::Fact{LLVM::ICmpPredicate::eq, 0, "zero"};
        case zero::Kind::NonZero:
          return zero::Fact{LLVM::ICmpPredicate::ne, 0, "nonzero"};
        default:
          return std::nullopt;
        }
      });
      return;
    }

    auto describe = [&](Value value, AsmState &asmState) -> std::string {
      const auto *lattice = solver.lookupState<zero::ZeroLattice>(value);
      if (!lattice)
        return {};
      zero::Kind kind = lattice->getValue().kind;
      // Top and bottom say nothing; printing them would bury the real facts.
      if (kind == zero::Kind::Top || kind == zero::Kind::Bottom)
        return {};
      std::string description;
      llvm::raw_string_ostream os(description);
      value.printAsOperand(os, asmState);
      os << " is " << zero::name(kind);
      return description;
    };

    // stderr, so that mlir-opt's stdout stays the unmodified IR and the two can
    // be redirected independently.
    zero::printAnnotated(getOperation(), describe, llvm::errs());

    // This pass only reads.
    markAllAnalysesPreserved();
  }
};

struct SignAnalysisPass
    : PassWrapper<SignAnalysisPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(SignAnalysisPass)

  SignAnalysisPass() = default;
  SignAnalysisPass(const SignAnalysisPass &other) : PassWrapper(other) {}

  Option<bool> instrument{
      *this, "instrument",
      llvm::cl::desc("Instead of printing facts, insert a runtime check for "
                     "each one (see Instrument.h)"),
      llvm::cl::init(false)};

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  StringRef getArgument() const final { return "sign-analysis"; }

  StringRef getDescription() const final {
    return "Determine what is known about the sign of integer values";
  }

  void runOnOperation() override {
    DataFlowConfig config;
    config.setInterprocedural(false);

    DataFlowSolver solver(config);
    // Same prerequisites as ZeroAnalysisPass, for the same reasons: without
    // reachability the solver must assume every branch is taken, and without
    // constant propagation it cannot resolve branch conditions to prune
    // unreachable ones.
    solver.load<dataflow::DeadCodeAnalysis>();
    solver.load<dataflow::SparseConstantPropagation>();
    solver.load<sign::SignAnalysis>();

    if (failed(solver.initializeAndRun(getOperation()))) {
      getOperation().emitError("sign analysis failed to reach a fixed point");
      return signalPassFailure();
    }

    if (instrument) {
      zero::instrument(getOperation(), [&](Value value) -> std::optional<zero::Fact> {
        const auto *lattice = solver.lookupState<sign::SignLattice>(value);
        if (!lattice)
          return std::nullopt;
        using P = LLVM::ICmpPredicate;
        switch (lattice->getValue().kind) {
        case sign::Kind::Neg:
          return zero::Fact{P::slt, 0, "negative"};
        case sign::Kind::Zero:
          return zero::Fact{P::eq, 0, "zero"};
        case sign::Kind::One:
          return zero::Fact{P::eq, 1, "one"};
        case sign::Kind::Pos:
          return zero::Fact{P::sgt, 0, "positive"};
        case sign::Kind::NonPos:
          return zero::Fact{P::sle, 0, "nonpositive"};
        case sign::Kind::NonZero:
          return zero::Fact{P::ne, 0, "nonzero"};
        case sign::Kind::NonNeg:
          return zero::Fact{P::sge, 0, "nonnegative"};
        default:
          return std::nullopt;
        }
      });
      return;
    }

    auto describe = [&](Value value, AsmState &asmState) -> std::string {
      const auto *lattice = solver.lookupState<sign::SignLattice>(value);
      if (!lattice)
        return {};
      sign::Kind kind = lattice->getValue().kind;
      // Top and bottom say nothing; printing them would bury the real facts.
      if (kind == sign::Kind::Top || kind == sign::Kind::Bottom)
        return {};
      std::string description;
      llvm::raw_string_ostream os(description);
      value.printAsOperand(os, asmState);
      os << " is " << sign::name(kind);
      return description;
    };

    zero::printAnnotated(getOperation(), describe, llvm::errs());

    // This pass only reads.
    markAllAnalysesPreserved();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  // LLVM_VERSION_STRING is baked in at compile time and checked by mlir-opt at
  // load time, which is what turns an ABI mismatch into a clear diagnostic.
  return {MLIR_PLUGIN_API_VERSION, "ZeroAnalysis", LLVM_VERSION_STRING, []() {
            PassRegistration<ZeroAnalysisPass>();
            PassRegistration<SignAnalysisPass>();
          }};
}
