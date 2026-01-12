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

void NMScriptEditorPanel::showCommandPalette() {
  if (m_commandPalette) {
    m_commandPalette->show();
  }
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

} // namespace NovelMind::editor::qt
