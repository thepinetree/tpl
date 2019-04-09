#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "tpl_test.h"  // NOLINT

#include "sql/catalog.h"
#include "sql/vector_projection_iterator.h"

namespace tpl::sql::test {

///
/// This test uses a vector projection with four columns. The first column,
/// named "col_a" is a non-nullable small integer column whose values are
/// monotonically increasing. The second column, "col_b", is a nullable integer
/// column whose values are random. The third column, "col_c", is a non-nullable
/// integer column whose values are in the range [0, 1000). The last column,
/// "col_d", is a nullable big-integer column whose values are random.
///

namespace {

template <typename T>
std::unique_ptr<byte[]> CreateMonotonicallyIncreasing(u32 num_elems) {
  auto input = std::make_unique<byte[]>(num_elems * sizeof(T));
  auto *typed_input = reinterpret_cast<T *>(input.get());
  std::iota(typed_input, &typed_input[num_elems], static_cast<T>(0));
  return input;
}

template <typename T>
std::unique_ptr<byte[]> CreateRandom(u32 num_elems, T min = 0,
                                     T max = std::numeric_limits<T>::max()) {
  auto input = std::make_unique<byte[]>(num_elems * sizeof(T));

  std::mt19937 generator;
  std::uniform_int_distribution<T> distribution(min, max);

  auto *typed_input = reinterpret_cast<T *>(input.get());
  for (u32 i = 0; i < num_elems; i++) {
    typed_input[i] = distribution(generator);
  }

  return input;
}

std::pair<std::unique_ptr<u32[]>, u32> CreateRandomNullBitmap(u32 num_elems) {
  auto input =
      std::make_unique<u32[]>(util::BitUtil::Num32BitWordsFor(num_elems));
  auto num_nulls = 0;

  std::mt19937 generator;
  std::uniform_int_distribution<u32> distribution(0, 10);

  for (u32 i = 0; i < num_elems; i++) {
    if (distribution(generator) < 5) {
      util::BitUtil::Flip(input.get(), i);
      num_nulls++;
    }
  }

  return {std::move(input), num_nulls};
}

}  // namespace

class VectorProjectionIteratorTest : public TplTest {
 protected:
  enum ColId : u8 { col_a = 0, col_b = 1, col_c = 2, col_d = 3 };

  struct ColData {
    std::unique_ptr<byte[]> data;
    std::unique_ptr<u32[]> nulls;
    u32 num_nulls;
    u32 num_tuples;

    ColData(std::unique_ptr<byte[]> data, std::unique_ptr<u32[]> nulls,
            u32 num_nulls, u32 num_tuples)
        : data(std::move(data)),
          nulls(std::move(nulls)),
          num_nulls(num_nulls),
          num_tuples(num_tuples) {}
  };

 public:
  VectorProjectionIteratorTest() : num_tuples_(kDefaultVectorSize) {
    // Create the schema
    std::vector<Schema::ColumnInfo> cols = {
        Schema::ColumnInfo("col_a", SmallIntType::InstanceNonNullable()),
        Schema::ColumnInfo("col_b", IntegerType::InstanceNullable()),
        Schema::ColumnInfo("col_c", IntegerType::InstanceNonNullable()),
        Schema::ColumnInfo("col_b", BigIntType::InstanceNullable())};
    schema_ = std::make_unique<Schema>(std::move(cols));

    std::vector<const Schema::ColumnInfo *> vp_cols_info = {
        schema_->GetColumnInfo(0), schema_->GetColumnInfo(1),
        schema_->GetColumnInfo(2), schema_->GetColumnInfo(3)};
    vp_ = std::make_unique<VectorProjection>(vp_cols_info, num_tuples());

    // Load the data
    LoadData();
  }

  void LoadData() {
    auto cola_data = CreateMonotonicallyIncreasing<i16>(num_tuples());
    auto colb_data = CreateRandom<i32>(num_tuples());
    auto colb_null = CreateRandomNullBitmap(num_tuples());
    auto colc_data = CreateRandom<i32>(num_tuples(), 0, 1000);
    auto cold_data = CreateRandom<i64>(num_tuples());
    auto cold_null = CreateRandomNullBitmap(num_tuples());

    data_.emplace_back(std::move(cola_data), nullptr, 0, num_tuples());
    data_.emplace_back(std::move(colb_data), std::move(colb_null.first),
                       colb_null.second, num_tuples());
    data_.emplace_back(std::move(colc_data), nullptr, 0, num_tuples());
    data_.emplace_back(std::move(cold_data), std::move(cold_null.first),
                       cold_null.second, num_tuples());

    for (u32 col_idx = 0; col_idx < data_.size(); col_idx++) {
      auto &[data, nulls, num_nulls, num_tuples] = data_[col_idx];
      (void)num_nulls;
      vp_->ResetFromRaw(data.get(), nulls.get(), col_idx, num_tuples);
    }
  }

 protected:
  u32 num_tuples() const { return num_tuples_; }

  VectorProjection *vp() { return vp_.get(); }

  const ColData &column_data(u32 col_idx) const { return data_[col_idx]; }

 private:
  u32 num_tuples_;
  std::unique_ptr<Schema> schema_;
  std::unique_ptr<VectorProjection> vp_;
  std::vector<ColData> data_;
};

TEST_F(VectorProjectionIteratorTest, EmptyIteratorTest) {
  //
  // Check to see that iteration doesn't begin without an input block
  //

  std::vector<const Schema::ColumnInfo *> vp_col_info;
  VectorProjection empty_vecproj(vp_col_info, 0);
  VectorProjectionIterator iter;
  iter.SetVectorProjection(&empty_vecproj);

  for (; iter.HasNext(); iter.Advance()) {
    FAIL() << "Should not iterate with empty vector projection!";
  }
}

TEST_F(VectorProjectionIteratorTest, SimpleIteratorTest) {
  //
  // Check to see that iteration iterates over all tuples in the projection
  //

  {
    u32 tuple_count = 0;

    VectorProjectionIterator iter;
    iter.SetVectorProjection(vp());

    for (; iter.HasNext(); iter.Advance()) {
      tuple_count++;
    }

    EXPECT_EQ(vp()->total_tuple_count(), tuple_count);
    EXPECT_FALSE(iter.IsFiltered());
  }

  //
  // Check to see that column A is monotonically increasing
  //

  {
    VectorProjectionIterator iter;
    iter.SetVectorProjection(vp());

    bool entered = false;
    for (i16 last = -1; iter.HasNext(); iter.Advance()) {
      entered = true;
      auto *ptr = iter.Get<i16, false>(ColId::col_a, nullptr);
      EXPECT_NE(nullptr, ptr);
      if (last != -1) {
        EXPECT_LE(last, *ptr);
      }
      last = *ptr;
    }

    EXPECT_TRUE(entered);
    EXPECT_FALSE(iter.IsFiltered());
  }
}

TEST_F(VectorProjectionIteratorTest, ReadNullableColumnsTest) {
  //
  // Check to see that we can correctly count all NULL values in NULLable cols
  //

  VectorProjectionIterator iter;
  iter.SetVectorProjection(vp());

  u32 num_nulls = 0;
  for (; iter.HasNext(); iter.Advance()) {
    bool null = false;
    auto *ptr = iter.Get<i32, true>(ColId::col_b, &null);
    EXPECT_NE(nullptr, ptr);
    num_nulls += static_cast<u32>(null);
  }

  EXPECT_EQ(column_data(ColId::col_b).num_nulls, num_nulls);
}

TEST_F(VectorProjectionIteratorTest, ManualFilterTest) {
  //
  // Check to see that we can correctly manually apply a single filter on a
  // single column. We apply the filter IS_NOT_NULL(col_b)
  //

  {
    VectorProjectionIterator iter;
    iter.SetVectorProjection(vp());

    for (; iter.HasNext(); iter.Advance()) {
      bool null = false;
      iter.Get<i32, true>(ColId::col_b, &null);
      iter.Match(!null);
    }

    iter.ResetFiltered();

    // Now all selected/active elements must not be null
    u32 num_non_null = 0;
    for (; iter.HasNextFiltered(); iter.AdvanceFiltered()) {
      bool null = false;
      iter.Get<i32, true>(ColId::col_b, &null);
      EXPECT_FALSE(null);
      num_non_null++;
    }

    const auto &col_data = column_data(ColId::col_b);
    u32 actual_non_null = col_data.num_tuples - col_data.num_nulls;
    EXPECT_EQ(actual_non_null, num_non_null);
    EXPECT_EQ(actual_non_null, iter.num_selected());
  }

  //
  // Now try to apply filters individually on separate columns. Let's try:
  //
  // WHERE col_a < 100 and IS_NULL(col_b)
  //
  // The first filter (col_a < 100) should return 100 rows since it's a
  // monotonically increasing column. The second filter returns a
  // non-deterministic number of tuples, but it should be less than or equal to
  // 100. We'll do a manual check to be sure.
  //

  {
    VectorProjectionIterator iter;
    iter.SetVectorProjection(vp());

    for (; iter.HasNext(); iter.Advance()) {
      auto *val = iter.Get<i16, false>(ColId::col_a, nullptr);
      iter.Match(*val < 100);
    }

    iter.ResetFiltered();

    for (; iter.HasNextFiltered(); iter.AdvanceFiltered()) {
      bool null = false;
      iter.Get<i32, true>(ColId::col_b, &null);
      iter.Match(null);
    }

    iter.ResetFiltered();

    EXPECT_LE(iter.num_selected(), 100u);

    for (; iter.HasNextFiltered(); iter.AdvanceFiltered()) {
      // col_a must be less than 100
      {
        auto *val = iter.Get<i16, false>(ColId::col_a, nullptr);
        EXPECT_LT(*val, 100);
      }

      // col_b must be NULL
      {
        bool null = false;
        iter.Get<i32, true>(ColId::col_b, &null);
        EXPECT_TRUE(null);
      }
    }
  }
}

TEST_F(VectorProjectionIteratorTest, ManagedFilterTest) {
  //
  // Check to see that we can correctly apply a single filter on a single
  // column using VPI's managed filter. We apply the filter IS_NOT_NULL(col_b)
  //

  VectorProjectionIterator iter;
  iter.SetVectorProjection(vp());

  iter.RunFilter([&iter]() {
    bool null = false;
    iter.Get<i32, true>(ColId::col_b, &null);
    return !null;
  });

  const auto &col_data = column_data(ColId::col_b);
  u32 actual_non_null = col_data.num_tuples - col_data.num_nulls;
  EXPECT_EQ(actual_non_null, iter.num_selected());

  //
  // Ensure subsequent iterations only work on selected items
  //
  {
    u32 c = 0;
    iter.ForEach([&iter, &c]() {
      c++;
      bool null = false;
      iter.Get<i32, true>(ColId::col_b, &null);
      EXPECT_FALSE(null);
    });

    EXPECT_EQ(actual_non_null, iter.num_selected());
    EXPECT_EQ(iter.num_selected(), c);
  }
}

TEST_F(VectorProjectionIteratorTest, SimpleVectorizedFilterTest) {
  //
  // Check to see that we can correctly apply a single vectorized filter. Here
  // we just check col_c < 100
  //

  VectorProjectionIterator iter;
  iter.SetVectorProjection(vp());

  // Compute expected result
  u32 expected = 0;
  for (; iter.HasNext(); iter.Advance()) {
    auto val = *iter.Get<i32, false>(ColId::col_c, nullptr);
    if (val < 100) {
      expected++;
    }
  }

  // Filter
  iter.FilterColByVal<i32, std::less>(ColId::col_c, 100);

  // Check
  u32 count = 0;
  for (; iter.HasNextFiltered(); iter.AdvanceFiltered()) {
    auto val = *iter.Get<i32, false>(ColId::col_c, nullptr);
    EXPECT_LT(val, 100);
    count++;
  }

  EXPECT_EQ(expected, count);
}

TEST_F(VectorProjectionIteratorTest, MultipleVectorizedFilterTest) {
  //
  // Apply two filters in order:
  //  - col_c < 750
  //  - col_a < 10
  //
  // The first filter will return a non-deterministic result because it is a
  // randomly generated column. But, the second filter will return at most 10
  // results because it is a monotonically increasing column beginning at 0.
  //

  VectorProjectionIterator iter;
  iter.SetVectorProjection(vp());

  iter.FilterColByVal<i32, std::less>(ColId::col_c, 750);
  iter.FilterColByVal<i16, std::less>(ColId::col_a, 10);

  // Check
  u32 count = 0;
  for (; iter.HasNextFiltered(); iter.AdvanceFiltered()) {
    auto col_a_val = *iter.Get<i16, false>(ColId::col_a, nullptr);
    auto col_c_val = *iter.Get<i32, false>(ColId::col_c, nullptr);
    EXPECT_LT(col_a_val, 10);
    EXPECT_LT(col_c_val, 750);
    count++;
  }

  LOG_INFO("Selected: {}", count);
  EXPECT_LE(count, 10u);
}

}  // namespace tpl::sql::test
