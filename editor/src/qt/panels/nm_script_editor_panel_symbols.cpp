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

void NMScriptEditorPanel::refreshSymbolIndex() {
  core::Logger::instance().info("applyProjectToPanels: refreshSymbolIndex starting");
  QMutexLocker locker(&m_symbolIndexMutex);
  m_symbolIndex = {};
  const QString root = scriptsRootPath();
  if (root.isEmpty()) {
    pushCompletionsToEditors();
    refreshSymbolList();
    if (m_issuesPanel) {
      m_issuesPanel->setIssues({});
    }
    core::Logger::instance().info("applyProjectToPanels: refreshSymbolIndex completed (empty root)");
    return;
  }

  namespace fs = std::filesystem;
  fs::path base(root.toStdString());
  if (!fs::exists(base)) {
    pushCompletionsToEditors();
    refreshSymbolList();
    core::Logger::instance().info("applyProjectToPanels: refreshSymbolIndex completed (root not exists)");
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

  core::Logger::instance().info("applyProjectToPanels: Starting directory iteration for symbol index");
  int filesProcessed = 0;
  try {
    // Use skip_permission_denied to avoid hanging on problematic directories
    auto options = fs::directory_options::skip_permission_denied;
    for (const auto &entry : fs::recursive_directory_iterator(base, options)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
        continue;
      }
      filesProcessed++;

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
    core::Logger::instance().info("applyProjectToPanels: Directory iteration for symbol index completed, processed " + std::to_string(filesProcessed) + " files");
  } catch (const std::exception &e) {
    core::Logger::instance().warning(
        std::string("Failed to build script symbols: ") + e.what());
  }

  core::Logger::instance().info("applyProjectToPanels: Adding project context assets");
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

  core::Logger::instance().info("applyProjectToPanels: Pushing completions to editors");
  pushCompletionsToEditors();
  core::Logger::instance().info("applyProjectToPanels: Refreshing symbol list");
  refreshSymbolList();
  if (m_issuesPanel) {
    m_issuesPanel->setIssues(issues);
  }
  core::Logger::instance().info("applyProjectToPanels: refreshSymbolIndex completed");
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

} // namespace NovelMind::editor::qt
