#pragma once

/**
 * @file editor_state.hpp
 * @brief Editor State Serialization System for NovelMind
 *
 * Provides serialization for editor state:
 * - Window layout (panel positions, sizes)
 * - User preferences (theme, scale, hotkeys)
 * - Recent projects
 * - Last opened files
 * - Session state for recovery
 *
 * This is critical for v0.2.0 GUI to remember user preferences
 * and restore the editor to its previous state.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Panel state for layout serialization
 */
struct PanelState {
  std::string panelId;
  bool visible = true;
  bool docked = true;
  i32 x = 0;
  i32 y = 0;
  i32 width = 300;
  i32 height = 400;
  f32 dockRatio = 0.25f;    // Ratio in dock split
  std::string dockTarget;   // Which dock area
  std::string dockPosition; // left, right, top, bottom, center
};

/**
 * @brief Layout preset
 */
struct LayoutPreset {
  std::string name;
  std::string description;
  std::vector<PanelState> panels;
  bool isBuiltIn = false;
};

/**
 * @brief Editor preferences
 */
struct EditorPreferences {
  // Appearance
  std::string theme = "dark";
  f32 uiScale = 1.0f;
  std::string fontFamily = "Inter";
  i32 fontSize = 14;
  bool showGrid = true;
  bool showGizmos = true;
  bool showFps = false;

  // Behavior
  bool autoSave = true;
  i32 autoSaveIntervalMinutes = 5;
  bool confirmOnExit = true;
  bool rememberOpenFiles = true;
  bool reopenLastProject = true;

  // Performance
  i32 undoHistorySize = 100;
  bool hardwareAcceleration = true;
  i32 maxRecentProjects = 10;

  // Language
  std::string locale = "en";
};

/**
 * @brief Hotkey binding
 */
struct HotkeyBinding {
  std::string action;
  std::string key;     // e.g., "Ctrl+S", "F5"
  std::string context; // "global", "scene", "timeline", etc.
  bool enabled = true;
};

/**
 * @brief Session state for recovery
 */
struct SessionState {
  std::string projectPath;
  std::vector<std::string> openFiles;
  std::string activeFile;
  std::unordered_map<std::string, std::string> panelStates; // Panel-specific state
  f64 timestamp = 0.0;
  bool cleanShutdown = false;
};

/**
 * @brief Editor state container
 */
struct EditorState {
  EditorPreferences preferences;
  std::vector<LayoutPreset> layouts;
  std::string activeLayoutName = "Default";
  std::vector<HotkeyBinding> hotkeys;
  std::vector<std::string> recentProjects;
  SessionState lastSession;
};

/**
 * @brief State value types for key-value storage
 */
using StateValue = std::variant<bool, i32, i64, f32, f64, std::string, std::vector<std::string>>;

/**
 * @brief Editor state manager
 *
 * Responsibilities:
 * - Save/load editor preferences
 * - Manage layout presets
 * - Track recent projects
 * - Handle session recovery
 * - Provide key-value state storage
 */
class EditorStateManager {
public:
  EditorStateManager();
  ~EditorStateManager();

  // Prevent copying
  EditorStateManager(const EditorStateManager&) = delete;
  EditorStateManager& operator=(const EditorStateManager&) = delete;

  /**
   * @brief Get singleton instance
   */
  static EditorStateManager& instance();

  // =========================================================================
  // Persistence
  // =========================================================================

  /**
   * @brief Load editor state from disk
   */
  Result<void> load();

  /**
   * @brief Save editor state to disk
   */
  Result<void> save();

  /**
   * @brief Get state file path
   */
  [[nodiscard]] std::string getStatePath() const;

  /**
   * @brief Set state file path
   */
  void setStatePath(const std::string& path);

  /**
   * @brief Reset to default state
   */
  void resetToDefaults();

  // =========================================================================
  // Preferences
  // =========================================================================

  /**
   * @brief Get preferences
   */
  [[nodiscard]] const EditorPreferences& getPreferences() const;

  /**
   * @brief Get mutable preferences
   */
  EditorPreferences& getPreferences();

  /**
   * @brief Set preferences
   */
  void setPreferences(const EditorPreferences& prefs);

  /**
   * @brief Get specific preference value
   */
  template <typename T> [[nodiscard]] std::optional<T> getPreference(const std::string& key) const;

  /**
   * @brief Set specific preference value
   */
  template <typename T> void setPreference(const std::string& key, const T& value);

  // =========================================================================
  // Layouts
  // =========================================================================

  /**
   * @brief Get all layout presets
   */
  [[nodiscard]] const std::vector<LayoutPreset>& getLayouts() const;

  /**
   * @brief Get active layout
   */
  [[nodiscard]] const LayoutPreset* getActiveLayout() const;

  /**
   * @brief Set active layout by name
   */
  bool setActiveLayout(const std::string& name);

  /**
   * @brief Save current layout as preset
   */
  void saveCurrentLayoutAs(const std::string& name, const std::vector<PanelState>& panels);

  /**
   * @brief Delete a layout preset
   */
  bool deleteLayout(const std::string& name);

  /**
   * @brief Reset layout to default
   */
  void resetLayoutToDefault();

  /**
   * @brief Get built-in layout names
   */
  [[nodiscard]] std::vector<std::string> getBuiltInLayoutNames() const;

  // =========================================================================
  // Hotkeys
  // =========================================================================

  /**
   * @brief Get all hotkey bindings
   */
  [[nodiscard]] const std::vector<HotkeyBinding>& getHotkeys() const;

  /**
   * @brief Get hotkey for action
   */
  [[nodiscard]] std::optional<HotkeyBinding> getHotkeyForAction(const std::string& action) const;

  /**
   * @brief Set hotkey for action
   */
  void setHotkey(const std::string& action, const std::string& key,
                 const std::string& context = "global");

  /**
   * @brief Remove hotkey for action
   */
  void removeHotkey(const std::string& action);

  /**
   * @brief Reset hotkeys to defaults
   */
  void resetHotkeysToDefaults();

  /**
   * @brief Check for hotkey conflicts
   */
  [[nodiscard]] std::vector<std::string> checkHotkeyConflicts(const std::string& key,
                                                              const std::string& context) const;

  // =========================================================================
  // Recent Projects
  // =========================================================================

  /**
   * @brief Get recent projects
   */
  [[nodiscard]] const std::vector<std::string>& getRecentProjects() const;

  /**
   * @brief Add to recent projects
   */
  void addRecentProject(const std::string& path);

  /**
   * @brief Remove from recent projects
   */
  void removeRecentProject(const std::string& path);

  /**
   * @brief Clear recent projects
   */
  void clearRecentProjects();

  // =========================================================================
  // Session Management
  // =========================================================================

  /**
   * @brief Save current session state
   */
  void saveSession(const SessionState& session);

  /**
   * @brief Get last session state
   */
  [[nodiscard]] const SessionState& getLastSession() const;

  /**
   * @brief Check if there's a recoverable session
   */
  [[nodiscard]] bool hasRecoverableSession() const;

  /**
   * @brief Mark session as clean shutdown
   */
  void markCleanShutdown();

  /**
   * @brief Clear session state
   */
  void clearSession();

  // =========================================================================
  // Key-Value Storage
  // =========================================================================

  /**
   * @brief Get state value
   */
  [[nodiscard]] std::optional<StateValue> getValue(const std::string& key) const;

  /**
   * @brief Set state value
   */
  void setValue(const std::string& key, const StateValue& value);

  /**
   * @brief Remove state value
   */
  void removeValue(const std::string& key);

  /**
   * @brief Check if key exists
   */
  [[nodiscard]] bool hasValue(const std::string& key) const;

  // =========================================================================
  // Change Notification
  // =========================================================================

  /**
   * @brief Listener for state changes
   */
  class IStateListener {
  public:
    virtual ~IStateListener() = default;
    virtual void onPreferencesChanged() {}
    virtual void onLayoutChanged() {}
    virtual void onHotkeysChanged() {}
  };

  void addListener(IStateListener* listener);
  void removeListener(IStateListener* listener);

private:
  void initializeDefaults();
  void notifyPreferencesChanged();
  void notifyLayoutChanged();
  void notifyHotkeysChanged();

  EditorState m_state;
  std::string m_statePath;
  std::unordered_map<std::string, StateValue> m_keyValueStore;
  std::vector<IStateListener*> m_listeners;
  bool m_dirty = false;

  static std::unique_ptr<EditorStateManager> s_instance;
};

} // namespace NovelMind::editor
