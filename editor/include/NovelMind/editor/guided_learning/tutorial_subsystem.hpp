#pragma once

/**
 * @file tutorial_subsystem.hpp
 * @brief Tutorial Subsystem - editor-only service entry point
 *
 * This is the main entry point for the Guided Learning System.
 * It manages initialization, lifecycle, and integration with the
 * rest of the editor.
 *
 * The subsystem:
 * - Registers with the editor at startup
 * - Listens to editor events for contextual triggers
 * - Manages the overlay widget
 * - Handles settings integration
 * - Coordinates all tutorial components
 *
 * IMPORTANT: This entire subsystem is EDITOR-ONLY.
 * It does not compile into runtime builds.
 */

// Include event_bus.hpp BEFORE Qt headers to avoid Qt's 'emit' macro
// interfering with EventBus::emit template method
#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/event_bus.hpp"
#include <QObject>
#include <memory>
#include <string>

// Forward declarations
class QWidget;

namespace NovelMind::editor::guided_learning {

// Forward declarations
class NMTutorialManager;
class NMAnchorRegistry;
class NMHelpOverlay;

/**
 * @brief Configuration for the tutorial subsystem
 */
struct TutorialSubsystemConfig {
  // Paths
  std::string tutorialDefinitionsPath = "tutorials";
  std::string userProgressPath; // Set to empty for default location

  // Default settings
  bool enabledByDefault = true;
  bool hintsEnabledByDefault = true;
  bool walkthroughsOnFirstRunByDefault = true;

  // Debug options
  bool verboseLogging = false;
};

/**
 * @brief Tutorial Subsystem - main service for guided learning
 *
 * This class owns and coordinates all components of the guided
 * learning system. It is created by the editor main window on
 * startup and destroyed on shutdown.
 */
class NMTutorialSubsystem : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static NMTutorialSubsystem& instance();

  /**
   * @brief Check if instance exists
   */
  static bool hasInstance();

  /**
   * @brief Initialize the subsystem
   * @param parentWidget The main window (for overlay attachment)
   * @param config Configuration options
   */
  Result<void> initialize(QWidget* parentWidget, const TutorialSubsystemConfig& config = {});

  /**
   * @brief Shutdown the subsystem
   */
  void shutdown();

  /**
   * @brief Check if subsystem is initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // ========================================================================
  // Component Access
  // ========================================================================

  /**
   * @brief Get the tutorial manager
   */
  [[nodiscard]] NMTutorialManager& tutorialManager();

  /**
   * @brief Get the anchor registry
   */
  [[nodiscard]] NMAnchorRegistry& anchorRegistry();

  /**
   * @brief Get the help overlay
   */
  [[nodiscard]] NMHelpOverlay* helpOverlay();

  // ========================================================================
  // Quick Access Methods (convenience wrappers)
  // ========================================================================

  /**
   * @brief Start a tutorial by ID
   */
  bool startTutorial(const std::string& tutorialId);

  /**
   * @brief Show a contextual hint
   */
  bool showHint(const std::string& hintId);

  /**
   * @brief Hide all active tutorials/hints
   */
  void hideAll();

  /**
   * @brief Check if guided learning is globally enabled
   */
  [[nodiscard]] bool isEnabled() const;

  /**
   * @brief Enable/disable guided learning globally
   */
  void setEnabled(bool enabled);

  // ========================================================================
  // Settings Integration
  // ========================================================================

  /**
   * @brief Register tutorial settings with the editor settings registry
   */
  void registerSettings();

  /**
   * @brief Apply settings from the editor settings registry
   */
  void applySettings();

  /**
   * @brief Save current state to user preferences
   */
  Result<void> saveUserPreferences();

  /**
   * @brief Load state from user preferences
   */
  Result<void> loadUserPreferences();

signals:
  /**
   * @brief Emitted when the subsystem is initialized
   */
  void initialized();

  /**
   * @brief Emitted when the subsystem is shutting down
   */
  void shuttingDown();

  /**
   * @brief Emitted when enabled state changes
   */
  void enabledChanged(bool enabled);

public:
  ~NMTutorialSubsystem() override;

private:
  NMTutorialSubsystem();

  // Non-copyable
  NMTutorialSubsystem(const NMTutorialSubsystem&) = delete;
  NMTutorialSubsystem& operator=(const NMTutorialSubsystem&) = delete;

  // Event bus integration
  void connectToEventBus();
  void disconnectFromEventBus();

  // Event handlers
  void onPanelFocusChanged(const PanelFocusChangedEvent& event);
  void onProjectOpened(const ProjectEvent& event);
  void onProjectClosed(const ProjectEvent& event);
  void onErrorOccurred(const ErrorEvent& event);

  // Components (owned by subsystem)
  std::unique_ptr<NMHelpOverlay> m_overlay;

  // Configuration
  TutorialSubsystemConfig m_config;

  // State
  bool m_initialized = false;

  // Event subscriptions
  std::vector<ScopedEventSubscription> m_eventSubscriptions;

  // Singleton instance
  static std::unique_ptr<NMTutorialSubsystem> s_instance;
};

/**
 * @brief Convenience macro for accessing the tutorial subsystem
 */
#define NM_TUTORIALS NMTutorialSubsystem::instance()

} // namespace NovelMind::editor::guided_learning
