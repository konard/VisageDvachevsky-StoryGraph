#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

namespace NovelMind::editor::qt {

// ============================================================================
// NMScriptCommandPalette - VSCode-like Command Palette
// ============================================================================

NMScriptCommandPalette::NMScriptCommandPalette(QWidget *parent)
    : QWidget(parent) {
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
  setFixedWidth(500);
  setMaximumHeight(400);

  const auto &palette = NMStyleManager::instance().palette();
  setStyleSheet(
      QString("QWidget { background-color: %1; border: 1px solid %2; }"
              "QLineEdit { background-color: %3; color: %4; border: none; "
              "padding: 8px; font-size: 14px; }"
              "QListWidget { background-color: %1; color: %4; border: none; }"
              "QListWidget::item { padding: 6px 12px; }"
              "QListWidget::item:selected { background-color: %5; }"
              "QListWidget::item:hover { background-color: %6; }")
          .arg(palette.bgMedium.name())
          .arg(palette.borderLight.name())
          .arg(palette.bgDark.name())
          .arg(palette.textPrimary.name())
          .arg(palette.accentPrimary.name())
          .arg(palette.bgLight.name()));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("> Type a command..."));
  m_filterEdit->installEventFilter(this);
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &NMScriptCommandPalette::onFilterChanged);

  m_commandList = new QListWidget(this);
  connect(m_commandList, &QListWidget::itemActivated, this,
          &NMScriptCommandPalette::onItemActivated);
  connect(m_commandList, &QListWidget::itemDoubleClicked, this,
          &NMScriptCommandPalette::onItemActivated);

  layout->addWidget(m_filterEdit);
  layout->addWidget(m_commandList);
}

void NMScriptCommandPalette::addCommand(const Command &cmd) {
  m_commands.append(cmd);
}

void NMScriptCommandPalette::show() {
  m_filterEdit->clear();
  updateCommandList(QString());

  // Position in center of parent
  if (parentWidget()) {
    QPoint center = parentWidget()->rect().center();
    move(parentWidget()->mapToGlobal(center) - QPoint(width() / 2, 100));
  }

  QWidget::show();
  m_filterEdit->setFocus();
  raise();
}

bool NMScriptCommandPalette::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_filterEdit && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);

    if (keyEvent->key() == Qt::Key_Escape) {
      hide();
      return true;
    }

    if (keyEvent->key() == Qt::Key_Down) {
      int row = m_commandList->currentRow();
      if (row < m_commandList->count() - 1) {
        m_commandList->setCurrentRow(row + 1);
      }
      return true;
    }

    if (keyEvent->key() == Qt::Key_Up) {
      int row = m_commandList->currentRow();
      if (row > 0) {
        m_commandList->setCurrentRow(row - 1);
      }
      return true;
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
      QListWidgetItem *item = m_commandList->currentItem();
      if (item) {
        onItemActivated(item);
      }
      return true;
    }
  }

  return QWidget::eventFilter(obj, event);
}

void NMScriptCommandPalette::onFilterChanged(const QString &filter) {
  updateCommandList(filter);
}

void NMScriptCommandPalette::onItemActivated(QListWidgetItem *item) {
  if (!item) {
    return;
  }

  const int index = item->data(Qt::UserRole).toInt();
  if (index >= 0 && index < static_cast<int>(m_commands.size())) {
    hide();
    if (m_commands[index].action) {
      m_commands[index].action();
      emit commandExecuted(m_commands[index].name);
    }
  }
}

void NMScriptCommandPalette::updateCommandList(const QString &filter) {
  m_commandList->clear();

  for (int i = 0; i < static_cast<int>(m_commands.size()); ++i) {
    const Command &cmd = m_commands[i];

    // Filter by name or category
    if (!filter.isEmpty() && !cmd.name.contains(filter, Qt::CaseInsensitive) &&
        !cmd.category.contains(filter, Qt::CaseInsensitive)) {
      continue;
    }

    QString label = cmd.name;
    if (!cmd.shortcut.isEmpty()) {
      label += QString("  [%1]").arg(cmd.shortcut);
    }

    auto *item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, i);
    if (!cmd.category.isEmpty()) {
      item->setToolTip(cmd.category);
    }
    m_commandList->addItem(item);
  }

  if (m_commandList->count() > 0) {
    m_commandList->setCurrentRow(0);
  }

  // Adjust height based on content
  const int itemHeight = 30;
  const int maxVisibleItems = 10;
  const int visibleItems = std::min(m_commandList->count(), maxVisibleItems);
  m_commandList->setFixedHeight(visibleItems * itemHeight);
  adjustSize();
}

} // namespace NovelMind::editor::qt
