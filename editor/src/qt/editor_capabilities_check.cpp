/**
 * @file editor_capabilities_check.cpp
 * @brief Implementation of Editor Capabilities Verification System
 *
 * Reference: docs/gui_core_parity_matrix.md
 */

#include "NovelMind/editor/qt/editor_capabilities_check.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"

#include <cassert>
#include <sstream>

namespace NovelMind::editor::qt {

const char* getPanelName(RequiredPanel panel) {
  switch (panel) {
  case RequiredPanel::SceneView:
    return "SceneView";
  case RequiredPanel::Hierarchy:
    return "Hierarchy";
  case RequiredPanel::Inspector:
    return "Inspector";
  case RequiredPanel::StoryGraph:
    return "StoryGraph";
  case RequiredPanel::ScriptEditor:
    return "ScriptEditor";
  case RequiredPanel::AssetBrowser:
    return "AssetBrowser";
  case RequiredPanel::Timeline:
    return "Timeline";
  case RequiredPanel::CurveEditor:
    return "CurveEditor";
  case RequiredPanel::VoiceManager:
    return "VoiceManager";
  case RequiredPanel::VoiceStudio:
    return "VoiceStudio";
  case RequiredPanel::RecordingStudio:
    return "RecordingStudio";
  case RequiredPanel::Localization:
    return "Localization";
  case RequiredPanel::Console:
    return "Console";
  case RequiredPanel::Diagnostics:
    return "Diagnostics";
  case RequiredPanel::DebugOverlay:
    return "DebugOverlay";
  case RequiredPanel::PlayToolbar:
    return "PlayToolbar";
  case RequiredPanel::ProjectSettings:
    return "ProjectSettings";
  case RequiredPanel::BuildSettings:
    return "BuildSettings";
  }
  return "Unknown";
}

static std::string getCategoryForPanel(RequiredPanel panel) {
  switch (panel) {
  case RequiredPanel::SceneView:
  case RequiredPanel::Hierarchy:
  case RequiredPanel::Inspector:
    return "Scene";
  case RequiredPanel::StoryGraph:
  case RequiredPanel::ScriptEditor:
    return "Script";
  case RequiredPanel::AssetBrowser:
    return "Assets";
  case RequiredPanel::Timeline:
  case RequiredPanel::CurveEditor:
    return "Animation";
  case RequiredPanel::VoiceManager:
  case RequiredPanel::VoiceStudio:
  case RequiredPanel::RecordingStudio:
    return "Audio";
  case RequiredPanel::Localization:
    return "Localization";
  case RequiredPanel::Console:
  case RequiredPanel::Diagnostics:
  case RequiredPanel::DebugOverlay:
    return "Diagnostics";
  case RequiredPanel::PlayToolbar:
    return "PlayMode";
  case RequiredPanel::ProjectSettings:
  case RequiredPanel::BuildSettings:
    return "Project";
  }
  return "Unknown";
}

static std::string getCategoryForAction(const char* actionId) {
  std::string id(actionId);
  if (id.find("Audio:") == 0 || id.find("Voice:") == 0) {
    return "Audio";
  }
  if (id.find("Localization:") == 0) {
    return "Localization";
  }
  if (id.find("Animation:") == 0) {
    return "Animation";
  }
  if (id.find("Scene:") == 0) {
    return "Scene";
  }
  if (id.find("Script:") == 0) {
    return "Script";
  }
  if (id.find("Project:") == 0) {
    return "Project";
  }
  if (id.find("PlayMode:") == 0) {
    return "PlayMode";
  }
  if (id.find("Diagnostics:") == 0) {
    return "Diagnostics";
  }
  return "Unknown";
}

CapabilityCheckResult checkPanelAvailable(const NMMainWindow* window, RequiredPanel panel) {
  CapabilityCheckResult result;
  result.category = getCategoryForPanel(panel);
  result.capability = std::string("Panel:") + getPanelName(panel);

  if (!window) {
    result.available = false;
    result.details = "Main window is null";
    return result;
  }

  // Check panel availability based on type
  switch (panel) {
  case RequiredPanel::SceneView:
    result.available = (window->sceneViewPanel() != nullptr);
    break;
  case RequiredPanel::Hierarchy:
    result.available = (window->hierarchyPanel() != nullptr);
    break;
  case RequiredPanel::Inspector:
    result.available = (window->inspectorPanel() != nullptr);
    break;
  case RequiredPanel::StoryGraph:
    result.available = (window->storyGraphPanel() != nullptr);
    break;
  case RequiredPanel::ScriptEditor:
    result.available = (window->scriptEditorPanel() != nullptr);
    break;
  case RequiredPanel::AssetBrowser:
    result.available = (window->assetBrowserPanel() != nullptr);
    break;
  case RequiredPanel::Timeline:
    result.available = (window->timelinePanel() != nullptr);
    break;
  case RequiredPanel::CurveEditor:
    result.available = (window->curveEditorPanel() != nullptr);
    break;
  case RequiredPanel::VoiceManager:
    result.available = (window->voiceManagerPanel() != nullptr);
    break;
  case RequiredPanel::VoiceStudio:
    result.available = (window->voiceStudioPanel() != nullptr);
    break;
  case RequiredPanel::RecordingStudio:
    result.available = (window->recordingStudioPanel() != nullptr);
    break;
  case RequiredPanel::Localization:
    result.available = (window->localizationPanel() != nullptr);
    break;
  case RequiredPanel::Console:
    result.available = (window->consolePanel() != nullptr);
    break;
  case RequiredPanel::Diagnostics:
    result.available = (window->diagnosticsPanel() != nullptr);
    break;
  case RequiredPanel::DebugOverlay:
    result.available = (window->debugOverlayPanel() != nullptr);
    break;
  case RequiredPanel::PlayToolbar:
    result.available = (window->playToolbarPanel() != nullptr);
    break;
  case RequiredPanel::ProjectSettings:
    result.available = (window->projectSettingsPanel() != nullptr);
    break;
  case RequiredPanel::BuildSettings:
    result.available = (window->buildSettingsPanel() != nullptr);
    break;
  }

  if (!result.available) {
    result.details = "Panel not initialized";
  }

  return result;
}

CapabilityCheckResult checkActionAvailable(const NMMainWindow* window, const char* actionId) {
  CapabilityCheckResult result;
  result.category = getCategoryForAction(actionId);
  result.capability = actionId;

  if (!window) {
    result.available = false;
    result.details = "Main window is null";
    return result;
  }

  // For now, we assume actions are available if their parent panel exists
  // In a more complete implementation, we would check specific action bindings
  std::string id(actionId);

  if (id == RequiredActions::AUDIO_PREVIEW || id == RequiredActions::AUDIO_VOLUME ||
      id == RequiredActions::VOICE_BINDING) {
    result.available = (window->voiceManagerPanel() != nullptr);
  } else if (id == RequiredActions::LOC_EDIT || id == RequiredActions::LOC_IMPORT ||
             id == RequiredActions::LOC_EXPORT || id == RequiredActions::LOC_MISSING) {
    result.available = (window->localizationPanel() != nullptr);
  } else if (id == RequiredActions::ANIM_KEYFRAME || id == RequiredActions::ANIM_EASING ||
             id == RequiredActions::ANIM_PREVIEW) {
    result.available = (window->timelinePanel() != nullptr);
  } else if (id == RequiredActions::SCENE_CREATE || id == RequiredActions::SCENE_DELETE ||
             id == RequiredActions::SCENE_TRANSFORM) {
    result.available = (window->sceneViewPanel() != nullptr);
  } else if (id == RequiredActions::SCENE_PROPERTY) {
    result.available = (window->inspectorPanel() != nullptr);
  } else if (id == RequiredActions::SCRIPT_EDIT || id == RequiredActions::SCRIPT_COMPILE) {
    result.available = (window->scriptEditorPanel() != nullptr);
  } else if (id == RequiredActions::SCRIPT_DEBUG) {
    result.available = (window->storyGraphPanel() != nullptr);
  } else if (id == RequiredActions::PROJECT_CREATE || id == RequiredActions::PROJECT_OPEN ||
             id == RequiredActions::PROJECT_SAVE) {
    // These are menu actions, always available
    result.available = true;
  } else if (id == RequiredActions::PLAY_START || id == RequiredActions::PLAY_PAUSE ||
             id == RequiredActions::PLAY_STEP) {
    result.available = (window->playToolbarPanel() != nullptr);
  } else if (id == RequiredActions::DIAG_SHOW || id == RequiredActions::DIAG_NAVIGATE) {
    result.available = (window->diagnosticsPanel() != nullptr);
  } else {
    result.available = false;
    result.details = "Unknown action ID";
  }

  if (!result.available && result.details.empty()) {
    result.details = "Required panel not available";
  }

  return result;
}

EditorCapabilitiesReport checkEditorCapabilities(const NMMainWindow* window, bool strict) {
  EditorCapabilitiesReport report;
  report.totalChecks = 0;
  report.passedChecks = 0;
  report.failedChecks = 0;
  report.warningChecks = 0;

  // Check all required panels
  const RequiredPanel panels[] = {
      RequiredPanel::SceneView,   RequiredPanel::Hierarchy,       RequiredPanel::Inspector,
      RequiredPanel::StoryGraph,  RequiredPanel::ScriptEditor,    RequiredPanel::AssetBrowser,
      RequiredPanel::Timeline,    RequiredPanel::CurveEditor,     RequiredPanel::VoiceManager,
      RequiredPanel::VoiceStudio, RequiredPanel::RecordingStudio, RequiredPanel::Localization,
      RequiredPanel::Console,     RequiredPanel::Diagnostics,     RequiredPanel::DebugOverlay,
      RequiredPanel::PlayToolbar, RequiredPanel::ProjectSettings, RequiredPanel::BuildSettings};

  for (auto panel : panels) {
    auto result = checkPanelAvailable(window, panel);
    report.results.push_back(result);
    report.totalChecks++;
    if (result.available) {
      report.passedChecks++;
    } else {
      report.failedChecks++;
      if (strict) {
        core::Logger::instance().error("CAPABILITY CHECK FAILED: Panel " + result.capability +
                                       " is not available");
        assert(false && "Missing required panel");
      }
    }
  }

  // Check all required actions
  const char* actions[] = {RequiredActions::AUDIO_PREVIEW,   RequiredActions::AUDIO_VOLUME,
                           RequiredActions::VOICE_BINDING,   RequiredActions::LOC_EDIT,
                           RequiredActions::LOC_IMPORT,      RequiredActions::LOC_EXPORT,
                           RequiredActions::LOC_MISSING,     RequiredActions::ANIM_KEYFRAME,
                           RequiredActions::ANIM_EASING,     RequiredActions::ANIM_PREVIEW,
                           RequiredActions::SCENE_CREATE,    RequiredActions::SCENE_DELETE,
                           RequiredActions::SCENE_TRANSFORM, RequiredActions::SCENE_PROPERTY,
                           RequiredActions::SCRIPT_EDIT,     RequiredActions::SCRIPT_COMPILE,
                           RequiredActions::SCRIPT_DEBUG,    RequiredActions::PROJECT_CREATE,
                           RequiredActions::PROJECT_OPEN,    RequiredActions::PROJECT_SAVE,
                           RequiredActions::PLAY_START,      RequiredActions::PLAY_PAUSE,
                           RequiredActions::PLAY_STEP,       RequiredActions::DIAG_SHOW,
                           RequiredActions::DIAG_NAVIGATE};

  for (auto actionId : actions) {
    auto result = checkActionAvailable(window, actionId);
    report.results.push_back(result);
    report.totalChecks++;
    if (result.available) {
      report.passedChecks++;
    } else {
      report.failedChecks++;
      if (strict) {
        core::Logger::instance().error("CAPABILITY CHECK FAILED: Action " + result.capability +
                                       " is not available");
        assert(false && "Missing required action");
      }
    }
  }

  return report;
}

std::string EditorCapabilitiesReport::toString() const {
  std::ostringstream ss;
  ss << "=== Editor Capabilities Report ===\n";
  ss << "Total checks: " << totalChecks << "\n";
  ss << "Passed: " << passedChecks << "\n";
  ss << "Failed: " << failedChecks << "\n";
  ss << "Warnings: " << warningChecks << "\n";
  ss << "\n";

  if (failedChecks > 0) {
    ss << "--- Failed Capabilities ---\n";
    for (const auto& result : results) {
      if (!result.available) {
        ss << "  [FAIL] " << result.category << "/" << result.capability;
        if (!result.details.empty()) {
          ss << " (" << result.details << ")";
        }
        ss << "\n";
      }
    }
    ss << "\n";
  }

  if (allMandatoryPassed()) {
    ss << "All mandatory capabilities are available.\n";
  } else {
    ss << "WARNING: Some mandatory capabilities are missing!\n";
    ss << "See docs/gui_core_parity_matrix.md for details.\n";
  }

  return ss.str();
}

void logCapabilitiesReport(const EditorCapabilitiesReport& report) {
  auto& logger = core::Logger::instance();

  logger.info("=== Editor Capabilities Check ===");
  logger.info("Total: " + std::to_string(report.totalChecks) +
              ", Passed: " + std::to_string(report.passedChecks) +
              ", Failed: " + std::to_string(report.failedChecks));

  for (const auto& result : report.results) {
    if (!result.available) {
      logger.warning("[MISSING] " + result.category + "/" + result.capability + ": " +
                     result.details);
    }
  }

  if (report.allMandatoryPassed()) {
    logger.info("All mandatory capabilities available.");
  } else {
    logger.warning("Some capabilities missing! Check docs/gui_core_parity_matrix.md");
  }
}

} // namespace NovelMind::editor::qt
