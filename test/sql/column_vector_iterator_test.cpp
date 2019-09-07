#include "sql_test.h"  // NOLINT

#include "sql/catalog.h"
#include "sql/column_vector_iterator.h"
#include "sql/table.h"

namespace tpl::sql {

class ColumnIteratorTest : public SqlBasedTest {};

TEST_F(ColumnIteratorTest, EmptyIteratorTest) {
  auto *table = sql::Catalog::Instance()->LookupTableById(TableId::Test1);

  const auto col_idx = 0;
  const auto col_info = table->schema().GetColumnInfo(col_idx);

  //
  // Test 1: Check to see that iteration doesn't begin without an input block
  //

  {
    for (ColumnVectorIterator iter(col_info); iter.Advance();) {
      FAIL() << "Iteration began on uninitialized iterator";
    }
  }

  //
  // Test 2: Check that iteration begins and completes with an input block
  //

  {
    const auto *col = table->GetBlock(0)->GetColumnData(0);

    ColumnVectorIterator iter(col_info);
    iter.Reset(col);

    uint32_t num_rows = 0;
    for (bool has_more = true; has_more; has_more = iter.Advance()) {
      num_rows += iter.NumTuples();
    }

    EXPECT_GT(col->num_tuples(), 0u);
    EXPECT_EQ(col->num_tuples(), num_rows);
  }
}

TEST_F(ColumnIteratorTest, IntegerIterationTest) {
  auto *table = sql::Catalog::Instance()->LookupTableById(TableId::Test1);

  const uint32_t col_idx = 0;
  const auto col_info = table->schema().GetColumnInfo(col_idx);

  //
  // This is a simple test. We iterate over a single block of the Test1 table
  // ensuring that the first column is stored in ascending order. This makes an
  // assumption on the test table, so we put an assertion before beginning the
  // test.
  //

  ASSERT_TRUE(col_info->sql_type.id() == SqlTypeId::Integer);

  const auto *col = table->GetBlock(0)->GetColumnData(0);

  ColumnVectorIterator iter(col_info);
  iter.Reset(col);

  uint32_t num_rows = 0;

  for (bool has_more = true; has_more; has_more = iter.Advance()) {
    auto *col_data = reinterpret_cast<int32_t *>(iter.col_data());
    for (uint32_t i = 1; i < iter.NumTuples(); i++) {
      EXPECT_LT(col_data[i - 1], col_data[i]);
      EXPECT_EQ(col_data[i - 1] + 1, col_data[i]);
    }
    num_rows += iter.NumTuples();
  }

  EXPECT_GT(col->num_tuples(), 0u);
  EXPECT_EQ(col->num_tuples(), num_rows);
}

}  // namespace tpl::sql
