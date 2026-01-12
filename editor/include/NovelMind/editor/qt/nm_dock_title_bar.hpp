#pragma once

/**
 * @file nm_dock_title_bar.hpp
 * @brief Custom title bar widget for dock panels
 *
 * Provides a compact, styled title bar for NMDockPanel with:
 * - Reduced height (20-22px) for more screen real estate
 * - Consistent dark theme styling
 * - Active/inactive visual feedback
 * - Additional controls (pin, settings, minimize)
 * - Panel-specific icons
 */

#include <QWidget>
#include <QString>
#include <QIcon>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>

namespace NovelMind::editor::qt {

// Forward declaration
class NMDockPanel;

/**
 * @brief Compact custom title bar for dock panels
 *
 * This widget replaces Qt's default dock widget title bar with a custom
 * implementation that provides:
 * - Compact height (configurable, default 22px)
 * - Dark theme styling with active/inactive states
 * - Panel icon display
 * - Standard controls (float, close)
 * - Additional controls (pin, settings, minimize)
 * - Double-click to float/dock
 * - Drag to reposition
 */
class NMDockTitleBar : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a custom title bar
   * @param parent The dock panel this title bar belongs to
   */
  explicit NMDockTitleBar(QWidget* parent = nullptr);

  /**
   * @brief Destructor
   */
  ~NMDockTitleBar() override = default;

  /**
   * @brief Set the title text
   */
  void setTitle(const QString& title);

  /**
   * @brief Get the title text
   */
  [[nodiscard]] QString title() const { return m_title; }

  /**
   * @brief Set the panel icon
   */
  void setIcon(const QIcon& icon);

  /**
   * @brief Set whether panel is pinned (locked in place)
   */
  void setPinned(bool pinned);

  /**
   * @brief Get pinned state
   */
  [[nodiscard]] bool isPinned() const { return m_pinned; }

  /**
   * @brief Set whether panel is minimized (collapsed to title bar)
   */
  void setMinimized(bool minimized);

  /**
   * @brief Get minimized state
   */
  [[nodiscard]] bool isMinimized() const { return m_minimized; }

  /**
   * @brief Set whether to show settings button
   */
  void setShowSettings(bool show);

  /**
   * @brief Set active state (focused panel)
   */
  void setActive(bool active);

  /**
   * @brief Get active state
   */
  [[nodiscard]] bool isActive() const { return m_active; }

  /**
   * @brief Set title bar height (compact mode)
   * @param height Height in pixels (default: 22)
   */
  void setTitleBarHeight(int height);

  /**
   * @brief Get title bar height
   */
  [[nodiscard]] int titleBarHeight() const { return m_titleBarHeight; }

signals:
  /**
   * @brief Emitted when float button clicked
   */
  void floatClicked();

  /**
   * @brief Emitted when close button clicked
   */
  void closeClicked();

  /**
   * @brief Emitted when pin button clicked
   */
  void pinClicked(bool pinned);

  /**
   * @brief Emitted when minimize button clicked
   */
  void minimizeClicked(bool minimized);

  /**
   * @brief Emitted when settings button clicked
   */
  void settingsClicked();

  /**
   * @brief Emitted when title bar is double-clicked
   */
  void titleDoubleClicked();

protected:
  // Qt event overrides for drag support
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private:
  void setupUi();
  void updateStyle();
  void updateButtonVisibility();
  void createButton(QPushButton*& button, const QString& iconName, const QString& tooltip);

  // UI Components
  QHBoxLayout* m_layout = nullptr;
  QLabel* m_iconLabel = nullptr;
  QLabel* m_titleLabel = nullptr;
  QPushButton* m_pinButton = nullptr;
  QPushButton* m_settingsButton = nullptr;
  QPushButton* m_minimizeButton = nullptr;
  QPushButton* m_floatButton = nullptr;
  QPushButton* m_closeButton = nullptr;

  // State
  QString m_title;
  QIcon m_icon;
  bool m_pinned = false;
  bool m_minimized = false;
  bool m_active = false;
  bool m_showSettings = false;
  int m_titleBarHeight = 22;

  // Drag support
  QPoint m_dragStartPos;
  bool m_dragging = false;
};

} // namespace NovelMind::editor::qt
