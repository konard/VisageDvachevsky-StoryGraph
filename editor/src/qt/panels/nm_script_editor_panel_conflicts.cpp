#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/widgets/nm_scene_preview_widget.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/dialogs/nm_script_welcome_dialog.hpp"
#include "NovelMind/editor/script_project_context.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMutexLocker>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextStream>
#include <QToolButton>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <filesystem>
#include <limits>

#include "nm_script_editor_panel_detail.hpp"

namespace NovelMind::editor::qt {

void NMScriptEditorPanel::onFileChanged(const QString& path) {
  // Find if this file is open in an editor tab
  NMScriptEditor* editor = findEditorForPath(path);
  if (!editor) {
    // File not open, just refresh symbol index
    refreshSymbolIndex();
    return;
  }

  // Check if tab has unsaved changes
  const bool hasUnsaved = isTabModified(editor);

  // Get file modification time
  QFileInfo fileInfo(path);
  if (!fileInfo.exists()) {
    // File was deleted, handle separately if needed
    core::Logger::instance().warning("File was deleted externally: " + path.toStdString());
    return;
  }
  const QDateTime fileMTime = fileInfo.lastModified();

  // Compare with last known save time
  const QDateTime editorMTime = getEditorSaveTime(editor);

  // If we have a recorded save time and file time is not newer, this was our
  // own save
  if (editorMTime.isValid() && fileMTime <= editorMTime) {
    // This was our own save, ignore
    refreshSymbolIndex();
    return;
  }

  // External modification detected
  if (hasUnsaved) {
    showConflictDialog(path, editor);
  } else {
    showReloadPrompt(path, editor);
  }
}

void NMScriptEditorPanel::onDirectoryChanged(const QString& path) {
  Q_UNUSED(path);
  // Directory changes don't affect open files, just refresh file list
  refreshFileList();
  refreshSymbolIndex();
}

void NMScriptEditorPanel::showConflictDialog(const QString& path, NMScriptEditor* editor) {
  if (!editor) {
    return;
  }

  const QString fileName = QFileInfo(path).fileName();
  const QString message = tr("The file \"%1\" has been modified externally, but you have unsaved "
                             "changes in the editor.\n\nWhat would you like to do?")
                              .arg(fileName);

  // Create custom dialog with three options
  auto* dialog = new QDialog(this);
  dialog->setWindowTitle(tr("File Conflict Detected"));
  dialog->setModal(true);
  dialog->setMinimumWidth(450);

  auto* layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Warning icon and message
  auto* messageLayout = new QHBoxLayout();
  auto* iconLabel = new QLabel(dialog);
  iconLabel->setText("⚠️");
  iconLabel->setStyleSheet("font-size: 32px;");
  messageLayout->addWidget(iconLabel);

  auto* messageLabel = new QLabel(message, dialog);
  messageLabel->setWordWrap(true);
  messageLayout->addWidget(messageLabel, 1);
  layout->addLayout(messageLayout);

  // Button layout
  auto* buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  auto* keepMyChangesBtn = new QPushButton(tr("Keep My Changes"), dialog);
  keepMyChangesBtn->setToolTip(
      tr("Discard the external file version and keep your unsaved changes"));
  buttonLayout->addWidget(keepMyChangesBtn);

  auto* useFileVersionBtn = new QPushButton(tr("Use File Version"), dialog);
  useFileVersionBtn->setToolTip(tr("Discard your unsaved changes and reload from disk"));
  buttonLayout->addWidget(useFileVersionBtn);

  auto* cancelBtn = new QPushButton(tr("Cancel"), dialog);
  cancelBtn->setToolTip(tr("Do nothing for now"));
  buttonLayout->addWidget(cancelBtn);

  layout->addLayout(buttonLayout);

  // Connect buttons
  int result = 0; // 1 = Keep Mine, 2 = Use File, 0 = Cancel
  connect(keepMyChangesBtn, &QPushButton::clicked, dialog, [dialog, &result]() {
    result = 1;
    dialog->accept();
  });
  connect(useFileVersionBtn, &QPushButton::clicked, dialog, [dialog, &result]() {
    result = 2;
    dialog->accept();
  });
  connect(cancelBtn, &QPushButton::clicked, dialog, [dialog]() { dialog->reject(); });

  dialog->exec();

  if (result == 1) {
    // Keep My Changes - force save over file version
    core::Logger::instance().info("User chose to keep editor changes for: " + path.toStdString());
    saveEditor(editor);
  } else if (result == 2) {
    // Use File Version - reload from disk
    core::Logger::instance().info("User chose to reload file from disk: " + path.toStdString());
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString content = QString::fromUtf8(file.readAll());
      editor->setPlainText(content);
      file.close();

      // Update save time to prevent immediate re-trigger
      setEditorSaveTime(editor, QFileInfo(path).lastModified());

      // Remove "*" from tab
      const int index = m_tabs->indexOf(editor);
      if (index >= 0) {
        m_tabs->setTabText(index, fileName);
      }

      refreshSymbolIndex();
      m_diagnosticsTimer.start();
    }
  }
  // else: Cancel - do nothing

  delete dialog;
}

void NMScriptEditorPanel::showReloadPrompt(const QString& path, NMScriptEditor* editor) {
  if (!editor) {
    return;
  }

  const QString fileName = QFileInfo(path).fileName();
  const QString message = tr("The file \"%1\" has been modified externally.\n\nDo you want to "
                             "reload it from disk?")
                              .arg(fileName);

  // Create simple reload dialog
  auto* dialog = new QDialog(this);
  dialog->setWindowTitle(tr("File Changed Externally"));
  dialog->setModal(true);
  dialog->setMinimumWidth(400);

  auto* layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Info icon and message
  auto* messageLayout = new QHBoxLayout();
  auto* iconLabel = new QLabel(dialog);
  iconLabel->setText("ℹ️");
  iconLabel->setStyleSheet("font-size: 28px;");
  messageLayout->addWidget(iconLabel);

  auto* messageLabel = new QLabel(message, dialog);
  messageLabel->setWordWrap(true);
  messageLayout->addWidget(messageLabel, 1);
  layout->addLayout(messageLayout);

  // Button layout
  auto* buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  auto* reloadBtn = new QPushButton(tr("Reload"), dialog);
  reloadBtn->setToolTip(tr("Reload the file from disk"));
  buttonLayout->addWidget(reloadBtn);

  auto* ignoreBtn = new QPushButton(tr("Ignore"), dialog);
  ignoreBtn->setToolTip(tr("Keep current version, you will be warned on save"));
  buttonLayout->addWidget(ignoreBtn);

  layout->addLayout(buttonLayout);

  // Connect buttons
  bool shouldReload = false;
  connect(reloadBtn, &QPushButton::clicked, dialog, [dialog, &shouldReload]() {
    shouldReload = true;
    dialog->accept();
  });
  connect(ignoreBtn, &QPushButton::clicked, dialog, [dialog]() { dialog->reject(); });

  dialog->exec();

  if (shouldReload) {
    core::Logger::instance().info("User chose to reload file: " + path.toStdString());
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString content = QString::fromUtf8(file.readAll());
      editor->setPlainText(content);
      file.close();

      // Update save time to prevent immediate re-trigger
      setEditorSaveTime(editor, QFileInfo(path).lastModified());

      refreshSymbolIndex();
      m_diagnosticsTimer.start();
    }
  } else {
    core::Logger::instance().info("User chose to ignore external change for: " +
                                  path.toStdString());
  }

  delete dialog;
}

} // namespace NovelMind::editor::qt
