#include <limits>
#include <memory>
#include <random>
#include <vector>

#include "tpl_test.h"  // NOLINT

#include "sql/functions/arithmetic_functions.h"
#include "sql/value.h"
#include "util/timer.h"

namespace tpl::sql::test {

class ArithmeticFunctionsTests : public TplTest {
 protected:
  inline double cotan(const double arg) { return (1.0 / std::tan(arg)); }
};

TEST_F(ArithmeticFunctionsTests, IntegerValueTests) {
  // Nulls
  {
    Integer a(0), b = Integer::Null(), result(0);

    ArithmeticFunctions::Add(&result, a, b);
    EXPECT_TRUE(result.is_null);

    result = Integer(0);
    ArithmeticFunctions::Sub(&result, a, b);
    EXPECT_TRUE(result.is_null);

    result = Integer(0);
    ArithmeticFunctions::Mul(&result, a, b);
    EXPECT_TRUE(result.is_null);

    bool div_by_zero = false;
    result = Integer(0);
    ArithmeticFunctions::IntDiv(&result, a, b, &div_by_zero);
    EXPECT_TRUE(result.is_null);
  }

  // Proper
  {
    const auto aval = 10, bval = 4;
    Integer a(aval), b(bval), result(0);

    ArithmeticFunctions::Add(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval + bval, result.val);

    result = Integer(0);
    ArithmeticFunctions::Sub(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval - bval, result.val);

    result = Integer(0);
    ArithmeticFunctions::Mul(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval * bval, result.val);

    bool div_by_zero = false;
    result = Integer(0);
    ArithmeticFunctions::IntDiv(&result, a, b, &div_by_zero);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval / bval, result.val);
  }

  // Overflow
  {
    const auto aval = std::numeric_limits<i64>::max() - 1, bval = 4l;
    Integer a(aval), b(bval), result(0);

    bool overflow = false;
    ArithmeticFunctions::Add(&result, a, b, &overflow);
    EXPECT_FALSE(result.is_null);
    EXPECT_TRUE(overflow);
  }

  {
    const auto aval = std::numeric_limits<i64>::min() + 1, bval = 4l;
    Integer a(aval), b(bval), result(0);

    bool overflow = false;
    ArithmeticFunctions::Sub(&result, a, b, &overflow);
    EXPECT_FALSE(result.is_null);
    EXPECT_TRUE(overflow);
  }

  {
    const auto aval = std::numeric_limits<i64>::max() - 1, bval = aval;
    Integer a(aval), b(bval), result(0);

    bool overflow = false;
    ArithmeticFunctions::Mul(&result, a, b, &overflow);
    EXPECT_FALSE(result.is_null);
    EXPECT_TRUE(overflow);
  }
}

TEST_F(ArithmeticFunctionsTests, RealValueTests) {
  // Nulls
  {
    Real a(0.0), b = Real::Null(), result(0.0);

    ArithmeticFunctions::Add(&result, a, b);
    EXPECT_TRUE(result.is_null);

    result = Real(0.0);
    ArithmeticFunctions::Sub(&result, a, b);
    EXPECT_TRUE(result.is_null);

    result = Real(0.0);
    ArithmeticFunctions::Mul(&result, a, b);
    EXPECT_TRUE(result.is_null);

    bool div_by_zero = false;
    result = Real(0.0);
    ArithmeticFunctions::Div(&result, a, b, &div_by_zero);
    EXPECT_TRUE(result.is_null);
  }

  // Proper
  {
    const auto aval = 10.0, bval = 4.0;
    Real a(aval), b(bval), result(0.0);

    ArithmeticFunctions::Add(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval + bval, result.val);

    result = Real(0.0);
    ArithmeticFunctions::Sub(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval - bval, result.val);

    result = Real(0.0);
    ArithmeticFunctions::Mul(&result, a, b);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval * bval, result.val);

    bool div_by_zero = false;
    result = Real(0.0);
    ArithmeticFunctions::Div(&result, a, b, &div_by_zero);
    EXPECT_FALSE(result.is_null);
    EXPECT_EQ(aval / bval, result.val);
  }
}

TEST_F(ArithmeticFunctionsTests, SimplePiETest) {
  {
    Real pi(0.0);
    ArithmeticFunctions::Pi(&pi);
    EXPECT_FALSE(pi.is_null);
    EXPECT_DOUBLE_EQ(M_PI, pi.val);
  }

  {
    Real e(0.0);
    ArithmeticFunctions::E(&e);
    EXPECT_FALSE(e.is_null);
    EXPECT_DOUBLE_EQ(M_E, e.val);
  }
}

TEST_F(ArithmeticFunctionsTests, TrigFunctionsTest) {
  std::vector<double> inputs, arc_inputs;

  std::mt19937 gen;
  std::uniform_real_distribution dist(1.0, 1000.0);
  std::uniform_real_distribution arc_dist(-1.0, 1.0);
  for (u32 i = 0; i < 100; i++) {
    inputs.push_back(dist(gen));
    arc_inputs.push_back(arc_dist(gen));
  }

#define CHECK_HANDLES_NULL(TPL_FUNC, C_FUNC)  \
  {                                           \
    Real arg = Real::Null();                  \
    Real ret(0.0);                            \
    ArithmeticFunctions::TPL_FUNC(&ret, arg); \
    EXPECT_TRUE(ret.is_null);                 \
  }
#define CHECK_HANDLES_NONNULL(TPL_FUNC, C_FUNC) \
  {                                             \
    Real arg(input);                            \
    Real ret(0.0);                              \
    ArithmeticFunctions::TPL_FUNC(&ret, arg);   \
    EXPECT_FALSE(ret.is_null);                  \
    EXPECT_DOUBLE_EQ(C_FUNC(input), ret.val);   \
  }

#define CHECK_FUNC(TPL_FUNC, C_FUNC)   \
  CHECK_HANDLES_NULL(TPL_FUNC, C_FUNC) \
  CHECK_HANDLES_NONNULL(TPL_FUNC, C_FUNC)

  // Check some of the trig functions on all inputs
  for (const auto input : inputs) {
    CHECK_FUNC(Cos, std::cos);
    CHECK_FUNC(Cot, cotan);
    CHECK_FUNC(Sin, std::sin);
    CHECK_FUNC(Tan, std::tan);
    CHECK_FUNC(Cosh, std::cosh);
    CHECK_FUNC(Tanh, std::tanh);
    CHECK_FUNC(Sinh, std::sinh);
  }

  for (const auto input : arc_inputs) {
    CHECK_FUNC(Acos, std::acos);
    CHECK_FUNC(Asin, std::asin);
    CHECK_FUNC(Atan, std::atan);
  }

#undef CHECK_FUNC
#undef CHECK_HANDLES_NONNULL
#undef CHECK_HANDLES_NULL
}

TEST_F(ArithmeticFunctionsTests, MathFuncsTest) {
#define CHECK_HANDLES_NULL(TPL_FUNC, C_FUNC)  \
  {                                           \
    Real arg = Real::Null();                  \
    Real ret(0.0);                            \
    ArithmeticFunctions::TPL_FUNC(&ret, arg); \
    EXPECT_TRUE(ret.is_null);                 \
  }
#define CHECK_HANDLES_NONNULL(TPL_FUNC, C_FUNC, INPUT) \
  {                                                    \
    Real arg(INPUT);                                   \
    Real ret(0.0);                                     \
    ArithmeticFunctions::TPL_FUNC(&ret, arg);          \
    EXPECT_FALSE(ret.is_null);                         \
    EXPECT_DOUBLE_EQ(C_FUNC(INPUT), ret.val);          \
  }

#define CHECK_FUNC(TPL_FUNC, C_FUNC, INPUT) \
  CHECK_HANDLES_NULL(TPL_FUNC, C_FUNC)      \
  CHECK_HANDLES_NONNULL(TPL_FUNC, C_FUNC, INPUT)

  CHECK_FUNC(Abs, std::fabs, -4.4);
  CHECK_FUNC(Abs, std::fabs, 1.10);

  CHECK_FUNC(Sqrt, std::sqrt, 4.0);
  CHECK_FUNC(Sqrt, std::sqrt, 1.0);

  CHECK_FUNC(Cbrt, std::cbrt, 4.0);
  CHECK_FUNC(Cbrt, std::cbrt, 1.0);

  CHECK_FUNC(Exp, std::exp, 4.0);
  CHECK_FUNC(Exp, std::exp, 1.0);

  CHECK_FUNC(Ceil, std::ceil, 4.4);
  CHECK_FUNC(Ceil, std::ceil, 1.2);
  CHECK_FUNC(Ceil, std::ceil, -100.1);
  CHECK_FUNC(Ceil, std::ceil, -100.34234);

  CHECK_FUNC(Floor, std::floor, 4.4);
  CHECK_FUNC(Floor, std::floor, 1.2);
  CHECK_FUNC(Floor, std::floor, 50.1);
  CHECK_FUNC(Floor, std::floor, 100.234);

  CHECK_FUNC(Ln, std::log, 4.4);
  CHECK_FUNC(Ln, std::log, 1.2);
  CHECK_FUNC(Ln, std::log, 50.1);
  CHECK_FUNC(Ln, std::log, 100.234);

  CHECK_FUNC(Log2, std::log2, 4.4);
  CHECK_FUNC(Log2, std::log2, 1.2);
  CHECK_FUNC(Log2, std::log2, 50.1);
  CHECK_FUNC(Log2, std::log2, 100.234);

  CHECK_FUNC(Log10, std::log10, 4.4);
  CHECK_FUNC(Log10, std::log10, 1.10);
  CHECK_FUNC(Log10, std::log10, 50.123);
  CHECK_FUNC(Log10, std::log10, 100.234);
}

}  // namespace tpl::sql::test