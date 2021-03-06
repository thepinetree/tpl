#pragma once

#include <array>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "llvm/ADT/StringMap.h"

#include "ast/builtins.h"
#include "ast/identifier.h"
#include "ast/type.h"
#include "common/common.h"
#include "parsing/token.h"
#include "sql/codegen/ast_fwd.h"
#include "sql/codegen/compilation_unit.h"
#include "sql/planner/expressions/expression_defs.h"
#include "sql/sql.h"
#include "util/region_containers.h"

namespace tpl::ast {
class AstNodeFactory;
}  // namespace tpl::ast

namespace tpl::sql::codegen {

class FunctionBuilder;

/**
 * Bundles convenience methods needed by other classes during code generation.
 */
class CodeGen {
  friend class If;
  friend class FunctionBuilder;
  friend class Loop;

  // The default number of cached scopes to keep around.
  static constexpr uint32_t kDefaultScopeCacheSize = 4;

  // Represents a lexical scope in TPL code. Holds a set of names (i.e., identifiers) and a link to
  // the outer scope. Note: this IS NOT an RAII object.
  class LexicalScope {
   public:
    // Create scope.
    explicit LexicalScope(LexicalScope *previous) : previous_(nullptr) { Init(previous); }

    // Initialize this scope.
    void Init(LexicalScope *previous) {
      previous_ = previous;
      names_.clear();
    }

    // Get a fresh name in this scope.
    std::string GetFreshName(std::string_view name);

    // Return the previous scope.
    LexicalScope *Previous() { return previous_; }

   private:
    // The outer scope.
    LexicalScope *previous_;
    // Map of identifier to next ID. Used to generate unique names.
    llvm::StringMap<uint64_t> names_;
  };

 public:
  /**
   * RAII scope class.
   */
  class CodeScope {
   public:
    /**
     * Create a scope.
     * @param codegen The code generator instance.
     */
    explicit CodeScope(CodeGen *codegen) : codegen_(codegen) { codegen_->EnterScope(); }

    /**
     * Destructor. Exits current scope.
     */
    ~CodeScope() { codegen_->ExitScope(); }

   private:
    CodeGen *codegen_;
  };

  /**
   * RAII container class.
   */
  class CodeContainerScope {
   public:
    CodeContainerScope(CodeGen *codegen, CompilationUnit *container)
        : codegen_(codegen), prev_container_(codegen_->container_) {
      codegen_->container_ = container;
    }

    ~CodeContainerScope() { codegen_->container_ = prev_container_; }

   private:
    // Code generation instance.
    CodeGen *codegen_;
    // The previous container.
    CompilationUnit *prev_container_;
  };

  /**
   * Create a code generator that generates code for the provided container.
   * @param container Where all code is generated into.
   */
  explicit CodeGen(CompilationUnit *container);

  /**
   * Destructor.
   */
  ~CodeGen();

  // ---------------------------------------------------------------------------
  //
  // Constant literals
  //
  // ---------------------------------------------------------------------------

  /**
   * @return A literal whose value is the provided boolean value.
   */
  [[nodiscard]] ast::Expression *ConstBool(bool val) const;

  /**
   * @return A literal whose value is the provided 8-bit signed integer.
   */
  [[nodiscard]] ast::Expression *Const8(int8_t val) const;

  /**
   * @return A literal whose value is the provided 16-bit signed integer.
   */
  [[nodiscard]] ast::Expression *Const16(int16_t val) const;

  /**
   * @return A literal whose value is the provided 32-bit signed integer.
   */
  [[nodiscard]] ast::Expression *Const32(int32_t val) const;

  /**
   * @return A literal whose value is the provided 64-bit signed integer.
   */
  [[nodiscard]] ast::Expression *Const64(int64_t val) const;

  /**
   * @return A literal whose value is the provided 64-bit floating point.
   */
  [[nodiscard]] ast::Expression *ConstDouble(double val) const;

  /**
   * @return A literal whose value is identical to the provided string.
   */
  [[nodiscard]] ast::Expression *ConstString(std::string_view str) const;

  // ---------------------------------------------------------------------------
  //
  // Type representations (not full TPL types !!)
  //
  // ---------------------------------------------------------------------------

  /**
   * @return The type representation for a primitive boolean value.
   */

  [[nodiscard]] ast::Expression *BoolType() const;

  /**
   * @return The type representation for an 8-bit signed integer (i.e., int8)
   */
  [[nodiscard]] ast::Expression *Int8Type() const;

  /**
   * @return The type representation for an 16-bit signed integer (i.e., int16)
   */
  [[nodiscard]] ast::Expression *Int16Type() const;

  /**
   * @return The type representation for an 32-bit signed integer (i.e., int32)
   */
  [[nodiscard]] ast::Expression *Int32Type() const;

  /**
   * @return The type representation for an unsigned 32-bit integer (i.e., uint32)
   */
  [[nodiscard]] ast::Expression *UInt32Type() const;

  /**
   * @return The type representation for an 64-bit signed integer (i.e., int64)
   */
  [[nodiscard]] ast::Expression *Int64Type() const;

  /**
   * @return The type representation for an 32-bit floating point number (i.e., float32)
   */
  [[nodiscard]] ast::Expression *Float32Type() const;

  /**
   * @return The type representation for an 64-bit floating point number (i.e., float64)
   */
  [[nodiscard]] ast::Expression *Float64Type() const;

  /**
   * @return A type representation expression that is "[num_elems]kind".
   */
  [[nodiscard]] ast::Expression *ArrayType(uint64_t num_elems, ast::BuiltinType::Kind kind);

  /**
   * @return The type representation for the provided builtin type.
   */
  [[nodiscard]] ast::Expression *BuiltinType(ast::BuiltinType::Kind builtin_kind) const;

  /**
   * @return The type representation for a TPL nil type.
   */
  [[nodiscard]] ast::Expression *Nil() const;

  /**
   * @return A type representation expression that is a pointer to the provided type.
   */
  [[nodiscard]] ast::Expression *PointerType(ast::Expression *base_type_repr) const;

  /**
   * @return A type representation expression that is a pointer to a named object.
   */
  [[nodiscard]] ast::Expression *PointerType(ast::Identifier type_name) const;

  /**
   * @return A type representation expression that is a pointer to the provided builtin type.
   */
  [[nodiscard]] ast::Expression *PointerType(ast::BuiltinType::Kind builtin) const;

  /**
   * Convert a SQL type into a runtime SQL value type representation expression.
   *
   * For example:
   * TypeId::Boolean = sql::Boolean
   * TypeId::BigInt = sql::Integer
   *
   * @param type The SQL type.
   * @return The corresponding TPL type.
   */
  [[nodiscard]] ast::Expression *TplType(TypeId type);

  /**
   * Convert the given type into a primitive TPL type.
   *
   * For example:
   * TypeId::Boolean = bool
   * TypeId::SmallInt = int16
   *
   * @param type The SQL type.
   * @return The corresponding primitive TPL type.
   */
  [[nodiscard]] ast::Expression *PrimitiveTplType(TypeId type);

  /**
   * Return the appropriate aggregate type for the given input aggregation expression.
   * @param agg_type The aggregate expression type.
   * @param ret_type The return type of the aggregate.
   * @return The corresponding TPL aggregate type.
   */
  [[nodiscard]] ast::Expression *AggregateType(planner::ExpressionType agg_type,
                                               TypeId ret_type) const;

  /**
   * @return An expression that represents the address of the provided object.
   */
  [[nodiscard]] ast::Expression *AddressOf(ast::Expression *obj) const;

  /**
   * @return An expression that represents the address of the provided object.
   */
  [[nodiscard]] ast::Expression *AddressOf(ast::Identifier obj_name) const;

  /**
   * @return An expression that represents the size of a type with the provided name, in bytes.
   */
  [[nodiscard]] ast::Expression *SizeOf(ast::Identifier type_name) const;

  /**
   * @return The offset of the given member within the given object.
   */
  [[nodiscard]] ast::Expression *OffsetOf(ast::Identifier obj, ast::Identifier member) const;

  /**
   * Perform a pointer case of the given expression into the provided type representation.
   * @param base The type to cast the expression into.
   * @param arg The expression to cast.
   * @return The result of the cast.
   */
  [[nodiscard]] ast::Expression *PtrCast(ast::Expression *base, ast::Expression *arg) const;

  /**
   * Perform a pointer case of the given argument into a pointer to the type with the provided
   * base name.
   * @param base_name The name of the base type.
   * @param arg The argument to the cast.
   * @return The result of the cast.
   */
  [[nodiscard]] ast::Expression *PtrCast(ast::Identifier base_name, ast::Expression *arg) const;

  // ---------------------------------------------------------------------------
  //
  // Declarations
  //
  // ---------------------------------------------------------------------------

  /**
   * Declare a variable with the provided name and type representation with no initial value.
   *
   * @param name The name of the variable to declare.
   * @param type_repr The provided type representation.
   * @return The variable declaration.
   */
  [[nodiscard]] ast::VariableDeclaration *DeclareVarNoInit(ast::Identifier name,
                                                           ast::Expression *type_repr);

  /**
   * Declare a variable with the provided name and builtin kind, but with no initial value.
   *
   * @param name The name of the variable.
   * @param kind The builtin kind of the declared variable.
   * @return The variable declaration.
   */
  [[nodiscard]] ast::VariableDeclaration *DeclareVarNoInit(ast::Identifier name,
                                                           ast::BuiltinType::Kind kind);

  /**
   * Declare a variable with the provided name and initial value. The variable's type will be
   * inferred from its initial value.
   *
   * @param name The name of the variable to declare.
   * @param init The initial value to assign the variable.
   * @return The variable declaration.
   */
  [[nodiscard]] ast::VariableDeclaration *DeclareVarWithInit(ast::Identifier name,
                                                             ast::Expression *init);

  /**
   * Declare a variable with the provided name, type representation, and initial value.
   *
   * Note: No check is performed to ensure the provided type representation matches the type of the
   *       provided initial expression here. That check will be done during semantic analysis. Thus,
   *       it's possible for users to pass wrong information here and for the call to return without
   *       error.
   *
   * @param name The name of the variable to declare.
   * @param type_repr The provided type representation of the variable.
   * @param init The initial value to assign the variable.
   * @return The variable declaration.
   */
  [[nodiscard]] ast::VariableDeclaration *DeclareVar(ast::Identifier name,
                                                     ast::Expression *type_repr,
                                                     ast::Expression *init);

  /**
   * Declare a struct with the provided name and struct field elements.
   *
   * @param name The name of the structure.
   * @param fields The fields constituting the struct.
   * @return The structure declaration.
   */
  ast::StructDeclaration *DeclareStruct(ast::Identifier name,
                                        util::RegionVector<ast::FieldDeclaration *> &&fields) const;

  // ---------------------------------------------------------------------------
  //
  // Assignments
  //
  // ---------------------------------------------------------------------------

  /**
   * Generate an assignment of the client-provide value to the provided destination.
   *
   * @param dest Where the value is stored.
   * @param value The value to assign.
   * @return The assignment statement.
   */
  [[nodiscard]] ast::Statement *Assign(ast::Expression *dest, ast::Expression *value);

  // ---------------------------------------------------------------------------
  //
  // Binary and comparison operations
  //
  // ---------------------------------------------------------------------------

  /**
   * Generate a binary operation of the provided operation type (<b>op</b>) between the
   * provided left and right operands, returning its result.
   * @param op The binary operation.
   * @param left The left input.
   * @param right The right input.
   * @return The result of the binary operation.
   */
  [[nodiscard]] ast::Expression *BinaryOp(parsing::Token::Type op, ast::Expression *left,
                                          ast::Expression *right) const;

  /**
   * Generate a comparison operation of the provided type between the provided left and right
   * operands, returning its result.
   * @param op The binary operation.
   * @param left The left input.
   * @param right The right input.
   * @return The result of the comparison.
   */
  [[nodiscard]] ast::Expression *Compare(parsing::Token::Type op, ast::Expression *left,
                                         ast::Expression *right) const;

  /**
   * Generate a comparison operation of the provided type between the provided left and right
   * operands, returning its result.
   * @param left The left input.
   * @param right The right input.
   * @return The result of the comparison.
   */
  [[nodiscard]] ast::Expression *CompareEq(ast::Expression *left, ast::Expression *right) const {
    return Compare(parsing::Token::Type::EQUAL_EQUAL, left, right);
  }

  /**
   * Generate a comparison of the given object pointer to 'nil', checking if it's a nil pointer.
   * @param obj The object to compare.
   * @return The boolean result of the comparison.
   */
  [[nodiscard]] ast::Expression *IsNilPointer(ast::Expression *obj) const;

  /**
   * Generate a unary operation of the provided operation type (<b>op</b>) on the provided input.
   * @param op The unary operation.
   * @param input The input.
   * @return The result of the unary operation.
   */
  [[nodiscard]] ast::Expression *UnaryOp(parsing::Token::Type op, ast::Expression *input) const;

  /**
   * @return The result of lhs & rhs.
   */
  [[nodiscard]] ast::Expression *BitAnd(ast::Expression *lhs, ast::Expression *rhs) const;

  /**
   * @return The result of lhs | rhs.
   */
  [[nodiscard]] ast::Expression *BitOr(ast::Expression *lhs, ast::Expression *rhs) const;

  /**
   * @return The result of val << num_bits.
   */
  [[nodiscard]] ast::Expression *BitShiftLeft(ast::Expression *val,
                                              ast::Expression *num_bits) const;

  /**
   * @return The result of val >> num_bits.
   */
  [[nodiscard]] ast::Expression *BitShiftRight(ast::Expression *val,
                                               ast::Expression *num_bits) const;

  /**
   * @return The result of left + right;
   */
  [[nodiscard]] ast::Expression *Add(ast::Expression *left, ast::Expression *right) const;

  /**
   * @return The result of left - right;
   */
  [[nodiscard]] ast::Expression *Sub(ast::Expression *left, ast::Expression *right) const;

  /**
   * @return The result of left * right;
   */
  [[nodiscard]] ast::Expression *Mul(ast::Expression *left, ast::Expression *right) const;

  // ---------------------------------------------------------------------------
  //
  // Struct/Array access
  //
  // ---------------------------------------------------------------------------

  /**
   * Generate an access to a member within an object/struct.
   * @param object The object to index into.
   * @param member The name of the struct member to access.
   * @return An expression accessing the desired struct member.
   */
  [[nodiscard]] ast::Expression *AccessStructMember(ast::Expression *object,
                                                    ast::Identifier member);

  /**
   * Create a return statement without a return value.
   * @return The statement.
   */
  [[nodiscard]] ast::Statement *Return();

  /**
   * Create a return statement that returns the given value.
   * @param ret The return value.
   * @return The statement.
   */
  [[nodiscard]] ast::Statement *Return(ast::Expression *ret);

  // ---------------------------------------------------------------------------
  //
  // Generic function calls and all builtins function calls.
  //
  // ---------------------------------------------------------------------------

  /**
   * Generate a call to the provided function by name and with the provided arguments.
   * @param func_name The name of the function to call.
   * @param args The arguments to pass in the call.
   */
  [[nodiscard]] ast::Expression *Call(ast::Identifier func_name,
                                      std::initializer_list<ast::Expression *> args) const;

  /**
   * Generate a call to the provided function by name and with the provided arguments.
   * @param func_name The name of the function to call.
   * @param args The arguments to pass in the call.
   */
  [[nodiscard]] ast::Expression *Call(ast::Identifier func_name,
                                      const std::vector<ast::Expression *> &args) const;

  /**
   * Generate a call to the provided builtin function and arguments.
   * @param builtin The builtin to call.
   * @param args The arguments to pass in the call.
   * @return The expression representing the call.
   */
  [[nodiscard]] ast::Expression *CallBuiltin(ast::Builtin builtin,
                                             std::initializer_list<ast::Expression *> args) const;

  /**
   * Generate a call to the provided function using the provided arguments. This function is almost
   * identical to the previous with the exception of the type of the arguments parameter. It's
   * an alternative API for callers that manually build up their arguments list.
   * @param builtin The builtin to call.
   * @param args The arguments to pass in the call.
   * @return The expression representing the call.
   */
  [[nodiscard]] ast::Expression *CallBuiltin(ast::Builtin builtin,
                                             const std::vector<ast::Expression *> &args) const;

  // ---------------------------------------------------------------------------
  //
  // SQL Value Functions
  //
  // ---------------------------------------------------------------------------

  /**
   * Call @boolToSql(). Convert a boolean into a SQL boolean.
   * @param b The constant bool.
   * @return The SQL bool.
   */
  [[nodiscard]] ast::Expression *BoolToSql(bool b) const;

  /**
   * Call @intToSql(). Convert a 64-bit integer into a SQL integer.
   * @param num The number to convert.
   * @return The SQL integer.
   */
  [[nodiscard]] ast::Expression *IntToSql(int64_t num) const;

  /**
   * Call @floatToSql(). Convert a 64-bit floating point number into a SQL real.
   * @param num The number to convert.
   * @return The SQL real.
   */
  [[nodiscard]] ast::Expression *FloatToSql(double num) const;

  /**
   * Call @dateToSql(). Convert a date into a SQL date.
   * @param date The date.
   * @return The SQL date.
   */
  [[nodiscard]] ast::Expression *DateToSql(Date date) const;

  /**
   * Call @dateToSql(). Convert a date into a SQL date.
   * @param year The number to convert.
   * @param month The number to convert.
   * @param day The number to convert.
   * @return The SQL date.
   */
  [[nodiscard]] ast::Expression *DateToSql(int32_t year, int32_t month, int32_t day) const;

  /**
   * Call @stringToSql(). Convert a string literal into a SQL string.
   * @param str The string.
   * @return The SQL varlen.
   */
  [[nodiscard]] ast::Expression *StringToSql(std::string_view str) const;

  /**
   * Perform a SQL cast.
   * @param input The input to the cast.
   * @param from_type The type of the input.
   * @param to_type The type to convert to.
   * @return
   */
  [[nodiscard]] ast::Expression *ConvertSql(ast::Expression *input, sql::TypeId from_type,
                                            sql::TypeId to_type) const;

  /**
   * Initialize the given SQL value as a SQL NULL.
   * @param val The SQL value to set.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *InitSqlNull(ast::Expression *val) const;

  // -------------------------------------------------------
  //
  // Table stuff
  //
  // -------------------------------------------------------

  /**
   * Call @tableIterInit(). Initializes a TableVectorIterator instance with a table ID.
   * @param tvi The table iterator variable.
   * @param table_name The name of the table to scan.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *TableIterInit(ast::Expression *tvi, std::string_view table_name);

  /**
   * Call @tableIterAdvance(). Attempt to advance the iterator, returning true if successful and
   * false otherwise.
   * @param tvi The table vector iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *TableIterAdvance(ast::Expression *tvi);

  /**
   * Call @tableIterGetVPI(). Retrieve the vector projection iterator from a table vector iterator.
   * @param tvi The table vector iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *TableIterGetVPI(ast::Expression *tvi);

  /**
   * Call @tableIterClose(). Close and destroy a table vector iterator.
   * @param tvi The table vector iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *TableIterClose(ast::Expression *tvi);

  /**
   * Call @iterateTableParallel(). Performs a parallel scan over the table with the provided name,
   * using the provided query state and thread-state container and calling the provided scan
   * function.
   * @param table_name The name of the table to scan.
   * @param query_state The query state pointer.
   * @param tls The thread state container.
   * @param worker_name The work function name.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *IterateTableParallel(std::string_view table_name,
                                                      ast::Expression *query_state,
                                                      ast::Expression *tls,
                                                      ast::Identifier worker_name);

  // -------------------------------------------------------
  //
  // VPI stuff
  //
  // -------------------------------------------------------

  /**
   * Call @vpiIsFiltered(). Determines if the provided vector projection iterator is filtered.
   * @param vpi The vector projection iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *VPIIsFiltered(ast::Expression *vpi);

  /**
   * Call @vpiHasNext(). Check if the provided VPI has more tuple data to iterate over.
   * @param vpi The vector projection iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *VPIHasNext(ast::Expression *vpi);

  /**
   * Call @vpiAdvance(). Advance the provided VPI to the next valid tuple.
   * @param vpi The vector projection iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *VPIAdvance(ast::Expression *vpi);

  /**
   * Call @vpiReset(). Reset the iterator.
   * @param vpi The vector projection iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *VPIReset(ast::Expression *vpi);

  /**
   * Call @vpiInit(). Initialize a new VPI using the provided vector projection. The last TID list
   * argument is optional and can be NULL.
   * @param vpi The vector projection iterator.
   * @param vp The vector projection.
   * @param tids The TID list.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *VPIInit(ast::Expression *vpi, ast::Expression *vp,
                                         ast::Expression *tids);

  /**
   * Call @vpiMatch(). Mark the current tuple the provided vector projection iterator is positioned
   * at as valid or invalid depending on the value of the provided condition.
   * @param vpi The vector projection iterator.
   * @param cond The boolean condition setting the current tuples filtration state.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *VPIMatch(ast::Expression *vpi, ast::Expression *cond);

  /**
   * Call @vpiGet[Type][Nullable](). Reads a value from the provided vector projection iterator of
   * the given type and NULL-ability flag, and at the provided column index.
   * @param vpi The vector projection iterator.
   * @param type_id The SQL type of the column value to read.
   * @param nullable NULL-ability flag.
   * @param idx The index of the column in the VPI to read.
   */
  [[nodiscard]] ast::Expression *VPIGet(ast::Expression *vpi, sql::TypeId type_id, bool nullable,
                                        uint32_t idx);

  /**
   * Call @filter[Comparison](). Invokes the vectorized filter on the provided vector projection
   * and column index, populating the results in the provided tuple ID list.
   * @param vp The vector projection.
   * @param comp_type The comparison type.
   * @param col_idx The index of the column in the vector projection to apply the filter on.
   * @param filter_val The filtering value.
   * @param The TID list.
   */
  ast::Expression *VPIFilter(ast::Expression *vp, planner::ExpressionType comp_type,
                             uint32_t col_idx, ast::Expression *filter_val, ast::Expression *tids);

  // -------------------------------------------------------
  //
  // Filter Manager stuff
  //
  // -------------------------------------------------------

  /**
   * Call @filterManagerInit(). Initialize the provided filter manager instance.
   * @param fm The filter manager pointer.
   */
  [[nodiscard]] ast::Expression *FilterManagerInit(ast::Expression *filter_manager);

  /**
   * Call @filterManagerFree(). Destroy and clean up the provided filter manager instance.
   * @param fm The filter manager pointer.
   */
  [[nodiscard]] ast::Expression *FilterManagerFree(ast::Expression *filter_manager);

  /**
   * Call @filterManagerInsert(). Insert a list of clauses.
   * @param fm The filter manager pointer.
   */
  [[nodiscard]] ast::Expression *FilterManagerInsert(
      ast::Expression *filter_manager, const std::vector<ast::Identifier> &clause_fn_names);

  /**
   * Call @filterManagerRun(). Runs all filters on the input vector projection iterator.
   * @param fm The filter manager pointer.
   * @param vpi The input vector projection iterator.
   */
  [[nodiscard]] ast::Expression *FilterManagerRunFilters(ast::Expression *filter_manager,
                                                         ast::Expression *vpi);

  /**
   * Call @execCtxGetMemPool(). Return the memory pool within an execution context.
   * @param exec_ctx The execution context variable.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *ExecCtxGetMemoryPool(ast::Expression *exec_ctx);

  /**
   * Call @execCtxGetTLS(). Return the thread state container within an execution context.
   * @param exec_ctx The name of the execution context variable.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *ExecCtxGetTLS(ast::Expression *exec_ctx);

  /**
   * Call @tlsGetCurrentThreadState(). Retrieves the current threads state in the state container.
   * It's assumed a previous call to @tlsReset() was made to specify the state parameters.
   * @param tls The state container pointer.
   * @param state_type_name The name of the expected state type to cast the result to.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *TLSAccessCurrentThreadState(ast::Expression *tls,
                                                             ast::Identifier state_type_name);

  /**
   * Call @tlsIterate(). Invokes the provided callback function for all thread-local state objects.
   * The context is provided as the first argument, and the thread-local state as the second.
   * @param tls A pointer to the thread state container.
   * @param context An opaque context object.
   * @param func The callback function.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *TLSIterate(ast::Expression *tls, ast::Expression *context,
                                            ast::Identifier func);

  /**
   * Call @tlsReset(). Reset the thread state container to a new state type with its own
   * initialization and tear-down functions.
   * @param tls The name of the thread state container variable.
   * @param tls_state_name The name of the thread state struct type.
   * @param init_fn The name of the initialization function.
   * @param tear_down_fn The name of the tear-down function.
   * @param context A context to pass along to each of the init and tear-down functions.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *TLSReset(ast::Expression *tls, ast::Identifier tls_state_name,
                                          ast::Identifier init_fn, ast::Identifier tear_down_fn,
                                          ast::Expression *context);

  /**
   * Call @tlsClear(). Clears all thread-local states.
   * @param tls The name of the thread state container variable.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *TLSClear(ast::Expression *tls);

  // -------------------------------------------------------
  //
  // Hash
  //
  // -------------------------------------------------------

  /**
   * Invoke @hash(). Hashes all input arguments and combines them into a single hash value.
   * @param values The values to hash.
   * @return The result of the hash.
   */
  [[nodiscard]] ast::Expression *Hash(const std::vector<ast::Expression *> &values);

  // -------------------------------------------------------
  //
  // Join stuff
  //
  // -------------------------------------------------------

  /**
   * Call @joinHTInit(). Initialize the provided join hash table using a memory pool and storing
   * the build-row structures with the provided name.
   * @param join_hash_table The join hash table.
   * @param mem_pool The memory pool.
   * @param build_row_type_name The name of the materialized build-side row in the hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableInit(ast::Expression *join_hash_table,
                                                   ast::Expression *mem_pool,
                                                   ast::Identifier build_row_type_name);

  /**
   * Call @joinHTInsert(). Allocates a new tuple in the join hash table with the given hash value.
   * The returned value is a pointer to an element with the given type.
   * @param join_hash_table The join hash table.
   * @param hash_val The hash value of the tuple that's to be inserted.
   * @param tuple_type_name The name of the struct type representing the tuple to be inserted.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableInsert(ast::Expression *join_hash_table,
                                                     ast::Expression *hash_val,
                                                     ast::Identifier tuple_type_name);

  /**
   * Call @joinHTBuild(). Performs the hash table build step of a hash join. Called on the provided
   * join hash table expected to be a *JoinHashTable.
   * @param join_hash_table The pointer to the join hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableBuild(ast::Expression *join_hash_table);

  /**
   * Call @joinHTBuildParallel(). Performs the parallel hash table build step of a hash join. Called
   * on the provided global join hash table (expected to be a *JoinHashTable), and a pointer to the
   * thread state container where thread-local join hash tables are stored at the given offset.
   * @param join_hash_table The global join hash table.
   * @param thread_state_container The thread state container.
   * @param offset The offset in the thread state container where thread-local tables are.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableBuildParallel(ast::Expression *join_hash_table,
                                                            ast::Expression *thread_state_container,
                                                            ast::Expression *offset);

  /**
   * Call @joinHTLookup(). Performs a single lookup into the hash table with a tuple with the
   * provided hash value. The returned entry points to the start of the bucket chain for the
   * given hash. It is the responsibility of the user to resolve hash and key collisions.
   * @param join_hash_table The join hash table.
   * @param hash_val The hash value of the probe key.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableLookup(ast::Expression *join_hash_table,
                                                     ast::Expression *hash_val);

  /**
   * Call @joinHTFree(). Cleanup and destroy the provided join hash table instance.
   * @param join_hash_table The join hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *JoinHashTableFree(ast::Expression *join_hash_table);

  /**
   * Call @htEntryGetHash(). Retrieves the hash value of the hash table entry.
   * @param entry The hash table entry.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *HTEntryGetHash(ast::Expression *entry);

  /**
   * Call @htEntryGetRow(). Retrieves a pointer to contents of the entry.
   * @param entry The hash table entry.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *HTEntryGetRow(ast::Expression *entry, ast::Identifier row_type);

  /**
   * Call @htEntryGetNext(). Return the next entry in the bucket chain.
   * @param entry The hash table entry.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *HTEntryGetNext(ast::Expression *entry);

  // -------------------------------------------------------
  //
  // Hash aggregation
  //
  // -------------------------------------------------------

  /**
   * Call @aggHTInit(). Initializes an aggregation hash table.
   * @param agg_ht A pointer to the aggregation hash table.
   * @param mem_pool A pointer to the memory pool.
   * @param agg_payload_type The name of the struct representing the aggregation payload.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableInit(ast::Expression *agg_ht,
                                                  ast::Expression *mem_pool,
                                                  ast::Identifier agg_payload_type);

  /**
   * Call @aggHTLookup(). Performs a single key lookup in an aggregation hash table. The hash value
   * is provided, as is a key check function to resolve hash collisions. The result of the lookup
   * is casted to the provided type.
   * @param agg_ht A pointer to the aggregation hash table.
   * @param hash_val The hash value of the key.
   * @param key_check The function to perform key-equality check.
   * @param input The probe aggregate values.
   * @param agg_payload_type The name of the struct representing the aggregation payload.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableLookup(ast::Expression *agg_ht,
                                                    ast::Expression *hash_val,
                                                    ast::Identifier key_check,
                                                    ast::Expression *input,
                                                    ast::Identifier agg_payload_type);

  /**
   * Call @aggHTInsert(). Inserts a new entry into the aggregation hash table. The result of the
   * insertion is casted to the provided type.
   * @param agg_ht A pointer to the aggregation hash table.
   * @param hash_val The hash value of the key.
   * @param partitioned Whether the insertion is in partitioned mode.
   * @param agg_payload_type The name of the struct representing the aggregation payload.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableInsert(ast::Expression *agg_ht,
                                                    ast::Expression *hash_val, bool partitioned,
                                                    ast::Identifier agg_payload_type);

  /**
   * Call @aggHTLink(). Directly inserts a new partial aggregate into the provided aggregation hash
   * table.
   * @param agg_ht A pointer to the aggregation hash table.
   * @param entry A pointer to the hash table entry storing the partial aggregate data.
   * @retur The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableLinkEntry(ast::Expression *agg_ht,
                                                       ast::Expression *entry);

  /**
   * Call @aggHTMoveParts(). Move all overflow partitions stored in thread-local aggregation hash
   * tables at the given offset inside the provided thread state container into the global hash
   * table.
   * @param agg_ht A pointer to the global aggregation hash table.
   * @param tls A pointer to the thread state container.
   * @param tl_agg_ht_offset The offset in the state container where the thread-local aggregation
   *                         hash tables.
   * @param merge_partitions_fn_name The name of the merging function to merge partial aggregates
   *                                 into the global hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableMovePartitions(
      ast::Expression *agg_ht, ast::Expression *tls, ast::Expression *tl_agg_ht_offset,
      ast::Identifier merge_partitions_fn_name);

  /**
   * Call @aggHTParallelPartScan(). Performs a parallel partitioned scan over an aggregation hash
   * table, using the provided worker function as a callback.
   * @param agg_ht A pointer to the global hash table.
   * @param query_state A pointer to the query state.
   * @param thread_state_container A pointer to the thread state.
   * @param worker_fn The name of the function used to scan over a partition of the hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableParallelScan(ast::Expression *agg_ht,
                                                          ast::Expression *query_state,
                                                          ast::Expression *thread_state_container,
                                                          ast::Identifier worker_fn);

  /**
   * Call @aggHTFree(). Cleans up and destroys the provided aggregation hash table.
   * @param agg_ht A pointer to the aggregation hash table.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableFree(ast::Expression *agg_ht);

  /**
   * Call @aggHTPartIterHasNext(). Determines if the provided overflow partition iterator has more
   * data.
   * @param iter A pointer to the overflow partition iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggPartitionIteratorHasNext(ast::Expression *iter);

  /**
   * Call @aggHTPartIterNext(). Advanced the iterator by one element.
   * @param iter A pointer to the overflow partition iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggPartitionIteratorNext(ast::Expression *iter);

  /**
   * Call @aggHTPartIterGetHash(). Returns the hash value of the entry the iterator is currently
   * positioned at.
   * @param iter A pointer to the overflow partition iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggPartitionIteratorGetHash(ast::Expression *iter);

  /**
   * Call @aggHTPartIterGetRow(). Returns a pointer to the aggregate payload struct of the entry the
   * iterator is currently positioned at.
   * @param iter A pointer to the overflow partition iterator.
   * @param agg_payload_type The type of aggregate payload.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggPartitionIteratorGetRow(ast::Expression *iter,
                                                            ast::Identifier agg_payload_type);

  /**
   * Call @aggHTPartIterGetRowEntry(). Returns a pointer to the current hash table entry.
   * @param iter A pointer to the overflow partition iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggPartitionIteratorGetRowEntry(ast::Expression *iter);

  /**
   * Call @aggHTIterInit(). Initializes an aggregation hash table iterator.
   * @param iter A pointer to the iterator.
   * @param agg_ht A pointer to the hash table to iterate over.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableIteratorInit(ast::Expression *iter,
                                                          ast::Expression *agg_ht);

  /**
   * Call @aggHTIterHasNExt(). Determines if the given iterator has more data.
   * @param iter A pointer to the iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableIteratorHasNext(ast::Expression *iter);

  /**
   * Call @aggHTIterNext(). Advances the iterator by one element.
   * @param iter A pointer to the iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableIteratorNext(ast::Expression *iter);

  /**
   * Call @aggHTIterGetRow(). Returns a pointer to the aggregate payload the iterator is currently
   * positioned at.
   * @param iter A pointer to the iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableIteratorGetRow(ast::Expression *iter,
                                                            ast::Identifier agg_payload_type);

  /**
   * Call @aggHTIterClose(). Cleans up and destroys the given iterator.
   * @param iter A pointer to the iterator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggHashTableIteratorClose(ast::Expression *iter);

  /**
   * Call @aggInit(). Initializes and aggregator.
   * @param agg A pointer to the aggregator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggregatorInit(ast::Expression *agg);

  /**
   * Call @aggAdvance(). Advanced an aggregator with the provided input value.
   * @param agg A pointer to the aggregator.
   * @param val The input value.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggregatorAdvance(ast::Expression *agg, ast::Expression *val);

  /**
   * Call @aggMerge(). Merges two aggregators storing the result in the first argument.
   * @param agg1 A pointer to the aggregator.
   * @param agg2 A pointer to the aggregator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggregatorMerge(ast::Expression *agg1, ast::Expression *agg2);

  /**
   * Call @aggResult(). Finalizes and returns the result of the aggregation.
   * @param agg A pointer to the aggregator.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *AggregatorResult(ast::Expression *agg);

  // -------------------------------------------------------
  //
  // Sorter stuff
  //
  // -------------------------------------------------------

  /**
   * Call @sorterInit(). Initialize the provided sorter instance using a memory pool, comparison
   * function and the struct that will be materialized into the sorter instance.
   * @param sorter The sorter instance.
   * @param mem_pool The memory pool instance.
   * @param cmp_func_name The name of the comparison function to use.
   * @param sort_row_type_name The name of the materialized sort-row type.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterInit(ast::Expression *sorter, ast::Expression *mem_pool,
                                            ast::Identifier cmp_func_name,
                                            ast::Identifier sort_row_type_name);

  /**
   * Call @sorterInsert(). Prepare an insert into the provided sorter whose type is the given type.
   * @param sorter The sorter instance.
   * @param sort_row_type_name The name of the TPL type that will be stored in the sorter.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterInsert(ast::Expression *sorter,
                                              ast::Identifier sort_row_type_name);

  /**
   * Call @sorterInsertTopK(). Prepare a top-k insert into the provided sorter whose type is the
   * given type.
   * @param sorter The sorter instance.
   * @param sort_row_type_name The name of the TPL type that will be stored in the sorter.
   * @param top_k The top-k value.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterInsertTopK(ast::Expression *sorter,
                                                  ast::Identifier sort_row_type_name,
                                                  uint64_t top_k);

  /**
   * Call @sorterInsertTopK(). Complete a previous top-k insert into the provided sorter.
   * @param sorter The sorter instance.
   * @param top_k The top-k value.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterInsertTopKFinish(ast::Expression *sorter, uint64_t top_k);

  /**
   * Call @sorterSort().  Sort the provided sorter instance.
   * @param sorter The sorter instance.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterSort(ast::Expression *sorter);

  /**
   * Call @sorterSortParallel(). Perform a parallel sort of all sorter instances contained in the
   * provided thread-state  container at the given offset, merging sorter results into a central
   * sorter instance.
   * @param sorter The central sorter instance that will contain the results of the sort.
   * @param tls The thread state container.
   * @param offset The offset within the container where the thread-local sorter is.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SortParallel(ast::Expression *sorter, ast::Expression *tls,
                                              ast::Expression *offset);

  /**
   * Call @sorterSortTopKParallel(). Perform a parallel top-k sort of all sorter instances contained
   * in the provided thread-local container at the given offset.
   * @param sorter The central sorter instance that will contain the results of the sort.
   * @param tls The thread-state container.
   * @param offset The offset within the container where the thread-local sorters are.
   * @param top_k The top-K value.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SortTopKParallel(ast::Expression *sorter, ast::Expression *tls,
                                                  ast::Expression *offset, std::size_t top_k);

  /**
   * Call @sorterFree(). Destroy the provided sorter instance.
   * @param sorter The sorter instance.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *SorterFree(ast::Expression *sorter);

  /**
   * Call @sorterIterInit(). Initialize the provided sorter iterator over the given sorter.
   * @param iter The sorter iterator.
   * @param sorter The sorter instance to iterate.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterInit(ast::Expression *iter, ast::Expression *sorter);

  /**
   * Call @sorterIterHasNext(). Check if the sorter iterator has more data.
   * @param iter The iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterHasNext(ast::Expression *iter);

  /**
   * Call @sorterIterNext(). Advances the sorter iterator one tuple.
   * @param iter The iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterNext(ast::Expression *iter);

  /**
   * Call @sorterIterSkipRows(). Skips N rows in the provided sorter iterator.
   * @param iter The iterator.
   * @param n The number of rows to skip.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterSkipRows(ast::Expression *iter, uint32_t n);

  /**
   * Call @sorterIterGetRow(). Retrieves a pointer to the current iterator row casted to the
   * provided row type.
   * @param iter The iterator.
   * @param row_type_name The name of the TPL type that will be stored in the sorter.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterGetRow(ast::Expression *iter,
                                                  ast::Identifier row_type_name);

  /**
   * Call @sorterIterClose(). Destroy and cleanup the provided sorter iterator instance.
   * @param iter The sorter iterator.
   * @return The call expression.
   */
  [[nodiscard]] ast::Expression *SorterIterClose(ast::Expression *iter);

  /**
   * Call @like(). Implements the SQL LIKE() operation.
   * @param str The input string.
   * @param pattern The input pattern.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *Like(ast::Expression *str, ast::Expression *pattern);

  /**
   * Invoke !@like(). Implements the SQL NOT LIKE() operation.
   * @param str The input string.
   * @param pattern The input pattern.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *NotLike(ast::Expression *str, ast::Expression *pattern);

  // ---------------------------------------------------------------------------
  //
  // CSV
  //
  // ---------------------------------------------------------------------------

  /**
   * Call @csvReaderInit(). Initialize a CSV reader for the given file name.
   * @param reader The reader.
   * @param file_name The filename.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *CSVReaderInit(ast::Expression *reader, std::string_view file_name);

  /**
   * Call @csvReaderAdvance(). Advance the reader one row.
   * @param reader The reader.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *CSVReaderAdvance(ast::Expression *reader);

  /**
   * Call @csvReaderGetField(). Read the field at the given index in the current row.
   * @param reader The reader.
   * @param field_index The index of the field in the row to read.
   * @param result Where the result is written to.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *CSVReaderGetField(ast::Expression *reader, uint32_t field_index,
                                                   ast::Expression *result);

  /**
   * Call @csvReaderGetRecordNumber(). Get the current record number.
   * @param reader The reader.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *CSVReaderGetRecordNumber(ast::Expression *reader);

  /**
   * Call @csvReaderClose(). Destroy a CSV reader.
   * @param reader The reader.
   * @return The call.
   */
  [[nodiscard]] ast::Expression *CSVReaderClose(ast::Expression *reader);

  // ---------------------------------------------------------------------------
  //
  // Identifiers
  //
  // ---------------------------------------------------------------------------

  /**
   * @return A new unique identifier using the given string as a prefix.
   */
  ast::Identifier MakeFreshIdentifier(std::string_view str);

  /**
   * @return An identifier whose contents are identical to the input string.
   */
  ast::Identifier MakeIdentifier(std::string_view str) const;

  // ---------------------------------------------------------------------------
  //
  // Generic
  //
  // ---------------------------------------------------------------------------

  /**
   * @return A new identifier expression representing the given identifier.
   */
  ast::Expression *MakeExpr(ast::Identifier ident) const;

  /**
   * @return The variable declaration as a standalone statement.
   */
  ast::Statement *MakeStatement(ast::VariableDeclaration *var) const;

  /**
   * @return The expression as a standalone statement.
   */
  ast::Statement *MakeStatement(ast::Expression *expr) const;

  /**
   * @return An empty list of statements.
   */
  ast::BlockStatement *MakeEmptyBlock() const;

  /**
   * @return An empty list of fields.
   */
  util::RegionVector<ast::FieldDeclaration *> MakeEmptyFieldList() const;

  /**
   * @return A field list with the given fields.
   */
  util::RegionVector<ast::FieldDeclaration *> MakeFieldList(
      std::initializer_list<ast::FieldDeclaration *> fields) const;

  /**
   * Create a single field.
   * @param name The name of the field.
   * @param type The type representation of the field.
   * @return The field declaration.
   */
  ast::FieldDeclaration *MakeField(ast::Identifier name, ast::Expression *type) const;

  /**
   * @return The current (potentially null) function being built.
   */
  FunctionBuilder *GetCurrentFunction() const { return function_; }

  /**
   * @return The current source code position.
   */
  const SourcePosition &GetPosition() const { return position_; }

  /**
   * Move to a new line.
   */
  void NewLine() { position_.line++; }

  /**
   * Increase current indentation level.
   */
  void Indent() { position_.column += 4; }

  /**
   * Decrease Remove current indentation level.
   */
  void UnIndent() { position_.column -= 4; }

 private:
  // Enter a new lexical scope.
  void EnterScope();

  // Exit the current lexical scope.
  void ExitScope();

  // Return the context.
  ast::Context *Context() const { return container_->Context(); }

  // Return the AST node factory.
  ast::AstNodeFactory *NodeFactory() const;

 private:
  // The container for the current compilation.
  CompilationUnit *container_;
  // The current position in the source.
  SourcePosition position_;
  // Cache of code scopes.
  uint32_t num_cached_scopes_;
  std::array<std::unique_ptr<LexicalScope>, kDefaultScopeCacheSize> scope_cache_ = {nullptr};
  // Current scope.
  LexicalScope *scope_;
  // The current function being generated.
  FunctionBuilder *function_;
};

}  // namespace tpl::sql::codegen
