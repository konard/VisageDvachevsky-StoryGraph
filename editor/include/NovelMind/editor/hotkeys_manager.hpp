/**
 * @file hotkeys_manager.hpp
 * @brief Keyboard Shortcuts System for NovelMind Editor v0.2.0
 *
 * Provides comprehensive keyboard shortcut management:
 * - Configurable key bindings
 * - Action-based system with named commands
 * - Modifier key support (Ctrl, Alt, Shift, Meta)
 * - Context-aware shortcuts
 * - User customization and persistence
 * - Conflict detection
 */

#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Key codes (compatible with SDL2 keycodes)
 */
enum class KeyCode : i32 {
  Unknown = 0,

  // Letters
  A = 'a',
  B = 'b',
  C = 'c',
  D = 'd',
  E = 'e',
  F = 'f',
  G = 'g',
  H = 'h',
  I = 'i',
  J = 'j',
  K = 'k',
  L = 'l',
  M = 'm',
  N = 'n',
  O = 'o',
  P = 'p',
  Q = 'q',
  R = 'r',
  S = 's',
  T = 't',
  U = 'u',
  V = 'v',
  W = 'w',
  X = 'x',
  Y = 'y',
  Z = 'z',

  // Numbers
  Num0 = '0',
  Num1 = '1',
  Num2 = '2',
  Num3 = '3',
  Num4 = '4',
  Num5 = '5',
  Num6 = '6',
  Num7 = '7',
  Num8 = '8',
  Num9 = '9',

  // Function keys
  F1 = 0x4000003A,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,

  // Navigation
  Up = 0x40000052,
  Down = 0x40000051,
  Left = 0x40000050,
  Right = 0x4000004F,
  Home = 0x4000004A,
  End = 0x4000004D,
  PageUp = 0x4000004B,
  PageDown = 0x4000004E,

  // Editing
  Backspace = '\b',
  Tab = '\t',
  Enter = '\r',
  Escape = 0x1B,
  Space = ' ',
  Delete = 0x7F,
  Insert = 0x40000049,

  // Modifiers (for display purposes, not for binding)
  LeftShift = 0x400000E1,
  RightShift = 0x400000E5,
  LeftCtrl = 0x400000E0,
  RightCtrl = 0x400000E4,
  LeftAlt = 0x400000E2,
  RightAlt = 0x400000E6,
  LeftMeta = 0x400000E3,
  RightMeta = 0x400000E7,

  // Special
  PrintScreen = 0x40000046,
  ScrollLock = 0x40000047,
  Pause = 0x40000048,

  // Punctuation
  Minus = '-',
  Plus = '=',
  LeftBracket = '[',
  RightBracket = ']',
  Semicolon = ';',
  Quote = '\'',
  Backquote = '`',
  Comma = ',',
  Period = '.',
  Slash = '/',
  Backslash = '\\'
};

/**
 * @brief Modifier key flags
 */
enum class Modifiers : u8 {
  None = 0,
  Ctrl = 1 << 0,
  Shift = 1 << 1,
  Alt = 1 << 2,
  Meta = 1 << 3, // Cmd on Mac, Win on Windows

  CtrlShift = Ctrl | Shift,
  CtrlAlt = Ctrl | Alt,
  ShiftAlt = Shift | Alt,
  CtrlShiftAlt = Ctrl | Shift | Alt
};

inline Modifiers operator|(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline Modifiers operator&(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<u8>(a) & static_cast<u8>(b));
}

inline bool hasModifier(Modifiers mods, Modifiers flag) {
  return (static_cast<u8>(mods) & static_cast<u8>(flag)) != 0;
}

/**
 * @brief A keyboard shortcut combination (used by HotkeysManager)
 *
 * Note: This is distinct from the Shortcut struct in editor_settings.hpp
 * which uses different types (i32 keyCode vs KeyCode enum).
 */
struct Shortcut {
  KeyCode key = KeyCode::Unknown;
  Modifiers modifiers = Modifiers::None;

  Shortcut() = default;
  Shortcut(KeyCode k) : key(k), modifiers(Modifiers::None) {}
  Shortcut(KeyCode k, Modifiers m) : key(k), modifiers(m) {}

  bool operator==(const Shortcut &other) const {
    return key == other.key && modifiers == other.modifiers;
  }

  bool operator!=(const Shortcut &other) const { return !(*this == other); }

  [[nodiscard]] bool isValid() const { return key != KeyCode::Unknown; }

  /**
   * @brief Get human-readable string representation
   */
  [[nodiscard]] std::string toString() const;

  /**
   * @brief Parse from string (e.g., "Ctrl+S", "F5", "Ctrl+Shift+Z")
   */
  static Shortcut fromString(const std::string &str);
};

/**
 * @brief Hash function for Shortcut
 */
struct ShortcutHash {
  size_t operator()(const Shortcut &s) const {
    return std::hash<i32>{}(static_cast<i32>(s.key)) ^
           (std::hash<u8>{}(static_cast<u8>(s.modifiers)) << 16);
  }
};

/**
 * @brief Context in which shortcuts are active
 */
enum class ShortcutContext : u8 {
  Global,       // Always active
  Editor,       // When editor is focused
  SceneView,    // When scene view is focused
  StoryGraph,   // When story graph is focused
  Timeline,     // When timeline is focused
  Inspector,    // When inspector is focused
  AssetBrowser, // When asset browser is focused
  Hierarchy,    // When hierarchy is focused
  Console,      // When console is focused
  TextEdit,     // When editing text
  NodeEdit,     // When editing nodes
  PlayMode      // During play mode
};

/**
 * @brief Category for organizing shortcuts
 */
enum class ShortcutCategory : u8 {
  File,
  Edit,
  View,
  Selection,
  Transform,
  Playback,
  Navigation,
  Tools,
  Window,
  Script,
  Build,
  Debug,
  Custom
};

/**
 * @brief Action callback type
 */
using ShortcutAction = std::function<void()>;
using ShortcutEnabledCheck = std::function<bool()>;

/**
 * @brief A registered shortcut command
 */
struct ShortcutCommand {
  std::string id;          // Unique identifier (e.g., "edit.undo")
  std::string displayName; // Display name (e.g., "Undo")
  std::string description; // Description for tooltips
  ShortcutCategory category = ShortcutCategory::Edit;
  ShortcutContext context = ShortcutContext::Global;
  Shortcut defaultBinding; // Default key binding
  Shortcut customBinding;  // User-customized binding (if different)
  bool useCustomBinding = false;
  ShortcutAction action;
  ShortcutEnabledCheck isEnabled = []() { return true; };
};

/**
 * @brief Hotkeys Manager - Central keyboard shortcuts management
 *
 * The HotkeysManager provides a complete system for keyboard shortcuts:
 *
 * 1. Register commands with unique IDs
 * 2. Assign default key bindings
 * 3. Allow user customization
 * 4. Handle key events and dispatch actions
 * 5. Detect and resolve conflicts
 * 6. Persist user preferences
 *
 * Example usage:
 * @code
 * auto& hotkeys = HotkeysManager::instance();
 *
 * // Register commands
 * hotkeys.registerCommand({
 *     "edit.undo",
 *     "Undo",
 *     "Undo the last action",
 *     ShortcutCategory::Edit,
 *     ShortcutContext::Global,
 *     {KeyCode::Z, Modifiers::Ctrl},
 *     {},
 *     false,
 *     [&]() { undoSystem.undo(); }
 * });
 *
 * // Handle key event
 * if (hotkeys.handleKeyEvent(keyCode, modifiers)) {
 *     // Shortcut was triggered
 * }
 *
 * // User customization
 * hotkeys.setCustomBinding("edit.undo", {KeyCode::Z, Modifiers::CtrlShift});
 * @endcode
 */
class HotkeysManager {
public:
  HotkeysManager();
  ~HotkeysManager();

  // Prevent copying
  HotkeysManager(const HotkeysManager &) = delete;
  HotkeysManager &operator=(const HotkeysManager &) = delete;

  /**
   * @brief Get singleton instance
   */
  static HotkeysManager &instance();

  // =========================================================================
  // Command Registration
  // =========================================================================

  /**
   * @brief Register a shortcut command
   */
  void registerCommand(const ShortcutCommand &command);

  /**
   * @brief Unregister a command
   */
  void unregisterCommand(const std::string &commandId);

  /**
   * @brief Check if command exists
   */
  [[nodiscard]] bool hasCommand(const std::string &commandId) const;

  /**
   * @brief Get command by ID
   */
  [[nodiscard]] const ShortcutCommand *
  getCommand(const std::string &commandId) const;

  /**
   * @brief Get all commands
   */
  [[nodiscard]] std::vector<const ShortcutCommand *> getAllCommands() const;

  /**
   * @brief Get commands in a category
   */
  [[nodiscard]] std::vector<const ShortcutCommand *>
  getCommandsInCategory(ShortcutCategory category) const;

  // =========================================================================
  // Key Binding Management
  // =========================================================================

  /**
   * @brief Set custom binding for a command
   */
  void setCustomBinding(const std::string &commandId, const Shortcut &binding);

  /**
   * @brief Clear custom binding (revert to default)
   */
  void clearCustomBinding(const std::string &commandId);

  /**
   * @brief Get effective binding for a command
   */
  [[nodiscard]] Shortcut getBinding(const std::string &commandId) const;

  /**
   * @brief Get command ID bound to a key (if any)
   */
  [[nodiscard]] std::optional<std::string>
  getCommandForBinding(const Shortcut &binding, ShortcutContext context) const;

  /**
   * @brief Check for binding conflicts
   */
  [[nodiscard]] std::vector<std::string>
  getConflicts(const std::string &commandId, const Shortcut &binding) const;

  /**
   * @brief Reset all bindings to defaults
   */
  void resetAllToDefaults();

  // =========================================================================
  // Event Handling
  // =========================================================================

  /**
   * @brief Set current context
   */
  void setCurrentContext(ShortcutContext context);

  /**
   * @brief Get current context
   */
  [[nodiscard]] ShortcutContext getCurrentContext() const {
    return m_currentContext;
  }

  /**
   * @brief Handle a key event
   * @param key Key code
   * @param modifiers Active modifiers
   * @return true if shortcut was triggered
   */
  bool handleKeyEvent(KeyCode key, Modifiers modifiers);

  /**
   * @brief Handle a key down event (SDL-style)
   */
  bool handleKeyDown(i32 keyCode, bool ctrl, bool shift, bool alt, bool meta);

  /**
   * @brief Execute a command by ID
   */
  bool executeCommand(const std::string &commandId);

  // =========================================================================
  // Persistence
  // =========================================================================

  /**
   * @brief Save custom bindings to file
   */
  Result<void> saveBindings(const std::string &filepath);

  /**
   * @brief Load custom bindings from file
   */
  Result<void> loadBindings(const std::string &filepath);

  // =========================================================================
  // Built-in Commands Registration
  // =========================================================================

  /**
   * @brief Register all standard editor commands
   */
  void registerStandardCommands();

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Get human-readable name for key code
   */
  [[nodiscard]] static std::string keyCodeToString(KeyCode key);

  /**
   * @brief Get human-readable name for modifiers
   */
  [[nodiscard]] static std::string modifiersToString(Modifiers mods);

  /**
   * @brief Get human-readable name for category
   */
  [[nodiscard]] static std::string categoryToString(ShortcutCategory category);

  /**
   * @brief Get human-readable name for context
   */
  [[nodiscard]] static std::string contextToString(ShortcutContext context);

private:
  void rebuildBindingMap();
  bool isContextActive(ShortcutContext context) const;

  // Commands by ID
  std::unordered_map<std::string, ShortcutCommand> m_commands;

  // Quick lookup: binding -> command ID (per context)
  std::unordered_map<Shortcut, std::string, ShortcutHash> m_globalBindings;
  std::unordered_map<ShortcutContext,
                     std::unordered_map<Shortcut, std::string, ShortcutHash>>
      m_contextBindings;
  bool m_bindingMapDirty = true;

  // Current context
  ShortcutContext m_currentContext = ShortcutContext::Editor;

  // Singleton
  static std::unique_ptr<HotkeysManager> s_instance;
};

// ============================================================================
// Standard Command IDs
// ============================================================================

namespace Commands {
// File
constexpr const char *FileNew = "file.new";
constexpr const char *FileOpen = "file.open";
constexpr const char *FileSave = "file.save";
constexpr const char *FileSaveAs = "file.save_as";
constexpr const char *FileSaveAll = "file.save_all";
constexpr const char *FileClose = "file.close";
constexpr const char *FileExport = "file.export";
constexpr const char *FileQuit = "file.quit";

// Edit
constexpr const char *EditUndo = "edit.undo";
constexpr const char *EditRedo = "edit.redo";
constexpr const char *EditCut = "edit.cut";
constexpr const char *EditCopy = "edit.copy";
constexpr const char *EditPaste = "edit.paste";
constexpr const char *EditDelete = "edit.delete";
constexpr const char *EditDuplicate = "edit.duplicate";
constexpr const char *EditSelectAll = "edit.select_all";
constexpr const char *EditFind = "edit.find";
constexpr const char *EditFindReplace = "edit.find_replace";
constexpr const char *EditRename = "edit.rename";

// View
constexpr const char *ViewZoomIn = "view.zoom_in";
constexpr const char *ViewZoomOut = "view.zoom_out";
constexpr const char *ViewZoomFit = "view.zoom_fit";
constexpr const char *ViewZoomReset = "view.zoom_reset";
constexpr const char *ViewFullscreen = "view.fullscreen";
constexpr const char *ViewGrid = "view.grid";
constexpr const char *ViewSnapping = "view.snapping";

// Selection
constexpr const char *SelectionClear = "selection.clear";
constexpr const char *SelectionInvert = "selection.invert";
constexpr const char *SelectionFocus = "selection.focus";
constexpr const char *SelectionParent = "selection.parent";
constexpr const char *SelectionChildren = "selection.children";

// Transform
constexpr const char *TransformMove = "transform.move";
constexpr const char *TransformRotate = "transform.rotate";
constexpr const char *TransformScale = "transform.scale";
constexpr const char *TransformReset = "transform.reset";

// Playback
constexpr const char *PlaybackPlay = "playback.play";
constexpr const char *PlaybackPause = "playback.pause";
constexpr const char *PlaybackStop = "playback.stop";
constexpr const char *PlaybackStepForward = "playback.step_forward";
constexpr const char *PlaybackStepBackward = "playback.step_backward";
constexpr const char *PlaybackToggle = "playback.toggle";

// Navigation
constexpr const char *NavGoToStart = "nav.go_to_start";
constexpr const char *NavGoToEnd = "nav.go_to_end";
constexpr const char *NavGoToSelection = "nav.go_to_selection";
constexpr const char *NavGoBack = "nav.go_back";
constexpr const char *NavGoForward = "nav.go_forward";

// Window/Panels
constexpr const char *WindowSceneView = "window.scene_view";
constexpr const char *WindowStoryGraph = "window.story_graph";
constexpr const char *WindowTimeline = "window.timeline";
constexpr const char *WindowInspector = "window.inspector";
constexpr const char *WindowHierarchy = "window.hierarchy";
constexpr const char *WindowAssetBrowser = "window.asset_browser";
constexpr const char *WindowConsole = "window.console";
constexpr const char *WindowVoiceManager = "window.voice_manager";
constexpr const char *WindowLocalization = "window.localization";
constexpr const char *WindowCurveEditor = "window.curve_editor";
constexpr const char *WindowBuildSettings = "window.build_settings";
constexpr const char *WindowSettings = "window.settings";
constexpr const char *WindowSwitchPanel = "window.switch_panel";

// Debug
constexpr const char *DebugToggleBreakpoint = "debug.toggle_breakpoint";
constexpr const char *DebugContinue = "debug.continue";
constexpr const char *DebugStepOver = "debug.step_over";
constexpr const char *DebugStepInto = "debug.step_into";
constexpr const char *DebugStepOut = "debug.step_out";

// Script Operations
constexpr const char *ScriptFormat = "script.format";
constexpr const char *ScriptGoToDefinition = "script.go_to_definition";
constexpr const char *ScriptFindReferences = "script.find_references";
constexpr const char *ScriptShowOutline = "script.show_outline";
constexpr const char *ScriptToggleComment = "script.toggle_comment";
constexpr const char *ScriptRun = "script.run";
constexpr const char *ScriptCompile = "script.compile";

// Build Operations
constexpr const char *BuildProject = "build.project";
constexpr const char *BuildRun = "build.run";
constexpr const char *BuildClean = "build.clean";
constexpr const char *BuildSettings = "build.settings";
} // namespace Commands

} // namespace NovelMind::editor
