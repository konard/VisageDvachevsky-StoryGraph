/**
 * @file hotkeys_manager.cpp
 * @brief Hotkeys Manager implementation
 */

#include "NovelMind/editor/hotkeys_manager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<HotkeysManager> HotkeysManager::s_instance = nullptr;

HotkeysManager::HotkeysManager() {}

HotkeysManager::~HotkeysManager() {}

HotkeysManager &HotkeysManager::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<HotkeysManager>();
  }
  return *s_instance;
}

void HotkeysManager::registerCommand(const ShortcutCommand &command) {
  m_commands[command.id] = command;
  m_bindingMapDirty = true;
}

void HotkeysManager::unregisterCommand(const std::string &commandId) {
  m_commands.erase(commandId);
  m_bindingMapDirty = true;
}

bool HotkeysManager::hasCommand(const std::string &commandId) const {
  return m_commands.find(commandId) != m_commands.end();
}

const ShortcutCommand *
HotkeysManager::getCommand(const std::string &commandId) const {
  auto it = m_commands.find(commandId);
  return (it != m_commands.end()) ? &it->second : nullptr;
}

std::vector<const ShortcutCommand *> HotkeysManager::getAllCommands() const {
  std::vector<const ShortcutCommand *> result;
  result.reserve(m_commands.size());

  for (const auto &[id, cmd] : m_commands) {
    result.push_back(&cmd);
  }

  // Sort by category, then by display name
  std::sort(result.begin(), result.end(),
            [](const ShortcutCommand *a, const ShortcutCommand *b) {
              if (a->category != b->category) {
                return static_cast<u8>(a->category) <
                       static_cast<u8>(b->category);
              }
              return a->displayName < b->displayName;
            });

  return result;
}

std::vector<const ShortcutCommand *>
HotkeysManager::getCommandsInCategory(ShortcutCategory category) const {
  std::vector<const ShortcutCommand *> result;

  for (const auto &[id, cmd] : m_commands) {
    if (cmd.category == category) {
      result.push_back(&cmd);
    }
  }

  std::sort(result.begin(), result.end(),
            [](const ShortcutCommand *a, const ShortcutCommand *b) {
              return a->displayName < b->displayName;
            });

  return result;
}

void HotkeysManager::setCustomBinding(const std::string &commandId,
                                      const Shortcut &binding) {
  auto it = m_commands.find(commandId);
  if (it != m_commands.end()) {
    it->second.customBinding = binding;
    it->second.useCustomBinding = true;
    m_bindingMapDirty = true;
  }
}

void HotkeysManager::clearCustomBinding(const std::string &commandId) {
  auto it = m_commands.find(commandId);
  if (it != m_commands.end()) {
    it->second.useCustomBinding = false;
    m_bindingMapDirty = true;
  }
}

Shortcut HotkeysManager::getBinding(const std::string &commandId) const {
  auto it = m_commands.find(commandId);
  if (it == m_commands.end()) {
    return {};
  }

  return it->second.useCustomBinding ? it->second.customBinding
                                     : it->second.defaultBinding;
}

std::optional<std::string>
HotkeysManager::getCommandForBinding(const Shortcut &binding,
                                     ShortcutContext context) const {
  if (m_bindingMapDirty) {
    const_cast<HotkeysManager *>(this)->rebuildBindingMap();
  }

  // Check context-specific bindings first
  auto contextIt = m_contextBindings.find(context);
  if (contextIt != m_contextBindings.end()) {
    auto bindingIt = contextIt->second.find(binding);
    if (bindingIt != contextIt->second.end()) {
      return bindingIt->second;
    }
  }

  // Check global bindings
  auto globalIt = m_globalBindings.find(binding);
  if (globalIt != m_globalBindings.end()) {
    return globalIt->second;
  }

  return std::nullopt;
}

std::vector<std::string>
HotkeysManager::getConflicts(const std::string &commandId,
                             const Shortcut &binding) const {
  std::vector<std::string> conflicts;

  auto cmd = getCommand(commandId);
  if (!cmd)
    return conflicts;

  for (const auto &[id, other] : m_commands) {
    if (id == commandId)
      continue;

    Shortcut otherBinding =
        other.useCustomBinding ? other.customBinding : other.defaultBinding;
    if (otherBinding == binding) {
      // Check if contexts overlap
      if (cmd->context == ShortcutContext::Global ||
          other.context == ShortcutContext::Global ||
          cmd->context == other.context) {
        conflicts.push_back(id);
      }
    }
  }

  return conflicts;
}

void HotkeysManager::resetAllToDefaults() {
  for (auto &[id, cmd] : m_commands) {
    cmd.useCustomBinding = false;
  }
  m_bindingMapDirty = true;
}

void HotkeysManager::setCurrentContext(ShortcutContext context) {
  m_currentContext = context;
}

bool HotkeysManager::handleKeyEvent(KeyCode key, Modifiers modifiers) {
  Shortcut binding(key, modifiers);

  // Look up command
  auto cmdId = getCommandForBinding(binding, m_currentContext);
  if (!cmdId) {
    return false;
  }

  return executeCommand(*cmdId);
}

bool HotkeysManager::handleKeyDown(i32 keyCode, bool ctrl, bool shift, bool alt,
                                   bool meta) {
  KeyCode key = static_cast<KeyCode>(keyCode);

  Modifiers mods = Modifiers::None;
  if (ctrl)
    mods = mods | Modifiers::Ctrl;
  if (shift)
    mods = mods | Modifiers::Shift;
  if (alt)
    mods = mods | Modifiers::Alt;
  if (meta)
    mods = mods | Modifiers::Meta;

  return handleKeyEvent(key, mods);
}

bool HotkeysManager::executeCommand(const std::string &commandId) {
  auto cmd = getCommand(commandId);
  if (!cmd) {
    return false;
  }

  // Check if command is enabled
  if (cmd->isEnabled && !cmd->isEnabled()) {
    return false;
  }

  // Check if context is active
  if (!isContextActive(cmd->context)) {
    return false;
  }

  // Execute action
  if (cmd->action) {
    cmd->action();
    return true;
  }

  return false;
}

Result<void> HotkeysManager::saveBindings(const std::string &filepath) {
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + filepath);
  }

  // Simple format: commandId=binding
  for (const auto &[id, cmd] : m_commands) {
    if (cmd.useCustomBinding) {
      file << id << "=" << cmd.customBinding.toString() << "\n";
    }
  }

  return Result<void>::ok();
}

Result<void> HotkeysManager::loadBindings(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for reading: " + filepath);
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;

    std::string cmdId = line.substr(0, pos);
    std::string bindingStr = line.substr(pos + 1);

    Shortcut binding = Shortcut::fromString(bindingStr);
    if (binding.isValid()) {
      setCustomBinding(cmdId, binding);
    }
  }

  return Result<void>::ok();
}

void HotkeysManager::registerStandardCommands() {
  // File commands
  registerCommand({Commands::FileSave,
                   "Save",
                   "Save the current file",
                   ShortcutCategory::File,
                   ShortcutContext::Global,
                   {KeyCode::S, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::FileSaveAs,
                   "Save As",
                   "Save the current file with a new name",
                   ShortcutCategory::File,
                   ShortcutContext::Global,
                   {KeyCode::S, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::FileNew,
                   "New",
                   "Create a new project",
                   ShortcutCategory::File,
                   ShortcutContext::Global,
                   {KeyCode::N, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::FileOpen,
                   "Open",
                   "Open a project",
                   ShortcutCategory::File,
                   ShortcutContext::Global,
                   {KeyCode::O, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  // Edit commands
  registerCommand({Commands::EditUndo,
                   "Undo",
                   "Undo the last action",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::Z, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditRedo,
                   "Redo",
                   "Redo the last undone action",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::Y, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditCut,
                   "Cut",
                   "Cut selection to clipboard",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::X, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditCopy,
                   "Copy",
                   "Copy selection to clipboard",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::C, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditPaste,
                   "Paste",
                   "Paste from clipboard",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::V, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditDelete,
                   "Delete",
                   "Delete selection",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::Delete, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditDuplicate,
                   "Duplicate",
                   "Duplicate selection",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::D, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditSelectAll,
                   "Select All",
                   "Select all items",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::A, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditFind,
                   "Find",
                   "Open find dialog",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::F, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::EditRename,
                   "Rename",
                   "Rename selected item",
                   ShortcutCategory::Edit,
                   ShortcutContext::Global,
                   {KeyCode::F2, Modifiers::None},
                   {},
                   false,
                   nullptr});

  // Playback commands
  registerCommand({Commands::PlaybackToggle,
                   "Play/Pause",
                   "Toggle play mode",
                   ShortcutCategory::Playback,
                   ShortcutContext::Global,
                   {KeyCode::F5, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::PlaybackStop,
                   "Stop",
                   "Stop play mode",
                   ShortcutCategory::Playback,
                   ShortcutContext::Global,
                   {KeyCode::F5, Modifiers::Shift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::PlaybackStepForward,
                   "Step Forward",
                   "Step to next dialogue/choice",
                   ShortcutCategory::Playback,
                   ShortcutContext::PlayMode,
                   {KeyCode::F10, Modifiers::None},
                   {},
                   false,
                   nullptr});

  // View commands
  registerCommand({Commands::ViewZoomIn,
                   "Zoom In",
                   "Zoom in",
                   ShortcutCategory::View,
                   ShortcutContext::Global,
                   {KeyCode::Plus, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ViewZoomOut,
                   "Zoom Out",
                   "Zoom out",
                   ShortcutCategory::View,
                   ShortcutContext::Global,
                   {KeyCode::Minus, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ViewZoomReset,
                   "Reset Zoom",
                   "Reset zoom to 100%",
                   ShortcutCategory::View,
                   ShortcutContext::Global,
                   {KeyCode::Num0, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ViewZoomFit,
                   "Fit to View",
                   "Fit content to view",
                   ShortcutCategory::View,
                   ShortcutContext::Global,
                   {KeyCode::F, Modifiers::None},
                   {},
                   false,
                   nullptr});

  // Transform/Gizmo commands
  registerCommand({Commands::TransformMove,
                   "Move Tool",
                   "Switch to move/translate gizmo",
                   ShortcutCategory::Transform,
                   ShortcutContext::SceneView,
                   {KeyCode::W, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::TransformRotate,
                   "Rotate Tool",
                   "Switch to rotate gizmo",
                   ShortcutCategory::Transform,
                   ShortcutContext::SceneView,
                   {KeyCode::E, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::TransformScale,
                   "Scale Tool",
                   "Switch to scale gizmo",
                   ShortcutCategory::Transform,
                   ShortcutContext::SceneView,
                   {KeyCode::R, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::TransformReset,
                   "Reset Transform",
                   "Reset selected object transform",
                   ShortcutCategory::Transform,
                   ShortcutContext::SceneView,
                   {KeyCode::T, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  // Window/Panel commands
  registerCommand({Commands::WindowSwitchPanel,
                   "Switch Panel",
                   "Quick switch between panels",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::P, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowConsole,
                   "Console",
                   "Show/hide console panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Backquote, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowInspector,
                   "Inspector",
                   "Show/hide inspector panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::I, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowHierarchy,
                   "Hierarchy",
                   "Show/hide hierarchy panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::H, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowSceneView,
                   "Scene View",
                   "Show/hide scene view panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Num1, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowStoryGraph,
                   "Story Graph",
                   "Show/hide story graph panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Num2, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowTimeline,
                   "Timeline",
                   "Show/hide timeline panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Num3, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowAssetBrowser,
                   "Asset Browser",
                   "Show/hide asset browser panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Num4, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowVoiceManager,
                   "Voice Manager",
                   "Show/hide voice manager panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Num5, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowLocalization,
                   "Localization",
                   "Show/hide localization panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::L, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowCurveEditor,
                   "Curve Editor",
                   "Show/hide curve editor panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::C, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowBuildSettings,
                   "Build Settings",
                   "Show/hide build settings panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::B, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::WindowSettings,
                   "Project Settings",
                   "Show/hide project settings panel",
                   ShortcutCategory::Window,
                   ShortcutContext::Global,
                   {KeyCode::Comma, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  // Debug commands
  registerCommand({Commands::DebugToggleBreakpoint,
                   "Toggle Breakpoint",
                   "Toggle breakpoint at current node",
                   ShortcutCategory::Debug,
                   ShortcutContext::StoryGraph,
                   {KeyCode::F9, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::DebugContinue,
                   "Continue",
                   "Continue execution",
                   ShortcutCategory::Debug,
                   ShortcutContext::PlayMode,
                   {KeyCode::F5, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::DebugStepOver,
                   "Step Over",
                   "Step over current instruction",
                   ShortcutCategory::Debug,
                   ShortcutContext::PlayMode,
                   {KeyCode::F10, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::DebugStepInto,
                   "Step Into",
                   "Step into function",
                   ShortcutCategory::Debug,
                   ShortcutContext::PlayMode,
                   {KeyCode::F11, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::DebugStepOut,
                   "Step Out",
                   "Step out of function",
                   ShortcutCategory::Debug,
                   ShortcutContext::PlayMode,
                   {KeyCode::F11, Modifiers::Shift},
                   {},
                   false,
                   nullptr});

  // Script operation commands
  registerCommand({Commands::ScriptFormat,
                   "Format Script",
                   "Auto-format current script",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::K, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptGoToDefinition,
                   "Go to Definition",
                   "Go to symbol definition",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::F12, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptFindReferences,
                   "Find References",
                   "Find all references to symbol",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::F12, Modifiers::Shift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptShowOutline,
                   "Show Outline",
                   "Show script symbol outline",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::O, Modifiers::CtrlShift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptToggleComment,
                   "Toggle Comment",
                   "Toggle line/block comment",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::Slash, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptRun,
                   "Run Script",
                   "Run current script",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::F6, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::ScriptCompile,
                   "Compile Script",
                   "Compile current script",
                   ShortcutCategory::Script,
                   ShortcutContext::TextEdit,
                   {KeyCode::F6, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  // Build operation commands
  registerCommand({Commands::BuildProject,
                   "Build Project",
                   "Build the entire project",
                   ShortcutCategory::Build,
                   ShortcutContext::Global,
                   {KeyCode::F7, Modifiers::None},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::BuildRun,
                   "Build and Run",
                   "Build project and run",
                   ShortcutCategory::Build,
                   ShortcutContext::Global,
                   {KeyCode::F7, Modifiers::Ctrl},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::BuildClean,
                   "Clean Build",
                   "Clean build artifacts",
                   ShortcutCategory::Build,
                   ShortcutContext::Global,
                   {KeyCode::F7, Modifiers::Shift},
                   {},
                   false,
                   nullptr});

  registerCommand({Commands::BuildSettings,
                   "Build Settings",
                   "Open build settings",
                   ShortcutCategory::Build,
                   ShortcutContext::Global,
                   {KeyCode::B, Modifiers::CtrlAlt},
                   {},
                   false,
                   nullptr});
}

void HotkeysManager::rebuildBindingMap() {
  m_globalBindings.clear();
  m_contextBindings.clear();

  for (const auto &[id, cmd] : m_commands) {
    Shortcut binding =
        cmd.useCustomBinding ? cmd.customBinding : cmd.defaultBinding;
    if (!binding.isValid())
      continue;

    if (cmd.context == ShortcutContext::Global) {
      m_globalBindings[binding] = id;
    } else {
      m_contextBindings[cmd.context][binding] = id;
    }
  }

  m_bindingMapDirty = false;
}

bool HotkeysManager::isContextActive(ShortcutContext context) const {
  if (context == ShortcutContext::Global) {
    return true;
  }

  // Could implement more sophisticated context checking
  // For now, just check if current context matches
  return m_currentContext == context;
}

// ============================================================================
// Shortcut Implementation
// ============================================================================

std::string Shortcut::toString() const {
  std::string result;

  if (hasModifier(modifiers, Modifiers::Ctrl)) {
    result += "Ctrl+";
  }
  if (hasModifier(modifiers, Modifiers::Shift)) {
    result += "Shift+";
  }
  if (hasModifier(modifiers, Modifiers::Alt)) {
    result += "Alt+";
  }
  if (hasModifier(modifiers, Modifiers::Meta)) {
#ifdef __APPLE__
    result += "Cmd+";
#else
    result += "Win+";
#endif
  }

  result += HotkeysManager::keyCodeToString(key);
  return result;
}

Shortcut Shortcut::fromString(const std::string &str) {
  Shortcut result;

  std::string remaining = str;
  std::string delimiter = "+";

  while (true) {
    auto pos = remaining.find(delimiter);
    std::string part =
        (pos != std::string::npos) ? remaining.substr(0, pos) : remaining;

    // Convert to lowercase for comparison
    std::string lower = part;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "ctrl") {
      result.modifiers = result.modifiers | Modifiers::Ctrl;
    } else if (lower == "shift") {
      result.modifiers = result.modifiers | Modifiers::Shift;
    } else if (lower == "alt") {
      result.modifiers = result.modifiers | Modifiers::Alt;
    } else if (lower == "meta" || lower == "cmd" || lower == "win") {
      result.modifiers = result.modifiers | Modifiers::Meta;
    } else {
      // This is the key
      // Simple mapping for common keys
      if (lower.length() == 1) {
        result.key = static_cast<KeyCode>(lower[0]);
      } else if (lower == "space") {
        result.key = KeyCode::Space;
      } else if (lower == "enter" || lower == "return") {
        result.key = KeyCode::Enter;
      } else if (lower == "escape" || lower == "esc") {
        result.key = KeyCode::Escape;
      } else if (lower == "tab") {
        result.key = KeyCode::Tab;
      } else if (lower == "backspace") {
        result.key = KeyCode::Backspace;
      } else if (lower == "delete" || lower == "del") {
        result.key = KeyCode::Delete;
      } else if (lower == "insert" || lower == "ins") {
        result.key = KeyCode::Insert;
      } else if (lower == "home") {
        result.key = KeyCode::Home;
      } else if (lower == "end") {
        result.key = KeyCode::End;
      } else if (lower == "pageup" || lower == "pgup") {
        result.key = KeyCode::PageUp;
      } else if (lower == "pagedown" || lower == "pgdn") {
        result.key = KeyCode::PageDown;
      } else if (lower == "up") {
        result.key = KeyCode::Up;
      } else if (lower == "down") {
        result.key = KeyCode::Down;
      } else if (lower == "left") {
        result.key = KeyCode::Left;
      } else if (lower == "right") {
        result.key = KeyCode::Right;
      } else if (lower.length() >= 2 && lower[0] == 'f') {
        // F keys
        i32 fNum = std::stoi(lower.substr(1));
        if (fNum >= 1 && fNum <= 12) {
          result.key =
              static_cast<KeyCode>(static_cast<i32>(KeyCode::F1) + fNum - 1);
        }
      }
    }

    if (pos == std::string::npos)
      break;
    remaining = remaining.substr(pos + 1);
  }

  return result;
}

std::string HotkeysManager::keyCodeToString(KeyCode key) {
  // Letters and numbers
  if (key >= KeyCode::A && key <= KeyCode::Z) {
    return std::string(1,
                       static_cast<char>('A' + (static_cast<i32>(key) -
                                                static_cast<i32>(KeyCode::A))));
  }
  if (key >= KeyCode::Num0 && key <= KeyCode::Num9) {
    return std::string(
        1, static_cast<char>('0' + (static_cast<i32>(key) -
                                    static_cast<i32>(KeyCode::Num0))));
  }

  // Function keys
  if (key >= KeyCode::F1 && key <= KeyCode::F12) {
    return "F" + std::to_string(static_cast<i32>(key) -
                                static_cast<i32>(KeyCode::F1) + 1);
  }

  // Special keys
  switch (key) {
  case KeyCode::Space:
    return "Space";
  case KeyCode::Enter:
    return "Enter";
  case KeyCode::Escape:
    return "Escape";
  case KeyCode::Tab:
    return "Tab";
  case KeyCode::Backspace:
    return "Backspace";
  case KeyCode::Delete:
    return "Delete";
  case KeyCode::Insert:
    return "Insert";
  case KeyCode::Home:
    return "Home";
  case KeyCode::End:
    return "End";
  case KeyCode::PageUp:
    return "PageUp";
  case KeyCode::PageDown:
    return "PageDown";
  case KeyCode::Up:
    return "Up";
  case KeyCode::Down:
    return "Down";
  case KeyCode::Left:
    return "Left";
  case KeyCode::Right:
    return "Right";
  case KeyCode::Minus:
    return "-";
  case KeyCode::Plus:
    return "+";
  case KeyCode::LeftBracket:
    return "[";
  case KeyCode::RightBracket:
    return "]";
  case KeyCode::Semicolon:
    return ";";
  case KeyCode::Quote:
    return "'";
  case KeyCode::Backquote:
    return "`";
  case KeyCode::Comma:
    return ",";
  case KeyCode::Period:
    return ".";
  case KeyCode::Slash:
    return "/";
  case KeyCode::Backslash:
    return "\\";
  default:
    return "?";
  }
}

std::string HotkeysManager::modifiersToString(Modifiers mods) {
  std::string result;

  if (hasModifier(mods, Modifiers::Ctrl))
    result += "Ctrl+";
  if (hasModifier(mods, Modifiers::Shift))
    result += "Shift+";
  if (hasModifier(mods, Modifiers::Alt))
    result += "Alt+";
  if (hasModifier(mods, Modifiers::Meta)) {
#ifdef __APPLE__
    result += "Cmd+";
#else
    result += "Win+";
#endif
  }

  // Remove trailing +
  if (!result.empty() && result.back() == '+') {
    result.pop_back();
  }

  return result;
}

std::string HotkeysManager::categoryToString(ShortcutCategory category) {
  switch (category) {
  case ShortcutCategory::File:
    return "File";
  case ShortcutCategory::Edit:
    return "Edit";
  case ShortcutCategory::View:
    return "View";
  case ShortcutCategory::Selection:
    return "Selection";
  case ShortcutCategory::Transform:
    return "Transform";
  case ShortcutCategory::Playback:
    return "Playback";
  case ShortcutCategory::Navigation:
    return "Navigation";
  case ShortcutCategory::Tools:
    return "Tools";
  case ShortcutCategory::Window:
    return "Window";
  case ShortcutCategory::Script:
    return "Script";
  case ShortcutCategory::Build:
    return "Build";
  case ShortcutCategory::Debug:
    return "Debug";
  case ShortcutCategory::Custom:
    return "Custom";
  default:
    return "Unknown";
  }
}

std::string HotkeysManager::contextToString(ShortcutContext context) {
  switch (context) {
  case ShortcutContext::Global:
    return "Global";
  case ShortcutContext::Editor:
    return "Editor";
  case ShortcutContext::SceneView:
    return "Scene View";
  case ShortcutContext::StoryGraph:
    return "Story Graph";
  case ShortcutContext::Timeline:
    return "Timeline";
  case ShortcutContext::Inspector:
    return "Inspector";
  case ShortcutContext::AssetBrowser:
    return "Asset Browser";
  case ShortcutContext::Hierarchy:
    return "Hierarchy";
  case ShortcutContext::Console:
    return "Console";
  case ShortcutContext::TextEdit:
    return "Text Edit";
  case ShortcutContext::NodeEdit:
    return "Node Edit";
  case ShortcutContext::PlayMode:
    return "Play Mode";
  default:
    return "Unknown";
  }
}

} // namespace NovelMind::editor
