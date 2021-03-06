#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast/builtins.h"
#include "sql/planner/expressions/abstract_expression.h"
#include "sql/planner/expressions/aggregate_expression.h"
#include "sql/planner/expressions/builtin_function_expression.h"
#include "sql/planner/expressions/case_expression.h"
#include "sql/planner/expressions/column_value_expression.h"
#include "sql/planner/expressions/comparison_expression.h"
#include "sql/planner/expressions/conjunction_expression.h"
#include "sql/planner/expressions/constant_value_expression.h"
#include "sql/planner/expressions/derived_value_expression.h"
#include "sql/planner/expressions/operator_expression.h"
#include "sql/value.h"

namespace tpl::sql::planner {

/**
 * Helper class to reduce typing and increase readability when hand crafting expression.
 */
class ExpressionMaker {
  using NewExpression = std::unique_ptr<planner::AbstractExpression>;
  using NewAggExpression = std::unique_ptr<planner::AggregateExpression>;

 public:
  using Expression = const planner::AbstractExpression *;
  using AggExpression = const planner::AggregateExpression *;

  /**
   * @return A constant expression with the given boolean value.
   */
  Expression ConstantBool(bool val) {
    return Alloc(
        std::make_unique<planner::ConstantValueExpression>(sql::GenericValue::CreateBoolean(val)));
  }

  /**
   * Create an integer constant expression
   */
  Expression Constant(int32_t val) {
    return Alloc(
        std::make_unique<planner::ConstantValueExpression>(sql::GenericValue::CreateInteger(val)));
  }

  /**
   * Create a floating point constant expression
   */
  Expression Constant(float val) {
    return Alloc(
        std::make_unique<planner::ConstantValueExpression>(sql::GenericValue::CreateFloat(val)));
  }

  /**
   * Create a date constant expression
   */
  Expression Constant(int16_t year, uint8_t month, uint8_t day) {
    return Alloc(std::make_unique<planner::ConstantValueExpression>(
        sql::GenericValue::CreateDate(static_cast<uint32_t>(year), month, day)));
  }

  /**
   * Create a string constant expression
   */
  Expression Constant(const std::string &str) {
    return Alloc(
        std::make_unique<planner::ConstantValueExpression>(sql::GenericValue::CreateVarchar(str)));
  }

  /**
   * Create a column value expression
   */
  Expression CVE(uint16_t column_oid, sql::TypeId type) {
    return Alloc(std::make_unique<planner::ColumnValueExpression>(column_oid, type));
  }

  /**
   * Create a derived value expression
   */
  Expression DVE(sql::TypeId type, int tuple_idx, int value_idx) {
    return Alloc(std::make_unique<planner::DerivedValueExpression>(type, tuple_idx, value_idx));
  }

  /**
   * Create a Comparison expression
   */
  Expression Compare(planner::ExpressionType comp_type, Expression child1, Expression child2) {
    return Alloc(std::make_unique<planner::ComparisonExpression>(
        comp_type, std::vector<Expression>{child1, child2}));
  }

  /**
   *  expression for child1 == child2
   */
  Expression CompareEq(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_EQUAL, child1, child2);
  }

  /**
   * Create expression for child1 == child2
   */
  Expression CompareNeq(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_NOT_EQUAL, child1, child2);
  }

  /**
   * Create expression for child1 < child2
   */
  Expression CompareLt(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_LESS_THAN, child1, child2);
  }

  /**
   * Create expression for child1 <= child2
   */
  Expression CompareLe(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_LESS_THAN_OR_EQUAL_TO, child1, child2);
  }

  /**
   * Create expression for child1 > child2
   */
  Expression CompareGt(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_GREATER_THAN, child1, child2);
  }

  /**
   * Create expression for child1 >= child2
   */
  Expression CompareGe(Expression child1, Expression child2) {
    return Compare(planner::ExpressionType::COMPARE_GREATER_THAN_OR_EQUAL_TO, child1, child2);
  }

  /**
   * Create a BETWEEN expression: low <= input <= high
   */
  Expression CompareBetween(Expression input, Expression low, Expression high) {
    return Alloc(std::make_unique<planner::ComparisonExpression>(
        planner::ExpressionType::COMPARE_BETWEEN, std::vector<Expression>({input, low, high})));
  }

  /**
   * Create a LIKE() expression.
   */
  Expression CompareLike(Expression input, Expression s) {
    return Alloc(std::make_unique<planner::BuiltinFunctionExpression>(
        ast::Builtin::Like, std::vector<Expression>({input, s}), TypeId::Boolean));
  }

  /**
   * Create a NOT LIKE() expression.
   */
  Expression CompareNotLike(Expression input, Expression s) { return OpNot(CompareLike(input, s)); }

  /**
   * Create a unary operation expression
   */
  Expression Operator(planner::ExpressionType op_type, sql::TypeId ret_type, Expression child) {
    return Alloc(std::make_unique<planner::OperatorExpression>(op_type, ret_type,
                                                               std::vector<Expression>{child}));
  }

  /**
   * Create a binary operation expression
   */
  Expression Operator(planner::ExpressionType op_type, sql::TypeId ret_type, Expression child1,
                      Expression child2) {
    return Alloc(std::make_unique<planner::OperatorExpression>(
        op_type, ret_type, std::vector<Expression>{child1, child2}));
  }

  /**
   * Cast the input expression to the given type.
   */
  Expression OpCast(Expression input, sql::TypeId to_type) {
    return Operator(planner::ExpressionType::OPERATOR_CAST, to_type, input);
  }

  /**
   * create expression for child1 + child2
   */
  Expression OpSum(Expression child1, Expression child2) {
    return Operator(planner::ExpressionType::OPERATOR_PLUS, child1->GetReturnValueType(), child1,
                    child2);
  }

  /**
   * create expression for child1 - child2
   */
  Expression OpMin(Expression child1, Expression child2) {
    return Operator(planner::ExpressionType::OPERATOR_MINUS, child1->GetReturnValueType(), child1,
                    child2);
  }

  /**
   * create expression for child1 * child2
   */
  Expression OpMul(Expression child1, Expression child2) {
    return Operator(planner::ExpressionType::OPERATOR_MULTIPLY, child1->GetReturnValueType(),
                    child1, child2);
  }

  /**
   * create expression for child1 / child2
   */
  Expression OpDiv(Expression child1, Expression child2) {
    return Operator(planner::ExpressionType::OPERATOR_DIVIDE, child1->GetReturnValueType(), child1,
                    child2);
  }

  /**
   * Create expression for NOT(child)
   */
  Expression OpNot(Expression child) {
    return Operator(planner::ExpressionType::OPERATOR_NOT, sql::TypeId::Boolean, child);
  }

  /**
   * Create expression for child1 AND/OR child2
   */
  Expression Conjunction(planner::ExpressionType op_type, Expression child1, Expression child2) {
    return Alloc(std::make_unique<planner::ConjunctionExpression>(
        op_type, std::vector<Expression>{child1, child2}));
  }

  /**
   * Create expression for child1 AND child2
   */
  Expression ConjunctionAnd(Expression child1, Expression child2) {
    return Conjunction(planner::ExpressionType::CONJUNCTION_AND, child1, child2);
  }

  /**
   * Create expression for child1 OR child2
   */
  Expression ConjunctionOr(Expression child1, Expression child2) {
    return Conjunction(planner::ExpressionType::CONJUNCTION_OR, child1, child2);
  }

  /**
   * Create an expression for a builtin call.
   */
  Expression BuiltinFunction(ast::Builtin builtin, std::vector<Expression> args,
                             const sql::TypeId return_value_type) {
    return Alloc(std::make_unique<planner::BuiltinFunctionExpression>(builtin, std::move(args),
                                                                      return_value_type));
  }

  /**
   * Create an aggregate expression
   */
  AggExpression AggregateTerm(planner::ExpressionType agg_type, Expression child, bool distinct) {
    return Alloc(std::make_unique<planner::AggregateExpression>(
        agg_type, std::vector<Expression>{child}, distinct));
  }

  /**
   * Create a sum aggregate expression
   */
  AggExpression AggSum(Expression child, bool distinct = false) {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_SUM, child, distinct);
  }

  /**
   * Create a sum aggregate expression
   */
  AggExpression AggMin(Expression child, bool distinct = false) {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_MIN, child, distinct);
  }

  /**
   * Create a sum aggregate expression
   */
  AggExpression AggMax(Expression child, bool distinct = false) {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_MAX, child, distinct);
  }

  /**
   * Create a avg aggregate expression
   */
  AggExpression AggAvg(Expression child, bool distinct = false) {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_AVG, child, distinct);
  }

  /**
   * Create a count aggregate expression
   */
  AggExpression AggCount(Expression child, bool distinct = false) {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_COUNT, child, distinct);
  }

  /**
   * Create a count aggregate expression
   */
  AggExpression AggCountStar() {
    return AggregateTerm(planner::ExpressionType::AGGREGATE_COUNT, Constant(1), false);
  }

  /**
   * Create a case expression with no default result.
   */
  Expression Case(const std::vector<std::pair<Expression, Expression>> &cases) {
    return Case(cases, nullptr);
  }

  /**
   * Create a case expression.
   */
  Expression Case(const std::vector<std::pair<Expression, Expression>> &cases,
                  Expression default_val) {
    const auto ret_type = cases[0].second->GetReturnValueType();
    std::vector<CaseExpression::WhenClause> clauses;
    clauses.reserve(cases.size());
    std::ranges::transform(cases, std::back_inserter(clauses), [](auto &p) {
      return CaseExpression::WhenClause{p.first, p.second};
    });
    return Alloc(std::make_unique<CaseExpression>(ret_type, clauses, default_val));
  }

 private:
  Expression Alloc(NewExpression &&new_expr) {
    allocated_exprs_.emplace_back(std::move(new_expr));
    return allocated_exprs_.back().get();
  }

  AggExpression Alloc(NewAggExpression &&agg_expr) {
    allocated_aggs_.emplace_back(std::move(agg_expr));
    return allocated_aggs_.back().get();
  }

  std::vector<NewExpression> allocated_exprs_;
  std::vector<NewAggExpression> allocated_aggs_;
};

}  // namespace tpl::sql::planner
