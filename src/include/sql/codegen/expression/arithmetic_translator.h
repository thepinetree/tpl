#pragma once

#include "sql/codegen/expression/expression_translator.h"
#include "sql/planner/expressions/operator_expression.h"

namespace tpl::sql::codegen {

/**
 * A translator for arithmetic expressions.
 */
class ArithmeticTranslator : public ExpressionTranslator {
 public:
  /**
   * Create a translator for the given arithmetic expression.
   * @param expr The expression to translate.
   * @param compilation_context The context in which translation occurs.
   */
  ArithmeticTranslator(const planner::OperatorExpression &expr,
                       CompilationContext *compilation_context);

  /**
   * Derive the value of the expression.
   * @param context The context containing collected subexpressions.
   * @param provider A provider for specific column values.
   * @return The value of the expression.
   */
  ast::Expression *DeriveValue(ConsumerContext *context,
                               const ColumnValueProvider *provider) const override;
};

}  // namespace tpl::sql::codegen
