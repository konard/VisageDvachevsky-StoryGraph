#pragma once

/**
 * @file nm_scrollable_toolbar.hpp
 * @brief Scrollable toolbar container for adaptive UI layouts
 *
 * Provides a wrapper that enables horizontal scrolling for toolbars
 * when panel width becomes too small to display all controls.
 * This addresses issue #69: UI panels not adapting to window size.
 */

#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace NovelMind::editor::qt {

/**
 * @brief A scrollable container for QToolBar that enables horizontal scrolling
 *
 * When a panel is resized to be smaller than the toolbar's natural width,
 * this container allows users to scroll horizontally to access all toolbar
 * buttons and controls, ensuring key UI elements are always accessible.
 *
 * Usage:
 * @code
 * auto *scrollableToolbar = new NMScrollableToolBar(this);
 * auto *toolbar = scrollableToolbar->toolbar();
 * toolbar->addAction("Action1");
 * toolbar->addAction("Action2");
 * layout->addWidget(scrollableToolbar);
 * @endcode
 */
class NMScrollableToolBar : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a scrollable toolbar container
   * @param parent Parent widget
   */
  explicit NMScrollableToolBar(QWidget* parent = nullptr);

  /**
   * @brief Get the embedded toolbar for adding actions/widgets
   * @return Pointer to the internal QToolBar
   */
  [[nodiscard]] QToolBar* toolbar() const { return m_toolbar; }

  /**
   * @brief Set the object name for the internal toolbar
   * @param name Object name for styling
   */
  void setToolBarObjectName(const QString& name);

  /**
   * @brief Set the icon size for toolbar buttons
   * @param size Icon size
   */
  void setIconSize(const QSize& size);

  /**
   * @brief Check if scrolling is currently needed
   * @return true if content is wider than visible area
   */
  [[nodiscard]] bool isScrollingNeeded() const;

signals:
  /**
   * @brief Emitted when scroll visibility changes
   * @param visible true if scroll indicators should be shown
   */
  void scrollVisibilityChanged(bool visible);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void updateScrollIndicators();

  QScrollArea* m_scrollArea = nullptr;
  QToolBar* m_toolbar = nullptr;
  bool m_scrollNeeded = false;
};

/**
 * @brief A collapsible panel widget that can show/hide its content
 *
 * Used for panels like NMNodePalette to allow users to collapse
 * the panel to save space when the window is small.
 */
class NMCollapsiblePanel : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a collapsible panel
   * @param title Panel title shown in the header
   * @param parent Parent widget
   */
  explicit NMCollapsiblePanel(const QString& title, QWidget* parent = nullptr);

  /**
   * @brief Set the content widget
   * @param widget Content widget to show/hide
   */
  void setContentWidget(QWidget* widget);

  /**
   * @brief Get the content widget
   * @return Content widget or nullptr
   */
  [[nodiscard]] QWidget* contentWidget() const { return m_contentWidget; }

  /**
   * @brief Check if panel is collapsed
   * @return true if collapsed
   */
  [[nodiscard]] bool isCollapsed() const { return m_collapsed; }

  /**
   * @brief Set collapsed state
   * @param collapsed true to collapse
   */
  void setCollapsed(bool collapsed);

  /**
   * @brief Toggle collapsed state
   */
  void toggle();

signals:
  /**
   * @brief Emitted when collapsed state changes
   * @param collapsed New collapsed state
   */
  void collapsedChanged(bool collapsed);

private:
  void updateVisibility();

  QWidget* m_headerWidget = nullptr;
  QWidget* m_contentWidget = nullptr;
  QWidget* m_contentContainer = nullptr;
  QPushButton* m_toggleButton = nullptr;
  bool m_collapsed = false;
};

/**
 * @brief A scrollable container for vertical content
 *
 * Wraps content in a QScrollArea with vertical scrolling enabled,
 * useful for panels with many controls that may not fit in small windows.
 */
class NMScrollableContent : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a scrollable content container
   * @param parent Parent widget
   */
  explicit NMScrollableContent(QWidget* parent = nullptr);

  /**
   * @brief Set the content widget
   * @param widget Content widget to scroll
   */
  void setContentWidget(QWidget* widget);

  /**
   * @brief Get the scroll area
   * @return Pointer to the internal QScrollArea
   */
  [[nodiscard]] QScrollArea* scrollArea() const { return m_scrollArea; }

private:
  QScrollArea* m_scrollArea = nullptr;
};

} // namespace NovelMind::editor::qt
