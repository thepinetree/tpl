#pragma once

#include "sql/codegen/expression/expression_translator.h"
#include "sql/planner/expressions/operator_expression.h"

namespace tpl::sql::codegen {

/**
 * A translator for null-checking expressions.
 */
class NullCheckTranslator : public ExpressionTranslator {
 public:
  /**
   * Create a translator for the given derived value.
   * @param expr The expression to translate.
   * @param compilation_context The context in which translation occurs.
   */
  NullCheckTranslator(const planner::OperatorExpression &expr,
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
