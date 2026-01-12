/**
 * @file editor_capabilities_check.hpp
 * @brief Editor Capabilities Verification System
 *
 * This module provides a mechanism to verify that all required editor panels
 * and features are present and properly integrated. It serves as a regression
 * prevention mechanism for GUI ↔ Core feature parity.
 *
 * Usage:
 *   - Call checkEditorCapabilities() during editor startup in debug mode
 *   - Any missing capability will be logged as a warning
 *   - In strict mode, missing capabilities will cause assertion failures
 *
 * Reference: docs/gui_core_parity_matrix.md
 */

#pragma once

#include <string>
#include <vector>

namespace NovelMind::editor::qt {

// Forward declarations
class NMMainWindow;

/**
 * @brief Result of a capability check
 */
struct CapabilityCheckResult {
  std::string category;   // e.g., "Audio", "Localization", "Scene"
  std::string capability; // e.g., "VoicePreview", "StringTableEdit"
  bool available;         // Whether the capability is present
  std::string details;    // Additional details or reason for failure
};

/**
 * @brief Comprehensive capabilities check report
 */
struct EditorCapabilitiesReport {
  std::vector<CapabilityCheckResult> results;
  int totalChecks;
  int passedChecks;
  int failedChecks;
  int warningChecks;

  /**
   * @brief Check if all mandatory capabilities are available
   */
  bool allMandatoryPassed() const { return failedChecks == 0; }

  /**
   * @brief Get formatted report string
   */
  std::string toString() const;
};

/**
 * @brief Required panel types for GUI ↔ Core parity
 *
 * Based on docs/gui_core_parity_matrix.md, these panels are mandatory
 * for complete editor functionality.
 */
enum class RequiredPanel {
  // Core editing panels
  SceneView, // Visual scene editing
  Hierarchy, // Scene object tree
  Inspector, // Property editing

  // Story panels
  StoryGraph,   // Node-based story editor
  ScriptEditor, // NMScript code editor

  // Asset panels
  AssetBrowser, // Asset management

  // Animation panels
  Timeline,    // Keyframe animation
  CurveEditor, // Easing curves

  // Audio panels
  VoiceManager,    // Voice file management
  VoiceStudio,     // Audio editing
  RecordingStudio, // Voice recording

  // Localization
  Localization, // String table editing

  // Debug/Diagnostics
  Console,      // Log output
  Diagnostics,  // Error/warning list
  DebugOverlay, // Runtime debug info

  // Play mode
  PlayToolbar, // Play/pause/stop controls

  // Settings
  ProjectSettings, // Project configuration
  BuildSettings    // Build configuration
};

/**
 * @brief Required actions for each core system
 *
 * These are the minimum actions that must be available through the UI
 * for each core system to be considered fully exposed.
 */
struct RequiredActions {
  // Audio system
  static constexpr const char* AUDIO_PREVIEW = "Audio:Preview";
  static constexpr const char* AUDIO_VOLUME = "Audio:VolumeControl";
  static constexpr const char* VOICE_BINDING = "Voice:Binding";

  // Localization system
  static constexpr const char* LOC_EDIT = "Localization:EditString";
  static constexpr const char* LOC_IMPORT = "Localization:Import";
  static constexpr const char* LOC_EXPORT = "Localization:Export";
  static constexpr const char* LOC_MISSING = "Localization:FindMissing";

  // Animation system
  static constexpr const char* ANIM_KEYFRAME = "Animation:AddKeyframe";
  static constexpr const char* ANIM_EASING = "Animation:SetEasing";
  static constexpr const char* ANIM_PREVIEW = "Animation:Preview";

  // Scene system
  static constexpr const char* SCENE_CREATE = "Scene:CreateObject";
  static constexpr const char* SCENE_DELETE = "Scene:DeleteObject";
  static constexpr const char* SCENE_TRANSFORM = "Scene:TransformObject";
  static constexpr const char* SCENE_PROPERTY = "Scene:EditProperty";

  // Script system
  static constexpr const char* SCRIPT_EDIT = "Script:EditCode";
  static constexpr const char* SCRIPT_COMPILE = "Script:Compile";
  static constexpr const char* SCRIPT_DEBUG = "Script:Breakpoints";

  // Project system
  static constexpr const char* PROJECT_CREATE = "Project:Create";
  static constexpr const char* PROJECT_OPEN = "Project:Open";
  static constexpr const char* PROJECT_SAVE = "Project:Save";

  // Play mode
  static constexpr const char* PLAY_START = "PlayMode:Start";
  static constexpr const char* PLAY_PAUSE = "PlayMode:Pause";
  static constexpr const char* PLAY_STEP = "PlayMode:Step";

  // Diagnostics
  static constexpr const char* DIAG_SHOW = "Diagnostics:ShowErrors";
  static constexpr const char* DIAG_NAVIGATE = "Diagnostics:NavigateToSource";
};

/**
 * @brief Check if a specific panel is available
 * @param window Pointer to the main window
 * @param panel The required panel type
 * @return CapabilityCheckResult with availability status
 */
CapabilityCheckResult checkPanelAvailable(const NMMainWindow* window, RequiredPanel panel);

/**
 * @brief Check if a specific action is available
 * @param window Pointer to the main window
 * @param actionId Action identifier from RequiredActions
 * @return CapabilityCheckResult with availability status
 */
CapabilityCheckResult checkActionAvailable(const NMMainWindow* window, const char* actionId);

/**
 * @brief Run comprehensive editor capabilities check
 *
 * This function checks all required panels and actions as defined in
 * the GUI ↔ Core parity matrix (docs/gui_core_parity_matrix.md).
 *
 * @param window Pointer to the main window
 * @param strict If true, missing mandatory capabilities cause assertion
 * @return EditorCapabilitiesReport with all check results
 */
EditorCapabilitiesReport checkEditorCapabilities(const NMMainWindow* window, bool strict = false);

/**
 * @brief Log capabilities report to console
 * @param report The report to log
 */
void logCapabilitiesReport(const EditorCapabilitiesReport& report);

/**
 * @brief Get panel name string
 */
const char* getPanelName(RequiredPanel panel);

} // namespace NovelMind::editor::qt
