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

  auto &iconMgr = NMIconManager::instance();

  // File operations group
  QAction *actionSave = m_toolBar->addAction(tr("Save"));
  actionSave->setIcon(iconMgr.getIcon("file-save", 16));
  actionSave->setToolTip(tr("Save (Ctrl+S)"));
  connect(actionSave, &QAction::triggered, this,
          &NMScriptEditorPanel::onSaveRequested);

  QAction *actionSaveAll = m_toolBar->addAction(tr("Save All"));
  actionSaveAll->setIcon(iconMgr.getIcon("file-save", 16));
  actionSaveAll->setToolTip(tr("Save all open scripts"));
  connect(actionSaveAll, &QAction::triggered, this,
          &NMScriptEditorPanel::onSaveAllRequested);

  m_toolBar->addSeparator();

  // Edit operations group
  QAction *actionFormat = m_toolBar->addAction(tr("Format"));
  actionFormat->setIcon(iconMgr.getIcon("transform-scale", 16));
  actionFormat->setToolTip(tr("Auto-format script (Ctrl+Shift+F)"));
  actionFormat->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
  actionFormat->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionFormat);
  connect(actionFormat, &QAction::triggered, this,
          &NMScriptEditorPanel::onFormatRequested);

  m_toolBar->addSeparator();

  // Toggle Preview action (issue #240)
  m_togglePreviewAction = m_toolBar->addAction(tr("👁️ Preview"));
  m_togglePreviewAction->setIcon(iconMgr.getIcon("visible", 16));
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

  // Code operations group
  QAction *actionSnippet = m_toolBar->addAction(tr("Insert"));
  actionSnippet->setIcon(iconMgr.getIcon("add", 16));
  actionSnippet->setToolTip(tr("Insert code snippet (Ctrl+J)"));
  actionSnippet->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
  actionSnippet->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionSnippet);
  connect(actionSnippet, &QAction::triggered, this,
          &NMScriptEditorPanel::onInsertSnippetRequested);

  QAction *actionSymbols = m_toolBar->addAction(tr("Symbols"));
  actionSymbols->setIcon(iconMgr.getIcon("search", 16));
  actionSymbols->setToolTip(tr("Open symbol navigator (Ctrl+Shift+O)"));
  actionSymbols->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  actionSymbols->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(actionSymbols);
  connect(actionSymbols, &QAction::triggered, this,
          &NMScriptEditorPanel::onSymbolNavigatorRequested);

  m_toolBar->addSeparator();

  // View dropdown menu
  QToolButton *viewBtn = new QToolButton(m_toolBar);
  viewBtn->setText(tr("View"));
  viewBtn->setIcon(iconMgr.getIcon("visible", 16));
  viewBtn->setToolTip(tr("View options"));
  viewBtn->setPopupMode(QToolButton::InstantPopup);
  viewBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  QMenu *viewMenu = new QMenu(viewBtn);
  QAction *actionToggleMinimap = viewMenu->addAction(tr("Toggle Minimap"));
  actionToggleMinimap->setIcon(iconMgr.getIcon("layout-grid", 16));
  connect(actionToggleMinimap, &QAction::triggered, this,
          &NMScriptEditorPanel::onToggleMinimap);

  QAction *actionFoldAll = viewMenu->addAction(tr("Fold All"));
  actionFoldAll->setIcon(iconMgr.getIcon("chevron-up", 16));
  connect(actionFoldAll, &QAction::triggered, this,
          &NMScriptEditorPanel::onFoldAll);

  QAction *actionUnfoldAll = viewMenu->addAction(tr("Unfold All"));
  actionUnfoldAll->setIcon(iconMgr.getIcon("chevron-down", 16));
  connect(actionUnfoldAll, &QAction::triggered, this,
          &NMScriptEditorPanel::onUnfoldAll);

  viewBtn->setMenu(viewMenu);
  m_toolBar->addWidget(viewBtn);
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

} // namespace NovelMind::editor::qt
