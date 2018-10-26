#pragma once

#include <string>
#include <vector>

#include "sql/type.h"

namespace tpl::sql {

/**
 * A class to capture the physical schema layout
 */
class Schema {
 public:
  struct ColInfo {
    std::string name;
    Type type;
  };

  explicit Schema(std::vector<ColInfo> cols) : cols_(std::move(cols)) {}
  Schema(std::initializer_list<ColInfo> cols) : cols_(cols) {}

  const std::vector<ColInfo> columns() const { return cols_; }

 private:
  std::vector<ColInfo> cols_;
};

}  // namespace tpl::sql