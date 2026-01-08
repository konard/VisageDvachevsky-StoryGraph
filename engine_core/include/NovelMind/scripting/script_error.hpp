#pragma once

/**
 * @file script_error.hpp
 * @brief Unified error reporting system for NM Script
 *
 * This module provides a comprehensive error reporting infrastructure
 * for the lexer, parser, validator, and compiler stages.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/token.hpp"
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::scripting {

/**
 * @brief Severity level for script errors
 */
enum class Severity : u8 {
  Hint,    // Suggestions for improvement
  Info,    // Informational messages
  Warning, // Potential issues that don't prevent compilation
  Error    // Errors that prevent successful compilation
};

/**
 * @brief Error codes for script diagnostics
 *
 * Organized by category:
 * - 1xxx: Lexer errors
 * - 2xxx: Parser errors
 * - 3xxx: Validation errors (semantic)
 * - 4xxx: Compiler errors
 * - 5xxx: Runtime errors
 */
enum class ErrorCode : u32 {
  // Lexer errors (1xxx)
  UnexpectedCharacter = 1001,
  UnterminatedString = 1002,
  InvalidNumber = 1003,
  InvalidEscapeSequence = 1004,
  UnterminatedComment = 1005,

  // Parser errors (2xxx)
  UnexpectedToken = 2001,
  ExpectedIdentifier = 2002,
  ExpectedExpression = 2003,
  ExpectedStatement = 2004,
  ExpectedLeftBrace = 2005,
  ExpectedRightBrace = 2006,
  ExpectedLeftParen = 2007,
  ExpectedRightParen = 2008,
  ExpectedString = 2009,
  InvalidSyntax = 2010,

  // Validation errors - Characters (3xxx)
  UndefinedCharacter = 3001,
  DuplicateCharacterDefinition = 3002,
  UnusedCharacter = 3003,

  // Validation errors - Scenes (31xx)
  UndefinedScene = 3101,
  DuplicateSceneDefinition = 3102,
  UnusedScene = 3103,
  EmptyScene = 3104,
  UnreachableScene = 3105,

  // Validation errors - Variables (32xx)
  UndefinedVariable = 3201,
  UnusedVariable = 3202,
  VariableRedefinition = 3203,
  UninitializedVariable = 3204,

  // Validation errors - Control flow (33xx)
  DeadCode = 3301,
  InfiniteLoop = 3302,
  UnreachableCode = 3303,
  MissingReturn = 3304,
  InvalidGotoTarget = 3305,

  // Validation errors - Type (34xx)
  TypeMismatch = 3401,
  InvalidOperandTypes = 3402,
  InvalidConditionType = 3403,

  // Validation errors - Resources (35xx)
  UndefinedResource = 3501,
  InvalidResourcePath = 3502,
  MissingSceneFile = 3503,
  MissingSceneObject = 3504,
  MissingAssetFile = 3505,

  // Validation errors - Choice (36xx)
  EmptyChoiceBlock = 3601,
  DuplicateChoiceText = 3602,
  ChoiceWithoutBranch = 3603,

  // Compiler errors (4xxx)
  CompilationFailed = 4001,
  TooManyConstants = 4002,
  TooManyVariables = 4003,
  JumpTargetOutOfRange = 4004,
  InvalidOpcode = 4005,

  // Runtime errors (5xxx)
  StackOverflow = 5001,
  StackUnderflow = 5002,
  DivisionByZero = 5003,
  InvalidInstruction = 5004,
  ResourceLoadFailed = 5005
};

/**
 * @brief Represents a source span for multi-character error regions
 */
struct SourceSpan {
  SourceLocation start;
  SourceLocation end;

  SourceSpan() = default;
  SourceSpan(SourceLocation s, SourceLocation e) : start(s), end(e) {}
  explicit SourceSpan(SourceLocation loc) : start(loc), end(loc) {}
};

/**
 * @brief Additional context for errors (related locations, hints)
 */
struct RelatedInformation {
  SourceLocation location;
  std::string message;

  RelatedInformation() = default;
  RelatedInformation(SourceLocation loc, std::string msg)
      : location(loc), message(std::move(msg)) {}
};

/**
 * @brief Represents a complete script error/diagnostic
 *
 * This structure contains all information needed for comprehensive
 * error reporting in both editor and CLI contexts.
 */
struct ScriptError {
  ErrorCode code;
  Severity severity;
  std::string message;
  SourceSpan span;
  std::optional<std::string> source; // The source line/text if available

  // Related information (e.g., "defined here", "first used here")
  std::vector<RelatedInformation> relatedInfo;

  // Quick fix suggestions
  std::vector<std::string> suggestions;

  ScriptError() = default;

  ScriptError(ErrorCode c, Severity sev, std::string msg, SourceLocation loc)
      : code(c), severity(sev), message(std::move(msg)), span(loc) {}

  ScriptError(ErrorCode c, Severity sev, std::string msg, SourceSpan s)
      : code(c), severity(sev), message(std::move(msg)), span(s) {}

  /**
   * @brief Add related information to this error
   */
  ScriptError &withRelated(SourceLocation loc, std::string msg) {
    relatedInfo.emplace_back(loc, std::move(msg));
    return *this;
  }

  /**
   * @brief Add a suggestion for fixing this error
   */
  ScriptError &withSuggestion(std::string suggestion) {
    suggestions.push_back(std::move(suggestion));
    return *this;
  }

  /**
   * @brief Add source text context
   */
  ScriptError &withSource(std::string src) {
    source = std::move(src);
    return *this;
  }

  /**
   * @brief Check if this is an error (vs warning/info)
   */
  [[nodiscard]] bool isError() const { return severity == Severity::Error; }

  /**
   * @brief Check if this is a warning
   */
  [[nodiscard]] bool isWarning() const { return severity == Severity::Warning; }

  /**
   * @brief Format error for display
   */
  [[nodiscard]] std::string format() const {
    std::string result;

    // Severity prefix
    switch (severity) {
    case Severity::Hint:
      result = "hint";
      break;
    case Severity::Info:
      result = "info";
      break;
    case Severity::Warning:
      result = "warning";
      break;
    case Severity::Error:
      result = "error";
      break;
    }

    // Location
    result += "[" + std::to_string(span.start.line) + ":" +
              std::to_string(span.start.column) + "]: ";

    // Message
    result += message;

    // Error code
    result += " [E" + std::to_string(static_cast<u32>(code)) + "]";

    return result;
  }
};

/**
 * @brief Collection of errors with helper methods
 */
class ErrorList {
public:
  void add(ScriptError error) { m_errors.push_back(std::move(error)); }

  void addError(ErrorCode code, const std::string &message,
                SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Error, message, loc);
  }

  void addWarning(ErrorCode code, const std::string &message,
                  SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Warning, message, loc);
  }

  void addInfo(ErrorCode code, const std::string &message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Info, message, loc);
  }

  void addHint(ErrorCode code, const std::string &message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Hint, message, loc);
  }

  [[nodiscard]] bool hasErrors() const {
    for (const auto &e : m_errors) {
      if (e.isError()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool hasWarnings() const {
    for (const auto &e : m_errors) {
      if (e.isWarning()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] size_t errorCount() const {
    size_t count = 0;
    for (const auto &e : m_errors) {
      if (e.isError()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] size_t warningCount() const {
    size_t count = 0;
    for (const auto &e : m_errors) {
      if (e.isWarning()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] const std::vector<ScriptError> &all() const { return m_errors; }

  [[nodiscard]] std::vector<ScriptError> errors() const {
    std::vector<ScriptError> result;
    for (const auto &e : m_errors) {
      if (e.isError()) {
        result.push_back(e);
      }
    }
    return result;
  }

  [[nodiscard]] std::vector<ScriptError> warnings() const {
    std::vector<ScriptError> result;
    for (const auto &e : m_errors) {
      if (e.isWarning()) {
        result.push_back(e);
      }
    }
    return result;
  }

  void clear() { m_errors.clear(); }

  [[nodiscard]] bool empty() const { return m_errors.empty(); }

  [[nodiscard]] size_t size() const { return m_errors.size(); }

private:
  std::vector<ScriptError> m_errors;
};

/**
 * @brief Get human-readable description for an error code
 */
[[nodiscard]] inline const char *errorCodeDescription(ErrorCode code) {
  switch (code) {
  // Lexer errors
  case ErrorCode::UnexpectedCharacter:
    return "Unexpected character";
  case ErrorCode::UnterminatedString:
    return "Unterminated string literal";
  case ErrorCode::InvalidNumber:
    return "Invalid number format";
  case ErrorCode::InvalidEscapeSequence:
    return "Invalid escape sequence";
  case ErrorCode::UnterminatedComment:
    return "Unterminated block comment";

  // Parser errors
  case ErrorCode::UnexpectedToken:
    return "Unexpected token";
  case ErrorCode::ExpectedIdentifier:
    return "Expected identifier";
  case ErrorCode::ExpectedExpression:
    return "Expected expression";
  case ErrorCode::ExpectedStatement:
    return "Expected statement";
  case ErrorCode::ExpectedLeftBrace:
    return "Expected '{'";
  case ErrorCode::ExpectedRightBrace:
    return "Expected '}'";
  case ErrorCode::ExpectedLeftParen:
    return "Expected '('";
  case ErrorCode::ExpectedRightParen:
    return "Expected ')'";
  case ErrorCode::ExpectedString:
    return "Expected string";
  case ErrorCode::InvalidSyntax:
    return "Invalid syntax";

  // Validation - Characters
  case ErrorCode::UndefinedCharacter:
    return "Undefined character";
  case ErrorCode::DuplicateCharacterDefinition:
    return "Duplicate character definition";
  case ErrorCode::UnusedCharacter:
    return "Unused character";

  // Validation - Scenes
  case ErrorCode::UndefinedScene:
    return "Undefined scene";
  case ErrorCode::DuplicateSceneDefinition:
    return "Duplicate scene definition";
  case ErrorCode::UnusedScene:
    return "Unused scene";
  case ErrorCode::EmptyScene:
    return "Empty scene";
  case ErrorCode::UnreachableScene:
    return "Unreachable scene";

  // Validation - Variables
  case ErrorCode::UndefinedVariable:
    return "Undefined variable";
  case ErrorCode::UnusedVariable:
    return "Unused variable";
  case ErrorCode::VariableRedefinition:
    return "Variable redefinition";
  case ErrorCode::UninitializedVariable:
    return "Use of uninitialized variable";

  // Validation - Control flow
  case ErrorCode::DeadCode:
    return "Dead code detected";
  case ErrorCode::InfiniteLoop:
    return "Possible infinite loop";
  case ErrorCode::UnreachableCode:
    return "Unreachable code";
  case ErrorCode::MissingReturn:
    return "Missing return statement";
  case ErrorCode::InvalidGotoTarget:
    return "Invalid goto target";

  // Validation - Type
  case ErrorCode::TypeMismatch:
    return "Type mismatch";
  case ErrorCode::InvalidOperandTypes:
    return "Invalid operand types";
  case ErrorCode::InvalidConditionType:
    return "Invalid condition type";

  // Validation - Resources
  case ErrorCode::UndefinedResource:
    return "Undefined resource";
  case ErrorCode::InvalidResourcePath:
    return "Invalid resource path";
  case ErrorCode::MissingSceneFile:
    return "Missing scene file";
  case ErrorCode::MissingSceneObject:
    return "Missing scene object";
  case ErrorCode::MissingAssetFile:
    return "Missing asset file";

  // Validation - Choice
  case ErrorCode::EmptyChoiceBlock:
    return "Empty choice block";
  case ErrorCode::DuplicateChoiceText:
    return "Duplicate choice text";
  case ErrorCode::ChoiceWithoutBranch:
    return "Choice without branch";

  // Compiler errors
  case ErrorCode::CompilationFailed:
    return "Compilation failed";
  case ErrorCode::TooManyConstants:
    return "Too many constants";
  case ErrorCode::TooManyVariables:
    return "Too many variables";
  case ErrorCode::JumpTargetOutOfRange:
    return "Jump target out of range";
  case ErrorCode::InvalidOpcode:
    return "Invalid opcode";

  // Runtime errors
  case ErrorCode::StackOverflow:
    return "Stack overflow";
  case ErrorCode::StackUnderflow:
    return "Stack underflow";
  case ErrorCode::DivisionByZero:
    return "Division by zero";
  case ErrorCode::InvalidInstruction:
    return "Invalid instruction";
  case ErrorCode::ResourceLoadFailed:
    return "Resource load failed";

  default:
    return "Unknown error";
  }
}

/**
 * @brief Get the severity string
 */
[[nodiscard]] inline const char *severityToString(Severity sev) {
  switch (sev) {
  case Severity::Hint:
    return "hint";
  case Severity::Info:
    return "info";
  case Severity::Warning:
    return "warning";
  case Severity::Error:
    return "error";
  }
  return "unknown";
}

} // namespace NovelMind::scripting
