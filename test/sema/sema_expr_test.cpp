#include <string>
#include <vector>

#include "ast/ast_builder.h"
#include "util/test_harness.h"

#include "ast/ast_node_factory.h"
#include "sema/sema.h"

namespace tpl::sema {

class SemaExprTest : public TplTest, public ast::TestAstBuilder {
 public:
  void ResetErrorReporter() { error_reporter()->Reset(); }
};

struct TestCase {
  bool has_errors;
  std::string msg;
  ast::AstNode *tree;
};

TEST_F(SemaExprTest, LogicalOperationTest) {
  TestCase tests[] = {
      // Test: 1 and 2
      // Expectation: Error
      {true, "1 and 2 is not a valid logical operation",
       BinOp<parsing::Token::Type::AND>(IntLit(1), IntLit(2))},

      // Test: 1 and true
      // Expectation: Error
      {true, "1 and true is not a valid logical operation",
       BinOp<parsing::Token::Type::AND>(IntLit(1), BoolLit(true))},

      // Test: false and 2
      // Expectation: Error
      {true, "false and 1 is not a valid logical operation",
       BinOp<parsing::Token::Type::AND>(BoolLit(false), IntLit(2))},

      // Test: false and true
      // Expectation: Valid
      {false, "false and true is a valid logical operation",
       BinOp<parsing::Token::Type::AND>(BoolLit(false), BoolLit(true))},
  };

  for (const auto &test : tests) {
    Sema sema(ctx());
    bool has_errors = sema.Run(test.tree);
    EXPECT_EQ(test.has_errors, has_errors) << test.msg;
    ResetErrorReporter();
  }
}

TEST_F(SemaExprTest, ArithmeticOperationTest) {
  TestCase tests[] = {
      // Test: Add SQL integers.
      // Expectation: Valid
      {false, "SQL integers should be summable",
       Block({
           DeclStmt(DeclVar("a", "IntegerVal", nullptr)),  // var a: IntegerVal
           DeclStmt(DeclVar("b", "IntegerVal", nullptr)),  // var b: IntegerVal
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Add SQL floats.
      // Expectation: Valid
      {false, "SQL floats should be summable",
       Block({
           DeclStmt(DeclVar("a", "RealVal", nullptr)),  // var a: RealVal
           DeclStmt(DeclVar("b", "RealVal", nullptr)),  // var b: RealVal
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Add primitive floats.
      // Expectation: Valid
      {false, "Primitive floats should be summable",
       Block({
           DeclStmt(DeclVar("a", "float32", nullptr)),  // var a: float32
           DeclStmt(DeclVar("b", "float32", nullptr)),  // var b: float32
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Floats of different types are not summable.
      // Expectation: Invalid
      {true, "Primitive Floats of different types are not summable",
       Block({
           DeclStmt(DeclVar("a", "float64", nullptr)),  // var a: float64
           DeclStmt(DeclVar("b", "float32", nullptr)),  // var b: float32
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Mixing primitive and SQL types in arithmetic is invalid.
      // Expectation: Invalid
      {true, "Mixing primitive and SQL types in arithmetic is invalid",
       Block({
           DeclStmt(DeclVar("a", "RealVal", nullptr)),  // var a: RealVal
           DeclStmt(DeclVar("b", "float64", nullptr)),  // var b: float64
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Non-arithmetic types cannot be added.
      // Expectation: Invalid
      {true, "Non-arithmetic types cannot be added",
       Block({
           DeclStmt(DeclVar("a", "JoinHashTable", nullptr)),  // var a: JoinHashTable
           DeclStmt(DeclVar("b", "Sorter", nullptr)),         // var b: Sorter
           DeclStmt(DeclVar("c", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), IdentExpr("b")))),  // var c = a + b
       })},

      // Test: Implicit constant casting in arithmetic is supported.
      // Expectation: Invalid
      {false, "Implicit constant casting in arithmetic is supported",
       Block({
           DeclStmt(DeclVar("a", "float32", nullptr)),  // var a: float32
           DeclStmt(DeclVar("b", BinOp<parsing::Token::Type::PLUS>(
                                     IdentExpr("a"), FloatLit(23.0)))),  // var b = a + 23.0
       })},
  };

  for (const auto &test : tests) {
    Sema sema(ctx());
    bool has_errors = sema.Run(test.tree);
    EXPECT_EQ(test.has_errors, has_errors) << test.msg;
    ResetErrorReporter();
  }
}

TEST_F(SemaExprTest, ComparisonOperationWithPointersTest) {
  // clang-format off
  TestCase tests[] = {
      // Test: Compare a primitive int32 with an integer
      // Expectation: Invalid
      {true, "Integers should not be comparable to pointers",
       Block({
           DeclStmt(DeclVar(Ident("i"), IntLit(10))),                           // var i = 10
           DeclStmt(DeclVar(Ident("ptr"), PtrType(IdentExpr("int32")))),        // var ptr: *int32
           ExprStmt(CmpEq(IdentExpr("i"), IdentExpr("ptr"))),                   // i == ptr
       })},

      // Test: Compare pointers to primitive int32 and float32
      // Expectation: Valid
      {true, "Pointers of different types should not be comparable",
       Block({
           DeclStmt(DeclVar(Ident("ptr1"), PtrType(IdentExpr("int32")))),       // var ptr1: *int32
           DeclStmt(DeclVar(Ident("ptr2"), PtrType(IdentExpr("float32")))),     // var ptr2: *float32
           ExprStmt(CmpEq(IdentExpr("ptr1"), IdentExpr("ptr"))),                // ptr1 == ptr2
       })},

      // Test: Compare pointers to the same type
      // Expectation: Valid
      {false, "Pointers to the same type should be comparable",
       Block({
           DeclStmt(DeclVar(Ident("ptr1"), PtrType(IdentExpr("float32")))),     // var ptr1: *float32
           DeclStmt(DeclVar(Ident("ptr2"), PtrType(IdentExpr("float32")))),     // var ptr2: *float32
           ExprStmt(CmpEq(IdentExpr("ptr1"), IdentExpr("ptr2"))),               // ptr1 == ptr2
       })},

      // Test: Compare pointers to the same type, but using a relational op
      // Expectation: Invalid
      {true, "Pointers to the same type should be comparable",
       Block({
           DeclStmt(DeclVar(Ident("ptr1"), PtrType(IdentExpr("float32")))),     // var ptr1: *float32
           DeclStmt(DeclVar(Ident("ptr2"), PtrType(IdentExpr("float32")))),     // var ptr2: *float32
           ExprStmt(CmpLt(IdentExpr("ptr1"), IdentExpr("ptr2"))),               // ptr1 < ptr2
       })},
  };
  // clang-format on

  for (const auto &test : tests) {
    Sema sema(ctx());
    bool has_errors = sema.Run(test.tree);
    EXPECT_EQ(test.has_errors, has_errors) << test.msg;
    ResetErrorReporter();
  }
}

TEST_F(SemaExprTest, ArrayIndexTest) {
  // clang-format off
  TestCase tests[] = {
      // Test: Perform an array index using an integer literal
      // Expectation: Valid
      {false, "Array indexes can support literal indexes",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32")))),    // var arr: []int32
           ExprStmt(ArrayIndex(IdentExpr("arr"), IntLit(10))),                    // arr[10]
       })},

      // Test: Perform an array index using an integer variable
      // Expectation: Valid
      {false, "Array indexes can support variable integer indexes",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32")))),    // var arr: []int32
           DeclStmt(DeclVar(Ident("i"), IntLit(10))),                             // var i = 10
           ExprStmt(ArrayIndex(IdentExpr("arr"), IdentExpr("i"))),                // arr[i]
       })},

      // Test: Perform an array index using a negative integer.
      // Expectation: Invalid.
      {true, "Array indexes must be non-negative.",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32")))),    // var arr: []int32
           ExprStmt(ArrayIndex(IdentExpr("arr"), IntLit(-4))),                    // arr[-4]
       })},

      // Test: Perform an array index using a negative integer.
      // Expectation: Invalid.
      {true, "Array indexes must be smaller than declared array size.",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32"), 4))), // var arr: [4]int32
           ExprStmt(ArrayIndex(IdentExpr("arr"), IntLit(4))),                     // arr[4]
       })},

      // Test: Perform an array index using an floating-point variable
      // Expectation: Invalid
      {true, "Array indexes must be integer values",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32")))),    // var arr: []int32
           DeclStmt(DeclVar(Ident("i"), FloatLit(10.0))),                         // var i: float32 = 10.0
           ExprStmt(ArrayIndex(IdentExpr("arr"), IdentExpr("i"))),                // arr[i]
       })},

      // Test: Perform an array index using a SQL integer
      // Expectation: Invalid
      {true, "Array indexes must be integer values",
       Block({
           DeclStmt(DeclVar(Ident("arr"), ArrayTypeRepr(IdentExpr("int32")))),    // var arr: []int32
           DeclStmt(DeclVar(Ident("i"), IntegerSqlTypeRepr(), nullptr)),          // var i: IntegerVal
           ExprStmt(ArrayIndex(IdentExpr("arr"), IdentExpr("i"))),                // arr[i]
       })},
  };
  // clang-format on

  for (const auto &test : tests) {
    Sema sema(ctx());
    bool has_errors = sema.Run(test.tree);
    EXPECT_EQ(test.has_errors, has_errors) << test.msg;
    ResetErrorReporter();
  }
}

}  // namespace tpl::sema
