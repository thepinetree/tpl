#pragma once

#include <cmath>

#include "sql/value.h"
#include "util/arithmetic_overflow.h"

namespace tpl::sql {

/**
 * Utility class to handle various arithmetic SQL functions.
 */
class ArithmeticFunctions {
 public:
  // Delete to force only static functions
  ArithmeticFunctions() = delete;

  static void Add(Integer *result, const Integer &a, const Integer &b);
  static void Add(Integer *result, const Integer &a, const Integer &b,
                  bool *overflow);
  static void Add(Real *result, const Real &a, const Real &b);
  static void Sub(Integer *result, const Integer &a, const Integer &b);
  static void Sub(Integer *result, const Integer &a, const Integer &b,
                  bool *overflow);
  static void Sub(Real *result, const Real &a, const Real &b);
  static void Mul(Integer *result, const Integer &a, const Integer &b);
  static void Mul(Integer *result, const Integer &a, const Integer &b,
                  bool *overflow);
  static void Mul(Real *result, const Real &a, const Real &b);
  static void IntDiv(Integer *result, const Integer &a, const Integer &b,
                     bool *div_by_zero);
  static void Div(Real *result, const Real &a, const Real &b,
                  bool *div_by_zero);
  static void IntMod(Integer *result, const Integer &a, const Integer &b,
                     bool *div_by_zero);
  static void Mod(Real *result, const Real &a, const Real &b,
                  bool *div_by_zero);

  static void Pi(Real *result);
  static void E(Real *result);
  static void Abs(Integer *result, const Integer &v);
  static void Abs(Real *result, const Real &v);
  static void Sin(Real *result, const Real &v);
  static void Asin(Real *result, const Real &v);
  static void Cos(Real *result, const Real &v);
  static void Acos(Real *result, const Real &v);
  static void Tan(Real *result, const Real &v);
  static void Cot(Real *result, const Real &v);
  static void Atan(Real *result, const Real &v);
  static void Atan2(Real *result, const Real &a, const Real &b);
  static void Cosh(Real *result, const Real &v);
  static void Tanh(Real *result, const Real &v);
  static void Sinh(Real *result, const Real &v);
  static void Sqrt(Real *result, const Real &v);
  static void Cbrt(Real *result, const Real &v);
  static void Exp(Real *result, const Real &v);
  static void Ceil(Real *result, const Real &v);
  static void Floor(Real *result, const Real &v);
  static void Truncate(Real *result, const Real &v);
  static void Ln(Real *result, const Real &v);
  static void Log2(Real *result, const Real &v);
  static void Log10(Real *result, const Real &v);
  static void Sign(Real *result, const Real &v);
  static void Radians(Real *result, const Real &v);
  static void Degrees(Real *result, const Real &v);
  static void Round(Real *result, const Real &v);
  static void RoundUpTo(Real *result, const Real &v, const Integer &scale);
  static void Log(Real *result, const Real &base, const Real &val);
  static void Pow(Real *result, const Real &base, const Real &val);

 private:
  // Cotangent
  static double cotan(const double arg) { return (1.0 / std::tan(arg)); }
};

// ---------------------------------------------------------
// Implementation below
// ---------------------------------------------------------

// The functions below are inlined in the header for performance. Don't move it
// unless you know what you're doing.

#define UNARY_MATH_EXPENSIVE_HIDE_NULL(NAME, RET_TYPE, INPUT_TYPE, FN) \
  inline void ArithmeticFunctions::NAME(RET_TYPE *result,              \
                                        const INPUT_TYPE &v) {         \
    if (v.is_null) {                                                   \
      *result = RET_TYPE::Null();                                      \
      return;                                                          \
    }                                                                  \
    *result = RET_TYPE(FN(v.val));                                     \
  }

#define BINARY_MATH_EXPENSIVE_HIDE_NULL(NAME, RET_TYPE, INPUT_TYPE1,  \
                                        INPUT_TYPE2, FN)              \
  inline void ArithmeticFunctions::NAME(                              \
      RET_TYPE *result, const INPUT_TYPE1 &a, const INPUT_TYPE2 &b) { \
    if (a.is_null || b.is_null) {                                     \
      *result = RET_TYPE::Null();                                     \
      return;                                                         \
    }                                                                 \
    *result = RET_TYPE(FN(a.val, b.val));                             \
  }

#define BINARY_MATH_FAST_HIDE_NULL(NAME, RET_TYPE, INPUT_TYPE1, INPUT_TYPE2, \
                                   OP)                                       \
  inline void ArithmeticFunctions::NAME(                                     \
      RET_TYPE *result, const INPUT_TYPE1 &a, const INPUT_TYPE2 &b) {        \
    result->is_null = (a.is_null || b.is_null);                              \
    result->val = a.val OP b.val;                                            \
  }

#define BINARY_MATH_FAST_HIDE_NULL_OVERFLOW(NAME, RET_TYPE, INPUT_TYPE1, \
                                            INPUT_TYPE2, FN)             \
  inline void ArithmeticFunctions::NAME(                                 \
      RET_TYPE *result, const INPUT_TYPE1 &a, const INPUT_TYPE2 &b,      \
      bool *overflow) {                                                  \
    result->is_null = (a.is_null || b.is_null);                          \
    *overflow = FN(a.val, b.val, &result->val);                          \
  }

#define BINARY_OP_CHECK_ZERO(NAME, RET_TYPE, INPUT_TYPE1, INPUT_TYPE2, OP) \
  inline void ArithmeticFunctions::NAME(                                   \
      RET_TYPE *result, const INPUT_TYPE1 &a, const INPUT_TYPE2 &b,        \
      bool *div_by_zero) {                                                 \
    if (a.is_null || b.is_null || b.val == 0) {                            \
      *div_by_zero = true;                                                 \
      *result = RET_TYPE::Null();                                          \
      return;                                                              \
    }                                                                      \
    *result = RET_TYPE(a.val OP b.val);                                    \
  }

#define BINARY_FN_CHECK_ZERO(NAME, RET_TYPE, INPUT_TYPE1, INPUT_TYPE2, FN) \
  inline void ArithmeticFunctions::NAME(                                   \
      RET_TYPE *result, const INPUT_TYPE1 &a, const INPUT_TYPE2 &b,        \
      bool *div_by_zero) {                                                 \
    if (a.is_null || b.is_null || b.val == 0) {                            \
      *div_by_zero = true;                                                 \
      *result = RET_TYPE::Null();                                          \
      return;                                                              \
    }                                                                      \
    *result = RET_TYPE(FN(a.val, b.val));                                  \
  }

BINARY_MATH_FAST_HIDE_NULL(Add, Integer, Integer, Integer, +);
BINARY_MATH_FAST_HIDE_NULL(Add, Real, Real, Real, +);
BINARY_MATH_FAST_HIDE_NULL(Sub, Integer, Integer, Integer, -);
BINARY_MATH_FAST_HIDE_NULL(Sub, Real, Real, Real, -);
BINARY_MATH_FAST_HIDE_NULL(Mul, Integer, Integer, Integer, *);
BINARY_MATH_FAST_HIDE_NULL(Mul, Real, Real, Real, *);

BINARY_MATH_FAST_HIDE_NULL_OVERFLOW(Add, Integer, Integer, Integer,
                                    util::ArithmeticOverflow::Add);
BINARY_MATH_FAST_HIDE_NULL_OVERFLOW(Sub, Integer, Integer, Integer,
                                    util::ArithmeticOverflow::Sub);
BINARY_MATH_FAST_HIDE_NULL_OVERFLOW(Mul, Integer, Integer, Integer,
                                    util::ArithmeticOverflow::Mul);

BINARY_OP_CHECK_ZERO(IntDiv, Integer, Integer, Integer, /);
BINARY_OP_CHECK_ZERO(Div, Real, Real, Real, /);
BINARY_OP_CHECK_ZERO(IntMod, Integer, Integer, Integer, %);

inline void ArithmeticFunctions::Mod(Real *result, const Real &a, const Real &b,
                                     bool *div_by_zero) {
  *div_by_zero = (b.val == 0);
  if (a.is_null || b.is_null || b.val == 0) {
    *result = Real::Null();
    return;
  }
  *result = Real(std::fmod(a.val, b.val));
}

inline void ArithmeticFunctions::Pi(Real *result) { *result = Real(M_PI); }

inline void ArithmeticFunctions::E(Real *result) { *result = Real(M_E); }

UNARY_MATH_EXPENSIVE_HIDE_NULL(Abs, Integer, Integer, std::llabs);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Abs, Real, Real, std::fabs);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Sin, Real, Real, std::sin);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Asin, Real, Real, std::asin);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Cos, Real, Real, std::cos);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Acos, Real, Real, std::acos);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Tan, Real, Real, std::tan);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Cot, Real, Real, cotan);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Atan, Real, Real, std::atan);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Cosh, Real, Real, std::cosh);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Tanh, Real, Real, std::tanh);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Sinh, Real, Real, std::sinh);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Sqrt, Real, Real, std::sqrt);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Cbrt, Real, Real, std::cbrt);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Ceil, Real, Real, std::ceil);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Floor, Real, Real, std::floor);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Truncate, Real, Real, std::trunc);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Ln, Real, Real, std::log);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Log2, Real, Real, std::log2);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Log10, Real, Real, std::log10);
UNARY_MATH_EXPENSIVE_HIDE_NULL(Exp, Real, Real, std::exp);

BINARY_MATH_EXPENSIVE_HIDE_NULL(Atan2, Real, Real, Real, std::atan2);
BINARY_MATH_EXPENSIVE_HIDE_NULL(Pow, Real, Real, Real, std::pow);

#undef BINARY_FN_CHECK_ZERO
#undef BINARY_OP_CHECK_ZERO
#undef BINARY_MATH_FAST_HIDE_NULL_OVERFLOW
#undef BINARY_MATH_FAST_HIDE_NULL
#undef BINARY_MATH_EXPENSIVE_HIDE_NULL
#undef UNARY_MATH_EXPENSIVE_HIDE_NULL

inline void ArithmeticFunctions::Sign(Real *result, const Real &v) {
  if (v.is_null) {
    *result = Real::Null();
  }
  *result = Real((v.val > 0) ? 1.0f : ((v.val < 0) ? -1.0f : 0.0f));
}

inline void ArithmeticFunctions::Radians(Real *result, const Real &v) {
  if (v.is_null) {
    *result = Real::Null();
  }
  *result = Real(v.val * M_PI / 180.0);
}

inline void ArithmeticFunctions::Degrees(Real *result, const Real &v) {
  if (v.is_null) {
    *result = Real::Null();
  }
  *result = Real(v.val * 180.0 / M_PI);
}

inline void ArithmeticFunctions::Round(Real *result, const Real &v) {
  if (v.is_null) {
    *result = Real::Null();
  }
  *result = Real(v.val + ((v.val < 0) ? -0.5 : 0.5));
}

inline void ArithmeticFunctions::RoundUpTo(Real *result, const Real &v,
                                           const Integer &scale) {
  if (v.is_null || scale.is_null) {
    *result = Real::Null();
  }
  *result = Real(std::floor(v.val * std::pow(10.0, scale.val) + 0.5) /
                 std::pow(10.0, scale.val));
}

inline void ArithmeticFunctions::Log(Real *result, const Real &base,
                                     const Real &val) {
  if (base.is_null || val.is_null) {
    *result = Real::Null();
  }
  *result = Real(std::log(val.val) / std::log(base.val));
}

}  // namespace tpl::sql