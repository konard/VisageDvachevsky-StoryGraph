#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QWheelEvent>
#include <algorithm>

namespace NovelMind::editor::qt {

// ============================================================================
// NMScriptMinimap - VSCode-like code overview
// ============================================================================

NMScriptMinimap::NMScriptMinimap(NMScriptEditor *editor, QWidget *parent)
    : QWidget(parent), m_editor(editor) {
  setFixedWidth(kMinimapWidth);
  setMouseTracking(true);

  // Connect to editor signals for updates
  connect(m_editor->document(), &QTextDocument::contentsChanged, this,
          &NMScriptMinimap::updateContent);
  connect(m_editor, &NMScriptEditor::viewportChanged, this,
          &NMScriptMinimap::setViewportRange);
}

void NMScriptMinimap::updateContent() {
  m_totalLines = m_editor->document()->blockCount();

  // Create a cached image of the minimap
  const int height = static_cast<int>(m_totalLines * kMinimapLineHeight);
  if (height <= 0) {
    m_cachedImage = QImage();
    update();
    return;
  }

  m_cachedImage = QImage(kMinimapWidth, std::max(1, height),
                         QImage::Format_ARGB32_Premultiplied);
  m_cachedImage.fill(Qt::transparent);

  QPainter painter(&m_cachedImage);
  const auto &palette = NMStyleManager::instance().palette();

  // Draw each line as colored rectangles
  QTextBlock block = m_editor->document()->begin();
  int lineNum = 0;

  while (block.isValid()) {
    const QString text = block.text();
    const int y = static_cast<int>(lineNum * kMinimapLineHeight);
    int x = 0;

    for (const QChar &ch : text) {
      if (ch.isSpace()) {
        x += static_cast<int>(kMinimapCharWidth);
        continue;
      }

      QColor color = palette.textSecondary;
      // Simple syntax coloring for minimap
      if (ch == '{' || ch == '}') {
        color = palette.accentPrimary;
      } else if (ch == '"') {
        color = QColor(220, 180, 120);
      }

      painter.fillRect(QRectF(x, y, kMinimapCharWidth, kMinimapLineHeight - 1),
                       color);
      x += static_cast<int>(kMinimapCharWidth);

      if (x >= kMinimapWidth - 10) {
        break;
      }
    }

    block = block.next();
    ++lineNum;
  }

  update();
}

void NMScriptMinimap::setViewportRange(int firstLine, int lastLine) {
  m_firstVisibleLine = firstLine;
  m_lastVisibleLine = lastLine;
  update();
}

void NMScriptMinimap::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  const auto &palette = NMStyleManager::instance().palette();

  // Background
  painter.fillRect(rect(), palette.bgMedium);

  if (m_cachedImage.isNull() || m_totalLines == 0) {
    return;
  }

  // Calculate scale factor to fit minimap
  const double scale =
      std::min(1.0, static_cast<double>(height()) /
                        static_cast<double>(m_totalLines * kMinimapLineHeight));

  // Draw the cached minimap image
  painter.save();
  painter.scale(1.0, scale);
  painter.drawImage(0, 0, m_cachedImage);
  painter.restore();

  // Draw viewport indicator (the visible region box)
  if (m_totalLines > 0) {
    const double viewportTop =
        (static_cast<double>(m_firstVisibleLine) / m_totalLines) * height();
    const double viewportHeight =
        (static_cast<double>(m_lastVisibleLine - m_firstVisibleLine + 1) /
         m_totalLines) *
        height();

    QColor viewportColor = palette.bgLight;
    viewportColor.setAlpha(80);
    painter.fillRect(QRectF(0, viewportTop, width(), viewportHeight),
                     viewportColor);

    // Border for viewport
    painter.setPen(QPen(palette.borderLight, 1));
    painter.drawRect(QRectF(0, viewportTop, width() - 1, viewportHeight));
  }
}

void NMScriptMinimap::mousePressEvent(QMouseEvent *event) {
  m_isDragging = true;
  // Navigate to clicked position
  if (m_totalLines > 0) {
    const int targetLine = static_cast<int>(
        (static_cast<double>(event->pos().y()) / height()) * m_totalLines);
    QTextBlock block = m_editor->document()->findBlockByNumber(targetLine);
    if (block.isValid()) {
      QTextCursor cursor(block);
      m_editor->setTextCursor(cursor);
      m_editor->centerCursor();
    }
  }
}

void NMScriptMinimap::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
    if (m_totalLines > 0) {
      const int targetLine = static_cast<int>(
          (static_cast<double>(event->pos().y()) / height()) * m_totalLines);
      QTextBlock block = m_editor->document()->findBlockByNumber(
          std::max(0, std::min(targetLine, m_totalLines - 1)));
      if (block.isValid()) {
        QTextCursor cursor(block);
        m_editor->setTextCursor(cursor);
        m_editor->centerCursor();
      }
    }
  }
}

void NMScriptMinimap::wheelEvent(QWheelEvent *event) {
  // Forward wheel events to the editor
  QCoreApplication::sendEvent(m_editor, event);
}

} // namespace NovelMind::editor::qt
