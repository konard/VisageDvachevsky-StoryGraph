#pragma once

/**
 * @file nm_fuzzy_matcher.hpp
 * @brief Fuzzy string matching algorithm for command palette search
 *
 * Implements a fuzzy matching algorithm similar to VS Code's quick open,
 * allowing users to find commands/panels by typing partial matches.
 *
 * Example:
 *   "scp" matches "Scene Palette"
 *   "stgr" matches "Story Graph"
 *   "consol" matches "Console"
 */

#include <QString>
#include <QVector>

namespace NovelMind::editor::qt {

/**
 * @brief Result of a fuzzy match operation
 */
struct FuzzyMatchResult {
  bool matched = false;           ///< Whether the pattern matched
  int score = 0;                  ///< Match quality score (higher is better)
  QVector<int> matchedIndices;    ///< Character indices that matched

  /**
   * @brief Comparison operator for sorting by score
   */
  bool operator<(const FuzzyMatchResult &other) const {
    return score > other.score; // Higher scores come first
  }
};

/**
 * @brief Fuzzy string matching utility
 *
 * Provides fuzzy matching capabilities for searching through commands,
 * panels, and other searchable items in the editor.
 */
class NMFuzzyMatcher {
public:
  /**
   * @brief Perform fuzzy matching between pattern and text
   *
   * The algorithm awards points for:
   * - Sequential character matches
   * - Matching at word boundaries (camelCase, PascalCase, spaces)
   * - Matching at the start of the string
   * - Case-sensitive matches
   *
   * @param pattern Search pattern (user input)
   * @param text Text to search in
   * @return Match result with score and matched character positions
   */
  static FuzzyMatchResult match(const QString &pattern, const QString &text);

  /**
   * @brief Check if pattern matches text (simple boolean check)
   *
   * @param pattern Search pattern
   * @param text Text to search in
   * @return true if pattern matches
   */
  static bool matches(const QString &pattern, const QString &text);

  /**
   * @brief Calculate match score for sorting
   *
   * Used when you already know there's a match and just need the score.
   *
   * @param pattern Search pattern
   * @param text Text to search in
   * @return Match score (0 if no match)
   */
  static int calculateScore(const QString &pattern, const QString &text);

private:
  /**
   * @brief Check if a character is at a word boundary
   *
   * Word boundaries include:
   * - Start of string
   * - Character after space
   * - Uppercase letter after lowercase (camelCase)
   * - Character after special character
   *
   * @param text The text
   * @param index Index of character to check
   * @return true if at word boundary
   */
  static bool isWordBoundary(const QString &text, int index);

  /**
   * @brief Calculate bonus points for matching at a specific position
   *
   * @param text The text
   * @param index Index where match occurred
   * @param isConsecutive Whether this match follows the previous match
   * @return Bonus points
   */
  static int getPositionBonus(const QString &text, int index,
                              bool isConsecutive);
};

} // namespace NovelMind::editor::qt
