#pragma once

#include "util/common.h"

namespace tpl::sql {

/// A generic base catch-all SQL value
struct Val {
  bool is_null;

  explicit Val(bool is_null = false) noexcept : is_null(is_null) {}
};

/// A SQL boolean value
struct BoolVal : public Val {
  bool val;

  explicit BoolVal(bool val) noexcept : Val(false), val(val) {}

  /// Convert this SQL boolean into a primitive boolean. Thanks to SQL's
  /// three-valued logic, we implement the following truth table:
  ///
  ///   Value | NULL? | Output
  /// +-------+-------+--------+
  /// | false | false | false  |
  /// | false | true  | false  |
  /// | true  | false | true   |
  /// | true  | true  | false  |
  /// +-------+-------+--------+
  ///
  bool ForceTruth() const noexcept { return !is_null && val; }

  /// Return a NULL boolean value
  static BoolVal Null() {
    BoolVal val(false);
    val.is_null = true;
    return val;
  }
};

/// An integral SQL value
struct Integer : public Val {
  i64 val;

  explicit Integer(i64 val) noexcept : Val(false), val(val) {}

  /// Return a NULL integer value
  static Integer Null() {
    Integer val(0);
    val.is_null = true;
    return val;
  }
};

/// A decimal SQL value
struct Decimal : public Val {
  u64 val;
  u32 precision;
  u32 scale;

  Decimal(u64 val, u32 precision, u32 scale) noexcept
      : Val(false), val(val), precision(precision), scale(scale) {}

  /// Return a NULL decimal value
  static Decimal Null() {
    Decimal val(0, 0, 0);
    val.is_null = true;
    return val;
  }
};

/// A SQL string
struct String : public Val {
  u8 *str;
  u32 len;

  String(u8 *str, u32 len) noexcept : Val(str == nullptr), str(str), len(len) {}

  /// Return a NULL varchar/string
  static String Null() { return String(nullptr, 0); }
};

}  // namespace tpl::sql
