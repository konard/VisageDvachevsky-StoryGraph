#pragma once

/**
 * @file script_runtime.hpp
 * @brief Script Runtime Binding Layer
 *
 * This module provides the runtime binding between the Script VM
 * and the Scene/Audio/Renderer systems. It handles:
 * - VM opcode callbacks to scene methods
 * - Async execution (coroutine-style)
 * - Event waiting (clicks, animations, transitions)
 * - Save state serialization
 */

#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scene/animation.hpp"
#include "NovelMind/scene/character_sprite.hpp"
#include "NovelMind/scene/choice_menu.hpp"
#include "NovelMind/scene/dialogue_box.hpp"
#include "NovelMind/scene/scene_manager.hpp"
#include "NovelMind/scene/transition.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/vm.hpp"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <variant>

namespace NovelMind::resource {
class ResourceManager;
} // namespace NovelMind::resource

namespace NovelMind::scripting {

// Forward declarations
class ScriptRuntime;

/**
 * @brief Execution state of the runtime
 */
enum class RuntimeState : u8 {
  Idle,              // Not executing
  Running,           // Executing instructions
  WaitingInput,      // Waiting for user input (click to continue)
  WaitingChoice,     // Waiting for choice selection
  WaitingTimer,      // Waiting for a timed delay
  WaitingTransition, // Waiting for transition to complete
  WaitingAnimation,  // Waiting for animation to complete
  Paused,            // Manually paused
  Halted             // Execution complete
};

/**
 * @brief Event types for script callbacks
 */
enum class ScriptEventType : u8 {
  SceneChange,        // Scene was changed (by goto)
  BackgroundChanged,  // Background image changed
  CharacterShow,      // Character was shown
  CharacterHide,      // Character was hidden
  CharacterMove,      // Character was moved to a new position
  DialogueStart,      // Dialogue started
  DialogueComplete,   // Dialogue finished (typewriter complete)
  ChoiceStart,        // Choice menu shown
  ChoiceSelected,     // User selected a choice
  TransitionStart,    // Transition began
  TransitionComplete, // Transition finished
  MusicStart,         // Music started
  MusicStop,          // Music stopped
  SoundPlay,          // Sound effect played
  VariableChanged,    // Variable was modified
  FlagChanged         // Flag was modified
};

/**
 * @brief Event data for script callbacks
 */
struct ScriptEvent {
  ScriptEventType type;
  std::string name;        // Relevant name (scene, character, etc.)
  Value value;             // Associated value if any
  SourceLocation location; // Location in script
};

/**
 * @brief Script runtime configuration
 */
struct RuntimeConfig {
  f32 defaultTextSpeed = 30.0f; // Characters per second
  f32 defaultTransitionDuration = 0.5f;
  bool autoAdvanceEnabled = false;
  f32 autoAdvanceDelay = 2.0f; // Seconds after text complete
  bool skipModeEnabled = false;
  f32 skipModeSpeed = 100.0f; // Text speed in skip mode
};

/**
 * @brief Script runtime save state
 */
struct RuntimeSaveState {
  std::string currentScene;
  u32 instructionPointer;
  std::unordered_map<std::string, Value> variables;
  std::unordered_map<std::string, bool> flags;

  // Scene state
  std::vector<std::string> visibleCharacters;
  std::string currentBackground;
  std::string currentSpeaker;
  std::string currentDialogue;
  std::vector<std::string> currentChoices;
  i32 selectedChoice = -1;
  bool inDialogue;
  bool skipMode = false;
};

/**
 * @brief Script Runtime - connects VM to game systems
 *
 * The ScriptRuntime manages the execution of NM Script bytecode
 * and routes commands to the appropriate game systems (Scene, Audio, etc.)
 *
 * Example usage:
 * @code
 * ScriptRuntime runtime;
 * runtime.setSceneManager(&sceneManager);
 * runtime.setAudioManager(&audioManager);
 *
 * // Load compiled script
 * runtime.load(compiledScript);
 *
 * // Start from a scene
 * runtime.gotoScene("intro");
 *
 * // Game loop
 * while (running) {
 *     runtime.update(deltaTime);
 *
 *     if (runtime.isWaitingForInput()) {
 *         if (userClicked) {
 *             runtime.continueExecution();
 *         }
 *     }
 * }
 * @endcode
 */
class ScriptRuntime {
public:
  using EventCallback = std::function<void(const ScriptEvent &)>;

  ScriptRuntime();
  ~ScriptRuntime();

  /**
   * @brief Load a compiled script
   */
  Result<void> load(const CompiledScript &script);

  /**
   * @brief Set the scene manager for character/background commands
   */
  void setSceneManager(scene::SceneManager *manager);

  /**
   * @brief Set the dialogue box for text display
   */
  void setDialogueBox(Scene::DialogueBox *dialogueBox);

  /**
   * @brief Set the resource manager for font loading in dialogue box
   */
  void setResourceManager(resource::ResourceManager *resources);

  /**
   * @brief Set the choice menu for player choices
   */
  void setChoiceMenu(Scene::ChoiceMenu *choiceMenu);

  /**
   * @brief Set the audio manager for sound/music
   */
  void setAudioManager(audio::AudioManager *manager);

  /**
   * @brief Set the animation manager for async animations
   */
  void setAnimationManager(scene::AnimationManager *manager);

  /**
   * @brief Set runtime configuration
   */
  void setConfig(const RuntimeConfig &config);

  /**
   * @brief Get current configuration
   */
  [[nodiscard]] const RuntimeConfig &getConfig() const;

  /**
   * @brief Start execution from the first scene
   */
  void start();

  /**
   * @brief Go to a specific scene
   */
  Result<void> gotoScene(const std::string &sceneName);

  /**
   * @brief Update the runtime (call each frame)
   */
  void update(f64 deltaTime);

  /**
   * @brief Continue execution after waiting for input
   */
  void continueExecution();

  /**
   * @brief Select a choice option
   */
  void selectChoice(i32 index);

  /**
   * @brief Pause execution
   */
  void pause();

  /**
   * @brief Resume execution
   */
  void resume();

  /**
   * @brief Stop execution
   */
  void stop();

  /**
   * @brief Get current runtime state
   */
  [[nodiscard]] RuntimeState getState() const;

  /**
   * @brief Check if waiting for user input
   */
  [[nodiscard]] bool isWaitingForInput() const;

  /**
   * @brief Check if waiting for choice
   */
  [[nodiscard]] bool isWaitingForChoice() const;

  /**
   * @brief Check if execution is complete
   */
  [[nodiscard]] bool isComplete() const;

  /**
   * @brief Set a script variable
   */
  void setVariable(const std::string &name, Value value);

  /**
   * @brief Get a script variable
   */
  [[nodiscard]] Value getVariable(const std::string &name) const;

  /**
   * @brief Set a flag
   */
  void setFlag(const std::string &name, bool value);

  /**
   * @brief Get a flag
   */
  [[nodiscard]] bool getFlag(const std::string &name) const;
  [[nodiscard]] std::unordered_map<std::string, Value> getAllVariables() const;
  [[nodiscard]] std::unordered_map<std::string, bool> getAllFlags() const;

  /**
   * @brief Enable skip mode (fast-forward through text)
   */
  void setSkipMode(bool enabled);

  /**
   * @brief Check if in skip mode
   */
  [[nodiscard]] bool isSkipMode() const;

  // Introspection for editor/runtime UI
  [[nodiscard]] const std::string &getCurrentScene() const;
  [[nodiscard]] const std::string &getCurrentBackground() const;
  [[nodiscard]] const std::vector<std::string> &getVisibleCharacters() const;
  [[nodiscard]] const std::vector<std::string> &getCurrentChoices() const;
  [[nodiscard]] const std::string &getCurrentSpeaker() const;
  [[nodiscard]] const std::string &getCurrentDialogue() const;

  /**
   * @brief Save current state
   */
  [[nodiscard]] RuntimeSaveState saveState() const;

  /**
   * @brief Load state
   */
  Result<void> loadState(const RuntimeSaveState &state);

  /**
   * @brief Register event callback
   */
  void setEventCallback(EventCallback callback);

  /**
   * @brief Get the underlying VM (for debugging)
   */
  [[nodiscard]] VirtualMachine &getVM();

private:
  // VM callback handlers
  void onShowBackground(const std::vector<Value> &args);
  void onShowCharacter(const std::vector<Value> &args);
  void onHideCharacter(const std::vector<Value> &args);
  void onSay(const std::vector<Value> &args);
  void onChoice(const std::vector<Value> &args);
  void onGotoScene(const std::vector<Value> &args);
  void onWait(const std::vector<Value> &args);
  void onPlaySound(const std::vector<Value> &args);
  void onPlayMusic(const std::vector<Value> &args);
  void onStopMusic(const std::vector<Value> &args);
  void onTransition(const std::vector<Value> &args);
  void onMoveCharacter(const std::vector<Value> &args);

  // Internal helpers
  void registerCallbacks();
  void fireEvent(ScriptEventType type, const std::string &name = "",
                 const Value &value = Value{});

  void updateWaitTimer(f64 deltaTime);
  void updateTransition(f64 deltaTime);
  void updateAnimation(f64 deltaTime);
  void updateDialogue(f64 deltaTime);

  Scene::CharacterPosition parsePosition(i32 posCode);
  std::unique_ptr<Scene::ITransition> createTransition(const std::string &type,
                                                       f32 duration);

  // VM and compiled script
  VirtualMachine m_vm;
  CompiledScript m_script;

  // Connected systems
  scene::SceneManager *m_sceneManager = nullptr;
  Scene::DialogueBox *m_dialogueBox = nullptr;
  Scene::ChoiceMenu *m_choiceMenu = nullptr;
  audio::AudioManager *m_audioManager = nullptr;
  scene::AnimationManager *m_animationManager = nullptr;
  resource::ResourceManager *m_resources = nullptr;

  // State
  RuntimeState m_state = RuntimeState::Idle;
  std::string m_currentScene;
  std::string m_currentBackground;
  std::vector<std::string> m_visibleCharacters;
  std::string m_currentSpeaker;
  std::string m_currentDialogue;
  RuntimeConfig m_config;

  // Wait state
  f32 m_waitTimer = 0.0f;
  std::unique_ptr<Scene::ITransition> m_activeTransition;

  // Dialogue state
  bool m_dialogueActive = false;

  // Choice state
  std::vector<std::string> m_currentChoices;
  i32 m_selectedChoice = -1;

  // Skip mode
  bool m_skipMode = false;

  // Event callback
  EventCallback m_eventCallback;
};

/**
 * @brief Helper to create a character sprite from script data
 */
[[nodiscard]] inline std::unique_ptr<Scene::CharacterSprite>
createCharacterFromDecl(const CharacterDecl &decl) {
  auto sprite = std::make_unique<Scene::CharacterSprite>(decl.id, decl.id);
  sprite->setDisplayName(decl.displayName);

  // Parse color if provided
  if (!decl.color.empty()) {
    // Parse hex color
    std::string colorStr = decl.color;
    if (!colorStr.empty() && colorStr[0] == '#') {
      colorStr = colorStr.substr(1);
    }

    if (colorStr.length() >= 6) {
      try {
        u32 hex =
            static_cast<u32>(std::stoul(colorStr.substr(0, 6), nullptr, 16));
        u8 r = (hex >> 16) & 0xFF;
        u8 g = (hex >> 8) & 0xFF;
        u8 b = hex & 0xFF;
        sprite->setNameColor(renderer::Color(r, g, b, 255));
      } catch (...) {
      }
    }
  }

  return sprite;
}

} // namespace NovelMind::scripting
