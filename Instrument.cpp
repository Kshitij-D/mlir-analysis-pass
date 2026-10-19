//===- Instrument.cpp -----------------------------------------------------===//

#include "Instrument.h"

#include "mlir/IR/AsmState.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Matchers.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace zero {

namespace {

constexpr llvm::StringLiteral checkName = "__dataflow_check";

struct PendingCheck {
  Value value;
  Fact fact;
  std::string message;
};

/// "file:line:col" from the first file location buried in `loc` (imported
/// LLVM IR wraps debug locations in FusedLocs), or "<unknown>".
std::string where(Location loc) {
  std::string result = "<unknown>";
  loc->walk([&](Location inner) {
    if (auto file = dyn_cast<FileLineColLoc>(inner)) {
      result = (file.getFilename().str() + ":" +
                std::to_string(file.getLine()) + ":" +
                std::to_string(file.getColumn()));
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return result;
}

/// Only values computed inside a function body can be checked at runtime;
/// global initializer regions are evaluated at compile time.
LLVM::LLVMFuncOp enclosingFunction(Value value) {
  return value.getParentRegion()->getParentOfType<LLVM::LLVMFuncOp>();
}

} // namespace

void instrument(ModuleOp module,
                llvm::function_ref<std::optional<Fact>(Value)> factFor) {
  // Collect first, mutate after: the solver's states are keyed on the IR as
  // analyzed, and SSA names should match the annotated listing.
  AsmState asmState(module);
  SmallVector<PendingCheck> pending;

  auto consider = [&](Value value, Location loc) {
    auto type = dyn_cast<IntegerType>(value.getType());
    if (!type || type.getWidth() > 64)
      return;
    LLVM::LLVMFuncOp func = enclosingFunction(value);
    if (!func)
      return;
    std::optional<Fact> fact = factFor(value);
    if (!fact)
      return;
    std::string message;
    llvm::raw_string_ostream os(message);
    os << where(loc) << ": in @" << func.getSymName() << ", ";
    value.printAsOperand(os, asmState);
    os << " was proved " << fact->text;
    pending.push_back({value, *fact, std::move(message)});
  };

  module->walk([&](Operation *op) {
    if (!op->hasTrait<OpTrait::IsTerminator>() &&
        !op->hasTrait<OpTrait::ConstantLike>())
      for (Value result : op->getResults())
        consider(result, op->getLoc());
    for (Region &region : op->getRegions())
      for (Block &block : region)
        for (BlockArgument argument : block.getArguments())
          consider(argument, argument.getLoc());
  });

  if (pending.empty())
    return;

  MLIRContext *context = module.getContext();
  OpBuilder builder(context);
  auto i1 = IntegerType::get(context, 1);
  auto i64 = IntegerType::get(context, 64);
  auto ptr = LLVM::LLVMPointerType::get(context);
  Location unknown = builder.getUnknownLoc();

  // declare void @__dataflow_check(i1, i64, ptr)
  builder.setInsertionPointToStart(module.getBody());
  auto checkFunc = module.lookupSymbol<LLVM::LLVMFuncOp>(checkName);
  if (!checkFunc)
    checkFunc = LLVM::LLVMFuncOp::create(
        builder, unknown, checkName,
        LLVM::LLVMFunctionType::get(LLVM::LLVMVoidType::get(context),
                                    {i1, i64, ptr}));

  unsigned counter = 0;
  for (PendingCheck &check : pending) {
    // The message, as a private constant global.
    builder.setInsertionPointToStart(module.getBody());
    std::string text = check.message;
    text.push_back('\0');
    auto arrayType = LLVM::LLVMArrayType::get(IntegerType::get(context, 8),
                                              text.size());
    auto global = LLVM::GlobalOp::create(
        builder, unknown, arrayType, /*isConstant=*/true,
        LLVM::Linkage::Private,
        ("__dataflow_msg_" + std::to_string(counter++)),
        builder.getStringAttr(text));

    Value value = check.value;
    if (auto argument = dyn_cast<BlockArgument>(value))
      builder.setInsertionPointToStart(argument.getOwner());
    else
      builder.setInsertionPointAfter(value.getDefiningOp());
    Location loc = value.getLoc();

    auto type = cast<IntegerType>(value.getType());
    Value rhs = LLVM::ConstantOp::create(
        builder, loc, type, builder.getIntegerAttr(type, check.fact.rhs));
    Value ok =
        LLVM::ICmpOp::create(builder, loc, check.fact.predicate, value, rhs);
    Value observed =
        type.getWidth() == 64
            ? value
            : LLVM::SExtOp::create(builder, loc, i64, value).getResult();
    Value message = LLVM::AddressOfOp::create(builder, loc, global);
    LLVM::CallOp::create(builder, loc, checkFunc,
                         ValueRange{ok, observed, message});
  }
}

} // namespace zero
