#include <string>
#include <utility>

#include "ast/context.h"
#include "ast/type.h"
#include "sema/error_reporter.h"
#include "util/region.h"
#include "util/test_harness.h"

namespace tpl::ast {

class TypeTest : public TplTest {
 public:
  TypeTest() : errors_(), ctx_(&errors_) {}

  ast::Context *ctx() { return &ctx_; }

  util::Region *region() { return ctx()->GetRegion(); }

  ast::Identifier Name(const std::string &s) { return ctx()->GetIdentifier(s); }

 private:
  sema::ErrorReporter errors_;
  ast::Context ctx_;
};

TEST_F(TypeTest, StructPaddingTest) {
  //
  // Summary: We create a TPL struct functionally equivalent to the C++ struct
  // 'Test' below. We expect the sizes to be the exact same, and the offsets of
  // each field to be the same.  In essence, we want TPL's layout engine to
  // replicate C/C++.
  //

  // clang-format off
  struct Test {
    bool a;
    int64_t  b;
    int8_t   c;
    int32_t  d;
    int8_t   e;
    int16_t  f;
    int64_t *g;
  };
  // clang-format on

  auto fields = util::RegionVector<ast::Field>(
      {
          {Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool)},
          {Name("b"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int64)},
          {Name("c"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int8)},
          {Name("d"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int32)},
          {Name("e"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int8)},
          {Name("f"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int16)},
          {Name("g"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int64)->PointerTo()},
      },
      region());

  auto *type = ast::StructType::Get(std::move(fields));

  // Expect: [0-1] b, [2-7] pad, [8-15] int64_t, [16-17] int8_t_1, [18-19] pad,
  //         [20-23] int32_t, [24-25] int8_t_2, [26-27] int16_t, [28-31] pad, [32-40] p
  EXPECT_EQ(sizeof(Test), type->GetSize());
  EXPECT_EQ(alignof(Test), type->GetAlignment());
  EXPECT_EQ(offsetof(Test, a), type->GetOffsetOfFieldByName(Name("a")));
  EXPECT_EQ(offsetof(Test, b), type->GetOffsetOfFieldByName(Name("b")));
  EXPECT_EQ(offsetof(Test, c), type->GetOffsetOfFieldByName(Name("c")));
  EXPECT_EQ(offsetof(Test, d), type->GetOffsetOfFieldByName(Name("d")));
  EXPECT_EQ(offsetof(Test, e), type->GetOffsetOfFieldByName(Name("e")));
  EXPECT_EQ(offsetof(Test, f), type->GetOffsetOfFieldByName(Name("f")));
  EXPECT_EQ(offsetof(Test, g), type->GetOffsetOfFieldByName(Name("g")));
}

TEST_F(TypeTest, PrimitiveTypeCacheTest) {
  //
  // In any one Context, we must have a cache of types. First, check all the
  // integer types
  //

#define GEN_INT_TEST(Kind)                                                            \
  {                                                                                   \
    auto *type1 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind);               \
    auto *type2 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind);               \
    EXPECT_EQ(type1, type2) << "Received two different " #Kind " types from context"; \
  }
  GEN_INT_TEST(Int8);
  GEN_INT_TEST(Int16);
  GEN_INT_TEST(Int32);
  GEN_INT_TEST(Int64);
  GEN_INT_TEST(Int128);
  GEN_INT_TEST(UInt8);
  GEN_INT_TEST(UInt16);
  GEN_INT_TEST(UInt32);
  GEN_INT_TEST(UInt64);
  GEN_INT_TEST(UInt128);
#undef GEN_INT_TEST

  //
  // Try the floating point types ...
  //

#define GEN_FLOAT_TEST(Kind)                                                          \
  {                                                                                   \
    auto *type1 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind);               \
    auto *type2 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind);               \
    EXPECT_EQ(type1, type2) << "Received two different " #Kind " types from context"; \
  }
  GEN_FLOAT_TEST(Float32)
  GEN_FLOAT_TEST(Float64)
#undef GEN_FLOAT_TEST

  //
  // Really simple types
  //

  EXPECT_EQ(ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool),
            ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool));
  EXPECT_EQ(ast::BuiltinType::Get(ctx(), ast::BuiltinType::Nil),
            ast::BuiltinType::Get(ctx(), ast::BuiltinType::Nil));
}

TEST_F(TypeTest, StructTypeCacheTest) {
  //
  // Create two structurally equivalent types and ensure only one struct
  // instantiation is created in the context
  //

  {
    auto *type1 = ast::StructType::Get(util::RegionVector<ast::Field>(
        {{Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool)},
         {Name("b"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int64)}},
        region()));

    auto *type2 = ast::StructType::Get(util::RegionVector<ast::Field>(
        {{Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool)},
         {Name("b"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int64)}},
        region()));

    EXPECT_EQ(type1, type2) << "Received two different pointers to same struct type";
  }

  //
  // Create two **DIFFERENT** structures and ensure they have different
  // instantiations in the context
  //

  {
    auto *type1 = ast::StructType::Get(util::RegionVector<ast::Field>(
        {{Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool)}}, region()));

    auto *type2 = ast::StructType::Get(util::RegionVector<ast::Field>(
        {{Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int64)}}, region()));

    EXPECT_NE(type1, type2) << "Received two equivalent pointers for different struct types";
  }
}

TEST_F(TypeTest, PointerTypeCacheTest) {
  //
  // Pointer types should also be cached. Thus, two *int8_t types should have
  // pointer equality in a given context
  //

#define GEN_INT_TEST(Kind)                                                             \
  {                                                                                    \
    auto *type1 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind)->PointerTo();   \
    auto *type2 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Kind)->PointerTo();   \
    EXPECT_EQ(type1, type2) << "Received two different *" #Kind " types from context"; \
  }
  GEN_INT_TEST(Int8);
  GEN_INT_TEST(Int16);
  GEN_INT_TEST(Int32);
  GEN_INT_TEST(Int64);
  GEN_INT_TEST(Int128);
  GEN_INT_TEST(UInt8);
  GEN_INT_TEST(UInt16);
  GEN_INT_TEST(UInt32);
  GEN_INT_TEST(UInt64);
  GEN_INT_TEST(UInt128);
#undef GEN_INT_TEST

  //
  // Try to create a pointer to the same struct and ensure the they point to the
  // same type instance
  //

  {
    auto *struct_type = ast::StructType::Get(util::RegionVector<ast::Field>(
        {{Name("a"), ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool)}}, region()));
    EXPECT_EQ(struct_type->PointerTo(), struct_type->PointerTo());
  }
}

TEST_F(TypeTest, FunctionTypeCacheTest) {
  //
  // Check that even function types are cached in the context. In the first
  // test, both functions have type: (bool)->bool
  //

  {
    auto *bool_type_1 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool);
    auto *type1 = ast::FunctionType::Get(
        util::RegionVector<ast::Field>({{Name("a"), bool_type_1}}, region()), bool_type_1);

    auto *bool_type_2 = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool);
    auto *type2 = ast::FunctionType::Get(
        util::RegionVector<ast::Field>({{Name("a"), bool_type_2}}, region()), bool_type_2);

    EXPECT_EQ(type1, type2);
  }

  //
  // In this test, the two functions have different types, and hence, should not
  // cache to the same function type instance. The first function has type:
  // (bool)->bool, but the second has type (int32)->int32
  //

  {
    // The first function has type: (bool)->bool
    auto *bool_type = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Bool);
    auto *type1 = ast::FunctionType::Get(
        util::RegionVector<ast::Field>({{Name("a"), bool_type}}, region()), bool_type);

    auto *int_type = ast::BuiltinType::Get(ctx(), ast::BuiltinType::Int32);
    auto *type2 = ast::FunctionType::Get(
        util::RegionVector<ast::Field>({{Name("a"), int_type}}, region()), int_type);

    EXPECT_NE(type1, type2);
  }
}

}  // namespace tpl::ast
