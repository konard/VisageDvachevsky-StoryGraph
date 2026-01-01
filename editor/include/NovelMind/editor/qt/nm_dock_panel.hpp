#pragma once

/**
 * @file nm_dock_panel.hpp
 * @brief Base class for all dockable panels in the NovelMind Editor
 *
 * Provides a consistent interface and common functionality for all editor
 * panels. Each panel inherits from this base class to ensure uniform behavior
 * for:
 * - Docking and floating
 * - Title and icon management
 * - Visibility toggle
 * - Focus tracking
 */

#include <QDockWidget>
#include <QIcon>
#include <QSize>
#include <QString>
#include <memory>
#include <string>

// Forward declaration for guided learning integration
namespace NovelMind::editor::guided_learning {
class ScopedAnchorRegistration;
}

namespace NovelMind::editor::qt {

/**
 * @brief Base class for all dockable editor panels
 *
 * This class wraps QDockWidget and provides additional functionality
 * specific to the NovelMind Editor.
 */
class NMDockPanel : public QDockWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a new dock panel
   * @param title The panel title shown in the title bar
   * @param parent Parent widget (usually the main window)
   */
  explicit NMDockPanel(const QString &title, QWidget *parent = nullptr);

  /**
   * @brief Virtual destructor
   */
  ~NMDockPanel() override;

  /**
   * @brief Get the panel's unique identifier
   */
  [[nodiscard]] QString panelId() const { return m_panelId; }

  /**
   * @brief Set the panel's unique identifier
   *
   * This also registers the panel with the guided learning anchor registry,
   * allowing the tutorial system to show hints attached to this panel.
   */
  void setPanelId(const QString &id);

  /**
   * @brief Called when the panel should update its contents
   * @param deltaTime Time since last update in seconds
   */
  virtual void onUpdate(double deltaTime);

  /**
   * @brief Called when the panel is first shown
   */
  virtual void onInitialize();

  /**
   * @brief Called when the panel is about to be destroyed
   */
  virtual void onShutdown();

  /**
   * @brief Called when the panel gains focus
   */
  virtual void onFocusGained();

  /**
   * @brief Called when the panel loses focus
   */
  virtual void onFocusLost();

  /**
   * @brief Called when the panel is resized
   */
  virtual void onResize(int width, int height);

  /**
   * @brief Set the minimum size for this panel
   * @param width Minimum width in pixels
   * @param height Minimum height in pixels
   *
   * This helps prevent UI element overlap when panels are docked.
   * Qt's docking system respects these minimum sizes.
   */
  void setMinimumPanelSize(int width, int height);

  /**
   * @brief Set the minimum size for this panel
   * @param size Minimum size
   */
  void setMinimumPanelSize(const QSize &size);

  /**
   * @brief Get the default minimum size for panels
   * @return Default minimum size (200x150)
   */
  static QSize defaultMinimumSize() { return QSize(200, 150); }

signals:
  /**
   * @brief Emitted when the panel gains focus
   */
  void focusGained();

  /**
   * @brief Emitted when the panel loses focus
   */
  void focusLost();

  /**
   * @brief Emitted when the panel's title changes
   */
  void titleChanged(const QString &newTitle);

protected:
  /**
   * @brief Set the main content widget for this panel
   * @param widget The content widget
   */
  void setContentWidget(QWidget *widget);

  /**
   * @brief Get the content widget
   */
  [[nodiscard]] QWidget *contentWidget() const { return m_contentWidget; }

  // Qt event overrides
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

  /**
   * @brief Register a UI element as an anchor for the tutorial system
   * @param elementId Local element ID (e.g., "canvas", "toolbar")
   * @param widget The widget to register
   * @param description Human-readable description of the element
   *
   * The full anchor ID will be "{panelId}.{elementId}".
   * Use this to make specific UI elements targetable by tutorials.
   */
  void registerAnchor(const std::string &elementId, QWidget *widget,
                      const std::string &description = "");

private:
  QString m_panelId;
  QWidget *m_contentWidget = nullptr;
  bool m_initialized = false;
  bool m_inShowEvent = false; // Re-entrance guard for showEvent

  // Anchor registration for guided learning system
  std::unique_ptr<guided_learning::ScopedAnchorRegistration> m_panelAnchor;
  std::vector<std::unique_ptr<guided_learning::ScopedAnchorRegistration>>
      m_elementAnchors;
};

} // namespace NovelMind::editor::qt
