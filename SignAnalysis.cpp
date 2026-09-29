//===- SignAnalysis.cpp - Transfer functions ------------------------------===//
//
//
// Constant introduction deliberately stays outside that table: it is
// recognized by the ConstantLike trait (matchPattern/m_Constant), not by a
// specific op name, so it already works across dialects (arith.constant,
// llvm.mlir.constant, ...) without one table row per dialect.
//
//===----------------------------------------------------------------------===//

#include "SignAnalysis.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Matchers.h"
#include "llvm/ADT/StringMap.h"

using namespace mlir;

namespace sign {

namespace {

/// Flip every admitted sign: negative <-> positive, zero stays put.  Used to
/// turn `x - y` into `x + (-y)` so subtraction can reuse the addition table
/// below instead of repeating it with the signs crossed.
SignState negate(SignState s) {
  SignState result = SignState::bottom();
  auto add = [&](Kind k) { result = SignState::join(result, k); };
  if (s.couldBeNegative())
    add(Kind::Pos);
  if (s.couldBeZero())
    add(Kind::Zero);
  if (s.couldBePositive())
    add(Kind::Neg);
  return result;
}

/// x + y, as the join over every combination of admitted atomic signs.
/// Neg+Pos (and its mirror) is the one genuinely ambiguous case: -5+3,
/// -3+3, and -1+5 land on all three signs, so it has to join all the way to
/// Top -- addition can't do better than that without tracking magnitude.
SignState addRule(SignState lhs, SignState rhs) {
  SignState result = SignState::bottom();
  auto add = [&](Kind k) { result = SignState::join(result, k); };
  if (lhs.couldBeNegative() && rhs.couldBeNegative())
    add(Kind::Neg);
  if (lhs.couldBeNegative() && rhs.couldBeZero())
    add(Kind::Neg);
  if (lhs.couldBeNegative() && rhs.couldBePositive())
    add(Kind::Top);
  if (lhs.couldBeZero() && rhs.couldBeNegative())
    add(Kind::Neg);
  if (lhs.couldBeZero() && rhs.couldBeZero())
    add(Kind::Zero);
  if (lhs.couldBeZero() && rhs.couldBePositive())
    add(Kind::Pos);
  if (lhs.couldBePositive() && rhs.couldBeNegative())
    add(Kind::Top);
  if (lhs.couldBePositive() && rhs.couldBeZero())
    add(Kind::Pos);
  if (lhs.couldBePositive() && rhs.couldBePositive())
    add(Kind::Pos);
  return result;
}

/// x - y == x + (-y).
SignState subRule(SignState lhs, SignState rhs) {
  return addRule(lhs, negate(rhs));
}

/// x * y.  Multiplying by exactly 1 is special-cased to hand back the other
/// operand untouched -- this is the entire reason `One` exists in the
/// domain (see SignDomain.h).  Without it, `x * 1` would only ever be known
/// to be `Pos`, throwing away whatever finer fact was already known about
/// `x`.
SignState mulRule(SignState lhs, SignState rhs) {
  if (lhs.isExactlyOne())
    return rhs;
  if (rhs.isExactlyOne())
    return lhs;

  SignState result = SignState::bottom();
  auto add = [&](Kind k) { result = SignState::join(result, k); };
  if (lhs.couldBeNegative() && rhs.couldBeNegative())
    add(Kind::Pos);
  if (lhs.couldBeNegative() && rhs.couldBeZero())
    add(Kind::Zero);
  if (lhs.couldBeNegative() && rhs.couldBePositive())
    add(Kind::Neg);
  if (lhs.couldBeZero())
    add(Kind::Zero); // zero times anything admitted is zero, whatever rhs is
  if (lhs.couldBePositive() && rhs.couldBeNegative())
    add(Kind::Neg);
  if (lhs.couldBePositive() && rhs.couldBeZero())
    add(Kind::Zero);
  if (lhs.couldBePositive() && rhs.couldBePositive())
    add(Kind::Pos);
  return result;
}

using Rule = SignState (*)(SignState, SignState);

/// Op name -> rule, for every binary op whose result sign follows from its
/// operands' signs by table lookup.
const llvm::StringMap<Rule> binaryRules = {
    {LLVM::AddOp::getOperationName(), &addRule},
    {LLVM::SubOp::getOperationName(), &subRule},
    {LLVM::MulOp::getOperationName(), &mulRule},
};

/// A constant's sign is exactly its own sign, refined to `One` when the
/// value is literally 1.
SignState signOf(const llvm::APInt &value) {
  if (value.isZero())
    return Kind::Zero;
  if (value.isNegative())
    return Kind::Neg;
  if (value.isOne())
    return Kind::One;
  return Kind::Pos;
}

} // namespace

void SignAnalysis::setToEntryState(SignLattice *lattice) {
  propagateIfChanged(lattice, lattice->join(SignState::top()));
}

LogicalResult
SignAnalysis::visitOperation(Operation *op,
                             ArrayRef<const SignLattice *> operands,
                             ArrayRef<SignLattice *> results) {
  auto unknown = [&] {
    setAllToEntryStates(results);
    return success();
  };

  if (op->getNumResults() != 1 || !op->getResult(0).getType().isIntOrIndex())
    return unknown();
  SignLattice *result = results[0];

  IntegerAttr value;
  if (matchPattern(op, m_Constant(&value))) {
    propagateIfChanged(result, result->join(signOf(value.getValue())));
    return success();
  }

  auto it = binaryRules.find(op->getName().getStringRef());
  if (it == binaryRules.end() || op->getNumOperands() != 2)
    return unknown();

  SignState lhs = operands[0]->getValue();
  SignState rhs = operands[1]->getValue();

  if (lhs.isBottom() || rhs.isBottom())
    return success();

  propagateIfChanged(result, result->join(it->second(lhs, rhs)));
  return success();
}

} // namespace sign
