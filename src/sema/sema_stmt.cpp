#include "sema/sema.h"

#include "ast/ast_node_factory.h"
#include "ast/context.h"
#include "ast/type.h"
#include "sql/catalog.h"
#include "sql/table.h"

namespace tpl::sema {

void Sema::VisitAssignmentStmt(ast::AssignmentStmt *node) {
  ast::Type *src_type = Resolve(node->source());
  ast::Type *dest_type = Resolve(node->destination());

  if (src_type == nullptr || dest_type == nullptr) {
    return;
  }

  // Fast-path
  if (src_type == dest_type) {
    return;
  }

  /*
   * The left and right types are no the same. If both types are integral, see
   * if we can issue an integral cast.
   */

  if (src_type->IsIntegerType() || dest_type->IsIntegerType()) {
    auto *cast_expr = context()->node_factory().NewImplicitCastExpr(
        node->source()->position(), ast::CastKind::IntegralCast, dest_type,
        node->source());
    node->set_source(cast_expr);
    return;
  }
}

void Sema::VisitBlockStmt(ast::BlockStmt *node) {
  SemaScope block_scope(this, Scope::Kind::Block);

  for (auto *stmt : node->statements()) {
    Visit(stmt);
  }
}

void Sema::VisitFile(ast::File *node) {
  SemaScope file_scope(this, Scope::Kind::File);

  for (auto *decl : node->declarations()) {
    Visit(decl);
  }
}

void Sema::VisitForStmt(ast::ForStmt *node) {
  // Create a new scope for variables introduced in initialization block
  SemaScope for_scope(this, Scope::Kind::Loop);

  if (node->init() != nullptr) {
    Visit(node->init());
  }

  if (node->condition() != nullptr) {
    ast::Type *cond_type = Resolve(node->condition());
    if (!cond_type->IsBoolType()) {
      error_reporter().Report(node->condition()->position(),
                              ErrorMessages::kNonBoolForCondition);
    }
  }

  if (node->next() != nullptr) {
    Visit(node->next());
  }

  // The body
  Visit(node->body());
}

void Sema::VisitForInStmt(ast::ForInStmt *node) {
  SemaScope for_scope(this, Scope::Kind::Loop);

  if (!node->target()->IsIdentifierExpr()) {
    error_reporter().Report(node->target()->position(),
                            ErrorMessages::kNonIdentifierTargetInForInLoop);
    return;
  }

  if (!node->iter()->IsIdentifierExpr()) {
    error_reporter().Report(node->iter()->position(),
                            ErrorMessages::kNonIdentifierIterator);
    return;
  }

  auto *target = node->target()->As<ast::IdentifierExpr>();
  auto *iter = node->iter()->As<ast::IdentifierExpr>();

  // Lookup the table in the catalog
  auto *table = sql::Catalog::Instance()->LookupTableByName(iter->name());
  if (table == nullptr) {
    error_reporter().Report(iter->position(), ErrorMessages::kNonExistingTable,
                            iter->name());
    return;
  }

  //
  // Now we resolve the type of the iterable. If the user wanted a row-at-a-time
  // iteration, the type becomes a struct-equivalent representation of the row
  // as stored in the table. If the user wanted a vector-at-a-time iteration,
  // the iterable type becomes a VectorProjectionIterator.
  //

  ast::Type *iter_type = nullptr;
  if (auto *attributes = node->attributes();
      attributes != nullptr &&
      attributes->Contains(context()->GetIdentifier("batch"))) {
    iter_type = ast::InternalType::Get(
        context(), ast::InternalType::InternalKind::VectorProjectionIterator);
  } else {
    iter_type = ConvertSchemaToType(table->schema());
    TPL_ASSERT(iter_type->IsStructType(), "Rows must be structs");
  }

  // Set the target iterators type
  target->set_type(iter_type);

  // Declare iteration variable
  current_scope()->Declare(target->name(), iter_type);

  // Process body
  Visit(node->body());
}

void Sema::VisitExpressionStmt(ast::ExpressionStmt *node) {
  Visit(node->expression());
}

void Sema::VisitIfStmt(ast::IfStmt *node) {
  if (ast::Type *cond_type = Resolve(node->condition()); cond_type == nullptr) {
    // Error
    return;
  }

  /*
   * If the result type of the evaluated condition is a SQL boolean value, we
   * implicitly cast it to a native boolean value before we feed it into the
   * if-condition.
   */

  if (auto *type = node->condition()->type()->SafeAs<ast::SqlType>();
      type != nullptr && type->sql_type().type_id() == sql::TypeId::Boolean) {
    ast::Expr *cond = node->condition();
    cond = context()->node_factory().NewImplicitCastExpr(
        cond->position(), ast::CastKind::SqlBoolToBool,
        ast::BoolType::Get(context()), cond);
    cond->set_type(ast::BoolType::Get(context()));
    node->set_condition(cond);
  }

  if (!node->condition()->type()->IsBoolType()) {
    error_reporter().Report(node->condition()->position(),
                            ErrorMessages::kNonBoolIfCondition);
  }

  Visit(node->then_stmt());

  if (node->else_stmt() != nullptr) {
    Visit(node->else_stmt());
  }
}

void Sema::VisitDeclStmt(ast::DeclStmt *node) { Visit(node->declaration()); }

void Sema::VisitReturnStmt(ast::ReturnStmt *node) {
  if (current_function() == nullptr) {
    error_reporter().Report(node->position(),
                            ErrorMessages::kReturnOutsideFunction);
    return;
  }

  /*
   * If there's an expression with the return clause, resolve it now. We'll
   * check later if we need it.
   */

  ast::Type *return_type = nullptr;
  if (node->HasExpressionValue()) {
    return_type = Resolve(node->ret());
  }

  /*
   * If the function has a nil-type, we just need to make sure this return
   * statement doesn't have an attached expression. If it does, that's an error
   */

  auto *func_type = current_function()->type()->As<ast::FunctionType>();

  if (func_type->return_type()->IsNilType()) {
    if (return_type != nullptr) {
      error_reporter().Report(node->position(),
                              ErrorMessages::kMismatchedReturnType, return_type,
                              func_type->return_type());
    }
    return;
  }

  /*
   * The function has a non-nil return type. So, we need to make sure the
   * resolved type of the expression in this return is compatible with the
   * return type of the function.
   */

  if (return_type != func_type->return_type()) {
    // It's possible the return type is null (either because there was an error
    // or there wasn't an expression)
    if (return_type == nullptr) {
      return_type = ast::NilType::Get(context());
    }

    error_reporter().Report(node->position(),
                            ErrorMessages::kMismatchedReturnType, return_type,
                            func_type->return_type());
    return;
  }
}

}  // namespace tpl::sema
