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
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <filesystem>
#include <limits>

#include "nm_script_editor_panel_detail.hpp"

namespace NovelMind::editor::qt {

// NMScriptEditorPanel
// ============================================================================

NMScriptEditorPanel::NMScriptEditorPanel(QWidget *parent)
    : NMDockPanel(tr("Script Editor"), parent) {
  setPanelId("ScriptEditor");
  setWindowIcon(NMIconManager::instance().getIcon("panel-console", 16));
  m_diagnosticsTimer.setSingleShot(true);
  m_diagnosticsTimer.setInterval(600);
  connect(&m_diagnosticsTimer, &QTimer::timeout, this,
          &NMScriptEditorPanel::runDiagnostics);
  m_scriptWatcher = new QFileSystemWatcher(this);
  connect(m_scriptWatcher, &QFileSystemWatcher::directoryChanged, this,
          &NMScriptEditorPanel::onDirectoryChanged);
  connect(m_scriptWatcher, &QFileSystemWatcher::fileChanged, this,
          &NMScriptEditorPanel::onFileChanged);
  connect(m_scriptWatcher, &QFileSystemWatcher::fileChanged, this,
          [this](const QString &) { refreshSymbolIndex(); });

  // Initialize project context for asset validation
  m_projectContext = new ScriptProjectContext(QString());
  setupContent();
}

NMScriptEditorPanel::~NMScriptEditorPanel() {
  // Save state on destruction
  saveState();
  delete m_projectContext;
}

void NMScriptEditorPanel::onInitialize() {
  // Set project path for asset validation
  const std::string projectPath =
      ProjectManager::instance().getProjectPath();
  if (!projectPath.empty() && m_projectContext) {
    m_projectContext->setProjectPath(QString::fromStdString(projectPath));
  }
  refreshFileList();

  // Show welcome dialog on first launch
  QSettings settings("NovelMind", "Editor");
  bool showScriptWelcome =
      settings.value("scriptEditor/showWelcome", true).toBool();

  if (showScriptWelcome) {
    // Use QTimer to show dialog after the panel is fully initialized
    QTimer::singleShot(500, this, [this]() {
      NMScriptWelcomeDialog welcomeDialog(this);

      // Connect signals
      connect(&welcomeDialog, &NMScriptWelcomeDialog::openSampleRequested, this,
              [this](const QString &sampleId) { loadSampleScript(sampleId); });

      if (welcomeDialog.exec() == QDialog::Accepted) {
        if (welcomeDialog.shouldSkipInFuture()) {
          QSettings settings("NovelMind", "Editor");
          settings.setValue("scriptEditor/showWelcome", false);
        }
      }
    });
  }
  // Restore state after panels are initialized
  restoreState();
}

void NMScriptEditorPanel::onUpdate(double /*deltaTime*/) {}

void NMScriptEditorPanel::openScript(const QString &path) {
  if (path.isEmpty()) {
    return;
  }

  QString resolvedPath = path;
  if (QFileInfo(path).isRelative()) {
    resolvedPath = QString::fromStdString(
        ProjectManager::instance().toAbsolutePath(path.toStdString()));
  }

  if (!ensureScriptFile(resolvedPath)) {
    return;
  }

  refreshFileList();

  for (int i = 0; i < m_tabs->count(); ++i) {
    if (m_tabPaths.value(m_tabs->widget(i)) == resolvedPath) {
      m_tabs->setCurrentIndex(i);
      return;
    }
  }

  addEditorTab(resolvedPath);
}

void NMScriptEditorPanel::refreshFileList() {
  m_fileTree->clear();

  const QString rootPath = scriptsRootPath();
  if (rootPath.isEmpty()) {
    if (m_issuesPanel) {
      m_issuesPanel->setIssues({});
    }
    return;
  }

  QTreeWidgetItem *rootItem = new QTreeWidgetItem(m_fileTree);
  rootItem->setText(0, QFileInfo(rootPath).fileName());
  rootItem->setData(0, Qt::UserRole, rootPath);

  namespace fs = std::filesystem;
  fs::path base(rootPath.toStdString());
  if (!fs::exists(base)) {
    return;
  }

  try {
    for (const auto &entry : fs::recursive_directory_iterator(base)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      if (entry.path().extension() != ".nms") {
        continue;
      }

      const fs::path rel = fs::relative(entry.path(), base);
      QTreeWidgetItem *parentItem = rootItem;

      for (const auto &part : rel.parent_path()) {
        const QString partName = QString::fromStdString(part.string());
        QTreeWidgetItem *child = nullptr;
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

      QTreeWidgetItem *fileItem = new QTreeWidgetItem(parentItem);
      fileItem->setText(
          0, QString::fromStdString(entry.path().filename().string()));
      fileItem->setData(0, Qt::UserRole,
                        QString::fromStdString(entry.path().string()));
    }
  } catch (const std::exception &e) {
    core::Logger::instance().warning(
        std::string("Failed to scan scripts folder: ") + e.what());
  }

  m_fileTree->expandAll();

  rebuildWatchList();
  refreshSymbolIndex();
}

void NMScriptEditorPanel::onFileActivated(QTreeWidgetItem *item, int column) {
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
  if (auto *editor = qobject_cast<QPlainTextEdit *>(m_tabs->currentWidget())) {
    saveEditor(editor);
  }
  refreshSymbolIndex();
  m_diagnosticsTimer.start();
}

void NMScriptEditorPanel::onSaveAllRequested() {
  for (int i = 0; i < m_tabs->count(); ++i) {
    if (auto *editor = qobject_cast<QPlainTextEdit *>(m_tabs->widget(i))) {
      saveEditor(editor);
    }
  }
  refreshSymbolIndex();
  m_diagnosticsTimer.start();
}

void NMScriptEditorPanel::onFormatRequested() {
  if (!m_tabs) {
    return;
  }
  auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->currentWidget());
  if (!editor) {
    return;
  }

  const QStringList lines = editor->toPlainText().split('\n');
  QStringList formatted;
  formatted.reserve(lines.size());

  int indentLevel = 0;
  const int indentSize = editor->indentSize();

  for (const auto &line : lines) {
    const QString trimmed = line.trimmed();

    int leadingClosers = 0;
    while (leadingClosers < trimmed.size() &&
           trimmed.at(leadingClosers) == '}') {
      ++leadingClosers;
    }
    indentLevel = std::max(0, indentLevel - leadingClosers);

    if (trimmed.isEmpty()) {
      formatted.append(QString());
    } else {
      const QString indent(indentLevel * indentSize, ' ');
      formatted.append(indent + trimmed);
    }

    const int netBraces =
        static_cast<int>(trimmed.count('{') - trimmed.count('}'));
    indentLevel = std::max(0, indentLevel + netBraces);
  }

  const int originalPos = editor->textCursor().position();
  editor->setPlainText(formatted.join("\n"));
  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(
      std::min(originalPos, editor->document()->characterCount() - 1));
  editor->setTextCursor(cursor);
}

void NMScriptEditorPanel::onCurrentTabChanged(int index) {
  Q_UNUSED(index);
  emit docHtmlChanged(QString());
  m_diagnosticsTimer.start();
}

void NMScriptEditorPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  setupToolBar();
  layout->addWidget(m_toolBar);

  // Breadcrumb bar (shows current scope: scene > choice > if)
  const auto &palette = NMStyleManager::instance().palette();
  m_breadcrumbBar = new QWidget(m_contentWidget);
  auto *breadcrumbLayout = new QHBoxLayout(m_breadcrumbBar);
  breadcrumbLayout->setContentsMargins(8, 2, 8, 2);
  breadcrumbLayout->setSpacing(0);
  m_breadcrumbBar->setStyleSheet(
      QString("background-color: %1; border-bottom: 1px solid %2;")
          .arg(palette.bgMedium.name())
          .arg(palette.borderLight.name()));
  m_breadcrumbBar->setFixedHeight(24);
  layout->addWidget(m_breadcrumbBar);

  m_splitter = new QSplitter(Qt::Horizontal, m_contentWidget);

  // Left panel with file tree and symbol list
  m_leftSplitter = new QSplitter(Qt::Vertical, m_splitter);

  m_fileTree = new QTreeWidget(m_leftSplitter);
  m_fileTree->setHeaderHidden(true);
  m_fileTree->setMinimumWidth(180);
  m_fileTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

  connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptEditorPanel::onFileActivated);
  connect(m_fileTree, &QTreeWidget::itemActivated, this,
          &NMScriptEditorPanel::onFileActivated);

  // Symbol list for quick navigation
  auto *symbolGroup = new QGroupBox(tr("Symbols"), m_leftSplitter);
  auto *symbolLayout = new QVBoxLayout(symbolGroup);
  symbolLayout->setContentsMargins(4, 4, 4, 4);
  symbolLayout->setSpacing(4);

  auto *symbolFilter = new QLineEdit(symbolGroup);
  symbolFilter->setPlaceholderText(tr("Filter symbols..."));
  symbolFilter->setClearButtonEnabled(true);
  connect(symbolFilter, &QLineEdit::textChanged, this,
          &NMScriptEditorPanel::filterSymbolList);
  symbolLayout->addWidget(symbolFilter);

  m_symbolList = new QListWidget(symbolGroup);
  m_symbolList->setStyleSheet(
      QString("QListWidget { background-color: %1; color: %2; border: none; }"
              "QListWidget::item:selected { background-color: %3; }")
          .arg(palette.bgMedium.name())
          .arg(palette.textPrimary.name())
          .arg(palette.bgLight.name()));
  connect(m_symbolList, &QListWidget::itemDoubleClicked, this,
          &NMScriptEditorPanel::onSymbolListActivated);
  symbolLayout->addWidget(m_symbolList);

  m_leftSplitter->addWidget(m_fileTree);
  m_leftSplitter->addWidget(symbolGroup);
  m_leftSplitter->setStretchFactor(0, 1);
  m_leftSplitter->setStretchFactor(1, 1);

  // Create horizontal splitter for editor and preview (issue #240)
  m_mainSplitter = new QSplitter(Qt::Horizontal, m_splitter);

  m_tabs = new QTabWidget(m_mainSplitter);
  m_tabs->setTabsClosable(true);
  connect(m_tabs, &QTabWidget::currentChanged, this,
          &NMScriptEditorPanel::onCurrentTabChanged);
  connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
    QWidget *widget = m_tabs->widget(index);
    m_tabPaths.remove(widget);
    m_editorSaveTimes.remove(widget); // Clean up save time tracking (issue #246)
    m_tabs->removeTab(index);
    delete widget;
  });

  // Scene Preview Widget (issue #240)
  m_scenePreview = new NMScenePreviewWidget(m_mainSplitter);
  QSettings settings;
  m_scenePreviewEnabled =
      settings.value("scriptEditor/previewEnabled", false).toBool();
  m_scenePreview->setVisible(m_scenePreviewEnabled);
  m_scenePreview->setPreviewEnabled(m_scenePreviewEnabled);

  m_mainSplitter->addWidget(m_tabs);
  m_mainSplitter->addWidget(m_scenePreview);
  m_mainSplitter->setStretchFactor(0, 6); // 60% for editor
  m_mainSplitter->setStretchFactor(1, 4); // 40% for preview

  // Find/Replace widget (hidden by default)
  m_findReplaceWidget = new NMFindReplaceWidget(m_contentWidget);
  m_findReplaceWidget->hide();
  connect(m_findReplaceWidget, &NMFindReplaceWidget::closeRequested, this,
          [this]() { m_findReplaceWidget->hide(); });

  // Command Palette
  m_commandPalette = new NMScriptCommandPalette(this);
  setupCommandPalette();

  m_splitter->addWidget(m_leftSplitter);
  m_splitter->addWidget(m_mainSplitter);
  m_splitter->setStretchFactor(0, 0);
  m_splitter->setStretchFactor(1, 1);
  layout->addWidget(m_findReplaceWidget);
  layout->addWidget(m_splitter);

  // Status bar with syntax hints and cursor position
  m_statusBar = new QWidget(m_contentWidget);
  auto *statusLayout = new QHBoxLayout(m_statusBar);
  statusLayout->setContentsMargins(8, 2, 8, 2);
  statusLayout->setSpacing(16);
  m_statusBar->setStyleSheet(
      QString("background-color: %1; border-top: 1px solid %2;")
          .arg(palette.bgMedium.name())
          .arg(palette.borderLight.name()));
  m_statusBar->setFixedHeight(22);

  // Syntax hint label
  m_syntaxHintLabel = new QLabel(m_statusBar);
  m_syntaxHintLabel->setStyleSheet(
      QString("color: %1; font-family: %2;")
          .arg(palette.textSecondary.name())
          .arg(NMStyleManager::instance().monospaceFont().family()));
  statusLayout->addWidget(m_syntaxHintLabel);

  statusLayout->addStretch();

  // Cursor position label
  m_cursorPosLabel = new QLabel(tr("Ln 1, Col 1"), m_statusBar);
  m_cursorPosLabel->setStyleSheet(
      QString("color: %1;").arg(palette.textSecondary.name()));
  statusLayout->addWidget(m_cursorPosLabel);

  layout->addWidget(m_statusBar);

  // Initialize snippet templates
  m_snippetTemplates = detail::buildSnippetTemplates();

  setContentWidget(m_contentWidget);
}

void NMScriptEditorPanel::setupToolBar() {
  m_toolBar = new QToolBar(m_contentWidget);
  m_toolBar->setIconSize(QSize(16, 16));

  QAction *actionSave = m_toolBar->addAction(tr("Save"));
  actionSave->setToolTip(tr("Save (Ctrl+S)"));
  connect(actionSave, &QAction::triggered, this,
          &NMScriptEditorPanel::onSaveRequested);

  QAction *actionSaveAll = m_toolBar->addAction(tr("Save All"));
  actionSaveAll->setToolTip(tr("Save all open scripts"));
  connect(actionSaveAll, &QAction::triggered, this,
          &NMScriptEditorPanel::onSaveAllRequested);

  m_toolBar->addSeparator();

  QAction *actionFormat = m_toolBar->addAction(tr("Format"));
  actionFormat->setToolTip(tr("Auto-format script (Ctrl+Shift+F)"));
  actionFormat->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
  actionFormat->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionFormat);
  connect(actionFormat, &QAction::triggered, this,
          &NMScriptEditorPanel::onFormatRequested);

  m_toolBar->addSeparator();

  // Toggle Preview action (issue #240)
  m_togglePreviewAction = m_toolBar->addAction(tr("👁️ Preview"));
  m_togglePreviewAction->setToolTip(
      tr("Toggle live scene preview (Ctrl+Shift+V)"));
  m_togglePreviewAction->setCheckable(true);
  m_togglePreviewAction->setChecked(m_scenePreviewEnabled);
  m_togglePreviewAction->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
  m_togglePreviewAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(m_togglePreviewAction);
  connect(m_togglePreviewAction, &QAction::triggered, this,
          &NMScriptEditorPanel::toggleScenePreview);

  m_toolBar->addSeparator();

  // Snippet insertion
  QAction *actionSnippet = m_toolBar->addAction(tr("Insert"));
  actionSnippet->setToolTip(tr("Insert code snippet (Ctrl+J)"));
  actionSnippet->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
  actionSnippet->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionSnippet);
  connect(actionSnippet, &QAction::triggered, this,
          &NMScriptEditorPanel::onInsertSnippetRequested);

  // Symbol navigator
  QAction *actionSymbols = m_toolBar->addAction(tr("Symbols"));
  actionSymbols->setToolTip(tr("Open symbol navigator (Ctrl+Shift+O)"));
  actionSymbols->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  actionSymbols->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionSymbols);
  connect(actionSymbols, &QAction::triggered, this,
          &NMScriptEditorPanel::onSymbolNavigatorRequested);
}

void NMScriptEditorPanel::addEditorTab(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  const QString content = QString::fromUtf8(file.readAll());

  auto *editor = new NMScriptEditor(m_tabs);
  editor->setPlainText(content);
  editor->setHoverDocs(detail::buildHoverDocs());
  editor->setDocHtml(detail::buildDocHtml());
  editor->setSymbolLocations(buildSymbolLocations());

  connect(editor, &NMScriptEditor::requestSave, this,
          &NMScriptEditorPanel::onSaveRequested);
  connect(editor, &NMScriptEditor::hoverDocChanged, this,
          [this](const QString &, const QString &html) {
            emit docHtmlChanged(html);
          });
  connect(editor, &QPlainTextEdit::textChanged, this,
          [this]() {
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
  connect(editor, &NMScriptEditor::showFindRequested, this,
          &NMScriptEditorPanel::showFindDialog);
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
  connect(editor, &NMScriptEditor::breakpointToggled, this,
          [this, path](int line) {
            auto &controller = NMPlayModeController::instance();
            controller.toggleSourceBreakpoint(path, line);
          });

  // Sync breakpoints from controller to editor
  auto &controller = NMPlayModeController::instance();
  editor->setBreakpoints(controller.sourceBreakpointsForFile(path));

  // Listen for external breakpoint changes
  connect(&controller, &NMPlayModeController::sourceBreakpointsChanged, editor,
          [path, editor]() {
            auto &ctrl = NMPlayModeController::instance();
            editor->setBreakpoints(ctrl.sourceBreakpointsForFile(path));
          });

  // Listen for source-level breakpoint hits to show execution line
  connect(&controller, &NMPlayModeController::sourceBreakpointHit, editor,
          [path, editor](const QString &filePath, int line) {
            if (filePath == path) {
              editor->setCurrentExecutionLine(line);
            }
          });

  // Clear execution line when play mode changes
  connect(&controller, &NMPlayModeController::playModeChanged, editor,
          [editor](int mode) {
            if (mode == NMPlayModeController::Stopped) {
              editor->setCurrentExecutionLine(0);
            }
          });

  // Update cursor position in status bar
  connect(editor, &QPlainTextEdit::cursorPositionChanged, this,
          [this, editor]() {
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

bool NMScriptEditorPanel::saveEditor(QPlainTextEdit *editor) {
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

bool NMScriptEditorPanel::ensureScriptFile(const QString &path) {
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

  const QString sceneName =
      info.completeBaseName().isEmpty() ? "scene" : info.completeBaseName();
  QTextStream out(&file);
  out << "// " << sceneName << "\n";
  out << "scene " << sceneName << " {\n";
  out << "  say Narrator \"New script\"\n";
  out << "}\n";
  file.close();
  return true;
}

QString NMScriptEditorPanel::scriptsRootPath() const {
  const auto path =
      ProjectManager::instance().getFolderPath(ProjectFolder::Scripts);
  return QString::fromStdString(path);
}

void NMScriptEditorPanel::rebuildWatchList() {
  if (!m_scriptWatcher) {
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
    return;
  }

  QStringList directories;
  QStringList files;
  directories.append(root);

  namespace fs = std::filesystem;
  fs::path base(root.toStdString());
  try {
    for (const auto &entry : fs::recursive_directory_iterator(base)) {
      if (entry.is_directory()) {
        directories.append(QString::fromStdString(entry.path().string()));
      } else if (entry.is_regular_file() &&
                 entry.path().extension() == ".nms") {
        files.append(QString::fromStdString(entry.path().string()));
      }
    }
  } catch (const std::exception &e) {
    core::Logger::instance().warning(
        std::string("Failed to rebuild script watcher: ") + e.what());
  }

  if (!directories.isEmpty()) {
    m_scriptWatcher->addPaths(directories);
  }
  if (!files.isEmpty()) {
    m_scriptWatcher->addPaths(files);
  }
}

void NMScriptEditorPanel::refreshSymbolIndex() {
  QMutexLocker locker(&m_symbolIndexMutex);
  m_symbolIndex = {};
  const QString root = scriptsRootPath();
  if (root.isEmpty()) {
    pushCompletionsToEditors();
    refreshSymbolList();
    if (m_issuesPanel) {
      m_issuesPanel->setIssues({});
    }
    return;
  }

  namespace fs = std::filesystem;
  fs::path base(root.toStdString());
  if (!fs::exists(base)) {
    pushCompletionsToEditors();
    refreshSymbolList();
    return;
  }

  QSet<QString> seenScenes;
  QSet<QString> seenCharacters;
  QSet<QString> seenFlags;
  QSet<QString> seenVariables;
  QSet<QString> seenBackgrounds;
  QSet<QString> seenVoices;
  QSet<QString> seenMusic;

  auto insertMap = [](QHash<QString, QString> &map, QSet<QString> &seen,
                      const QString &value, const QString &filePath) {
    const QString key = value.toLower();
    if (value.isEmpty() || seen.contains(key)) {
      return;
    }
    seen.insert(key);
    map.insert(value, filePath);
  };

  auto insertList = [](QStringList &list, QSet<QString> &seen,
                       const QString &value) {
    const QString key = value.toLower();
    if (value.isEmpty() || seen.contains(key)) {
      return;
    }
    seen.insert(key);
    list.append(value);
  };

  const QRegularExpression reScene("\\bscene\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reCharacter(
      "\\bcharacter\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reSetFlag(
      "\\bset\\s+flag\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reFlag("\\bflag\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reSetVar(
      "\\bset\\s+(?!flag\\s)([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression reBackground("show\\s+background\\s+\"([^\"]+)\"");
  const QRegularExpression reVoice("play\\s+voice\\s+\"([^\"]+)\"");
  const QRegularExpression reMusic("play\\s+music\\s+\"([^\"]+)\"");

  QList<NMScriptIssue> issues;

  try {
    for (const auto &entry : fs::recursive_directory_iterator(base)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
        continue;
      }

      const QString path = QString::fromStdString(entry.path().string());
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        continue;
      }
      const QString content = QString::fromUtf8(file.readAll());
      file.close();

      // Collect symbols with line numbers
      const QStringList lines = content.split('\n');
      for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const QString &line = lines[lineNum];

        // Scenes with line numbers
        QRegularExpressionMatch sceneMatch = reScene.match(line);
        if (sceneMatch.hasMatch()) {
          const QString value = sceneMatch.captured(1);
          if (!seenScenes.contains(value.toLower())) {
            seenScenes.insert(value.toLower());
            m_symbolIndex.scenes.insert(value, path);
            m_symbolIndex.sceneLines.insert(value, lineNum + 1);
          }
        }

        // Characters with line numbers
        QRegularExpressionMatch charMatch = reCharacter.match(line);
        if (charMatch.hasMatch()) {
          const QString value = charMatch.captured(1);
          if (!seenCharacters.contains(value.toLower())) {
            seenCharacters.insert(value.toLower());
            m_symbolIndex.characters.insert(value, path);
            m_symbolIndex.characterLines.insert(value, lineNum + 1);
          }
        }
      }

      auto collect = [&](const QRegularExpression &regex, auto &&callback) {
        QRegularExpressionMatchIterator it = regex.globalMatch(content);
        while (it.hasNext()) {
          const QRegularExpressionMatch match = it.next();
          callback(match.captured(1));
        }
      };

      collect(reSetFlag, [&](const QString &value) {
        insertMap(m_symbolIndex.flags, seenFlags, value, path);
      });
      collect(reFlag, [&](const QString &value) {
        insertMap(m_symbolIndex.flags, seenFlags, value, path);
      });
      collect(reSetVar, [&](const QString &value) {
        insertMap(m_symbolIndex.variables, seenVariables, value, path);
      });
      collect(reBackground, [&](const QString &value) {
        insertList(m_symbolIndex.backgrounds, seenBackgrounds, value);
      });
      collect(reVoice, [&](const QString &value) {
        insertList(m_symbolIndex.voices, seenVoices, value);
      });
      collect(reMusic, [&](const QString &value) {
        insertList(m_symbolIndex.music, seenMusic, value);
      });
    }
  } catch (const std::exception &e) {
    core::Logger::instance().warning(
        std::string("Failed to build script symbols: ") + e.what());
  }

  // Add all available assets from project file system
  if (m_projectContext) {
    // Add available backgrounds
    auto backgrounds = m_projectContext->getAvailableBackgrounds();
    for (const auto &bg : backgrounds) {
      insertList(m_symbolIndex.backgrounds, seenBackgrounds,
                 QString::fromStdString(bg));
    }

    // Add available music
    auto music = m_projectContext->getAvailableAudio("music");
    for (const auto &track : music) {
      insertList(m_symbolIndex.music, seenMusic,
                 QString::fromStdString(track));
    }

    // Add available sound effects
    auto sounds = m_projectContext->getAvailableAudio("sound");
    for (const auto &sfx : sounds) {
      insertList(m_symbolIndex.music, seenMusic, QString::fromStdString(sfx));
    }

    // Add available voices
    auto voices = m_projectContext->getAvailableAudio("voice");
    for (const auto &voice : voices) {
      insertList(m_symbolIndex.voices, seenVoices,
                 QString::fromStdString(voice));
    }
  }

  pushCompletionsToEditors();
  refreshSymbolList();
  if (m_issuesPanel) {
    m_issuesPanel->setIssues(issues);
  }
}

QList<NMScriptIssue>
NMScriptEditorPanel::validateSource(const QString &path,
                                    const QString &source) const {
  QList<NMScriptIssue> out;
  using namespace scripting;

  std::string src = source.toStdString();

  Lexer lexer;
  auto lexResult = lexer.tokenize(src);
  for (const auto &err : lexer.getErrors()) {
    out.push_back({path, static_cast<int>(err.location.line),
                   QString::fromStdString(err.message), "error"});
  }
  if (!lexResult.isOk()) {
    return out;
  }

  Parser parser;
  auto parseResult = parser.parse(lexResult.value());
  for (const auto &err : parser.getErrors()) {
    out.push_back({path, static_cast<int>(err.location.line),
                   QString::fromStdString(err.message), "error"});
  }
  if (!parseResult.isOk()) {
    return out;
  }

  Validator validator;
  // Enable asset validation if project context is available
  if (m_projectContext) {
    validator.setProjectContext(m_projectContext);
    validator.setValidateAssets(true);
  }
  auto validation = validator.validate(parseResult.value());
  for (const auto &err : validation.errors.all()) {
    QString severity = "info";
    if (err.severity == Severity::Warning) {
      severity = "warning";
    } else if (err.severity == Severity::Error) {
      severity = "error";
    }
    out.push_back({path, static_cast<int>(err.span.start.line),
                   QString::fromStdString(err.message), severity});
  }

  return out;
}

void NMScriptEditorPanel::runDiagnostics() {
  auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->currentWidget());
  if (!editor) {
    return;
  }
  QString path = m_tabPaths.value(editor);
  if (path.isEmpty()) {
    return;
  }
  const QString text = editor->toPlainText();
  QList<NMScriptIssue> issues = validateSource(path, text);

  // Update inline error markers in the editor
  editor->setDiagnostics(issues);

  if (m_issuesPanel) {
    m_issuesPanel->setIssues(issues);
  }
}

void NMScriptEditorPanel::goToLocation(const QString &path, int line) {
  openScript(path);
  auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->currentWidget());
  if (!editor) {
    return;
  }
  QTextCursor cursor(
      editor->document()->findBlockByLineNumber(std::max(0, line - 1)));
  editor->setTextCursor(cursor);
  editor->setFocus();
}

QList<NMScriptEditor::CompletionEntry>
NMScriptEditorPanel::buildProjectCompletionEntries() const {
  QMutexLocker locker(&m_symbolIndexMutex);
  QList<NMScriptEditor::CompletionEntry> entries;

  auto addEntries = [&entries](const QStringList &list, const QString &detail) {
    for (const auto &item : list) {
      entries.push_back({item, detail});
    }
  };

  addEntries(m_symbolIndex.scenes.keys(), "scene");
  addEntries(m_symbolIndex.characters.keys(), "character");
  addEntries(m_symbolIndex.flags.keys(), "flag");
  addEntries(m_symbolIndex.variables.keys(), "variable");
  addEntries(m_symbolIndex.backgrounds, "background");
  addEntries(m_symbolIndex.music, "music");
  addEntries(m_symbolIndex.voices, "voice");

  return entries;
}

QHash<QString, QString> NMScriptEditorPanel::buildProjectHoverDocs() const {
  QMutexLocker locker(&m_symbolIndexMutex);
  QHash<QString, QString> docs;
  auto relPath = [](const QString &path) {
    if (path.isEmpty()) {
      return QString();
    }
    return QString::fromStdString(
        ProjectManager::instance().toRelativePath(path.toStdString()));
  };

  auto addDocs = [&](const QHash<QString, QString> &map, const QString &label) {
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
      const QString key = it.key();
      const QString path = relPath(it.value());
      docs.insert(
          key.toLower(),
          QString("%1 \"%2\"%3")
              .arg(label, key,
                   path.isEmpty() ? QString() : QString(" (%1)").arg(path)));
    }
  };

  addDocs(m_symbolIndex.scenes, tr("Scene"));
  addDocs(m_symbolIndex.characters, tr("Character"));
  addDocs(m_symbolIndex.flags, tr("Flag"));
  addDocs(m_symbolIndex.variables, tr("Variable"));

  for (const auto &bg : m_symbolIndex.backgrounds) {
    docs.insert(bg.toLower(), tr("Background asset \"%1\"").arg(bg));
  }
  for (const auto &m : m_symbolIndex.music) {
    docs.insert(m.toLower(), tr("Music track \"%1\"").arg(m));
  }
  for (const auto &v : m_symbolIndex.voices) {
    docs.insert(v.toLower(), tr("Voice asset \"%1\"").arg(v));
  }

  return docs;
}

QHash<QString, QString> NMScriptEditorPanel::buildProjectDocHtml() const {
  QMutexLocker locker(&m_symbolIndexMutex);
  QHash<QString, QString> docs;

  auto relPath = [](const QString &path) {
    if (path.isEmpty()) {
      return QString();
    }
    return QString::fromStdString(
        ProjectManager::instance().toRelativePath(path.toStdString()));
  };

  auto addDocs = [&](const QHash<QString, QString> &map, const QString &label) {
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
      const QString name = it.key();
      const QString file = relPath(it.value());
      QString html = QString("<h3>%1</h3>").arg(name.toHtmlEscaped());
      html += QString("<p>%1 definition%2</p>")
                  .arg(label.toHtmlEscaped(),
                       file.isEmpty() ? QString()
                                      : QString(" in <code>%1</code>")
                                            .arg(file.toHtmlEscaped()));
      docs.insert(name.toLower(), html);
    }
  };

  addDocs(m_symbolIndex.scenes, tr("Scene"));
  addDocs(m_symbolIndex.characters, tr("Character"));
  addDocs(m_symbolIndex.flags, tr("Flag"));
  addDocs(m_symbolIndex.variables, tr("Variable"));

  auto addSimple = [&](const QStringList &list, const QString &label) {
    for (const auto &item : list) {
      QString html = QString("<h3>%1</h3><p>%2</p>")
                         .arg(item.toHtmlEscaped(), label.toHtmlEscaped());
      docs.insert(item.toLower(), html);
    }
  };

  addSimple(m_symbolIndex.backgrounds, tr("Background asset"));
  addSimple(m_symbolIndex.music, tr("Music track"));
  addSimple(m_symbolIndex.voices, tr("Voice asset"));

  return docs;
}

void NMScriptEditorPanel::pushCompletionsToEditors() {
  QList<NMScriptEditor::CompletionEntry> entries =
      detail::buildKeywordEntries();
  entries.append(buildProjectCompletionEntries());

  QHash<QString, NMScriptEditor::CompletionEntry> merged;
  for (const auto &entry : entries) {
    const QString key = entry.text.toLower();
    if (!merged.contains(key)) {
      merged.insert(key, entry);
    }
  }

  QList<NMScriptEditor::CompletionEntry> combined = merged.values();
  std::sort(combined.begin(), combined.end(),
            [](const NMScriptEditor::CompletionEntry &a,
               const NMScriptEditor::CompletionEntry &b) {
              return a.text.compare(b.text, Qt::CaseInsensitive) < 0;
            });

  QHash<QString, QString> hoverDocs = detail::buildHoverDocs();
  const QHash<QString, QString> projectHoverDocs = buildProjectHoverDocs();
  for (auto it = projectHoverDocs.constBegin();
       it != projectHoverDocs.constEnd(); ++it) {
    hoverDocs.insert(it.key(), it.value());
  }

  QHash<QString, QString> docHtml = detail::buildDocHtml();
  const QHash<QString, QString> projectDocHtml = buildProjectDocHtml();
  for (auto it = projectDocHtml.constBegin(); it != projectDocHtml.constEnd();
       ++it) {
    docHtml.insert(it.key(), it.value());
  }

  // Build symbol locations for go-to-definition
  const auto symbolLocations = buildSymbolLocations();

  for (auto *editor : editors()) {
    editor->setCompletionEntries(combined);
    editor->setHoverDocs(hoverDocs);
    editor->setProjectDocs(projectHoverDocs);
    editor->setDocHtml(docHtml);
    editor->setSymbolLocations(symbolLocations);
  }

  m_diagnosticsTimer.start();
}

QList<NMScriptEditor *> NMScriptEditorPanel::editors() const {
  QList<NMScriptEditor *> list;
  if (!m_tabs) {
    return list;
  }
  for (int i = 0; i < m_tabs->count(); ++i) {
    if (auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->widget(i))) {
      list.push_back(editor);
    }
  }
  return list;
}

NMScriptEditor *NMScriptEditorPanel::currentEditor() const {
  if (!m_tabs) {
    return nullptr;
  }
  return qobject_cast<NMScriptEditor *>(m_tabs->currentWidget());
}

bool NMScriptEditorPanel::goToSceneDefinition(const QString &sceneName) {
  QMutexLocker locker(&m_symbolIndexMutex);
  const QString key = sceneName.toLower();
  for (auto it = m_symbolIndex.scenes.constBegin();
       it != m_symbolIndex.scenes.constEnd(); ++it) {
    if (it.key().toLower() == key) {
      const QString filePath = it.value();
      const int line = m_symbolIndex.sceneLines.value(it.key(), 1);
      locker.unlock();  // Unlock before calling goToLocation to avoid deadlock
      goToLocation(filePath, line);
      return true;
    }
  }
  return false;
}

QList<ReferenceResult>
NMScriptEditorPanel::findAllReferences(const QString &symbol) const {
  QList<ReferenceResult> results;
  const QString root = scriptsRootPath();
  if (root.isEmpty()) {
    return results;
  }

  const QString lowerSymbol = symbol.toLower();
  const QRegularExpression re(
      QString("\\b%1\\b").arg(QRegularExpression::escape(symbol)),
      QRegularExpression::CaseInsensitiveOption);

  namespace fs = std::filesystem;
  fs::path base(root.toStdString());
  if (!fs::exists(base)) {
    return results;
  }

  try {
    for (const auto &entry : fs::recursive_directory_iterator(base)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
        continue;
      }

      const QString filePath = QString::fromStdString(entry.path().string());
      QFile file(filePath);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        continue;
      }

      const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
      for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        if (re.match(line).hasMatch()) {
          ReferenceResult ref;
          ref.filePath = filePath;
          ref.line = i + 1;
          ref.context = line.trimmed();

          // Check if this is a definition (scene, character declaration)
          ref.isDefinition = line.contains(
              QRegularExpression(QString("\\b(scene|character)\\s+%1\\b")
                                     .arg(QRegularExpression::escape(symbol)),
                                 QRegularExpression::CaseInsensitiveOption));
          results.append(ref);
        }
      }
    }
  } catch (const std::exception &e) {
    core::Logger::instance().warning(
        std::string("Failed to find references: ") + e.what());
  }

  return results;
}

void NMScriptEditorPanel::onSymbolListActivated(QListWidgetItem *item) {
  if (!item) {
    return;
  }
  const QString filePath = item->data(Qt::UserRole).toString();
  const int line = item->data(Qt::UserRole + 1).toInt();
  if (!filePath.isEmpty()) {
    goToLocation(filePath, line);
  }
}

void NMScriptEditorPanel::onSymbolNavigatorRequested() {
  refreshSymbolList();
  if (m_symbolList) {
    m_symbolList->setFocus();
  }
}

void NMScriptEditorPanel::onGoToDefinition(const QString &symbol,
                                           const SymbolLocation &location) {
  if (!location.filePath.isEmpty()) {
    goToLocation(location.filePath, location.line);
  }
}

void NMScriptEditorPanel::onFindReferences(const QString &symbol) {
  const QList<ReferenceResult> references = findAllReferences(symbol);
  showReferencesDialog(symbol, references);
  emit referencesFound(symbol, references);
}

void NMScriptEditorPanel::onInsertSnippetRequested() {
  if (auto *editor = currentEditor()) {
    editor->insertSnippet("scene");
  }
}

void NMScriptEditorPanel::onNavigateToGraphNode(const QString &sceneId) {
  emit navigateToGraphNode(sceneId);
}

void NMScriptEditorPanel::refreshSymbolList() {
  if (!m_symbolList) {
    return;
  }
  m_symbolList->clear();

  QMutexLocker locker(&m_symbolIndexMutex);
  const auto &palette = NMStyleManager::instance().palette();
  auto addItems = [this, &palette](const QHash<QString, QString> &map,
                                   const QString &typeLabel,
                                   const QColor &color) {
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
      auto *item =
          new QListWidgetItem(QString("%1 (%2)").arg(it.key(), typeLabel));
      item->setData(Qt::UserRole, it.value());
      item->setData(Qt::UserRole + 1,
                    m_symbolIndex.sceneLines.value(it.key(), 1));
      item->setForeground(color);
      m_symbolList->addItem(item);
    }
  };

  addItems(m_symbolIndex.scenes, tr("scene"), palette.accentPrimary);
  addItems(m_symbolIndex.characters, tr("character"), QColor(220, 180, 120));
  addItems(m_symbolIndex.flags, tr("flag"), QColor(170, 200, 255));
  addItems(m_symbolIndex.variables, tr("variable"), QColor(200, 170, 255));
}

void NMScriptEditorPanel::filterSymbolList(const QString &filter) {
  if (!m_symbolList) {
    return;
  }
  for (int i = 0; i < m_symbolList->count(); ++i) {
    auto *item = m_symbolList->item(i);
    const bool matches =
        filter.isEmpty() || item->text().contains(filter, Qt::CaseInsensitive);
    item->setHidden(!matches);
  }
}

void NMScriptEditorPanel::showReferencesDialog(
    const QString &symbol, const QList<ReferenceResult> &references) {
  if (references.isEmpty()) {
    return;
  }

  // Create a simple dialog to show references
  auto *dialog = new QDialog(this);
  dialog->setWindowTitle(
      tr("References to '%1' (%2 found)").arg(symbol).arg(references.size()));
  dialog->resize(600, 400);

  auto *layout = new QVBoxLayout(dialog);
  auto *list = new QListWidget(dialog);

  const auto &palette = NMStyleManager::instance().palette();
  list->setStyleSheet(
      QString("QListWidget { background-color: %1; color: %2; }"
              "QListWidget::item:selected { background-color: %3; }")
          .arg(palette.bgMedium.name())
          .arg(palette.textPrimary.name())
          .arg(palette.bgLight.name()));

  for (const auto &ref : references) {
    const QString fileName = QFileInfo(ref.filePath).fileName();
    QString label =
        QString("%1:%2: %3").arg(fileName).arg(ref.line).arg(ref.context);
    if (ref.isDefinition) {
      label = "[DEF] " + label;
    }
    auto *item = new QListWidgetItem(label);
    item->setData(Qt::UserRole, ref.filePath);
    item->setData(Qt::UserRole + 1, ref.line);
    if (ref.isDefinition) {
      item->setForeground(palette.accentPrimary);
    }
    list->addItem(item);
  }

  connect(list, &QListWidget::itemDoubleClicked, this,
          [this, dialog](QListWidgetItem *item) {
            const QString path = item->data(Qt::UserRole).toString();
            const int line = item->data(Qt::UserRole + 1).toInt();
            goToLocation(path, line);
            dialog->accept();
          });

  layout->addWidget(list);
  dialog->setLayout(layout);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

QHash<QString, SymbolLocation>
NMScriptEditorPanel::buildSymbolLocations() const {
  QMutexLocker locker(&m_symbolIndexMutex);
  QHash<QString, SymbolLocation> locations;

  // Add scenes
  for (auto it = m_symbolIndex.scenes.constBegin();
       it != m_symbolIndex.scenes.constEnd(); ++it) {
    SymbolLocation loc;
    loc.filePath = it.value();
    loc.line = m_symbolIndex.sceneLines.value(it.key(), 1);
    loc.context = QString("scene %1").arg(it.key());
    locations.insert(it.key().toLower(), loc);
  }

  // Add characters
  for (auto it = m_symbolIndex.characters.constBegin();
       it != m_symbolIndex.characters.constEnd(); ++it) {
    SymbolLocation loc;
    loc.filePath = it.value();
    loc.line = m_symbolIndex.characterLines.value(it.key(), 1);
    loc.context = QString("character %1").arg(it.key());
    locations.insert(it.key().toLower(), loc);
  }

  // Add flags and variables
  for (auto it = m_symbolIndex.flags.constBegin();
       it != m_symbolIndex.flags.constEnd(); ++it) {
    SymbolLocation loc;
    loc.filePath = it.value();
    loc.line = 1;
    loc.context = QString("flag %1").arg(it.key());
    locations.insert(it.key().toLower(), loc);
  }

  for (auto it = m_symbolIndex.variables.constBegin();
       it != m_symbolIndex.variables.constEnd(); ++it) {
    SymbolLocation loc;
    loc.filePath = it.value();
    loc.line = 1;
    loc.context = QString("variable %1").arg(it.key());
    locations.insert(it.key().toLower(), loc);
  }

  return locations;
}

// ============================================================================
// VSCode-like Features: Find/Replace, Command Palette, Minimap Toggle
// ============================================================================

void NMScriptEditorPanel::showFindDialog() {
  if (!m_findReplaceWidget) {
    return;
  }
  auto *editor = currentEditor();
  if (editor) {
    m_findReplaceWidget->setEditor(editor);
    // Pre-fill with selected text
    const QString selected = editor->textCursor().selectedText();
    if (!selected.isEmpty()) {
      m_findReplaceWidget->setSearchText(selected);
    }
  }
  m_findReplaceWidget->showFind();
}

void NMScriptEditorPanel::showReplaceDialog() {
  if (!m_findReplaceWidget) {
    return;
  }
  auto *editor = currentEditor();
  if (editor) {
    m_findReplaceWidget->setEditor(editor);
    const QString selected = editor->textCursor().selectedText();
    if (!selected.isEmpty()) {
      m_findReplaceWidget->setSearchText(selected);
    }
  }
  m_findReplaceWidget->showReplace();
}

void NMScriptEditorPanel::showCommandPalette() {
  if (m_commandPalette) {
    m_commandPalette->show();
  }
}

void NMScriptEditorPanel::setupCommandPalette() {
  if (!m_commandPalette) {
    return;
  }

  // File commands
  m_commandPalette->addCommand(
      {tr("Save"), "Ctrl+S", tr("File"), [this]() { onSaveRequested(); }});
  m_commandPalette->addCommand(
      {tr("Save All"), "", tr("File"), [this]() { onSaveAllRequested(); }});

  // Edit commands
  m_commandPalette->addCommand(
      {tr("Find"), "Ctrl+F", tr("Edit"), [this]() { showFindDialog(); }});
  m_commandPalette->addCommand(
      {tr("Replace"), "Ctrl+H", tr("Edit"), [this]() { showReplaceDialog(); }});
  m_commandPalette->addCommand({tr("Format Document"), "Ctrl+Shift+F",
                                tr("Edit"), [this]() { onFormatRequested(); }});

  // Navigation commands
  m_commandPalette->addCommand({tr("Go to Symbol"), "Ctrl+Shift+O",
                                tr("Navigation"),
                                [this]() { onSymbolNavigatorRequested(); }});
  m_commandPalette->addCommand(
      {tr("Go to Definition"), "F12", tr("Navigation"), [this]() {
         if (auto *editor = currentEditor()) {
           // Trigger go-to-definition on current word
           const QString symbol = editor->textCursor().selectedText();
           if (!symbol.isEmpty()) {
             goToSceneDefinition(symbol);
           }
         }
       }});

  // View commands
  m_commandPalette->addCommand(
      {tr("Toggle Minimap"), "", tr("View"), [this]() { onToggleMinimap(); }});
  m_commandPalette->addCommand(
      {tr("Fold All"), "", tr("View"), [this]() { onFoldAll(); }});
  m_commandPalette->addCommand(
      {tr("Unfold All"), "", tr("View"), [this]() { onUnfoldAll(); }});

  // Insert commands
  m_commandPalette->addCommand(
      {tr("Insert Scene Snippet"), "Ctrl+J", tr("Insert"), [this]() {
         if (auto *editor = currentEditor()) {
           editor->insertSnippet("scene");
         }
       }});
  m_commandPalette->addCommand(
      {tr("Insert Choice Snippet"), "", tr("Insert"), [this]() {
         if (auto *editor = currentEditor()) {
           editor->insertSnippet("choice");
         }
       }});
  m_commandPalette->addCommand(
      {tr("Insert Character"), "", tr("Insert"), [this]() {
         if (auto *editor = currentEditor()) {
           editor->insertSnippet("character");
         }
       }});
}

void NMScriptEditorPanel::onToggleMinimap() {
  m_minimapEnabled = !m_minimapEnabled;
  for (auto *editor : editors()) {
    editor->setMinimapEnabled(m_minimapEnabled);
  }
}

void NMScriptEditorPanel::onFoldAll() {
  if (auto *editor = currentEditor()) {
    for (const auto &region : editor->foldingRegions()) {
      if (!region.isCollapsed) {
        editor->toggleFold(region.startLine);
      }
    }
  }
}

void NMScriptEditorPanel::onUnfoldAll() {
  if (auto *editor = currentEditor()) {
    for (const auto &region : editor->foldingRegions()) {
      if (region.isCollapsed) {
        editor->toggleFold(region.startLine);
      }
    }
  }
}

void NMScriptEditorPanel::onSyntaxHintChanged(const QString &hint) {
  if (m_syntaxHintLabel) {
    m_syntaxHintLabel->setText(hint);
    m_syntaxHintLabel->setVisible(!hint.isEmpty());
  }
}

void NMScriptEditorPanel::onBreadcrumbsChanged(const QStringList &breadcrumbs) {
  if (!m_breadcrumbBar) {
    return;
  }

  // Clear existing breadcrumb buttons
  QLayoutItem *item;
  while ((item = m_breadcrumbBar->layout()->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }

  const auto &palette = NMStyleManager::instance().palette();

  for (int i = 0; i < breadcrumbs.size(); ++i) {
    if (i > 0) {
      auto *separator = new QLabel(">", m_breadcrumbBar);
      separator->setStyleSheet(QString("color: %1; padding: 0 4px;")
                                   .arg(palette.textSecondary.name()));
      m_breadcrumbBar->layout()->addWidget(separator);
    }

    auto *button = new QPushButton(breadcrumbs[i], m_breadcrumbBar);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        QString("QPushButton { color: %1; border: none; padding: 2px 4px; }"
                "QPushButton:hover { background-color: %2; }")
            .arg(palette.textPrimary.name())
            .arg(palette.bgLight.name()));
    m_breadcrumbBar->layout()->addWidget(button);
  }

  // Add stretch at the end
  static_cast<QHBoxLayout *>(m_breadcrumbBar->layout())->addStretch();
}

void NMScriptEditorPanel::onQuickFixRequested() {
  auto *editor = currentEditor();
  if (!editor) {
    return;
  }

  const int line = editor->textCursor().blockNumber() + 1;
  const auto fixes = editor->getQuickFixes(line);
  if (!fixes.isEmpty()) {
    showQuickFixMenu(fixes);
  }
}

void NMScriptEditorPanel::showQuickFixMenu(const QList<QuickFix> &fixes) {
  auto *editor = currentEditor();
  if (!editor || fixes.isEmpty()) {
    return;
  }

  const auto &palette = NMStyleManager::instance().palette();

  QMenu menu(this);
  menu.setStyleSheet(
      QString("QMenu { background-color: %1; color: %2; border: 1px solid %3; }"
              "QMenu::item { padding: 6px 20px; }"
              "QMenu::item:selected { background-color: %4; }")
          .arg(palette.bgMedium.name())
          .arg(palette.textPrimary.name())
          .arg(palette.borderLight.name())
          .arg(palette.accentPrimary.name()));

  for (const auto &fix : fixes) {
    QAction *action = menu.addAction(fix.title);
    action->setToolTip(fix.description);
    connect(action, &QAction::triggered, this, [this, fix]() {
      if (auto *ed = currentEditor()) {
        ed->applyQuickFix(fix);
        m_diagnosticsTimer.start();
      }
    });
  }

  // Position menu at cursor
  const QRect cursorRect = editor->cursorRect();
  const QPoint globalPos = editor->mapToGlobal(cursorRect.bottomLeft());
  menu.exec(globalPos);
}

void NMScriptEditorPanel::setReadOnly(bool readOnly, const QString &reason) {
  if (m_readOnly == readOnly) {
    return;
  }

  m_readOnly = readOnly;

  // Create or update the read-only banner
  if (readOnly) {
    if (!m_readOnlyBanner) {
      m_readOnlyBanner = new QFrame(m_contentWidget);
      m_readOnlyBanner->setObjectName("WorkflowReadOnlyBanner");
      m_readOnlyBanner->setStyleSheet(
          "QFrame#WorkflowReadOnlyBanner {"
          "  background-color: #4a5568;"
          "  border: 1px solid #718096;"
          "  border-radius: 4px;"
          "  padding: 6px 12px;"
          "  margin: 4px 8px;"
          "}");

      auto *bannerLayout = new QHBoxLayout(m_readOnlyBanner);
      bannerLayout->setContentsMargins(8, 4, 8, 4);
      bannerLayout->setSpacing(8);

      // Info icon (using text for now)
      auto *iconLabel = new QLabel(QString::fromUtf8("\xE2\x84\xB9"), // ℹ
                                   m_readOnlyBanner);
      iconLabel->setStyleSheet("font-size: 14px; color: #e2e8f0;");
      bannerLayout->addWidget(iconLabel);

      m_readOnlyLabel = new QLabel(m_readOnlyBanner);
      m_readOnlyLabel->setStyleSheet("color: #e2e8f0; font-weight: bold;");
      bannerLayout->addWidget(m_readOnlyLabel);

      bannerLayout->addStretch();

      // Sync to Graph button for Graph mode
      m_syncToGraphBtn = new QPushButton(tr("Sync to Graph"), m_readOnlyBanner);
      m_syncToGraphBtn->setToolTip(
          tr("Parse script content and update Story Graph nodes"));
      m_syncToGraphBtn->setStyleSheet(
          "QPushButton { background-color: #4299e1; color: white; "
          "border: none; padding: 4px 12px; border-radius: 3px; }"
          "QPushButton:hover { background-color: #3182ce; }");
      connect(m_syncToGraphBtn, &QPushButton::clicked, this,
              &NMScriptEditorPanel::syncScriptToGraph);
      bannerLayout->addWidget(m_syncToGraphBtn);

      // Insert banner at the top of the content widget
      if (auto *layout =
              qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
        layout->insertWidget(0, m_readOnlyBanner);
      }
    }

    // Update banner text
    QString bannerText = tr("Read-only mode");
    if (!reason.isEmpty()) {
      bannerText += QString(" (%1)").arg(reason);
    }
    bannerText += tr(" - Script editing is disabled.");
    m_readOnlyLabel->setText(bannerText);
    m_readOnlyBanner->setVisible(true);

    // Hide sync button in Script mode (it wouldn't make sense)
    if (m_syncToGraphBtn) {
      m_syncToGraphBtn->setVisible(
          reason.contains("Graph", Qt::CaseInsensitive));
    }
  } else if (m_readOnlyBanner) {
    m_readOnlyBanner->setVisible(false);
  }

  // Disable/enable all editors
  for (auto *editor : editors()) {
    editor->setReadOnly(readOnly);
    if (readOnly) {
      editor->setStyleSheet("background-color: #2d3748;");
    } else {
      editor->setStyleSheet("");
    }
  }

  // Disable/enable toolbar save buttons
  if (m_toolBar) {
    for (auto *action : m_toolBar->actions()) {
      if (action->text().contains("Save", Qt::CaseInsensitive) ||
          action->text().contains("Format", Qt::CaseInsensitive)) {
        action->setEnabled(!readOnly);
      }
    }
  }

  qDebug() << "[ScriptEditor] Read-only mode:" << readOnly
           << "reason:" << reason;
}

void NMScriptEditorPanel::syncScriptToGraph() {
  qDebug() << "[ScriptEditor] Starting sync to graph...";

  // Collect all script content and parse scene data
  const QString scriptsRoot = scriptsRootPath();
  if (scriptsRoot.isEmpty()) {
    qWarning() << "[ScriptEditor] No scripts directory found";
    return;
  }

  namespace fs = std::filesystem;
  fs::path base(scriptsRoot.toStdString());
  if (!fs::exists(base)) {
    qWarning() << "[ScriptEditor] Scripts directory does not exist:"
               << scriptsRoot;
    return;
  }

  int synced = 0;
  int total = 0;

  // Parse each NMS file
  for (const auto &entry : fs::recursive_directory_iterator(base)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
      continue;
    }

    QFile file(QString::fromStdString(entry.path().string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Parse scene definitions
    const QRegularExpression sceneRe(
        "\\bscene\\s+([\\p{L}_][\\p{L}\\p{N}_]*)\\s*\\{",
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression sayRe(
        "\\bsay\\s*(?:\"([^\"]*)\"\\s*)?\"([^\"]*)\"",
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression choiceRe(
        "\\bchoice\\s*\\{([^}]*)\\}",
        QRegularExpression::UseUnicodePropertiesOption |
            QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression optionRe(
        "\"([^\"]+)\"\\s*->\\s*([\\p{L}_][\\p{L}\\p{N}_]*)",
        QRegularExpression::UseUnicodePropertiesOption);

    // Find each scene
    QRegularExpressionMatchIterator sceneIt = sceneRe.globalMatch(content);
    while (sceneIt.hasNext()) {
      const QRegularExpressionMatch sceneMatch = sceneIt.next();
      const QString sceneName = sceneMatch.captured(1);
      const qsizetype sceneStart = sceneMatch.capturedEnd();

      // Find scene end (matching brace)
      int braceCount = 1;
      qsizetype sceneEnd = sceneStart;
      for (qsizetype i = sceneStart; i < content.size() && braceCount > 0;
           ++i) {
        if (content[i] == '{') {
          braceCount++;
        } else if (content[i] == '}') {
          braceCount--;
        }
        sceneEnd = i;
      }

      const QString sceneContent = content.mid(sceneStart, sceneEnd - sceneStart);

      // Extract first say statement
      QString speaker;
      QString dialogue;
      const QRegularExpressionMatch sayMatch = sayRe.match(sceneContent);
      if (sayMatch.hasMatch()) {
        speaker = sayMatch.captured(1);
        dialogue = sayMatch.captured(2);
      }

      // Extract choices
      QStringList choices;
      const QRegularExpressionMatch choiceMatch = choiceRe.match(sceneContent);
      if (choiceMatch.hasMatch()) {
        const QString choiceBlock = choiceMatch.captured(1);
        QRegularExpressionMatchIterator optionIt = optionRe.globalMatch(choiceBlock);
        while (optionIt.hasNext()) {
          choices.append(optionIt.next().captured(1));
        }
      }

      // Emit signal to update graph
      if (!dialogue.isEmpty() || !choices.isEmpty()) {
        emit syncToGraphRequested(sceneName, speaker, dialogue, choices);
        synced++;
      }
      total++;
    }
  }

  qDebug() << "[ScriptEditor] Sync complete:" << synced << "of" << total
           << "scenes with data";
}

void NMScriptEditorPanel::loadSampleScript(const QString &sampleId) {
  // Sample script content based on sampleId
  QString scriptContent;
  QString fileName;

  if (sampleId == "basic") {
    fileName = "sample_basic.nms";
    scriptContent = R"(// Sample Script: Basic Scene
// A simple introduction to NMScript with dialogue and characters

// Define characters
character Alice(name="Alice", color="#4A90D9")
character Bob(name="Bob", color="#E74C3C")

// Main scene
scene intro {
    // Set the background
    show background "bg_room"

    // Show Alice at center
    show Alice at center

    // Basic dialogue
    say Alice "Hello! Welcome to the Script Editor."
    say Alice "This is a basic scene demonstrating character dialogue."

    // Show Bob entering
    show Bob at right with slide_left

    say Bob "Hi Alice! Great to be here."
    say Alice "Let me show you around!"

    // Transition to next scene
    goto exploration
}

scene exploration {
    say Alice "You can create scenes, characters, and dialogue easily."
    say Bob "This is amazing!"

    // End of sample
    say Alice "Try experimenting with your own scripts!"
}
)";
  } else if (sampleId == "choices") {
    fileName = "sample_choices.nms";
    scriptContent = R"(// Sample Script: Choice System
// Demonstrates branching dialogue with player choices and flags

character Hero(name="Hero", color="#2ECC71")
character Guide(name="Guide", color="#3498DB")

scene start {
    show background "bg_crossroads"
    show Guide at center

    say Guide "Welcome, traveler! You've reached a crossroads."
    say Guide "Which path will you choose?"

    // Player choice
    choice {
        "Take the forest path" -> {
            set flag chose_forest = true
            say Hero "I'll go through the forest."
            goto forest_path
        }
        "Take the mountain pass" -> {
            set flag chose_mountain = true
            say Hero "The mountain pass looks challenging."
            goto mountain_path
        }
        "Ask for advice" -> {
            say Hero "What would you recommend?"
            say Guide "Both paths have their rewards."
            goto start
        }
    }
}

scene forest_path {
    show background "bg_forest"
    say Hero "The forest is beautiful and peaceful."

    if flag chose_forest {
        say Guide "A wise choice for those who appreciate nature."
    }

    goto ending
}

scene mountain_path {
    show background "bg_mountain"
    say Hero "The view from up here is breathtaking!"

    if flag chose_mountain {
        say Guide "The brave path rewards those who take it."
    }

    goto ending
}

scene ending {
    say Guide "Your journey continues..."
    say Hero "Thank you for the guidance!"
}
)";
  } else if (sampleId == "advanced") {
    fileName = "sample_advanced.nms";
    scriptContent = R"(// Sample Script: Advanced Features
// Showcases variables, conditionals, transitions, and more

character Sage(name="Elder Sage", color="#9B59B6")
character Player(name="You", color="#1ABC9C")

scene intro {
    // Fade transition
    transition fade 1.0

    show background "bg_temple"
    play music "ambient_mystical" loop=true

    wait 0.5

    show Sage at center with fade

    say Sage "Welcome to the ancient temple."
    say Sage "Let me test your wisdom..."

    // Initialize score variable
    set score = 0
    set max_questions = 3

    goto question_1
}

scene question_1 {
    say Sage "First question: What is the most valuable treasure?"

    choice {
        "Gold and jewels" -> {
            say Player "Wealth and riches!"
            set score = score + 0
            goto question_2
        }
        "Knowledge and wisdom" -> {
            say Player "Knowledge that lasts forever."
            set score = score + 10
            say Sage "A wise answer."
            goto question_2
        }
        "Friends and family" -> {
            say Player "The people we love."
            set score = score + 10
            say Sage "True wisdom."
            goto question_2
        }
    }
}

scene question_2 {
    say Sage "Second question: How do you face challenges?"

    choice {
        "With courage" -> {
            set score = score + 10
            goto question_3
        }
        "With caution" -> {
            set score = score + 5
            goto question_3
        }
        "By avoiding them" -> {
            set score = score + 0
            goto question_3
        }
    }
}

scene question_3 {
    say Sage "Final question: What drives you forward?"

    choice {
        "Personal glory" -> goto results
        "Helping others" -> {
            set score = score + 10
            goto results
        }
        "Curiosity" -> {
            set score = score + 5
            goto results
        }
    }
}

scene results {
    say Sage "Let me see your results..."

    wait 1.0

    if score >= 25 {
        // High score path
        play sound "success_chime"
        say Sage "Exceptional! You possess great wisdom."
        show Sage with "proud"
        goto good_ending
    } else if score >= 15 {
        // Medium score
        say Sage "Good! You show promise."
        goto normal_ending
    } else {
        // Low score
        say Sage "You have much to learn, young one."
        goto learning_ending
    }
}

scene good_ending {
    transition fade 0.5
    show background "bg_temple_golden"

    say Sage "I shall teach you the ancient arts."

    flash color="#FFD700" duration=0.3

    say Player "Thank you, Master!"

    transition fade 2.0
}

scene normal_ending {
    say Sage "Return when you've gained more experience."
    say Player "I will!"
}

scene learning_ending {
    say Sage "Study and return to try again."
    say Player "I understand."
}
)";
  } else {
    core::Logger::instance().warning("Unknown sample script ID: " +
                                     sampleId.toStdString());
    return;
  }

  // Create sample script in project scripts folder
  const QString scriptsPath = scriptsRootPath();
  if (scriptsPath.isEmpty()) {
    core::Logger::instance().warning(
        "Cannot load sample script: No scripts folder found");
    return;
  }

  const QString fullPath = scriptsPath + "/" + fileName;

  // Write the sample script
  QFile file(fullPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out << scriptContent;
    file.close();

    core::Logger::instance().info("Created sample script: " +
                                  fullPath.toStdString());

    // Refresh file list and open the sample
    refreshFileList();
    openScript(fullPath);
  } else {
    core::Logger::instance().error("Failed to create sample script: " +
                                   fullPath.toStdString());
  }
}

// ============================================================================
// State Persistence
// ============================================================================

void NMScriptEditorPanel::saveState() {
  QSettings settings;

  // Save splitter positions
  if (m_splitter) {
    settings.setValue("scriptEditor/splitterState", m_splitter->saveState());
  }
  if (m_leftSplitter) {
    settings.setValue("scriptEditor/leftSplitterState", m_leftSplitter->saveState());
  }

  // Save open files
  QStringList openFiles;
  for (int i = 0; i < m_tabs->count(); ++i) {
    QWidget *widget = m_tabs->widget(i);
    QString path = m_tabPaths.value(widget);
    if (!path.isEmpty()) {
      openFiles.append(path);
    }
  }
  settings.setValue("scriptEditor/openFiles", openFiles);

  // Save active file index
  settings.setValue("scriptEditor/activeFileIndex", m_tabs->currentIndex());

  // Save cursor positions for each open file
  for (int i = 0; i < m_tabs->count(); ++i) {
    if (auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->widget(i))) {
      QString path = m_tabPaths.value(editor);
      if (!path.isEmpty()) {
        QTextCursor cursor = editor->textCursor();
        settings.setValue(QString("scriptEditor/cursorPos/%1").arg(path), cursor.position());
      }
    }
  }

  // Save minimap visibility
  settings.setValue("scriptEditor/minimapVisible", m_minimapEnabled);
}

void NMScriptEditorPanel::restoreState() {
  QSettings settings;

  // Restore splitter positions
  if (m_splitter) {
    QByteArray state = settings.value("scriptEditor/splitterState").toByteArray();
    if (!state.isEmpty()) {
      m_splitter->restoreState(state);
    }
  }
  if (m_leftSplitter) {
    QByteArray state = settings.value("scriptEditor/leftSplitterState").toByteArray();
    if (!state.isEmpty()) {
      m_leftSplitter->restoreState(state);
    }
  }

  // Restore minimap visibility
  m_minimapEnabled = settings.value("scriptEditor/minimapVisible", true).toBool();

  // Restore open files if enabled
  bool restoreOpenFiles = settings.value("editor.script.restore_open_files", true).toBool();
  if (restoreOpenFiles) {
    QStringList openFiles = settings.value("scriptEditor/openFiles").toStringList();
    int activeIndex = settings.value("scriptEditor/activeFileIndex", 0).toInt();

    for (const QString &path : openFiles) {
      if (QFileInfo::exists(path)) {
        openScript(path);

        // Restore cursor position if enabled
        bool restoreCursor = settings.value("editor.script.restore_cursor_position", true).toBool();
        if (restoreCursor) {
          if (auto *editor = qobject_cast<NMScriptEditor *>(m_tabs->currentWidget())) {
            int cursorPos = settings.value(QString("scriptEditor/cursorPos/%1").arg(path), 0).toInt();
            QTextCursor cursor = editor->textCursor();
            cursor.setPosition(cursorPos);
            editor->setTextCursor(cursor);
          }
        }
      }
    }

    // Restore active tab
    if (activeIndex >= 0 && activeIndex < m_tabs->count()) {
      m_tabs->setCurrentIndex(activeIndex);
    }
  }

  // Apply settings from registry
  applySettings();
}

void NMScriptEditorPanel::applySettings() {
  QSettings settings;

  // Apply diagnostic delay
  int diagnosticDelay = settings.value("editor.script.diagnostic_delay", 600).toInt();
  m_diagnosticsTimer.setInterval(diagnosticDelay);

  // Apply settings to all open editors
  for (auto *editor : editors()) {
    if (!editor) continue;

    // Font settings
    QString fontFamily = settings.value("editor.script.font_family", "monospace").toString();
    int fontSize = settings.value("editor.script.font_size", 14).toInt();
    QFont font(fontFamily, fontSize);
    editor->setFont(font);

    // Display settings
    bool showMinimap = settings.value("editor.script.show_minimap", true).toBool();
    editor->setMinimapEnabled(showMinimap);
    m_minimapEnabled = showMinimap;

    // Word wrap
    bool wordWrap = settings.value("editor.script.word_wrap", false).toBool();
    editor->setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    // Tab size (note: the editor already has a tab size property)
    int tabSize = settings.value("editor.script.tab_size", 4).toInt();
    QFontMetrics metrics(font);
    editor->setTabStopDistance(tabSize * metrics.horizontalAdvance(' '));
  }
}

// ============================================================================
// File Conflict Detection (Issue #246)
// ============================================================================

NMScriptEditor *
NMScriptEditorPanel::findEditorForPath(const QString &path) const {
  for (int i = 0; i < m_tabs->count(); ++i) {
    QWidget *widget = m_tabs->widget(i);
    if (m_tabPaths.value(widget) == path) {
      return qobject_cast<NMScriptEditor *>(widget);
    }
  }
  return nullptr;
}

bool NMScriptEditorPanel::isTabModified(const QWidget *editor) const {
  if (!editor || !m_tabs) {
    return false;
  }
  const int index = m_tabs->indexOf(const_cast<QWidget *>(editor));
  if (index < 0) {
    return false;
  }
  // Check if tab text has "*" suffix indicating unsaved changes
  return m_tabs->tabText(index).endsWith("*");
}

QDateTime
NMScriptEditorPanel::getEditorSaveTime(const QWidget *editor) const {
  return m_editorSaveTimes.value(const_cast<QWidget *>(editor));
}

void NMScriptEditorPanel::setEditorSaveTime(QWidget *editor,
                                            const QDateTime &time) {
  if (editor) {
    m_editorSaveTimes.insert(editor, time);
  }
}

void NMScriptEditorPanel::onFileChanged(const QString &path) {
  // Find if this file is open in an editor tab
  NMScriptEditor *editor = findEditorForPath(path);
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
    core::Logger::instance().warning(
        "File was deleted externally: " + path.toStdString());
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

void NMScriptEditorPanel::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path);
  // Directory changes don't affect open files, just refresh file list
  refreshFileList();
  refreshSymbolIndex();
}

void NMScriptEditorPanel::showConflictDialog(const QString &path,
                                              NMScriptEditor *editor) {
  if (!editor) {
    return;
  }

  const QString fileName = QFileInfo(path).fileName();
  const QString message =
      tr("The file \"%1\" has been modified externally, but you have unsaved "
         "changes in the editor.\n\nWhat would you like to do?")
          .arg(fileName);

  // Create custom dialog with three options
  auto *dialog = new QDialog(this);
  dialog->setWindowTitle(tr("File Conflict Detected"));
  dialog->setModal(true);
  dialog->setMinimumWidth(450);

  auto *layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Warning icon and message
  auto *messageLayout = new QHBoxLayout();
  auto *iconLabel = new QLabel(dialog);
  iconLabel->setText("⚠️");
  iconLabel->setStyleSheet("font-size: 32px;");
  messageLayout->addWidget(iconLabel);

  auto *messageLabel = new QLabel(message, dialog);
  messageLabel->setWordWrap(true);
  messageLayout->addWidget(messageLabel, 1);
  layout->addLayout(messageLayout);

  // Button layout
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  auto *keepMyChangesBtn =
      new QPushButton(tr("Keep My Changes"), dialog);
  keepMyChangesBtn->setToolTip(
      tr("Discard the external file version and keep your unsaved changes"));
  buttonLayout->addWidget(keepMyChangesBtn);

  auto *useFileVersionBtn =
      new QPushButton(tr("Use File Version"), dialog);
  useFileVersionBtn->setToolTip(
      tr("Discard your unsaved changes and reload from disk"));
  buttonLayout->addWidget(useFileVersionBtn);

  auto *cancelBtn = new QPushButton(tr("Cancel"), dialog);
  cancelBtn->setToolTip(tr("Do nothing for now"));
  buttonLayout->addWidget(cancelBtn);

  layout->addLayout(buttonLayout);

  // Connect buttons
  int result = 0; // 1 = Keep Mine, 2 = Use File, 0 = Cancel
  connect(keepMyChangesBtn, &QPushButton::clicked, dialog,
          [dialog, &result]() {
            result = 1;
            dialog->accept();
          });
  connect(useFileVersionBtn, &QPushButton::clicked, dialog,
          [dialog, &result]() {
            result = 2;
            dialog->accept();
          });
  connect(cancelBtn, &QPushButton::clicked, dialog, [dialog]() {
    dialog->reject();
  });

  dialog->exec();

  if (result == 1) {
    // Keep My Changes - force save over file version
    core::Logger::instance().info(
        "User chose to keep editor changes for: " + path.toStdString());
    saveEditor(editor);
  } else if (result == 2) {
    // Use File Version - reload from disk
    core::Logger::instance().info(
        "User chose to reload file from disk: " + path.toStdString());
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

void NMScriptEditorPanel::showReloadPrompt(const QString &path,
                                            NMScriptEditor *editor) {
  if (!editor) {
    return;
  }

  const QString fileName = QFileInfo(path).fileName();
  const QString message =
      tr("The file \"%1\" has been modified externally.\n\nDo you want to "
         "reload it from disk?")
          .arg(fileName);

  // Create simple reload dialog
  auto *dialog = new QDialog(this);
  dialog->setWindowTitle(tr("File Changed Externally"));
  dialog->setModal(true);
  dialog->setMinimumWidth(400);

  auto *layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Info icon and message
  auto *messageLayout = new QHBoxLayout();
  auto *iconLabel = new QLabel(dialog);
  iconLabel->setText("ℹ️");
  iconLabel->setStyleSheet("font-size: 28px;");
  messageLayout->addWidget(iconLabel);

  auto *messageLabel = new QLabel(message, dialog);
  messageLabel->setWordWrap(true);
  messageLayout->addWidget(messageLabel, 1);
  layout->addLayout(messageLayout);

  // Button layout
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  auto *reloadBtn = new QPushButton(tr("Reload"), dialog);
  reloadBtn->setToolTip(tr("Reload the file from disk"));
  buttonLayout->addWidget(reloadBtn);

  auto *ignoreBtn = new QPushButton(tr("Ignore"), dialog);
  ignoreBtn->setToolTip(
      tr("Keep current version, you will be warned on save"));
  buttonLayout->addWidget(ignoreBtn);

  layout->addLayout(buttonLayout);

  // Connect buttons
  bool shouldReload = false;
  connect(reloadBtn, &QPushButton::clicked, dialog,
          [dialog, &shouldReload]() {
            shouldReload = true;
            dialog->accept();
          });
  connect(ignoreBtn, &QPushButton::clicked, dialog,
          [dialog]() { dialog->reject(); });

  dialog->exec();

  if (shouldReload) {
    core::Logger::instance().info("User chose to reload file: " +
                                  path.toStdString());
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
