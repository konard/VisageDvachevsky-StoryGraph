#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMFindReplaceWidget - VSCode-like Find and Replace
// ============================================================================

NMFindReplaceWidget::NMFindReplaceWidget(QWidget *parent) : QWidget(parent) {
  const auto &palette = NMStyleManager::instance().palette();
  setStyleSheet(
      QString("QWidget { background-color: %1; }"
              "QLineEdit { background-color: %2; color: %3; border: 1px solid "
              "%4; padding: 4px; }"
              "QCheckBox { color: %3; }"
              "QPushButton { background-color: %5; color: %3; border: none; "
              "padding: 4px 8px; }"
              "QPushButton:hover { background-color: %6; }")
          .arg(palette.bgMedium.name())
          .arg(palette.bgDark.name())
          .arg(palette.textPrimary.name())
          .arg(palette.borderLight.name())
          .arg(palette.bgLight.name())
          .arg(palette.accentPrimary.name()));

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  // Search row
  auto *searchRow = new QHBoxLayout();
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(tr("Find"));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &NMFindReplaceWidget::onSearchTextChanged);
  connect(m_searchEdit, &QLineEdit::returnPressed, this,
          &NMFindReplaceWidget::findNext);

  auto& iconMgr = NMIconManager::instance();
  auto *findPrevBtn = new QPushButton(this);
  findPrevBtn->setIcon(iconMgr.getIcon("arrow-left", 16));
  findPrevBtn->setToolTip(tr("Find Previous (Shift+Enter)"));
  findPrevBtn->setFixedWidth(30);
  connect(findPrevBtn, &QPushButton::clicked, this,
          &NMFindReplaceWidget::findPrevious);

  auto *findNextBtn = new QPushButton(this);
  findNextBtn->setIcon(iconMgr.getIcon("arrow-right", 16));
  findNextBtn->setToolTip(tr("Find Next (Enter)"));
  findNextBtn->setFixedWidth(30);
  connect(findNextBtn, &QPushButton::clicked, this,
          &NMFindReplaceWidget::findNext);

  m_matchCountLabel = new QLabel(this);
  m_matchCountLabel->setStyleSheet(
      QString("color: %1; padding: 0 8px;").arg(palette.textSecondary.name()));

  m_closeBtn = new QPushButton(this);
  m_closeBtn->setIcon(iconMgr.getIcon("file-close", 16));
  m_closeBtn->setFixedWidth(24);
  m_closeBtn->setToolTip(tr("Close (Escape)"));
  connect(m_closeBtn, &QPushButton::clicked, this,
          &NMFindReplaceWidget::closeRequested);

  searchRow->addWidget(m_searchEdit, 1);
  searchRow->addWidget(findPrevBtn);
  searchRow->addWidget(findNextBtn);
  searchRow->addWidget(m_matchCountLabel);
  searchRow->addWidget(m_closeBtn);

  // Options row
  auto *optionsRow = new QHBoxLayout();
  m_caseSensitive = new QCheckBox(tr("Aa"), this);
  m_caseSensitive->setToolTip(tr("Match Case"));
  connect(m_caseSensitive, &QCheckBox::toggled, this,
          [this]() { onSearchTextChanged(m_searchEdit->text()); });

  m_wholeWord = new QCheckBox(tr("W"), this);
  m_wholeWord->setToolTip(tr("Match Whole Word"));
  connect(m_wholeWord, &QCheckBox::toggled, this,
          [this]() { onSearchTextChanged(m_searchEdit->text()); });

  m_useRegex = new QCheckBox(tr(".*"), this);
  m_useRegex->setToolTip(tr("Use Regular Expression"));
  connect(m_useRegex, &QCheckBox::toggled, this,
          [this]() { onSearchTextChanged(m_searchEdit->text()); });

  optionsRow->addWidget(m_caseSensitive);
  optionsRow->addWidget(m_wholeWord);
  optionsRow->addWidget(m_useRegex);
  optionsRow->addStretch();

  // Replace row (hidden by default in find-only mode)
  m_replaceRow = new QWidget(this);
  auto *replaceLayout = new QHBoxLayout(m_replaceRow);
  replaceLayout->setContentsMargins(0, 0, 0, 0);

  m_replaceEdit = new QLineEdit(m_replaceRow);
  m_replaceEdit->setPlaceholderText(tr("Replace"));

  auto *replaceBtn = new QPushButton(tr("Replace"), m_replaceRow);
  replaceBtn->setIcon(iconMgr.getIcon("edit-paste", 16));
  connect(replaceBtn, &QPushButton::clicked, this,
          &NMFindReplaceWidget::replaceNext);

  auto *replaceAllBtn = new QPushButton(tr("Replace All"), m_replaceRow);
  replaceAllBtn->setIcon(iconMgr.getIcon("edit-paste", 16));
  connect(replaceAllBtn, &QPushButton::clicked, this,
          &NMFindReplaceWidget::replaceAll);

  replaceLayout->addWidget(m_replaceEdit, 1);
  replaceLayout->addWidget(replaceBtn);
  replaceLayout->addWidget(replaceAllBtn);

  mainLayout->addLayout(searchRow);
  mainLayout->addLayout(optionsRow);
  mainLayout->addWidget(m_replaceRow);

  m_replaceRow->hide();
  setMaximumHeight(120);
}

void NMFindReplaceWidget::setEditor(NMScriptEditor *editor) {
  m_editor = editor;
}

void NMFindReplaceWidget::showFind() {
  m_replaceRow->hide();
  setMaximumHeight(80);
  show();
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void NMFindReplaceWidget::showReplace() {
  m_replaceRow->show();
  setMaximumHeight(120);
  show();
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void NMFindReplaceWidget::setSearchText(const QString &text) {
  m_searchEdit->setText(text);
}

void NMFindReplaceWidget::findNext() { performSearch(true); }

void NMFindReplaceWidget::findPrevious() { performSearch(false); }

void NMFindReplaceWidget::replaceNext() {
  if (!m_editor) {
    return;
  }

  const QString searchText = m_searchEdit->text();
  const QString replaceText = m_replaceEdit->text();
  if (searchText.isEmpty()) {
    return;
  }

  QTextCursor cursor = m_editor->textCursor();
  if (cursor.hasSelection() &&
      cursor.selectedText().compare(searchText, m_caseSensitive->isChecked()
                                                    ? Qt::CaseSensitive
                                                    : Qt::CaseInsensitive) ==
          0) {
    cursor.insertText(replaceText);
    m_editor->setTextCursor(cursor);
  }

  findNext();
  updateMatchCount();
}

void NMFindReplaceWidget::replaceAll() {
  if (!m_editor) {
    return;
  }

  const QString searchText = m_searchEdit->text();
  const QString replaceText = m_replaceEdit->text();
  if (searchText.isEmpty()) {
    return;
  }

  QTextDocument::FindFlags flags;
  if (m_caseSensitive->isChecked()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (m_wholeWord->isChecked()) {
    flags |= QTextDocument::FindWholeWords;
  }

  QTextCursor cursor = m_editor->textCursor();
  cursor.beginEditBlock();
  cursor.movePosition(QTextCursor::Start);
  m_editor->setTextCursor(cursor);

  int count = 0;
  while (true) {
    if (m_useRegex->isChecked()) {
      QRegularExpression re(searchText);
      if (!m_caseSensitive->isChecked()) {
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
      }
      cursor = m_editor->document()->find(re, cursor, flags);
    } else {
      cursor = m_editor->document()->find(searchText, cursor, flags);
    }

    if (cursor.isNull()) {
      break;
    }

    cursor.insertText(replaceText);
    ++count;
  }

  cursor.endEditBlock();
  updateMatchCount();
}

void NMFindReplaceWidget::onSearchTextChanged(const QString &text) {
  Q_UNUSED(text);
  highlightAllMatches();
  updateMatchCount();
}

void NMFindReplaceWidget::performSearch(bool forward) {
  if (!m_editor) {
    return;
  }

  const QString searchText = m_searchEdit->text();
  if (searchText.isEmpty()) {
    clearHighlights();
    return;
  }

  QTextDocument::FindFlags flags;
  if (!forward) {
    flags |= QTextDocument::FindBackward;
  }
  if (m_caseSensitive->isChecked()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (m_wholeWord->isChecked()) {
    flags |= QTextDocument::FindWholeWords;
  }

  QTextCursor cursor = m_editor->textCursor();
  QTextCursor found;

  if (m_useRegex->isChecked()) {
    QRegularExpression re(searchText);
    if (!m_caseSensitive->isChecked()) {
      re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
    found = m_editor->document()->find(re, cursor, flags);
  } else {
    found = m_editor->document()->find(searchText, cursor, flags);
  }

  // Wrap around if not found
  if (found.isNull()) {
    cursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
    if (m_useRegex->isChecked()) {
      QRegularExpression re(searchText);
      if (!m_caseSensitive->isChecked()) {
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
      }
      found = m_editor->document()->find(re, cursor, flags);
    } else {
      found = m_editor->document()->find(searchText, cursor, flags);
    }
  }

  if (!found.isNull()) {
    m_editor->setTextCursor(found);
    m_editor->centerCursor();
  }
}

void NMFindReplaceWidget::highlightAllMatches() {
  clearHighlights();
  if (!m_editor) {
    return;
  }

  const QString searchText = m_searchEdit->text();
  if (searchText.isEmpty()) {
    return;
  }

  QTextCharFormat format;
  format.setBackground(QColor(255, 255, 0, 80)); // Yellow highlight

  QTextDocument::FindFlags flags;
  if (m_caseSensitive->isChecked()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (m_wholeWord->isChecked()) {
    flags |= QTextDocument::FindWholeWords;
  }

  QTextCursor cursor(m_editor->document());
  cursor.movePosition(QTextCursor::Start);

  while (true) {
    QTextCursor found;
    if (m_useRegex->isChecked()) {
      QRegularExpression re(searchText);
      if (!m_caseSensitive->isChecked()) {
        re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
      }
      found = m_editor->document()->find(re, cursor, flags);
    } else {
      found = m_editor->document()->find(searchText, cursor, flags);
    }

    if (found.isNull()) {
      break;
    }

    QTextEdit::ExtraSelection selection;
    selection.cursor = found;
    selection.format = format;
    m_searchHighlights.append(selection);

    cursor = found;
  }

  m_editor->setSearchHighlights(m_searchHighlights);
}

void NMFindReplaceWidget::clearHighlights() {
  m_searchHighlights.clear();
  if (m_editor) {
    m_editor->clearSearchHighlights();
  }
}

int NMFindReplaceWidget::countMatches() const {
  return static_cast<int>(m_searchHighlights.size());
}

void NMFindReplaceWidget::updateMatchCount() {
  const int count = countMatches();
  if (count == 0) {
    m_matchCountLabel->setText(tr("No results"));
  } else {
    m_matchCountLabel->setText(tr("%1 found").arg(count));
  }
}

} // namespace NovelMind::editor::qt
