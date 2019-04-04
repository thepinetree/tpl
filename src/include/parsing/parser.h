#pragma once

#include <unordered_set>

#include "ast/ast.h"
#include "ast/ast_node_factory.h"
#include "ast/context.h"
#include "ast/identifier.h"
#include "parsing/scanner.h"
#include "sema/error_reporter.h"

namespace tpl::parsing {

class Parser {
 public:
  /// Build a parser instance using the given scanner and AST context
  /// \param scanner The scanner used to read input tokens
  /// \param context The context
  Parser(Scanner &scanner, ast::Context &context);

  /// This class cannot be copied or moved
  DISALLOW_COPY_AND_MOVE(Parser);

  /// Parse and generate an abstract syntax tree from the input TPL source code
  /// \return The generated AST
  ast::AstNode *Parse();

 private:
  // -------------------------------------------------------
  // Accessors
  // -------------------------------------------------------

  Scanner &scanner() { return scanner_; }

  ast::Context &context() { return context_; }

  ast::AstNodeFactory &node_factory() { return node_factory_; }

  util::Region *region() { return context().region(); }

  sema::ErrorReporter &error_reporter() { return error_reporter_; }

  // -------------------------------------------------------
  // Token logic
  // -------------------------------------------------------

  // Move to the next token in the stream
  Token::Type Next() { return scanner().Next(); }

  // Peek at the next token in the stream
  Token::Type peek() { return scanner().peek(); }

  void Consume(UNUSED Token::Type expected) {
    UNUSED Token::Type next = Next();
#ifndef NDEBUG
    if (next != expected) {
      error_reporter().Report(scanner().current_position(),
                              sema::ErrorMessages::kUnexpectedToken, next,
                              expected);
    }
#endif
  }

  // If the next token doesn't matched the given expected token, throw an error
  void Expect(Token::Type expected) {
    Token::Type next = Next();
    if (next != expected) {
      error_reporter().Report(scanner().current_position(),
                              sema::ErrorMessages::kUnexpectedToken, next,
                              expected);
    }
  }

  // If the next token matches the given expected token, consume it and return
  // true; otherwise, return false
  bool Matches(Token::Type expected) {
    if (peek() != expected) {
      return false;
    }

    Consume(expected);
    return true;
  }

  // Get the current symbol as an AST string
  ast::Identifier GetSymbol() {
    const std::string &literal = scanner().current_literal();
    return context().GetIdentifier(literal);
  }

  // In case of errors, sync up to any token in the list
  void Sync(std::unordered_set<Token::Type> &s);

  // -------------------------------------------------------
  // Parsing productions
  // -------------------------------------------------------

  ast::Decl *ParseDecl();

  ast::Decl *ParseFunctionDecl();

  ast::Decl *ParseStructDecl();

  ast::Decl *ParseVariableDecl();

  ast::Stmt *ParseStmt();

  ast::Stmt *ParseSimpleStmt();

  ast::Stmt *ParseBlockStmt();

  class ForHeader;

  ForHeader ParseForHeader();

  ast::Stmt *ParseForStmt();

  ast::Stmt *ParseIfStmt();

  ast::Stmt *ParseReturnStmt();

  ast::Expr *ParseExpr();

  ast::Expr *ParseBinaryOpExpr(u32 min_prec);

  ast::Expr *ParseUnaryOpExpr();

  ast::Expr *ParseLeftHandSideExpression();

  ast::Expr *ParsePrimaryExpr();

  ast::Expr *ParseFunctionLitExpr();

  ast::Expr *ParseType();

  ast::Expr *ParseFunctionType();

  ast::Expr *ParsePointerType();

  ast::Expr *ParseArrayType();

  ast::Expr *ParseStructType();

  ast::Expr *ParseMapType();

  ast::Attributes *ParseAttributes();

 private:
  // The source code scanner
  Scanner &scanner_;

  // The context
  ast::Context &context_;

  // A factory for all node types
  ast::AstNodeFactory &node_factory_;

  // The error reporter
  sema::ErrorReporter &error_reporter_;
};

}  // namespace tpl::parsing
