#pragma once

#include "sql/codegen/expression/expression_translator.h"
#include "sql/planner/expressions/comparison_expression.h"

namespace tpl::sql::codegen {

/**
 * A translator for a comparison.
 */
class ComparisonTranslator : public ExpressionTranslator {
 public:
  /**
   * Create a translator for the given comparison expression.
   * @param expr The expression to translate.
   * @param compilation_context The context in which translation occurs.
   */
  ComparisonTranslator(const planner::ComparisonExpression &expr,
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
