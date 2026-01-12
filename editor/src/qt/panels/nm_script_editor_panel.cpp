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
          QSettings welcomeSettings("NovelMind", "Editor");
          welcomeSettings.setValue("scriptEditor/showWelcome", false);
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

} // namespace NovelMind::editor::qt
