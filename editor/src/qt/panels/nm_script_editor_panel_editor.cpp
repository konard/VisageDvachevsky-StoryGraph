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
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextStream>
#include <QToolTip>
#include <QVBoxLayout>
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

  const QStringList keywords = detail::buildCompletionWords();
  for (const auto &word : keywords) {
    Rule rule;
    rule.pattern = QRegularExpression(QString("\\b%1\\b").arg(word));
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
      while (startPos < text.size() && text.at(startPos).isSpace()) {
        ++startPos;
      }
      if (startPos < text.size()) {
        setFormat(startPos, clampToInt(text.size()) - startPos, format);
      }
    }
  }
}

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

  m_lineNumberArea = new NMScriptEditorLineNumberArea(this);
  connect(this, &QPlainTextEdit::blockCountChanged, this,
          &NMScriptEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this,
          &NMScriptEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &NMScriptEditor::highlightCurrentLine);
  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  m_highlighter = new NMScriptHighlighter(document());

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
}

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
  QTextCursor cursor = textCursor();
  cursor.beginEditBlock();

  QString snippet;
  int cursorOffset = 0; // Position cursor within snippet

  if (snippetType == "scene") {
    snippet = "scene scene_name {\n  say Narrator \"Description\"\n}\n";
    cursorOffset = 6; // Position at scene_name
  } else if (snippetType == "choice") {
    snippet = "choice {\n  \"Option 1\" -> scene_target1\n  \"Option 2\" -> "
              "scene_target2\n}\n";
    cursorOffset = 12; // Position at first option
  } else if (snippetType == "if") {
    snippet = "if flag condition {\n  // true branch\n} else {\n  // false "
              "branch\n}\n";
    cursorOffset = 8; // Position at condition
  } else if (snippetType == "goto") {
    snippet = "goto scene_name\n";
    cursorOffset = 5; // Position at scene_name
  } else if (snippetType == "character") {
    snippet = "character CharName(name=\"Display Name\", color=\"#4A9FD9\")\n";
    cursorOffset = 10; // Position at CharName
  } else if (snippetType == "say") {
    snippet = "say Character \"Dialogue text\"\n";
    cursorOffset = 4; // Position at Character
  } else if (snippetType == "show") {
    snippet = "show background \"asset_path\"\n";
    cursorOffset = 17; // Position at asset_path
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
  menu.setStyleSheet(
      "QMenu { background-color: #2d2d2d; color: #e0e0e0; }"
      "QMenu::item:selected { background-color: #404040; }");

  menu.addAction(tr("Scene block"), this,
                 [this]() { insertSnippet("scene"); });
  menu.addAction(tr("Choice block"), this,
                 [this]() { insertSnippet("choice"); });
  menu.addAction(tr("If/Else block"), this,
                 [this]() { insertSnippet("if"); });
  menu.addAction(tr("Goto statement"), this,
                 [this]() { insertSnippet("goto"); });
  menu.addSeparator();
  menu.addAction(tr("Character declaration"), this,
                 [this]() { insertSnippet("character"); });
  menu.addAction(tr("Say dialogue"), this,
                 [this]() { insertSnippet("say"); });
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
  if (event->key() == Qt::Key_F12 &&
      (event->modifiers() & Qt::ShiftModifier)) {
    findReferences();
    event->accept();
    return;
  }

  // Ctrl+J: Insert Snippet
  if (event->key() == Qt::Key_J &&
      (event->modifiers() & Qt::ControlModifier)) {
    showSnippetMenu();
    event->accept();
    return;
  }

  // Ctrl+G: Navigate to Graph (for scenes)
  if (event->key() == Qt::Key_G &&
      (event->modifiers() & Qt::ControlModifier) &&
      (event->modifiers() & Qt::ShiftModifier)) {
    const QString symbol = textUnderCursor();
    if (!symbol.isEmpty()) {
      emit navigateToGraphNodeRequested(symbol);
    }
    event->accept();
    return;
  }

  if ((event->key() == Qt::Key_Tab) &&
      !(event->modifiers() & Qt::ControlModifier) &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    handleTabKey(event);
    return;
  }

  if ((event->key() == Qt::Key_Backtab) ||
      ((event->key() == Qt::Key_Tab) &&
       (event->modifiers() & Qt::ShiftModifier))) {
    handleBacktabKey(event);
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
  if (completionPrefix.size() < 2 && !isShortcut) {
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
  connect(gotoAction, &QAction::triggered, this, &NMScriptEditor::goToDefinition);

  QAction *findRefsAction = menu->addAction(tr("Find References (Shift+F12)"));
  findRefsAction->setEnabled(hasSymbol);
  connect(findRefsAction, &QAction::triggered, this, &NMScriptEditor::findReferences);

  if (isNavigable && m_symbolLocations.value(symbol.toLower()).filePath.contains("scene")) {
    QAction *graphAction = menu->addAction(tr("Navigate to Graph (Ctrl+Shift+G)"));
    connect(graphAction, &QAction::triggered, this,
            [this, symbol]() { emit navigateToGraphNodeRequested(symbol); });
  }

  menu->addSeparator();

  QAction *snippetAction = menu->addAction(tr("Insert Snippet... (Ctrl+J)"));
  connect(snippetAction, &QAction::triggered, this, &NMScriptEditor::showSnippetMenu);

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
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void NMScriptEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (!m_lineNumberArea) {
    return;
  }
  if (dy) {
    m_lineNumberArea->scroll(0, dy);
  } else {
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(),
                             rect.height());
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

void NMScriptEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  if (!m_lineNumberArea) {
    return;
  }
  QRect cr = contentsRect();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
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
  while (leading < text.size() && text.at(leading).isSpace()) {
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
      while (removable < text.size() && text.at(removable).isSpace() &&
             removable < m_indentSize) {
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
      while (removable < text.size() && text.at(removable).isSpace() &&
             removable < m_indentSize) {
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

} // namespace NovelMind::editor::qt
