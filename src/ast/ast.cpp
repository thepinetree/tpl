#include "ast/ast.h"

namespace tpl::ast {

FunctionType *FunctionDeclaration::type() { return fun_->type(); }

ExpressionStatement::ExpressionStatement(Expression *expr)
    : Statement(Kind::ExpressionStatement, expr->position()), expr_(expr) {}

FunctionLiteralExpression::FunctionLiteralExpression(FunctionType *type,
                                                     BlockStatement *body)
    : Expression(Kind::FunctionLiteralExpression, type->position()),
      type_(type),
      body_(body) {}

}  // namespace tpl::ast