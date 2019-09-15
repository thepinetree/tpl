#include <vector>

#include "sql/constant_vector.h"
#include "sql/tuple_id_list.h"
#include "sql/vector.h"
#include "sql/vector_operations/vector_operators.h"
#include "util/sql_test_harness.h"

namespace tpl::sql {

class VectorSelectTest : public TplTest {};

TEST_F(VectorSelectTest, Select) {
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

    TupleIdList input_list(a->num_elements());
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
  }
}

TEST_F(VectorSelectTest, SelectNullConstant) {
  // a = [0, 1, NULL, NULL, 4, 5]
  auto a = MakeIntegerVector({0, 1, 2, 3, 4, 5}, {false, false, true, true, false, false});
  auto null_constant = ConstantVector(GenericValue::CreateNull(a->type_id()));

#define NULL_TEST(OP)                                \
  /* a <OP> NULL */                                  \
  {                                                  \
    TupleIdList list(a->num_elements());             \
    list.AddAll();                                   \
    VectorOps::Select##OP(*a, null_constant, &list); \
    EXPECT_TRUE(list.IsEmpty());                     \
  }                                                  \
  /* NULL <OP> a */                                  \
  {                                                  \
    TupleIdList list(a->num_elements());             \
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

}  // namespace tpl::sql
