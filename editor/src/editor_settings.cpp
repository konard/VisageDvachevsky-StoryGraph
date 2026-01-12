/**
 * @file editor_settings.cpp
 * @brief Editor Settings implementation
 */

#include "NovelMind/editor/editor_settings.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace NovelMind::editor {

namespace fs = std::filesystem;

// ============================================================================
// KeyBinding
// ============================================================================

std::string KeyBinding::toString() const {
  std::string result;

  if ((modifiers & KeyModifier::Ctrl) == KeyModifier::Ctrl)
    result += "Ctrl+";
  if ((modifiers & KeyModifier::Shift) == KeyModifier::Shift)
    result += "Shift+";
  if ((modifiers & KeyModifier::Alt) == KeyModifier::Alt)
    result += "Alt+";
  if ((modifiers & KeyModifier::Super) == KeyModifier::Super)
    result += "Super+";

  // Convert key code to name (simplified)
  if (keyCode >= 'A' && keyCode <= 'Z') {
    result += static_cast<char>(keyCode);
  } else if (keyCode >= '0' && keyCode <= '9') {
    result += static_cast<char>(keyCode);
  } else {
    result += std::to_string(keyCode);
  }

  return result;
}

KeyBinding KeyBinding::fromString(const std::string& str) {
  KeyBinding binding;

  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);

  if (s.find("CTRL+") != std::string::npos) {
    binding.modifiers = binding.modifiers | KeyModifier::Ctrl;
    s.erase(s.find("CTRL+"), 5);
  }
  if (s.find("SHIFT+") != std::string::npos) {
    binding.modifiers = binding.modifiers | KeyModifier::Shift;
    s.erase(s.find("SHIFT+"), 6);
  }
  if (s.find("ALT+") != std::string::npos) {
    binding.modifiers = binding.modifiers | KeyModifier::Alt;
    s.erase(s.find("ALT+"), 4);
  }
  if (s.find("SUPER+") != std::string::npos) {
    binding.modifiers = binding.modifiers | KeyModifier::Super;
    s.erase(s.find("SUPER+"), 6);
  }

  if (!s.empty()) {
    if (s.length() == 1) {
      binding.keyCode = s[0];
    } else {
      try {
        binding.keyCode = std::stoi(s);
      } catch (...) {
        binding.keyCode = 0;
      }
    }
  }

  return binding;
}

// ============================================================================
// LayoutManager
// ============================================================================

LayoutManager::LayoutManager() {}

void LayoutManager::initialize(EditorApp* editor) {
  m_editor = editor;
}

Result<void> LayoutManager::saveLayout(const std::string& name) {
  std::unique_lock lock(m_mutex);
  if (!m_editor) {
    return Result<void>::error("Editor not initialized");
  }

  EditorLayout layout = captureCurrentLayout();
  layout.name = name;
  m_savedLayouts[name] = layout;

  // Save to file
  if (!m_layoutsPath.empty()) {
    try {
      fs::create_directories(m_layoutsPath);
      std::string filePath = m_layoutsPath + "/" + name + ".layout";

      std::ofstream file(filePath);
      if (!file) {
        return Result<void>::error("Failed to open layout file for writing");
      }

      // Write layout data (simplified)
      file << "[Layout]\n";
      file << "name=" << layout.name << "\n";
      file << "width=" << layout.mainWindowWidth << "\n";
      file << "height=" << layout.mainWindowHeight << "\n";
      file << "maximized=" << (layout.maximized ? "true" : "false") << "\n";

      for (const auto& panel : layout.panels) {
        file << "\n[Panel:" << panel.name << "]\n";
        file << "visible=" << (panel.visible ? "true" : "false") << "\n";
        file << "x=" << panel.x << "\n";
        file << "y=" << panel.y << "\n";
        file << "width=" << panel.width << "\n";
        file << "height=" << panel.height << "\n";
        file << "dock=" << panel.dockRegion << "\n";
      }
    } catch (const std::exception& e) {
      return Result<void>::error(std::string("Failed to save layout: ") + e.what());
    }
  }

  return Result<void>::ok();
}

Result<void> LayoutManager::loadLayout(const std::string& name) {
  std::unique_lock lock(m_mutex);
  auto it = m_savedLayouts.find(name);
  if (it != m_savedLayouts.end()) {
    applyLayout(it->second);
    return Result<void>::ok();
  }

  // Try loading from file
  if (!m_layoutsPath.empty()) {
    std::string filePath = m_layoutsPath + "/" + name + ".layout";
    if (fs::exists(filePath)) {
      // Load layout file (simplified)
      EditorLayout layout;
      layout.name = name;
      // Would parse file here
      m_savedLayouts[name] = layout;
      applyLayout(layout);
      return Result<void>::ok();
    }
  }

  return Result<void>::error("Layout not found: " + name);
}

void LayoutManager::applyPreset(LayoutPreset preset) {
  std::unique_lock lock(m_mutex);
  EditorLayout layout = createPresetLayout(preset);
  applyLayout(layout);
}

EditorLayout LayoutManager::getCurrentLayout() const {
  std::shared_lock lock(m_mutex);
  if (m_editor) {
    return const_cast<LayoutManager*>(this)->captureCurrentLayout();
  }
  return EditorLayout();
}

std::vector<std::string> LayoutManager::getSavedLayouts() const {
  std::shared_lock lock(m_mutex);
  std::vector<std::string> names;
  for (const auto& [name, layout] : m_savedLayouts) {
    names.push_back(name);
  }
  return names;
}

void LayoutManager::deleteLayout(const std::string& name) {
  std::unique_lock lock(m_mutex);
  m_savedLayouts.erase(name);

  if (!m_layoutsPath.empty()) {
    std::string filePath = m_layoutsPath + "/" + name + ".layout";
    fs::remove(filePath);
  }
}

Result<void> LayoutManager::exportLayout(const std::string& path) {
  std::shared_lock lock(m_mutex);
  EditorLayout layout = captureCurrentLayout();

  try {
    std::ofstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open file for writing");
    }

    // Write layout as JSON/ini format
    file << "[Layout]\n";
    file << "name=" << layout.name << "\n";
    // Write rest of layout data

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Export failed: ") + e.what());
  }
}

Result<void> LayoutManager::importLayout(const std::string& path) {
  std::unique_lock lock(m_mutex);
  if (!fs::exists(path)) {
    return Result<void>::error("File not found: " + path);
  }

  // Parse layout file
  EditorLayout layout;
  // Would parse file here
  layout.name = fs::path(path).stem().string();

  m_savedLayouts[layout.name] = layout;
  return Result<void>::ok();
}

void LayoutManager::setLayoutsPath(const std::string& path) {
  std::unique_lock lock(m_mutex);
  m_layoutsPath = path;
}

EditorLayout LayoutManager::createPresetLayout(LayoutPreset preset) {
  EditorLayout layout;

  switch (preset) {
  case LayoutPreset::Default:
    layout.name = "Default";
    layout.description = "Standard layout with all panels";
    layout.panels = {{"Project", true, true, 0, 0, 250, 400, "left", 0},
                     {"Scene", true, true, 250, 0, 800, 600, "center", 0},
                     {"Story", true, true, 250, 0, 800, 600, "center", 1},
                     {"Inspector", true, true, 1050, 0, 300, 400, "right", 0},
                     {"Assets", true, true, 0, 400, 600, 200, "bottom", 0},
                     {"Diagnostics", true, true, 0, 400, 600, 200, "bottom", 1}};
    break;

  case LayoutPreset::StoryFocused:
    layout.name = "Story Focused";
    layout.description = "Story graph prominently displayed";
    layout.panels = {{"Story", true, true, 0, 0, 1200, 700, "center", 0},
                     {"Inspector", true, true, 1200, 0, 300, 400, "right", 0},
                     {"Diagnostics", true, true, 0, 700, 600, 150, "bottom", 0}};
    break;

  case LayoutPreset::SceneFocused:
    layout.name = "Scene Focused";
    layout.description = "Scene view prominently displayed";
    layout.panels = {{"Scene", true, true, 0, 0, 1200, 700, "center", 0},
                     {"Inspector", true, true, 1200, 0, 300, 400, "right", 0},
                     {"Assets", true, true, 0, 700, 600, 150, "bottom", 0}};
    break;

  case LayoutPreset::ScriptFocused:
    layout.name = "Script Focused";
    layout.description = "Script editor prominently displayed";
    layout.panels = {{"Script", true, true, 0, 0, 1000, 700, "center", 0},
                     {"Diagnostics", true, true, 1000, 0, 400, 350, "right", 0},
                     {"Variables", true, true, 1000, 350, 400, 350, "right", 1}};
    break;

  case LayoutPreset::DebugLayout:
    layout.name = "Debug";
    layout.description = "Layout with debug panels visible";
    layout.panels = {{"Scene", true, true, 0, 0, 800, 500, "center", 0},
                     {"Story", true, true, 0, 0, 800, 500, "center", 1},
                     {"Inspector", true, true, 800, 0, 300, 250, "right", 0},
                     {"Variables", true, true, 800, 250, 300, 250, "right", 1},
                     {"CallStack", true, true, 800, 500, 300, 200, "right", 2},
                     {"Diagnostics", true, true, 0, 500, 500, 200, "bottom", 0},
                     {"Console", true, true, 500, 500, 300, 200, "bottom", 1}};
    break;

  case LayoutPreset::Minimal:
    layout.name = "Minimal";
    layout.description = "Minimal distraction layout";
    layout.panels = {{"Scene", true, true, 0, 0, 1400, 900, "center", 0}};
    break;

  default:
    layout.name = "Custom";
    break;
  }

  return layout;
}

void LayoutManager::applyLayout(const EditorLayout& layout) {
  if (!m_editor) {
    return;
  }

  // Apply panel states
  for (const auto& panelState : layout.panels) {
    // Would update actual panels here
    (void)panelState;
  }
}

EditorLayout LayoutManager::captureCurrentLayout() {
  EditorLayout layout;
  layout.name = "Current";

  // NOTE: EditorApp interface not yet implemented
  // This function requires complete type definition of EditorApp
  // which doesn't exist yet. Returning empty layout for now.
  (void)m_editor; // Suppress unused variable warning

  return layout;
}

// ============================================================================
// HotkeyManager
// ============================================================================

HotkeyManager::HotkeyManager() {}

void HotkeyManager::registerAction(const HotkeyAction& action, HotkeyCallback callback) {
  std::unique_lock lock(m_mutex);
  m_actions[action.id] = action;
  m_callbacks[action.id] = std::move(callback);
}

void HotkeyManager::unregisterAction(const std::string& actionId) {
  std::unique_lock lock(m_mutex);
  m_actions.erase(actionId);
  m_callbacks.erase(actionId);
}

void HotkeyManager::setBinding(const std::string& actionId, const KeyBinding& binding) {
  std::unique_lock lock(m_mutex);
  auto it = m_actions.find(actionId);
  if (it != m_actions.end()) {
    it->second.currentBinding = binding;
  }
}

void HotkeyManager::resetToDefault(const std::string& actionId) {
  std::unique_lock lock(m_mutex);
  auto it = m_actions.find(actionId);
  if (it != m_actions.end()) {
    it->second.currentBinding = it->second.defaultBinding;
  }
}

void HotkeyManager::resetAllToDefaults() {
  std::unique_lock lock(m_mutex);
  for (auto& [id, action] : m_actions) {
    action.currentBinding = action.defaultBinding;
  }
}

std::optional<HotkeyAction> HotkeyManager::getAction(const std::string& actionId) const {
  std::shared_lock lock(m_mutex);
  auto it = m_actions.find(actionId);
  if (it != m_actions.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<HotkeyAction> HotkeyManager::getActionsByCategory(ActionCategory category) const {
  std::shared_lock lock(m_mutex);
  std::vector<HotkeyAction> result;
  for (const auto& [id, action] : m_actions) {
    if (action.category == category) {
      result.push_back(action);
    }
  }
  return result;
}

const std::unordered_map<std::string, HotkeyAction>& HotkeyManager::getAllActions() const {
  std::shared_lock lock(m_mutex);
  return m_actions;
}

bool HotkeyManager::handleKeyPress(i32 keyCode, KeyModifier modifiers) {
  // Copy callback to avoid holding lock during execution
  HotkeyCallback callback;
  {
    std::shared_lock lock(m_mutex);
    KeyBinding pressed;
    pressed.keyCode = keyCode;
    pressed.modifiers = modifiers;

    for (const auto& [id, action] : m_actions) {
      if (action.enabled && action.currentBinding == pressed) {
        auto callbackIt = m_callbacks.find(id);
        if (callbackIt != m_callbacks.end() && callbackIt->second) {
          callback = callbackIt->second;
          break;
        }
      }
    }
  }

  // Execute callback without holding lock
  if (callback) {
    callback();
    return true;
  }

  return false;
}

std::vector<std::string> HotkeyManager::getConflicts(const KeyBinding& binding) const {
  std::shared_lock lock(m_mutex);
  std::vector<std::string> conflicts;
  for (const auto& [id, action] : m_actions) {
    if (action.currentBinding == binding) {
      conflicts.push_back(id);
    }
  }
  return conflicts;
}

Result<void> HotkeyManager::save(const std::string& path) {
  std::shared_lock lock(m_mutex);
  try {
    std::ofstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open file for writing");
    }

    file << "[Hotkeys]\n";
    for (const auto& [id, action] : m_actions) {
      file << id << "=" << action.currentBinding.toString() << "\n";
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to save hotkeys: ") + e.what());
  }
}

Result<void> HotkeyManager::load(const std::string& path) {
  std::unique_lock lock(m_mutex);
  if (!fs::exists(path)) {
    return Result<void>::error("File not found");
  }

  try {
    std::ifstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open file");
    }

    std::string line;
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '[' || line[0] == '#') {
        continue;
      }

      auto pos = line.find('=');
      if (pos != std::string::npos) {
        std::string actionId = line.substr(0, pos);
        std::string bindingStr = line.substr(pos + 1);

        auto it = m_actions.find(actionId);
        if (it != m_actions.end()) {
          it->second.currentBinding = KeyBinding::fromString(bindingStr);
        }
      }
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to load hotkeys: ") + e.what());
  }
}

void HotkeyManager::registerDefaultHotkeys(EditorApp* editor) {
  if (!editor) {
    return;
  }

  // File actions
  registerAction({"file.new_project",
                  "New Project",
                  "Create a new project",
                  ActionCategory::File,
                  {static_cast<i32>('N'), KeyModifier::Ctrl | KeyModifier::Shift},
                  {static_cast<i32>('N'), KeyModifier::Ctrl | KeyModifier::Shift}},
                 [editor]() { /* editor->newProject(); */
                              (void)editor;
                 });

  registerAction({"file.open_project",
                  "Open Project",
                  "Open an existing project",
                  ActionCategory::File,
                  {static_cast<i32>('O'), KeyModifier::Ctrl | KeyModifier::Shift},
                  {static_cast<i32>('O'), KeyModifier::Ctrl | KeyModifier::Shift}},
                 [editor]() { /* editor->openProject(); */
                              (void)editor;
                 });

  registerAction({"file.save",
                  "Save",
                  "Save current file",
                  ActionCategory::File,
                  {static_cast<i32>('S'), KeyModifier::Ctrl},
                  {static_cast<i32>('S'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->saveProject(); */
                              (void)editor;
                 });

  registerAction({"file.save_all",
                  "Save All",
                  "Save all open files",
                  ActionCategory::File,
                  {static_cast<i32>('S'), KeyModifier::Ctrl | KeyModifier::Shift},
                  {static_cast<i32>('S'), KeyModifier::Ctrl | KeyModifier::Shift}},
                 [editor]() { /* editor->saveProject(); */
                              (void)editor;
                 });

  // Edit actions
  registerAction({"edit.undo",
                  "Undo",
                  "Undo last action",
                  ActionCategory::Edit,
                  {static_cast<i32>('Z'), KeyModifier::Ctrl},
                  {static_cast<i32>('Z'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->undo(); */
                              (void)editor;
                 });

  registerAction({"edit.redo",
                  "Redo",
                  "Redo last undone action",
                  ActionCategory::Edit,
                  {static_cast<i32>('Y'), KeyModifier::Ctrl},
                  {static_cast<i32>('Y'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->redo(); */
                              (void)editor;
                 });

  registerAction({"edit.cut",
                  "Cut",
                  "Cut selection",
                  ActionCategory::Edit,
                  {static_cast<i32>('X'), KeyModifier::Ctrl},
                  {static_cast<i32>('X'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->cut(); */
                              (void)editor;
                 });

  registerAction({"edit.copy",
                  "Copy",
                  "Copy selection",
                  ActionCategory::Edit,
                  {static_cast<i32>('C'), KeyModifier::Ctrl},
                  {static_cast<i32>('C'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->copy(); */
                              (void)editor;
                 });

  registerAction({"edit.paste",
                  "Paste",
                  "Paste from clipboard",
                  ActionCategory::Edit,
                  {static_cast<i32>('V'), KeyModifier::Ctrl},
                  {static_cast<i32>('V'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->paste(); */
                              (void)editor;
                 });

  registerAction({"edit.delete",
                  "Delete",
                  "Delete selection",
                  ActionCategory::Edit,
                  {127, KeyModifier::None}, // Delete key
                  {127, KeyModifier::None}},
                 [editor]() { /* editor->deleteSelection(); */
                              (void)editor;
                 });

  registerAction({"edit.select_all",
                  "Select All",
                  "Select all",
                  ActionCategory::Edit,
                  {static_cast<i32>('A'), KeyModifier::Ctrl},
                  {static_cast<i32>('A'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->selectAll(); */
                              (void)editor;
                 });

  // Play actions
  registerAction({"play.start",
                  "Play",
                  "Start preview",
                  ActionCategory::Play,
                  {static_cast<i32>('P'), KeyModifier::Ctrl},
                  {static_cast<i32>('P'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->startPreview(); */
                              (void)editor;
                 });

  registerAction({"play.stop",
                  "Stop",
                  "Stop preview",
                  ActionCategory::Play,
                  {static_cast<i32>('P'), KeyModifier::Ctrl | KeyModifier::Shift},
                  {static_cast<i32>('P'), KeyModifier::Ctrl | KeyModifier::Shift}},
                 [editor]() { /* editor->stopPreview(); */
                              (void)editor;
                 });

  // Build actions
  registerAction({"build.build",
                  "Build",
                  "Build project",
                  ActionCategory::Build,
                  {static_cast<i32>('B'), KeyModifier::Ctrl},
                  {static_cast<i32>('B'), KeyModifier::Ctrl}},
                 [editor]() { /* editor->quickBuild(); */
                              (void)editor;
                 });

  // Navigation actions
  registerAction({"nav.next_error",
                  "Next Error",
                  "Go to next error",
                  ActionCategory::Navigation,
                  {static_cast<i32>('E'), KeyModifier::Ctrl},
                  {static_cast<i32>('E'), KeyModifier::Ctrl}},
                 []() { /* Navigate to next error */ });

  registerAction({"nav.prev_error",
                  "Previous Error",
                  "Go to previous error",
                  ActionCategory::Navigation,
                  {static_cast<i32>('E'), KeyModifier::Ctrl | KeyModifier::Shift},
                  {static_cast<i32>('E'), KeyModifier::Ctrl | KeyModifier::Shift}},
                 []() { /* Navigate to previous error */ });
}

// ============================================================================
// Theme
// ============================================================================

const renderer::Color& Theme::getColor(ThemeColor color) const {
  return colors[static_cast<size_t>(color)];
}

void Theme::setColor(ThemeColor color, const renderer::Color& value) {
  colors[static_cast<size_t>(color)] = value;
}

// ============================================================================
// ThemeManager
// ============================================================================

ThemeManager::ThemeManager() {
  registerBuiltinThemes();
  applyTheme("dark");
}

void ThemeManager::applyTheme(const std::string& themeName) {
  std::unique_lock lock(m_mutex);
  auto it = m_themes.find(themeName);
  if (it != m_themes.end()) {
    m_currentTheme = it->second;
    m_currentThemeName = themeName;
  }
}

const Theme& ThemeManager::getCurrentTheme() const {
  std::shared_lock lock(m_mutex);
  return m_currentTheme;
}

std::optional<Theme> ThemeManager::getTheme(const std::string& name) const {
  std::shared_lock lock(m_mutex);
  auto it = m_themes.find(name);
  if (it != m_themes.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<std::string> ThemeManager::getAvailableThemes() const {
  std::shared_lock lock(m_mutex);
  std::vector<std::string> names;
  for (const auto& [name, theme] : m_themes) {
    names.push_back(name);
  }
  return names;
}

void ThemeManager::registerTheme(const Theme& theme) {
  std::unique_lock lock(m_mutex);
  m_themes[theme.name] = theme;
}

void ThemeManager::unregisterTheme(const std::string& name) {
  std::unique_lock lock(m_mutex);
  if (name != "light" && name != "dark") // Don't allow removing builtins
  {
    m_themes.erase(name);
  }
}

Result<void> ThemeManager::exportTheme(const std::string& themeName, const std::string& path) {
  std::shared_lock lock(m_mutex);
  auto it = m_themes.find(themeName);
  if (it == m_themes.end()) {
    return Result<void>::error("Theme not found: " + themeName);
  }

  try {
    std::ofstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open file for writing");
    }

    const Theme& theme = it->second;
    file << "[Theme]\n";
    file << "name=" << theme.name << "\n";
    file << "author=" << theme.author << "\n";
    file << "dark=" << (theme.isDark ? "true" : "false") << "\n";

    // Write colors
    file << "\n[Colors]\n";
    for (size_t i = 0; i < theme.colors.size(); ++i) {
      const auto& c = theme.colors[i];
      file << i << "=" << static_cast<int>(c.r) << "," << static_cast<int>(c.g) << ","
           << static_cast<int>(c.b) << "," << static_cast<int>(c.a) << "\n";
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Export failed: ") + e.what());
  }
}

Result<void> ThemeManager::importTheme(const std::string& path) {
  if (!fs::exists(path)) {
    return Result<void>::error("File not found");
  }

  // Would parse theme file here
  Theme theme;
  theme.name = fs::path(path).stem().string();
  registerTheme(theme);

  return Result<void>::ok();
}

const renderer::Color& ThemeManager::getColor(ThemeColor color) const {
  std::shared_lock lock(m_mutex);
  return m_currentTheme.getColor(color);
}

const ThemeFonts& ThemeManager::getFonts() const {
  std::shared_lock lock(m_mutex);
  return m_currentTheme.fonts;
}

const ThemeMetrics& ThemeManager::getMetrics() const {
  std::shared_lock lock(m_mutex);
  return m_currentTheme.metrics;
}

Theme ThemeManager::createLightTheme() {
  Theme theme;
  theme.name = "light";
  theme.author = "NovelMind";
  theme.isDark = false;

  // Window
  theme.setColor(ThemeColor::WindowBackground, {245, 245, 245, 255});
  theme.setColor(ThemeColor::WindowForeground, {30, 30, 30, 255});
  theme.setColor(ThemeColor::WindowBorder, {200, 200, 200, 255});

  // Panel
  theme.setColor(ThemeColor::PanelBackground, {255, 255, 255, 255});
  theme.setColor(ThemeColor::PanelHeader, {240, 240, 240, 255});
  theme.setColor(ThemeColor::PanelHeaderText, {50, 50, 50, 255});
  theme.setColor(ThemeColor::PanelBorder, {220, 220, 220, 255});

  // Text
  theme.setColor(ThemeColor::TextPrimary, {30, 30, 30, 255});
  theme.setColor(ThemeColor::TextSecondary, {100, 100, 100, 255});
  theme.setColor(ThemeColor::TextDisabled, {180, 180, 180, 255});
  theme.setColor(ThemeColor::TextLink, {0, 122, 204, 255});
  theme.setColor(ThemeColor::TextHighlight, {255, 255, 0, 128});

  // Input
  theme.setColor(ThemeColor::InputBackground, {255, 255, 255, 255});
  theme.setColor(ThemeColor::InputBorder, {200, 200, 200, 255});
  theme.setColor(ThemeColor::InputBorderFocused, {0, 122, 204, 255});
  theme.setColor(ThemeColor::InputText, {30, 30, 30, 255});
  theme.setColor(ThemeColor::InputPlaceholder, {150, 150, 150, 255});

  // Button
  theme.setColor(ThemeColor::ButtonBackground, {240, 240, 240, 255});
  theme.setColor(ThemeColor::ButtonBackgroundHover, {230, 230, 230, 255});
  theme.setColor(ThemeColor::ButtonBackgroundPressed, {220, 220, 220, 255});
  theme.setColor(ThemeColor::ButtonText, {30, 30, 30, 255});
  theme.setColor(ThemeColor::ButtonBorder, {200, 200, 200, 255});

  // Selection
  theme.setColor(ThemeColor::SelectionBackground, {0, 122, 204, 255});
  theme.setColor(ThemeColor::SelectionText, {255, 255, 255, 255});

  // Status
  theme.setColor(ThemeColor::StatusError, {220, 53, 69, 255});
  theme.setColor(ThemeColor::StatusWarning, {255, 193, 7, 255});
  theme.setColor(ThemeColor::StatusInfo, {0, 123, 255, 255});
  theme.setColor(ThemeColor::StatusSuccess, {40, 167, 69, 255});

  // Code
  theme.setColor(ThemeColor::CodeBackground, {255, 255, 255, 255});
  theme.setColor(ThemeColor::CodeKeyword, {0, 0, 255, 255});
  theme.setColor(ThemeColor::CodeString, {163, 21, 21, 255});
  theme.setColor(ThemeColor::CodeNumber, {9, 134, 88, 255});
  theme.setColor(ThemeColor::CodeComment, {0, 128, 0, 255});
  theme.setColor(ThemeColor::CodeFunction, {121, 94, 38, 255});
  theme.setColor(ThemeColor::CodeVariable, {0, 16, 128, 255});
  theme.setColor(ThemeColor::CodeOperator, {0, 0, 0, 255});

  return theme;
}

Theme ThemeManager::createDarkTheme() {
  Theme theme;
  theme.name = "dark";
  theme.author = "NovelMind";
  theme.isDark = true;

  // Window
  theme.setColor(ThemeColor::WindowBackground, {30, 30, 30, 255});
  theme.setColor(ThemeColor::WindowForeground, {220, 220, 220, 255});
  theme.setColor(ThemeColor::WindowBorder, {60, 60, 60, 255});

  // Panel
  theme.setColor(ThemeColor::PanelBackground, {37, 37, 38, 255});
  theme.setColor(ThemeColor::PanelHeader, {45, 45, 46, 255});
  theme.setColor(ThemeColor::PanelHeaderText, {220, 220, 220, 255});
  theme.setColor(ThemeColor::PanelBorder, {60, 60, 60, 255});

  // Text
  theme.setColor(ThemeColor::TextPrimary, {220, 220, 220, 255});
  theme.setColor(ThemeColor::TextSecondary, {150, 150, 150, 255});
  theme.setColor(ThemeColor::TextDisabled, {90, 90, 90, 255});
  theme.setColor(ThemeColor::TextLink, {75, 156, 211, 255});
  theme.setColor(ThemeColor::TextHighlight, {255, 255, 0, 64});

  // Input
  theme.setColor(ThemeColor::InputBackground, {60, 60, 60, 255});
  theme.setColor(ThemeColor::InputBorder, {80, 80, 80, 255});
  theme.setColor(ThemeColor::InputBorderFocused, {0, 122, 204, 255});
  theme.setColor(ThemeColor::InputText, {220, 220, 220, 255});
  theme.setColor(ThemeColor::InputPlaceholder, {100, 100, 100, 255});

  // Button
  theme.setColor(ThemeColor::ButtonBackground, {60, 60, 60, 255});
  theme.setColor(ThemeColor::ButtonBackgroundHover, {70, 70, 70, 255});
  theme.setColor(ThemeColor::ButtonBackgroundPressed, {50, 50, 50, 255});
  theme.setColor(ThemeColor::ButtonText, {220, 220, 220, 255});
  theme.setColor(ThemeColor::ButtonBorder, {80, 80, 80, 255});

  // Selection
  theme.setColor(ThemeColor::SelectionBackground, {38, 79, 120, 255});
  theme.setColor(ThemeColor::SelectionText, {255, 255, 255, 255});

  // Status
  theme.setColor(ThemeColor::StatusError, {244, 67, 54, 255});
  theme.setColor(ThemeColor::StatusWarning, {255, 152, 0, 255});
  theme.setColor(ThemeColor::StatusInfo, {33, 150, 243, 255});
  theme.setColor(ThemeColor::StatusSuccess, {76, 175, 80, 255});

  // Code
  theme.setColor(ThemeColor::CodeBackground, {30, 30, 30, 255});
  theme.setColor(ThemeColor::CodeKeyword, {86, 156, 214, 255});
  theme.setColor(ThemeColor::CodeString, {206, 145, 120, 255});
  theme.setColor(ThemeColor::CodeNumber, {181, 206, 168, 255});
  theme.setColor(ThemeColor::CodeComment, {106, 153, 85, 255});
  theme.setColor(ThemeColor::CodeFunction, {220, 220, 170, 255});
  theme.setColor(ThemeColor::CodeVariable, {156, 220, 254, 255});
  theme.setColor(ThemeColor::CodeOperator, {212, 212, 212, 255});

  // Scene View
  theme.setColor(ThemeColor::SceneBackground, {40, 40, 40, 255});
  theme.setColor(ThemeColor::SceneGrid, {60, 60, 60, 255});
  theme.setColor(ThemeColor::SceneGridMajor, {80, 80, 80, 255});
  theme.setColor(ThemeColor::SceneSelection, {0, 174, 239, 255});
  theme.setColor(ThemeColor::SceneGizmo, {255, 128, 0, 255});

  // Story Graph
  theme.setColor(ThemeColor::GraphBackground, {25, 25, 25, 255});
  theme.setColor(ThemeColor::GraphGrid, {45, 45, 45, 255});
  theme.setColor(ThemeColor::GraphNodeBackground, {55, 55, 55, 255});
  theme.setColor(ThemeColor::GraphNodeBorder, {80, 80, 80, 255});
  theme.setColor(ThemeColor::GraphNodeSelected, {0, 122, 204, 255});
  theme.setColor(ThemeColor::GraphConnection, {150, 150, 150, 255});
  theme.setColor(ThemeColor::GraphConnectionSelected, {0, 174, 239, 255});

  return theme;
}

void ThemeManager::registerBuiltinThemes() {
  registerTheme(createLightTheme());
  registerTheme(createDarkTheme());
}

// ============================================================================
// PreferencesManager
// ============================================================================

PreferencesManager::PreferencesManager() {}

Result<void> PreferencesManager::load(const std::string& path) {
  std::unique_lock lock(m_mutex);
  m_prefsPath = path;

  if (!fs::exists(path)) {
    // Use defaults
    return Result<void>::ok();
  }

  try {
    std::ifstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open preferences file");
    }

    // Would parse preferences file here
    // For now, just return success

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to load preferences: ") + e.what());
  }
}

Result<void> PreferencesManager::save(const std::string& path) {
  std::shared_lock lock(m_mutex);
  try {
    std::ofstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open preferences file for writing");
    }

    file << "[General]\n";
    file << "theme=" << m_prefs.theme << "\n";
    file << "uiScale=" << m_prefs.uiScale << "\n";
    file << "autoSave=" << (m_prefs.autoSave ? "true" : "false") << "\n";
    file << "autoSaveInterval=" << m_prefs.autoSaveIntervalSeconds << "\n";

    file << "\n[Editor]\n";
    file << "showLineNumbers=" << (m_prefs.showLineNumbers ? "true" : "false") << "\n";
    file << "wordWrap=" << (m_prefs.wordWrap ? "true" : "false") << "\n";
    file << "tabSize=" << m_prefs.tabSize << "\n";
    file << "insertSpaces=" << (m_prefs.insertSpaces ? "true" : "false") << "\n";

    file << "\n[RecentProjects]\n";
    for (size_t i = 0; i < m_prefs.recentProjects.size(); ++i) {
      file << i << "=" << m_prefs.recentProjects[i] << "\n";
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to save preferences: ") + e.what());
  }
}

EditorPreferences& PreferencesManager::get() {
  // Note: Returning reference to internal data - caller must ensure thread-safety
  std::shared_lock lock(m_mutex);
  return m_prefs;
}

const EditorPreferences& PreferencesManager::get() const {
  std::shared_lock lock(m_mutex);
  return m_prefs;
}

void PreferencesManager::resetToDefaults() {
  std::unique_lock lock(m_mutex);
  m_prefs = EditorPreferences();
}

void PreferencesManager::addRecentProject(const std::string& path) {
  std::unique_lock lock(m_mutex);
  // Remove if already exists
  auto it = std::find(m_prefs.recentProjects.begin(), m_prefs.recentProjects.end(), path);
  if (it != m_prefs.recentProjects.end()) {
    m_prefs.recentProjects.erase(it);
  }

  // Add to front
  m_prefs.recentProjects.insert(m_prefs.recentProjects.begin(), path);

  // Trim to max size
  while (static_cast<i32>(m_prefs.recentProjects.size()) > m_prefs.maxRecentProjects) {
    m_prefs.recentProjects.pop_back();
  }
}

const std::vector<std::string>& PreferencesManager::getRecentProjects() const {
  std::shared_lock lock(m_mutex);
  return m_prefs.recentProjects;
}

} // namespace NovelMind::editor
