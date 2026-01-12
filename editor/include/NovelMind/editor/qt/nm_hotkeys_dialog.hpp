#pragma once

/**
 * @file nm_hotkeys_dialog.hpp
 * @brief Modal dialog for hotkey configuration with conflict detection.
 *
 * Provides:
 * - List of all configurable hotkeys
 * - Keyboard shortcut customization
 * - Conflict detection and resolution
 * - Import/export hotkey profiles
 * - Reset to defaults
 */

#include <QDialog>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QString>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;

namespace NovelMind::editor::qt {

/**
 * @brief Hotkey entry with editability
 */
struct NMHotkeyEntry {
  QString id;              // Unique action ID
  QString section;         // Category (e.g., "File", "Edit", "View")
  QString action;          // Action name
  QString shortcut;        // Current shortcut
  QString defaultShortcut; // Default shortcut
  QString notes;
  bool isCustomizable = true;
  bool isModified = false;
};

/**
 * @brief Conflict information when shortcuts clash
 */
struct NMHotkeyConflict {
  QString actionId1;
  QString actionId2;
  QString shortcut;
  QString action1Name;
  QString action2Name;
};

class NMHotkeysDialog final : public QDialog {
  Q_OBJECT

public:
  explicit NMHotkeysDialog(const QList<NMHotkeyEntry>& entries, QWidget* parent = nullptr);

  /**
   * @brief Get modified hotkeys
   */
  [[nodiscard]] QList<NMHotkeyEntry> getModifiedEntries() const;

  /**
   * @brief Check for conflicts
   */
  [[nodiscard]] QList<NMHotkeyConflict> detectConflicts() const;

  /**
   * @brief Export hotkeys to file
   */
  bool exportToFile(const QString& filePath) const;

  /**
   * @brief Import hotkeys from file
   */
  bool importFromFile(const QString& filePath);

signals:
  void hotkeyChanged(const QString& actionId, const QString& newShortcut);
  void conflictDetected(const NMHotkeyConflict& conflict);

private slots:
  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onRecordShortcut();
  void onResetToDefault();
  void onResetAllToDefaults();
  void onExportClicked();
  void onImportClicked();
  void onApplyClicked();

private:
  void buildUi(const QList<NMHotkeyEntry>& entries);
  void applyFilter(const QString& text);
  void updateConflictWarnings();
  void highlightConflicts();
  void setShortcutForItem(QTreeWidgetItem* item, const QString& shortcut);
  QString recordKeySequence();

  QLineEdit* m_filterEdit = nullptr;
  QTreeWidget* m_tree = nullptr;
  QPushButton* m_recordBtn = nullptr;
  QPushButton* m_resetBtn = nullptr;
  QPushButton* m_resetAllBtn = nullptr;
  QPushButton* m_exportBtn = nullptr;
  QPushButton* m_importBtn = nullptr;
  QPushButton* m_applyBtn = nullptr;
  QLabel* m_conflictLabel = nullptr;

  QHash<QString, NMHotkeyEntry> m_entries;
  QHash<QString, QTreeWidgetItem*> m_itemLookup;
  QTreeWidgetItem* m_recordingItem = nullptr;
  bool m_isRecording = false;
};

} // namespace NovelMind::editor::qt
