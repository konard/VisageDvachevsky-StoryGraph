#pragma once

/**
 * @file script_error.hpp
 * @brief Unified error reporting system for NM Script
 *
 * This module provides a comprehensive error reporting infrastructure
 * for the lexer, parser, validator, and compiler stages.
 *
 * Features:
 * - Location information (file, line, column)
 * - Source code context with visual indicators
 * - "Did you mean?" suggestions using Levenshtein distance
 * - Documentation links for error codes
 * - Rich formatting for CLI and editor display
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/token.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace NovelMind::scripting {

// =============================================================================
// String Similarity Utilities
// =============================================================================

/**
 * @brief Calculate Levenshtein (edit) distance between two strings
 *
 * This algorithm computes the minimum number of single-character edits
 * (insertions, deletions, substitutions) needed to transform one string
 * into another.
 *
 * @param s1 First string
 * @param s2 Second string
 * @return The edit distance between the two strings
 */
[[nodiscard]] inline size_t levenshteinDistance(const std::string& s1, const std::string& s2) {
  const size_t m = s1.size();
  const size_t n = s2.size();

  // Early exit for empty strings
  if (m == 0)
    return n;
  if (n == 0)
    return m;

  // Use two rows to save memory (O(min(m,n)) space instead of O(m*n))
  std::vector<size_t> prev(n + 1);
  std::vector<size_t> curr(n + 1);

  // Initialize first row
  for (size_t j = 0; j <= n; ++j) {
    prev[j] = j;
  }

  for (size_t i = 1; i <= m; ++i) {
    curr[0] = i;
    for (size_t j = 1; j <= n; ++j) {
      size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
      curr[j] = std::min({prev[j] + 1,        // deletion
                          curr[j - 1] + 1,    // insertion
                          prev[j - 1] + cost} // substitution
      );
    }
    std::swap(prev, curr);
  }

  return prev[n];
}

/**
 * @brief Find similar strings from a list of candidates
 *
 * Returns strings within a given edit distance threshold, sorted by
 * similarity (closest matches first).
 *
 * @param name The string to match against
 * @param candidates List of candidate strings
 * @param maxDistance Maximum edit distance to consider (default: 2)
 * @param maxResults Maximum number of results to return (default: 3)
 * @return Vector of similar strings, sorted by edit distance
 */
[[nodiscard]] inline std::vector<std::string>
findSimilarStrings(const std::string& name, const std::vector<std::string>& candidates,
                   size_t maxDistance = 2, size_t maxResults = 3) {
  std::vector<std::pair<size_t, std::string>> matches;

  for (const auto& candidate : candidates) {
    // Skip if lengths differ too much (early optimization)
    size_t lenDiff = (name.size() > candidate.size()) ? (name.size() - candidate.size())
                                                      : (candidate.size() - name.size());
    if (lenDiff > maxDistance)
      continue;

    size_t dist = levenshteinDistance(name, candidate);
    if (dist <= maxDistance && dist > 0) { // dist > 0 to exclude exact matches
      matches.emplace_back(dist, candidate);
    }
  }

  // Sort by distance (closest first)
  std::sort(matches.begin(), matches.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // Extract strings up to maxResults
  std::vector<std::string> result;
  for (size_t i = 0; i < std::min(matches.size(), maxResults); ++i) {
    result.push_back(matches[i].second);
  }

  return result;
}

// =============================================================================
// Source Context Extraction
// =============================================================================

/**
 * @brief Extract source code context around an error location
 *
 * Shows the error line with surrounding context and a visual indicator
 * (caret ^) pointing to the error column.
 *
 * @param source The full source code
 * @param line Error line number (1-based)
 * @param column Error column number (1-based)
 * @param contextLines Number of lines to show before and after error (default:
 * 2)
 * @return Formatted string with source context and caret indicator
 */
[[nodiscard]] inline std::string extractSourceContext(const std::string& source, u32 line,
                                                      u32 column, u32 contextLines = 2) {
  if (source.empty() || line == 0)
    return "";

  // Split source into lines
  std::vector<std::string> lines;
  std::istringstream stream(source);
  std::string lineStr;
  while (std::getline(stream, lineStr)) {
    lines.push_back(lineStr);
  }

  if (line > lines.size())
    return "";

  std::ostringstream result;

  // Calculate line number width for formatting
  u32 startLine = (line > contextLines) ? (line - contextLines) : 1;
  u32 endLine = std::min(static_cast<u32>(lines.size()), line + contextLines);
  size_t lineNumWidth = std::to_string(endLine).size(); // Width for line numbers

  // Show context lines before error
  for (u32 i = startLine; i <= endLine; ++i) {
    size_t idx = i - 1; // Convert to 0-based index
    if (idx >= lines.size())
      break;

    // Format line number with padding
    std::string lineNum = std::to_string(i);
    while (lineNum.size() < lineNumWidth) {
      lineNum = " " + lineNum;
    }

    // Mark the error line
    bool isErrorLine = (i == line);
    std::string marker = isErrorLine ? " > " : "   ";

    result << marker << lineNum << " | " << lines[idx] << "\n";

    // Add caret indicator for error line
    if (isErrorLine && column > 0) {
      std::string caretLine(lineNumWidth + 4, ' '); // "   " + lineNum + " | "
      caretLine += std::string(" | ");

      // Add spaces up to the error column, then the caret
      u32 caretPos = (column > 0) ? (column - 1) : 0;
      // Account for tab characters in the line
      size_t visualPos = 0;
      for (size_t j = 0; j < caretPos && j < lines[idx].size(); ++j) {
        if (lines[idx][j] == '\t') {
          visualPos += 4; // Assume tab width of 4
        } else {
          visualPos += 1;
        }
      }
      caretLine += std::string(visualPos, ' ');
      caretLine += "^";

      result << caretLine << "\n";
    }
  }

  return result.str();
}

// =============================================================================
// Error Documentation URLs
// =============================================================================

/**
 * @brief Base URL for error documentation
 */
constexpr const char* ERROR_DOCS_BASE_URL = "https://docs.novelmind.dev/errors/";

// =============================================================================
// Error Severity
// =============================================================================

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
 * @brief Get the severity string
 */
[[nodiscard]] inline const char* severityToString(Severity sev) {
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
 *
 * Features:
 * - File path and location (line:column)
 * - Source code context with visual indicators
 * - "Did you mean?" suggestions
 * - Documentation links
 * - Related information for cross-references
 */
struct ScriptError {
  ErrorCode code;
  Severity severity;
  std::string message;
  SourceSpan span;

  // File path where the error occurred
  std::optional<std::string> filePath;

  // The full source code (for context extraction)
  std::optional<std::string> source;

  // Related information (e.g., "defined here", "first used here")
  std::vector<RelatedInformation> relatedInfo;

  // Quick fix suggestions (e.g., "Did you mean 'Hero'?")
  std::vector<std::string> suggestions;

  ScriptError() = default;

  ScriptError(ErrorCode c, Severity sev, std::string msg, SourceLocation loc)
      : code(c), severity(sev), message(std::move(msg)), span(loc) {}

  ScriptError(ErrorCode c, Severity sev, std::string msg, SourceSpan s)
      : code(c), severity(sev), message(std::move(msg)), span(s) {}

  /**
   * @brief Add file path to this error
   */
  ScriptError& withFilePath(std::string path) {
    filePath = std::move(path);
    return *this;
  }

  /**
   * @brief Add related information to this error
   */
  ScriptError& withRelated(SourceLocation loc, std::string msg) {
    relatedInfo.emplace_back(loc, std::move(msg));
    return *this;
  }

  /**
   * @brief Add a suggestion for fixing this error
   */
  ScriptError& withSuggestion(std::string suggestion) {
    suggestions.push_back(std::move(suggestion));
    return *this;
  }

  /**
   * @brief Add source text context (full source code)
   */
  ScriptError& withSource(std::string src) {
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
   * @brief Get the error code as a string (e.g., "E3001")
   */
  [[nodiscard]] std::string errorCodeString() const {
    return "E" + std::to_string(static_cast<u32>(code));
  }

  /**
   * @brief Get the documentation URL for this error code
   */
  [[nodiscard]] std::string helpUrl() const {
    return std::string(ERROR_DOCS_BASE_URL) + errorCodeString();
  }

  /**
   * @brief Format error for simple display (single line)
   *
   * Example output:
   *   error[E3001] at script.nms:15:10: Undefined character 'Villian'
   */
  [[nodiscard]] std::string format() const {
    std::ostringstream ss;

    // Severity and error code
    ss << severityToString(severity) << "[" << errorCodeString() << "]";

    // Location
    ss << " at ";
    if (filePath.has_value()) {
      ss << filePath.value() << ":";
    }
    ss << span.start.line << ":" << span.start.column;

    // Message
    ss << ": " << message;

    return ss.str();
  }

  /**
   * @brief Format error with full context for display
   *
   * Example output:
   *   error[E3001] at scripts/intro.nms:23:10: Undefined character 'Villian'
   *
   *      21 | scene intro {
   *      22 |     show background "forest"
   *    > 23 |     say Villian "I am evil"
   *         |         ^
   *      24 | }
   *
   *   Did you mean 'Villain'? (declared at line 5)
   *
   *   See: https://docs.novelmind.dev/errors/E3001
   */
  [[nodiscard]] std::string formatRich() const {
    std::ostringstream ss;

    // Header line
    ss << format() << "\n";

    // Source context
    if (source.has_value() && !source.value().empty()) {
      ss << "\n";
      ss << extractSourceContext(source.value(), span.start.line, span.start.column);
    }

    // Related information
    for (const auto& related : relatedInfo) {
      ss << "\n";
      ss << "  note: " << related.message;
      ss << " (at line " << related.location.line << ":" << related.location.column << ")";
    }

    // Suggestions
    if (!suggestions.empty()) {
      ss << "\n";
      if (suggestions.size() == 1) {
        ss << "  suggestion: " << suggestions[0];
      } else {
        ss << "  suggestions:\n";
        for (size_t i = 0; i < suggestions.size(); ++i) {
          ss << "    " << (i + 1) << ". " << suggestions[i] << "\n";
        }
      }
    }

    // Help URL
    ss << "\n  See: " << helpUrl() << "\n";

    return ss.str();
  }
};

/**
 * @brief Collection of errors with helper methods
 */
class ErrorList {
public:
  void add(ScriptError error) { m_errors.push_back(std::move(error)); }

  void addError(ErrorCode code, const std::string& message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Error, message, loc);
  }

  void addWarning(ErrorCode code, const std::string& message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Warning, message, loc);
  }

  void addInfo(ErrorCode code, const std::string& message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Info, message, loc);
  }

  void addHint(ErrorCode code, const std::string& message, SourceLocation loc) {
    m_errors.emplace_back(code, Severity::Hint, message, loc);
  }

  [[nodiscard]] bool hasErrors() const {
    for (const auto& e : m_errors) {
      if (e.isError()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool hasWarnings() const {
    for (const auto& e : m_errors) {
      if (e.isWarning()) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] size_t errorCount() const {
    size_t count = 0;
    for (const auto& e : m_errors) {
      if (e.isError()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] size_t warningCount() const {
    size_t count = 0;
    for (const auto& e : m_errors) {
      if (e.isWarning()) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] const std::vector<ScriptError>& all() const { return m_errors; }

  [[nodiscard]] std::vector<ScriptError> errors() const {
    std::vector<ScriptError> result;
    for (const auto& e : m_errors) {
      if (e.isError()) {
        result.push_back(e);
      }
    }
    return result;
  }

  [[nodiscard]] std::vector<ScriptError> warnings() const {
    std::vector<ScriptError> result;
    for (const auto& e : m_errors) {
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
[[nodiscard]] inline const char* errorCodeDescription(ErrorCode code) {
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

} // namespace NovelMind::scripting
