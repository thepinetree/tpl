#include "sql/catalog.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "logging/logger.h"
#include "sql/data_types.h"
#include "sql/schema.h"
#include "sql/table.h"
#include "util/memory.h"

namespace tpl::sql {

namespace {

/**
 * Enumeration to characterize the distribution of values in a given column
 */
enum class Dist : uint8_t { Uniform, Zipf_50, Zipf_75, Zipf_95, Zipf_99, Serial };

/**
 * Metadata about the data for a given column. Specifically, the type of the
 * column, the distribution of values, a min and max if appropriate.
 */
struct ColumnInsertMeta {
  const char *name;
  const SqlType &sql_type;
  Dist dist;
  std::variant<int64_t, double> min;
  std::variant<int64_t, double> max;

  ColumnInsertMeta(const char *name, const SqlType &sql_type, Dist dist,
                   std::variant<int64_t, double> min, std::variant<int64_t, double> max)
      : name(name), sql_type(sql_type), dist(dist), min(min), max(max) {}
};

/**
 * Metadata about data within a table. Specifically, the schema and number of
 * rows in the table.
 */
struct TableInsertMeta {
  uint16_t id;
  const char *name;
  uint32_t num_rows;
  std::vector<ColumnInsertMeta> col_meta;

  TableInsertMeta(uint16_t id, const char *name, uint32_t num_rows,
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
    // The empty table
    {static_cast<uint16_t>(TableId::EmptyTable), "empty_table", 0,
     {{"colA", sql::IntegerType::Instance(false), Dist::Serial, int64_t{0}, int64_t{0}}}},

    // Table 1
    {static_cast<uint16_t>(TableId::Test1), "test_1", 2000000,
     {{"colA", sql::IntegerType::Instance(false), Dist::Serial, int64_t{0}, int64_t{0}},
      {"colB", sql::IntegerType::Instance(false), Dist::Uniform, int64_t{0}, int64_t{9}},
      {"colC", sql::IntegerType::Instance(false), Dist::Uniform, int64_t{0}, int64_t{9999}},
      {"colD", sql::IntegerType::Instance(false), Dist::Uniform, int64_t{0}, int64_t{99999}}}},
};
// clang-format on

template <class T>
auto GetRandomDistribution(T min, T max)
    -> std::enable_if_t<std::is_integral_v<T>, std::uniform_int_distribution<T>> {
  return std::uniform_int_distribution<T>(min, max);
}

template <class T>
auto GetRandomDistribution(T min, T max)
    -> std::enable_if_t<std::is_floating_point_v<T>, std::uniform_real_distribution<T>> {
  return std::uniform_real_distribution<T>(min, max);
}

template <typename T>
T *CreateNumberColumnData(Dist dist, uint32_t num_vals, T min, T max) {
  static T serial_counter = 0;
  auto *val = static_cast<T *>(util::MallocAligned(sizeof(T) * num_vals, CACHELINE_SIZE));

  switch (dist) {
    case Dist::Uniform: {
      auto generator = std::mt19937{};
      auto distribution = GetRandomDistribution(min, max);

      for (uint32_t i = 0; i < num_vals; i++) {
        val[i] = distribution(generator);
      }

      break;
    }
    case Dist::Serial: {
      for (uint32_t i = 0; i < num_vals; i++) {
        val[i] = serial_counter++;
      }
      break;
    }
    default:
      throw std::runtime_error("Implement me!");
  }

  return val;
}

std::pair<byte *, uint32_t *> GenerateColumnData(const ColumnInsertMeta &col_meta,
                                                 uint32_t num_rows) {
  // Create data
  byte *col_data = nullptr;
  switch (col_meta.sql_type.id()) {
    case SqlTypeId::TinyInt: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<int8_t>(col_meta.dist, num_rows, std::get<int64_t>(col_meta.min),
                                         std::get<int64_t>(col_meta.max)));
      break;
    }
    case SqlTypeId::SmallInt: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<int16_t>(col_meta.dist, num_rows, std::get<int64_t>(col_meta.min),
                                          std::get<int64_t>(col_meta.max)));
      break;
    }
    case SqlTypeId::Integer: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<int32_t>(col_meta.dist, num_rows, std::get<int64_t>(col_meta.min),
                                          std::get<int64_t>(col_meta.max)));
      break;
    }
    case SqlTypeId::BigInt:
    case SqlTypeId::Decimal: {
      col_data = reinterpret_cast<byte *>(
          CreateNumberColumnData<int64_t>(col_meta.dist, num_rows, std::get<int64_t>(col_meta.min),
                                          std::get<int64_t>(col_meta.max)));
      break;
    }
    case SqlTypeId::Real: {
      col_data = reinterpret_cast<byte *>(CreateNumberColumnData<float>(
          col_meta.dist, num_rows, std::get<double>(col_meta.min), std::get<double>(col_meta.max)));
      break;
    }
    case SqlTypeId::Double: {
      col_data = reinterpret_cast<byte *>(CreateNumberColumnData<double>(
          col_meta.dist, num_rows, std::get<double>(col_meta.min), std::get<double>(col_meta.max)));
      break;
    }
    case SqlTypeId::Boolean:
    case SqlTypeId::Date:
    case SqlTypeId::Char:
    case SqlTypeId::Varchar: {
      throw std::runtime_error("Implement me!");
    }
  }

  // Create bitmap
  uint32_t *null_bitmap = nullptr;
  if (col_meta.sql_type.nullable()) {
    TPL_ASSERT(num_rows != 0, "Cannot have 0 rows.");
    uint64_t num_words = util::BitUtil::Num32BitWordsFor(num_rows);
    null_bitmap =
        static_cast<uint32_t *>(util::MallocAligned(num_words * sizeof(uint32_t), CACHELINE_SIZE));
  }

  return {col_data, null_bitmap};
}

void InitTable(const TableInsertMeta &table_meta, Table *table) {
  LOG_INFO("Populating table instance '{}' with {} rows", table_meta.name, table_meta.num_rows);

  uint32_t batch_size = 10000;
  uint32_t num_batches = table_meta.num_rows / batch_size +
                         static_cast<uint32_t>(table_meta.num_rows % batch_size != 0);

  for (uint32_t i = 0; i < num_batches; i++) {
    std::vector<ColumnSegment> columns;

    // Generate column data for all columns
    uint32_t num_vals = std::min(batch_size, table_meta.num_rows - (i * batch_size));
    TPL_ASSERT(num_vals != 0, "Can't have empty columns.");
    for (const auto &col_meta : table_meta.col_meta) {
      auto [data, null_bitmap] = GenerateColumnData(col_meta, num_vals);
      // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
      columns.emplace_back(col_meta.sql_type, data, null_bitmap, num_vals);
    }

    // Insert into table
    table->Insert(Table::Block(std::move(columns), num_vals));
  }
}

}  // namespace

/*
 * Create a catalog, setting up all tables.
 */
Catalog::Catalog(): next_table_id_(static_cast<uint16_t>(TableId::Last)) {
  LOG_INFO("Initializing catalog");

  // Insert tables into catalog
  for (const auto &meta : insert_meta) {
    LOG_INFO("Creating table instance '{}' in catalog", meta.name);

    std::vector<Schema::ColumnInfo> cols;
    for (const auto &col_meta : meta.col_meta) {
      cols.emplace_back(col_meta.name, col_meta.sql_type);
    }

    // Insert into catalog
    table_catalog_[meta.id] = std::make_unique<Table>(static_cast<uint16_t>(meta.id),
                                                      std::make_unique<Schema>(std::move(cols)));
    table_name_to_id_map_[meta.name] = meta.id;
  }

  // Populate all tables
  for (const auto &table_meta : insert_meta) {
    InitTable(table_meta, LookupTableById(table_meta.id));
  }

  LOG_INFO("Catalog initialization complete");
}

// We need this here because a catalog has a map that stores unique pointers to
// SQL Table objects. SQL Table is forward-declared in the header file, so the
// destructor cannot be inlined in the header.
Catalog::~Catalog() = default;

Catalog *Catalog::Instance() {
  static Catalog kInstance;
  return &kInstance;
}

Table *Catalog::LookupTableByName(const std::string &name) const {
  auto iter = table_name_to_id_map_.find(name);
  return iter == table_name_to_id_map_.end() ? nullptr : LookupTableById(iter->second);
}

Table *Catalog::LookupTableByName(const ast::Identifier name) const {
  return LookupTableByName(name.data());
}

Table *Catalog::LookupTableById(uint16_t table_id) const {
  auto iter = table_catalog_.find(table_id);
  return (iter == table_catalog_.end() ? nullptr : iter->second.get());
}

void Catalog::InsertTable(const std::string &table_name, std::unique_ptr<Table> &&table) {
  const uint16_t table_id = table->id();
  table_catalog_.emplace(table_id, std::move(table));
  table_name_to_id_map_.insert(std::make_pair(table_name, table_id));
}

}  // namespace tpl::sql
