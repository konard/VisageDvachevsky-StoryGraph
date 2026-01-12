#pragma once

/**
 * @file tutorial_types.hpp
 * @brief Core types and enums for the Guided Learning System
 *
 * This file defines the fundamental types used throughout the tutorial system.
 * All types are editor-only and never participate in runtime builds.
 */

#include "NovelMind/core/types.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor::guided_learning {

/**
 * @brief Difficulty level for tutorials
 */
enum class TutorialLevel : u8 {
  Beginner,     // New users, basic concepts
  Intermediate, // Users familiar with basics
  Advanced      // Power users, complex workflows
};

/**
 * @brief Tutorial trigger mode - when does it activate?
 */
enum class TutorialTrigger : u8 {
  Manual,     // Only via Help Hub or explicit request
  FirstRun,   // On first launch of feature (version-tracked)
  Contextual, // When specific conditions are met (empty state, etc.)
  OnError     // When specific errors occur
};

/**
 * @brief Type of hint/step in a tutorial
 */
enum class HintType : u8 {
  Tooltip,    // Small tooltip near element
  Callout,    // Larger callout with arrow
  Spotlight,  // Dim everything except target
  EmptyState, // Inline hint for empty states
  Inline,     // Subtle inline text
  Dialog      // Modal dialog for important info
};

/**
 * @brief Position of callout relative to anchor
 */
enum class CalloutPosition : u8 {
  Auto, // Automatically determine best position
  Top,
  Bottom,
  Left,
  Right,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight
};

/**
 * @brief State of a tutorial step
 */
enum class StepState : u8 {
  Pending,   // Not yet shown
  Active,    // Currently showing
  Completed, // User acknowledged or condition met
  Skipped    // User skipped
};

/**
 * @brief State of overall tutorial
 */
enum class TutorialState : u8 {
  NotStarted,
  InProgress,
  Completed,
  Disabled // User disabled this tutorial
};

/**
 * @brief Condition that must be met to advance step
 */
struct StepCondition {
  enum class Type : u8 {
    UserAcknowledge, // User clicks Next/OK
    ElementClick,    // User clicks specific element
    ElementFocus,    // User focuses specific element
    ValueEntered,    // User enters a value
    PanelOpened,     // User opens a panel
    EventFired,      // Specific editor event occurs
    Timeout,         // Wait for timeout (auto-advance)
    Custom           // Custom condition callback
  };

  Type type = Type::UserAcknowledge;
  std::string targetAnchorId;    // For element-based conditions
  std::string eventType;         // For EventFired condition
  f32 timeoutSeconds = 0.0f;     // For Timeout condition
  std::string customConditionId; // For Custom condition
};

/**
 * @brief A single step in a tutorial walkthrough
 */
struct TutorialStep {
  std::string id;       // Unique step ID
  std::string title;    // Short title (optional)
  std::string content;  // Main text content
  std::string anchorId; // UI element to anchor to

  HintType hintType = HintType::Callout;
  CalloutPosition position = CalloutPosition::Auto;

  StepCondition advanceCondition; // When to move to next step

  bool showBackButton = true;    // Show "Back" button
  bool showSkipButton = true;    // Show "Skip" button
  bool showDontShowAgain = true; // Show "Don't show again" checkbox

  std::optional<std::string> customStyleClass; // For theming

  // Auto-disappear settings for subtle hints
  bool autoHide = false;
  f32 autoHideDelaySeconds = 5.0f;

  // Content formatting
  bool allowHtml = false; // Allow basic HTML in content
};

/**
 * @brief Contextual hint - simpler than full tutorial step
 */
struct ContextualHint {
  std::string id;
  std::string content;
  std::string anchorId;

  HintType hintType = HintType::Tooltip;
  CalloutPosition position = CalloutPosition::Auto;

  // Trigger conditions
  std::string triggerCondition; // E.g., "panel.empty", "selection.none"
  std::vector<std::string> requiredFeatureFlags;

  // Display limits
  u32 maxShowCount = 3;            // Show at most N times, then stop
  bool showOncePerSession = false; // Only show once per editor session

  // Auto-disappear
  bool autoHide = true;
  f32 autoHideDelaySeconds = 8.0f;
};

/**
 * @brief Complete tutorial definition (walkthrough)
 */
struct TutorialDefinition {
  std::string id;          // Unique tutorial ID
  std::string title;       // Display title
  std::string description; // Brief description for Help Hub
  std::string category;    // Category path (e.g., "Basics/Scene")

  TutorialLevel level = TutorialLevel::Beginner;
  TutorialTrigger trigger = TutorialTrigger::Manual;

  std::vector<TutorialStep> steps;

  // Trigger conditions
  std::string triggerPanelId; // Panel that triggers this tutorial
  std::string featureVersion; // Version tag for first-run tracking
  std::vector<std::string> requiredFeatureFlags;

  // Dependencies
  std::vector<std::string> prerequisiteTutorials; // Must complete these first

  // Metadata
  std::string author;
  std::string lastUpdated;       // ISO date string
  std::vector<std::string> tags; // For search

  // Duration estimate
  u32 estimatedMinutes = 5;
};

/**
 * @brief User progress for a single tutorial
 */
struct TutorialProgress {
  std::string tutorialId;
  TutorialState state = TutorialState::NotStarted;
  u32 currentStepIndex = 0;
  std::vector<StepState> stepStates;

  // Timestamps
  std::string startedAt;   // ISO timestamp
  std::string completedAt; // ISO timestamp

  // User preferences
  bool disabled = false;       // User disabled this tutorial
  bool neverShowAgain = false; // User said "don't show again"
};

/**
 * @brief User progress for contextual hints
 */
struct HintProgress {
  std::string hintId;
  u32 showCount = 0;       // How many times shown
  bool disabled = false;   // User disabled this hint
  std::string lastShownAt; // ISO timestamp
};

/**
 * @brief All user progress data
 */
struct GuidedLearningProgress {
  std::unordered_map<std::string, TutorialProgress> tutorials;
  std::unordered_map<std::string, HintProgress> hints;

  // Global settings stored here for convenience
  bool globallyDisabled = false;
  bool hintsEnabled = true;
  bool walkthroughsOnFirstRun = true;

  // Version tracking for first-run detection
  std::unordered_map<std::string, std::string> seenFeatureVersions;
};

/**
 * @brief Convert enum to string for serialization
 */
const char* tutorialLevelToString(TutorialLevel level);
const char* tutorialTriggerToString(TutorialTrigger trigger);
const char* hintTypeToString(HintType type);
const char* calloutPositionToString(CalloutPosition pos);
const char* stepStateToString(StepState state);
const char* tutorialStateToString(TutorialState state);

/**
 * @brief Parse enum from string
 */
std::optional<TutorialLevel> parseTutorialLevel(const std::string& str);
std::optional<TutorialTrigger> parseTutorialTrigger(const std::string& str);
std::optional<HintType> parseHintType(const std::string& str);
std::optional<CalloutPosition> parseCalloutPosition(const std::string& str);

} // namespace NovelMind::editor::guided_learning
