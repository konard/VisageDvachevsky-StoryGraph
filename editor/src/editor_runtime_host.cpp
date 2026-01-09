#include "NovelMind/editor/editor_runtime_host.hpp"

#include <filesystem>

namespace NovelMind::editor {

namespace fs = std::filesystem;

EditorRuntimeHost::EditorRuntimeHost() {}

EditorRuntimeHost::~EditorRuntimeHost() {
  if (m_state != EditorRuntimeState::Unloaded) {
    stop();
  }
}

Result<void> EditorRuntimeHost::loadProject(const ProjectDescriptor &project) {
  if (m_projectLoaded) {
    unloadProject();
  }

  m_project = project;

  // Validate project paths
  if (!fs::exists(project.path)) {
    return Result<void>::error("Project path does not exist: " + project.path);
  }

  // Set default scripts path if not specified
  if (m_project.scriptsPath.empty()) {
    m_project.scriptsPath = (fs::path(project.path) / "Scripts").string();
  }

  // Set default assets path if not specified
  if (m_project.assetsPath.empty()) {
    m_project.assetsPath = (fs::path(project.path) / "Assets").string();
  }

  // Set default scenes path if not specified
  if (m_project.scenesPath.empty()) {
    m_project.scenesPath = (fs::path(project.path) / "Scenes").string();
  }

  // Compile the project scripts
  auto compileResult = compileProject();
  if (!compileResult.isOk()) {
    return compileResult;
  }

  // Initialize runtime components
  auto initResult = initializeRuntime();
  if (!initResult.isOk()) {
    return initResult;
  }

  m_projectLoaded = true;
  m_state = EditorRuntimeState::Stopped;
  fireStateChanged(m_state);

  return Result<void>::ok();
}

void EditorRuntimeHost::unloadProject() {
  if (m_state == EditorRuntimeState::Running ||
      m_state == EditorRuntimeState::Paused) {
    stop();
  }

  m_scriptRuntime.reset();
  m_sceneGraph.reset();
  m_animationManager.reset();
  m_audioManager.reset();
  m_saveManager.reset();
  m_program.reset();
  m_compiledScript.reset();

  m_sceneNames.clear();
  m_fileTimestamps.clear();

  m_project = ProjectDescriptor();
  m_projectLoaded = false;
  m_state = EditorRuntimeState::Unloaded;
  fireStateChanged(m_state);
}

bool EditorRuntimeHost::isProjectLoaded() const { return m_projectLoaded; }

const ProjectDescriptor &EditorRuntimeHost::getProject() const {
  return m_project;
}

// ============================================================================
// Playback Control
// ============================================================================

Result<void> EditorRuntimeHost::play() {
  if (!m_projectLoaded) {
    return Result<void>::error("No project loaded");
  }

  if (!m_project.startScene.empty()) {
    return playFromScene(m_project.startScene);
  }

  // Find first scene if no start scene specified
  if (!m_sceneNames.empty()) {
    return playFromScene(m_sceneNames[0]);
  }

  return Result<void>::error("No scenes found in project");
}

Result<void> EditorRuntimeHost::playFromScene(const std::string &sceneId) {
  if (!m_projectLoaded) {
    return Result<void>::error("No project loaded");
  }

  // Reset runtime state
  resetRuntime();

  // Load compiled script into runtime
  if (m_compiledScript && m_scriptRuntime) {
    auto loadResult = m_scriptRuntime->load(*m_compiledScript);
    if (!loadResult.isOk()) {
      m_state = EditorRuntimeState::Error;
      fireStateChanged(m_state);
      if (m_onRuntimeError) {
        m_onRuntimeError("Failed to load script: " + loadResult.error());
      }
      return loadResult;
    }

    // Go to the specified scene
    auto gotoResult = m_scriptRuntime->gotoScene(sceneId);
    if (!gotoResult.isOk()) {
      m_state = EditorRuntimeState::Error;
      fireStateChanged(m_state);
      if (m_onRuntimeError) {
        m_onRuntimeError("Failed to go to scene: " + gotoResult.error());
      }
      return gotoResult;
    }
  }

  // Update scene graph with the current scene ID
  if (m_sceneGraph) {
    m_sceneGraph->setSceneId(sceneId);
  }

  m_state = EditorRuntimeState::Running;
  fireStateChanged(m_state);

  if (m_onSceneChanged) {
    m_onSceneChanged(sceneId);
  }

  return Result<void>::ok();
}

void EditorRuntimeHost::pause() {
  if (m_state == EditorRuntimeState::Running) {
    m_state = EditorRuntimeState::Paused;
    fireStateChanged(m_state);

    if (m_scriptRuntime) {
      m_scriptRuntime->pause();
    }
  }
}

void EditorRuntimeHost::resume() {
  if (m_state == EditorRuntimeState::Paused) {
    m_state = EditorRuntimeState::Running;
    fireStateChanged(m_state);

    if (m_scriptRuntime) {
      m_scriptRuntime->resume();
    }
  }
}

void EditorRuntimeHost::stop() {
  if (m_state == EditorRuntimeState::Running ||
      m_state == EditorRuntimeState::Paused ||
      m_state == EditorRuntimeState::Stepping) {
    if (m_scriptRuntime) {
      m_scriptRuntime->stop();
    }

    // Reset scene to initial state
    if (m_sceneGraph) {
      m_sceneGraph->clear();
    }

    m_state = EditorRuntimeState::Stopped;
    fireStateChanged(m_state);
  }
}

void EditorRuntimeHost::stepFrame() {
  if (!m_projectLoaded) {
    return;
  }

  if (m_state == EditorRuntimeState::Stopped) {
    // Start in stepping mode
    play();
    m_state = EditorRuntimeState::Stepping;
  }

  if (m_state == EditorRuntimeState::Paused ||
      m_state == EditorRuntimeState::Stepping) {
    // Execute one frame worth of time
    f64 frameTime = 1.0 / 60.0;
    if (m_scriptRuntime) {
      m_scriptRuntime->update(frameTime);
    }
    if (m_sceneGraph) {
      m_sceneGraph->update(frameTime);
    }
    if (m_animationManager) {
      m_animationManager->update(frameTime);
    }
  }
}

void EditorRuntimeHost::stepScriptInstruction() {
  if (!m_projectLoaded || !m_scriptRuntime) {
    return;
  }

  if (m_state == EditorRuntimeState::Stopped) {
    // Start in stepping mode
    play();
  }

  m_state = EditorRuntimeState::Stepping;
  m_singleStepping = true;

  // Execute exactly one VM instruction
  // The VM will pause after the next instruction
  m_scriptRuntime->continueExecution();

  fireStateChanged(m_state);
}

void EditorRuntimeHost::stepLine() {
  if (!m_projectLoaded || !m_scriptRuntime) {
    return;
  }

  // Step until we reach a different source line
  // Implementation would track current line and continue until line changes
  stepScriptInstruction();
}

void EditorRuntimeHost::stepOver() {
  if (!m_projectLoaded || !m_scriptRuntime) {
    return;
  }

  if (m_state == EditorRuntimeState::Stopped) {
    // Start in stepping mode
    play();
  }

  // For the current VM implementation, CALL opcodes execute native callbacks
  // rather than script functions, so step-over is effectively the same as
  // stepping one instruction. In a future implementation with script-to-script
  // calls, this would track call depth and step until returning to the same depth.
  stepScriptInstruction();
}

void EditorRuntimeHost::stepOut() {
  if (!m_projectLoaded || !m_scriptRuntime) {
    return;
  }

  if (m_state == EditorRuntimeState::Stopped) {
    // Start in stepping mode
    play();
  }

  // Track the current scene to detect scene changes
  m_stepOutScene = m_scriptRuntime->getCurrentScene();
  m_steppingOut = true;

  // Continue execution until RETURN or scene change
  m_state = EditorRuntimeState::Stepping;
  m_singleStepping = false;

  if (m_scriptRuntime) {
    m_scriptRuntime->resume();
  }

  // The update() method or runtime will need to check for RETURN opcodes
  // and pause when encountered. For now, we execute until the scene changes
  // or a RETURN is hit (which currently halts execution).
  fireStateChanged(m_state);
}

void EditorRuntimeHost::continueExecution() {
  if (m_state == EditorRuntimeState::Paused ||
      m_state == EditorRuntimeState::Stepping) {
    m_singleStepping = false;
    m_state = EditorRuntimeState::Running;
    fireStateChanged(m_state);

    if (m_scriptRuntime) {
      m_scriptRuntime->resume();
    }
  }
}

EditorRuntimeState EditorRuntimeHost::getState() const { return m_state; }

void EditorRuntimeHost::update(f64 deltaTime) {
  if (m_state != EditorRuntimeState::Running && m_state != EditorRuntimeState::Stepping) {
    return;
  }

  // Check for hot reload if enabled
  if (m_autoHotReload) {
    checkForFileChanges();
  }

  // Update script runtime
  if (m_scriptRuntime) {
    m_scriptRuntime->update(deltaTime);

    // Check for step-out condition: scene changed or script completed
    if (m_steppingOut) {
      std::string currentScene = m_scriptRuntime->getCurrentScene();
      bool sceneChanged = (currentScene != m_stepOutScene);
      bool runtimeComplete = m_scriptRuntime->isComplete();

      // Also check if we hit a RETURN opcode (which currently halts the VM)
      // Since RETURN halts execution, isComplete() will be true
      if (sceneChanged || runtimeComplete) {
        m_steppingOut = false;
        m_state = EditorRuntimeState::Paused;
        m_scriptRuntime->pause();
        fireStateChanged(m_state);
        return;
      }
    }

    // Check if runtime completed
    if (m_scriptRuntime->isComplete()) {
      m_state = EditorRuntimeState::Stopped;
      fireStateChanged(m_state);
    }
  }

  // Update scene graph
  if (m_sceneGraph) {
    m_sceneGraph->update(deltaTime);
  }

  // Update animations
  if (m_animationManager) {
    m_animationManager->update(deltaTime);
  }
}

// ============================================================================
// User Input Simulation
// ============================================================================

void EditorRuntimeHost::simulateClick() {
  if (m_scriptRuntime && m_state == EditorRuntimeState::Running) {
    if (m_scriptRuntime->isWaitingForInput()) {
      m_scriptRuntime->continueExecution();
    }
  }
}

void EditorRuntimeHost::simulateChoiceSelect(i32 index) {
  if (m_scriptRuntime && m_state == EditorRuntimeState::Running) {
    if (m_scriptRuntime->isWaitingForChoice()) {
      m_scriptRuntime->selectChoice(index);
    }
  }
}

void EditorRuntimeHost::simulateKeyPress(i32 /*keyCode*/) {
  // Handle keyboard input
  // This would translate key codes to runtime actions
}

// ============================================================================

} // namespace NovelMind::editor
