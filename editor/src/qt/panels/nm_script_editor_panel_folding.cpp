#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QPainter>
#include <QPlainTextEdit>
#include <QStack>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>

namespace NovelMind::editor::qt {

// ============================================================================
// VSCode-like features: Folding, Bracket Matching, Minimap, etc.
// ============================================================================

int NMScriptEditor::foldingAreaWidth() const { return 14; }

void NMScriptEditor::foldingAreaPaintEvent(QPaintEvent *event) {
  if (!m_foldingArea) {
    return;
  }

  QPainter painter(m_foldingArea);
  const auto &palette = NMStyleManager::instance().palette();
  painter.fillRect(event->rect(), palette.bgMedium);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      // Check if this line starts a folding region
      for (const auto &region : m_foldingRegions) {
        if (region.startLine == blockNumber) {
          // Draw fold indicator
          const int iconSize = 10;
          const int x = (m_foldingArea->width() - iconSize) / 2;
          const int y = top + (fontMetrics().height() - iconSize) / 2;

          painter.setPen(palette.textSecondary);
          painter.drawRect(x, y, iconSize, iconSize);

          // Draw +/- sign
          painter.drawLine(x + 2, y + iconSize / 2, x + iconSize - 2,
                           y + iconSize / 2);
          if (region.isCollapsed) {
            painter.drawLine(x + iconSize / 2, y + 2, x + iconSize / 2,
                             y + iconSize - 2);
          }
          break;
        }
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

void NMScriptEditor::toggleFold(int line) {
  for (auto &region : m_foldingRegions) {
    if (region.startLine == line) {
      region.isCollapsed = !region.isCollapsed;

      // Hide or show the lines in the region
      QTextBlock startBlock = document()->findBlockByNumber(line + 1);
      QTextBlock endBlock = document()->findBlockByNumber(region.endLine);
      (void)endBlock;

      while (startBlock.isValid() &&
             startBlock.blockNumber() <= region.endLine) {
        startBlock.setVisible(!region.isCollapsed);
        startBlock = startBlock.next();
      }

      // Force a repaint
      viewport()->update();
      m_foldingArea->update();

      if (m_minimap) {
        m_minimap->updateContent();
      }
      break;
    }
  }
}

void NMScriptEditor::updateFoldingRegions() {
  m_foldingRegions.clear();

  const QString text = document()->toPlainText();
  const QStringList lines = text.split('\n');

  QStack<int> braceStack;

  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    const QString &line = lines[i];
    const int openCount = static_cast<int>(line.count('{'));
    const int closeCount = static_cast<int>(line.count('}'));

    // Push opening braces
    for (int j = 0; j < openCount; ++j) {
      braceStack.push(i);
    }

    // Pop closing braces and create regions
    for (int j = 0; j < closeCount; ++j) {
      if (!braceStack.isEmpty()) {
        const int startLine = braceStack.pop();
        if (i > startLine) {
          FoldingRegion region;
          region.startLine = startLine;
          region.endLine = i;
          region.isCollapsed = false;
          m_foldingRegions.append(region);
        }
      }
    }
  }

  if (m_foldingArea) {
    m_foldingArea->update();
  }
}

void NMScriptEditor::setMinimapEnabled(bool enabled) {
  m_minimapEnabled = enabled;
  if (m_minimap) {
    m_minimap->setVisible(enabled);
    updateMinimapGeometry();
  }
}

void NMScriptEditor::updateMinimapGeometry() {
  if (!m_minimap) {
    return;
  }

  if (m_minimapEnabled) {
    QRect cr = contentsRect();
    const int minimapWidth = 120;
    m_minimap->setGeometry(cr.right() - minimapWidth, cr.top(), minimapWidth,
                           cr.height());
    m_minimap->show();

    // Adjust viewport margins to make room for minimap
    // Issue #239: Include graph gutter width
    setViewportMargins(breakpointGutterWidth() + graphGutterWidth() + lineNumberAreaWidth() + foldingAreaWidth(), 0,
                       minimapWidth, 0);
  } else {
    m_minimap->hide();
    // Issue #239: Include graph gutter width
    setViewportMargins(breakpointGutterWidth() + graphGutterWidth() + lineNumberAreaWidth() + foldingAreaWidth(), 0, 0, 0);
  }
}

void NMScriptEditor::emitViewportChanged() {
  const QTextBlock firstBlock = firstVisibleBlock();
  const int firstLine = firstBlock.blockNumber();

  // Calculate last visible line
  QTextBlock block = firstBlock;
  int lastLine = firstLine;
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());

  while (block.isValid() && top < viewport()->height()) {
    lastLine = block.blockNumber();
    top += static_cast<int>(blockBoundingRect(block).height());
    block = block.next();
  }

  emit viewportChanged(firstLine, lastLine);
}

BracketPosition NMScriptEditor::findMatchingBracket(int position) const {
  BracketPosition result;
  const QString text = document()->toPlainText();

  if (position < 0 || position >= static_cast<int>(text.size())) {
    return result;
  }

  const QChar ch = text.at(position);
  const QString openBrackets = "({[";
  const QString closeBrackets = ")}]";

  int direction = 0;
  QChar matchingBracket;

  if (openBrackets.contains(ch)) {
    direction = 1;
    matchingBracket = closeBrackets.at(openBrackets.indexOf(ch));
  } else if (closeBrackets.contains(ch)) {
    direction = -1;
    matchingBracket = openBrackets.at(closeBrackets.indexOf(ch));
  } else {
    return result;
  }

  int depth = 1;
  int i = position + direction;

  while (i >= 0 && i < static_cast<int>(text.size()) && depth > 0) {
    const QChar current = text.at(i);
    if (current == ch) {
      ++depth;
    } else if (current == matchingBracket) {
      --depth;
    }

    if (depth == 0) {
      result.position = i;
      result.bracket = matchingBracket;
      result.isOpening = (direction == -1);
      return result;
    }

    i += direction;
  }

  return result;
}

void NMScriptEditor::highlightMatchingBrackets() {
  m_bracketHighlights.clear();

  const QTextCursor cursor = textCursor();
  const int pos = cursor.position();
  const QString text = document()->toPlainText();

  // Check character at cursor and before cursor
  QList<int> positions;
  positions << pos << (pos - 1);

  const auto &palette = NMStyleManager::instance().palette();
  QTextCharFormat bracketFormat;
  bracketFormat.setBackground(QColor(palette.accentPrimary.red(),
                                     palette.accentPrimary.green(),
                                     palette.accentPrimary.blue(), 80));

  for (int checkPos : positions) {
    if (checkPos < 0 || checkPos >= static_cast<int>(text.size())) {
      continue;
    }

    const QChar ch = text.at(checkPos);
    if (QString("(){}[]").contains(ch)) {
      BracketPosition match = findMatchingBracket(checkPos);
      if (match.position >= 0) {
        // Highlight current bracket
        QTextEdit::ExtraSelection sel1;
        sel1.format = bracketFormat;
        sel1.cursor = QTextCursor(document());
        sel1.cursor.setPosition(checkPos);
        sel1.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        m_bracketHighlights.append(sel1);

        // Highlight matching bracket
        QTextEdit::ExtraSelection sel2;
        sel2.format = bracketFormat;
        sel2.cursor = QTextCursor(document());
        sel2.cursor.setPosition(match.position);
        sel2.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        m_bracketHighlights.append(sel2);

        break;
      }
    }
  }

  // Combine with current line highlight and search highlights
  QList<QTextEdit::ExtraSelection> allSelections;

  // Current line highlight
  if (!isReadOnly()) {
    QTextEdit::ExtraSelection lineSelection;
    lineSelection.format.setBackground(QColor(palette.bgLight.red(),
                                              palette.bgLight.green(),
                                              palette.bgLight.blue(), 60));
    lineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
    lineSelection.cursor = textCursor();
    lineSelection.cursor.clearSelection();
    allSelections.append(lineSelection);
  }

  allSelections.append(m_searchHighlights);
  allSelections.append(m_bracketHighlights);
  setExtraSelections(allSelections);
}

void NMScriptEditor::setSearchHighlights(
    const QList<QTextEdit::ExtraSelection> &highlights) {
  m_searchHighlights = highlights;
  highlightMatchingBrackets(); // This will combine all highlights
}

void NMScriptEditor::clearSearchHighlights() {
  m_searchHighlights.clear();
  highlightMatchingBrackets();
}

void NMScriptEditor::paintEvent(QPaintEvent *event) {
  QPlainTextEdit::paintEvent(event);
  // Additional custom painting can go here if needed
}

void NMScriptEditor::scrollContentsBy(int dx, int dy) {
  QPlainTextEdit::scrollContentsBy(dx, dy);
  emitViewportChanged();
}

} // namespace NovelMind::editor::qt
