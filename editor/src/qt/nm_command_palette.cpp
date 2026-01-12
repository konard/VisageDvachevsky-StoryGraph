#include "NovelMind/editor/qt/nm_command_palette.hpp"
#include "NovelMind/editor/qt/nm_fuzzy_matcher.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace NovelMind::editor::qt {

// ============================================================================
// CommandItem
// ============================================================================

QString CommandItem::categoryLabel() const {
  switch (type) {
  case CommandItemType::Panel:
    return QObject::tr("Panel");
  case CommandItemType::Command:
    return QObject::tr("Command");
  case CommandItemType::Workspace:
    return QObject::tr("Workspace");
  case CommandItemType::RecentScene:
    return QObject::tr("Recent Scene");
  case CommandItemType::RecentScript:
    return QObject::tr("Recent Script");
  }
  return QString();
}

// ============================================================================
// NMCommandPalette
// ============================================================================

NMCommandPalette::NMCommandPalette(QWidget* parent, const QList<QAction*>& actions, Mode mode)
    : QDialog(parent), m_mode(mode), m_actions(actions) {
  setWindowFlag(Qt::FramelessWindowHint);
  setWindowModality(Qt::ApplicationModal);
  setAttribute(Qt::WA_DeleteOnClose);
  setMinimumWidth(500);
  setMaximumWidth(700);
  setObjectName("CommandPalette");

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  // Search input
  m_input = new QLineEdit(this);
  m_input->setObjectName("CommandPaletteInput");
  m_input->installEventFilter(this);

  // Set placeholder based on mode
  if (m_mode == Mode::Panels) {
    m_input->setPlaceholderText(tr("Type panel name..."));
  } else {
    m_input->setPlaceholderText(tr("Type command or panel name..."));
  }

  layout->addWidget(m_input);

  // Results list
  m_list = new QListWidget(this);
  m_list->setObjectName("CommandPaletteList");
  m_list->setMinimumHeight(300);
  m_list->setMaximumHeight(500);
  layout->addWidget(m_list, 1);

  // Connections
  connect(m_input, &QLineEdit::textChanged, this, &NMCommandPalette::onFilterChanged);
  connect(m_list, &QListWidget::itemActivated, this, &NMCommandPalette::onItemActivated);

  // Build and populate
  buildCommandList();
  populateList();
  m_input->setFocus();
}

void NMCommandPalette::openCentered(QWidget* anchor) {
  if (!anchor) {
    show();
    return;
  }

  // Ensure size is calculated
  adjustSize();

  // Center over anchor
  const QRect geom = anchor->geometry();
  const QPoint globalCenter = anchor->mapToGlobal(geom.center());
  move(globalCenter.x() - width() / 2, globalCenter.y() - height() / 2);

  show();
  m_input->setFocus();
}

void NMCommandPalette::addRecentScene(const QString& sceneName) {
  CommandItem item;
  item.name = sceneName;
  item.searchableText = sceneName;
  item.tooltip = tr("Recently opened scene");
  item.type = CommandItemType::RecentScene;
  item.iconName = "file";

  // Add to front of list (most recent first)
  m_commands.prepend(item);
}

void NMCommandPalette::addRecentScript(const QString& scriptPath) {
  CommandItem item;
  item.name = scriptPath;
  item.searchableText = scriptPath;
  item.tooltip = tr("Recently edited script");
  item.type = CommandItemType::RecentScript;
  item.iconName = "file-code";

  // Add to front of list (most recent first)
  m_commands.prepend(item);
}

void NMCommandPalette::clearRecentItems() {
  // Remove all recent items
  m_commands.erase(std::remove_if(m_commands.begin(), m_commands.end(),
                                  [](const CommandItem& item) {
                                    return item.type == CommandItemType::RecentScene ||
                                           item.type == CommandItemType::RecentScript;
                                  }),
                   m_commands.end());
}

bool NMCommandPalette::eventFilter(QObject* obj, QEvent* event) {
  if (obj == m_input && event->type() == QEvent::KeyPress) {
    auto* keyEvent = static_cast<QKeyEvent*>(event);

    // Escape closes dialog
    if (keyEvent->key() == Qt::Key_Escape) {
      reject();
      return true;
    }

    // Up arrow navigates list
    if (keyEvent->key() == Qt::Key_Up) {
      int row = m_list->currentRow();
      if (row > 0) {
        m_list->setCurrentRow(row - 1);
      }
      return true;
    }

    // Down arrow navigates list
    if (keyEvent->key() == Qt::Key_Down) {
      int row = m_list->currentRow();
      if (row < m_list->count() - 1) {
        m_list->setCurrentRow(row + 1);
      } else if (row == -1 && m_list->count() > 0) {
        m_list->setCurrentRow(0);
      }
      return true;
    }

    // Enter/Return activates selected item
    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
      QListWidgetItem* item = m_list->currentItem();
      if (item) {
        onItemActivated(item);
      }
      return true;
    }
  }

  return QDialog::eventFilter(obj, event);
}

void NMCommandPalette::onFilterChanged(const QString& text) {
  updateFilteredList(text);
}

void NMCommandPalette::onItemActivated(QListWidgetItem* item) {
  if (!item) {
    return;
  }

  // Get command index from item data
  int index = item->data(Qt::UserRole).toInt();
  if (index < 0 || index >= m_commands.size()) {
    return;
  }

  const CommandItem& cmd = m_commands[index];

  // Execute action if available
  if (cmd.action && cmd.action->isEnabled()) {
    cmd.action->trigger();
  }

  // Record panel access for recent items tracking
  if (cmd.type == CommandItemType::Panel) {
    NMRecentItemsTracker::instance().recordPanelAccess(cmd.name);
  }

  accept();
}

void NMCommandPalette::buildCommandList() {
  m_commands.clear();

  // Build from actions
  for (QAction* action : m_actions) {
    if (!action || action->text().isEmpty()) {
      continue;
    }

    CommandItem item;
    item.name = action->text().remove('&'); // Remove mnemonics
    item.action = action;
    item.type = determineItemType(action);

    // Skip non-panel items if in Panels mode
    if (m_mode == Mode::Panels && item.type != CommandItemType::Panel) {
      continue;
    }

    // Build searchable text (includes name and metadata)
    QString meta = action->toolTip();
    if (meta.isEmpty()) {
      meta = action->statusTip();
    }
    item.searchableText = item.name + " " + meta;

    // Tooltip
    item.tooltip = meta;

    // Shortcut
    item.shortcut = action->shortcut().toString();

    // Icon
    item.iconName = getIconNameForAction(action);

    m_commands.append(item);
  }

  // Add recent items from tracker (only in All mode)
  if (m_mode == Mode::All) {
    auto& tracker = NMRecentItemsTracker::instance();

    // Recent scenes
    for (const QString& scene : tracker.getRecentScenes(5)) {
      addRecentScene(scene);
    }

    // Recent scripts
    for (const QString& script : tracker.getRecentScripts(5)) {
      addRecentScript(script);
    }
  }
}

void NMCommandPalette::populateList() {
  m_list->clear();

  // Show all items initially
  for (int i = 0; i < m_commands.size(); ++i) {
    const CommandItem& item = m_commands[i];
    addListItem(item);

    // Store command index in item data
    QListWidgetItem* listItem = m_list->item(m_list->count() - 1);
    listItem->setData(Qt::UserRole, i);
  }

  // Select first item
  if (m_list->count() > 0) {
    m_list->setCurrentRow(0);
  }
}

void NMCommandPalette::updateFilteredList(const QString& filter) {
  m_list->clear();

  // If filter is empty, show all items
  if (filter.isEmpty()) {
    populateList();
    return;
  }

  // Build list of matched items with scores
  QList<QPair<int, int>> matches; // <score, index>

  for (int i = 0; i < m_commands.size(); ++i) {
    const CommandItem& item = m_commands[i];

    // Perform fuzzy match
    auto result = NMFuzzyMatcher::match(filter, item.searchableText);
    if (result.matched) {
      matches.append(qMakePair(result.score, i));
    }
  }

  // Sort by score (highest first)
  std::sort(matches.begin(), matches.end(),
            [](const QPair<int, int>& a, const QPair<int, int>& b) { return a.first > b.first; });

  // Add matched items to list
  for (const auto& match : matches) {
    int index = match.second;
    const CommandItem& item = m_commands[index];
    addListItem(item);

    // Store command index in item data
    QListWidgetItem* listItem = m_list->item(m_list->count() - 1);
    listItem->setData(Qt::UserRole, index);
  }

  // Select first item
  if (m_list->count() > 0) {
    m_list->setCurrentRow(0);
  }
}

CommandItemType NMCommandPalette::determineItemType(QAction* action) const {
  if (!action) {
    return CommandItemType::Command;
  }

  const QString text = action->text();

  // Panel toggle actions typically contain "Show" or "Toggle"
  if (text.contains("Scene View", Qt::CaseInsensitive) ||
      text.contains("Story Graph", Qt::CaseInsensitive) ||
      text.contains("Inspector", Qt::CaseInsensitive) ||
      text.contains("Console", Qt::CaseInsensitive) ||
      text.contains("Asset Browser", Qt::CaseInsensitive) ||
      text.contains("Scene Palette", Qt::CaseInsensitive) ||
      text.contains("Hierarchy", Qt::CaseInsensitive) ||
      text.contains("Script Editor", Qt::CaseInsensitive) ||
      text.contains("Script Doc", Qt::CaseInsensitive) ||
      text.contains("Issues", Qt::CaseInsensitive) ||
      text.contains("Diagnostics", Qt::CaseInsensitive) ||
      text.contains("Debug Overlay", Qt::CaseInsensitive) ||
      text.contains("Script Runtime", Qt::CaseInsensitive) ||
      text.contains("Voice Manager", Qt::CaseInsensitive) ||
      text.contains("Voice Studio", Qt::CaseInsensitive) ||
      text.contains("Audio Mixer", Qt::CaseInsensitive) ||
      text.contains("Localization", Qt::CaseInsensitive) ||
      text.contains("Timeline", Qt::CaseInsensitive) ||
      text.contains("Curve Editor", Qt::CaseInsensitive) ||
      text.contains("Build Settings", Qt::CaseInsensitive) ||
      text.contains("Project Settings", Qt::CaseInsensitive)) {
    return CommandItemType::Panel;
  }

  // Workspace presets
  if (text.contains("Workspace", Qt::CaseInsensitive) ||
      text.contains("Layout:", Qt::CaseInsensitive) ||
      text.contains("Story Script", Qt::CaseInsensitive) ||
      text.contains("Scene Animation", Qt::CaseInsensitive) ||
      text.contains("Audio Voice", Qt::CaseInsensitive)) {
    return CommandItemType::Workspace;
  }

  return CommandItemType::Command;
}

QString NMCommandPalette::getIconNameForAction(QAction* action) const {
  if (!action) {
    return QString();
  }

  const QString text = action->text();
  CommandItemType type = determineItemType(action);

  // Panel icons
  if (type == CommandItemType::Panel) {
    if (text.contains("Scene View", Qt::CaseInsensitive))
      return "layout";
    if (text.contains("Story Graph", Qt::CaseInsensitive))
      return "network";
    if (text.contains("Inspector", Qt::CaseInsensitive))
      return "info";
    if (text.contains("Console", Qt::CaseInsensitive))
      return "terminal";
    if (text.contains("Asset Browser", Qt::CaseInsensitive))
      return "folder";
    if (text.contains("Hierarchy", Qt::CaseInsensitive))
      return "list-tree";
    if (text.contains("Script", Qt::CaseInsensitive))
      return "file-code";
    if (text.contains("Timeline", Qt::CaseInsensitive))
      return "timeline";
    return "panel";
  }

  // Workspace icons
  if (type == CommandItemType::Workspace) {
    return "layout-dashboard";
  }

  // Command icons (based on common patterns)
  if (text.contains("New", Qt::CaseInsensitive))
    return "file-plus";
  if (text.contains("Open", Qt::CaseInsensitive))
    return "folder-open";
  if (text.contains("Save", Qt::CaseInsensitive))
    return "save";
  if (text.contains("Close", Qt::CaseInsensitive))
    return "x";
  if (text.contains("Exit", Qt::CaseInsensitive) || text.contains("Quit", Qt::CaseInsensitive))
    return "log-out";
  if (text.contains("Undo", Qt::CaseInsensitive))
    return "undo";
  if (text.contains("Redo", Qt::CaseInsensitive))
    return "redo";
  if (text.contains("Cut", Qt::CaseInsensitive))
    return "scissors";
  if (text.contains("Copy", Qt::CaseInsensitive))
    return "copy";
  if (text.contains("Paste", Qt::CaseInsensitive))
    return "clipboard";
  if (text.contains("Delete", Qt::CaseInsensitive))
    return "trash";
  if (text.contains("Settings", Qt::CaseInsensitive) ||
      text.contains("Preferences", Qt::CaseInsensitive))
    return "settings";
  if (text.contains("Play", Qt::CaseInsensitive))
    return "play";
  if (text.contains("Pause", Qt::CaseInsensitive))
    return "pause";
  if (text.contains("Stop", Qt::CaseInsensitive))
    return "stop-circle";
  if (text.contains("Help", Qt::CaseInsensitive))
    return "help-circle";
  if (text.contains("About", Qt::CaseInsensitive))
    return "info";

  // Default icon
  return "command";
}

void NMCommandPalette::addListItem(const CommandItem& item) {
  QString displayText = item.name;

  // Add shortcut if available
  if (!item.shortcut.isEmpty()) {
    displayText += QString("  [%1]").arg(item.shortcut);
  }

  auto* listItem = new QListWidgetItem(displayText, m_list);

  // Set tooltip
  if (!item.tooltip.isEmpty()) {
    listItem->setToolTip(item.tooltip);
  }

  // Set icon
  if (!item.iconName.isEmpty()) {
    auto& iconMgr = NMIconManager::instance();
    listItem->setIcon(iconMgr.getIcon(item.iconName, 16));
  }

  m_list->addItem(listItem);
}

// ============================================================================
// NMRecentItemsTracker
// ============================================================================

NMRecentItemsTracker& NMRecentItemsTracker::instance() {
  static NMRecentItemsTracker instance;
  return instance;
}

NMRecentItemsTracker::NMRecentItemsTracker() = default;

void NMRecentItemsTracker::recordPanelAccess(const QString& panelName) {
  addToMruList(m_recentPanels, panelName, 10);
}

void NMRecentItemsTracker::recordSceneAccess(const QString& sceneName) {
  addToMruList(m_recentScenes, sceneName, 10);
}

void NMRecentItemsTracker::recordScriptAccess(const QString& scriptPath) {
  addToMruList(m_recentScripts, scriptPath, 10);
}

QStringList NMRecentItemsTracker::getRecentPanels(int maxCount) const {
  return m_recentPanels.mid(0, qMin(maxCount, m_recentPanels.size()));
}

QStringList NMRecentItemsTracker::getRecentScenes(int maxCount) const {
  return m_recentScenes.mid(0, qMin(maxCount, m_recentScenes.size()));
}

QStringList NMRecentItemsTracker::getRecentScripts(int maxCount) const {
  return m_recentScripts.mid(0, qMin(maxCount, m_recentScripts.size()));
}

void NMRecentItemsTracker::clear() {
  m_recentPanels.clear();
  m_recentScenes.clear();
  m_recentScripts.clear();
}

void NMRecentItemsTracker::addToMruList(QStringList& list, const QString& item, int maxSize) {
  // Remove existing instance (to move to front)
  list.removeAll(item);

  // Add to front
  list.prepend(item);

  // Trim to max size
  while (list.size() > maxSize) {
    list.removeLast();
  }
}

} // namespace NovelMind::editor::qt
