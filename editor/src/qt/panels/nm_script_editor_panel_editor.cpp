#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStack>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextStream>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <filesystem>
#include <limits>

#include "nm_script_editor_panel_detail.hpp"

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

class NMCompletionDelegate : public QStyledItemDelegate {
public:
  explicit NMCompletionDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const auto &palette = NMStyleManager::instance().palette();
    painter->save();

    QColor bg = (option.state & QStyle::State_Selected) ? palette.bgLight
                                                        : palette.bgMedium;
    painter->fillRect(opt.rect, bg);

    QRect textRect = opt.rect.adjusted(8, 0, -8, 0);
    const QString detail = index.data(Qt::UserRole + 1).toString();
    const QFont mainFont = NMStyleManager::instance().monospaceFont();

    painter->setPen(palette.textPrimary);
    painter->setFont(mainFont);
    const int badgePadding = 6;
    const int badgeHeight = 18;

    QRect badgeRect = textRect;
    if (!detail.isEmpty()) {
      QFontMetrics fm(mainFont);
      const int badgeWidth = fm.horizontalAdvance(detail) + badgePadding * 2;
      badgeRect.setLeft(textRect.right() - badgeWidth);
      badgeRect.setWidth(badgeWidth);
      badgeRect.setHeight(badgeHeight);
      badgeRect.moveCenter(
          QPoint(badgeRect.center().x(), textRect.center().y()));
      textRect.setRight(badgeRect.left() - 8);

      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setBrush(palette.bgDark);
      painter->setPen(QPen(palette.borderLight, 1));
      painter->drawRoundedRect(badgeRect, 6, 6);

      painter->setPen(palette.textSecondary);
      painter->drawText(badgeRect, Qt::AlignCenter, detail);
    }

    painter->setPen(palette.textPrimary);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, opt.text);

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    base.setHeight(std::max(base.height(), 22));
    return base;
  }
};

class NMScriptEditorLineNumberArea : public QWidget {
public:
  explicit NMScriptEditorLineNumberArea(NMScriptEditor *editor)
      : QWidget(editor), m_editor(editor) {}

  [[nodiscard]] QSize sizeHint() const override {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    m_editor->lineNumberAreaPaintEvent(event);
  }

private:
  NMScriptEditor *m_editor = nullptr;
};

class NMScriptEditorFoldingArea : public QWidget {
public:
  explicit NMScriptEditorFoldingArea(NMScriptEditor *editor)
      : QWidget(editor), m_editor(editor) {
    setCursor(Qt::PointingHandCursor);
  }

  [[nodiscard]] QSize sizeHint() const override {
    return QSize(m_editor->foldingAreaWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    m_editor->foldingAreaPaintEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      // Calculate which line was clicked
      QTextBlock block = m_editor->getFirstVisibleBlock();
      int top = static_cast<int>(m_editor->getBlockBoundingGeometry(block)
                                     .translated(m_editor->getContentOffset())
                                     .top());
      int bottom = top + static_cast<int>(
                             m_editor->getBlockBoundingRect(block).height());

      while (block.isValid() && top <= event->pos().y()) {
        if (block.isVisible() && bottom >= event->pos().y()) {
          m_editor->toggleFold(block.blockNumber());
          break;
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(
                           m_editor->getBlockBoundingRect(block).height());
      }
    }
    QWidget::mousePressEvent(event);
  }

private:
  NMScriptEditor *m_editor = nullptr;
};

/**
 * @brief Breakpoint gutter widget - shows red dots for breakpoints
 *
 * Click to toggle breakpoints at line positions.
 */
class NMScriptEditorBreakpointGutter : public QWidget {
public:
  explicit NMScriptEditorBreakpointGutter(NMScriptEditor *editor)
      : QWidget(editor), m_editor(editor) {
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
  }

  [[nodiscard]] QSize sizeHint() const override {
    return QSize(m_editor->breakpointGutterWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    m_editor->breakpointGutterPaintEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      // Calculate which line was clicked
      QTextBlock block = m_editor->getFirstVisibleBlock();
      int top = static_cast<int>(m_editor->getBlockBoundingGeometry(block)
                                     .translated(m_editor->getContentOffset())
                                     .top());
      int bottom = top + static_cast<int>(
                             m_editor->getBlockBoundingRect(block).height());

      while (block.isValid() && top <= event->pos().y()) {
        if (block.isVisible() && bottom >= event->pos().y()) {
          // Toggle breakpoint at this line (1-based line number)
          m_editor->toggleBreakpoint(block.blockNumber() + 1);
          break;
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(
                           m_editor->getBlockBoundingRect(block).height());
      }
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    // Update for potential hover effect
    update();
    QWidget::mouseMoveEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    update();
    QWidget::leaveEvent(event);
  }

private:
  NMScriptEditor *m_editor = nullptr;
};

/**
 * @brief Graph integration gutter widget - shows indicators for graph-connected scenes
 *
 * Issue #239: Visual indicators showing which script scenes are connected to Story Graph.
 * Click to navigate to the corresponding graph node.
 *
 * Indicator colors:
 * - Green: Scene is connected to a Story Graph node (valid)
 * - Yellow: Scene may have warnings (orphaned node reference)
 */
class NMScriptEditorGraphGutter : public QWidget {
public:
  explicit NMScriptEditorGraphGutter(NMScriptEditor *editor)
      : QWidget(editor), m_editor(editor) {
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setToolTip(tr("Click to navigate to Story Graph node"));
  }

  [[nodiscard]] QSize sizeHint() const override {
    return QSize(m_editor->graphGutterWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    m_editor->graphGutterPaintEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      // Calculate which line was clicked
      QTextBlock block = m_editor->getFirstVisibleBlock();
      int top = static_cast<int>(m_editor->getBlockBoundingGeometry(block)
                                     .translated(m_editor->getContentOffset())
                                     .top());
      int bottom = top + static_cast<int>(
                             m_editor->getBlockBoundingRect(block).height());

      while (block.isValid() && top <= event->pos().y()) {
        if (block.isVisible() && bottom >= event->pos().y()) {
          const int line = block.blockNumber() + 1;
          if (m_editor->hasGraphConnectedScene(line)) {
            const QString sceneId = m_editor->sceneIdAtLine(line);
            emit m_editor->graphIndicatorClicked(sceneId);
          }
          break;
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(
                           m_editor->getBlockBoundingRect(block).height());
      }
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    // Update tooltip based on hovered line
    QTextBlock block = m_editor->getFirstVisibleBlock();
    int top = static_cast<int>(m_editor->getBlockBoundingGeometry(block)
                                   .translated(m_editor->getContentOffset())
                                   .top());
    int bottom = top + static_cast<int>(
                           m_editor->getBlockBoundingRect(block).height());

    while (block.isValid() && top <= event->pos().y()) {
      if (block.isVisible() && bottom >= event->pos().y()) {
        const int line = block.blockNumber() + 1;
        if (m_editor->hasGraphConnectedScene(line)) {
          const QString sceneId = m_editor->sceneIdAtLine(line);
          setToolTip(tr("Scene '%1' connected to Story Graph\n"
                        "Click to navigate to graph node")
                         .arg(sceneId));
        } else {
          setToolTip(QString());
        }
        break;
      }
      block = block.next();
      top = bottom;
      bottom = top + static_cast<int>(
                         m_editor->getBlockBoundingRect(block).height());
    }

    update();
    QWidget::mouseMoveEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    setToolTip(QString());
    update();
    QWidget::leaveEvent(event);
  }

private:
  NMScriptEditor *m_editor = nullptr;
};

} // namespace

// ============================================================================
// NMScriptEditor
// ============================================================================

NMScriptEditor::NMScriptEditor(QWidget *parent) : QPlainTextEdit(parent) {
  setMouseTracking(true);
  setFont(NMStyleManager::instance().monospaceFont());
  setTabStopDistance(fontMetrics().horizontalAdvance(' ') * m_indentSize);
  setLineWrapMode(QPlainTextEdit::NoWrap);

  const auto &palette = NMStyleManager::instance().palette();
  setStyleSheet(QString("QPlainTextEdit {"
                        "  background-color: %1;"
                        "  color: %2;"
                        "  border: none;"
                        "  selection-background-color: %3;"
                        "  selection-color: %4;"
                        "}")
                    .arg(palette.bgDark.name())
                    .arg(palette.textPrimary.name())
                    .arg(palette.accentPrimary.name())
                    .arg(palette.bgDarkest.name()));

  m_breakpointGutter = new NMScriptEditorBreakpointGutter(this);
  m_graphGutter = new NMScriptEditorGraphGutter(this);  // Issue #239
  m_lineNumberArea = new NMScriptEditorLineNumberArea(this);
  m_foldingArea = new NMScriptEditorFoldingArea(this);

  connect(this, &QPlainTextEdit::blockCountChanged, this,
          &NMScriptEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this,
          &NMScriptEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &NMScriptEditor::highlightCurrentLine);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &NMScriptEditor::highlightMatchingBrackets);
  connect(document(), &QTextDocument::contentsChanged, this,
          &NMScriptEditor::updateFoldingRegions);

  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  m_highlighter = new NMScriptHighlighter(document());

  // Create minimap
  m_minimap = new NMScriptMinimap(this, this);
  m_minimap->updateContent();

  m_baseCompletionWords = detail::buildCompletionWords();
  auto *completer = new QCompleter(this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setFilterMode(Qt::MatchContains);
  completer->setWrapAround(false);
  completer->setWidget(this);
  completer->popup()->setItemDelegate(new NMCompletionDelegate(completer));
  completer->popup()->setStyleSheet(
      QString(
          "QListView { background-color: %1; color: %2; border: 1px solid %3; }"
          "QListView::item { padding: 4px 6px; }"
          "QListView::item:selected { background: %4; color: %2; }")
          .arg(palette.bgMedium.name())
          .arg(palette.textPrimary.name())
          .arg(palette.borderLight.name())
          .arg(palette.bgLight.name()));
  m_completer = completer;
  setCompletionEntries(detail::buildKeywordEntries());

  connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
          this, &NMScriptEditor::insertCompletion);
  connect(document(), &QTextDocument::contentsChanged, this,
          &NMScriptEditor::refreshDynamicCompletions);

  // Emit viewport changes for minimap sync
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          &NMScriptEditor::emitViewportChanged);
}

NMScriptEditor::~NMScriptEditor() = default;

void NMScriptEditor::setCompletionWords(const QStringList &words) {
  QList<CompletionEntry> entries;
  for (const auto &word : words) {
    entries.push_back({word, "keyword"});
  }
  setCompletionEntries(entries);
}

void NMScriptEditor::setCompletionEntries(
    const QList<CompletionEntry> &entries) {
  m_staticCompletionEntries = entries;
  refreshDynamicCompletions();
}

static QHash<QString, QString>
normalizedDocs(const QHash<QString, QString> &docs) {
  QHash<QString, QString> result;
  for (auto it = docs.constBegin(); it != docs.constEnd(); ++it) {
    result.insert(it.key().toLower(), it.value());
  }
  return result;
}

void NMScriptEditor::setHoverDocs(const QHash<QString, QString> &docs) {
  m_hoverDocs = normalizedDocs(docs);
}

void NMScriptEditor::setDocHtml(const QHash<QString, QString> &docs) {
  m_docHtml = normalizedDocs(docs);
}

void NMScriptEditor::setProjectDocs(const QHash<QString, QString> &docs) {
  const auto normalized = normalizedDocs(docs);
  for (auto it = normalized.constBegin(); it != normalized.constEnd(); ++it) {
    m_hoverDocs.insert(it.key(), it.value());
  }
}

void NMScriptEditor::setSymbolLocations(
    const QHash<QString, SymbolLocation> &locations) {
  m_symbolLocations.clear();
  for (auto it = locations.constBegin(); it != locations.constEnd(); ++it) {
    m_symbolLocations.insert(it.key().toLower(), it.value());
  }
}

void NMScriptEditor::setDiagnostics(const QList<NMScriptIssue> &issues) {
  if (!m_highlighter) {
    return;
  }
  QHash<int, QList<NMScriptIssue>> byLine;
  for (const auto &issue : issues) {
    byLine[issue.line].append(issue);
  }
  m_highlighter->setDiagnostics(byLine);
}

void NMScriptEditor::insertSnippet(const QString &snippetType) {
  // Find matching snippet template
  for (const auto &tmpl : detail::buildSnippetTemplates()) {
    if (tmpl.prefix == snippetType ||
        tmpl.name.toLower().contains(snippetType.toLower())) {
      insertSnippetTemplate(tmpl);
      return;
    }
  }

  // Fallback to simple insertion
  QTextCursor cursor = textCursor();
  cursor.beginEditBlock();

  QString snippet;
  int cursorOffset = 0;

  if (snippetType == "scene") {
    snippet = "scene scene_name {\n  say Narrator \"Description\"\n}\n";
    cursorOffset = 6;
  } else if (snippetType == "choice") {
    snippet = "choice {\n  \"Option 1\" -> scene_target1\n  \"Option 2\" -> "
              "scene_target2\n}\n";
    cursorOffset = 12;
  } else if (snippetType == "if") {
    snippet = "if flag condition {\n  // true branch\n} else {\n  // false "
              "branch\n}\n";
    cursorOffset = 8;
  } else if (snippetType == "goto") {
    snippet = "goto scene_name\n";
    cursorOffset = 5;
  } else if (snippetType == "character") {
    snippet = "character CharName(name=\"Display Name\", color=\"#4A9FD9\")\n";
    cursorOffset = 10;
  } else if (snippetType == "say") {
    snippet = "say Character \"Dialogue text\"\n";
    cursorOffset = 4;
  } else if (snippetType == "show") {
    snippet = "show background \"asset_path\"\n";
    cursorOffset = 17;
  } else {
    cursor.endEditBlock();
    return;
  }

  const int startPos = cursor.position();
  cursor.insertText(snippet);
  cursor.setPosition(startPos + cursorOffset);
  cursor.endEditBlock();
  setTextCursor(cursor);
}

void NMScriptEditor::insertSnippetTemplate(const SnippetTemplate &snippet) {
  QTextCursor cursor = textCursor();
  cursor.beginEditBlock();

  // Parse snippet body and replace tabstop placeholders
  QString body = snippet.body;
  const int startPos = cursor.position();

  // Store tabstop positions
  m_tabstopPositions.clear();
  m_currentTabstop = 0;

  // Replace ${N:placeholder} with placeholder and record positions
  QRegularExpression re("\\$\\{(\\d+):([^}]*)\\}");
  qsizetype offset = 0;

  QRegularExpressionMatchIterator it = re.globalMatch(body);
  QList<QPair<int, QRegularExpressionMatch>> matches;

  while (it.hasNext()) {
    matches.append({0, it.next()});
  }

  // Process matches in reverse order to maintain correct positions
  for (int i = static_cast<int>(matches.size()) - 1; i >= 0; --i) {
    const auto &match = matches[i].second;
    const QString placeholder = match.captured(2);
    body.replace(match.capturedStart(), match.capturedLength(), placeholder);
  }

  // Insert the processed body
  cursor.insertText(body);

  // Now find the tabstop positions in the inserted text
  QRegularExpressionMatchIterator it2 = re.globalMatch(snippet.body);
  offset = 0;
  int tabstopIndex = 0;

  while (it2.hasNext()) {
    QRegularExpressionMatch match = it2.next();
    const QString placeholder = match.captured(2);
    const qsizetype originalPos = match.capturedStart() - offset;

    // Calculate actual position in document
    const int docPos = startPos + static_cast<int>(originalPos);
    m_tabstopPositions.append(
        {docPos, static_cast<int>(placeholder.length())});

    offset += match.capturedLength() - placeholder.length();
    ++tabstopIndex;
  }

  cursor.endEditBlock();

  // Select the first tabstop
  if (!m_tabstopPositions.isEmpty()) {
    m_inSnippetMode = true;
    m_currentTabstop = 0;
    const auto &firstTabstop = m_tabstopPositions[0];
    cursor.setPosition(firstTabstop.first);
    cursor.setPosition(firstTabstop.first + firstTabstop.second,
                       QTextCursor::KeepAnchor);
    setTextCursor(cursor);
  } else {
    setTextCursor(cursor);
  }
}

void NMScriptEditor::nextTabstop() {
  if (!m_inSnippetMode || m_tabstopPositions.isEmpty()) {
    return;
  }

  ++m_currentTabstop;
  if (m_currentTabstop >= static_cast<int>(m_tabstopPositions.size())) {
    // Exit snippet mode
    m_inSnippetMode = false;
    m_tabstopPositions.clear();
    return;
  }

  const auto &tabstop = m_tabstopPositions[m_currentTabstop];
  QTextCursor cursor = textCursor();
  cursor.setPosition(tabstop.first);
  cursor.setPosition(tabstop.first + tabstop.second, QTextCursor::KeepAnchor);
  setTextCursor(cursor);
}

void NMScriptEditor::previousTabstop() {
  if (!m_inSnippetMode || m_tabstopPositions.isEmpty()) {
    return;
  }

  if (m_currentTabstop > 0) {
    --m_currentTabstop;
    const auto &tabstop = m_tabstopPositions[m_currentTabstop];
    QTextCursor cursor = textCursor();
    cursor.setPosition(tabstop.first);
    cursor.setPosition(tabstop.first + tabstop.second, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
  }
}

CompletionContext NMScriptEditor::getCompletionContext() const {
  QTextCursor cursor = textCursor();
  const int pos = cursor.position();
  const QString text = document()->toPlainText();

  if (pos == 0) {
    return CompletionContext::Unknown;
  }

  // Get the current line up to cursor
  const int lineStart = clampToInt(text.lastIndexOf('\n', pos - 1)) + 1;
  const QString lineBeforeCursor = text.mid(lineStart, pos - lineStart);

  // Check if inside a string
  int quoteCount = 0;
  for (const QChar &ch : lineBeforeCursor) {
    if (ch == '"') {
      ++quoteCount;
    }
  }
  if (quoteCount % 2 == 1) {
    return CompletionContext::InString;
  }

  // Check if inside a comment
  if (lineBeforeCursor.contains("//")) {
    return CompletionContext::InComment;
  }

  // Check for keywords before cursor
  const QString trimmed = lineBeforeCursor.trimmed().toLower();

  if (trimmed.endsWith("say")) {
    return CompletionContext::AfterSay;
  }
  if (trimmed.endsWith("goto")) {
    return CompletionContext::AfterGoto;
  }
  if (trimmed.endsWith("show")) {
    return CompletionContext::AfterShow;
  }
  if (trimmed.endsWith("hide")) {
    return CompletionContext::AfterHide;
  }
  if (trimmed.endsWith("play")) {
    return CompletionContext::AfterPlay;
  }
  if (trimmed.endsWith("stop")) {
    return CompletionContext::AfterStop;
  }
  if (trimmed.endsWith("set")) {
    return CompletionContext::AfterSet;
  }
  if (trimmed.endsWith("if")) {
    return CompletionContext::AfterIf;
  }
  if (trimmed.endsWith("at")) {
    return CompletionContext::AfterAt;
  }
  if (trimmed.endsWith("transition")) {
    return CompletionContext::AfterTransition;
  }

  // Check if inside choice block
  const int choiceStart = clampToInt(text.lastIndexOf("choice", pos));
  if (choiceStart >= 0) {
    const int braceAfterChoice = clampToInt(text.indexOf('{', choiceStart));
    const int closingBrace = clampToInt(text.indexOf('}', braceAfterChoice));
    if (braceAfterChoice >= 0 && pos > braceAfterChoice &&
        (closingBrace < 0 || pos < closingBrace)) {
      return CompletionContext::AfterChoice;
    }
  }

  return CompletionContext::Unknown;
}

QList<NMScriptEditor::CompletionEntry>
NMScriptEditor::getContextualCompletions(const QString &prefix) const {
  Q_UNUSED(prefix);
  // This would be called by the panel with symbol index
  // For now, return basic keywords
  return detail::buildKeywordEntries();
}

QList<QuickFix> NMScriptEditor::getQuickFixes(int line) const {
  if (m_quickFixes.contains(line)) {
    return m_quickFixes.value(line);
  }
  return {};
}

void NMScriptEditor::applyQuickFix(const QuickFix &fix) {
  QTextCursor cursor = textCursor();
  cursor.beginEditBlock();

  // Move to the fix location
  QTextBlock block = document()->findBlockByNumber(fix.line - 1);
  if (!block.isValid()) {
    cursor.endEditBlock();
    return;
  }

  cursor.setPosition(block.position() + fix.column);

  if (fix.replacementLength > 0) {
    // Select and replace
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                        fix.replacementLength);
  }

  cursor.insertText(fix.replacement);
  cursor.endEditBlock();
  setTextCursor(cursor);
}

QString NMScriptEditor::getSyntaxHint() const {
  // Get the keyword near cursor
  QTextCursor cursor = textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  QString word = cursor.selectedText();

  // If no word, check the previous word
  if (word.isEmpty()) {
    cursor = textCursor();
    cursor.movePosition(QTextCursor::PreviousWord);
    cursor.select(QTextCursor::WordUnderCursor);
    word = cursor.selectedText();
  }

  return detail::getSyntaxHintForKeyword(word);
}

QStringList NMScriptEditor::getBreadcrumbs() const {
  QStringList breadcrumbs;
  const int pos = textCursor().position();
  const QString text = document()->toPlainText();

  // Find enclosing scene
  const QRegularExpression sceneRe("scene\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\{");
  QRegularExpressionMatchIterator sceneIt = sceneRe.globalMatch(text);
  QString currentScene;
  int sceneStart = -1;

  while (sceneIt.hasNext()) {
    QRegularExpressionMatch match = sceneIt.next();
    if (clampToInt(match.capturedStart()) < pos) {
      // Check if we're still inside this scene
      int braceCount = 1;
      int searchPos = clampToInt(match.capturedEnd());
      while (searchPos < static_cast<int>(text.size()) && braceCount > 0) {
        if (text.at(searchPos) == '{') {
          ++braceCount;
        } else if (text.at(searchPos) == '}') {
          --braceCount;
        }
        ++searchPos;
      }
      if (searchPos >= pos) {
        currentScene = match.captured(1);
        sceneStart = clampToInt(match.capturedStart());
      }
    }
  }

  if (!currentScene.isEmpty()) {
    breadcrumbs.append(QString("scene %1").arg(currentScene));
  }

  // Find enclosing choice block
  if (sceneStart >= 0) {
    int choicePos = clampToInt(text.lastIndexOf("choice", pos));
    if (choicePos > sceneStart) {
      const int braceAfterChoice = clampToInt(text.indexOf('{', choicePos));
      if (braceAfterChoice >= 0 && braceAfterChoice < pos) {
        // Check if still inside
        int braceCount = 1;
        int searchPos = braceAfterChoice + 1;
        while (searchPos < static_cast<int>(text.size()) && searchPos < pos &&
               braceCount > 0) {
          if (text.at(searchPos) == '{') {
            ++braceCount;
          } else if (text.at(searchPos) == '}') {
            --braceCount;
          }
          ++searchPos;
        }
        if (braceCount > 0) {
          breadcrumbs.append("choice");
        }
      }
    }

    // Find enclosing if block
    int ifPos = clampToInt(text.lastIndexOf("if ", pos));
    if (ifPos > sceneStart &&
        ifPos > clampToInt(text.lastIndexOf("choice", pos))) {
      const int braceAfterIf = clampToInt(text.indexOf('{', ifPos));
      if (braceAfterIf >= 0 && braceAfterIf < pos) {
        breadcrumbs.append("if");
      }
    }
  }

  return breadcrumbs;
}

QString NMScriptEditor::symbolAtPosition(const QPoint &pos) const {
  QTextCursor cursor = cursorForPosition(pos);
  cursor.select(QTextCursor::WordUnderCursor);
  return cursor.selectedText();
}

void NMScriptEditor::goToDefinition() {
  const QString symbol = textUnderCursor();
  if (symbol.isEmpty()) {
    return;
  }
  const QString key = symbol.toLower();
  if (m_symbolLocations.contains(key)) {
    emit goToDefinitionRequested(symbol, m_symbolLocations.value(key));
  }
}

void NMScriptEditor::findReferences() {
  const QString symbol = textUnderCursor();
  if (!symbol.isEmpty()) {
    emit findReferencesRequested(symbol);
  }
}

void NMScriptEditor::showSnippetMenu() {
  QMenu menu(this);
  menu.setStyleSheet("QMenu { background-color: #2d2d2d; color: #e0e0e0; }"
                     "QMenu::item:selected { background-color: #404040; }");

  menu.addAction(tr("Scene block"), this, [this]() { insertSnippet("scene"); });
  menu.addAction(tr("Choice block"), this,
                 [this]() { insertSnippet("choice"); });
  menu.addAction(tr("If/Else block"), this, [this]() { insertSnippet("if"); });
  menu.addAction(tr("Goto statement"), this,
                 [this]() { insertSnippet("goto"); });
  menu.addSeparator();
  menu.addAction(tr("Character declaration"), this,
                 [this]() { insertSnippet("character"); });
  menu.addAction(tr("Say dialogue"), this, [this]() { insertSnippet("say"); });
  menu.addAction(tr("Show background"), this,
                 [this]() { insertSnippet("show"); });

  menu.exec(mapToGlobal(cursorRect().bottomLeft()));
}

void NMScriptEditor::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Save)) {
    emit requestSave();
    event->accept();
    return;
  }

  // F12: Go to Definition
  if (event->key() == Qt::Key_F12 && event->modifiers() == Qt::NoModifier) {
    goToDefinition();
    event->accept();
    return;
  }

  // Shift+F12: Find References
  if (event->key() == Qt::Key_F12 && (event->modifiers() & Qt::ShiftModifier)) {
    findReferences();
    event->accept();
    return;
  }

  // Ctrl+J: Insert Snippet
  if (event->key() == Qt::Key_J && (event->modifiers() & Qt::ControlModifier)) {
    showSnippetMenu();
    event->accept();
    return;
  }

  // Ctrl+G: Navigate to Graph (for scenes)
  if (event->key() == Qt::Key_G && (event->modifiers() & Qt::ControlModifier) &&
      (event->modifiers() & Qt::ShiftModifier)) {
    const QString symbol = textUnderCursor();
    if (!symbol.isEmpty()) {
      emit navigateToGraphNodeRequested(symbol);
    }
    event->accept();
    return;
  }

  // Ctrl+F: Find (VSCode-like)
  if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    emit showFindRequested();
    event->accept();
    return;
  }

  // Ctrl+H: Find and Replace (VSCode-like)
  if (event->key() == Qt::Key_H && (event->modifiers() & Qt::ControlModifier)) {
    emit showReplaceRequested();
    event->accept();
    return;
  }

  // Ctrl+Shift+P: Command Palette (VSCode-like)
  if (event->key() == Qt::Key_P && (event->modifiers() & Qt::ControlModifier) &&
      (event->modifiers() & Qt::ShiftModifier)) {
    emit showCommandPaletteRequested();
    event->accept();
    return;
  }

  // Ctrl+.: Quick Fix (VSCode-like)
  if (event->key() == Qt::Key_Period &&
      (event->modifiers() & Qt::ControlModifier)) {
    const int line = textCursor().blockNumber() + 1;
    const auto fixes = getQuickFixes(line);
    if (!fixes.isEmpty()) {
      emit quickFixesAvailable(fixes);
    }
    event->accept();
    return;
  }

  // Handle Tab for snippet navigation or normal indent
  if ((event->key() == Qt::Key_Tab) &&
      !(event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    if (m_inSnippetMode) {
      nextTabstop();
      event->accept();
      return;
    }
    handleTabKey(event);
    return;
  }

  // Handle Shift+Tab for snippet navigation or normal outdent
  if ((event->key() == Qt::Key_Backtab) ||
      ((event->key() == Qt::Key_Tab) &&
       (event->modifiers() & Qt::ShiftModifier))) {
    if (m_inSnippetMode) {
      previousTabstop();
      event->accept();
      return;
    }
    handleBacktabKey(event);
    return;
  }

  // Escape exits snippet mode
  if (event->key() == Qt::Key_Escape && m_inSnippetMode) {
    m_inSnippetMode = false;
    m_tabstopPositions.clear();
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    handleReturnKey(event);
    return;
  }

  if (!m_completer) {
    QPlainTextEdit::keyPressEvent(event);
    return;
  }

  const bool isShortcut = (event->modifiers() & Qt::ControlModifier) &&
                          event->key() == Qt::Key_Space;

  if (!isShortcut) {
    QPlainTextEdit::keyPressEvent(event);
  } else {
    event->accept();
  }

  if (!m_completer->widget()) {
    m_completer->setWidget(this);
  }

  if (!isVisible() || !window() || !window()->windowHandle()) {
    if (m_completer->popup()) {
      m_completer->popup()->hide();
    }
    return;
  }

  const QString completionPrefix = textUnderCursor();
  if (static_cast<int>(completionPrefix.size()) < 2 && !isShortcut) {
    m_completer->popup()->hide();
    return;
  }

  if (!m_completer->completionModel() ||
      m_completer->completionModel()->rowCount() == 0) {
    m_completer->popup()->hide();
    return;
  }

  if (completionPrefix != m_completer->completionPrefix()) {
    m_completer->setCompletionPrefix(completionPrefix);
    m_completer->popup()->setCurrentIndex(
        m_completer->completionModel()->index(0, 0));
  }

  QRect cr = cursorRect();
  if (auto *popup = m_completer->popup()) {
    const int baseWidth = popup->sizeHintForColumn(0);
    const int scrollWidth = popup->verticalScrollBar()
                                ? popup->verticalScrollBar()->sizeHint().width()
                                : 0;
    cr.setWidth(baseWidth + scrollWidth);
    m_completer->complete(cr);
  }
}

void NMScriptEditor::mousePressEvent(QMouseEvent *event) {
  // Ctrl+Click for go-to-definition
  if (event->button() == Qt::LeftButton &&
      (event->modifiers() & Qt::ControlModifier)) {
    const QString symbol = symbolAtPosition(event->pos());
    if (!symbol.isEmpty()) {
      const QString key = symbol.toLower();
      if (m_symbolLocations.contains(key)) {
        emit goToDefinitionRequested(symbol, m_symbolLocations.value(key));
        event->accept();
        return;
      }
    }
  }
  QPlainTextEdit::mousePressEvent(event);
}

void NMScriptEditor::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  // Change cursor to pointing hand when hovering over a navigable symbol with
  // Ctrl held
  if (event->modifiers() & Qt::ControlModifier) {
    const QString symbol = symbolAtPosition(event->pos());
    if (!symbol.isEmpty() && m_symbolLocations.contains(symbol.toLower())) {
      viewport()->setCursor(Qt::PointingHandCursor);
    } else {
      viewport()->setCursor(Qt::IBeamCursor);
    }
  } else {
    viewport()->setCursor(Qt::IBeamCursor);
  }

  QTextCursor cursor = cursorForPosition(event->pos());
  cursor.select(QTextCursor::WordUnderCursor);
  const QString token = cursor.selectedText();
  const QString key = token.toLower();

  if (key.isEmpty() || key == m_lastHoverToken) {
    return;
  }

  m_lastHoverToken = key;
  if (m_hoverDocs.contains(key)) {
    QToolTip::showText(event->globalPosition().toPoint(),
                       m_hoverDocs.value(key), this);
    const QString html = m_docHtml.value(key);
    emit hoverDocChanged(key, html);
  } else {
    QToolTip::hideText();
    emit hoverDocChanged(QString(), QString());
  }
}

void NMScriptEditor::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = createStandardContextMenu();
  const auto &palette = NMStyleManager::instance().palette();
  menu->setStyleSheet(QString("QMenu { background-color: %1; color: %2; }"
                              "QMenu::item:selected { background-color: %3; }")
                          .arg(palette.bgMedium.name())
                          .arg(palette.textPrimary.name())
                          .arg(palette.bgLight.name()));

  menu->addSeparator();

  const QString symbol = textUnderCursor();
  const bool hasSymbol = !symbol.isEmpty();
  const bool isNavigable =
      hasSymbol && m_symbolLocations.contains(symbol.toLower());

  QAction *gotoAction = menu->addAction(tr("Go to Definition (F12)"));
  gotoAction->setEnabled(isNavigable);
  connect(gotoAction, &QAction::triggered, this,
          &NMScriptEditor::goToDefinition);

  QAction *findRefsAction = menu->addAction(tr("Find References (Shift+F12)"));
  findRefsAction->setEnabled(hasSymbol);
  connect(findRefsAction, &QAction::triggered, this,
          &NMScriptEditor::findReferences);

  if (isNavigable &&
      m_symbolLocations.value(symbol.toLower()).filePath.contains("scene")) {
    QAction *graphAction =
        menu->addAction(tr("Navigate to Graph (Ctrl+Shift+G)"));
    connect(graphAction, &QAction::triggered, this,
            [this, symbol]() { emit navigateToGraphNodeRequested(symbol); });
  }

  menu->addSeparator();

  QAction *snippetAction = menu->addAction(tr("Insert Snippet... (Ctrl+J)"));
  connect(snippetAction, &QAction::triggered, this,
          &NMScriptEditor::showSnippetMenu);

  menu->exec(event->globalPos());
  delete menu;
}

QString NMScriptEditor::textUnderCursor() const {
  QTextCursor cursor = textCursor();
  cursor.select(QTextCursor::WordUnderCursor);
  return cursor.selectedText();
}

int NMScriptEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = std::max(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  const int space =
      12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
  return space;
}

void NMScriptEditor::updateLineNumberAreaWidth(int newBlockCount) {
  Q_UNUSED(newBlockCount);
  const int rightMargin = m_minimapEnabled && m_minimap ? 120 : 0;
  // Issue #239: Include graph gutter width in left margin
  setViewportMargins(
      breakpointGutterWidth() + graphGutterWidth() + lineNumberAreaWidth() + foldingAreaWidth(), 0,
      rightMargin, 0);
}

void NMScriptEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy) {
    if (m_breakpointGutter) {
      m_breakpointGutter->scroll(0, dy);
    }
    // Issue #239: Scroll graph gutter
    if (m_graphGutter) {
      m_graphGutter->scroll(0, dy);
    }
    if (m_lineNumberArea) {
      m_lineNumberArea->scroll(0, dy);
    }
    if (m_foldingArea) {
      m_foldingArea->scroll(0, dy);
    }
  } else {
    if (m_breakpointGutter) {
      m_breakpointGutter->update(0, rect.y(), m_breakpointGutter->width(),
                                 rect.height());
    }
    // Issue #239: Update graph gutter
    if (m_graphGutter) {
      m_graphGutter->update(0, rect.y(), m_graphGutter->width(), rect.height());
    }
    if (m_lineNumberArea) {
      m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(),
                               rect.height());
    }
    if (m_foldingArea) {
      m_foldingArea->update(0, rect.y(), m_foldingArea->width(), rect.height());
    }
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

void NMScriptEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  int xOffset = cr.left();

  if (m_breakpointGutter) {
    m_breakpointGutter->setGeometry(
        QRect(xOffset, cr.top(), breakpointGutterWidth(), cr.height()));
    xOffset += breakpointGutterWidth();
  }

  // Issue #239: Position graph gutter after breakpoint gutter
  if (m_graphGutter) {
    m_graphGutter->setGeometry(
        QRect(xOffset, cr.top(), graphGutterWidth(), cr.height()));
    xOffset += graphGutterWidth();
  }

  if (m_lineNumberArea) {
    m_lineNumberArea->setGeometry(
        QRect(xOffset, cr.top(), lineNumberAreaWidth(), cr.height()));
    xOffset += lineNumberAreaWidth();
  }

  if (m_foldingArea) {
    m_foldingArea->setGeometry(
        QRect(xOffset, cr.top(), foldingAreaWidth(), cr.height()));
  }

  if (m_minimap && m_minimapEnabled) {
    const int minimapWidth = 120;
    m_minimap->setGeometry(cr.right() - minimapWidth, cr.top(), minimapWidth,
                           cr.height());
  }
}

void NMScriptEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
  if (!m_lineNumberArea) {
    return;
  }
  QPainter painter(m_lineNumberArea);
  const auto &palette = NMStyleManager::instance().palette();
  painter.fillRect(event->rect(), palette.bgMedium);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const QString number = QString::number(blockNumber + 1);
      painter.setPen(palette.textSecondary);
      painter.drawText(0, top, m_lineNumberArea->width() - 6,
                       fontMetrics().height(), Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

void NMScriptEditor::highlightCurrentLine() {
  if (isReadOnly()) {
    return;
  }

  QTextEdit::ExtraSelection selection;
  const auto &palette = NMStyleManager::instance().palette();
  selection.format.setBackground(QColor(palette.bgLight.red(),
                                        palette.bgLight.green(),
                                        palette.bgLight.blue(), 60));
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = textCursor();
  selection.cursor.clearSelection();

  setExtraSelections({selection});
}

QString NMScriptEditor::indentForCurrentLine(int *outLogicalIndent) const {
  const QString text = textCursor().block().text();
  int leading = 0;
  while (leading < static_cast<int>(text.size()) &&
         text.at(leading).isSpace()) {
    ++leading;
  }
  if (outLogicalIndent) {
    *outLogicalIndent = leading;
  }
  QString indent(leading, ' ');
  const QString trimmed = text.trimmed();
  if (trimmed.endsWith("{")) {
    indent.append(QString(m_indentSize, ' '));
  }
  return indent;
}

void NMScriptEditor::handleReturnKey(QKeyEvent *event) {
  QTextCursor cursor = textCursor();
  cursor.beginEditBlock();
  const QString indent = indentForCurrentLine();
  cursor.insertBlock();
  cursor.insertText(indent);
  cursor.endEditBlock();
  setTextCursor(cursor);
  event->accept();
}

void NMScriptEditor::handleTabKey(QKeyEvent *event) {
  indentSelection(m_indentSize);
  event->accept();
}

void NMScriptEditor::handleBacktabKey(QKeyEvent *event) {
  indentSelection(-m_indentSize);
  event->accept();
}

void NMScriptEditor::indentSelection(int delta) {
  QTextCursor cursor = textCursor();
  const QString indentUnit(m_indentSize, ' ');

  cursor.beginEditBlock();

  if (!cursor.hasSelection()) {
    QTextBlock block = cursor.block();
    if (delta > 0) {
      cursor.insertText(indentUnit);
    } else {
      const QString text = block.text();
      int removable = 0;
      while (removable < static_cast<int>(text.size()) &&
             text.at(removable).isSpace() && removable < m_indentSize) {
        ++removable;
      }
      if (removable > 0) {
        cursor.setPosition(block.position());
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                            removable);
        cursor.removeSelectedText();
      }
    }
    cursor.endEditBlock();
    return;
  }

  const int start = cursor.selectionStart();
  int end = cursor.selectionEnd();

  QTextBlock block = document()->findBlock(start);
  while (block.isValid() && block.position() <= end) {
    QTextCursor lineCursor(block);
    if (delta > 0) {
      lineCursor.insertText(indentUnit);
      end += m_indentSize;
    } else {
      const QString text = block.text();
      int removable = 0;
      while (removable < static_cast<int>(text.size()) &&
             text.at(removable).isSpace() && removable < m_indentSize) {
        ++removable;
      }
      if (removable > 0) {
        lineCursor.setPosition(block.position());
        lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                                removable);
        lineCursor.removeSelectedText();
        end -= removable;
      }
    }
    block = block.next();
  }

  cursor.endEditBlock();
}

void NMScriptEditor::insertCompletion(const QString &completion) {
  if (!m_completer) {
    return;
  }
  QTextCursor cursor = textCursor();
  const int prefixLength =
      static_cast<int>(m_completer->completionPrefix().length());
  cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, prefixLength);
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                      prefixLength);
  cursor.insertText(completion);
  setTextCursor(cursor);
}

void NMScriptEditor::refreshDynamicCompletions() {
  if (!m_completer) {
    return;
  }

  const QString text = document()->toPlainText();
  QStringList dynamicWords;

  const QRegularExpression reScene("\\bscene\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reCharacter(
      "\\bcharacter\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reSet("\\bset\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reFlag("\\bflag\\s+([A-Za-z_][A-Za-z0-9_]*)");

  const QRegularExpression *patterns[] = {&reScene, &reCharacter, &reSet,
                                          &reFlag};
  for (const auto *pattern : patterns) {
    QRegularExpressionMatchIterator it = pattern->globalMatch(text);
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      const QString token = match.captured(1);
      if (!token.isEmpty()) {
        dynamicWords.append(token);
      }
    }
  }

  dynamicWords.removeDuplicates();

  QHash<QString, CompletionEntry> merged;
  for (const auto &entry : m_staticCompletionEntries) {
    merged.insert(entry.text.toLower(), entry);
  }

  for (const auto &word : dynamicWords) {
    const QString key = word.toLower();
    if (!merged.contains(key)) {
      merged.insert(key, {word, "local"});
    }
  }

  QList<CompletionEntry> combined = merged.values();
  std::sort(combined.begin(), combined.end(),
            [](const CompletionEntry &a, const CompletionEntry &b) {
              return a.text.compare(b.text, Qt::CaseInsensitive) < 0;
            });

  rebuildCompleterModel(combined);
  m_cachedCompletionEntries = combined;
}

void NMScriptEditor::rebuildCompleterModel(
    const QList<CompletionEntry> &entries) {
  if (!m_completer) {
    return;
  }
  auto *model =
      new QStandardItemModel(static_cast<int>(entries.size()), 1, m_completer);
  int row = 0;
  for (const auto &entry : entries) {
    auto *item = new QStandardItem(entry.text);
    item->setData(entry.detail, Qt::UserRole + 1);
    model->setItem(row, 0, item);
    ++row;
  }
  m_completer->setModel(model);
}

// ============================================================================
// Breakpoint Support
// ============================================================================

void NMScriptEditor::setBreakpoints(const QSet<int> &lines) {
  m_breakpoints = lines;
  if (m_breakpointGutter) {
    m_breakpointGutter->update();
  }
}

void NMScriptEditor::toggleBreakpoint(int line) {
  if (m_breakpoints.contains(line)) {
    m_breakpoints.remove(line);
  } else {
    m_breakpoints.insert(line);
  }
  if (m_breakpointGutter) {
    m_breakpointGutter->update();
  }
  emit breakpointToggled(line);
}

void NMScriptEditor::setCurrentExecutionLine(int line) {
  m_currentExecutionLine = line;
  if (m_breakpointGutter) {
    m_breakpointGutter->update();
  }
  // Scroll to the execution line if it's not visible
  if (line > 0) {
    QTextBlock block = document()->findBlockByNumber(line - 1);
    if (block.isValid()) {
      QTextCursor cursor(block);
      setTextCursor(cursor);
      centerCursor();
    }
  }
  // Trigger repaint to show execution highlight
  viewport()->update();
}

void NMScriptEditor::breakpointGutterPaintEvent(QPaintEvent *event) {
  if (!m_breakpointGutter) {
    return;
  }

  QPainter painter(m_breakpointGutter);
  const auto &palette = NMStyleManager::instance().palette();

  // Fill background
  painter.fillRect(event->rect(), palette.bgMedium);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());
  const int gutterWidth = breakpointGutterWidth();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int lineNumber = blockNumber + 1; // 1-based

      // Draw current execution marker (yellow arrow)
      if (m_currentExecutionLine == lineNumber) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor("#ffeb3b")); // Yellow
        painter.setPen(QPen(QColor("#f57c00"), 1));

        // Draw arrow pointing right
        QPolygonF arrow;
        const int centerY = top + fontMetrics().height() / 2;
        const int arrowSize = 5;
        arrow << QPointF(2, centerY - arrowSize)
              << QPointF(gutterWidth - 2, centerY)
              << QPointF(2, centerY + arrowSize);
        painter.drawPolygon(arrow);
      }

      // Draw breakpoint indicator (red circle)
      if (m_breakpoints.contains(lineNumber)) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor("#f44336")); // Red
        painter.setPen(QPen(QColor("#b71c1c"), 1));

        const int diameter = 10;
        const int x = (gutterWidth - diameter) / 2;
        const int y = top + (fontMetrics().height() - diameter) / 2;
        painter.drawEllipse(x, y, diameter, diameter);
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ============================================================================
// Issue #239: Graph Integration Gutter Methods
// ============================================================================

void NMScriptEditor::setGraphConnectedScenes(
    const QHash<int, QString> &sceneLines) {
  m_graphConnectedScenes = sceneLines;
  if (m_graphGutter) {
    m_graphGutter->update();
  }
  qDebug() << "[ScriptEditor] Graph-connected scenes updated:"
           << sceneLines.size() << "scenes";
}

void NMScriptEditor::graphGutterPaintEvent(QPaintEvent *event) {
  if (!m_graphGutter) {
    return;
  }

  QPainter painter(m_graphGutter);
  const auto &palette = NMStyleManager::instance().palette();

  // Fill background
  painter.fillRect(event->rect(), palette.bgMedium);

  if (m_graphConnectedScenes.isEmpty()) {
    return; // No connected scenes to display
  }

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());
  const int gutterWidth = graphGutterWidth();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int lineNumber = blockNumber + 1; // 1-based

      // Draw graph-connected scene indicator (green diamond)
      if (m_graphConnectedScenes.contains(lineNumber)) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor("#4CAF50")); // Green
        painter.setPen(QPen(QColor("#2E7D32"), 1));

        // Draw diamond shape to differentiate from breakpoint circle
        const int size = 8;
        const int centerX = gutterWidth / 2;
        const int centerY = top + fontMetrics().height() / 2;

        QPolygonF diamond;
        diamond << QPointF(centerX, centerY - size / 2)      // Top
                << QPointF(centerX + size / 2, centerY)      // Right
                << QPointF(centerX, centerY + size / 2)      // Bottom
                << QPointF(centerX - size / 2, centerY);     // Left
        painter.drawPolygon(diamond);
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

} // namespace NovelMind::editor::qt
