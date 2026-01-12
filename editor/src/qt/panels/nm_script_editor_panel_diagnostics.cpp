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

} // namespace NovelMind::editor::qt
