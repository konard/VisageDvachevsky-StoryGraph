#include "NovelMind/editor/editor_runtime_host.hpp"

#include <chrono>
#include <filesystem>

namespace NovelMind::editor {

namespace fs = std::filesystem;

// ============================================================================
// Hot Reload
// ============================================================================

Result<void> EditorRuntimeHost::reloadScripts() {
  // Save current state
  scripting::RuntimeSaveState savedState;
  if (m_scriptRuntime) {
    savedState = m_scriptRuntime->saveState();
  }

  // Recompile
  auto compileResult = compileProject();
  if (!compileResult.isOk()) {
    return compileResult;
  }

  // Reload into runtime
  if (m_scriptRuntime && m_compiledScript) {
    auto loadResult = m_scriptRuntime->load(*m_compiledScript);
    if (!loadResult.isOk()) {
      return loadResult;
    }

    // Try to restore state
    auto restoreResult = m_scriptRuntime->loadState(savedState);
    if (!restoreResult.isOk()) {
      // State restore failed, start fresh
      return restoreResult;
    }
  }

  return Result<void>::ok();
}

Result<void> EditorRuntimeHost::reloadAsset(const std::string& /*assetPath*/) {
  // This would reload a specific asset from disk
  // For textures: reload texture data
  // For audio: reload audio buffer
  // For now, return success
  return Result<void>::ok();
}

void EditorRuntimeHost::checkForFileChanges() {
  if (!m_projectLoaded) {
    return;
  }

  try {
    fs::path scriptsPath(m_project.scriptsPath);
    if (!fs::exists(scriptsPath)) {
      return;
    }

    bool changesDetected = false;

    for (const auto& entry : fs::recursive_directory_iterator(scriptsPath)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nm") {
          std::string path = entry.path().string();
          u64 modTime = static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                                             entry.last_write_time().time_since_epoch())
                                             .count());

          auto it = m_fileTimestamps.find(path);
          if (it != m_fileTimestamps.end()) {
            if (it->second != modTime) {
              changesDetected = true;
              it->second = modTime;
            }
          } else {
            m_fileTimestamps[path] = modTime;
          }
        }
      }
    }

    if (changesDetected) {
      reloadScripts();
    }
  } catch (const fs::filesystem_error&) {
    // Ignore filesystem errors during hot reload check
  }
}

void EditorRuntimeHost::setAutoHotReload(bool enabled) {
  m_autoHotReload = enabled;
}

bool EditorRuntimeHost::isAutoHotReloadEnabled() const {
  return m_autoHotReload;
}

// ============================================================================

} // namespace NovelMind::editor
