#pragma once

/**
 * @file editor_settings.hpp
 * @brief Editor Settings - Layouts, Hotkeys, and Themes
 *
 * Provides comprehensive editor customization:
 * - Layout management (save/load/presets)
 * - Hotkey configuration
 * - Theme support (light/dark/custom)
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class EditorApp;

// ============================================================================
// Layout System
// ============================================================================

/**
 * @brief Panel state for layout serialization
 */
struct PanelState {
  std::string name;
  bool visible = true;
  bool docked = true;
  i32 x = 0;
  i32 y = 0;
  i32 width = 300;
  i32 height = 400;
  std::string dockRegion; // left, right, top, bottom, center, floating
  i32 tabOrder = 0;
};

/**
 * @brief Window split configuration
 */
struct SplitState {
  std::string name;
  bool horizontal = true; // true = horizontal split, false = vertical
  f32 ratio = 0.5f;       // Split ratio (0.0 - 1.0)
  std::string first;      // Name of first panel/split
  std::string second;     // Name of second panel/split
};

/**
 * @brief Complete editor layout
 */
struct EditorLayout {
  std::string name;
  std::string description;
  std::vector<PanelState> panels;
  std::vector<SplitState> splits;
  std::string rootSplit;
  i32 mainWindowWidth = 1920;
  i32 mainWindowHeight = 1080;
  bool maximized = false;
};

/**
 * @brief Layout preset types
 */
enum class LayoutPreset : u8 {
  Default,
  StoryFocused,  // Story graph prominent
  SceneFocused,  // Scene view prominent
  ScriptFocused, // Script editor prominent
  DebugLayout,   // Debug panels visible
  Minimal,       // Minimal panels
  Custom
};

/**
 * @brief Layout Manager - handles window layout persistence
 */
class LayoutManager {
public:
  LayoutManager();
  ~LayoutManager() = default;

  /**
   * @brief Initialize with editor reference
   */
  void initialize(EditorApp *editor);

  /**
   * @brief Save current layout
   */
  Result<void> saveLayout(const std::string &name);

  /**
   * @brief Load a saved layout
   */
  Result<void> loadLayout(const std::string &name);

  /**
   * @brief Apply a preset layout
   */
  void applyPreset(LayoutPreset preset);

  /**
   * @brief Get current layout
   */
  [[nodiscard]] EditorLayout getCurrentLayout() const;

  /**
   * @brief Get list of saved layout names
   */
  [[nodiscard]] std::vector<std::string> getSavedLayouts() const;

  /**
   * @brief Delete a saved layout
   */
  void deleteLayout(const std::string &name);

  /**
   * @brief Export layout to file
   */
  Result<void> exportLayout(const std::string &path);

  /**
   * @brief Import layout from file
   */
  Result<void> importLayout(const std::string &path);

  /**
   * @brief Set layouts directory
   */
  void setLayoutsPath(const std::string &path);

private:
  EditorLayout createPresetLayout(LayoutPreset preset);
  void applyLayout(const EditorLayout &layout);
  EditorLayout captureCurrentLayout();

  EditorApp *m_editor = nullptr;
  std::string m_layoutsPath;
  std::unordered_map<std::string, EditorLayout> m_savedLayouts;
};

// ============================================================================
// Hotkey System
// ============================================================================

/**
 * @brief Key modifier flags
 */
enum class KeyModifier : u8 {
  None = 0,
  Ctrl = 1 << 0,
  Shift = 1 << 1,
  Alt = 1 << 2,
  Super = 1 << 3 // Windows/Command key
};

inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
  return static_cast<KeyModifier>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline KeyModifier operator&(KeyModifier a, KeyModifier b) {
  return static_cast<KeyModifier>(static_cast<u8>(a) & static_cast<u8>(b));
}

/**
 * @brief Key combination for hotkey
 */
struct KeyBinding {
  i32 keyCode = 0; // Platform-specific key code
  KeyModifier modifiers = KeyModifier::None;

  bool operator==(const KeyBinding &other) const {
    return keyCode == other.keyCode && modifiers == other.modifiers;
  }

  [[nodiscard]] std::string toString() const;
  static KeyBinding fromString(const std::string &str);
};

/**
 * @brief Action category for organization
 */
enum class ActionCategory : u8 {
  File,
  Edit,
  View,
  Project,
  Build,
  Play,
  Navigation,
  Selection,
  Custom
};

/**
 * @brief Hotkey action definition
 */
struct HotkeyAction {
  std::string id;   // Unique identifier
  std::string name; // Display name
  std::string description;
  ActionCategory category = ActionCategory::Custom;
  KeyBinding defaultBinding;
  KeyBinding currentBinding;
  bool enabled = true;
};

/**
 * @brief Callback for hotkey actions
 */
using HotkeyCallback = std::function<void()>;

/**
 * @brief Hotkey Manager - keyboard shortcut configuration
 */
class HotkeyManager {
public:
  HotkeyManager();
  ~HotkeyManager() = default;

  /**
   * @brief Register a hotkey action
   */
  void registerAction(const HotkeyAction &action, HotkeyCallback callback);

  /**
   * @brief Unregister an action
   */
  void unregisterAction(const std::string &actionId);

  /**
   * @brief Set hotkey binding for an action
   */
  void setBinding(const std::string &actionId, const KeyBinding &binding);

  /**
   * @brief Reset to default binding
   */
  void resetToDefault(const std::string &actionId);

  /**
   * @brief Reset all to defaults
   */
  void resetAllToDefaults();

  /**
   * @brief Get action by ID
   */
  [[nodiscard]] std::optional<HotkeyAction>
  getAction(const std::string &actionId) const;

  /**
   * @brief Get all actions in a category
   */
  [[nodiscard]] std::vector<HotkeyAction>
  getActionsByCategory(ActionCategory category) const;

  /**
   * @brief Get all actions
   */
  [[nodiscard]] const std::unordered_map<std::string, HotkeyAction> &
  getAllActions() const;

  /**
   * @brief Handle key press event
   * @return true if handled
   */
  bool handleKeyPress(i32 keyCode, KeyModifier modifiers);

  /**
   * @brief Check for binding conflicts
   */
  [[nodiscard]] std::vector<std::string>
  getConflicts(const KeyBinding &binding) const;

  /**
   * @brief Save hotkey configuration
   */
  Result<void> save(const std::string &path);

  /**
   * @brief Load hotkey configuration
   */
  Result<void> load(const std::string &path);

  /**
   * @brief Register default editor hotkeys
   */
  void registerDefaultHotkeys(EditorApp *editor);

private:
  std::unordered_map<std::string, HotkeyAction> m_actions;
  std::unordered_map<std::string, HotkeyCallback> m_callbacks;
};

// ============================================================================
// Theme System
// ============================================================================

/**
 * @brief Color role in the theme
 */
enum class ThemeColor : u8 {
  // Window
  WindowBackground,
  WindowForeground,
  WindowBorder,

  // Panel
  PanelBackground,
  PanelHeader,
  PanelHeaderText,
  PanelBorder,

  // Text
  TextPrimary,
  TextSecondary,
  TextDisabled,
  TextLink,
  TextHighlight,

  // Input
  InputBackground,
  InputBorder,
  InputBorderFocused,
  InputText,
  InputPlaceholder,

  // Button
  ButtonBackground,
  ButtonBackgroundHover,
  ButtonBackgroundPressed,
  ButtonText,
  ButtonBorder,

  // Selection
  SelectionBackground,
  SelectionText,

  // List
  ListBackground,
  ListItemHover,
  ListItemSelected,
  ListItemAlternate,

  // Scrollbar
  ScrollbarTrack,
  ScrollbarThumb,
  ScrollbarThumbHover,

  // Status
  StatusError,
  StatusWarning,
  StatusInfo,
  StatusSuccess,

  // Scene View
  SceneBackground,
  SceneGrid,
  SceneGridMajor,
  SceneSelection,
  SceneGizmo,

  // Story Graph
  GraphBackground,
  GraphGrid,
  GraphNodeBackground,
  GraphNodeBorder,
  GraphNodeSelected,
  GraphConnection,
  GraphConnectionSelected,

  // Code
  CodeBackground,
  CodeKeyword,
  CodeString,
  CodeNumber,
  CodeComment,
  CodeFunction,
  CodeVariable,
  CodeOperator,

  // Misc
  DragOverlay,
  DropTarget,
  Tooltip,

  COUNT
};

/**
 * @brief Font size presets
 */
struct ThemeFonts {
  f32 small = 11.0f;
  f32 normal = 13.0f;
  f32 large = 15.0f;
  f32 title = 18.0f;
  f32 header = 24.0f;
  std::string fontFamily = "default";
  std::string monoFamily = "monospace";
};

/**
 * @brief Theme metrics (spacing, sizing)
 */
struct ThemeMetrics {
  f32 paddingSmall = 4.0f;
  f32 paddingNormal = 8.0f;
  f32 paddingLarge = 16.0f;
  f32 borderRadius = 4.0f;
  f32 borderWidth = 1.0f;
  f32 scrollbarWidth = 12.0f;
  f32 panelHeaderHeight = 28.0f;
  f32 toolbarHeight = 40.0f;
  f32 statusBarHeight = 24.0f;
};

/**
 * @brief Complete theme definition
 */
struct Theme {
  std::string name;
  std::string author;
  bool isDark = true;

  std::array<renderer::Color, static_cast<size_t>(ThemeColor::COUNT)> colors;
  ThemeFonts fonts;
  ThemeMetrics metrics;

  [[nodiscard]] const renderer::Color &getColor(ThemeColor color) const;
  void setColor(ThemeColor color, const renderer::Color &value);
};

/**
 * @brief Theme Manager - visual styling system
 */
class ThemeManager {
public:
  ThemeManager();
  ~ThemeManager() = default;

  /**
   * @brief Apply a theme
   */
  void applyTheme(const std::string &themeName);

  /**
   * @brief Get current theme
   */
  [[nodiscard]] const Theme &getCurrentTheme() const;

  /**
   * @brief Get theme by name
   */
  [[nodiscard]] std::optional<Theme> getTheme(const std::string &name) const;

  /**
   * @brief Get list of available themes
   */
  [[nodiscard]] std::vector<std::string> getAvailableThemes() const;

  /**
   * @brief Register a custom theme
   */
  void registerTheme(const Theme &theme);

  /**
   * @brief Unregister a theme
   */
  void unregisterTheme(const std::string &name);

  /**
   * @brief Export theme to file
   */
  Result<void> exportTheme(const std::string &themeName,
                           const std::string &path);

  /**
   * @brief Import theme from file
   */
  Result<void> importTheme(const std::string &path);

  /**
   * @brief Get color from current theme
   */
  [[nodiscard]] const renderer::Color &getColor(ThemeColor color) const;

  /**
   * @brief Get current font settings
   */
  [[nodiscard]] const ThemeFonts &getFonts() const;

  /**
   * @brief Get current metrics
   */
  [[nodiscard]] const ThemeMetrics &getMetrics() const;

  /**
   * @brief Create default light theme
   */
  static Theme createLightTheme();

  /**
   * @brief Create default dark theme
   */
  static Theme createDarkTheme();

private:
  void registerBuiltinThemes();

  std::unordered_map<std::string, Theme> m_themes;
  std::string m_currentThemeName;
  Theme m_currentTheme;
};

// ============================================================================
// Editor Preferences
// ============================================================================

/**
 * @brief General editor preferences
 */
struct EditorPreferences {
  // Appearance
  std::string theme = "dark";
  f32 uiScale = 1.0f;
  bool showTooltips = true;

  // Dock Panel Title Bars (Issue #293)
  i32 titleBarHeight = 22;         // Height in pixels (18-32)
  bool compactTitleBars = true;    // Use compact mode
  bool autoHideTitleBars = false;  // Auto-hide when maximized
  bool showPanelIcons = true;      // Show icons in title bars

  // Layout
  std::string defaultLayout = "default";
  bool rememberLayout = true;

  // Behavior
  bool autoSave = true;
  i32 autoSaveIntervalSeconds = 300;
  bool confirmOnClose = true;
  bool reopenLastProject = true;

  // Script Editor
  bool showLineNumbers = true;
  bool wordWrap = false;
  i32 tabSize = 4;
  bool insertSpaces = true;
  bool autoComplete = true;
  bool highlightCurrentLine = true;

  // Preview
  f32 previewScale = 1.0f;
  bool showFps = false;
  bool vsync = true;

  // Debug
  bool showPerformanceOverlay = false;
  bool verboseLogging = false;

  // Recent files
  std::vector<std::string> recentProjects;
  i32 maxRecentProjects = 10;
};

/**
 * @brief Preferences Manager
 */
class PreferencesManager {
public:
  PreferencesManager();
  ~PreferencesManager() = default;

  /**
   * @brief Load preferences from disk
   */
  Result<void> load(const std::string &path);

  /**
   * @brief Save preferences to disk
   */
  Result<void> save(const std::string &path);

  /**
   * @brief Get current preferences
   */
  [[nodiscard]] EditorPreferences &get();
  [[nodiscard]] const EditorPreferences &get() const;

  /**
   * @brief Reset to defaults
   */
  void resetToDefaults();

  /**
   * @brief Add a recent project
   */
  void addRecentProject(const std::string &path);

  /**
   * @brief Get recent projects
   */
  [[nodiscard]] const std::vector<std::string> &getRecentProjects() const;

private:
  EditorPreferences m_prefs;
  std::string m_prefsPath;
};

} // namespace NovelMind::editor
