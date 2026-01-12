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

void NMScriptEditorPanel::onGoToDefinition(const QString &symbol,
                                           const SymbolLocation &location) {
  (void)symbol;
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

} // namespace NovelMind::editor::qt
