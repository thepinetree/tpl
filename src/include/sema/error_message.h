#pragma once

#include <cstdint>

namespace tpl::sema {

/*
 * The following macro lists out all the semantic and syntactic error messages
 * in TPL. Each macro has three parts: a globally unique textual message ID, the
 * templated string message that will be displayed, and the types of each
 * template argument.
 */
#define MESSAGE_LIST(F)                                                   \
  F(UnexpectedToken, "unexpected token '%0', expecting '%1'",             \
    (parsing::Token::Type, parsing::Token::Type))                         \
  F(DuplicateArgName, "duplicate named argument '%0' in function '%0'",   \
    (const char *))                                                       \
  F(DuplicateStructFieldName, "duplicate field name '%0' in struct '%1'", \
    (const char *, const char *))                                         \
  F(AssignmentUsedAsValue, "assignment '%0' = '%1' used as value",        \
    (const char *, const char *))                                         \
  F(ExpectingExpression, "expecting expression", ())                      \
  F(ExpectingType, "expecting type", ())                                  \
  F(InvalidOperation, "invalid operation: '%0' on type '%1'",             \
    (parsing::Token::Type, const ast::AstString *))                       \
  F(VariableRedeclared, "'%0' redeclared in this block",                  \
    (const ast::AstString *))                                             \
  F(UndefinedVariable, "undefined: '%0'", (const ast::AstString *))       \
  F(NonFunction, "cannot call non-function '%0'", ())                     \
  F(NotEnoughCallArgs, "not enough arguments to call to '%0'", ())        \
  F(TooManyCallArgs, "too many arguments to call to '%0'", ())            \
  F(IncorrectCallArgType,                                                 \
    "cannot use a '%0' as type '%1' in argument to '%2'", ())             \
  F(NonBoolIfCondition, "non-bool used as if condition", ())              \
  F(NonBoolForCondition, "non-bool used as for condition", ())            \
  F(NonIntegerArrayLength, "non-integer literal used as array size", ())  \
  F(NegativeArrayLength, "array bound must be non-negative", ())  \
  F(ReturnOutsideFunction, "return outside function", ())

/**
 * Define the ErrorMessageId enumeration
 */
#define F(id, str, arg_types) id,
enum class ErrorMessageId : uint16_t { MESSAGE_LIST(F) };
#undef F

/**
 * A templated struct that captures the ID of an error message and the argument
 * types that must be supplied when the error is reported. The template
 * arguments allow us to type-check to make sure users are providing all info.
 */
template <typename... ArgTypes>
struct ErrorMessage {
  const ErrorMessageId id;
};

/**
 * A container for all TPL error messages
 */
class ErrorMessages {
  template <typename T>
  struct ReflectErrorMessageWithDetails;

  template <typename... ArgTypes>
  struct ReflectErrorMessageWithDetails<void(ArgTypes...)> {
    using type = ErrorMessage<ArgTypes...>;
  };

 public:
#define MSG(kind, str, arg_types)                                              \
  static constexpr const ReflectErrorMessageWithDetails<void(arg_types)>::type \
      k##kind = {ErrorMessageId::kind};
  MESSAGE_LIST(MSG)
#undef MSG
};

}  // namespace tpl::sema