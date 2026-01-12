#pragma once

/**
 * @file anchor_registry.hpp
 * @brief UI Anchor Registry for the Guided Learning System
 *
 * Provides a stable anchor system for attaching tutorial hints to UI elements.
 * Panels register their anchors with stable IDs, and the tutorial system uses
 * these IDs to position hints without directly querying the widget hierarchy.
 *
 * Key features:
 * - Stable anchor IDs that survive layout changes
 * - Safe weak references (QPointer) to widgets
 * - Automatic cleanup when widgets are destroyed
 * - DPI-aware positioning
 */

#include "NovelMind/core/types.hpp"
#include <QPointer>
#include <QRect>
#include <QWidget>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace NovelMind::editor::guided_learning {

/**
 * @brief Information about a registered anchor
 */
struct AnchorInfo {
  std::string id;          // Unique anchor ID
  std::string panelId;     // Parent panel ID
  std::string description; // Human-readable description

  // Position callback - called to get current position
  // This allows for dynamic positioning as layouts change
  std::function<QRect()> getRect;

  // Visibility callback
  std::function<bool()> isVisible;

  // Optional: associated widget (weak reference)
  QPointer<QWidget> widget;
};

/**
 * @brief Central registry for UI anchors
 *
 * Thread-safe singleton that manages all anchor registrations.
 * Panels register anchors on creation and unregister on destruction.
 */
class NMAnchorRegistry : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get singleton instance
   */
  static NMAnchorRegistry& instance();

  /**
   * @brief Register an anchor point
   * @param id Unique anchor ID (e.g., "timeline.addTrackButton")
   * @param widget The widget to anchor to
   * @param description Human-readable description
   * @param panelId Parent panel ID
   */
  void registerAnchor(const std::string& id, QWidget* widget, const std::string& description = "",
                      const std::string& panelId = "");

  /**
   * @brief Register an anchor with custom rect provider
   * @param id Unique anchor ID
   * @param rectProvider Function returning current global rect
   * @param visibilityProvider Function returning visibility state
   * @param description Human-readable description
   * @param panelId Parent panel ID
   */
  void registerAnchor(const std::string& id, std::function<QRect()> rectProvider,
                      std::function<bool()> visibilityProvider, const std::string& description = "",
                      const std::string& panelId = "");

  /**
   * @brief Unregister an anchor
   */
  void unregisterAnchor(const std::string& id);

  /**
   * @brief Unregister all anchors for a panel
   */
  void unregisterPanelAnchors(const std::string& panelId);

  /**
   * @brief Check if an anchor exists
   */
  [[nodiscard]] bool hasAnchor(const std::string& id) const;

  /**
   * @brief Get anchor info
   */
  [[nodiscard]] std::optional<AnchorInfo> getAnchor(const std::string& id) const;

  /**
   * @brief Get global rect for an anchor
   * Returns nullopt if anchor doesn't exist or widget was destroyed
   */
  [[nodiscard]] std::optional<QRect> getAnchorRect(const std::string& id) const;

  /**
   * @brief Check if anchor is currently visible
   */
  [[nodiscard]] bool isAnchorVisible(const std::string& id) const;

  /**
   * @brief Get all anchor IDs for a panel
   */
  [[nodiscard]] std::vector<std::string> getAnchorsForPanel(const std::string& panelId) const;

  /**
   * @brief Get all registered anchor IDs
   */
  [[nodiscard]] std::vector<std::string> getAllAnchorIds() const;

  /**
   * @brief Debug: dump all anchors to console
   */
  void debugDumpAnchors() const;

signals:
  /**
   * @brief Emitted when an anchor is registered
   */
  void anchorRegistered(const QString& anchorId);

  /**
   * @brief Emitted when an anchor is unregistered
   */
  void anchorUnregistered(const QString& anchorId);

  /**
   * @brief Emitted when an anchor becomes visible
   */
  void anchorBecameVisible(const QString& anchorId);

  /**
   * @brief Emitted when an anchor becomes hidden
   */
  void anchorBecameHidden(const QString& anchorId);

private:
  NMAnchorRegistry();
  ~NMAnchorRegistry() override = default;

  // Non-copyable
  NMAnchorRegistry(const NMAnchorRegistry&) = delete;
  NMAnchorRegistry& operator=(const NMAnchorRegistry&) = delete;

  // Clean up destroyed widgets
  void cleanupDestroyedWidgets();

  std::unordered_map<std::string, AnchorInfo> m_anchors;
  mutable std::mutex m_mutex;
};

/**
 * @brief RAII helper for anchor registration
 *
 * Use this in panel constructors to automatically register/unregister anchors.
 */
class ScopedAnchorRegistration {
public:
  ScopedAnchorRegistration(const std::string& anchorId, QWidget* widget,
                           const std::string& description = "", const std::string& panelId = "");

  ~ScopedAnchorRegistration();

  // Move-only
  ScopedAnchorRegistration(ScopedAnchorRegistration&& other) noexcept;
  ScopedAnchorRegistration& operator=(ScopedAnchorRegistration&& other) noexcept;

  ScopedAnchorRegistration(const ScopedAnchorRegistration&) = delete;
  ScopedAnchorRegistration& operator=(const ScopedAnchorRegistration&) = delete;

private:
  std::string m_anchorId;
  bool m_registered = false;
};

/**
 * @brief Convenience macro for registering anchors
 *
 * Usage: NM_REGISTER_ANCHOR(myButton, "panel.myButton", "My Button", "myPanel")
 */
#define NM_REGISTER_ANCHOR(widget, id, description, panelId)                                       \
  NMAnchorRegistry::instance().registerAnchor(id, widget, description, panelId)

/**
 * @brief Convenience macro for creating scoped anchor registration
 */
#define NM_SCOPED_ANCHOR(varName, widget, id, description, panelId)                                \
  ScopedAnchorRegistration varName(id, widget, description, panelId)

} // namespace NovelMind::editor::guided_learning
