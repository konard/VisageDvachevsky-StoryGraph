#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "nm_scene_view_overlays.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QDateTime>
#include <QFileInfo>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QShortcut>
#include <QStackedLayout>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace NovelMind::editor::qt {

void NMSceneViewPanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("SceneViewToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  auto &iconMgr = NMIconManager::instance();

  // Zoom controls
  QAction *actionZoomIn =
      m_toolBar->addAction(iconMgr.getIcon("zoom-in"), tr("Zoom In"));
  actionZoomIn->setToolTip(tr("Zoom In (Scroll Up)"));
  connect(actionZoomIn, &QAction::triggered, this, &NMSceneViewPanel::onZoomIn);

  QAction *actionZoomOut =
      m_toolBar->addAction(iconMgr.getIcon("zoom-out"), tr("Zoom Out"));
  actionZoomOut->setToolTip(tr("Zoom Out (Scroll Down)"));
  connect(actionZoomOut, &QAction::triggered, this,
          &NMSceneViewPanel::onZoomOut);

  QAction *actionZoomReset =
      m_toolBar->addAction(iconMgr.getIcon("zoom-fit"), tr("Reset"));
  actionZoomReset->setToolTip(tr("Reset Zoom (1:1)"));
  connect(actionZoomReset, &QAction::triggered, this,
          &NMSceneViewPanel::onZoomReset);

  m_toolBar->addSeparator();

  // Grid toggle
  QAction *actionToggleGrid = m_toolBar->addAction(tr("Grid"));
  actionToggleGrid->setToolTip(tr("Toggle Grid (G)"));
  actionToggleGrid->setCheckable(true);
  actionToggleGrid->setChecked(true);
  connect(actionToggleGrid, &QAction::toggled, this,
          &NMSceneViewPanel::onToggleGrid);

  QAction *actionToggleSnap = m_toolBar->addAction(tr("Snap"));
  actionToggleSnap->setToolTip(tr("Snap to Grid"));
  actionToggleSnap->setCheckable(true);
  actionToggleSnap->setChecked(false);
  connect(actionToggleSnap, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setSnapToGrid(enabled);
    }
  });

  auto *guidesMenu = new QMenu(this);
  QAction *actionStageGuides = guidesMenu->addAction(tr("Stage Frame"));
  actionStageGuides->setCheckable(true);
  actionStageGuides->setChecked(true);
  connect(actionStageGuides, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setStageGuidesVisible(enabled);
    }
  });
  QAction *actionSafeGuides = guidesMenu->addAction(tr("Safe Frame"));
  actionSafeGuides->setCheckable(true);
  actionSafeGuides->setChecked(true);
  connect(actionSafeGuides, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setSafeFrameVisible(enabled);
    }
  });
  QAction *actionBaseline = guidesMenu->addAction(tr("Baseline"));
  actionBaseline->setCheckable(true);
  actionBaseline->setChecked(true);
  connect(actionBaseline, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setBaselineVisible(enabled);
    }
  });

  auto *guidesButton = new QToolButton(m_toolBar);
  guidesButton->setText(tr("Guides"));
  guidesButton->setMenu(guidesMenu);
  guidesButton->setPopupMode(QToolButton::InstantPopup);
  m_toolBar->addWidget(guidesButton);

  m_toolBar->addSeparator();

  // Object creation buttons
  QAction *actionAddCharacter = m_toolBar->addAction(
      iconMgr.getIcon("object-character"), tr("Character"));
  actionAddCharacter->setToolTip(tr("Add Character"));
  connect(actionAddCharacter, &QAction::triggered, this, [this]() {
    if (m_scene) {
      SceneObjectSnapshot snapshot;
      snapshot.id =
          QString("character_%1").arg(QDateTime::currentMSecsSinceEpoch());
      snapshot.name = "New Character";
      snapshot.type = NMSceneObjectType::Character;
      snapshot.position = QPointF(0, 0);
      snapshot.scaleX = 1.0;
      snapshot.scaleY = 1.0;
      snapshot.opacity = 1.0;
      snapshot.visible = true;
      snapshot.zValue = 0.0;
      NMUndoManager::instance().pushCommand(
          new AddObjectCommand(this, snapshot));
    }
  });

  QAction *actionAddBackground = m_toolBar->addAction(
      iconMgr.getIcon("object-background"), tr("Background"));
  actionAddBackground->setToolTip(tr("Add Background"));
  connect(actionAddBackground, &QAction::triggered, this, [this]() {
    if (m_scene) {
      SceneObjectSnapshot snapshot;
      snapshot.id =
          QString("background_%1").arg(QDateTime::currentMSecsSinceEpoch());
      snapshot.name = "New Background";
      snapshot.type = NMSceneObjectType::Background;
      snapshot.position = QPointF(0, 0);
      snapshot.scaleX = 1.0;
      snapshot.scaleY = 1.0;
      snapshot.opacity = 1.0;
      snapshot.visible = true;
      snapshot.zValue = 0.0;
      NMUndoManager::instance().pushCommand(
          new AddObjectCommand(this, snapshot));
    }
  });

  QAction *actionAddUI =
      m_toolBar->addAction(iconMgr.getIcon("object-ui"), tr("UI"));
  actionAddUI->setToolTip(tr("Add UI Element"));
  connect(actionAddUI, &QAction::triggered, this, [this]() {
    if (m_scene) {
      SceneObjectSnapshot snapshot;
      snapshot.id = QString("ui_%1").arg(QDateTime::currentMSecsSinceEpoch());
      snapshot.name = "New UI Element";
      snapshot.type = NMSceneObjectType::UI;
      snapshot.position = QPointF(0, 0);
      snapshot.scaleX = 1.0;
      snapshot.scaleY = 1.0;
      snapshot.opacity = 1.0;
      snapshot.visible = true;
      snapshot.zValue = 0.0;
      NMUndoManager::instance().pushCommand(
          new AddObjectCommand(this, snapshot));
    }
  });

  QAction *actionAddEffect =
      m_toolBar->addAction(iconMgr.getIcon("object-effect"), tr("Effect"));
  actionAddEffect->setToolTip(tr("Add Effect"));
  connect(actionAddEffect, &QAction::triggered, this, [this]() {
    if (m_scene) {
      SceneObjectSnapshot snapshot;
      snapshot.id =
          QString("effect_%1").arg(QDateTime::currentMSecsSinceEpoch());
      snapshot.name = "New Effect";
      snapshot.type = NMSceneObjectType::Effect;
      snapshot.position = QPointF(0, 0);
      snapshot.scaleX = 1.0;
      snapshot.scaleY = 1.0;
      snapshot.opacity = 1.0;
      snapshot.visible = true;
      snapshot.zValue = 0.0;
      NMUndoManager::instance().pushCommand(
          new AddObjectCommand(this, snapshot));
    }
  });

  enum class AlignMode { Left, CenterX, Right, Top, CenterY, Bottom };
  auto alignSelected = [this](AlignMode mode) {
    if (!m_scene) {
      return;
    }
    auto *obj = m_scene->selectedObject();
    if (!obj) {
      return;
    }
    const QRectF stage = m_scene->stageRect();
    const QRectF bounds = obj->sceneBoundingRect();
    QPointF newPos = obj->pos();

    switch (mode) {
    case AlignMode::Left:
      newPos.setX(newPos.x() + (stage.left() - bounds.left()));
      break;
    case AlignMode::CenterX:
      newPos.setX(newPos.x() + (stage.center().x() - bounds.center().x()));
      break;
    case AlignMode::Right:
      newPos.setX(newPos.x() + (stage.right() - bounds.right()));
      break;
    case AlignMode::Top:
      newPos.setY(newPos.y() + (stage.top() - bounds.top()));
      break;
    case AlignMode::CenterY:
      newPos.setY(newPos.y() + (stage.center().y() - bounds.center().y()));
      break;
    case AlignMode::Bottom:
      newPos.setY(newPos.y() + (stage.bottom() - bounds.bottom()));
      break;
    }

    if (newPos == obj->pos()) {
      return;
    }
    const QPointF oldPos = obj->pos();
    m_scene->setObjectPosition(obj->id(), newPos);
    NMUndoManager::instance().pushCommand(
        new TransformObjectCommand(this, obj->id(), oldPos, newPos));
  };

  auto adjustZ = [this](int mode) {
    if (!m_scene) {
      return;
    }
    auto *obj = m_scene->selectedObject();
    if (!obj) {
      return;
    }
    qreal oldZ = obj->zValue();
    qreal newZ = oldZ;
    qreal minZ = oldZ;
    qreal maxZ = oldZ;
    for (auto *other : m_scene->sceneObjects()) {
      if (!other) {
        continue;
      }
      minZ = std::min(minZ, other->zValue());
      maxZ = std::max(maxZ, other->zValue());
    }

    switch (mode) {
    case -2: // send to back
      newZ = minZ - 1.0;
      break;
    case -1: // move down
      newZ = oldZ - 1.0;
      break;
    case 1: // move up
      newZ = oldZ + 1.0;
      break;
    case 2: // bring to front
      newZ = maxZ + 1.0;
      break;
    default:
      break;
    }

    if (qFuzzyCompare(oldZ + 1.0, newZ + 1.0)) {
      return;
    }
    m_scene->setObjectZOrder(obj->id(), newZ);
    emit sceneObjectsChanged();
  };

  auto *alignMenu = new QMenu(this);
  QAction *actionAlignLeft = alignMenu->addAction(tr("Align Left"));
  QAction *actionAlignCenter = alignMenu->addAction(tr("Align Center"));
  QAction *actionAlignRight = alignMenu->addAction(tr("Align Right"));
  alignMenu->addSeparator();
  QAction *actionAlignTop = alignMenu->addAction(tr("Align Top"));
  QAction *actionAlignMiddle = alignMenu->addAction(tr("Align Middle"));
  QAction *actionAlignBottom = alignMenu->addAction(tr("Align Bottom"));

  connect(actionAlignLeft, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::Left); });
  connect(actionAlignCenter, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::CenterX); });
  connect(actionAlignRight, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::Right); });
  connect(actionAlignTop, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::Top); });
  connect(actionAlignMiddle, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::CenterY); });
  connect(actionAlignBottom, &QAction::triggered, this,
          [alignSelected]() { alignSelected(AlignMode::Bottom); });

  auto *alignButton = new QToolButton(m_toolBar);
  alignButton->setText(tr("Align"));
  alignButton->setMenu(alignMenu);
  alignButton->setPopupMode(QToolButton::InstantPopup);
  m_toolBar->addWidget(alignButton);

  auto *orderMenu = new QMenu(this);
  QAction *actionBringFront = orderMenu->addAction(tr("Bring to Front"));
  QAction *actionSendBack = orderMenu->addAction(tr("Send to Back"));
  orderMenu->addSeparator();
  QAction *actionMoveForward = orderMenu->addAction(tr("Move Forward"));
  QAction *actionMoveBackward = orderMenu->addAction(tr("Move Backward"));

  connect(actionBringFront, &QAction::triggered, this,
          [adjustZ]() { adjustZ(2); });
  connect(actionSendBack, &QAction::triggered, this,
          [adjustZ]() { adjustZ(-2); });
  connect(actionMoveForward, &QAction::triggered, this,
          [adjustZ]() { adjustZ(1); });
  connect(actionMoveBackward, &QAction::triggered, this,
          [adjustZ]() { adjustZ(-1); });

  auto *orderButton = new QToolButton(m_toolBar);
  orderButton->setText(tr("Order"));
  orderButton->setMenu(orderMenu);
  orderButton->setPopupMode(QToolButton::InstantPopup);
  m_toolBar->addWidget(orderButton);

  m_toolBar->addSeparator();

  // Gizmo mode buttons (exclusive)
  auto *gizmoGroup = new QButtonGroup(this);

  QAction *actionMove =
      m_toolBar->addAction(iconMgr.getIcon("transform-move"), tr("Move"));
  actionMove->setToolTip(tr("Move Gizmo (W)"));
  actionMove->setCheckable(true);
  actionMove->setChecked(true);
  gizmoGroup->addButton(m_toolBar->widgetForAction(actionMove)
                            ? qobject_cast<QAbstractButton *>(
                                  m_toolBar->widgetForAction(actionMove))
                            : nullptr);
  connect(actionMove, &QAction::triggered, this,
          &NMSceneViewPanel::onGizmoModeMove);

  QAction *actionRotate =
      m_toolBar->addAction(iconMgr.getIcon("transform-rotate"), tr("Rotate"));
  actionRotate->setToolTip(tr("Rotate Gizmo (E)"));
  actionRotate->setCheckable(true);
  connect(actionRotate, &QAction::triggered, this,
          &NMSceneViewPanel::onGizmoModeRotate);

  QAction *actionScale =
      m_toolBar->addAction(iconMgr.getIcon("transform-scale"), tr("Scale"));
  actionScale->setToolTip(tr("Scale Gizmo (R)"));
  actionScale->setCheckable(true);
  connect(actionScale, &QAction::triggered, this,
          &NMSceneViewPanel::onGizmoModeScale);

  // Insert toolbar at top of content widget
  if (auto *layout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_toolBar);
  }
}

void NMSceneViewPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create graphics scene and view
  m_scene = new NMSceneGraphicsScene(this);
  m_view = new NMSceneGraphicsView(m_contentWidget);
  m_view->setScene(m_scene);
  m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setStyleSheet("background: transparent;");
  if (auto *vp = m_view->viewport()) {
    vp->setAttribute(Qt::WA_TranslucentBackground);
    vp->setAutoFillBackground(false);
  }

  // GL viewport for real renderer preview
  m_glViewport = new NMSceneGLViewport(m_contentWidget);

  // Create info overlay
  m_infoOverlay = new NMSceneInfoOverlay(m_view);
  m_infoOverlay->setGeometry(m_view->rect());
  m_playOverlay = new NMPlayPreviewOverlay(m_view);
  m_playOverlay->setGeometry(m_view->rect());
  m_playOverlay->setInteractionEnabled(false);

  m_fontWarning = new QLabel(m_contentWidget);
  m_fontWarning->setObjectName("SceneViewFontWarning");
  m_fontWarning->setAlignment(Qt::AlignCenter);
  m_fontWarning->setWordWrap(true);
  m_fontWarning->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_fontWarning->hide();

  auto *stack = new QStackedLayout();
  stack->setStackingMode(QStackedLayout::StackAll);
  stack->addWidget(m_glViewport);
  stack->addWidget(m_view);
  stack->addWidget(m_fontWarning);
  stack->setCurrentWidget(m_view);
  layout->addLayout(stack);

  // SceneView keyboard shortcuts
  auto registerShortcut = [this](const QKeySequence &seq,
                                 const std::function<void()> &slot) {
    auto *sc = new QShortcut(seq, this);
    connect(sc, &QShortcut::activated, this, slot);
  };
  registerShortcut(QKeySequence("F"), [this]() { frameSelected(); });
  registerShortcut(QKeySequence("A"), [this]() { frameAll(); });
  registerShortcut(QKeySequence("G"), [this]() { toggleGrid(); });
  registerShortcut(QKeySequence("H"),
                   [this]() { toggleSelectionVisibility(); });
  registerShortcut(QKeySequence::Copy, [this]() { copySelectedObject(); });
  registerShortcut(QKeySequence::Paste, [this]() { pasteClipboardObject(); });
  registerShortcut(QKeySequence(Qt::CTRL | Qt::Key_D),
                   [this]() { duplicateSelectedObject(); });
  registerShortcut(QKeySequence(Qt::Key_Delete),
                   [this]() { deleteSelectedObject(); });
  registerShortcut(QKeySequence(Qt::Key_Backspace),
                   [this]() { deleteSelectedObject(); });
  registerShortcut(QKeySequence(Qt::Key_F2),
                   [this]() { renameSelectedObject(); });
  registerShortcut(QKeySequence(Qt::Key_Space), []() {
    auto &playController = NMPlayModeController::instance();
    if (playController.isPlaying() || playController.isPaused()) {
      playController.advanceDialogue();
    }
  });
  registerShortcut(QKeySequence(Qt::Key_Return), []() {
    auto &playController = NMPlayModeController::instance();
    if (playController.isPlaying() || playController.isPaused()) {
      playController.advanceDialogue();
    }
  });

  setContentWidget(m_contentWidget);
  setWindowTitle(tr("Scene View (WYSIWYG Preview)"));

  // Connect signals
  connect(m_view, &NMSceneGraphicsView::cursorPositionChanged, this,
          &NMSceneViewPanel::onCursorPositionChanged);
  connect(m_view, &NMSceneGraphicsView::assetsDropped, this,
          &NMSceneViewPanel::onAssetsDropped);
  connect(m_view, &NMSceneGraphicsView::contextMenuRequested, this,
          &NMSceneViewPanel::onContextMenuRequested);
  connect(m_view, &NMSceneGraphicsView::dragActiveChanged, this,
          &NMSceneViewPanel::onDragActiveChanged);
  connect(m_scene, &NMSceneGraphicsScene::objectSelected, this,
          &NMSceneViewPanel::onSceneObjectSelected);
  connect(m_scene, &NMSceneGraphicsScene::objectPositionChanged, this,
          &NMSceneViewPanel::onObjectPositionChanged);
  connect(m_scene, &NMSceneGraphicsScene::objectMoveFinished, this,
          &NMSceneViewPanel::onObjectMoveFinished);
  connect(m_scene, &NMSceneGraphicsScene::objectTransformFinished, this,
          &NMSceneViewPanel::onObjectTransformFinished);
  connect(m_scene, &NMSceneGraphicsScene::deleteRequested, this,
          &NMSceneViewPanel::onDeleteRequested);

  connect(
      m_playOverlay, &NMPlayPreviewOverlay::choiceSelected, this,
      [](int index) { NMPlayModeController::instance().selectChoice(index); });
  connect(m_playOverlay, &NMPlayPreviewOverlay::advanceRequested, this,
          []() { NMPlayModeController::instance().advanceDialogue(); });
  connect(m_playOverlay, &NMPlayPreviewOverlay::previousRequested, this,
          []() { NMPlayModeController::instance().stepBackward(); });
  connect(m_playOverlay, &NMPlayPreviewOverlay::nextRequested, this,
          []() { NMPlayModeController::instance().stepForward(); });

  updateRuntimePreviewVisibility();
}

void NMSceneViewPanel::updateInfoOverlay() {
  // Resize overlay to match view
  if (m_infoOverlay && m_view) {
    m_infoOverlay->setGeometry(m_view->rect());
  }
  if (m_playOverlay && m_view) {
    m_playOverlay->setGeometry(m_view->rect());
  }
  if (m_dropHint && m_view) {
    m_dropHint->setGeometry(m_view->rect().adjusted(40, 40, -40, -40));
  }
}

void NMSceneViewPanel::syncCameraToPreview() {
  if (!m_glViewport || !m_view || !m_view->viewport()) {
    return;
  }
  const QPointF center =
      m_view->mapToScene(m_view->viewport()->rect().center());
  m_glViewport->setViewCamera(center, m_view->zoomLevel());
}

void NMSceneViewPanel::frameSelected() {
  if (!m_scene || !m_view) {
    return;
  }
  if (auto *obj = m_scene->selectedObject()) {
    QRectF rect = obj->sceneBoundingRect();
    m_view->fitInView(rect.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
  }
}

void NMSceneViewPanel::frameAll() {
  if (!m_scene || !m_view) {
    return;
  }
  QRectF rect = m_scene->itemsBoundingRect();
  if (rect.isValid()) {
    m_view->fitInView(rect.adjusted(-80, -80, 80, 80), Qt::KeepAspectRatio);
  }
}

void NMSceneViewPanel::toggleGrid() {
  if (!m_scene) {
    return;
  }
  m_scene->setGridVisible(!m_scene->isGridVisible());
}

void NMSceneViewPanel::toggleSelectionVisibility() {
  if (!m_scene) {
    return;
  }
  if (auto *obj = m_scene->selectedObject()) {
    obj->setVisible(!obj->isVisible());
  }
}

void NMSceneViewPanel::onZoomIn() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() * 1.25);
  }
}

void NMSceneViewPanel::onZoomOut() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() / 1.25);
  }
}

void NMSceneViewPanel::onZoomReset() {
  if (m_view) {
    m_view->setZoomLevel(1.0);
    m_view->centerOnScene();
  }
}

void NMSceneViewPanel::onCenterScene() {
  if (m_view) {
    m_view->centerOnScene();
  }
}

void NMSceneViewPanel::onFitScene() {
  if (m_view) {
    m_view->fitToScene();
  }
}

void NMSceneViewPanel::onToggleGrid() {
  if (m_scene) {
    m_scene->setGridVisible(!m_scene->isGridVisible());
  }
}

void NMSceneViewPanel::onGizmoModeMove() {
  setGizmoMode(NMTransformGizmo::GizmoMode::Move);
}

void NMSceneViewPanel::onGizmoModeRotate() {
  setGizmoMode(NMTransformGizmo::GizmoMode::Rotate);
}

void NMSceneViewPanel::onGizmoModeScale() {
  setGizmoMode(NMTransformGizmo::GizmoMode::Scale);
}

void NMSceneViewPanel::onCursorPositionChanged(const QPointF &scenePos) {
  if (m_infoOverlay) {
    m_infoOverlay->setCursorPosition(scenePos);
  }
}

void NMSceneViewPanel::onAssetsDropped(const QStringList &paths,
                                       const QPointF &scenePos) {
  if (!m_scene || paths.isEmpty()) {
    return;
  }

  QPointF dropPos = scenePos;
  const QPointF offset(32.0, 32.0);
  for (const QString &path : paths) {
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
      dropPos += offset;
      continue;
    }

    const QString ext = info.suffix().toLower();
    const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" ||
                          ext == "bmp" || ext == "gif");
    if (!isImage) {
      dropPos += offset;
      continue;
    }

    auto *selected = m_scene->selectedObject();
    if (selected && selected->sceneBoundingRect().contains(dropPos)) {
      setObjectAsset(selected->id(), path);
    } else {
      addObjectFromAsset(path, dropPos);
    }

    dropPos += offset;
  }
}

void NMSceneViewPanel::onSceneObjectSelected(const QString &objectId) {
  if (m_infoOverlay) {
    if (objectId.isEmpty()) {
      m_infoOverlay->clearSelectedObjectInfo();
    } else if (auto *obj = m_scene->findSceneObject(objectId)) {
      m_infoOverlay->setSelectedObjectInfo(obj->name(), obj->pos());
    }
  }

  // Forward to main window's selection system
  emit objectSelected(objectId);
}

void NMSceneViewPanel::onObjectPositionChanged(const QString &objectId,
                                               const QPointF &position) {
  if (m_infoOverlay && m_scene) {
    if (auto *obj = m_scene->findSceneObject(objectId)) {
      m_infoOverlay->setSelectedObjectInfo(obj->name(), position);
    }
  }
  emit objectPositionChanged(objectId, position);
}

void NMSceneViewPanel::onObjectMoveFinished(const QString &objectId,
                                            const QPointF &oldPos,
                                            const QPointF &newPos) {
  if (objectId.isEmpty()) {
    return;
  }
  // Push a real undo command capturing the positional delta
  NMUndoManager::instance().pushCommand(
      new TransformObjectCommand(this, objectId, oldPos, newPos));
}

void NMSceneViewPanel::onObjectTransformFinished(
    const QString &objectId, const QPointF &oldPos, const QPointF &newPos,
    qreal oldRotation, qreal newRotation, qreal oldScaleX, qreal newScaleX,
    qreal oldScaleY, qreal newScaleY) {
  if (objectId.isEmpty()) {
    return;
  }

  NMUndoManager::instance().pushCommand(new TransformObjectCommand(
      this, objectId, oldPos, newPos, oldRotation, newRotation, oldScaleX,
      newScaleX, oldScaleY, newScaleY));
  emit objectTransformFinished(objectId, oldPos, newPos, oldRotation,
                               newRotation, oldScaleX, newScaleX, oldScaleY,
                               newScaleY);
}

void NMSceneViewPanel::onDeleteRequested(const QString &objectId) {
  if (!m_scene || objectId.isEmpty()) {
    return;
  }

  if (auto *obj = m_scene->findSceneObject(objectId)) {
    SceneObjectSnapshot snapshot;
    snapshot.id = objectId;
    snapshot.name = obj->name();
    snapshot.type = obj->objectType();
    snapshot.position = obj->pos();
    snapshot.rotation = obj->rotation();
    snapshot.scaleX = obj->scaleX();
    snapshot.scaleY = obj->scaleY();
    snapshot.opacity = obj->opacity();
    snapshot.visible = obj->isVisible();
    snapshot.zValue = obj->zValue();
    snapshot.assetPath = obj->assetPath();
    NMUndoManager::instance().pushCommand(
        new DeleteObjectCommand(this, snapshot));
  }
}

void NMSceneViewPanel::onContextMenuRequested(const QPoint &globalPos,
                                              const QPointF &scenePos) {
  Q_UNUSED(scenePos);
  if (!m_view) {
    return;
  }

  const bool hasSelection = m_scene && m_scene->selectedObject();
  const bool canEdit = canEditScene();

  QMenu menu(this);
  QAction *copyAction = menu.addAction(tr("Copy"));
  copyAction->setShortcut(QKeySequence::Copy);
  copyAction->setEnabled(canEdit && hasSelection);
  connect(copyAction, &QAction::triggered, this,
          [this]() { copySelectedObject(); });

  QAction *pasteAction = menu.addAction(tr("Paste"));
  pasteAction->setShortcut(QKeySequence::Paste);
  pasteAction->setEnabled(canEdit && m_sceneClipboardValid);
  connect(pasteAction, &QAction::triggered, this,
          [this]() { pasteClipboardObject(); });

  QAction *duplicateAction = menu.addAction(tr("Duplicate"));
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  duplicateAction->setEnabled(canEdit && hasSelection);
  connect(duplicateAction, &QAction::triggered, this,
          [this]() { duplicateSelectedObject(); });

  QAction *renameAction = menu.addAction(tr("Rename"));
  renameAction->setShortcut(QKeySequence(Qt::Key_F2));
  renameAction->setEnabled(canEdit && hasSelection);
  connect(renameAction, &QAction::triggered, this,
          [this]() { renameSelectedObject(); });

  QAction *deleteAction = menu.addAction(tr("Delete"));
  deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
  deleteAction->setEnabled(canEdit && hasSelection);
  connect(deleteAction, &QAction::triggered, this,
          [this]() { deleteSelectedObject(); });

  menu.addSeparator();

  QAction *visibilityAction = menu.addAction(tr("Toggle Visibility"));
  visibilityAction->setEnabled(canEdit && hasSelection);
  connect(visibilityAction, &QAction::triggered, this,
          [this]() { toggleSelectedVisibility(); });

  QAction *lockAction = menu.addAction(tr("Lock/Unlock"));
  lockAction->setEnabled(canEdit && hasSelection);
  connect(lockAction, &QAction::triggered, this,
          [this]() { toggleSelectedLocked(); });

  menu.addSeparator();

  QAction *frameSelectedAction = menu.addAction(tr("Frame Selected"));
  frameSelectedAction->setEnabled(m_scene && m_scene->selectedObject());
  connect(frameSelectedAction, &QAction::triggered, this,
          [this]() { frameSelected(); });

  QAction *frameAllAction = menu.addAction(tr("Frame All"));
  connect(frameAllAction, &QAction::triggered, this, [this]() { frameAll(); });

  menu.addSeparator();

  QAction *centerViewAction = menu.addAction(tr("Center View"));
  connect(centerViewAction, &QAction::triggered, this,
          &NMSceneViewPanel::onCenterScene);

  QAction *fitSceneAction = menu.addAction(tr("Fit Scene"));
  connect(fitSceneAction, &QAction::triggered, this,
          &NMSceneViewPanel::onFitScene);

  QAction *resetZoomAction = menu.addAction(tr("Reset Zoom"));
  connect(resetZoomAction, &QAction::triggered, this,
          &NMSceneViewPanel::onZoomReset);

  menu.addSeparator();

  QAction *gridAction = menu.addAction(tr("Show Grid"));
  gridAction->setCheckable(true);
  gridAction->setChecked(m_scene && m_scene->isGridVisible());
  connect(gridAction, &QAction::toggled, this,
          [this](bool enabled) { setGridVisible(enabled); });

  QAction *snapAction = menu.addAction(tr("Snap to Grid"));
  snapAction->setCheckable(true);
  snapAction->setChecked(m_scene && m_scene->snapToGrid());
  connect(snapAction, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setSnapToGrid(enabled);
    }
  });

  menu.exec(globalPos);
}

void NMSceneViewPanel::onDragActiveChanged(bool active) {
  if (!m_view) {
    return;
  }

  if (!m_dropHint) {
    m_dropHint = new QFrame(m_view);
    m_dropHint->setObjectName("SceneDropHint");
    m_dropHint->setStyleSheet(
        "QFrame#SceneDropHint {"
        " background-color: rgba(20, 26, 34, 200);"
        " border: 1px dashed rgba(120, 150, 180, 200);"
        " border-radius: 10px;"
        "}"
        "QLabel { color: rgb(231, 236, 242); font-weight: 600; }");
    auto *layout = new QVBoxLayout(m_dropHint);
    layout->setContentsMargins(16, 16, 16, 16);
    auto *label = new QLabel(tr("Drop assets to add to the scene"), m_dropHint);
    label->setAlignment(Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
  }

  m_dropHint->setVisible(active);
  if (active) {
    m_dropHint->setGeometry(m_view->rect().adjusted(40, 40, -40, -40));
    m_dropHint->raise();
  }
}

// === Play Mode Integration ===

} // namespace NovelMind::editor::qt
