#include "sql/codegen/expression/null_check_translator.h"

#include "common/exception.h"
#include "sql/codegen/compilation_context.h"
#include "sql/codegen/work_context.h"

namespace tpl::sql::codegen {

NullCheckTranslator::NullCheckTranslator(const planner::OperatorExpression &expr,
                                         CompilationContext *compilation_context)
    : ExpressionTranslator(expr, compilation_context) {
  compilation_context->Prepare(*expr.GetChild(0));
}

ast::Expr *NullCheckTranslator::DeriveValue(WorkContext *ctx,
                                            const ColumnValueProvider *provider) const {
  auto codegen = GetCodeGen();
  auto input = ctx->DeriveValue(*GetExpression().GetChild(0), provider);
  switch (auto type = GetExpression().GetExpressionType()) {
    case planner::ExpressionType::OPERATOR_IS_NULL:
      return codegen->CallBuiltin(ast::Builtin::IsValNull, {input});
    case planner::ExpressionType::OPERATOR_IS_NOT_NULL:
      return codegen->UnaryOp(parsing::Token::Type::BANG,
                              codegen->CallBuiltin(ast::Builtin::IsValNull, {input}));
    default:
      throw NotImplementedException("Operator expression type {}",
                                    planner::ExpressionTypeToString(type, false));
  }
}

}  // namespace tpl::sql::codegen
