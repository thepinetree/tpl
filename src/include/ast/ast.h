#pragma once

#include <cstdint>

#include "llvm/Support/Casting.h"

#include "ast/identifier.h"
#include "parsing/token.h"
#include "util/common.h"
#include "util/region.h"
#include "util/region_containers.h"

namespace tpl {

namespace sema {
class Sema;
}  // namespace sema

namespace ast {

/*
 *
 */
#define FILE_NODE(T) T(File)

/*
 * All possible declarations
 *
 * If you add a new declaration node to either the beginning or end of the list,
 * remember to modify Decl::classof() to update the bounds check.
 */
#define DECLARATION_NODES(T) \
  T(FieldDecl)               \
  T(FunctionDecl)            \
  T(StructDecl)              \
  T(VariableDecl)

/*
 * All possible statements
 *
 * If you add a new statement node to either the beginning or end of the list,
 * remember to modify Statement::classof() to update the bounds check.
 */
#define STATEMENT_NODES(T) \
  T(AssignmentStmt)        \
  T(BlockStmt)             \
  T(DeclStmt)              \
  T(ExpressionStmt)        \
  T(ForStmt)               \
  T(ForInStmt)             \
  T(IfStmt)                \
  T(ReturnStmt)

/*
 * All possible expressions
 *
 * If you add a new expression node to either the beginning or end of the list,
 * remember to modify Expr::classof() to update the bounds check.
 */
#define EXPRESSION_NODES(T) \
  T(BadExpr)                \
  T(BinaryOpExpr)           \
  T(CallExpr)               \
  T(ComparisonOpExpr)       \
  T(FunctionLitExpr)        \
  T(IdentifierExpr)         \
  T(ImplicitCastExpr)       \
  T(LitExpr)                \
  T(SelectorExpr)           \
  T(UnaryOpExpr)            \
  /* Types */               \
  T(ArrayTypeRepr)          \
  T(FunctionTypeRepr)       \
  T(PointerTypeRepr)        \
  T(StructTypeRepr)

/*
 * All possible AST nodes
 */
#define AST_NODES(T)   \
  DECLARATION_NODES(T) \
  EXPRESSION_NODES(T)  \
  FILE_NODE(T)         \
  STATEMENT_NODES(T)

// Forward declare some base classes
class Decl;
class Expr;
class Stmt;
class Type;

// Forward declare all nodes
#define FORWARD_DECLARE(name) class name;
AST_NODES(FORWARD_DECLARE)
#undef FORWARD_DECLARE

////////////////////////////////////////////////////////////////////////////////
///
/// AST Nodes
///
////////////////////////////////////////////////////////////////////////////////

/**
 * The base class for all AST nodes
 */
class AstNode : public util::RegionObject {
 public:
  // The kind enumeration listing all possible node kinds
#define T(kind) kind,
  enum class Kind : u8 { AST_NODES(T) };
#undef T

  // The kind of this node
  Kind kind() const { return kind_; }

  // The position in the source where this element was found
  const SourcePosition &position() const { return pos_; }

  // This is mainly used in tests!
  const char *kind_name() const {
#define KIND_CASE(kind) \
  case Kind::kind:      \
    return #kind;

    // Main type switch
    // clang-format off
    switch (kind()) {
      default: { UNREACHABLE("Impossible kind name"); }
      AST_NODES(KIND_CASE)
    }
      // clang-format on
#undef KIND_CASE
  }

 public:
  // Checks if this node is an instance of the specified class
  template <typename T>
  bool Is() const {
    return llvm::isa<T>(this);
  }

  // Casts this node to an instance of the specified class, asserting if the
  // conversion is invalid. This is probably most similar to std::static_cast<>
  // or std::reinterpret_cast<>
  template <typename T>
  T *As() {
    TPL_ASSERT(Is<T>(), "Using unsafe cast on mismatched node types");
    return reinterpret_cast<T *>(this);
  }

  template <typename T>
  const T *As() const {
    TPL_ASSERT(Is<T>(), "Using unsafe cast on mismatched node types");
    return reinterpret_cast<const T *>(this);
  }

  // Casts this node to an instance of the provided class if valid. If the
  // conversion is invalid, this returns a NULL pointer. This is most similar to
  // std::dynamic_cast<T>, i.e., it's a checked cast.
  template <typename T>
  T *SafeAs() {
    return (Is<T>() ? As<T>() : nullptr);
  }

  template <typename T>
  const T *SafeAs() const {
    return (Is<T>() ? As<T>() : nullptr);
  }

#define F(kind) \
  bool Is##kind() const { return Is<kind>(); }
  AST_NODES(F)
#undef F

 protected:
  AstNode(Kind kind, const SourcePosition &pos) : kind_(kind), pos_(pos) {}

 private:
  // The kind of AST node
  Kind kind_;

  // The position in the original source where this node's underlying
  // information was found
  const SourcePosition pos_;
};

/**
 * Represents a file
 */
class File : public AstNode {
 public:
  File(const SourcePosition &pos, util::RegionVector<Decl *> &&decls)
      : AstNode(Kind::File, pos), decls_(std::move(decls)) {}

  util::RegionVector<Decl *> &declarations() { return decls_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::File;
  }

 private:
  util::RegionVector<Decl *> decls_;
};

////////////////////////////////////////////////////////////////////////////////
///
/// Decl nodes
///
////////////////////////////////////////////////////////////////////////////////

class Decl : public AstNode {
 public:
  Decl(Kind kind, const SourcePosition &pos, Identifier name, Expr *type_repr)
      : AstNode(kind, pos), name_(name), type_repr_(type_repr) {}

  Identifier name() const { return name_; }

  Expr *type_repr() const { return type_repr_; }

  static bool classof(const AstNode *node) {
    return node->kind() >= Kind::FieldDecl &&
           node->kind() <= Kind::VariableDecl;
  }

 private:
  Identifier name_;
  Expr *type_repr_;
};

/**
 * A generic declaration of a function argument or a field in a struct
 */
class FieldDecl : public Decl {
 public:
  FieldDecl(const SourcePosition &pos, Identifier name, Expr *type_repr)
      : Decl(Kind::FieldDecl, pos, name, type_repr) {}

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::FieldDecl;
  }
};

/**
 * A function declaration
 */
class FunctionDecl : public Decl {
 public:
  FunctionDecl(const SourcePosition &pos, Identifier name,
               FunctionLitExpr *fun);

  FunctionLitExpr *function() const { return fun_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::FunctionDecl;
  }

 private:
  FunctionLitExpr *fun_;
};

/**
 * A structure declaration
 */
class StructDecl : public Decl {
 public:
  StructDecl(const SourcePosition &pos, Identifier name,
             StructTypeRepr *type_repr);

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::StructDecl;
  }
};

/**
 * A variable declaration
 */
class VariableDecl : public Decl {
 public:
  VariableDecl(const SourcePosition &pos, Identifier name, Expr *type_repr,
               Expr *init)
      : Decl(Kind::VariableDecl, pos, name, type_repr), init_(init) {}

  Expr *initial() const { return init_; }

  bool HasInitialValue() const { return init_ != nullptr; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::VariableDecl;
  }

 private:
  Expr *init_;
};

////////////////////////////////////////////////////////////////////////////////
///
/// Statement nodes
///
////////////////////////////////////////////////////////////////////////////////

/**
 * Base class for all statement nodes
 */
class Stmt : public AstNode {
 public:
  Stmt(Kind kind, const SourcePosition &pos) : AstNode(kind, pos) {}

  static bool classof(const AstNode *node) {
    return node->kind() >= Kind::AssignmentStmt &&
           node->kind() <= Kind::ReturnStmt;
  }
};

/**
 * An assignment, dest = source
 */
class AssignmentStmt : public Stmt {
 public:
  AssignmentStmt(const SourcePosition &pos, Expr *dest, Expr *src)
      : Stmt(AstNode::Kind::AssignmentStmt, pos), dest_(dest), src_(src) {}

  Expr *destination() const { return dest_; }

  Expr *source() const { return src_; };

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::AssignmentStmt;
  }

 private:
  Expr *dest_;
  Expr *src_;
};

/**
 * A block of statements
 */
class BlockStmt : public Stmt {
 public:
  BlockStmt(const SourcePosition &pos, const SourcePosition &rbrace_pos,
            util::RegionVector<Stmt *> &&statements)
      : Stmt(Kind::BlockStmt, pos),
        rbrace_pos_(rbrace_pos),
        statements_(std::move(statements)) {}

  util::RegionVector<Stmt *> &statements() { return statements_; }

  const SourcePosition &right_brace_position() const { return rbrace_pos_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::BlockStmt;
  }

 private:
  const SourcePosition rbrace_pos_;

  util::RegionVector<Stmt *> statements_;
};

/**
 * A statement that is just a declaration
 */
class DeclStmt : public Stmt {
 public:
  explicit DeclStmt(Decl *decl)
      : Stmt(Kind::DeclStmt, decl->position()), decl_(decl) {}

  Decl *declaration() const { return decl_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::DeclStmt;
  }

 private:
  Decl *decl_;
};

/**
 * The bridge between statements and expressions
 */
class ExpressionStmt : public Stmt {
 public:
  explicit ExpressionStmt(Expr *expr);

  Expr *expression() { return expr_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ExpressionStmt;
  }

 private:
  Expr *expr_;
};

/**
 * Base class for all iteration-based statements
 */
class IterationStmt : public Stmt {
 public:
  IterationStmt(const SourcePosition &pos, AstNode::Kind kind, BlockStmt *body)
      : Stmt(kind, pos), body_(body) {}

  BlockStmt *body() const { return body_; }

  static bool classof(const AstNode *node) {
    return node->kind() >= Kind::ForStmt && node->kind() <= Kind::ForInStmt;
  }

 private:
  BlockStmt *body_;
};

/**
 * A for statement
 */
class ForStmt : public IterationStmt {
 public:
  ForStmt(const SourcePosition &pos, Stmt *init, Expr *cond, Stmt *next,
          BlockStmt *body)
      : IterationStmt(pos, AstNode::Kind::ForStmt, body),
        init_(init),
        cond_(cond),
        next_(next) {}

  Stmt *init() const { return init_; }

  Expr *condition() const { return cond_; }

  Stmt *next() const { return next_; }

  bool HasInitializer() const { return init_ != nullptr; }

  bool HasCondition() const { return cond_ != nullptr; }

  bool HasNext() const { return next_ != nullptr; }

  bool IsInfinite() const { return cond_ == nullptr; }

  bool IsLikeWhile() const {
    return init_ == nullptr && cond_ != nullptr && next_ == nullptr;
  }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ForStmt;
  }

 private:
  Stmt *init_;
  Expr *cond_;
  Stmt *next_;
};

/**
 * A range for statement
 */
class ForInStmt : public IterationStmt {
 public:
  ForInStmt(const SourcePosition &pos, Expr *target, Expr *iter,
            BlockStmt *body)
      : IterationStmt(pos, AstNode::Kind::ForInStmt, body),
        target_(target),
        iter_(iter) {}

  Expr *target() const { return target_; }

  Expr *iter() const { return iter_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ForInStmt;
  }

 private:
  Expr *target_;
  Expr *iter_;
};

/**
 * An if-then-else statement
 */
class IfStmt : public Stmt {
 public:
  IfStmt(const SourcePosition &pos, Expr *cond, BlockStmt *then_stmt,
         Stmt *else_stmt)
      : Stmt(Kind::IfStmt, pos),
        cond_(cond),
        then_stmt_(then_stmt),
        else_stmt_(else_stmt) {}

  Expr *condition() { return cond_; }

  BlockStmt *then_stmt() { return then_stmt_; }

  Stmt *else_stmt() { return else_stmt_; }

  bool HasElseStmt() const { return else_stmt_ != nullptr; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::IfStmt;
  }

 private:
  friend class sema::Sema;
  void set_condition(Expr *cond) {
    TPL_ASSERT(cond != nullptr, "Cannot set null condition");
    cond_ = cond;
  }

 private:
  Expr *cond_;
  BlockStmt *then_stmt_;
  Stmt *else_stmt_;
};

/**
 * A return statement
 */
class ReturnStmt : public Stmt {
 public:
  ReturnStmt(const SourcePosition &pos, Expr *ret)
      : Stmt(Kind::ReturnStmt, pos), ret_(ret) {}

  Expr *ret() { return ret_; }

  bool HasValue() const { return ret_ != nullptr; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ReturnStmt;
  }

 private:
  Expr *ret_;
};

////////////////////////////////////////////////////////////////////////////////
///
/// Expr nodes
///
////////////////////////////////////////////////////////////////////////////////

/**
 * Base class for all expression nodes
 */
class Expr : public AstNode {
 public:
  enum class Context : u8 {
    LValue,
    RValue,
    Test,
    Effect,
  };

  Expr(Kind kind, const SourcePosition &pos, Type *type = nullptr)
      : AstNode(kind, pos), type_(type) {}

  Type *type() { return type_; }
  const Type *type() const { return type_; }

  void set_type(Type *type) { type_ = type; }

  static bool classof(const AstNode *node) {
    return node->kind() >= Kind::BadExpr &&
           node->kind() <= Kind::StructTypeRepr;
  }

 private:
  Type *type_;
};

/**
 * A bad statement
 */
class BadExpr : public Expr {
 public:
  explicit BadExpr(const SourcePosition &pos)
      : Expr(AstNode::Kind::BadExpr, pos) {}

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::BadExpr;
  }
};

/**
 * A binary expression with non-null left and right children and an operator
 */
class BinaryOpExpr : public Expr {
 public:
  BinaryOpExpr(const SourcePosition &pos, parsing::Token::Type op, Expr *left,
               Expr *right)
      : Expr(Kind::BinaryOpExpr, pos), op_(op), left_(left), right_(right) {}

  parsing::Token::Type op() { return op_; }

  Expr *left() { return left_; }

  Expr *right() { return right_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::BinaryOpExpr;
  }

 private:
  friend class sema::Sema;

  void set_left(Expr *left) {
    TPL_ASSERT(left != nullptr, "Left cannot be null!");
    left_ = left;
  }

  void set_right(Expr *right) {
    TPL_ASSERT(right != nullptr, "Right cannot be null!");
    right_ = right;
  }

 private:
  parsing::Token::Type op_;
  Expr *left_;
  Expr *right_;
};

/**
 * A function call expression
 */
class CallExpr : public Expr {
 public:
  CallExpr(Expr *fun, util::RegionVector<Expr *> &&args)
      : Expr(Kind::CallExpr, fun->position()),
        fun_(fun),
        args_(std::move(args)) {}

  Expr *function() { return fun_; }

  util::RegionVector<Expr *> &arguments() { return args_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::CallExpr;
  }

 private:
  Expr *fun_;
  util::RegionVector<Expr *> args_;
};

/**
 * A binary comparison operator
 */
class ComparisonOpExpr : public Expr {
 public:
  ComparisonOpExpr(const SourcePosition &pos, parsing::Token::Type op,
                   Expr *left, Expr *right)
      : Expr(Kind::ComparisonOpExpr, pos),
        op_(op),
        left_(left),
        right_(right) {}

  parsing::Token::Type op() { return op_; }

  Expr *left() { return left_; }

  Expr *right() { return right_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ComparisonOpExpr;
  }

 private:
  friend class sema::Sema;

  void set_left(Expr *left) {
    TPL_ASSERT(left != nullptr, "Left cannot be null!");
    left_ = left;
  }

  void set_right(Expr *right) {
    TPL_ASSERT(right != nullptr, "Right cannot be null!");
    right_ = right;
  }

 private:
  parsing::Token::Type op_;
  Expr *left_;
  Expr *right_;
};

class FunctionLitExpr : public Expr {
 public:
  FunctionLitExpr(FunctionTypeRepr *type_repr, BlockStmt *body);

  FunctionTypeRepr *type_repr() const { return type_repr_; }

  BlockStmt *body() const { return body_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::FunctionLitExpr;
  }

 private:
  FunctionTypeRepr *type_repr_;
  BlockStmt *body_;
};

/**
 * A reference to a variable, function or struct
 */
class IdentifierExpr : public Expr {
 public:
  IdentifierExpr(const SourcePosition &pos, Identifier name)
      : Expr(Kind::IdentifierExpr, pos), name_(name), decl_(nullptr) {}

  Identifier name() const { return name_; }

  void BindTo(Decl *decl) { decl_ = decl; }

  bool is_bound() const { return decl_ != nullptr; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::IdentifierExpr;
  }

 private:
  // TODO(pmenon) Should these two be a union since only one should be active?
  // Pre-binding, 'name_' is used, and post-binding 'decl_' should be used?
  Identifier name_;
  Decl *decl_;
};

/**
 *
 */
class ImplicitCastExpr : public Expr {
 public:
  enum class CastKind {
    IntToSqlInt,
    IntToSqlDecimal,
    SqlBoolToBool,
  };

  CastKind cast_kind() const { return cast_kind_; }

  Expr *input() { return input_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ImplicitCastExpr;
  }

 private:
  friend class AstNodeFactory;

  ImplicitCastExpr(const SourcePosition &pos, CastKind cast_kind, Expr *input)
      : Expr(Kind::ImplicitCastExpr, pos),
        cast_kind_(cast_kind),
        input_(input) {}

 private:
  CastKind cast_kind_;
  Expr *input_;
};

/**
 * A literal in the original source code
 */
class LitExpr : public Expr {
 public:
  enum class LitKind : u8 { Nil, Boolean, Int, Float, String };

  explicit LitExpr(const SourcePosition &pos)
      : Expr(Kind::LitExpr, pos), lit_kind_(LitExpr::LitKind::Nil) {}

  LitExpr(const SourcePosition &pos, bool val)
      : Expr(Kind::LitExpr, pos),
        lit_kind_(LitExpr::LitKind::Boolean),
        boolean_(val) {}

  LitExpr(const SourcePosition &pos, LitExpr::LitKind lit_kind, Identifier str)
      : Expr(Kind::LitExpr, pos), lit_kind_(lit_kind), str_(str) {}

  LitExpr(const SourcePosition &pos, i32 num)
      : Expr(Kind::LitExpr, pos), lit_kind_(LitKind::Int), int32_(num) {}

  LitExpr(const SourcePosition &pos, f32 num)
      : Expr(Kind::LitExpr, pos), lit_kind_(LitKind::Float), float32_(num) {}

  LitExpr::LitKind literal_kind() const { return lit_kind_; }

  bool bool_val() const {
    TPL_ASSERT(literal_kind() == LitKind::Boolean,
               "Getting boolean value from a non-bool expression!");
    return boolean_;
  }

  Identifier raw_string_val() const {
    TPL_ASSERT(
        literal_kind() != LitKind::Nil && literal_kind() != LitKind::Boolean,
        "Getting a raw string value from a non-string or numeric value");
    return str_;
  }

  i32 int32_val() const {
    TPL_ASSERT(literal_kind() == LitKind::Int,
               "Getting integer value from a non-integer literal expression");
    return int32_;
  }

  f32 float32_val() const {
    TPL_ASSERT(literal_kind() == LitKind::Float,
               "Getting float value from a non-float literal expression");
    return float32_;
  }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::LitExpr;
  }

 private:
  LitKind lit_kind_;

  union {
    bool boolean_;
    Identifier str_;
    i8 int8_;
    i16 int16_;
    i32 int32_;
    i64 int64_;
    f32 float32_;
  };
};

/**
 * A selector expression for struct field access
 */
class SelectorExpr : public Expr {
 public:
  SelectorExpr(const SourcePosition &pos, Expr *obj, Expr *sel)
      : Expr(Kind::SelectorExpr, pos), object_(obj), selector_(sel) {}

  Expr *object() const { return object_; }
  Expr *selector() const { return selector_; }

  bool IsSugaredArrow() const;

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::SelectorExpr;
  }

 private:
  Expr *object_;
  Expr *selector_;
};

/**
 * A unary expression with a non-null inner expression and an operator
 */
class UnaryOpExpr : public Expr {
 public:
  UnaryOpExpr(const SourcePosition &pos, parsing::Token::Type op, Expr *expr)
      : Expr(Kind::UnaryOpExpr, pos), op_(op), expr_(expr) {}

  parsing::Token::Type op() { return op_; }

  Expr *expr() { return expr_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::UnaryOpExpr;
  }

 private:
  parsing::Token::Type op_;
  Expr *expr_;
};

////////////////////////////////////////////////////////////////////////////////
///
/// Type representation nodes. A type representation is a thin representation of
/// how the type appears in code. They are structurally the same as their full
/// blown Type counterparts, but we use the expressions to defer their type
/// resolution.
///
////////////////////////////////////////////////////////////////////////////////

/**
 * Array type
 */
class ArrayTypeRepr : public Expr {
 public:
  ArrayTypeRepr(const SourcePosition &pos, Expr *len, Expr *elem_type)
      : Expr(Kind::ArrayTypeRepr, pos), len_(len), elem_type_(elem_type) {}

  Expr *length() const { return len_; }

  Expr *element_type() const { return elem_type_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::ArrayTypeRepr;
  }

 private:
  Expr *len_;
  Expr *elem_type_;
};

/**
 * Function type
 */
class FunctionTypeRepr : public Expr {
 public:
  FunctionTypeRepr(const SourcePosition &pos,
                   util::RegionVector<FieldDecl *> &&param_types,
                   Expr *ret_type)
      : Expr(Kind::FunctionTypeRepr, pos),
        param_types_(std::move(param_types)),
        ret_type_(ret_type) {}

  const util::RegionVector<FieldDecl *> &parameters() const {
    return param_types_;
  }

  Expr *return_type() const { return ret_type_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::FunctionTypeRepr;
  }

 private:
  util::RegionVector<FieldDecl *> param_types_;
  Expr *ret_type_;
};

/**
 * Pointer type
 */
class PointerTypeRepr : public Expr {
 public:
  PointerTypeRepr(const SourcePosition &pos, Expr *base)
      : Expr(Kind::PointerTypeRepr, pos), base_(base) {}

  Expr *base() const { return base_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::PointerTypeRepr;
  }

 private:
  Expr *base_;
};

/**
 * Struct type
 */
class StructTypeRepr : public Expr {
 public:
  StructTypeRepr(const SourcePosition &pos,
                 util::RegionVector<FieldDecl *> &&fields)
      : Expr(Kind::StructTypeRepr, pos), fields_(std::move(fields)) {}

  const util::RegionVector<FieldDecl *> &fields() const { return fields_; }

  static bool classof(const AstNode *node) {
    return node->kind() == Kind::StructTypeRepr;
  }

 private:
  util::RegionVector<FieldDecl *> fields_;
};

}  // namespace ast
}  // namespace tpl