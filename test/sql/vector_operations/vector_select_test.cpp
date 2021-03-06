#include <vector>

#include "common/exception.h"
#include "sql/constant_vector.h"
#include "sql/tuple_id_list.h"
#include "sql/vector.h"
#include "sql/vector_operations/vector_operations.h"
#include "util/sql_test_harness.h"

namespace tpl::sql {

class VectorSelectTest : public TplTest {};

TEST_F(VectorSelectTest, MismatchedInputTypes) {
  auto a = MakeTinyIntVector(2);
  auto b = MakeBigIntVector(2);
  auto result = TupleIdList(a->GetSize());
  result.AddAll();
  EXPECT_THROW(VectorOps::SelectEqual(*a, *b, &result), TypeMismatchException);
}

TEST_F(VectorSelectTest, MismatchedSizes) {
  auto a = MakeTinyIntVector(54);
  auto b = MakeBigIntVector(19);
  auto result = TupleIdList(a->GetSize());
  result.AddAll();
  EXPECT_THROW(VectorOps::SelectEqual(*a, *b, &result), Exception);
}

TEST_F(VectorSelectTest, MismatchedCounts) {
  auto a = MakeTinyIntVector(10);
  auto b = MakeBigIntVector(10);

  auto filter_a = TupleIdList(a->GetCount());
  auto filter_b = TupleIdList(b->GetCount());

  filter_a = {0, 1, 2};
  filter_b = {9, 8};

  a->SetFilteredTupleIdList(&filter_a, filter_a.GetTupleCount());
  b->SetFilteredTupleIdList(&filter_b, filter_b.GetTupleCount());

  auto result = TupleIdList(a->GetSize());
  result.AddAll();

  EXPECT_THROW(VectorOps::SelectEqual(*a, *b, &result), Exception);
}

TEST_F(VectorSelectTest, InvalidTIDListSize) {
  auto a = MakeTinyIntVector(10);
  auto b = MakeBigIntVector(10);

  auto result = TupleIdList(1);
  result.AddAll();

  EXPECT_THROW(VectorOps::SelectEqual(*a, *b, &result), Exception);
}

TEST_F(VectorSelectTest, SelectNumeric) {
  // a = [NULL, 1, 6, NULL, 4, 5]
  // b = [0, NULL, 4, NULL, 5, 5]
  auto a = MakeTinyIntVector({0, 1, 6, 3, 4, 5}, {true, false, false, true, false, false});
  auto b = MakeTinyIntVector({0, 1, 4, 3, 5, 5}, {false, true, false, true, false, false});
  auto _2 = ConstantVector(GenericValue::CreateTinyInt(2));

  for (auto type_id : {TypeId::TinyInt, TypeId::SmallInt, TypeId::Integer, TypeId::BigInt,
                       TypeId::Float, TypeId::Double}) {
    a->Cast(type_id);
    b->Cast(type_id);
    _2.Cast(type_id);

    TupleIdList input_list(a->GetSize());
    input_list.AddAll();

    // a < 2
    VectorOps::SelectLessThan(*a, _2, &input_list);
    EXPECT_EQ(1u, input_list.GetTupleCount());
    EXPECT_EQ(1u, input_list[0]);

    input_list.AddAll();

    // 2 < a
    VectorOps::SelectLessThan(_2, *a, &input_list);
    EXPECT_EQ(3u, input_list.GetTupleCount());
    EXPECT_EQ(2u, input_list[0]);
    EXPECT_EQ(4u, input_list[1]);
    EXPECT_EQ(5u, input_list[2]);

    input_list.AddAll();

    // 2 == a
    VectorOps::SelectEqual(_2, *a, &input_list);
    EXPECT_TRUE(input_list.IsEmpty());

    input_list.AddAll();

    // a != b = [2, 4]
    VectorOps::SelectNotEqual(*a, *b, &input_list);
    EXPECT_EQ(2u, input_list.GetTupleCount());
    EXPECT_EQ(2u, input_list[0]);
    EXPECT_EQ(4u, input_list[1]);

    input_list.AddAll();

    // b == a = [5]
    VectorOps::SelectEqual(*b, *a, &input_list);
    EXPECT_EQ(1u, input_list.GetTupleCount());
    EXPECT_EQ(5u, input_list[0]);

    input_list.AddAll();

    // a < b = [4]
    VectorOps::SelectLessThan(*a, *b, &input_list);
    EXPECT_EQ(1u, input_list.GetTupleCount());
    EXPECT_EQ(4u, input_list[0]);

    input_list.AddAll();

    // a <= b = [4, 5]
    VectorOps::SelectLessThanEqual(*a, *b, &input_list);
    EXPECT_EQ(2, input_list.GetTupleCount());
    EXPECT_EQ(4u, input_list[0]);
    EXPECT_EQ(5u, input_list[1]);

    input_list.AddAll();

    // a > b = [2]
    VectorOps::SelectGreaterThan(*a, *b, &input_list);
    EXPECT_EQ(1, input_list.GetTupleCount());
    EXPECT_EQ(2u, input_list[0]);

    input_list.AddAll();

    // a >= b = [2]
    VectorOps::SelectGreaterThanEqual(*a, *b, &input_list);
    EXPECT_EQ(2, input_list.GetTupleCount());
    EXPECT_EQ(2u, input_list[0]);
    EXPECT_EQ(5u, input_list[1]);
  }
}

TEST_F(VectorSelectTest, SelectNumericWithNullConstant) {
  // a = [0, 1, NULL, NULL, 4, 5]
  auto a = MakeIntegerVector({0, 1, 2, 3, 4, 5}, {false, false, true, true, false, false});
  auto null_constant = ConstantVector(GenericValue::CreateNull(a->GetTypeId()));

#define NULL_TEST(OP)                                \
  /* a <OP> NULL */                                  \
  {                                                  \
    TupleIdList list(a->GetSize());                  \
    list.AddAll();                                   \
    VectorOps::Select##OP(*a, null_constant, &list); \
    EXPECT_TRUE(list.IsEmpty());                     \
  }                                                  \
  /* NULL <OP> a */                                  \
  {                                                  \
    TupleIdList list(a->GetSize());                  \
    list.AddAll();                                   \
    VectorOps::Select##OP(*a, null_constant, &list); \
    EXPECT_TRUE(list.IsEmpty());                     \
  }

  NULL_TEST(Equal)
  NULL_TEST(GreaterThan)
  NULL_TEST(GreaterThanEqual)
  NULL_TEST(LessThan)
  NULL_TEST(LessThanEqual)
  NULL_TEST(NotEqual)

#undef NULL_TEST
}

TEST_F(VectorSelectTest, SelectString) {
  auto a = MakeVarcharVector({"His palm's are sweaty",
                              {},
                              "arms are heavy",
                              "vomit on his sweater already",
                              "mom's spaghetti"},
                             {false, true, false, false, false});
  auto b = MakeVarcharVector({"He's nervous",
                              "but on the surface he looks calm and ready",
                              {},
                              "to drop bombs",
                              "but he keeps on forgetting"},
                             {false, false, true, false, false});
  auto tid_list = TupleIdList(a->GetSize());

  // a == b = []
  tid_list.AddAll();
  VectorOps::SelectEqual(*a, *b, &tid_list);
  EXPECT_EQ(0u, tid_list.GetTupleCount());

  // a != b = [0, 3, 4]
  tid_list.AddAll();
  VectorOps::SelectNotEqual(*a, *b, &tid_list);
  EXPECT_EQ(3u, tid_list.GetTupleCount());
  EXPECT_EQ(0u, tid_list[0]);
  EXPECT_EQ(3u, tid_list[1]);
  EXPECT_EQ(4u, tid_list[2]);

  // a < b = [0]
  tid_list.AddAll();
  VectorOps::SelectLessThan(*a, *b, &tid_list);
  EXPECT_EQ(0u, tid_list.GetTupleCount());

  // a > b = [1, 3, 4]
  tid_list.AddAll();
  VectorOps::SelectGreaterThan(*a, *b, &tid_list);
  EXPECT_EQ(3u, tid_list.GetTupleCount());
  EXPECT_EQ(0u, tid_list[0]);
  EXPECT_EQ(3u, tid_list[1]);
  EXPECT_EQ(4u, tid_list[2]);
}

TEST_F(VectorSelectTest, SelectBetweenNumeric) {
  // input = [NULL,    1, NULL,    4, NULL,    6, NULL,  2, 2, 20, 10]
  // low   = [NULL, NULL,    6,    2, NULL, NULL,    4,  2, 1,  1,  0]
  // high  = [NULL, NULL, NULL, NULL,   10,   20,   10, 10, 2, 10, 20]
  auto input =
      MakeTinyIntVector({0, 1, 0, 4, 0, 6, 0, 2, 2, 20, 10},
                        {true, false, true, false, true, false, true, false, false, false, false});
  auto lower =
      MakeTinyIntVector({0, 0, 6, 2, 0, 0, 4, 2, 1, 1, 0},
                        {true, true, false, false, true, true, false, false, false, false, false});
  auto upper =
      MakeTinyIntVector({0, 0, 0, 0, 10, 20, 10, 10, 2, 10, 20},
                        {true, true, true, true, false, false, false, false, false, false, false});

  auto const_lower = sql::ConstantVector(sql::GenericValue::CreateTinyInt(1));
  auto const_upper = sql::ConstantVector(sql::GenericValue::CreateTinyInt(20));

  for (auto type_id : {TypeId::TinyInt, TypeId::SmallInt, TypeId::Integer, TypeId::BigInt,
                       TypeId::Float, TypeId::Double}) {
    input->Cast(type_id);
    lower->Cast(type_id);
    upper->Cast(type_id);
    const_lower.Cast(type_id);
    const_upper.Cast(type_id);

    TupleIdList input_list(input->GetSize());
    input_list.AddAll();

    // lower <= a <= upper
    VectorOps::SelectBetween(*input, *lower, *upper, true, true, &input_list);
    EXPECT_EQ(3u, input_list.GetTupleCount());
    EXPECT_EQ(7u, input_list[0]);
    EXPECT_EQ(8u, input_list[1]);
    EXPECT_EQ(10u, input_list[2]);

    input_list.AddAll();

    // lower <= a < upper
    VectorOps::SelectBetween(*input, *lower, *upper, true, false, &input_list);
    EXPECT_EQ(2u, input_list.GetTupleCount());
    EXPECT_EQ(7u, input_list[0]);
    EXPECT_EQ(10u, input_list[1]);

    input_list.AddAll();

    // lower < a <= upper
    VectorOps::SelectBetween(*input, *lower, *upper, false, true, &input_list);
    EXPECT_EQ(2u, input_list.GetTupleCount());
    EXPECT_EQ(8u, input_list[0]);
    EXPECT_EQ(10u, input_list[1]);

    input_list.AddAll();

    // lower < a < upper
    VectorOps::SelectBetween(*input, *lower, *upper, false, false, &input_list);
    EXPECT_EQ(1u, input_list.GetTupleCount());
    EXPECT_EQ(10u, input_list[0]);

    input_list.AddAll();

    // const_lower < a < upper
    VectorOps::SelectBetween(*input, const_lower, *upper, false, false, &input_list);
    EXPECT_EQ(3u, input_list.GetTupleCount());
    EXPECT_EQ(5u, input_list[0]);
    EXPECT_EQ(7u, input_list[1]);
    EXPECT_EQ(10u, input_list[2]);
  }
}

TEST_F(VectorSelectTest, SelectBetweenWithNullConstant) {
  // a = [0, 1, NULL, NULL, 4, 5]
  auto a = MakeIntegerVector({0, 1, 2, 3, 4, 5}, {false, false, true, true, false, false});
  auto null_constant = ConstantVector(GenericValue::CreateNull(a->GetTypeId()));

  {
    TupleIdList list(a->GetSize());
    list.AddAll();
    VectorOps::SelectBetween(*a, null_constant, null_constant, true, true, &list);
    EXPECT_TRUE(list.IsEmpty());
  }
  {
    TupleIdList list(a->GetSize());
    list.AddAll();
    VectorOps::SelectBetween(*a, *a, null_constant, true, true, &list);
    EXPECT_TRUE(list.IsEmpty());
  }

  {
    TupleIdList list(a->GetSize());
    list.AddAll();
    VectorOps::SelectBetween(null_constant, *a, *a, true, true, &list);
    EXPECT_TRUE(list.IsEmpty());
  }
}

TEST_F(VectorSelectTest, IsNullAndIsNotNull) {
  auto vec = MakeFloatVector({1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0},
                             {false, true, false, true, true, false, false});
  auto tid_list = TupleIdList(vec->GetSize());

  // Try first with a full TID list

  // IS NULL(vec) = [1, 3, 4]
  tid_list.AddAll();
  VectorOps::IsNull(*vec, &tid_list);
  EXPECT_EQ(3u, tid_list.GetTupleCount());
  EXPECT_EQ(1u, tid_list[0]);
  EXPECT_EQ(3u, tid_list[1]);
  EXPECT_EQ(4u, tid_list[2]);

  // IS_NOT_NULL(vec) = [0, 2, 5, 6]
  tid_list.AddAll();
  VectorOps::IsNotNull(*vec, &tid_list);
  EXPECT_EQ(4u, tid_list.GetTupleCount());
  EXPECT_EQ(0u, tid_list[0]);
  EXPECT_EQ(2u, tid_list[1]);
  EXPECT_EQ(5u, tid_list[2]);
  EXPECT_EQ(6u, tid_list[3]);

  // Try with a partial input list

  tid_list.Clear();
  tid_list.Add(1);
  tid_list.Add(4);
  VectorOps::IsNull(*vec, &tid_list);
  EXPECT_EQ(2u, tid_list.GetTupleCount());
  EXPECT_EQ(1u, tid_list[0]);
  EXPECT_EQ(4u, tid_list[1]);

  tid_list.Clear();
  tid_list.AddRange(2, 5);
  VectorOps::IsNotNull(*vec, &tid_list);
  EXPECT_EQ(1u, tid_list.GetTupleCount());
  EXPECT_EQ(2u, tid_list[0]);
}

}  // namespace tpl::sql
