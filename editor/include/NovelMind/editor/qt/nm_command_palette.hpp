#pragma once

/**
 * @file nm_command_palette.hpp
 * @brief Command palette for quick access to panels, commands, and recent items
 *
 * Provides a VS Code-style command palette that allows users to:
 * - Open panels by typing their names (Ctrl+P)
 * - Execute menu commands (Ctrl+Shift+P)
 * - Access recently used panels, scenes, and scripts
 * - Search with fuzzy matching
 */

#include <QAction>
#include <QDialog>
#include <QList>
#include <QListWidget>
#include <QString>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace NovelMind::editor::qt {

/**
 * @brief Type of command palette item
 */
enum class CommandItemType {
  Panel,       ///< Panel toggle action
  Command,     ///< Menu command/action
  Workspace,   ///< Workspace preset
  RecentScene, ///< Recently opened scene
  RecentScript ///< Recently edited script
};

/**
 * @brief Item in the command palette
 */
struct CommandItem {
  QString name;              ///< Display name
  QString searchableText;    ///< Text used for searching (includes metadata)
  QString tooltip;           ///< Tooltip/description
  QString shortcut;          ///< Keyboard shortcut (if any)
  QString iconName;          ///< Icon identifier
  CommandItemType type;      ///< Type of item
  QAction* action = nullptr; ///< Associated action (for panel/command items)
  int score = 0;             ///< Match score (for sorting)

  /**
   * @brief Get category label for this item type
   */
  QString categoryLabel() const;
};

/**
 * @brief Command palette dialog
 *
 * A frameless, centered dialog that provides quick access to:
 * - All panels (Scene View, Inspector, Console, etc.)
 * - All menu commands (New Project, Save, Undo, etc.)
 * - Workspace presets
 * - Recently opened scenes
 * - Recently edited scripts
 *
 * Features:
 * - Fuzzy search with scoring
 * - Keyboard navigation (Up/Down, Enter, Escape)
 * - Recent items shown first
 * - Category grouping
 * - Icon display
 * - Keyboard shortcut hints
 */
class NMCommandPalette : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Search mode for the palette
   */
  enum class Mode {
    Panels, ///< Show only panels (Ctrl+P)
    All     ///< Show all commands (Ctrl+Shift+P)
  };

  /**
   * @brief Construct command palette
   * @param parent Parent widget (usually main window)
   * @param actions List of all available actions
   * @param mode Search mode (panels only or all commands)
   */
  explicit NMCommandPalette(QWidget* parent, const QList<QAction*>& actions, Mode mode = Mode::All);

  /**
   * @brief Open the palette centered over an anchor widget
   * @param anchor Widget to center over (usually main window)
   */
  void openCentered(QWidget* anchor);

  /**
   * @brief Add a recent scene to the palette
   * @param sceneName Scene name/path
   */
  void addRecentScene(const QString& sceneName);

  /**
   * @brief Add a recent script to the palette
   * @param scriptPath Script file path
   */
  void addRecentScript(const QString& scriptPath);

  /**
   * @brief Clear all recent items
   */
  void clearRecentItems();

protected:
  /**
   * @brief Handle key presses for navigation
   */
  bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
  /**
   * @brief Handle search text changes
   */
  void onFilterChanged(const QString& text);

  /**
   * @brief Handle item activation (Enter key or double-click)
   */
  void onItemActivated(QListWidgetItem* item);

private:
  /**
   * @brief Build the command item list from actions
   */
  void buildCommandList();

  /**
   * @brief Populate the list widget with all items
   */
  void populateList();

  /**
   * @brief Update list widget based on search filter
   */
  void updateFilteredList(const QString& filter);

  /**
   * @brief Determine item type from action
   */
  CommandItemType determineItemType(QAction* action) const;

  /**
   * @brief Get icon name for action
   */
  QString getIconNameForAction(QAction* action) const;

  /**
   * @brief Add list widget item for a command
   */
  void addListItem(const CommandItem& item);

  Mode m_mode;                   ///< Current mode (panels/all)
  QList<CommandItem> m_commands; ///< All command items
  QList<QAction*> m_actions;     ///< All available actions
  QLineEdit* m_input = nullptr;  ///< Search input field
  QListWidget* m_list = nullptr; ///< Results list
};

/**
 * @brief Recent items tracker for command palette
 *
 * Singleton that tracks recently accessed panels, scenes, and scripts
 * for quick access via the command palette.
 */
class NMRecentItemsTracker {
public:
  static NMRecentItemsTracker& instance();

  /**
   * @brief Record that a panel was accessed
   */
  void recordPanelAccess(const QString& panelName);

  /**
   * @brief Record that a scene was opened
   */
  void recordSceneAccess(const QString& sceneName);

  /**
   * @brief Record that a script was edited
   */
  void recordScriptAccess(const QString& scriptPath);

  /**
   * @brief Get recently accessed panels (most recent first)
   */
  QStringList getRecentPanels(int maxCount = 5) const;

  /**
   * @brief Get recently opened scenes (most recent first)
   */
  QStringList getRecentScenes(int maxCount = 5) const;

  /**
   * @brief Get recently edited scripts (most recent first)
   */
  QStringList getRecentScripts(int maxCount = 5) const;

  /**
   * @brief Clear all recent items
   */
  void clear();

private:
  NMRecentItemsTracker();
  ~NMRecentItemsTracker() = default;

  NMRecentItemsTracker(const NMRecentItemsTracker&) = delete;
  NMRecentItemsTracker& operator=(const NMRecentItemsTracker&) = delete;

  /**
   * @brief Add item to MRU list (removes duplicates, keeps max size)
   */
  void addToMruList(QStringList& list, const QString& item, int maxSize = 10);

  QStringList m_recentPanels;
  QStringList m_recentScenes;
  QStringList m_recentScripts;
};

} // namespace NovelMind::editor::qt
