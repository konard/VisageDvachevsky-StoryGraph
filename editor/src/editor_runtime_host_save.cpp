#include "NovelMind/editor/editor_runtime_host.hpp"
#include "editor_runtime_host_detail.hpp"

#include <cstdlib>
#include <variant>

namespace NovelMind::editor {

// ============================================================================
// Save / Load
// ============================================================================

Result<void> EditorRuntimeHost::saveGame(i32 slot) {
  if (!m_projectLoaded || !m_scriptRuntime || !m_saveManager) {
    return Result<void>::error("Runtime is not ready for saving");
  }

  const auto state = m_scriptRuntime->saveState();
  save::SaveData data;
  data.sceneId = state.currentScene;
  data.nodeId = std::to_string(state.instructionPointer);

  for (const auto& [name, value] : state.variables) {
    const std::string typeKey = "__var.type." + name;
    if (std::holds_alternative<i32>(value)) {
      data.intVariables[name] = std::get<i32>(value);
      data.stringVariables[typeKey] = "int";
    } else if (std::holds_alternative<f32>(value)) {
      data.floatVariables[name] = std::get<f32>(value);
      data.stringVariables[typeKey] = "float";
    } else if (std::holds_alternative<bool>(value)) {
      data.intVariables[name] = std::get<bool>(value) ? 1 : 0;
      data.stringVariables[typeKey] = "bool";
    } else if (std::holds_alternative<std::string>(value)) {
      data.stringVariables[name] = std::get<std::string>(value);
      data.stringVariables[typeKey] = "string";
    }
  }

  for (const auto& [name, value] : state.flags) {
    data.flags[name] = value;
  }

  data.intVariables["__runtime.ip"] = static_cast<i32>(state.instructionPointer);
  data.intVariables["__runtime.choice"] = state.selectedChoice;
  data.intVariables["__runtime.skip"] = state.skipMode ? 1 : 0;
  data.intVariables["__runtime.dialogue_active"] = state.inDialogue ? 1 : 0;
  data.stringVariables["__runtime.speaker"] = state.currentSpeaker;
  data.stringVariables["__runtime.dialogue"] = state.currentDialogue;
  data.stringVariables["__runtime.background"] = state.currentBackground;
  data.stringVariables["__runtime.visible"] = detail::encodeList(state.visibleCharacters);
  data.stringVariables["__runtime.choices"] = detail::encodeList(state.currentChoices);

  return m_saveManager->save(slot, data);
}

Result<void> EditorRuntimeHost::loadGame(i32 slot) {
  if (!m_projectLoaded || !m_scriptRuntime || !m_saveManager) {
    return Result<void>::error("Runtime is not ready for loading");
  }

  auto loadResult = m_saveManager->load(slot);
  if (loadResult.isError()) {
    return Result<void>::error(loadResult.error());
  }

  return applySaveDataToRuntime(loadResult.value());
}

Result<void> EditorRuntimeHost::saveAuto() {
  if (!m_projectLoaded || !m_saveManager || !m_scriptRuntime) {
    return Result<void>::error("Runtime is not ready for saving");
  }
  const auto state = m_scriptRuntime->saveState();
  save::SaveData data;
  data.sceneId = state.currentScene;
  data.nodeId = std::to_string(state.instructionPointer);
  for (const auto& [name, value] : state.variables) {
    const std::string typeKey = "__var.type." + name;
    if (std::holds_alternative<i32>(value)) {
      data.intVariables[name] = std::get<i32>(value);
      data.stringVariables[typeKey] = "int";
    } else if (std::holds_alternative<f32>(value)) {
      data.floatVariables[name] = std::get<f32>(value);
      data.stringVariables[typeKey] = "float";
    } else if (std::holds_alternative<bool>(value)) {
      data.intVariables[name] = std::get<bool>(value) ? 1 : 0;
      data.stringVariables[typeKey] = "bool";
    } else if (std::holds_alternative<std::string>(value)) {
      data.stringVariables[name] = std::get<std::string>(value);
      data.stringVariables[typeKey] = "string";
    }
  }
  for (const auto& [name, value] : state.flags) {
    data.flags[name] = value;
  }
  data.intVariables["__runtime.ip"] = static_cast<i32>(state.instructionPointer);
  data.intVariables["__runtime.choice"] = state.selectedChoice;
  data.intVariables["__runtime.skip"] = state.skipMode ? 1 : 0;
  data.intVariables["__runtime.dialogue_active"] = state.inDialogue ? 1 : 0;
  data.stringVariables["__runtime.speaker"] = state.currentSpeaker;
  data.stringVariables["__runtime.dialogue"] = state.currentDialogue;
  data.stringVariables["__runtime.background"] = state.currentBackground;
  data.stringVariables["__runtime.visible"] = detail::encodeList(state.visibleCharacters);
  data.stringVariables["__runtime.choices"] = detail::encodeList(state.currentChoices);

  return m_saveManager->saveAuto(data);
}

Result<void> EditorRuntimeHost::loadAuto() {
  if (!m_projectLoaded || !m_scriptRuntime || !m_saveManager) {
    return Result<void>::error("Runtime is not ready for loading");
  }

  auto loadResult = m_saveManager->loadAuto();
  if (loadResult.isError()) {
    return Result<void>::error(loadResult.error());
  }

  return applySaveDataToRuntime(loadResult.value());
}

bool EditorRuntimeHost::autoSaveExists() const {
  if (!m_saveManager) {
    return false;
  }
  return m_saveManager->autoSaveExists();
}

std::optional<save::SaveMetadata> EditorRuntimeHost::getSaveMetadata(i32 slot) const {
  if (!m_saveManager) {
    return std::nullopt;
  }
  return m_saveManager->getSlotMetadata(slot);
}

Result<void> EditorRuntimeHost::applySaveDataToRuntime(const save::SaveData& data) {
  if (!m_compiledScript) {
    return Result<void>::error("No compiled script loaded");
  }

  auto reloadResult = m_scriptRuntime->load(*m_compiledScript);
  if (!reloadResult.isOk()) {
    return reloadResult;
  }

  scripting::RuntimeSaveState state;
  state.currentScene = data.sceneId;
  state.instructionPointer = 0;
  auto ipIt = data.intVariables.find("__runtime.ip");
  if (ipIt != data.intVariables.end()) {
    state.instructionPointer = static_cast<u32>(ipIt->second);
  } else if (!data.nodeId.empty()) {
    state.instructionPointer = static_cast<u32>(std::strtoul(data.nodeId.c_str(), nullptr, 10));
  }

  std::unordered_map<std::string, std::string> typeMap;
  for (const auto& [name, value] : data.stringVariables) {
    if (detail::startsWith(name, "__var.type.")) {
      typeMap[name.substr(11)] = value;
    }
  }

  for (const auto& [name, value] : data.intVariables) {
    if (detail::startsWith(name, "__runtime.")) {
      continue;
    }
    auto typeIt = typeMap.find(name);
    if (typeIt != typeMap.end() && typeIt->second == "bool") {
      state.variables[name] = scripting::Value{value != 0};
    } else {
      state.variables[name] = scripting::Value{value};
    }
  }

  for (const auto& [name, value] : data.floatVariables) {
    state.variables[name] = scripting::Value{value};
  }

  for (const auto& [name, value] : data.stringVariables) {
    if (detail::startsWith(name, "__runtime.") || detail::startsWith(name, "__var.type.")) {
      continue;
    }
    state.variables[name] = scripting::Value{value};
  }

  for (const auto& [name, value] : data.flags) {
    state.flags[name] = value;
  }

  auto speakerIt = data.stringVariables.find("__runtime.speaker");
  if (speakerIt != data.stringVariables.end()) {
    state.currentSpeaker = speakerIt->second;
  }
  auto dialogueIt = data.stringVariables.find("__runtime.dialogue");
  if (dialogueIt != data.stringVariables.end()) {
    state.currentDialogue = dialogueIt->second;
  }
  auto bgIt = data.stringVariables.find("__runtime.background");
  if (bgIt != data.stringVariables.end()) {
    state.currentBackground = bgIt->second;
  }
  auto visibleIt = data.stringVariables.find("__runtime.visible");
  if (visibleIt != data.stringVariables.end()) {
    state.visibleCharacters = detail::decodeList(visibleIt->second);
  }
  auto choicesIt = data.stringVariables.find("__runtime.choices");
  if (choicesIt != data.stringVariables.end()) {
    state.currentChoices = detail::decodeList(choicesIt->second);
  }

  auto choiceIndexIt = data.intVariables.find("__runtime.choice");
  if (choiceIndexIt != data.intVariables.end()) {
    state.selectedChoice = choiceIndexIt->second;
  }
  auto skipIt = data.intVariables.find("__runtime.skip");
  if (skipIt != data.intVariables.end()) {
    state.skipMode = skipIt->second != 0;
  }
  auto dialogueActiveIt = data.intVariables.find("__runtime.dialogue_active");
  if (dialogueActiveIt != data.intVariables.end()) {
    state.inDialogue = dialogueActiveIt->second != 0;
  }

  auto restoreResult = m_scriptRuntime->loadState(state);
  if (!restoreResult.isOk()) {
    return restoreResult;
  }

  if (m_sceneGraph) {
    m_sceneGraph->clear();
    m_sceneGraph->setSceneId(state.currentScene);

    if (!state.currentBackground.empty()) {
      m_sceneGraph->showBackground(state.currentBackground);
    }

    const auto& chars = state.visibleCharacters;
    for (size_t i = 0; i < chars.size(); ++i) {
      scene::CharacterObject::Position pos = scene::CharacterObject::Position::Center;
      if (chars.size() == 2) {
        pos = (i == 0) ? scene::CharacterObject::Position::Left
                       : scene::CharacterObject::Position::Right;
      } else if (chars.size() >= 3) {
        if (i == 0) {
          pos = scene::CharacterObject::Position::Left;
        } else if (i == 1) {
          pos = scene::CharacterObject::Position::Center;
        } else if (i == 2) {
          pos = scene::CharacterObject::Position::Right;
        } else {
          pos = scene::CharacterObject::Position::Custom;
        }
      }
      m_sceneGraph->showCharacter(chars[i], chars[i], pos);
    }

    if (state.inDialogue || !state.currentDialogue.empty() || !state.currentSpeaker.empty()) {
      m_sceneGraph->showDialogue(state.currentSpeaker, state.currentDialogue);
    } else {
      m_sceneGraph->hideDialogue();
    }

    if (!state.currentChoices.empty()) {
      std::vector<scene::ChoiceUIObject::ChoiceOption> options;
      options.reserve(state.currentChoices.size());
      for (size_t i = 0; i < state.currentChoices.size(); ++i) {
        scene::ChoiceUIObject::ChoiceOption opt;
        opt.id = "choice_" + std::to_string(i);
        opt.text = state.currentChoices[i];
        options.push_back(opt);
      }
      auto* menu = m_sceneGraph->showChoices(options);
      if (menu) {
        menu->setSelectedIndex(state.selectedChoice);
      }
    } else {
      m_sceneGraph->hideChoices();
    }
  }

  return Result<void>::ok();
}

// ============================================================================

} // namespace NovelMind::editor
