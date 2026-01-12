#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "nm_script_editor_panel_detail.hpp"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFormat>
#include <limits>

namespace NovelMind::editor::qt {

namespace {

int clampToInt(qsizetype value) {
  const qsizetype minInt =
      static_cast<qsizetype>(std::numeric_limits<int>::min());
  const qsizetype maxInt =
      static_cast<qsizetype>(std::numeric_limits<int>::max());
  if (value < minInt) {
    return std::numeric_limits<int>::min();
  }
  if (value > maxInt) {
    return std::numeric_limits<int>::max();
  }
  return static_cast<int>(value);
}

} // namespace

// ============================================================================
// NMScriptHighlighter
// ============================================================================

NMScriptHighlighter::NMScriptHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  const auto &palette = NMStyleManager::instance().palette();

  QTextCharFormat keywordFormat;
  keywordFormat.setForeground(palette.accentPrimary);
  keywordFormat.setFontWeight(QFont::Bold);

  // Use Unicode-aware word boundaries for keyword matching
  // This enables proper highlighting in presence of Cyrillic/Unicode
  // identifiers
  const QStringList keywords = detail::buildCompletionWords();
  for (const auto &word : keywords) {
    Rule rule;
    // Use explicit word boundary pattern that works with Unicode
    // (?<![\\w\\p{L}]) - not preceded by word char or Unicode letter
    // (?![\\w\\p{L}]) - not followed by word char or Unicode letter
    rule.pattern = QRegularExpression(
        QString("(?<![\\w\\p{L}])%1(?![\\w\\p{L}])").arg(word),
        QRegularExpression::UseUnicodePropertiesOption);
    rule.format = keywordFormat;
    m_rules.push_back(rule);
  }

  QTextCharFormat stringFormat;
  stringFormat.setForeground(QColor(220, 180, 120));
  Rule stringRule;
  stringRule.pattern = QRegularExpression("\"([^\"\\\\]|\\\\.)*\"");
  stringRule.format = stringFormat;
  m_rules.push_back(stringRule);

  QTextCharFormat numberFormat;
  numberFormat.setForeground(QColor(170, 200, 255));
  Rule numberRule;
  numberRule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
  numberRule.format = numberFormat;
  m_rules.push_back(numberRule);

  // Add identifier highlighting for Unicode identifiers (Cyrillic, etc.)
  // This helps users see that their non-ASCII identifiers are recognized
  QTextCharFormat identifierFormat;
  identifierFormat.setForeground(QColor(200, 200, 230)); // Light blue-gray
  Rule unicodeIdentifierRule;
  // Match identifiers starting with Unicode letters (including Cyrillic)
  // Pattern: Unicode letter followed by any word chars or Unicode letters
  unicodeIdentifierRule.pattern = QRegularExpression(
      "(?<![\\w\\p{L}])[\\p{L}_][\\p{L}\\p{N}_]*(?![\\w\\p{L}])",
      QRegularExpression::UseUnicodePropertiesOption);
  unicodeIdentifierRule.format = identifierFormat;
  m_rules.push_back(unicodeIdentifierRule);

  m_commentFormat.setForeground(QColor(120, 140, 150));
  m_commentStart = QRegularExpression("/\\*");
  m_commentEnd = QRegularExpression("\\*/");

  Rule lineCommentRule;
  lineCommentRule.pattern = QRegularExpression("//[^\n]*");
  lineCommentRule.format = m_commentFormat;
  m_rules.push_back(lineCommentRule);

  // Error format: red wavy underline
  m_errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
  m_errorFormat.setUnderlineColor(QColor(220, 80, 80));

  // Warning format: yellow wavy underline
  m_warningFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
  m_warningFormat.setUnderlineColor(QColor(230, 180, 60));
}

void NMScriptHighlighter::setDiagnostics(
    const QHash<int, QList<NMScriptIssue>> &diagnostics) {
  m_diagnostics = diagnostics;
  rehighlight();
}

void NMScriptHighlighter::clearDiagnostics() {
  m_diagnostics.clear();
  rehighlight();
}

void NMScriptHighlighter::highlightBlock(const QString &text) {
  for (const auto &rule : m_rules) {
    QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      const int start = clampToInt(match.capturedStart());
      const int length = clampToInt(match.capturedLength());
      setFormat(start, length, rule.format);
    }
  }

  setCurrentBlockState(0);

  int startIndex = 0;
  if (previousBlockState() != 1) {
    startIndex = clampToInt(text.indexOf(m_commentStart));
  }

  while (startIndex >= 0) {
    QRegularExpressionMatch endMatch = m_commentEnd.match(text, startIndex);
    int endIndex = clampToInt(endMatch.capturedStart());
    int commentLength = 0;
    const int textLength = clampToInt(text.length());
    const int endLength = clampToInt(endMatch.capturedLength());

    if (endIndex == -1) {
      setCurrentBlockState(1);
      commentLength = textLength - startIndex;
    } else {
      commentLength = endIndex - startIndex + endLength;
    }

    setFormat(startIndex, commentLength, m_commentFormat);
    startIndex =
        clampToInt(text.indexOf(m_commentStart, startIndex + commentLength));
  }

  // Apply diagnostic underlines for errors and warnings
  const int lineNumber = currentBlock().blockNumber() + 1;
  if (m_diagnostics.contains(lineNumber)) {
    const auto &issues = m_diagnostics.value(lineNumber);
    for (const auto &issue : issues) {
      // Underline the whole line for the diagnostic
      const QTextCharFormat &format =
          (issue.severity == "error") ? m_errorFormat : m_warningFormat;
      // Apply to non-whitespace portion of line
      int startPos = 0;
      while (startPos < static_cast<int>(text.size()) &&
             text.at(startPos).isSpace()) {
        ++startPos;
      }
      if (startPos < static_cast<int>(text.size())) {
        setFormat(startPos, clampToInt(text.size()) - startPos, format);
      }
    }
  }
}

} // namespace NovelMind::editor::qt
