#pragma once

/**
 * @file editor_runtime_host.hpp
 * @brief EditorRuntimeHost - Play-in-Editor runtime environment
 *
 * Provides a complete runtime environment for previewing games inside the
 * editor:
 * - Runs ScriptRuntime in editor mode (unencrypted VFS, dev project tree)
 * - Supports play, pause, stop, step operations
 * - Provides inspection APIs for debugging
 * - Scene state snapshots for instant jumps
 * - Variable and call stack inspection
 */

#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/save/save_manager.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/script_runtime.hpp"
#include "NovelMind/scripting/validator.hpp"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
struct ProjectDescriptor;
class EditorRuntimeHost;

/**
 * @brief Describes a project for runtime loading
 */
struct ProjectDescriptor {
  std::string name;
  std::string path;
  std::string scriptsPath;
  std::string assetsPath;
  std::string scenesPath;
  std::string startScene;
};

/**
 * @brief Current state of the editor runtime
 */
enum class EditorRuntimeState : u8 {
  Unloaded, // No project loaded
  Stopped,  // Project loaded but not running
  Running,  // Actively executing
  Paused,   // Execution paused
  Stepping, // Single-stepping mode
  Error     // Error state, cannot continue
};

/**
 * @brief Snapshot of the current scene for inspection
 */
struct SceneSnapshot {
  std::string currentSceneId;
  std::string activeBackground;
  std::vector<std::string> visibleCharacters;
  std::vector<std::pair<std::string, std::string>> characterExpressions;
  std::vector<scene::SceneObjectState> objects;
  struct CameraState {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 zoom = 1.0f;
    f32 rotation = 0.0f;
    bool valid = false;
  };
  CameraState camera;
  bool dialogueVisible = false;
  std::string dialogueSpeaker;
  std::string dialogueText;
  bool choiceMenuVisible = false;
  std::vector<std::string> choiceOptions;
  i32 selectedChoice = -1;
};

/**
 * @brief Entry in the script call stack
 */
struct CallStackEntry {
  std::string sceneName;
  std::string functionName;
  u32 instructionPointer;
  scripting::SourceLocation sourceLocation;
};

/**
 * @brief Script call stack for debugging
 */
struct ScriptCallStack {
  std::vector<CallStackEntry> frames;
  u32 currentDepth = 0;
};

/**
 * @brief Breakpoint definition
 */
struct Breakpoint {
  std::string scriptPath;
  u32 line;
  u32 column;
  bool enabled = true;
  std::string condition; // Optional conditional expression
};

/**
 * @brief Callback types for runtime events
 */
using OnStateChanged = std::function<void(EditorRuntimeState)>;
using OnBreakpointHit =
    std::function<void(const Breakpoint &, const ScriptCallStack &)>;
using OnSceneChanged = std::function<void(const std::string &)>;
using OnVariableChanged =
    std::function<void(const std::string &, const scripting::Value &)>;
using OnRuntimeError = std::function<void(const std::string &)>;
using OnDialogueChanged =
    std::function<void(const std::string &speaker, const std::string &text)>;
using OnChoicesChanged = std::function<void(const std::vector<std::string> &)>;

/**
 * @brief EditorRuntimeHost - Manages game runtime within the editor
 *
 * This is the core component enabling Play-in-Editor functionality.
 * It wraps the ScriptRuntime and provides editor-specific features:
 * - Dev-mode VFS (reads directly from project tree)
 * - Play/Pause/Stop/Step execution control
 * - Breakpoints and stepping
 * - Variable inspection
 * - Scene state snapshots
 *
 * Example usage:
 * @code
 * EditorRuntimeHost host;
 *
 * ProjectDescriptor project;
 * project.path = "/path/to/project";
 * project.scriptsPath = "/path/to/project/scripts";
 *
 * host.loadProject(project);
 * host.playFromScene("intro");
 *
 * // In update loop
 * host.update(deltaTime);
 *
 * // Get state for UI
 * auto snapshot = host.getSceneSnapshot();
 * auto stack = host.getScriptCallStack();
 * @endcode
 */
class EditorRuntimeHost {
public:
  EditorRuntimeHost();
  ~EditorRuntimeHost();

  // Non-copyable
  EditorRuntimeHost(const EditorRuntimeHost &) = delete;
  EditorRuntimeHost &operator=(const EditorRuntimeHost &) = delete;

  /**
   * @brief Load a project for playback
   * @param project Project descriptor with paths
   * @return Success or error
   */
  Result<void> loadProject(const ProjectDescriptor &project);

  /**
   * @brief Unload the current project
   */
  void unloadProject();

  /**
   * @brief Check if a project is loaded
   */
  [[nodiscard]] bool isProjectLoaded() const;

  /**
   * @brief Get the loaded project info
   */
  [[nodiscard]] const ProjectDescriptor &getProject() const;

  // =========================================================================
  // Playback Control
  // =========================================================================

  /**
   * @brief Start playing from the beginning
   */
  Result<void> play();

  /**
   * @brief Start playing from a specific scene
   * @param sceneId The scene to start from
   */
  Result<void> playFromScene(const std::string &sceneId);

  /**
   * @brief Pause execution
   */
  void pause();

  /**
   * @brief Resume paused execution
   */
  void resume();

  /**
   * @brief Stop execution and reset state
   */
  void stop();

  /**
   * @brief Step forward one frame
   */
  void stepFrame();

  /**
   * @brief Step forward one script instruction
   */
  void stepScriptInstruction();

  /**
   * @brief Step to the next line of script
   */
  void stepLine();

  /**
   * @brief Step over the current instruction
   *
   * Executes the current instruction without stepping into function calls.
   * For the current VM implementation where CALL opcodes execute native
   * callbacks, this behaves similarly to stepScriptInstruction().
   */
  void stepOver();

  /**
   * @brief Step out of the current function/scene
   *
   * Continues execution until a RETURN opcode is encountered or the
   * scene changes, then pauses. Useful for quickly exiting the current
   * function context.
   */
  void stepOut();

  /**
   * @brief Continue until the next breakpoint or end
   */
  void continueExecution();

  /**
   * @brief Get current runtime state
   */
  [[nodiscard]] EditorRuntimeState getState() const;

  /**
   * @brief Update the runtime (call each frame)
   * @param deltaTime Time since last update in seconds
   */
  void update(f64 deltaTime);

  // =========================================================================
  // User Input Simulation
  // =========================================================================

  /**
   * @brief Simulate a click to advance dialogue
   */
  void simulateClick();

  /**
   * @brief Simulate selecting a choice
   * @param index Choice index (0-based)
   */
  void simulateChoiceSelect(i32 index);

  /**
   * @brief Simulate a key press
   * @param keyCode Key code to simulate
   */
  void simulateKeyPress(i32 keyCode);

  // =========================================================================
  // Inspection APIs
  // =========================================================================

  /**
   * @brief Get a snapshot of the current scene state
   */
  [[nodiscard]] SceneSnapshot getSceneSnapshot() const;

  /**
   * @brief Get the current script call stack
   */
  [[nodiscard]] ScriptCallStack getScriptCallStack() const;

  /**
   * @brief Get all script variables
   */
  [[nodiscard]] std::unordered_map<std::string, scripting::Value>
  getVariables() const;

  /**
   * @brief Get a specific variable value
   */
  [[nodiscard]] scripting::Value getVariable(const std::string &name) const;

  /**
   * @brief Set a variable value (for debugging)
   */
  void setVariable(const std::string &name, const scripting::Value &value);

  /**
   * @brief Get all flags
   */
  [[nodiscard]] std::unordered_map<std::string, bool> getFlags() const;

  /**
   * @brief Get a specific flag value
   */
  [[nodiscard]] bool getFlag(const std::string &name) const;

  /**
   * @brief Set a flag value (for debugging)
   */
  void setFlag(const std::string &name, bool value);

  /**
   * @brief Get list of available scenes
   */
  [[nodiscard]] std::vector<std::string> getScenes() const;

  /**
   * @brief Get current scene name
   */
  [[nodiscard]] std::string getCurrentScene() const;

  // =========================================================================
  // Breakpoints
  // =========================================================================

  /**
   * @brief Add a breakpoint
   */
  void addBreakpoint(const Breakpoint &breakpoint);

  /**
   * @brief Remove a breakpoint
   */
  void removeBreakpoint(const std::string &scriptPath, u32 line);

  /**
   * @brief Enable/disable a breakpoint
   */
  void setBreakpointEnabled(const std::string &scriptPath, u32 line,
                            bool enabled);

  /**
   * @brief Get all breakpoints
   */
  [[nodiscard]] const std::vector<Breakpoint> &getBreakpoints() const;

  /**
   * @brief Clear all breakpoints
   */
  void clearBreakpoints();

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnStateChanged(OnStateChanged callback);
  void setOnBreakpointHit(OnBreakpointHit callback);
  void setOnSceneChanged(OnSceneChanged callback);
  void setOnVariableChanged(OnVariableChanged callback);
  void setOnRuntimeError(OnRuntimeError callback);
  void setOnDialogueChanged(OnDialogueChanged callback);
  void setOnChoicesChanged(OnChoicesChanged callback);

  // =========================================================================
  // Scene Graph Access (for SceneView)
  // =========================================================================

  /**
   * @brief Get the internal scene graph for rendering
   */
  [[nodiscard]] scene::SceneGraph *getSceneGraph();

  /**
   * @brief Get the script runtime for advanced inspection
   */
  [[nodiscard]] scripting::ScriptRuntime *getScriptRuntime();

  // === Save / Load ===
  Result<void> saveGame(i32 slot);
  Result<void> loadGame(i32 slot);
  Result<void> saveAuto();
  Result<void> loadAuto();
  [[nodiscard]] bool autoSaveExists() const;
  [[nodiscard]] std::optional<save::SaveMetadata>
  getSaveMetadata(i32 slot) const;

  // =========================================================================
  // Hot Reload
  // =========================================================================

  /**
   * @brief Reload scripts without stopping
   * @return True if reload succeeded
   */
  Result<void> reloadScripts();

  /**
   * @brief Reload a specific asset
   * @param assetPath Path to the asset to reload
   */
  Result<void> reloadAsset(const std::string &assetPath);

  /**
   * @brief Check for file changes and auto-reload if enabled
   */
  void checkForFileChanges();

  /**
   * @brief Enable/disable auto hot reload
   */
  void setAutoHotReload(bool enabled);
  [[nodiscard]] bool isAutoHotReloadEnabled() const;

private:
  // Internal helpers
  Result<void> compileProject();
  Result<void> initializeRuntime();
  void resetRuntime();
  bool checkBreakpoint(const scripting::SourceLocation &location);
  void fireStateChanged(EditorRuntimeState newState);
  void fireBreakpointHit(const Breakpoint &bp);
  void onRuntimeEvent(const scripting::ScriptEvent &event);
  void applySceneDocument(const std::string &sceneId);
  Result<void> applySaveDataToRuntime(const save::SaveData &data);

  // Project info
  ProjectDescriptor m_project;
  bool m_projectLoaded = false;

  // Compiled data
  std::unique_ptr<scripting::Program> m_program;
  std::unique_ptr<scripting::CompiledScript> m_compiledScript;

  // Runtime components
  std::unique_ptr<scripting::ScriptRuntime> m_scriptRuntime;
  std::unique_ptr<scene::SceneGraph> m_sceneGraph;
  std::unique_ptr<scene::AnimationManager> m_animationManager;
  std::unique_ptr<audio::AudioManager> m_audioManager;
  std::unique_ptr<save::SaveManager> m_saveManager;
  std::unique_ptr<resource::ResourceManager> m_resourceManager;

  // State
  EditorRuntimeState m_state = EditorRuntimeState::Unloaded;
  bool m_singleStepping = false;
  u32 m_targetInstructionPointer = 0;

  // Step-out tracking
  bool m_steppingOut = false;
  std::string m_stepOutScene;

  // Breakpoints
  std::vector<Breakpoint> m_breakpoints;

  // Callbacks
  OnStateChanged m_onStateChanged;
  OnBreakpointHit m_onBreakpointHit;
  OnSceneChanged m_onSceneChanged;
  OnVariableChanged m_onVariableChanged;
  OnRuntimeError m_onRuntimeError;
  OnDialogueChanged m_onDialogueChanged;
  OnChoicesChanged m_onChoicesChanged;

  // Hot reload
  bool m_autoHotReload = true;
  std::unordered_map<std::string, u64> m_fileTimestamps;

  // Cached data for inspection
  std::vector<std::string> m_sceneNames;
};

} // namespace NovelMind::editor
