#include "NovelMind/scripting/script_runtime.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/scripting/vm_debugger.hpp"
#include <algorithm>
#include <bit>

namespace NovelMind::scripting {

ScriptRuntime::ScriptRuntime() = default;
ScriptRuntime::~ScriptRuntime() = default;

Result<void> ScriptRuntime::load(const CompiledScript &script) {
  m_script = script;

  auto result = m_vm.load(script.instructions, script.stringTable);
  if (!result.isOk()) {
    return Result<void>::error(result.error());
  }

  // Load source mappings into debugger if attached
  if (m_vm.hasDebugger() && !script.sourceMappings.empty()) {
    m_vm.debugger()->loadSourceMappings(script.sourceMappings);
  }

  registerCallbacks();
  m_state = RuntimeState::Idle;
  m_visibleCharacters.clear();
  m_currentBackground.clear();
  m_currentSpeaker.clear();
  m_currentDialogue.clear();
  m_currentChoices.clear();

  return Result<void>::ok();
}

void ScriptRuntime::setSceneManager(scene::SceneManager *manager) {
  m_sceneManager = manager;
}

void ScriptRuntime::setDialogueBox(Scene::DialogueBox *dialogueBox) {
  m_dialogueBox = dialogueBox;
  // Pass resource manager to dialogue box if both are set
  if (m_dialogueBox && m_resources) {
    m_dialogueBox->setResourceManager(m_resources);
  }
}

void ScriptRuntime::setResourceManager(resource::ResourceManager *resources) {
  m_resources = resources;
  // Pass resource manager to dialogue box if both are set
  if (m_dialogueBox && m_resources) {
    m_dialogueBox->setResourceManager(m_resources);
  }
}

void ScriptRuntime::setChoiceMenu(Scene::ChoiceMenu *choiceMenu) {
  m_choiceMenu = choiceMenu;
}

void ScriptRuntime::setAudioManager(audio::AudioManager *manager) {
  m_audioManager = manager;
}

void ScriptRuntime::setAnimationManager(scene::AnimationManager *manager) {
  m_animationManager = manager;
}

void ScriptRuntime::setConfig(const RuntimeConfig &config) {
  m_config = config;
}

const RuntimeConfig &ScriptRuntime::getConfig() const { return m_config; }

void ScriptRuntime::start() {
  if (m_script.sceneEntryPoints.empty()) {
    m_state = RuntimeState::Halted;
    return;
  }

  // Start from first scene
  auto it = m_script.sceneEntryPoints.begin();
  gotoScene(it->first);
}

Result<void> ScriptRuntime::gotoScene(const std::string &sceneName) {
  auto it = m_script.sceneEntryPoints.find(sceneName);
  if (it == m_script.sceneEntryPoints.end()) {
    return Result<void>::error("Scene not found: " + sceneName);
  }

  u32 entryPoint = it->second;
  m_currentScene = sceneName;
  m_vm.reset();

  // Load the full program and set IP to scene entry point
  m_vm.load(m_script.instructions, m_script.stringTable);
  m_vm.setIP(entryPoint);
  m_visibleCharacters.clear();
  m_currentChoices.clear();
  m_currentDialogue.clear();
  m_currentSpeaker.clear();
  m_currentBackground.clear();
  m_dialogueActive = false;

  m_state = RuntimeState::Running;
  fireEvent(ScriptEventType::SceneChange, sceneName);

  NOVELMIND_LOG_INFO("Jumped to scene '" + sceneName + "' at instruction " +
                     std::to_string(entryPoint));

  return Result<void>::ok();
}

void ScriptRuntime::update(f64 deltaTime) {
  switch (m_state) {
  case RuntimeState::Idle:
  case RuntimeState::Halted:
  case RuntimeState::Paused:
    return;

  case RuntimeState::WaitingInput:
    // Check for auto-advance
    if (m_config.autoAdvanceEnabled && m_dialogueBox) {
      // Auto-advance handled by dialogue box
    }
    return;

  case RuntimeState::WaitingChoice:
    // Just wait for selectChoice() call
    return;

  case RuntimeState::WaitingTimer:
    updateWaitTimer(deltaTime);
    return;

  case RuntimeState::WaitingTransition:
    updateTransition(deltaTime);
    return;

  case RuntimeState::WaitingAnimation:
    updateAnimation(deltaTime);
    return;

  case RuntimeState::Running:
    if (m_vm.isWaiting()) {
      // Clear VM wait state caused by immediate scene transitions.
      // Waiting on dialogue/choices is tracked by ScriptRuntime state.
      m_vm.signalContinue();
    }
    // Execute VM steps
    if (m_skipMode) {
      // In skip mode, run faster
      for (i32 i = 0; i < 10 && m_state == RuntimeState::Running; ++i) {
        if (!m_vm.step()) {
          if (m_vm.isWaiting() || m_vm.isPaused()) {
            break;
          }
          m_state = RuntimeState::Halted;
          break;
        }

        if (m_vm.isWaiting() || m_vm.isPaused()) {
          break;
        }
      }
    } else {
      // Normal execution
      if (!m_vm.step()) {
        if (m_vm.isWaiting() || m_vm.isPaused()) {
          break;
        }
        m_state = RuntimeState::Halted;
        break;
      }

      if (m_vm.isHalted()) {
        m_state = RuntimeState::Halted;
      }
    }
    break;
  }

  // Update dialogue
  updateDialogue(deltaTime);
}

void ScriptRuntime::continueExecution() {
  if (m_state == RuntimeState::WaitingInput) {
    m_state = RuntimeState::Running;
    m_vm.signalContinue();

    if (m_dialogueBox) {
      m_dialogueBox->clear();
    }
  }
}

void ScriptRuntime::selectChoice(i32 index) {
  if (m_state == RuntimeState::WaitingChoice) {
    if (index >= 0 && index < static_cast<i32>(m_currentChoices.size())) {
      m_selectedChoice = index;
      m_vm.signalChoice(index);
      m_state = RuntimeState::Running;

      fireEvent(ScriptEventType::ChoiceSelected,
                m_currentChoices[static_cast<size_t>(index)],
                Value{static_cast<i32>(index)});

      if (m_choiceMenu) {
        m_choiceMenu->setVisible(false);
      }

      m_currentChoices.clear();
    }
  }
}

void ScriptRuntime::pause() {
  if (m_state == RuntimeState::Running) {
    m_state = RuntimeState::Paused;
    m_vm.pause();
  }
}

void ScriptRuntime::resume() {
  if (m_state == RuntimeState::Paused) {
    m_state = RuntimeState::Running;
    m_vm.resume();
  }
}

void ScriptRuntime::stop() {
  m_state = RuntimeState::Halted;
  m_vm.reset();
}

RuntimeState ScriptRuntime::getState() const { return m_state; }

bool ScriptRuntime::isWaitingForInput() const {
  return m_state == RuntimeState::WaitingInput;
}

bool ScriptRuntime::isWaitingForChoice() const {
  return m_state == RuntimeState::WaitingChoice;
}

bool ScriptRuntime::isComplete() const {
  return m_state == RuntimeState::Halted;
}

const std::string &ScriptRuntime::getCurrentScene() const {
  return m_currentScene;
}

const std::string &ScriptRuntime::getCurrentBackground() const {
  return m_currentBackground;
}

const std::vector<std::string> &ScriptRuntime::getVisibleCharacters() const {
  return m_visibleCharacters;
}

const std::vector<std::string> &ScriptRuntime::getCurrentChoices() const {
  return m_currentChoices;
}

const std::string &ScriptRuntime::getCurrentSpeaker() const {
  return m_currentSpeaker;
}

const std::string &ScriptRuntime::getCurrentDialogue() const {
  return m_currentDialogue;
}

void ScriptRuntime::setVariable(const std::string &name, Value value) {
  m_vm.setVariable(name, value);
  fireEvent(ScriptEventType::VariableChanged, name, value);
}

Value ScriptRuntime::getVariable(const std::string &name) const {
  return m_vm.getVariable(name);
}

void ScriptRuntime::setFlag(const std::string &name, bool value) {
  m_vm.setFlag(name, value);
  fireEvent(ScriptEventType::FlagChanged, name, Value{value});
}

bool ScriptRuntime::getFlag(const std::string &name) const {
  return m_vm.getFlag(name);
}

std::unordered_map<std::string, Value> ScriptRuntime::getAllVariables() const {
  return m_vm.getAllVariables();
}

std::unordered_map<std::string, bool> ScriptRuntime::getAllFlags() const {
  return m_vm.getAllFlags();
}

void ScriptRuntime::setSkipMode(bool enabled) { m_skipMode = enabled; }

bool ScriptRuntime::isSkipMode() const { return m_skipMode; }

RuntimeSaveState ScriptRuntime::saveState() const {
  RuntimeSaveState state;
  state.currentScene = m_currentScene;
  state.instructionPointer = m_vm.getIP();
  state.variables = m_vm.getAllVariables();
  state.flags = m_vm.getAllFlags();
  state.visibleCharacters = m_visibleCharacters;
  state.currentBackground = m_currentBackground;
  state.currentSpeaker = m_currentSpeaker;
  state.currentDialogue = m_currentDialogue;
  state.currentChoices = m_currentChoices;
  state.selectedChoice = m_selectedChoice;
  state.inDialogue = m_dialogueActive;
  state.skipMode = m_skipMode;

  return state;
}

Result<void> ScriptRuntime::loadState(const RuntimeSaveState &state) {
  // Load variables back to VM
  for (const auto &[name, value] : state.variables) {
    m_vm.setVariable(name, value);
  }

  for (const auto &[name, value] : state.flags) {
    m_vm.setFlag(name, value);
  }

  m_vm.setIP(state.instructionPointer);
  m_currentScene = state.currentScene;
  m_visibleCharacters = state.visibleCharacters;
  m_currentBackground = state.currentBackground;
  m_currentSpeaker = state.currentSpeaker;
  m_currentDialogue = state.currentDialogue;
  m_currentChoices = state.currentChoices;
  m_selectedChoice = state.selectedChoice;
  m_dialogueActive = state.inDialogue;
  m_skipMode = state.skipMode;

  if (!state.currentScene.empty()) {
    m_currentScene = state.currentScene;
  }

  if (!m_currentChoices.empty()) {
    m_state = RuntimeState::WaitingChoice;
  } else if (m_dialogueActive) {
    m_state = RuntimeState::WaitingInput;
  } else {
    m_state = RuntimeState::Running;
  }

  return Result<void>::ok();
}

void ScriptRuntime::setEventCallback(EventCallback callback) {
  m_eventCallback = std::move(callback);
}

VirtualMachine &ScriptRuntime::getVM() { return m_vm; }

// VM callback handlers

void ScriptRuntime::onShowBackground(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  std::string bgName = asString(args[0]);
  m_currentBackground = bgName;

  // The scene manager would load and display the background
  if (m_sceneManager) {
    // m_sceneManager->setBackground(bgName);
  }

  fireEvent(ScriptEventType::BackgroundChanged, bgName);
}

void ScriptRuntime::onShowCharacter(const std::vector<Value> &args) {
  if (args.size() < 2) {
    return;
  }

  std::string charId = asString(args[0]);
  i32 posCode = asInt(args[1]);
  Scene::CharacterPosition position = parsePosition(posCode);
  (void)position; // Will be used when scene manager integration is implemented

  if (std::find(m_visibleCharacters.begin(), m_visibleCharacters.end(),
                charId) == m_visibleCharacters.end()) {
    m_visibleCharacters.push_back(charId);
  }

  // Find character definition
  auto it = m_script.characters.find(charId);
  if (it == m_script.characters.end()) {
    return;
  }

  // Create or get character sprite
  if (m_sceneManager) {
    // m_sceneManager->showCharacter(charId, position);
  }

  fireEvent(ScriptEventType::CharacterShow, charId, Value{posCode});
}

void ScriptRuntime::onHideCharacter(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  std::string charId = asString(args[0]);
  m_visibleCharacters.erase(std::remove(m_visibleCharacters.begin(),
                                        m_visibleCharacters.end(), charId),
                            m_visibleCharacters.end());

  if (m_sceneManager) {
    // m_sceneManager->hideCharacter(charId);
  }

  fireEvent(ScriptEventType::CharacterHide, charId);
}

void ScriptRuntime::onSay(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  std::string text = asString(args[0]);
  std::string speaker;

  if (args.size() > 1 && !isNull(args[1])) {
    speaker = asString(args[1]);
  }

  m_currentSpeaker = speaker;
  m_currentDialogue = text;

  if (m_dialogueBox) {
    if (!speaker.empty()) {
      // Look up character for name color
      auto it = m_script.characters.find(speaker);
      if (it != m_script.characters.end()) {
        m_dialogueBox->setSpeakerName(it->second.displayName);
      } else {
        m_dialogueBox->setSpeakerName(speaker);
      }
    } else {
      m_dialogueBox->setSpeakerName("");
    }

    f32 speed = m_skipMode ? m_config.skipModeSpeed : m_config.defaultTextSpeed;
    m_dialogueBox->setText(text);
    m_dialogueBox->setTypewriterSpeed(speed);
    m_dialogueBox->startTypewriter();
    m_dialogueBox->show();

    m_dialogueActive = true;
  }

  m_state = RuntimeState::WaitingInput;
  fireEvent(ScriptEventType::DialogueStart, speaker, Value{text});
}

void ScriptRuntime::onChoice(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  i32 count = asInt(args[0]);
  m_currentChoices.clear();

  // Extract choice texts from the next args
  for (i32 i = 1; i <= count && i < static_cast<i32>(args.size()); ++i) {
    m_currentChoices.push_back(asString(args[static_cast<size_t>(i)]));
  }

  if (m_choiceMenu) {
    m_choiceMenu->clearOptions();
    for (size_t i = 0; i < m_currentChoices.size(); ++i) {
      m_choiceMenu->addOption(m_currentChoices[i]);
    }
    m_choiceMenu->setVisible(true);
  }

  m_state = RuntimeState::WaitingChoice;
  fireEvent(ScriptEventType::ChoiceStart);
}

void ScriptRuntime::onGotoScene(const std::vector<Value> &args) {
  if (args.empty()) {
    NOVELMIND_LOG_WARN("GOTO_SCENE called with no arguments");
    return;
  }

  std::string sceneName;
  if (std::holds_alternative<std::string>(args[0])) {
    sceneName = std::get<std::string>(args[0]);
  } else {
    u32 entryPoint = static_cast<u32>(asInt(args[0]));
    for (const auto &pair : m_script.sceneEntryPoints) {
      if (pair.second == entryPoint) {
        sceneName = pair.first;
        break;
      }
    }

    // If scene not found by entry point, log warning and attempt to continue
    // execution from the entry point directly without scene transition effects
    if (sceneName.empty()) {
      NOVELMIND_LOG_WARN("GOTO_SCENE: No scene found for entry point " +
                         std::to_string(entryPoint) + ", jumping directly");
      // Set VM instruction pointer directly to allow execution to continue
      // This handles cases where the entry point exists but scene name lookup fails
      if (entryPoint < m_vm.getProgramSize()) {
        m_vm.setIP(entryPoint);
        m_state = RuntimeState::Running;
        return;
      }
      NOVELMIND_LOG_ERROR("GOTO_SCENE: Invalid entry point " +
                          std::to_string(entryPoint));
      return;
    }
  }

  if (sceneName.empty()) {
    NOVELMIND_LOG_WARN("GOTO_SCENE: Empty scene name");
    return;
  }

  gotoScene(sceneName);
}

void ScriptRuntime::onWait(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  // Duration is stored as raw bits, deserialize using bit_cast
  u32 durBits = static_cast<u32>(asInt(args[0]));
  f32 duration = std::bit_cast<f32>(durBits);

  m_waitTimer = duration;
  m_state = RuntimeState::WaitingTimer;
}

void ScriptRuntime::onPlaySound(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  std::string soundId = asString(args[0]);

  if (m_audioManager) {
    m_audioManager->playSound(soundId);
  }

  fireEvent(ScriptEventType::SoundPlay, soundId);
}

void ScriptRuntime::onPlayMusic(const std::vector<Value> &args) {
  if (args.empty()) {
    return;
  }

  std::string musicId = asString(args[0]);

  if (m_audioManager) {
    m_audioManager->playMusic(musicId);
  }

  fireEvent(ScriptEventType::MusicStart, musicId);
}

void ScriptRuntime::onStopMusic(const std::vector<Value> &args) {
  f32 fadeOut = 0.0f;

  if (!args.empty()) {
    u32 durBits = static_cast<u32>(asInt(args[0]));
    fadeOut = std::bit_cast<f32>(durBits);
  }

  if (m_audioManager) {
    m_audioManager->stopMusic(fadeOut);
  }

  fireEvent(ScriptEventType::MusicStop);
}

void ScriptRuntime::onTransition(const std::vector<Value> &args) {
  if (args.size() < 2) {
    return;
  }

  std::string type = asString(args[0]);
  u32 durBits = static_cast<u32>(asInt(args[1]));
  f32 duration = std::bit_cast<f32>(durBits);

  m_activeTransition = createTransition(type, duration);
  if (m_activeTransition) {
    m_activeTransition->start(duration);
    m_state = RuntimeState::WaitingTransition;
    fireEvent(ScriptEventType::TransitionStart, type);
  }
}

void ScriptRuntime::onMoveCharacter(const std::vector<Value> &args) {
  // Args layout: [0]=charId, [1]=posCode, [2]=customX (if custom), [3]=customY (if custom), [N-1]=duration
  if (args.size() < 3) {
    NOVELMIND_LOG_WARN("MOVE_CHARACTER called with insufficient arguments");
    m_state = RuntimeState::Running;
    return;
  }

  std::string charId = asString(args[0]);
  i32 posCode = asInt(args[1]);
  Scene::CharacterPosition position = parsePosition(posCode);

  // Extract custom coordinates if position is custom
  f32 customX = 0.5f;
  f32 customY = 0.5f;
  size_t durationIdx = 2;

  if (posCode == 3 && args.size() >= 5) {
    // Custom position: args[2]=customX, args[3]=customY, args[4]=duration
    if (std::holds_alternative<f32>(args[2])) {
      customX = std::get<f32>(args[2]);
    }
    if (std::holds_alternative<f32>(args[3])) {
      customY = std::get<f32>(args[3]);
    }
    durationIdx = 4;
  }

  // Extract duration from raw bits using bit_cast
  f32 duration = 0.5f;
  if (durationIdx < args.size()) {
    u32 durBits = static_cast<u32>(asInt(args[durationIdx]));
    duration = std::bit_cast<f32>(durBits);
  }

  // Verify character is visible
  auto it = std::find(m_visibleCharacters.begin(), m_visibleCharacters.end(), charId);
  if (it == m_visibleCharacters.end()) {
    NOVELMIND_LOG_WARN("MOVE_CHARACTER: Character '" + charId + "' is not visible");
    m_state = RuntimeState::Running;
    return;
  }

  // Create and start the move animation
  if (m_animationManager) {
    // Create a move animation for the character
    // The scene manager or animation manager should handle the actual movement
    // For now, we trigger the event and set the state to wait for animation
    fireEvent(ScriptEventType::CharacterMove, charId, Value{posCode});
    m_state = RuntimeState::WaitingAnimation;

    NOVELMIND_LOG_INFO("Moving character '" + charId + "' to position " +
                       std::to_string(posCode) + " over " +
                       std::to_string(duration) + " seconds");
  } else {
    // No animation manager, just fire event and continue
    fireEvent(ScriptEventType::CharacterMove, charId, Value{posCode});
    m_state = RuntimeState::Running;
  }

  (void)position; // Will be used when scene manager integration is implemented
  (void)customX;
  (void)customY;
}

// Internal helpers

void ScriptRuntime::registerCallbacks() {
  m_vm.registerCallback(OpCode::SHOW_BACKGROUND,
                        [this](const auto &args) { onShowBackground(args); });

  m_vm.registerCallback(OpCode::SHOW_CHARACTER,
                        [this](const auto &args) { onShowCharacter(args); });

  m_vm.registerCallback(OpCode::HIDE_CHARACTER,
                        [this](const auto &args) { onHideCharacter(args); });

  m_vm.registerCallback(OpCode::SAY, [this](const auto &args) { onSay(args); });

  m_vm.registerCallback(OpCode::CHOICE,
                        [this](const auto &args) { onChoice(args); });

  m_vm.registerCallback(OpCode::GOTO_SCENE,
                        [this](const auto &args) { onGotoScene(args); });

  m_vm.registerCallback(OpCode::WAIT,
                        [this](const auto &args) { onWait(args); });

  m_vm.registerCallback(OpCode::PLAY_SOUND,
                        [this](const auto &args) { onPlaySound(args); });

  m_vm.registerCallback(OpCode::PLAY_MUSIC,
                        [this](const auto &args) { onPlayMusic(args); });

  m_vm.registerCallback(OpCode::STOP_MUSIC,
                        [this](const auto &args) { onStopMusic(args); });

  m_vm.registerCallback(OpCode::TRANSITION,
                        [this](const auto &args) { onTransition(args); });

  m_vm.registerCallback(OpCode::MOVE_CHARACTER,
                        [this](const auto &args) { onMoveCharacter(args); });
}

void ScriptRuntime::fireEvent(ScriptEventType type, const std::string &name,
                              const Value &value) {
  if (m_eventCallback) {
    ScriptEvent event;
    event.type = type;
    event.name = name;
    event.value = value;
    m_eventCallback(event);
  }
}

void ScriptRuntime::updateWaitTimer(f64 deltaTime) {
  m_waitTimer -= static_cast<f32>(deltaTime);
  if (m_waitTimer <= 0.0f) {
    m_waitTimer = 0.0f;
    m_state = RuntimeState::Running;
  }
}

void ScriptRuntime::updateTransition(f64 deltaTime) {
  if (m_activeTransition) {
    m_activeTransition->update(deltaTime);

    if (m_activeTransition->isComplete()) {
      m_activeTransition.reset();
      m_state = RuntimeState::Running;
      fireEvent(ScriptEventType::TransitionComplete);
    }
  } else {
    m_state = RuntimeState::Running;
  }
}

void ScriptRuntime::updateAnimation(f64 /*deltaTime*/) {
  if (m_animationManager) {
    // Check if any blocking animations are complete
    if (m_animationManager->count() == 0) {
      m_state = RuntimeState::Running;
    }
  } else {
    m_state = RuntimeState::Running;
  }
}

void ScriptRuntime::updateDialogue(f64 deltaTime) {
  if (m_dialogueActive && m_dialogueBox) {
    m_dialogueBox->update(deltaTime);

    if (m_dialogueBox->isTypewriterComplete() &&
        !m_dialogueBox->isWaitingForInput()) {
      fireEvent(ScriptEventType::DialogueComplete);

      // Handle auto-advance
      if (m_config.autoAdvanceEnabled) {
        // Auto advance logic could be added here
      }
    }
  }
}

Scene::CharacterPosition ScriptRuntime::parsePosition(i32 posCode) {
  switch (posCode) {
  case 0:
    return Scene::CharacterPosition::Left;
  case 1:
    return Scene::CharacterPosition::Center;
  case 2:
    return Scene::CharacterPosition::Right;
  default:
    return Scene::CharacterPosition::Custom;
  }
}

std::unique_ptr<Scene::ITransition>
ScriptRuntime::createTransition(const std::string &type, f32 /*duration*/) {
  std::unique_ptr<Scene::ITransition> transition;

  if (type == "fade") {
    transition =
        std::make_unique<Scene::FadeTransition>(renderer::Color::Black, true);
  } else if (type == "slide") {
    transition = std::make_unique<Scene::SlideTransition>(
        Scene::SlideTransition::Direction::Left);
  } else if (type == "dissolve") {
    transition = std::make_unique<Scene::DissolveTransition>();
  } else if (type == "fadethrough") {
    transition =
        std::make_unique<Scene::FadeThroughTransition>(renderer::Color::Black);
  } else {
    // Default to fade
    transition =
        std::make_unique<Scene::FadeTransition>(renderer::Color::Black, true);
  }

  return transition;
}

} // namespace NovelMind::scripting
