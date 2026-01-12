#include "NovelMind/editor/editor_runtime_host.hpp"

#include <algorithm>
#include <utility>

namespace NovelMind::editor {

// ============================================================================
// Breakpoints
// ============================================================================

void EditorRuntimeHost::addBreakpoint(const Breakpoint& breakpoint) {
  // Check if breakpoint already exists
  for (auto& bp : m_breakpoints) {
    if (bp.scriptPath == breakpoint.scriptPath && bp.line == breakpoint.line) {
      bp = breakpoint;
      return;
    }
  }
  m_breakpoints.push_back(breakpoint);
}

void EditorRuntimeHost::removeBreakpoint(const std::string& scriptPath, u32 line) {
  m_breakpoints.erase(std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                     [&](const Breakpoint& bp) {
                                       return bp.scriptPath == scriptPath && bp.line == line;
                                     }),
                      m_breakpoints.end());
}

void EditorRuntimeHost::setBreakpointEnabled(const std::string& scriptPath, u32 line,
                                             bool enabled) {
  for (auto& bp : m_breakpoints) {
    if (bp.scriptPath == scriptPath && bp.line == line) {
      bp.enabled = enabled;
      return;
    }
  }
}

const std::vector<Breakpoint>& EditorRuntimeHost::getBreakpoints() const {
  return m_breakpoints;
}

void EditorRuntimeHost::clearBreakpoints() {
  m_breakpoints.clear();
}

// ============================================================================
// Callbacks
// ============================================================================

void EditorRuntimeHost::setOnStateChanged(OnStateChanged callback) {
  m_onStateChanged = std::move(callback);
}

void EditorRuntimeHost::setOnBreakpointHit(OnBreakpointHit callback) {
  m_onBreakpointHit = std::move(callback);
}

void EditorRuntimeHost::setOnSceneChanged(OnSceneChanged callback) {
  m_onSceneChanged = std::move(callback);
}

void EditorRuntimeHost::setOnVariableChanged(OnVariableChanged callback) {
  m_onVariableChanged = std::move(callback);
}

void EditorRuntimeHost::setOnRuntimeError(OnRuntimeError callback) {
  m_onRuntimeError = std::move(callback);
}

void EditorRuntimeHost::setOnDialogueChanged(OnDialogueChanged callback) {
  m_onDialogueChanged = std::move(callback);
}

void EditorRuntimeHost::setOnChoicesChanged(OnChoicesChanged callback) {
  m_onChoicesChanged = std::move(callback);
}

// ============================================================================
// Scene Graph Access
// ============================================================================

scene::SceneGraph* EditorRuntimeHost::getSceneGraph() {
  return m_sceneGraph.get();
}

scripting::ScriptRuntime* EditorRuntimeHost::getScriptRuntime() {
  return m_scriptRuntime.get();
}

// ============================================================================

} // namespace NovelMind::editor
