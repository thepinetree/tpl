#include "sql/table.h"

#include <iostream>
#include <limits>
#include <utility>

#include "sql/catalog.h"

// TODO(siva): Hack! Fix me!
i32 current_partition = -1;

namespace tpl::sql {

// ---------------------------------------------------------
// Table
// ---------------------------------------------------------

void Table::Insert(Block &&block) {
#ifndef NDEBUG
  // Sanity check
  TPL_ASSERT(block.num_cols() == schema_->num_columns(),
             "Column count mismatch");
  for (u32 i = 0; i < schema_->num_columns(); i++) {
    const auto &block_col_type = block.GetColumnData(i)->type();
    const auto &schema_col_type = schema().GetColumnInfo(i)->type;
    TPL_ASSERT(schema_col_type.Equals(block_col_type), "Column type mismatch");
  }
#endif

  num_tuples_ += block.num_tuples();
  blocks_.emplace_back(std::move(block));
}

namespace {

void DumpColValue(std::ostream &os, const Type &type, const ColumnSegment &col,
                  u32 row_idx) {
  switch (type.type_id()) {
    case TypeId::Boolean: {
      break;
    }
    case TypeId::SmallInt: {
      if (type.nullable() && col.IsNullAt(row_idx)) {
        os << "NULL";
      } else {
        os << col.TypedAccessAt<i16>(row_idx);
      }
      break;
    }
    case TypeId::Integer: {
      if (type.nullable() && col.IsNullAt(row_idx)) {
        os << "NULL";
      } else {
        os << col.TypedAccessAt<i32>(row_idx);
      }
      break;
    }
    case TypeId::BigInt: {
      if (type.nullable() && col.IsNullAt(row_idx)) {
        os << "NULL";
      } else {
        os << col.TypedAccessAt<i64>(row_idx);
      }
      break;
    }
    case TypeId::Real: {
      if (type.nullable() && col.IsNullAt(row_idx)) {
        os << "NULL";
      } else {
        os << col.TypedAccessAt<double>(row_idx);
      }
      break;
    }
    case TypeId::Decimal:
    case TypeId::Date:
    case TypeId::Char:
    case TypeId::Varchar: {
      break;
    }
  }
}

}  // namespace

void Table::Dump(std::ostream &os) const {
  const auto &cols_meta = schema().columns();
  for (const auto &block : blocks_) {
    for (u32 row_idx = 0; row_idx < block.num_tuples(); row_idx++) {
      for (u32 col_idx = 0; col_idx < cols_meta.size(); col_idx++) {
        if (col_idx != 0) {
          os << ", ";
        }
        const auto *col_vector = block.GetColumnData(col_idx);
        DumpColValue(os, cols_meta[col_idx].type, *col_vector, row_idx);
      }
      os << "\n";
    }
  }
}

// ---------------------------------------------------------
// Table Block Iterator
// ---------------------------------------------------------

TableBlockIterator::TableBlockIterator(u16 table_id)
    : TableBlockIterator(table_id, 0, std::numeric_limits<u32>::max()) {}

TableBlockIterator::TableBlockIterator(u16 table_id, u32 start_block_idx,
                                       u32 end_block_idx)
    : table_id_(table_id),
      start_block_idx_(start_block_idx),
      end_block_idx_(end_block_idx),
      table_(nullptr),
      curr_block_(nullptr) {}

bool TableBlockIterator::Init() {
  // Lookup the table
  const Catalog *catalog = Catalog::Instance();
  table_ = catalog->LookupTableById(static_cast<TableId>(table_id_));

  // If the table wasn't found, we didn't initialize
  if (table_ == nullptr) {
    return false;
  }

  // Check block range
  if (start_block_idx_ > table_->num_blocks()) {
    return false;
  }

  if (end_block_idx_ != std::numeric_limits<u32>::max()) {
    if (end_block_idx_ > table_->num_blocks()) {
      return false;
    }
  }

  if (start_block_idx_ >= end_block_idx_) {
    return false;
  }

  // Setup the block position boundaries
  curr_block_ = nullptr;
  pos_ = table_->begin() + start_block_idx_;
  end_ = (end_block_idx_ == std::numeric_limits<u32>::max()
              ? table_->end()
              : table_->begin() + end_block_idx_);
  return true;
}

bool TableBlockIterator::Advance() {
  if (pos_ == end_) {
    return false;
  }

  curr_block_ = &*pos_;
  ++pos_;
  return true;
}

}  // namespace tpl::sql
