#pragma once

/**
 * @file lexer.hpp
 * @brief Lexer for the NM Script language
 *
 * This module provides the Lexer class which tokenizes NM Script
 * source code into a stream of tokens for parsing.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/token.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NovelMind::scripting {

/**
 * @brief Lexer error information
 */
struct LexerError {
  std::string message;
  SourceLocation location;

  LexerError() = default;
  LexerError(std::string msg, SourceLocation loc)
      : message(std::move(msg)), location(loc) {}
};

/**
 * @brief Tokenizes NM Script source code
 *
 * The Lexer reads source code character by character and produces
 * a sequence of tokens. It handles comments, string literals,
 * numbers, identifiers, and keywords.
 *
 * Example usage:
 * @code
 * Lexer lexer;
 * auto result = lexer.tokenize("show Hero at center");
 * if (result.isOk()) {
 *     for (const auto& token : result.value()) {
 *         // process tokens
 *     }
 * }
 * @endcode
 */
class Lexer {
public:
  Lexer();
  ~Lexer();

  /**
   * @brief Tokenize source code into a vector of tokens
   * @param source The source code string to tokenize
   * @return Result containing tokens or an error
   */
  [[nodiscard]] Result<std::vector<Token>> tokenize(std::string_view source);

  /**
   * @brief Reset lexer state for reuse
   */
  void reset();

  /**
   * @brief Get all errors encountered during tokenization
   */
  [[nodiscard]] const std::vector<LexerError> &getErrors() const;

private:
  void initKeywords();

  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] unsigned char peek() const;
  [[nodiscard]] unsigned char peekNext() const;
  unsigned char advance();
  bool match(char expected);

  void skipWhitespace();
  void skipLineComment();
  void skipBlockComment();

  Token scanToken();
  Token makeToken(TokenType type);
  Token makeToken(TokenType type, const std::string &lexeme);
  Token errorToken(const std::string &message);

  Token scanString();
  Token scanNumber();
  Token scanIdentifier();
  Token scanColorLiteral();

  [[nodiscard]] TokenType identifierType(const std::string &lexeme) const;

  std::string_view m_source;
  size_t m_start;
  size_t m_current;
  u32 m_line;
  u32 m_column;
  u32 m_startColumn;

  std::vector<LexerError> m_errors;
  std::unordered_map<std::string, TokenType> m_keywords;
};

} // namespace NovelMind::scripting
