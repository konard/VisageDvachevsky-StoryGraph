#pragma once

/**
 * @file tutorial_manager.hpp
 * @brief Tutorial Manager - registry and lifecycle management
 *
 * Manages the registry of all tutorials and contextual hints,
 * handles loading from JSON/YAML definitions, and coordinates
 * tutorial lifecycle (start, step, complete, skip).
 *
 * Key responsibilities:
 * - Load tutorial definitions from files
 * - Track user progress
 * - Coordinate with overlay for display
 * - Persist progress between sessions
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/guided_learning/tutorial_types.hpp"
#include <QObject>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor::guided_learning {

// Forward declarations
class NMHelpOverlay;

/**
 * @brief Callback for custom step conditions
 */
using CustomConditionCallback = std::function<bool()>;

/**
 * @brief Tutorial Manager - central controller for guided learning
 *
 * This is the main API for the tutorial system. Other components
 * interact with this manager to:
 * - Register/query tutorials
 * - Start/stop tutorials
 * - Track progress
 * - Show contextual hints
 */
class NMTutorialManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get singleton instance
   */
  static NMTutorialManager& instance();

  /**
   * @brief Initialize the tutorial system
   * @param overlay Pointer to the overlay widget
   */
  void initialize(NMHelpOverlay* overlay);

  /**
   * @brief Shutdown and cleanup
   */
  void shutdown();

  /**
   * @brief Check if system is initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // ========================================================================
  // Tutorial Definition Management
  // ========================================================================

  /**
   * @brief Load tutorials from a directory
   * Loads all .json files in the directory as tutorial definitions
   */
  Result<u32> loadTutorialsFromDirectory(const std::string& directory);

  /**
   * @brief Load a single tutorial from JSON file
   */
  Result<void> loadTutorialFromFile(const std::string& filePath);

  /**
   * @brief Load a tutorial from JSON string
   */
  Result<void> loadTutorialFromJson(const std::string& jsonContent);

  /**
   * @brief Register a tutorial definition programmatically
   */
  void registerTutorial(const TutorialDefinition& tutorial);

  /**
   * @brief Unregister a tutorial
   */
  void unregisterTutorial(const std::string& tutorialId);

  /**
   * @brief Get a tutorial definition
   */
  [[nodiscard]] std::optional<TutorialDefinition> getTutorial(const std::string& tutorialId) const;

  /**
   * @brief Get all registered tutorials
   */
  [[nodiscard]] std::vector<TutorialDefinition> getAllTutorials() const;

  /**
   * @brief Get tutorials by category
   */
  [[nodiscard]] std::vector<TutorialDefinition>
  getTutorialsByCategory(const std::string& category) const;

  /**
   * @brief Get tutorials by level
   */
  [[nodiscard]] std::vector<TutorialDefinition> getTutorialsByLevel(TutorialLevel level) const;

  /**
   * @brief Search tutorials by text
   */
  [[nodiscard]] std::vector<TutorialDefinition> searchTutorials(const std::string& query) const;

  // ========================================================================
  // Contextual Hints Management
  // ========================================================================

  /**
   * @brief Register a contextual hint
   */
  void registerHint(const ContextualHint& hint);

  /**
   * @brief Unregister a contextual hint
   */
  void unregisterHint(const std::string& hintId);

  /**
   * @brief Get a contextual hint definition
   */
  [[nodiscard]] std::optional<ContextualHint> getHint(const std::string& hintId) const;

  /**
   * @brief Get all registered hints
   */
  [[nodiscard]] std::vector<ContextualHint> getAllHints() const;

  // ========================================================================
  // Tutorial Lifecycle
  // ========================================================================

  /**
   * @brief Start a tutorial walkthrough
   * @return false if tutorial doesn't exist or prerequisites not met
   */
  bool startTutorial(const std::string& tutorialId);

  /**
   * @brief Stop the currently active tutorial
   * @param markComplete If true, mark as completed; otherwise preserve progress
   */
  void stopTutorial(bool markComplete = false);

  /**
   * @brief Advance to the next step
   */
  void nextStep();

  /**
   * @brief Go back to the previous step
   */
  void previousStep();

  /**
   * @brief Skip the current step
   */
  void skipStep();

  /**
   * @brief Skip all remaining steps and complete
   */
  void skipAll();

  /**
   * @brief Check if a tutorial is currently active
   */
  [[nodiscard]] bool isTutorialActive() const { return m_activeTutorial.has_value(); }

  /**
   * @brief Get the currently active tutorial ID
   */
  [[nodiscard]] std::optional<std::string> getActiveTutorialId() const;

  /**
   * @brief Get the current step index (0-based)
   */
  [[nodiscard]] u32 getCurrentStepIndex() const { return m_currentStepIndex; }

  /**
   * @brief Get the current step
   */
  [[nodiscard]] std::optional<TutorialStep> getCurrentStep() const;

  // ========================================================================
  // Contextual Hint Display
  // ========================================================================

  /**
   * @brief Show a contextual hint
   * Respects max show count and disabled state
   * @return false if hint was suppressed
   */
  bool showHint(const std::string& hintId);

  /**
   * @brief Hide a currently shown hint
   */
  void hideHint(const std::string& hintId);

  /**
   * @brief Hide all currently shown hints
   */
  void hideAllHints();

  /**
   * @brief Check if a hint is currently visible
   */
  [[nodiscard]] bool isHintVisible(const std::string& hintId) const;

  // ========================================================================
  // Progress Management
  // ========================================================================

  /**
   * @brief Get progress for a tutorial
   */
  [[nodiscard]] TutorialProgress getTutorialProgress(const std::string& tutorialId) const;

  /**
   * @brief Get progress for a hint
   */
  [[nodiscard]] HintProgress getHintProgress(const std::string& hintId) const;

  /**
   * @brief Reset progress for a tutorial
   */
  void resetTutorialProgress(const std::string& tutorialId);

  /**
   * @brief Reset progress for a hint
   */
  void resetHintProgress(const std::string& hintId);

  /**
   * @brief Reset all progress
   */
  void resetAllProgress();

  /**
   * @brief Disable a tutorial (user choice)
   */
  void disableTutorial(const std::string& tutorialId);

  /**
   * @brief Enable a previously disabled tutorial
   */
  void enableTutorial(const std::string& tutorialId);

  /**
   * @brief Disable a hint (user choice)
   */
  void disableHint(const std::string& hintId);

  /**
   * @brief Enable a previously disabled hint
   */
  void enableHint(const std::string& hintId);

  /**
   * @brief Check if a tutorial is completed
   */
  [[nodiscard]] bool isTutorialCompleted(const std::string& tutorialId) const;

  /**
   * @brief Check if a tutorial is disabled
   */
  [[nodiscard]] bool isTutorialDisabled(const std::string& tutorialId) const;

  /**
   * @brief Check if all prerequisites for a tutorial are met
   */
  [[nodiscard]] bool arePrerequisitesMet(const std::string& tutorialId) const;

  // ========================================================================
  // Persistence
  // ========================================================================

  /**
   * @brief Load progress from file
   */
  Result<void> loadProgress(const std::string& filePath);

  /**
   * @brief Save progress to file
   */
  Result<void> saveProgress(const std::string& filePath) const;

  /**
   * @brief Set the progress file path (for auto-save)
   */
  void setProgressFilePath(const std::string& filePath);

  /**
   * @brief Auto-save progress (called periodically)
   */
  void autoSaveProgress();

  // ========================================================================
  // Global Settings
  // ========================================================================

  /**
   * @brief Check if guided learning is globally enabled
   */
  [[nodiscard]] bool isEnabled() const { return !m_progress.globallyDisabled; }

  /**
   * @brief Enable/disable guided learning globally
   */
  void setEnabled(bool enabled);

  /**
   * @brief Check if contextual hints are enabled
   */
  [[nodiscard]] bool areHintsEnabled() const { return m_progress.hintsEnabled; }

  /**
   * @brief Enable/disable contextual hints
   */
  void setHintsEnabled(bool enabled);

  /**
   * @brief Check if walkthroughs on first run are enabled
   */
  [[nodiscard]] bool areWalkthroughsOnFirstRunEnabled() const {
    return m_progress.walkthroughsOnFirstRun;
  }

  /**
   * @brief Enable/disable walkthroughs on first run
   */
  void setWalkthroughsOnFirstRunEnabled(bool enabled);

  // ========================================================================
  // Custom Conditions
  // ========================================================================

  /**
   * @brief Register a custom condition evaluator
   */
  void registerCustomCondition(const std::string& conditionId, CustomConditionCallback callback);

  /**
   * @brief Unregister a custom condition evaluator
   */
  void unregisterCustomCondition(const std::string& conditionId);

  // ========================================================================
  // Event Integration
  // ========================================================================

  /**
   * @brief Notify that a panel was opened (for contextual triggers)
   */
  void onPanelOpened(const std::string& panelId);

  /**
   * @brief Notify that a panel entered empty state
   */
  void onPanelEmptyState(const std::string& panelId, bool isEmpty);

  /**
   * @brief Notify that an error occurred
   */
  void onErrorOccurred(const std::string& errorCode, const std::string& context);

  /**
   * @brief Notify that a feature version was encountered
   */
  void onFeatureVersionEncountered(const std::string& featureId, const std::string& version);

signals:
  /**
   * @brief Emitted when a tutorial starts
   */
  void tutorialStarted(const QString& tutorialId);

  /**
   * @brief Emitted when a tutorial step changes
   */
  void tutorialStepChanged(const QString& tutorialId, int stepIndex);

  /**
   * @brief Emitted when a tutorial completes
   */
  void tutorialCompleted(const QString& tutorialId);

  /**
   * @brief Emitted when a tutorial is stopped (without completing)
   */
  void tutorialStopped(const QString& tutorialId);

  /**
   * @brief Emitted when a hint is shown
   */
  void hintShown(const QString& hintId);

  /**
   * @brief Emitted when a hint is hidden
   */
  void hintHidden(const QString& hintId);

  /**
   * @brief Emitted when progress changes
   */
  void progressChanged();

  /**
   * @brief Emitted when global settings change
   */
  void settingsChanged();

private:
  NMTutorialManager();
  ~NMTutorialManager() override = default;

  // Non-copyable
  NMTutorialManager(const NMTutorialManager&) = delete;
  NMTutorialManager& operator=(const NMTutorialManager&) = delete;

  // Internal helpers
  void displayCurrentStep();
  void hideCurrentStep();
  void updateStepProgress();
  bool evaluateCondition(const StepCondition& condition);
  void connectToEventBus();

  // Tutorial definitions (id -> definition)
  std::unordered_map<std::string, TutorialDefinition> m_tutorials;

  // Contextual hint definitions (id -> definition)
  std::unordered_map<std::string, ContextualHint> m_hints;

  // User progress
  GuidedLearningProgress m_progress;

  // Active tutorial state
  std::optional<TutorialDefinition> m_activeTutorial;
  u32 m_currentStepIndex = 0;

  // Currently visible hints
  std::unordered_set<std::string> m_visibleHints;

  // Custom condition callbacks
  std::unordered_map<std::string, CustomConditionCallback> m_customConditions;

  // Overlay reference (not owned)
  NMHelpOverlay* m_overlay = nullptr;

  // State
  bool m_initialized = false;
  std::string m_progressFilePath;
};

} // namespace NovelMind::editor::guided_learning
