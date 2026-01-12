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

void NMScriptEditorPanel::refreshFileList() {
  core::Logger::instance().info("applyProjectToPanels: refreshFileList starting");
  m_fileTree->clear();

  const QString rootPath = scriptsRootPath();
  core::Logger::instance().info("applyProjectToPanels: Scripts root path: " +
                                rootPath.toStdString());
  if (rootPath.isEmpty()) {
    if (m_issuesPanel) {
      m_issuesPanel->setIssues({});
    }
    core::Logger::instance().info("applyProjectToPanels: refreshFileList completed (empty path)");
    return;
  }

  QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_fileTree);
  rootItem->setText(0, QFileInfo(rootPath).fileName());
  rootItem->setData(0, Qt::UserRole, rootPath);

  namespace fs = std::filesystem;
  fs::path base(rootPath.toStdString());
  if (!fs::exists(base)) {
    core::Logger::instance().warning("applyProjectToPanels: Scripts path does not exist: " +
                                     base.string());
    core::Logger::instance().info(
        "applyProjectToPanels: refreshFileList completed (path not exists)");
    return;
  }

  core::Logger::instance().info("applyProjectToPanels: Starting directory iteration for scripts");
  int fileCount = 0;
  try {
    // Use skip_permission_denied to avoid hanging on problematic directories
    auto options = fs::directory_options::skip_permission_denied;
    for (const auto& entry : fs::recursive_directory_iterator(base, options)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      if (entry.path().extension() != ".nms") {
        continue;
      }

      fileCount++;
      const fs::path rel = fs::relative(entry.path(), base);
      QTreeWidgetItem* parentItem = rootItem;

      for (const auto& part : rel.parent_path()) {
        const QString partName = QString::fromStdString(part.string());
        QTreeWidgetItem* child = nullptr;
        for (int i = 0; i < parentItem->childCount(); ++i) {
          if (parentItem->child(i)->text(0) == partName) {
            child = parentItem->child(i);
            break;
          }
        }
        if (!child) {
          child = new QTreeWidgetItem(parentItem);
          child->setText(0, partName);
          child->setData(0, Qt::UserRole, QString());
        }
        parentItem = child;
      }

      QTreeWidgetItem* fileItem = new QTreeWidgetItem(parentItem);
      fileItem->setText(0, QString::fromStdString(entry.path().filename().string()));
      fileItem->setData(0, Qt::UserRole, QString::fromStdString(entry.path().string()));
    }
    core::Logger::instance().info("applyProjectToPanels: Directory iteration completed, found " +
                                  std::to_string(fileCount) + " script files");
  } catch (const std::exception& e) {
    core::Logger::instance().warning(std::string("Failed to scan scripts folder: ") + e.what());
  }

  core::Logger::instance().info("applyProjectToPanels: Expanding file tree");
  m_fileTree->expandAll();

  core::Logger::instance().info("applyProjectToPanels: Calling rebuildWatchList");
  rebuildWatchList();
  core::Logger::instance().info("applyProjectToPanels: Calling refreshSymbolIndex");
  refreshSymbolIndex();
  core::Logger::instance().info("applyProjectToPanels: refreshFileList completed");
}

void NMScriptEditorPanel::onFileActivated(QTreeWidgetItem* item, int column) {
  Q_UNUSED(column);
  if (!item) {
    return;
  }
  const QString path = item->data(0, Qt::UserRole).toString();
  if (path.isEmpty()) {
    return;
  }
  openScript(path);
}

void NMScriptEditorPanel::onSaveRequested() {
  if (!m_tabs) {
    return;
  }
  if (auto* editor = qobject_cast<QPlainTextEdit*>(m_tabs->currentWidget())) {
    saveEditor(editor);
  }
  refreshSymbolIndex();
  m_diagnosticsTimer.start();
}

void NMScriptEditorPanel::onSaveAllRequested() {
  for (int i = 0; i < m_tabs->count(); ++i) {
    if (auto* editor = qobject_cast<QPlainTextEdit*>(m_tabs->widget(i))) {
      saveEditor(editor);
    }
  }
  refreshSymbolIndex();
  m_diagnosticsTimer.start();
}

void NMScriptEditorPanel::addEditorTab(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  const QString content = QString::fromUtf8(file.readAll());

  auto* editor = new NMScriptEditor(m_tabs);
  editor->setPlainText(content);
  editor->setHoverDocs(detail::buildHoverDocs());
  editor->setDocHtml(detail::buildDocHtml());
  editor->setSymbolLocations(buildSymbolLocations());

  connect(editor, &NMScriptEditor::requestSave, this, &NMScriptEditorPanel::onSaveRequested);
  connect(editor, &NMScriptEditor::hoverDocChanged, this,
          [this](const QString&, const QString& html) { emit docHtmlChanged(html); });
  connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
    m_diagnosticsTimer.start();
    // Trigger preview update on text change (issue #240)
    onScriptTextChanged();
  });

  // IDE feature connections
  connect(editor, &NMScriptEditor::goToDefinitionRequested, this,
          &NMScriptEditorPanel::onGoToDefinition);
  connect(editor, &NMScriptEditor::findReferencesRequested, this,
          &NMScriptEditorPanel::onFindReferences);
  connect(editor, &NMScriptEditor::navigateToGraphNodeRequested, this,
          &NMScriptEditorPanel::onNavigateToGraphNode);

  // VSCode-like feature connections
  connect(editor, &NMScriptEditor::showFindRequested, this, &NMScriptEditorPanel::showFindDialog);
  connect(editor, &NMScriptEditor::showReplaceRequested, this,
          &NMScriptEditorPanel::showReplaceDialog);
  connect(editor, &NMScriptEditor::showCommandPaletteRequested, this,
          &NMScriptEditorPanel::showCommandPalette);

  // Status bar and breadcrumb connections
  connect(editor, &NMScriptEditor::syntaxHintChanged, this,
          &NMScriptEditorPanel::onSyntaxHintChanged);
  connect(editor, &NMScriptEditor::breadcrumbsChanged, this,
          &NMScriptEditorPanel::onBreadcrumbsChanged);
  connect(editor, &NMScriptEditor::quickFixesAvailable, this,
          &NMScriptEditorPanel::showQuickFixMenu);

  // Breakpoint connection - wire to NMPlayModeController
  connect(editor, &NMScriptEditor::breakpointToggled, this, [this, path](int line) {
    auto& controller = NMPlayModeController::instance();
    controller.toggleSourceBreakpoint(path, line);
  });

  // Sync breakpoints from controller to editor
  auto& controller = NMPlayModeController::instance();
  editor->setBreakpoints(controller.sourceBreakpointsForFile(path));

  // Listen for external breakpoint changes
  connect(&controller, &NMPlayModeController::sourceBreakpointsChanged, editor, [path, editor]() {
    auto& ctrl = NMPlayModeController::instance();
    editor->setBreakpoints(ctrl.sourceBreakpointsForFile(path));
  });

  // Listen for source-level breakpoint hits to show execution line
  connect(&controller, &NMPlayModeController::sourceBreakpointHit, editor,
          [path, editor](const QString& filePath, int line) {
            if (filePath == path) {
              editor->setCurrentExecutionLine(line);
            }
          });

  // Clear execution line when play mode changes
  connect(&controller, &NMPlayModeController::playModeChanged, editor, [editor](int mode) {
    if (mode == NMPlayModeController::Stopped) {
      editor->setCurrentExecutionLine(0);
    }
  });

  // Update cursor position in status bar
  connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, editor]() {
    const QTextCursor cursor = editor->textCursor();
    const int line = cursor.blockNumber() + 1;
    const int col = cursor.positionInBlock() + 1;
    if (m_cursorPosLabel) {
      m_cursorPosLabel->setText(tr("Ln %1, Col %2").arg(line).arg(col));
    }

    // Trigger preview update on cursor position change (issue #240)
    onCursorPositionChanged();

    // Update syntax hint
    const QString hint = editor->getSyntaxHint();
    if (m_syntaxHintLabel && hint != m_syntaxHintLabel->text()) {
      m_syntaxHintLabel->setText(hint);
    }

    // Update breadcrumbs
    const QStringList breadcrumbs = editor->getBreadcrumbs();
    onBreadcrumbsChanged(breadcrumbs);
  });

  connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
    const int index = m_tabs->indexOf(editor);
    if (index < 0) {
      return;
    }
    const QString filePath = m_tabPaths.value(editor);
    const QString name = QFileInfo(filePath).fileName();
    if (!m_tabs->tabText(index).endsWith("*")) {
      m_tabs->setTabText(index, name + "*");
    }
    m_diagnosticsTimer.start();
  });

  const QString name = QFileInfo(path).fileName();
  m_tabs->addTab(editor, name);
  m_tabs->setCurrentWidget(editor);
  editor->setFocus();
  m_tabPaths.insert(editor, path);

  // Initialize save time for conflict detection (issue #246)
  setEditorSaveTime(editor, QFileInfo(path).lastModified());

  pushCompletionsToEditors();
}

bool NMScriptEditorPanel::saveEditor(QPlainTextEdit* editor) {
  if (!editor) {
    return false;
  }

  const QString path = m_tabPaths.value(editor);
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream out(&file);
  out << editor->toPlainText();
  file.close();

  // Record save time to prevent triggering conflict dialog for our own saves
  setEditorSaveTime(editor, QFileInfo(path).lastModified());

  const QString name = QFileInfo(path).fileName();
  const int index = m_tabs->indexOf(editor);
  if (index >= 0) {
    m_tabs->setTabText(index, name);
  }

  m_diagnosticsTimer.start();
  return true;
}

bool NMScriptEditorPanel::ensureScriptFile(const QString& path) {
  if (path.isEmpty()) {
    return false;
  }

  QFileInfo info(path);
  QDir dir = info.dir();
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      return false;
    }
  }

  if (info.exists()) {
    return true;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  const QString sceneName = info.completeBaseName().isEmpty() ? "scene" : info.completeBaseName();
  QTextStream out(&file);
  out << "// " << sceneName << "\n";
  out << "scene " << sceneName << " {\n";
  out << "  say Narrator \"New script\"\n";
  out << "}\n";
  file.close();
  return true;
}

QString NMScriptEditorPanel::scriptsRootPath() const {
  const auto path = ProjectManager::instance().getFolderPath(ProjectFolder::Scripts);
  return QString::fromStdString(path);
}

void NMScriptEditorPanel::rebuildWatchList() {
  core::Logger::instance().info("applyProjectToPanels: rebuildWatchList starting");
  if (!m_scriptWatcher) {
    core::Logger::instance().info("applyProjectToPanels: rebuildWatchList completed (no watcher)");
    return;
  }

  const QString root = scriptsRootPath();
  const QStringList watchedDirs = m_scriptWatcher->directories();
  if (!watchedDirs.isEmpty()) {
    m_scriptWatcher->removePaths(watchedDirs);
  }
  const QStringList watchedFiles = m_scriptWatcher->files();
  if (!watchedFiles.isEmpty()) {
    m_scriptWatcher->removePaths(watchedFiles);
  }

  if (root.isEmpty() || !QFileInfo::exists(root)) {
    core::Logger::instance().info(
        "applyProjectToPanels: rebuildWatchList completed (root invalid)");
    return;
  }

  QStringList directories;
  QStringList files;
  directories.append(root);

  namespace fs = std::filesystem;
  fs::path base(root.toStdString());
  core::Logger::instance().info(
      "applyProjectToPanels: Starting directory iteration for watch list");
  try {
    // Use skip_permission_denied to avoid hanging on problematic directories
    auto options = fs::directory_options::skip_permission_denied;
    for (const auto& entry : fs::recursive_directory_iterator(base, options)) {
      if (entry.is_directory()) {
        directories.append(QString::fromStdString(entry.path().string()));
      } else if (entry.is_regular_file() && entry.path().extension() == ".nms") {
        files.append(QString::fromStdString(entry.path().string()));
      }
    }
    core::Logger::instance().info(
        "applyProjectToPanels: Directory iteration for watch list completed");
  } catch (const std::exception& e) {
    core::Logger::instance().warning(std::string("Failed to rebuild script watcher: ") + e.what());
  }

  if (!directories.isEmpty()) {
    m_scriptWatcher->addPaths(directories);
  }
  if (!files.isEmpty()) {
    m_scriptWatcher->addPaths(files);
  }
  core::Logger::instance().info("applyProjectToPanels: rebuildWatchList completed");
}

NMScriptEditor* NMScriptEditorPanel::findEditorForPath(const QString& path) const {
  for (int i = 0; i < m_tabs->count(); ++i) {
    QWidget* widget = m_tabs->widget(i);
    if (m_tabPaths.value(widget) == path) {
      return qobject_cast<NMScriptEditor*>(widget);
    }
  }
  return nullptr;
}

bool NMScriptEditorPanel::isTabModified(const QWidget* editor) const {
  if (!editor || !m_tabs) {
    return false;
  }
  const int index = m_tabs->indexOf(const_cast<QWidget*>(editor));
  if (index < 0) {
    return false;
  }
  // Check if tab text has "*" suffix indicating unsaved changes
  return m_tabs->tabText(index).endsWith("*");
}

QDateTime NMScriptEditorPanel::getEditorSaveTime(const QWidget* editor) const {
  return m_editorSaveTimes.value(const_cast<QWidget*>(editor));
}

void NMScriptEditorPanel::setEditorSaveTime(QWidget* editor, const QDateTime& time) {
  if (editor) {
    m_editorSaveTimes.insert(editor, time);
  }
}

} // namespace NovelMind::editor::qt
