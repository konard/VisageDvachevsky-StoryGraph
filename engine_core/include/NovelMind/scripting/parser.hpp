#pragma once

/**
 * @file parser.hpp
 * @brief Parser for the NM Script language
 *
 * This module provides the Parser class which transforms a token
 * stream into an Abstract Syntax Tree (AST).
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ast.hpp"
#include "NovelMind/scripting/token.hpp"
#include <string>
#include <vector>

namespace NovelMind::scripting {

/**
 * @brief Parser error information
 */
struct ParseError {
  std::string message;
  SourceLocation location;

  ParseError() = default;
  ParseError(std::string msg, SourceLocation loc)
      : message(std::move(msg)), location(loc) {}
};

/**
 * @brief Parses NM Script tokens into an AST
 *
 * The Parser implements a recursive descent parser for the NM Script
 * language. It reads tokens from the lexer and produces an AST.
 *
 * Example usage:
 * @code
 * Parser parser;
 * auto result = parser.parse(tokens);
 * if (result.isOk()) {
 *     const Program& program = result.value();
 *     // Process AST
 * }
 * @endcode
 */
class Parser {
public:
  Parser();
  ~Parser();

  /**
   * @brief Parse tokens into an AST
   * @param tokens Vector of tokens from the lexer
   * @return Result containing the program AST or an error
   */
  [[nodiscard]] Result<Program> parse(const std::vector<Token> &tokens);

  /**
   * @brief Get all errors encountered during parsing
   */
  [[nodiscard]] const std::vector<ParseError> &getErrors() const;

private:
  // Token navigation
  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] const Token &peek() const;
  [[nodiscard]] const Token &previous() const;
  const Token &advance();
  bool check(TokenType type) const;
  bool match(TokenType type);
  bool match(std::initializer_list<TokenType> types);
  const Token &consume(TokenType type, const std::string &message);

  // Error handling
  void error(const std::string &message);
  void synchronize();
  void skipNewlines();

  // Grammar rules - declarations
  void parseDeclaration();
  CharacterDecl parseCharacterDecl();
  SceneDecl parseSceneDecl();

  // Grammar rules - statements
  StmtPtr parseStatement();
  StmtPtr parseShowStmt();
  StmtPtr parseHideStmt();
  StmtPtr parseSayStmt();
  StmtPtr parseChoiceStmt();
  StmtPtr parseIfStmt();
  StmtPtr parseGotoStmt();
  StmtPtr parseWaitStmt();
  StmtPtr parsePlayStmt();
  StmtPtr parseStopStmt();
  StmtPtr parseSetStmt();
  StmtPtr parseTransitionStmt();
  StmtPtr parseMoveStmt();
  StmtPtr parseBlock();

  // Grammar rules - expressions
  ExprPtr parseExpression();
  ExprPtr parseOr();
  ExprPtr parseAnd();
  ExprPtr parseEquality();
  ExprPtr parseComparison();
  ExprPtr parseTerm();
  ExprPtr parseFactor();
  ExprPtr parseUnary();
  ExprPtr parseCall();
  ExprPtr parsePrimary();

  // Helper parsers
  Position parsePosition();
  std::string parseString();
  std::vector<StmtPtr> parseStatementList();

  const std::vector<Token> *m_tokens;
  size_t m_current;
  std::vector<ParseError> m_errors;
  Program m_program;
};

} // namespace NovelMind::scripting
