#include "NovelMind/editor/editor_runtime_host.hpp"

namespace NovelMind::editor {

// ============================================================================
// Inspection APIs
// ============================================================================

SceneSnapshot EditorRuntimeHost::getSceneSnapshot() const {
  SceneSnapshot snapshot;

  if (!m_sceneGraph) {
    snapshot.camera.valid = false;
    return snapshot;
  }

  snapshot.currentSceneId = m_sceneGraph->getSceneId();
  snapshot.camera.valid = false;

  // Get background
  const auto& bgLayer = const_cast<scene::SceneGraph*>(m_sceneGraph.get())->getBackgroundLayer();
  const auto& bgObjects = bgLayer.getObjects();
  if (!bgObjects.empty()) {
    if (auto* bg = dynamic_cast<scene::BackgroundObject*>(bgObjects[0].get())) {
      snapshot.activeBackground = bg->getTextureId();
    }
  }

  // Get visible characters
  const auto& charLayer = const_cast<scene::SceneGraph*>(m_sceneGraph.get())->getCharacterLayer();
  for (const auto& obj : charLayer.getObjects()) {
    if (obj->isVisible()) {
      snapshot.visibleCharacters.push_back(obj->getId());
      if (auto* charObj = dynamic_cast<scene::CharacterObject*>(obj.get())) {
        snapshot.characterExpressions.emplace_back(charObj->getCharacterId(),
                                                   charObj->getExpression());
      }
    }
  }

  // Get dialogue state
  const auto& uiLayer = const_cast<scene::SceneGraph*>(m_sceneGraph.get())->getUILayer();
  for (const auto& obj : uiLayer.getObjects()) {
    if (obj->getType() == scene::SceneObjectType::DialogueUI) {
      if (auto* dlg = dynamic_cast<scene::DialogueUIObject*>(obj.get())) {
        snapshot.dialogueVisible = dlg->isVisible();
        snapshot.dialogueSpeaker = dlg->getSpeaker();
        snapshot.dialogueText = dlg->getText();
      }
    } else if (obj->getType() == scene::SceneObjectType::ChoiceUI) {
      if (auto* choice = dynamic_cast<scene::ChoiceUIObject*>(obj.get())) {
        snapshot.choiceMenuVisible = choice->isVisible();
        for (const auto& opt : choice->getChoices()) {
          snapshot.choiceOptions.push_back(opt.text);
        }
        snapshot.selectedChoice = choice->getSelectedIndex();
      }
    }
  }

  // Serialize all visible objects for editor preview
  auto collectLayer = [&](const scene::Layer& layer) {
    const auto& objects = layer.getObjects();
    for (const auto& obj : objects) {
      if (!obj) {
        continue;
      }
      snapshot.objects.push_back(obj->saveState());
    }
  };

  collectLayer(bgLayer);
  collectLayer(charLayer);
  collectLayer(const_cast<scene::SceneGraph*>(m_sceneGraph.get())->getUILayer());
  collectLayer(const_cast<scene::SceneGraph*>(m_sceneGraph.get())->getEffectLayer());

  return snapshot;
}

ScriptCallStack EditorRuntimeHost::getScriptCallStack() const {
  ScriptCallStack stack;

  if (!m_scriptRuntime) {
    return stack;
  }

  // Build a minimal stack from current scene/IP
  const auto& vm = const_cast<scripting::ScriptRuntime*>(m_scriptRuntime.get())->getVM();

  CallStackEntry entry;
  entry.sceneName = m_scriptRuntime->getCurrentScene();
  entry.instructionPointer = vm.getIP();
  entry.sourceLocation = {};

  stack.frames.push_back(entry);
  stack.currentDepth = 1;

  return stack;
}

std::unordered_map<std::string, scripting::Value> EditorRuntimeHost::getVariables() const {
  if (!m_scriptRuntime) {
    return {};
  }
  return m_scriptRuntime->getAllVariables();
}

scripting::Value EditorRuntimeHost::getVariable(const std::string& name) const {
  if (m_scriptRuntime) {
    return m_scriptRuntime->getVariable(name);
  }
  return scripting::Value{};
}

void EditorRuntimeHost::setVariable(const std::string& name, const scripting::Value& value) {
  if (m_scriptRuntime) {
    m_scriptRuntime->setVariable(name, value);
    if (m_onVariableChanged) {
      m_onVariableChanged(name, value);
    }
  }
}

std::unordered_map<std::string, bool> EditorRuntimeHost::getFlags() const {
  if (!m_scriptRuntime) {
    return {};
  }
  return m_scriptRuntime->getAllFlags();
}

bool EditorRuntimeHost::getFlag(const std::string& name) const {
  if (m_scriptRuntime) {
    return m_scriptRuntime->getFlag(name);
  }
  return false;
}

void EditorRuntimeHost::setFlag(const std::string& name, bool value) {
  if (m_scriptRuntime) {
    m_scriptRuntime->setFlag(name, value);
  }
}

std::vector<std::string> EditorRuntimeHost::getScenes() const {
  return m_sceneNames;
}

std::string EditorRuntimeHost::getCurrentScene() const {
  if (m_scriptRuntime) {
    return m_scriptRuntime->getCurrentScene();
  }
  return "";
}

// ============================================================================

} // namespace NovelMind::editor
