#pragma once

/**
 * @file inspector_ui.hpp
 * @brief UI layout and widget management for inspector panel
 *
 * Handles UI setup, layout management, and widget creation
 */

#include <QList>
#include <QString>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QWidget;

namespace NovelMind::editor::qt {

class NMPropertyGroup;
class NMInspectorPanel;

/**
 * @brief Manages UI layout and widget creation
 */
class InspectorUI {
public:
  explicit InspectorUI(NMInspectorPanel* panel);

  /**
   * @brief Setup the main content widget structure
   */
  QWidget* setupContent(QWidget* parent);

  /**
   * @brief Add a property group to the layout
   */
  NMPropertyGroup* addGroup(const QString& title, QWidget* scrollContent, QVBoxLayout* mainLayout,
                            QList<NMPropertyGroup*>& groups);

  /**
   * @brief Show "no selection" message
   */
  void showNoSelection(QLabel* headerLabel, QLabel* noSelectionLabel);

  /**
   * @brief Clear all property groups
   */
  void clearGroups(QVBoxLayout* mainLayout, QList<NMPropertyGroup*>& groups, QLabel* headerLabel);

  /**
   * @brief Set header text for inspected object
   */
  void setHeader(QLabel* headerLabel, const QString& text);

  /**
   * @brief Get main layout
   */
  QVBoxLayout* getMainLayout() const { return m_mainLayout; }

  /**
   * @brief Get scroll content widget
   */
  QWidget* getScrollContent() const { return m_scrollContent; }

  /**
   * @brief Get header label
   */
  QLabel* getHeaderLabel() const { return m_headerLabel; }

  /**
   * @brief Get no selection label
   */
  QLabel* getNoSelectionLabel() const { return m_noSelectionLabel; }

private:
  NMInspectorPanel* m_panel = nullptr;

  // UI widgets (owned by Qt parent-child system)
  QScrollArea* m_scrollArea = nullptr;
  QWidget* m_scrollContent = nullptr;
  QVBoxLayout* m_mainLayout = nullptr;
  QLabel* m_headerLabel = nullptr;
  QLabel* m_noSelectionLabel = nullptr;
};

} // namespace NovelMind::editor::qt
