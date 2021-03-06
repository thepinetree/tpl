#pragma once

#include <limits>

#include "common/common.h"

namespace tpl::util {

/**
 * Utility class to handle arithmetic operations that can overflow.
 */
class ArithmeticOverflow : public AllStatic {
 public:
  static constexpr int128_t kMinInt128 = std::numeric_limits<int128_t>::min();
  static constexpr int128_t kMaxInt128 = std::numeric_limits<int128_t>::max();

  /**
   * Add two integral values and store their result in @em res. Return true if
   * the addition overflowed.
   * @tparam T The types of the input and output.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  template <typename T>
  static bool Add(const T a, const T b, T *res) {
    return __builtin_add_overflow(a, b, res);
  }

  /**
   * Add two signed 32-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const int32_t a, const int32_t b, int32_t *res) {
    return __builtin_sadd_overflow(a, b, res);
  }

  /**
   * Add two signed 64-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const int64_t a, const int64_t b, int64_t *res) {
    return __builtin_saddl_overflow(a, b, res);
  }

  /**
   * Add two signed 128-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const int128_t a, const int128_t b, int128_t *res) {
    *res = a + b;
    return (b > 0 && a > kMaxInt128 - b) || (b < 0 && a < kMinInt128 - b);
  }

  /**
   * Add two unsigned 32-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const uint32_t a, const uint32_t b, uint32_t *res) {
    return __builtin_uadd_overflow(a, b, res);
  }

  /**
   * Add two unsigned 64-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const uint64_t a, const uint64_t b, uint64_t *res) {
    return __builtin_uaddl_overflow(a, b, res);
  }

  /**
   * Add two unsigned 64-bit integer values and store their result in @em res.
   * Return true if the addition overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the addition is written to.
   * @return True if the addition overflowed; false otherwise.
   */
  static bool Add(const uint128_t a, const uint128_t b, uint128_t *res) {
    *res = a + b;
    return (a > kMaxInt128 - b);
  }

  /**
   * Subtract two integer values and store their result in @em res. Return true
   * if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  template <typename T>
  static bool Sub(const T a, const T b, T *res) {
    return __builtin_sub_overflow(a, b, res);
  }

  /**
   * Subtract two signed 32-bit values and store their result in @em res. Return
   * true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const int32_t a, const int32_t b, int32_t *res) {
    return __builtin_ssub_overflow(a, b, res);
  }

  /**
   * Subtract two signed 64-bit values and store their result in @em res. Return
   * true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const int64_t a, const int64_t b, int64_t *res) {
    return __builtin_ssubl_overflow(a, b, res);
  }

  /**
   * Subtract two signed 128-bit values and store their result in @em res.
   * Return true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const int128_t a, const int128_t b, int128_t *res) {
    *res = a - b;
    return (b > 0 && a < kMinInt128 + b) || (b < 0 && a > kMaxInt128 + b);
  }

  /**
   * Subtract two unsigned 32-bit values and store their result in @em res.
   * Return true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const uint32_t a, const uint32_t b, uint32_t *res) {
    return __builtin_usub_overflow(a, b, res);
  }

  /**
   * Subtract two unsigned 64-bit values and store their result in @em res.
   * Return true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const uint64_t a, const uint64_t b, uint64_t *res) {
    return __builtin_usubl_overflow(a, b, res);
  }

  /**
   * Subtract two unsigned 128-bit values and store their result in @em res.
   * Return true if the subtraction overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the subtraction is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Sub(const uint128_t a, const uint128_t b, uint128_t *res) {
    *res = a + b;
    return (a < kMinInt128 + b);
  }

  /**
   * Multiply two integer values and store their result in @em res. Return true
   * if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  template <typename T>
  static bool Mul(const T a, const T b, T *res) {
    return __builtin_mul_overflow(a, b, res);
  }

  /**
   * Multiply two signed 32-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const int32_t a, const int32_t b, int32_t *res) {
    return __builtin_smul_overflow(a, b, res);
  }

  /**
   * Multiply two signed 64-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const int64_t a, const int64_t b, int64_t *res) {
    return __builtin_smull_overflow(a, b, res);
  }

  /**
   * Multiply two signed 128-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const int128_t a, const int128_t b, int128_t *res) {
    *res = static_cast<const uint128_t>(a) * static_cast<const uint128_t>(b);
    if (a == 0 || b == 0) {
      return false;
    }

    return (a * b) / b != a;
  }

  /**
   * Multiply two unsigned 32-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const uint32_t a, const uint32_t b, uint32_t *res) {
    return __builtin_umul_overflow(a, b, res);
  }

  /**
   * Multiply two unsigned 64-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const uint64_t a, const uint64_t b, uint64_t *res) {
    return __builtin_umull_overflow(a, b, res);
  }

  /**
   * Multiply two unsigned 128-bit integer values and store their result in @em
   * res. Return true if the multiplication overflowed.
   * @param a The first operand.
   * @param b The second operand.
   * @param[out] res Where the result of the multiplication is written to.
   * @return True if the subtraction overflowed; false otherwise.
   */
  static bool Mul(const uint128_t a, const uint128_t b, uint128_t *res) {
    *res = a * b;
    return (a * b) / b != a;
  }
};

}  // namespace tpl::util
