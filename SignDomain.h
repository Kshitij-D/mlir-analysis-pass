//===- SignDomain.h - The abstract domain ---------------------------------===//
//
// A nine-point lattice recording what is known about the sign of an integer
// value, with one extra refinement below `positive`: whether the value is
// known to be *exactly* the literal 1.
//
//                       Top                      nothing is known
//             /          |          \
//         NonPos       NonZero      NonNeg        <=0, !=0, >=0
//             \         /    \        /
//               \      /       \    /
//                Neg          Zero  Pos           exact facts
//                 \             |    |
//                   \           |   One           exactly the literal 1
//                     \         |   /
//                       \       |  /
//                          Bottom                 unreachable, or not yet
//                          analyzed
//
//===----------------------------------------------------------------------===//

#ifndef SIGN_DOMAIN_H
#define SIGN_DOMAIN_H

#include "llvm/Support/raw_ostream.h"

namespace sign {

enum class Kind {
  Bottom,
  Neg,     // exactly negative
  Zero,    // exactly zero
  One,     // exactly the literal 1
  Pos,     // exactly positive (a superset of One)
  NonPos,  // Neg or Zero       -- "<= 0"
  NonZero, // Neg or Pos        -- "!= 0"
  NonNeg,  // Zero or Pos       -- ">= 0"
  Top,
};

inline const char *name(Kind kind) {
  switch (kind) {
  case Kind::Bottom:
    return "bottom";
  case Kind::Neg:
    return "negative";
  case Kind::Zero:
    return "zero";
  case Kind::One:
    return "one";
  case Kind::Pos:
    return "positive";
  case Kind::NonPos:
    return "nonpositive";
  case Kind::NonZero:
    return "nonzero";
  case Kind::NonNeg:
    return "nonnegative";
  case Kind::Top:
    return "top";
  }
  return "top";
}

struct SignState {
  Kind kind = Kind::Bottom;

  SignState() = default;
  /* implicit */ SignState(Kind kind) : kind(kind) {}

  static SignState bottom() { return Kind::Bottom; }
  static SignState top() { return Kind::Top; }

  bool isBottom() const { return kind == Kind::Bottom; }

  /// Does this state admit the possibility of a negative / zero / positive
  /// value?  Bottom admits none of them, which is the correct answer for
  /// "not yet proven to be anything".  `One` counts as admitting positive,
  /// since {1} is a subset of the positives.
  bool couldBeNegative() const {
    switch (kind) {
    case Kind::Neg:
    case Kind::NonPos:
    case Kind::NonZero:
    case Kind::Top:
      return true;
    default:
      return false;
    }
  }

  bool couldBeZero() const {
    switch (kind) {
    case Kind::Zero:
    case Kind::NonPos:
    case Kind::NonNeg:
    case Kind::Top:
      return true;
    default:
      return false;
    }
  }

  bool couldBePositive() const {
    switch (kind) {
    case Kind::One:
    case Kind::Pos:
    case Kind::NonZero:
    case Kind::NonNeg:
    case Kind::Top:
      return true;
    default:
      return false;
    }
  }

  bool isExactlyOne() const { return kind == Kind::One; }

  /// Least upper bound.
  static SignState join(const SignState &lhs, const SignState &rhs) {
    if (lhs.kind == rhs.kind)
      return lhs;
    if (lhs.isBottom())
      return rhs;
    if (rhs.isBottom())
      return lhs;
    if (lhs.kind == Kind::Top || rhs.kind == Kind::Top)
      return top();

    // What's left is two *different* facts, neither Bottom nor Top.
    if (static_cast<int>(lhs.kind) > static_cast<int>(rhs.kind))
      return join(rhs, lhs);

    switch (lhs.kind) {

    case Kind::Neg:
      switch (rhs.kind) {
      case Kind::Zero:
        return Kind::NonPos;
      case Kind::One:
      case Kind::Pos:
        return Kind::NonZero;
      case Kind::NonPos:
        return Kind::NonPos;
      case Kind::NonZero:
        return Kind::NonZero;
      case Kind::NonNeg:
        return Kind::Top;
      default:
        break;
      }
      break;

    case Kind::Zero:
      switch (rhs.kind) {
      case Kind::One:
      case Kind::Pos:
        return Kind::NonNeg;
      case Kind::NonPos:
        return Kind::NonPos;
      case Kind::NonZero:
        return Kind::Top;
      case Kind::NonNeg:
        return Kind::NonNeg;
      default:
        break;
      }
      break;

    case Kind::One:
      switch (rhs.kind) {
      case Kind::Pos:
        return Kind::Pos;
      case Kind::NonPos:
        return Kind::Top;
      case Kind::NonZero:
        return Kind::NonZero;
      case Kind::NonNeg:
        return Kind::NonNeg;
      default:
        break;
      }
      break;

    case Kind::Pos:
      switch (rhs.kind) {
      case Kind::NonPos:
        return Kind::Top;
      case Kind::NonZero:
        return Kind::NonZero;
      case Kind::NonNeg:
        return Kind::NonNeg;
      default:
        break;
      }
      break;

    case Kind::NonPos:
      switch (rhs.kind) {
      case Kind::NonZero:
      case Kind::NonNeg:
        return Kind::Top;
      default:
        break;
      }
      break;

    case Kind::NonZero:
      if (rhs.kind == Kind::NonNeg)
        return Kind::Top;
      break;

    default:
      break;
    }

    // Unreachable
    return top();
  }

  bool operator==(const SignState &other) const { return kind == other.kind; }
  bool operator!=(const SignState &other) const { return kind != other.kind; }

  void print(llvm::raw_ostream &os) const { os << name(kind); }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const SignState &state) {
  state.print(os);
  return os;
}

} // namespace sign

#endif
