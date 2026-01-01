#pragma once

/**
 * @file qt_selection_manager.hpp
 * @brief Qt-based Selection Manager for the editor
 *
 * Provides centralized selection management with Qt signals for
 * synchronization across panels.
 */

#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace NovelMind::editor::qt {

/**
 * @brief Selection type enumeration
 */
enum class SelectionType {
  None,
  SceneObject,
  GraphNode,
  TimelineItem,
  Asset,
  HierarchyItem
};

/**
 * @brief Qt Selection Manager singleton
 *
 * Manages selection state across all editor panels and notifies
 * listeners when selection changes.
 */
class QtSelectionManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static QtSelectionManager &instance();

  // =========================================================================
  // Selection Operations
  // =========================================================================

  /**
   * @brief Select a single item (clears previous selection)
   */
  void select(const QString &id, SelectionType type);

  /**
   * @brief Select multiple items
   */
  void selectMultiple(const QStringList &ids, SelectionType type);

  /**
   * @brief Add to current selection
   */
  void addToSelection(const QString &id, SelectionType type);

  /**
   * @brief Remove from current selection
   */
  void removeFromSelection(const QString &id);

  /**
   * @brief Toggle selection of an item
   */
  void toggleSelection(const QString &id, SelectionType type);

  /**
   * @brief Clear all selection
   */
  void clearSelection();

  // =========================================================================
  // Selection Queries
  // =========================================================================

  /**
   * @brief Check if anything is selected
   */
  [[nodiscard]] bool hasSelection() const;

  /**
   * @brief Get current selection type
   */
  [[nodiscard]] SelectionType currentSelectionType() const;

  /**
   * @brief Get all selected IDs
   */
  [[nodiscard]] QStringList selectedIds() const;

  /**
   * @brief Get the primary (first) selection
   */
  [[nodiscard]] QString primarySelection() const;

  /**
   * @brief Get selection count
   */
  [[nodiscard]] int selectionCount() const;

  /**
   * @brief Check if a specific item is selected
   */
  [[nodiscard]] bool isSelected(const QString &id) const;

signals:
  /**
   * @brief Emitted when selection changes
   */
  void selectionChanged(const QStringList &selectedIds, SelectionType type);

  /**
   * @brief Emitted when selection is cleared
   */
  void selectionCleared();

  /**
   * @brief Emitted when primary selection changes
   */
  void primarySelectionChanged(const QString &id, SelectionType type);

private:
  QtSelectionManager();
  ~QtSelectionManager() override = default;

  // Prevent copying
  QtSelectionManager(const QtSelectionManager &) = delete;
  QtSelectionManager &operator=(const QtSelectionManager &) = delete;

  void notifySelectionChanged();

  mutable QMutex m_mutex;  // Protects selection state
  QStringList m_selectedIds;
  SelectionType m_currentType = SelectionType::None;
};

} // namespace NovelMind::editor::qt
