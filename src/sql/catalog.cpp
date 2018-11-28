#include "sql/catalog.h"

#include <iostream>
#include <random>
#include <utility>

#include "logging/logger.h"
#include "sql/schema.h"
#include "sql/table.h"
#include "sql/type.h"

namespace tpl::sql {

namespace {

/**
 * Enumeration to characterize the distribution of values in a given column
 */
enum class Dist : u8 { Uniform, Zipf_50, Zipf_75, Zipf_95, Zipf_99, Serial };

/**
 * Metadata about the data for a given column. Specifically, the type of the
 * column, the distribution of values, a min and max if appropriate.
 */
struct ColumnInsertMeta {
  const char *name;
  Type type;
  Dist dist;
  u64 min;
  u64 max;

  ColumnInsertMeta(const char *name, const Type &type, Dist dist, u64 min,
                   u64 max)
      : name(name), type(type), dist(dist), min(min), max(max) {}
};

/**
 * Metadata about data within a table. Specifically, the schema and number of
 * rows in the table.
 */
struct TableInsertMeta {
  TableId id;
  const char *name;
  u32 num_rows;
  std::vector<ColumnInsertMeta> col_meta;

  TableInsertMeta(TableId id, const char *name, u32 num_rows,
                  std::vector<ColumnInsertMeta> col_meta)
      : id(id), name(name), num_rows(num_rows), col_meta(std::move(col_meta)) {}
};

/**
 * This array configures each of the test tables. When the catalog is created,
 * it bootstraps itself with the tables in this array. Each able is configured
 * with a name, size, and schema. We also configure the columns of the table. If
 * you add a new table, set it up here.
 */
// clang-format off
TableInsertMeta insert_meta[] = {
    // Table 1
    {TableId::Test1, "test_1", 2000000,
     {{"colA", Type(TypeId::Integer, false), Dist::Serial, 0, 0},
      {"colB", Type(TypeId::Integer, false), Dist::Uniform, 0, 9},
      {"colC", Type(TypeId::Integer, false), Dist::Uniform, 0, 9999},
      {"colD", Type(TypeId::Integer, false), Dist::Uniform, 0, 99999}}},
};
// clang-format on

template <typename T>
T *CreateNumberColumnData(Dist dist, u32 num_vals, u64 min, u64 max) {
  static u64 serial_counter = 0;
  T *val = static_cast<T *>(malloc(sizeof(T) * num_vals));

  switch (dist) {
    case Dist::Uniform: {
      std::mt19937 generator;
      std::uniform_int_distribution<T> distribution(min, max);

      for (u32 i = 0; i < num_vals; i++) {
        val[i] = distribution(generator);
      }

      break; 
    }
    case Dist::Serial: {
      for (u32 i = 0; i < num_vals; i++) {
        val[i] = serial_counter++;
      }
      break;
    }
    default:
      throw std::runtime_error("Implement me!");
  }

  return val;
}

std::pair<const byte *, const bool *> GenerateColumnData(
    const ColumnInsertMeta &col_meta, u32 num_vals) {
  // Create data
  byte *col_data = nullptr;
  switch (col_meta.type.type_id()) {
    case TypeId::Boolean: {
      throw std::runtime_error("Implement me!");
    }
    case TypeId::SmallInt: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<i16>(col_meta.dist, num_vals, col_meta.min,
              col_meta.max));
      break;
    }
    case TypeId::Integer: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<i32>(col_meta.dist, num_vals, col_meta.min,
              col_meta.max));
      break;
    }
    case TypeId::BigInt:
    case TypeId::Decimal: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<i64>(col_meta.dist, num_vals, col_meta.min,
              col_meta.max));
      break;
    }
    case TypeId::Varchar: {
      throw std::runtime_error("Implement me!");
    }
  }

  // Create bitmap
  bool *null_bitmap = nullptr;
  if (col_meta.type.nullable()) {
    null_bitmap = static_cast<bool *>(malloc(sizeof(bool)));
  }

  return {col_data, null_bitmap};
}

void InitTable(const TableInsertMeta &table_meta, Table *table) {
  LOG_INFO("Populating table instance '{}' with {} rows", table_meta.name,
           table_meta.num_rows);

  u32 batch_size = 10000;
  u32 num_batches = table_meta.num_rows / batch_size +
                    (table_meta.num_rows % batch_size != 0);

  for (u32 i = 0; i < num_batches; i++) {
    std::vector<Table::ColumnVector> columns;

    // Generate column data for all columns
    u32 size = std::min(batch_size, table_meta.num_rows - (i * batch_size));
    for (const auto &col_meta : table_meta.col_meta) {
      const auto &[data, null_bitmap] = GenerateColumnData(col_meta, size);
      columns.emplace_back(data, null_bitmap);
    }

    // Insert into table
    table->BulkInsert(std::move(columns), size);
  }
}

}  // namespace

/*
 * Create a catalog, setting up all tables.
 */
Catalog::Catalog() {
  LOG_INFO("Initializing catalog");

  // Insert tables into catalog
  for (const auto &meta : insert_meta) {
    LOG_INFO("Creating table instance '{}' in catalog", meta.name);

    std::vector<Schema::ColInfo> cols;
    for (const auto &col_meta : meta.col_meta) {
      cols.emplace_back(
          Schema::ColInfo{.name = col_meta.name, .type = col_meta.type});
    }

    // Insert into catalog
    table_catalog_[TableId::Test1] = std::make_unique<Table>(
        static_cast<u16>(TableId::Test1), Schema(std::move(cols)));
  }

  // Populate all tables
  for (const auto &table_meta : insert_meta) {
    InitTable(table_meta, LookupTableById(table_meta.id));
  }

  LOG_INFO("Catalog initialization complete");
}

Catalog::~Catalog() = default;

Table *Catalog::LookupTableByName(const std::string &name) {
  static std::unordered_map<std::string, TableId> kTableNameMap = {
#define ENTRY(Name, Str, ...) {Str, TableId::Name},
      TABLES(ENTRY)
#undef ENTRY
  };

  auto iter = kTableNameMap.find(name);
  if (iter == kTableNameMap.end()) {
    return nullptr;
  }

  return LookupTableById(iter->second);
}

Table *Catalog::LookupTableByName(const ast::Identifier name) {
  return LookupTableByName(name.data());
}

Table *Catalog::LookupTableById(TableId table_id) {
  auto iter = table_catalog_.find(table_id);
  return (iter == table_catalog_.end() ? nullptr : iter->second.get());
}

}  // namespace tpl::sql