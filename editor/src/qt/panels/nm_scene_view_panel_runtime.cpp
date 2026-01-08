#include "NovelMind/editor/editor_runtime_host.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "nm_scene_view_overlays.hpp"

#include <QDebug>
#include <memory>
#include <new>

namespace NovelMind::editor::qt {

void NMSceneViewPanel::onPlayModeCurrentNodeChanged(const QString &nodeId) {
  if (!m_scene) {
    return;
  }

  qDebug() << "[SceneView] Play mode node changed:" << nodeId;
  if (nodeId.isEmpty()) {
    if (m_playOverlay) {
      m_playOverlay->clearDialogue();
      m_playOverlay->clearChoices();
    }
    return;
  }

  if (!m_followPlayModeNodes || !m_playModeActive) {
    return;
  }

  if (nodeId == m_currentSceneId || m_isLoadingScene) {
    return;
  }

  if (!loadSceneDocument(nodeId)) {
    qWarning() << "[SceneView] Failed to load scene for node:" << nodeId;
  }
}

void NMSceneViewPanel::onPlayModeDialogueChanged(const QString &speaker,
                                                 const QString &text) {
  if (!m_playOverlay) {
    return;
  }
  if (text.isEmpty()) {
    m_playOverlay->clearDialogue();
  } else {
    m_playOverlay->setDialogue(speaker, text);
  }
}

void NMSceneViewPanel::onPlayModeChoicesChanged(const QStringList &choices) {
  if (!m_playOverlay) {
    return;
  }
  if (choices.isEmpty()) {
    m_playOverlay->clearChoices();
  } else {
    m_playOverlay->setChoices(choices);
  }
}

void NMSceneViewPanel::setStoryPreview(const QString &speaker,
                                       const QString &text,
                                       const QStringList &choices) {
  if (!m_playOverlay) {
    return;
  }
  m_editorPreviewSpeaker = speaker;
  m_editorPreviewText = text;
  m_editorPreviewChoices = choices;
  m_editorPreviewActive = !text.trimmed().isEmpty() || !choices.isEmpty();

  if (m_playModeActive) {
    return;
  }

  applyEditorPreview();
}

void NMSceneViewPanel::clearStoryPreview() {
  m_editorPreviewActive = false;
  m_editorPreviewSpeaker.clear();
  m_editorPreviewText.clear();
  m_editorPreviewChoices.clear();

  if (!m_playOverlay || m_playModeActive) {
    return;
  }

  m_playOverlay->clearDialogue();
  m_playOverlay->clearChoices();
  updatePreviewOverlayVisibility();
}

void NMSceneViewPanel::hideEditorObjectsForRuntime() {
  if (!m_scene) {
    return;
  }

  captureEditorObjectsForRuntime();
  for (auto *obj : m_scene->sceneObjects()) {
    if (!obj || obj->id().startsWith("runtime_")) {
      continue;
    }
    obj->setVisible(false);
  }
}

void NMSceneViewPanel::captureEditorObjectsForRuntime() {
  if (!m_scene) {
    return;
  }

  if (!m_editorVisibility.isEmpty() || !m_editorOpacity.isEmpty()) {
    return;
  }

  m_editorVisibility.clear();
  m_editorOpacity.clear();
  m_editorVisibilitySceneId = m_currentSceneId;
  for (auto *obj : m_scene->sceneObjects()) {
    if (!obj || obj->id().startsWith("runtime_")) {
      continue;
    }
    m_editorVisibility.insert(obj->id(), obj->isVisible());
    m_editorOpacity.insert(obj->id(), obj->opacity());
  }
}

void NMSceneViewPanel::restoreEditorObjectsAfterRuntime() {
  if (!m_scene) {
    return;
  }

  if (!m_editorVisibilitySceneId.isEmpty() &&
      m_currentSceneId != m_editorVisibilitySceneId) {
    return;
  }

  for (auto it = m_editorVisibility.constBegin();
       it != m_editorVisibility.constEnd(); ++it) {
    if (auto *obj = m_scene->findSceneObject(it.key())) {
      obj->setVisible(it.value());
    }
  }
  for (auto it = m_editorOpacity.constBegin(); it != m_editorOpacity.constEnd();
       ++it) {
    if (auto *obj = m_scene->findSceneObject(it.key())) {
      obj->setOpacity(it.value());
    }
  }
  m_editorVisibility.clear();
  m_editorOpacity.clear();
  m_editorVisibilitySceneId.clear();
}

void NMSceneViewPanel::applyRuntimeSnapshot(
    const ::NovelMind::editor::SceneSnapshot &snapshot) {
  // P5.3: Validate scene before runtime snapshot operations
  if (!validateSceneReady()) {
    qWarning() << "[SceneViewPanel] Cannot apply runtime snapshot: scene not "
                  "initialized";
    // Note: We don't show a dialog here as this is called during playback
    // and the user should see an error from the play mode controller instead
    return;
  }

  if (m_followPlayModeNodes) {
    if (m_runtimePreviewActive) {
      clearRuntimePreview();
    }
    updateRuntimePreviewVisibility();
    return;
  }

  const bool enteringRuntime = !m_runtimePreviewActive;

  // Remember current selection to restore if possible
  QString previousSelection;
  if (auto *selected = m_scene->selectedObject()) {
    previousSelection = selected->id();
  }

  if (enteringRuntime) {
    m_editorSelectionBeforeRuntime.clear();
    if (!previousSelection.isEmpty() &&
        !previousSelection.startsWith("runtime_")) {
      m_editorSelectionBeforeRuntime = previousSelection;
    }
  }

  // Remove previous runtime-only objects
  for (const auto &id : m_runtimeObjectIds) {
    m_scene->removeSceneObject(id);
  }
  m_runtimeObjectIds.clear();

  if (enteringRuntime) {
    hideEditorObjectsForRuntime();
    m_scene->clearSelection();
  }

  const QString assetsRoot = QString::fromStdString(
      ProjectManager::instance().getFolderPath(ProjectFolder::Assets));
  m_assetsRoot = assetsRoot;

  if (m_glViewport) {
    m_glViewport->setSnapshot(snapshot, m_assetsRoot);
    syncCameraToPreview();
  }

  if (!m_renderRuntimeSceneObjects) {
    m_runtimePreviewActive = true;
    emit sceneObjectsChanged();
    return;
  }

  auto objectNameFromState = [](const scene::SceneObjectState &state) {
    auto it = state.properties.find("name");
    if (it != state.properties.end()) {
      return QString::fromStdString(it->second);
    }
    return QString::fromStdString(state.id);
  };

  auto textureHintFromState = [](const scene::SceneObjectState &state) {
    const char *keys[] = {"textureId", "texture", "image", "sprite",
                          "background"};
    for (auto *key : keys) {
      auto it = state.properties.find(key);
      if (it != state.properties.end()) {
        return QString::fromStdString(it->second);
      }
    }
    return QString::fromStdString(state.id);
  };

  // Recreate objects from snapshot
  auto toQtType = [](scene::SceneObjectType type) {
    switch (type) {
    case scene::SceneObjectType::Background:
      return NMSceneObjectType::Background;
    case scene::SceneObjectType::Character:
      return NMSceneObjectType::Character;
    case scene::SceneObjectType::EffectOverlay:
      return NMSceneObjectType::Effect;
    default:
      return NMSceneObjectType::UI;
    }
  };

  // P5.3: Create runtime objects with comprehensive validation
  int runtimeIndex = 0;
  int successCount = 0;
  int failureCount = 0;

  for (const auto &state : snapshot.objects) {
    QString baseId = QString::fromStdString(state.id);
    if (baseId.isEmpty()) {
      qDebug() << "[SceneViewPanel] Runtime object has empty ID, generating "
                  "fallback ID";
      baseId = QString::number(runtimeIndex++);
    }
    QString id = QString("runtime_%1").arg(baseId);
    while (m_scene->findSceneObject(id) || m_runtimeObjectIds.contains(id)) {
      id = QString("runtime_%1_%2").arg(baseId).arg(runtimeIndex++);
    }

    // MEM-4 fix: Use unique_ptr for exception-safe allocation
    // Note: new throws std::bad_alloc on failure, it doesn't return nullptr
    try {
      auto objPtr = std::make_unique<NMSceneObject>(id, toQtType(state.type));
      auto *obj = objPtr.get();
      obj->setName(objectNameFromState(state));
      obj->setPos(QPointF(state.x, state.y));
      obj->setZValue(state.zOrder);
      obj->setRotation(state.rotation);
      obj->setScaleXY(state.scaleX, state.scaleY);
      obj->setOpacity(state.alpha);
      obj->setVisible(state.visible);
      obj->setPixmap(
          loadPixmapForAsset(textureHintFromState(state), obj->objectType()));
      m_scene->addSceneObject(objPtr.release()); // Scene takes ownership
      m_runtimeObjectIds << id;
    } catch (const std::bad_alloc &e) {
      qWarning() << "[SceneViewPanel] Failed to allocate runtime object:" << id
                 << "-" << e.what();
      continue;
    }
    // P5.3/MEM-4 fix: Validate allocation before use with defensive programming
    NMSceneObject *obj = nullptr;
    try {
      obj = new NMSceneObject(id, toQtType(state.type));
    } catch (const std::exception &e) {
      qCritical() << "[SceneViewPanel] Exception allocating runtime object:"
                  << id << "error:" << e.what();
      failureCount++;
      continue;
    }

    if (!obj) {
      // This branch is technically unreachable with standard new, but kept
      // for defensive programming in case of custom allocators
      qWarning() << "[SceneViewPanel] Failed to allocate runtime object:" << id;
      failureCount++;
      continue;
    }

    // Configure the object properties
    obj->setName(objectNameFromState(state));
    obj->setPos(QPointF(state.x, state.y));
    obj->setZValue(state.zOrder);
    obj->setRotation(state.rotation);
    obj->setScaleXY(state.scaleX, state.scaleY);
    obj->setOpacity(state.alpha);
    obj->setVisible(state.visible);
    obj->setPixmap(
        loadPixmapForAsset(textureHintFromState(state), obj->objectType()));

    // Add to scene
    m_scene->addSceneObject(obj);
    m_runtimeObjectIds << id;
    successCount++;
  }

  // Log summary of runtime object creation
  if (successCount > 0 || failureCount > 0) {
    qDebug() << "[SceneViewPanel] Runtime snapshot applied: created"
             << successCount << "objects," << failureCount << "failures";
  }

  // Show warning if some objects failed to create
  if (failureCount > 0) {
    qWarning() << "[SceneViewPanel]" << failureCount
               << "runtime objects failed to create";
  }

  m_runtimePreviewActive = true;

  const QString fallbackSelection =
      m_runtimeObjectIds.isEmpty() ? QString() : m_runtimeObjectIds.front();

  // Restore selection if it still exists
  bool restored = false;
  if (!enteringRuntime && !previousSelection.isEmpty() &&
      m_scene->findSceneObject(previousSelection)) {
    m_scene->selectObject(previousSelection);
    restored = true;
  } else if (!fallbackSelection.isEmpty() &&
             m_scene->findSceneObject(fallbackSelection)) {
    m_scene->selectObject(fallbackSelection);
    restored = true;
  }

  if (!restored) {
    m_scene->clearSelection();
  }

  emit sceneObjectsChanged();
  syncRuntimeSelection();
}

void NMSceneViewPanel::onPlayModeChanged(int mode) {
  auto playMode = static_cast<NMPlayModeController::PlayMode>(mode);
  qDebug() << "[SceneView] Play mode changed to:" << playMode;

  if (m_playOverlay) {
    m_playOverlay->setInteractionEnabled(playMode !=
                                         NMPlayModeController::Stopped);
    // Show navigation controls during play mode
    m_playOverlay->setNavigationVisible(playMode != NMPlayModeController::Stopped);
    if (playMode != NMPlayModeController::Stopped) {
      m_playOverlay->setFocus();
    }
  }

  if (playMode != NMPlayModeController::Stopped) {
    m_playModeActive = true;
    if (m_followPlayModeNodes && m_sceneIdBeforePlay.isEmpty()) {
      m_sceneIdBeforePlay = m_currentSceneId;
      if (!m_currentSceneId.isEmpty() && !m_isLoadingScene) {
        saveSceneDocument();
      }
    }
    m_suppressSceneSave = m_followPlayModeNodes;
    captureEditorObjectsForRuntime();
    if (m_scene) {
      m_gridVisibleBeforeRuntime = m_scene->isGridVisible();
      m_scene->setGridVisible(false);
    }
    if (m_infoOverlay) {
      m_infoOverlay->setPlayModeActive(true);
      m_infoOverlay->show();
    }
    if (m_followPlayModeNodes && m_runtimePreviewActive) {
      clearRuntimePreview();
    }
  }

  if (playMode == NMPlayModeController::Stopped) {
    m_playModeActive = false;
    m_suppressSceneSave = false;
    // Hide all runtime UI elements when stopped
    qDebug() << "[SceneView] Hiding runtime UI elements (playback stopped)";
    if (m_playOverlay && !m_editorPreviewActive) {
      m_playOverlay->clearDialogue();
      m_playOverlay->clearChoices();
    }
    if (m_scene) {
      m_scene->setGridVisible(m_gridVisibleBeforeRuntime);
    }
    if (m_infoOverlay) {
      m_infoOverlay->setPlayModeActive(false);
      m_infoOverlay->show();
    }
    clearRuntimePreview();

    if (!m_sceneIdBeforePlay.isEmpty() &&
        m_sceneIdBeforePlay != m_currentSceneId) {
      const bool prevSuppress = m_suppressSceneSave;
      m_suppressSceneSave = true;
      loadSceneDocument(m_sceneIdBeforePlay);
      m_suppressSceneSave = prevSuppress;
    }
    m_sceneIdBeforePlay.clear();

    applyEditorPreview();
  }

  updateRuntimePreviewVisibility();
  updatePreviewOverlayVisibility();
}

void NMSceneViewPanel::syncRuntimeSelection() {
  // If selection is a runtime object, keep inspector/hierarchy in sync
  if (!m_scene) {
    return;
  }
  if (auto *selected = m_scene->selectedObject()) {
    emit objectSelected(selected->id());
  }
}

void NMSceneViewPanel::clearRuntimePreview() {
  if (!m_scene) {
    return;
  }

  bool selectionWasRuntime = false;
  if (auto *selected = m_scene->selectedObject()) {
    selectionWasRuntime = m_runtimeObjectIds.contains(selected->id());
  }

  for (const auto &id : m_runtimeObjectIds) {
    m_scene->removeSceneObject(id);
  }
  m_runtimeObjectIds.clear();
  m_runtimePreviewActive = false;

  restoreEditorObjectsAfterRuntime();

  if (selectionWasRuntime) {
    m_scene->clearSelection();
    if (m_infoOverlay) {
      m_infoOverlay->clearSelectedObjectInfo();
    }
  }

  if (!m_editorSelectionBeforeRuntime.isEmpty()) {
    if (m_scene->findSceneObject(m_editorSelectionBeforeRuntime)) {
      m_scene->selectObject(m_editorSelectionBeforeRuntime);
    }
    m_editorSelectionBeforeRuntime.clear();
  }

  if (m_glViewport) {
    m_glViewport->setSnapshot(::NovelMind::editor::SceneSnapshot{},
                              m_assetsRoot);
  }

  emit sceneObjectsChanged();
}

void NMSceneViewPanel::updateRuntimePreviewVisibility() {
  if (!m_glViewport) {
    return;
  }

  const bool showGL = m_playModeActive && !m_followPlayModeNodes &&
                      !m_renderRuntimeSceneObjects;
  m_glViewport->setVisible(showGL);

  if (m_fontWarning) {
    const QString status = m_glViewport->fontAtlasStatus();
    const bool showWarning = showGL && !status.isEmpty();
    m_fontWarning->setText(status);
    m_fontWarning->setVisible(showWarning);
  }
}

void NMSceneViewPanel::applyEditorPreview() {
  if (!m_playOverlay || m_playModeActive) {
    return;
  }

  if (m_editorPreviewActive) {
    m_playOverlay->setDialogueImmediate(m_editorPreviewSpeaker,
                                        m_editorPreviewText);
    // In editor preview mode, do not show choices
    // Choices should only appear during actual play mode
    m_playOverlay->clearChoices();
    m_playOverlay->setInteractionEnabled(false);
  } else {
    m_playOverlay->clearDialogue();
    m_playOverlay->clearChoices();
  }

  updatePreviewOverlayVisibility();
}

void NMSceneViewPanel::updatePreviewOverlayVisibility() {
  if (!m_playOverlay) {
    return;
  }
  const bool visible = m_playModeActive || m_editorPreviewActive;
  m_playOverlay->setVisible(visible);

  // Show preview badge only in editor preview mode, not in actual play mode
  m_playOverlay->setPreviewMode(m_editorPreviewActive && !m_playModeActive);
}

// P5.3 - Scene validation helpers for runtime object allocation

bool NMSceneViewPanel::validateSceneReady() const {
  if (!m_scene) {
    qWarning() << "[SceneViewPanel] Scene validation failed: scene is nullptr";
    return false;
  }
  return true;
}

void NMSceneViewPanel::showSceneNotLoadedError() {
  qWarning() << "[SceneViewPanel] Cannot perform operation: scene not loaded";
  NMMessageDialog::showError(
      const_cast<NMSceneViewPanel *>(this), tr("Scene Not Loaded"),
      tr("Cannot perform this operation because no scene is loaded.\n\n"
         "Please load or create a scene first:\n"
         "• File → New Scene\n"
         "• File → Open Scene"));
}

void NMSceneViewPanel::showSceneInvalidError() {
  qWarning() << "[SceneViewPanel] Cannot perform operation: scene is invalid";
  NMMessageDialog::showWarning(
      const_cast<NMSceneViewPanel *>(this), tr("Invalid Scene State"),
      tr("The scene is in an invalid state and cannot perform this "
         "operation.\n\n"
         "Try reloading the scene or creating a new one."));
}

void NMSceneViewPanel::showRuntimeObjectCreationError(const QString &objectId,
                                                      const QString &reason) {
  qWarning() << "[SceneViewPanel] Failed to create runtime object:" << objectId
             << "reason:" << reason;
  NMMessageDialog::showError(const_cast<NMSceneViewPanel *>(this),
                             tr("Object Creation Failed"),
                             tr("Failed to create runtime object '%1'.\n\n"
                                "Reason: %2\n\n"
                                "Check the console for more details.")
                                 .arg(objectId)
                                 .arg(reason));
}

} // namespace NovelMind::editor::qt
