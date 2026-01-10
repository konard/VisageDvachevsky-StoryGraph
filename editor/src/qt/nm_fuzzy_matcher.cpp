#include "NovelMind/editor/qt/nm_fuzzy_matcher.hpp"

namespace NovelMind::editor::qt {

// Scoring constants
static constexpr int SCORE_MATCH = 10;              // Base score for any match
static constexpr int SCORE_CONSECUTIVE = 15;        // Bonus for consecutive matches
static constexpr int SCORE_WORD_BOUNDARY = 20;      // Bonus for word boundary
static constexpr int SCORE_START = 25;              // Bonus for start of string
static constexpr int SCORE_CASE_MATCH = 5;          // Bonus for case match
static constexpr int PENALTY_GAP = -2;              // Penalty for each gap

FuzzyMatchResult NMFuzzyMatcher::match(const QString &pattern,
                                       const QString &text) {
  FuzzyMatchResult result;

  // Empty pattern matches everything with zero score
  if (pattern.isEmpty()) {
    result.matched = true;
    result.score = 0;
    return result;
  }

  // Pattern longer than text cannot match
  if (pattern.length() > text.length()) {
    return result;
  }

  // Case-insensitive search
  const QString patternLower = pattern.toLower();
  const QString textLower = text.toLower();

  // Find all characters of pattern in text
  int patternIndex = 0;
  int textIndex = 0;
  int score = 0;
  int lastMatchIndex = -1;
  QVector<int> matchedIndices;

  while (patternIndex < patternLower.length() &&
         textIndex < textLower.length()) {
    if (patternLower[patternIndex] == textLower[textIndex]) {
      // Found a match
      matchedIndices.append(textIndex);

      // Calculate score
      score += SCORE_MATCH;

      // Consecutive match bonus
      bool isConsecutive = (lastMatchIndex >= 0 && textIndex == lastMatchIndex + 1);
      if (isConsecutive) {
        score += SCORE_CONSECUTIVE;
      }

      // Position-based bonuses
      score += getPositionBonus(textLower, textIndex, isConsecutive);

      // Case match bonus
      if (pattern[patternIndex] == text[textIndex]) {
        score += SCORE_CASE_MATCH;
      }

      // Gap penalty
      if (!isConsecutive && lastMatchIndex >= 0) {
        score += PENALTY_GAP * (textIndex - lastMatchIndex - 1);
      }

      lastMatchIndex = textIndex;
      patternIndex++;
    }
    textIndex++;
  }

  // Check if all pattern characters were found
  if (patternIndex == patternLower.length()) {
    result.matched = true;
    result.score = score;
    result.matchedIndices = matchedIndices;
  }

  return result;
}

bool NMFuzzyMatcher::matches(const QString &pattern, const QString &text) {
  return match(pattern, text).matched;
}

int NMFuzzyMatcher::calculateScore(const QString &pattern,
                                   const QString &text) {
  return match(pattern, text).score;
}

bool NMFuzzyMatcher::isWordBoundary(const QString &text, int index) {
  if (index == 0) {
    return true; // Start of string
  }

  const QChar current = text[index];
  const QChar previous = text[index - 1];

  // After space or special character
  if (previous == ' ' || previous == '_' || previous == '-' || previous == '/' ||
      previous == '\\' || previous == '.') {
    return true;
  }

  // Uppercase after lowercase (camelCase)
  if (current.isUpper() && previous.isLower()) {
    return true;
  }

  // Letter after number
  if (current.isLetter() && previous.isNumber()) {
    return true;
  }

  return false;
}

int NMFuzzyMatcher::getPositionBonus(const QString &text, int index,
                                     [[maybe_unused]] bool isConsecutive) {
  int bonus = 0;

  // Start of string is most valuable
  if (index == 0) {
    bonus += SCORE_START;
  }

  // Word boundary is valuable
  if (isWordBoundary(text, index)) {
    bonus += SCORE_WORD_BOUNDARY;
  }

  return bonus;
}

} // namespace NovelMind::editor::qt
