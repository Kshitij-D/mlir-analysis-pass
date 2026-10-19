//===- Instrument.h - Turn dataflow facts into runtime assertions ---------===//
//
// Scaffolding, reusable by any analysis whose facts can be phrased as
// `value <pred> constant`.  Each fact becomes, right after the value is
// defined (or at the top of its block, for block arguments):
//
//   %c  = llvm.mlir.constant(<rhs>) : iN
//   %ok = llvm.icmp "<pred>" %value, %c : iN
//   llvm.call @__dataflow_check(%ok, sext(%value), @"<file:line: fact>")
//
// `__dataflow_check` lives in runtime/dataflow_check.c and aborts with the
// message when %ok is false.  A failing check is a soundness bug: the
// analysis proved something the running program contradicts.
//
//===----------------------------------------------------------------------===//

#ifndef ZERO_INSTRUMENT_H
#define ZERO_INSTRUMENT_H

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/ArrayRef.h"

#include <optional>
#include <string>

namespace zero {

/// One proved fact: `value <predicate> rhs` holds whenever `value` is defined.
struct Fact {
  mlir::LLVM::ICmpPredicate predicate;
  int64_t rhs;
  std::string text; // human-readable, e.g. "positive"
};

/// Insert a runtime check for every value inside an `llvm.func` for which
/// `factFor` returns a fact.  Constants are skipped: their checks are true by
/// construction and only add noise.
void instrument(
    mlir::ModuleOp module,
    llvm::function_ref<std::optional<Fact>(mlir::Value)> factFor);

} // namespace zero

#endif
