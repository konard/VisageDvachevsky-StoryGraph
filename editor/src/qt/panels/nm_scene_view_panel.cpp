#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "nm_scene_view_overlays.hpp"

#include <QDebug>

namespace NovelMind::editor::qt {

NMSceneViewPanel::NMSceneViewPanel(QWidget *parent)
    : NMDockPanel(tr("Scene View"), parent) {
  setPanelId("SceneView");

  // Scene View is a central panel that needs sufficient space for
  // the visual canvas, toolbar, and status overlays
  setMinimumPanelSize(400, 300);

  setupContent();
  setupToolBar();
}

NMSceneViewPanel::~NMSceneViewPanel() = default;

void NMSceneViewPanel::onInitialize() {
  // Center the view on origin
  if (m_view) {
    m_view->centerOnScene();
  }

  // Connect to Play Mode Controller for runtime preview
  auto &playController = NMPlayModeController::instance();
  connect(&playController, &NMPlayModeController::currentNodeChanged, this,
          &NMSceneViewPanel::onPlayModeCurrentNodeChanged);
  connect(&playController, &NMPlayModeController::dialogueLineChanged, this,
          &NMSceneViewPanel::onPlayModeDialogueChanged);
  connect(&playController, &NMPlayModeController::choicesChanged, this,
          &NMSceneViewPanel::onPlayModeChoicesChanged);
  connect(&playController, &NMPlayModeController::playModeChanged, this,
          &NMSceneViewPanel::onPlayModeChanged);
  connect(&playController, &NMPlayModeController::sceneSnapshotUpdated, this,
          [this, &playController]() {
            applyRuntimeSnapshot(playController.lastSnapshot());
          });

  qDebug() << "[SceneView] Connected to PlayModeController for runtime preview";

  connect(this, &NMSceneViewPanel::sceneObjectsChanged, this, [this]() {
    if (!m_isLoadingScene && !m_runtimePreviewActive && !m_playModeActive &&
        !m_suppressSceneSave) {
      saveSceneDocument();
    }
  });
  connect(this, &NMSceneViewPanel::objectTransformFinished, this,
          [this](const QString &, const QPointF &, const QPointF &, qreal,
                 qreal, qreal, qreal, qreal, qreal) {
            if (!m_isLoadingScene && !m_runtimePreviewActive &&
                !m_playModeActive && !m_suppressSceneSave) {
              saveSceneDocument();
            }
          });
}

void NMSceneViewPanel::onUpdate(double /*deltaTime*/) {
  syncCameraToPreview();
  if (m_glViewport) {
    m_glViewport->update();
  }
}

void NMSceneViewPanel::onResize(int /*width*/, int /*height*/) {
  updateInfoOverlay();
  syncCameraToPreview();
  if (m_glViewport) {
    m_glViewport->update();
  }
}

void NMSceneViewPanel::setGridVisible(bool visible) {
  if (m_scene) {
    m_scene->setGridVisible(visible);
  }
}

void NMSceneViewPanel::setZoomLevel(qreal zoom) {
  if (m_view) {
    m_view->setZoomLevel(zoom);
  }
}

void NMSceneViewPanel::setGizmoMode(NMTransformGizmo::GizmoMode mode) {
  if (m_scene) {
    m_scene->setGizmoMode(mode);
  }
}

void NMSceneViewPanel::setAnimationPreviewMode(bool enabled) {
  if (m_animationPreviewMode == enabled) {
    return;
  }

  m_animationPreviewMode = enabled;

  if (enabled) {
    qDebug() << "[SceneView] Animation preview mode enabled";
    // Disable editing during preview
    if (m_scene) {
      // Could disable gizmos, etc.
    }
  } else {
    qDebug() << "[SceneView] Animation preview mode disabled";
    // Re-enable editing
  }

  // Request redraw
  if (m_view) {
    m_view->viewport()->update();
  }
}

} // namespace NovelMind::editor::qt
