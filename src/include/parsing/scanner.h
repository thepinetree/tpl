#pragma once

#include <cstdint>
#include <string>

#include "common/common.h"
#include "common/macros.h"
#include "parsing/token.h"

namespace tpl::parsing {

/**
 * Token scanner for TPL source.
 */
class Scanner {
  static constexpr int32_t kEndOfInput = -1;

  static constexpr int32_t kNewLine = '\n';

 public:
  /**
   * Construct a scanner over the given input string.
   * @param source Input TPL source.
   * @param source_len Length (in bytes) of input source.
   */
  Scanner(const char *source, uint64_t source_len);

  /**
   * Construct a scanner over the given input string @em source.
   * @param source The input TPL source.
   */
  explicit Scanner(const std::string &source);

  /**
   * This class cannot be copied or moved.
   */
  DISALLOW_COPY_AND_MOVE(Scanner);

  /**
   * Move the scanner ahead by one token.
   * @return The token that is read.
   */
  Token::Type Next();

  /**
   * Look ahead one token in the token stream without advancing the scanner.
   * @return The next token in the stream.
   */
  Token::Type Peek() const { return next_.type; }

  /**
   * @return The token the scanner is currently positioned at.
   */
  Token::Type CurrentToken() const { return curr_.type; }

  /**
   * @return The string value of the token at the current scan position. Valid only if the current
   *         token is a literal.
   */
  const std::string &CurrentLiteral() const { return curr_.literal; }

  /**
   * @return The position (line and column) of the scanner.
   */
  const SourcePosition &CurrentPosition() const { return curr_.pos; }

 private:
  // Scan the next token.
  void Scan();

  // Advance a single character into the source input stream.
  void Advance() {
    bool at_end = (offset_ == source_len_);
    if (TPL_UNLIKELY(at_end)) {
      c0_ = kEndOfInput;
      return;
    }

    // Not at end, bump
    c0_ = source_[offset_++];
    c0_pos_.column++;
  }

  // Advance until the given predicate returns false or we reach the end of the input.
  template <typename P>
  void AdvanceUntil(P &&p) {
    while (c0_ != kEndOfInput && !p(c0_)) {
      Advance();
    }
  }

  // Does the current character match the expected? If so, advance the scanner.
  bool Matches(int32_t expected) {
    if (c0_ != expected) {
      return false;
    }

    Advance();

    return true;
  }

  // Skip whitespace from the current token to the next valid token.
  void SkipWhiteSpace();

  // Skip a line comment.
  void SkipLineComment();

  // Skip a block comment.
  void SkipBlockComment();

  // Scan an identifier token.
  Token::Type ScanIdentifierOrKeyword();

  // Check if the given input is a keyword or an identifier.
  Token::Type CheckIdentifierOrKeyword(const char *input, uint32_t input_len);

  // Scan a number literal.
  Token::Type ScanNumber();

  // Scan a string literal.
  Token::Type ScanString();

  /**
   * This struct describes information about a single token, including its type,
   * its position in the origin source, and its literal value if it should have
   * one.
   */
  struct TokenDesc {
    Token::Type type;
    SourcePosition pos;
    uint64_t offset;
    std::string literal;
  };

  // -------------------------------------------------------
  // Static utilities
  // -------------------------------------------------------

  // Is the current character a character?
  static bool IsInRange(int32_t c, int32_t lower, int32_t upper) {
    return (c >= lower && c <= upper);
  }

  // Is the provided character an alphabetic character.
  static bool IsAlpha(int32_t c) { return IsInRange(c, 'a', 'z') || IsInRange(c, 'A', 'Z'); }

  // Is the current character a digit?
  static bool IsDigit(int32_t c) { return IsInRange(c, '0', '9'); }

  // Is this character allowed in an identifier?
  static bool IsIdentifierChar(int32_t c) { return IsAlpha(c) || IsDigit(c) || c == '_'; }

 private:
  // The source input and its length.
  const char *source_;
  uint64_t source_len_;

  // The offset/position in the source where the next character is read from.
  uint64_t offset_;

  // The lookahead character and its position in the source.
  int32_t c0_;
  SourcePosition c0_pos_;

  // Information about the current and next token in the input stream.
  TokenDesc curr_;
  TokenDesc next_;
};

}  // namespace tpl::parsing
