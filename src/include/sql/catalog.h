#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/identifier.h"
#include "common/common.h"

namespace tpl::sql {

class Table;

#define TABLES(V)              \
  V(EmptyTable, "empty_table") \
  V(Test1, "test_1")           \
  V(Test2, "test_2")           \
  V(AllTypes, "all_types")     \
  V(Small1, "small_1")

enum class TableId : uint16_t {
#define ENTRY(Id, ...) Id,
  TABLES(ENTRY)
#undef ENTRY
#define COUNT_OP(inst, ...) +1
      Last = TABLES(COUNT_OP)
#undef COUNT_OP
};

/**
 * A catalog of all the databases and tables in the system. Only one exists
 * per TPL process, and its instance can be acquired through
 * @em Catalog::instance(). At this time, the catalog is read-only after
 * startup. If you want to add a new table, modify the \ref TABLES macro which
 * lists the IDs of all tables, and configure your table in the Catalog
 * initialization function.
 *
 * @see Catalog::Instance() - Method to retrieve Catalog instance
 */
class Catalog {
 public:
  /**
   * Destructor
   */
  ~Catalog();

  /**
   * Access singleton Catalog object. Singletons are bad blah blah blah ...
   * @return The singleton Catalog object
   */
  static Catalog *Instance();

  /**
   * Lookup a table in this catalog by name.
   * @param name The name of the target table
   * @return A pointer to the table, or NULL if the table doesn't exist.
   */
  Table *LookupTableByName(const std::string &name) const;

  /**
   * Lookup a table in this catalog by name, using an identifier.
   * @param name The name of the target table
   * @return A pointer to the table, or NULL if the table doesn't exist.
   */
  Table *LookupTableByName(ast::Identifier name) const;

  /**
   * Lookup a table in this catalog by ID.
   * @param table_id The ID of the target table
   * @return A pointer to the table, or NULL if the table doesn't exist.
   */
  Table *LookupTableById(uint16_t table_id) const;

  /**
   * Allocate a table ID
   */
  uint16_t AllocateTableId() { return next_table_id_++; }

  /**
   * Insert the table into the catalog.
   * @param table_name
   * @param table
   */
  void InsertTable(const std::string &table_name, std::unique_ptr<Table> &&table);

 private:
  /**
   * Private on purpose to force access through singleton Instance() method.
   * @see Catalog::Instance()
   */
  Catalog();

 private:
  std::unordered_map<uint16_t, std::unique_ptr<Table>> table_catalog_;
  std::unordered_map<std::string, uint16_t> table_name_to_id_map_;
  uint16_t next_table_id_;
};

}  // namespace tpl::sql
