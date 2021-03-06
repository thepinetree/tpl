#include "sql/codegen/expression/arithmetic_translator.h"

#include "spdlog/fmt/fmt.h"

#include "common/exception.h"
#include "sql/codegen/codegen.h"
#include "sql/codegen/compilation_context.h"
#include "sql/codegen/consumer_context.h"

namespace tpl::sql::codegen {

ArithmeticTranslator::ArithmeticTranslator(const planner::OperatorExpression &expr,
                                           CompilationContext *compilation_context)
    : ExpressionTranslator(expr, compilation_context) {
  // Prepare the left and right expression subtrees for translation.
  compilation_context->Prepare(*expr.GetChild(0));
  compilation_context->Prepare(*expr.GetChild(1));
}

ast::Expression *ArithmeticTranslator::DeriveValue(ConsumerContext *context,
                                                   const ColumnValueProvider *provider) const {
  auto left_val = context->DeriveValue(*GetExpression().GetChild(0), provider);
  auto right_val = context->DeriveValue(*GetExpression().GetChild(1), provider);

  switch (auto expr_type = GetExpression().GetExpressionType(); expr_type) {
    case planner::ExpressionType::OPERATOR_PLUS:
      return codegen_->BinaryOp(parsing::Token::Type::PLUS, left_val, right_val);
    case planner::ExpressionType::OPERATOR_MINUS:
      return codegen_->BinaryOp(parsing::Token::Type::MINUS, left_val, right_val);
    case planner::ExpressionType::OPERATOR_MULTIPLY:
      return codegen_->BinaryOp(parsing::Token::Type::STAR, left_val, right_val);
    case planner::ExpressionType::OPERATOR_DIVIDE:
      return codegen_->BinaryOp(parsing::Token::Type::SLASH, left_val, right_val);
    case planner::ExpressionType::OPERATOR_MOD:
      return codegen_->BinaryOp(parsing::Token::Type::PERCENT, left_val, right_val);
    default: {
      throw NotImplementedException(fmt::format("Translation of arithmetic type {}",
                                                planner::ExpressionTypeToString(expr_type, true)));
    }
  }
}

}  // namespace tpl::sql::codegen
