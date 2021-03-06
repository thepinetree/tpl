#include <algorithm>
#include <random>
#include <vector>

#include "sql/operators/comparison_operators.h"
#include "sql/sorter.h"
#include "sql/vector_projection.h"
#include "sql/vector_projection_iterator.h"
#include "util/test_harness.h"

namespace tpl::sql {

namespace {

/**
 * The input tuples. All sorter instances use this tuple as input and output.
 */
struct Tuple {
  int64_t key;
  int64_t attributes;
};

/**
 * Insert a number of input tuples into the given sorter instance.
 * @param sorter The sorter to insert into.
 * @param num_tuples The number of tuple to insert.
 */
void PopulateSorter(Sorter *sorter, uint32_t num_tuples = kDefaultVectorSize + 7) {
  std::random_device r = RandomDevice();
  for (uint32_t i = 0; i < num_tuples; i++) {
    auto tuple = reinterpret_cast<Tuple *>(sorter->AllocInputTuple());
    tuple->key = r() % num_tuples;
    tuple->attributes = r() % 10;
  }
}

/**
 * Compare two tuples.
 * @param lhs The first tuple.
 * @param rhs The second tuple.
 * @return < 0 if lhs < rhs, 0 if lhs = rhs, > 0 if lhs > rhs.
 */
bool CompareTuple(const Tuple &lhs, const Tuple &rhs) { return lhs.key < rhs.key; }

/**
 * Convert row-oriented data in the rows vector to columnar format in the given
 * vector projection.
 * @param rows The array of row-oriented tuple data.
 * @param num_rows The number of rows in the array.
 * @param iter The output vector projection iterator.
 */
void Transpose(const byte *rows[], uint64_t num_rows, VectorProjectionIterator *iter) {
  for (uint64_t i = 0; i < num_rows; i++) {
    auto tuple = reinterpret_cast<const Tuple *>(rows[i]);
    iter->SetValue<int64_t, false>(0, tuple->key, false);
    iter->SetValue<int64_t, false>(1, tuple->attributes, false);
    iter->Advance();
  }
}

/**
 * Check if the given vector is sorted.
 * @tparam T The primitive/native type of the element the vector contains.
 * @param vec The vector to check is sorted.
 * @return True if sorted; false otherwise.
 */
template <class T>
bool IsSorted(const Vector &vec) {
  const auto data = reinterpret_cast<const T *>(vec.GetData());
  for (uint64_t i = 1; i < vec.GetCount(); i++) {
    bool left_null = vec.GetNullMask()[i - 1];
    bool right_null = vec.GetNullMask()[i];
    if (left_null != right_null) {
      return false;
    }
    if (left_null) {
      continue;
    }
    if (!LessThanEqual<T>::Apply(data[i - 1], data[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

class SorterVectorIteratorTest : public TplTest {
 public:
  SorterVectorIteratorTest()
      : memory_(nullptr),
        schema_({{"key", Type::BigIntType(false)}, {"attributes", Type::BigIntType(true)}}) {}

 protected:
  MemoryPool *memory() { return &memory_; }

  std::vector<const Schema::ColumnInfo *> row_meta() const {
    std::vector<const Schema::ColumnInfo *> cols;
    for (const auto &col : schema_.GetColumns()) {
      cols.push_back(&col);
    }
    return cols;
  }

 private:
  MemoryPool memory_;
  Schema schema_;
};

TEST_F(SorterVectorIteratorTest, EmptyIterator) {
  const auto compare = [](const void *lhs, const void *rhs) {
    return CompareTuple(*reinterpret_cast<const Tuple *>(lhs),
                        *reinterpret_cast<const Tuple *>(rhs));
  };
  Sorter sorter(memory(), compare, sizeof(Tuple));

  for (SorterVectorIterator iter(sorter, row_meta(), Transpose); iter.HasNext();
       iter.Next(Transpose)) {
    FAIL() << "Iteration should not occur on empty sorter instance";
  }
}

TEST_F(SorterVectorIteratorTest, Iterate) {
  // To ensure at least two vector's worth of data
  const uint32_t num_elems = kDefaultVectorSize + 29;

  const auto compare = [](const void *lhs, const void *rhs) {
    return CompareTuple(*reinterpret_cast<const Tuple *>(lhs),
                        *reinterpret_cast<const Tuple *>(rhs));
  };
  Sorter sorter(memory(), compare, sizeof(Tuple));
  PopulateSorter(&sorter, num_elems);
  sorter.Sort();

  uint32_t num_found = 0;
  for (SorterVectorIterator iter(sorter, row_meta(), Transpose); iter.HasNext();
       iter.Next(Transpose)) {
    auto *vpi = iter.GetVectorProjectionIterator();

    // VPI shouldn't be filtered
    EXPECT_FALSE(vpi->IsFiltered());
    EXPECT_GT(vpi->GetTotalTupleCount(), 0u);

    // VPI should have two columns: key and attributes
    EXPECT_EQ(2u, vpi->GetVectorProjection()->GetColumnCount());

    // Neither vector should have NULLs
    EXPECT_FALSE(vpi->GetVectorProjection()->GetColumn(0)->GetNullMask().Any());
    EXPECT_FALSE(vpi->GetVectorProjection()->GetColumn(1)->GetNullMask().Any());

    // Verify sorted
    const auto *key_vector = vpi->GetVectorProjection()->GetColumn(0);
    const auto *key_data = reinterpret_cast<const decltype(Tuple::key) *>(key_vector->GetData());
    EXPECT_TRUE(std::is_sorted(key_data, key_data + key_vector->GetCount()));

    // Count
    num_found += vpi->GetSelectedTupleCount();
  }

  EXPECT_EQ(num_elems, num_found);
}

}  // namespace tpl::sql
