#include "NovelMind/editor/editor_state.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<EditorStateManager> EditorStateManager::s_instance = nullptr;

EditorStateManager::EditorStateManager() {
  initializeDefaults();

  // Set default state path
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* appData = std::getenv("APPDATA");
  if (appData) {
    m_statePath = (fs::path(appData) / "NovelMind" / "editor_state.json").string();
  } else {
    m_statePath = "editor_state.json";
  }
#else
  const char* home = std::getenv("HOME");
  if (home) {
    m_statePath = (fs::path(home) / ".config" / "novelmind" / "editor_state.json").string();
  } else {
    m_statePath = "editor_state.json";
  }
#endif
}

EditorStateManager::~EditorStateManager() {
  if (m_dirty) {
    save();
  }
}

EditorStateManager& EditorStateManager::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<EditorStateManager>();
  }
  return *s_instance;
}

// ============================================================================
// Persistence
// ============================================================================

Result<void> EditorStateManager::load() {
  namespace fs = std::filesystem;

  if (!fs::exists(m_statePath)) {
    return Result<void>::ok(); // No state file yet, use defaults
  }

  std::ifstream file(m_statePath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open state file: " + m_statePath);
  }

  // Simple JSON parsing (in production, use a proper JSON library)
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  // Parse theme
  auto parseString = [&content](const std::string& key) -> std::string {
    auto pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos)
      return "";
    auto colonPos = content.find(':', pos);
    auto quoteStart = content.find('"', colonPos);
    auto quoteEnd = content.find('"', quoteStart + 1);
    if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
      return content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }
    return "";
  };

  std::string theme = parseString("theme");
  if (!theme.empty()) {
    m_state.preferences.theme = theme;
  }

  m_dirty = false;
  return Result<void>::ok();
}

Result<void> EditorStateManager::save() {
  namespace fs = std::filesystem;

  // Create directory if needed
  fs::path statePath(m_statePath);
  if (statePath.has_parent_path()) {
    std::error_code ec;
    fs::create_directories(statePath.parent_path(), ec);
    if (ec) {
      return Result<void>::error("Failed to create directory: " + ec.message());
    }
  }

  std::ofstream file(m_statePath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open state file for writing");
  }

  // Write JSON
  file << "{\n";
  file << "  \"preferences\": {\n";
  file << "    \"theme\": \"" << m_state.preferences.theme << "\",\n";
  file << "    \"uiScale\": " << m_state.preferences.uiScale << ",\n";
  file << "    \"fontFamily\": \"" << m_state.preferences.fontFamily << "\",\n";
  file << "    \"fontSize\": " << m_state.preferences.fontSize << ",\n";
  file << "    \"autoSave\": " << (m_state.preferences.autoSave ? "true" : "false") << ",\n";
  file << "    \"autoSaveIntervalMinutes\": " << m_state.preferences.autoSaveIntervalMinutes
       << ",\n";
  file << "    \"undoHistorySize\": " << m_state.preferences.undoHistorySize << "\n";
  file << "  },\n";

  file << "  \"activeLayout\": \"" << m_state.activeLayoutName << "\",\n";

  file << "  \"recentProjects\": [\n";
  for (size_t i = 0; i < m_state.recentProjects.size(); ++i) {
    file << "    \"" << m_state.recentProjects[i] << "\"";
    if (i < m_state.recentProjects.size() - 1) {
      file << ",";
    }
    file << "\n";
  }
  file << "  ],\n";

  file << "  \"lastSession\": {\n";
  file << "    \"projectPath\": \"" << m_state.lastSession.projectPath << "\",\n";
  file << "    \"cleanShutdown\": " << (m_state.lastSession.cleanShutdown ? "true" : "false")
       << "\n";
  file << "  }\n";

  file << "}\n";

  m_dirty = false;
  return Result<void>::ok();
}

std::string EditorStateManager::getStatePath() const {
  return m_statePath;
}

void EditorStateManager::setStatePath(const std::string& path) {
  m_statePath = path;
}

void EditorStateManager::resetToDefaults() {
  m_state = EditorState();
  initializeDefaults();
  m_dirty = true;
  notifyPreferencesChanged();
  notifyLayoutChanged();
  notifyHotkeysChanged();
}

// ============================================================================
// Preferences
// ============================================================================

const EditorPreferences& EditorStateManager::getPreferences() const {
  return m_state.preferences;
}

EditorPreferences& EditorStateManager::getPreferences() {
  m_dirty = true;
  return m_state.preferences;
}

void EditorStateManager::setPreferences(const EditorPreferences& prefs) {
  m_state.preferences = prefs;
  m_dirty = true;
  notifyPreferencesChanged();
}

// ============================================================================
// Layouts
// ============================================================================

const std::vector<LayoutPreset>& EditorStateManager::getLayouts() const {
  return m_state.layouts;
}

const LayoutPreset* EditorStateManager::getActiveLayout() const {
  for (const auto& layout : m_state.layouts) {
    if (layout.name == m_state.activeLayoutName) {
      return &layout;
    }
  }
  return nullptr;
}

bool EditorStateManager::setActiveLayout(const std::string& name) {
  for (const auto& layout : m_state.layouts) {
    if (layout.name == name) {
      m_state.activeLayoutName = name;
      m_dirty = true;
      notifyLayoutChanged();
      return true;
    }
  }
  return false;
}

void EditorStateManager::saveCurrentLayoutAs(const std::string& name,
                                             const std::vector<PanelState>& panels) {
  // Remove existing layout with same name (if not built-in)
  m_state.layouts.erase(
      std::remove_if(m_state.layouts.begin(), m_state.layouts.end(),
                     [&name](const LayoutPreset& l) { return l.name == name && !l.isBuiltIn; }),
      m_state.layouts.end());

  LayoutPreset preset;
  preset.name = name;
  preset.panels = panels;
  preset.isBuiltIn = false;

  m_state.layouts.push_back(preset);
  m_state.activeLayoutName = name;
  m_dirty = true;
  notifyLayoutChanged();
}

bool EditorStateManager::deleteLayout(const std::string& name) {
  auto it = std::find_if(m_state.layouts.begin(), m_state.layouts.end(),
                         [&name](const LayoutPreset& l) { return l.name == name; });

  if (it != m_state.layouts.end() && !it->isBuiltIn) {
    m_state.layouts.erase(it);
    if (m_state.activeLayoutName == name) {
      m_state.activeLayoutName = "Default";
    }
    m_dirty = true;
    notifyLayoutChanged();
    return true;
  }
  return false;
}

void EditorStateManager::resetLayoutToDefault() {
  setActiveLayout("Default");
}

std::vector<std::string> EditorStateManager::getBuiltInLayoutNames() const {
  std::vector<std::string> names;
  for (const auto& layout : m_state.layouts) {
    if (layout.isBuiltIn) {
      names.push_back(layout.name);
    }
  }
  return names;
}

// ============================================================================
// Hotkeys
// ============================================================================

const std::vector<HotkeyBinding>& EditorStateManager::getHotkeys() const {
  return m_state.hotkeys;
}

std::optional<HotkeyBinding>
EditorStateManager::getHotkeyForAction(const std::string& action) const {
  for (const auto& hk : m_state.hotkeys) {
    if (hk.action == action && hk.enabled) {
      return hk;
    }
  }
  return std::nullopt;
}

void EditorStateManager::setHotkey(const std::string& action, const std::string& key,
                                   const std::string& context) {
  // Update existing or add new
  for (auto& hk : m_state.hotkeys) {
    if (hk.action == action) {
      hk.key = key;
      hk.context = context;
      hk.enabled = true;
      m_dirty = true;
      notifyHotkeysChanged();
      return;
    }
  }

  HotkeyBinding binding;
  binding.action = action;
  binding.key = key;
  binding.context = context;
  binding.enabled = true;

  m_state.hotkeys.push_back(binding);
  m_dirty = true;
  notifyHotkeysChanged();
}

void EditorStateManager::removeHotkey(const std::string& action) {
  m_state.hotkeys.erase(
      std::remove_if(m_state.hotkeys.begin(), m_state.hotkeys.end(),
                     [&action](const HotkeyBinding& hk) { return hk.action == action; }),
      m_state.hotkeys.end());
  m_dirty = true;
  notifyHotkeysChanged();
}

void EditorStateManager::resetHotkeysToDefaults() {
  m_state.hotkeys.clear();
  initializeDefaults();
  m_dirty = true;
  notifyHotkeysChanged();
}

std::vector<std::string>
EditorStateManager::checkHotkeyConflicts(const std::string& key, const std::string& context) const {
  std::vector<std::string> conflicts;

  for (const auto& hk : m_state.hotkeys) {
    if (hk.key == key && hk.enabled) {
      if (context == "global" || hk.context == "global" || context == hk.context) {
        conflicts.push_back(hk.action);
      }
    }
  }

  return conflicts;
}

// ============================================================================
// Recent Projects
// ============================================================================

const std::vector<std::string>& EditorStateManager::getRecentProjects() const {
  return m_state.recentProjects;
}

void EditorStateManager::addRecentProject(const std::string& path) {
  // Remove if exists
  removeRecentProject(path);

  // Add at front
  m_state.recentProjects.insert(m_state.recentProjects.begin(), path);

  // Trim to max
  if (m_state.recentProjects.size() > static_cast<size_t>(m_state.preferences.maxRecentProjects)) {
    m_state.recentProjects.resize(static_cast<size_t>(m_state.preferences.maxRecentProjects));
  }

  m_dirty = true;
}

void EditorStateManager::removeRecentProject(const std::string& path) {
  m_state.recentProjects.erase(
      std::remove(m_state.recentProjects.begin(), m_state.recentProjects.end(), path),
      m_state.recentProjects.end());
  m_dirty = true;
}

void EditorStateManager::clearRecentProjects() {
  m_state.recentProjects.clear();
  m_dirty = true;
}

// ============================================================================
// Session Management
// ============================================================================

void EditorStateManager::saveSession(const SessionState& session) {
  m_state.lastSession = session;
  m_state.lastSession.timestamp =
      static_cast<f64>(std::chrono::system_clock::now().time_since_epoch().count());
  m_dirty = true;
  save();
}

const SessionState& EditorStateManager::getLastSession() const {
  return m_state.lastSession;
}

bool EditorStateManager::hasRecoverableSession() const {
  return !m_state.lastSession.projectPath.empty() && !m_state.lastSession.cleanShutdown;
}

void EditorStateManager::markCleanShutdown() {
  m_state.lastSession.cleanShutdown = true;
  m_dirty = true;
  save();
}

void EditorStateManager::clearSession() {
  m_state.lastSession = SessionState();
  m_dirty = true;
}

// ============================================================================
// Key-Value Storage
// ============================================================================

std::optional<StateValue> EditorStateManager::getValue(const std::string& key) const {
  auto it = m_keyValueStore.find(key);
  if (it != m_keyValueStore.end()) {
    return it->second;
  }
  return std::nullopt;
}

void EditorStateManager::setValue(const std::string& key, const StateValue& value) {
  m_keyValueStore[key] = value;
  m_dirty = true;
}

void EditorStateManager::removeValue(const std::string& key) {
  m_keyValueStore.erase(key);
  m_dirty = true;
}

bool EditorStateManager::hasValue(const std::string& key) const {
  return m_keyValueStore.find(key) != m_keyValueStore.end();
}

// ============================================================================
// Change Notification
// ============================================================================

void EditorStateManager::addListener(IStateListener* listener) {
  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void EditorStateManager::removeListener(IStateListener* listener) {
  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

// ============================================================================
// Private Methods
// ============================================================================

void EditorStateManager::initializeDefaults() {
  // Default layout
  LayoutPreset defaultLayout;
  defaultLayout.name = "Default";
  defaultLayout.description = "Standard editor layout";
  defaultLayout.isBuiltIn = true;

  PanelState sceneView;
  sceneView.panelId = "SceneView";
  sceneView.dockTarget = "center";
  sceneView.dockRatio = 0.6f;

  PanelState inspector;
  inspector.panelId = "Inspector";
  inspector.dockTarget = "right";
  inspector.dockRatio = 0.25f;

  PanelState projectBrowser;
  projectBrowser.panelId = "ProjectBrowser";
  projectBrowser.dockTarget = "left";
  projectBrowser.dockRatio = 0.2f;

  PanelState timeline;
  timeline.panelId = "Timeline";
  timeline.dockTarget = "bottom";
  timeline.dockRatio = 0.3f;

  defaultLayout.panels = {sceneView, inspector, projectBrowser, timeline};
  m_state.layouts.push_back(defaultLayout);

  // Default hotkeys
  m_state.hotkeys = {
      {"file.new", "Ctrl+N", "global", true},       {"file.open", "Ctrl+O", "global", true},
      {"file.save", "Ctrl+S", "global", true},      {"file.saveAs", "Ctrl+Shift+S", "global", true},
      {"edit.undo", "Ctrl+Z", "global", true},      {"edit.redo", "Ctrl+Y", "global", true},
      {"edit.cut", "Ctrl+X", "global", true},       {"edit.copy", "Ctrl+C", "global", true},
      {"edit.paste", "Ctrl+V", "global", true},     {"edit.delete", "Delete", "global", true},
      {"edit.selectAll", "Ctrl+A", "global", true}, {"view.zoomIn", "Ctrl++", "global", true},
      {"view.zoomOut", "Ctrl+-", "global", true},   {"view.resetZoom", "Ctrl+0", "global", true},
      {"play.start", "F5", "global", true},         {"play.pause", "F6", "global", true},
      {"play.stop", "Shift+F5", "global", true},    {"build.project", "F7", "global", true},
  };
}

void EditorStateManager::notifyPreferencesChanged() {
  for (auto* listener : m_listeners) {
    listener->onPreferencesChanged();
  }
}

void EditorStateManager::notifyLayoutChanged() {
  for (auto* listener : m_listeners) {
    listener->onLayoutChanged();
  }
}

void EditorStateManager::notifyHotkeysChanged() {
  for (auto* listener : m_listeners) {
    listener->onHotkeysChanged();
  }
}

} // namespace NovelMind::editor
