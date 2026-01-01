#pragma once

/**
 * @file nm_scene_view_panel.hpp
 * @brief Scene View panel for visual scene editing
 *
 * Displays the visual novel scene with:
 * - Background image
 * - Character sprites
 * - UI elements
 * - Selection highlighting
 * - Viewport controls (pan, zoom)
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QStringList>
#include <QToolBar>
#include <atomic>

namespace NovelMind::editor {
struct SceneSnapshot;
} // namespace NovelMind::editor

namespace NovelMind::editor::qt {

// Forward declarations
class NMSceneObject;
class NMTransformGizmo;
class NMPlayPreviewOverlay;
class NMSceneGLViewport;

/**
 * @brief Scene object types
 */
enum class NMSceneObjectType { Background, Character, UI, Effect };

/**
 * @brief Scene object representation
 */
class NMSceneObject : public QGraphicsPixmapItem {
public:
  explicit NMSceneObject(const QString &id, NMSceneObjectType type,
                         QGraphicsItem *parent = nullptr);

  [[nodiscard]] QString id() const { return m_id; }
  [[nodiscard]] NMSceneObjectType objectType() const { return m_objectType; }
  [[nodiscard]] QString name() const { return m_name; }
  [[nodiscard]] QString assetPath() const { return m_assetPath; }
  [[nodiscard]] bool isLocked() const { return m_locked; }

  void setName(const QString &name) { m_name = name; }
  void setAssetPath(const QString &path) { m_assetPath = path; }
  void setSelected(bool selected);
  void setLocked(bool locked);
  [[nodiscard]] bool isObjectSelected() const { return m_selected; }
  void setScaleX(qreal scale);
  void setScaleY(qreal scale);
  void setScaleXY(qreal scaleX, qreal scaleY);
  void setUniformScale(qreal scale);
  [[nodiscard]] qreal scaleX() const { return m_scaleX; }
  [[nodiscard]] qreal scaleY() const { return m_scaleY; }

  // Parent-child relationships
  void setParentObjectId(const QString &parentId) {
    m_parentObjectId = parentId;
  }
  [[nodiscard]] QString parentObjectId() const { return m_parentObjectId; }
  [[nodiscard]] QStringList childObjectIds() const { return m_childObjectIds; }
  void addChildObjectId(const QString &childId);
  void removeChildObjectId(const QString &childId);
  void clearChildObjectIds() { m_childObjectIds.clear(); }

  // Tags for filtering
  void addTag(const QString &tag);
  void removeTag(const QString &tag);
  [[nodiscard]] bool hasTag(const QString &tag) const;
  [[nodiscard]] QStringList tags() const { return m_tags; }
  void setTags(const QStringList &tags) { m_tags = tags; }

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant &value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
  QString m_id;
  QString m_name;
  QString m_assetPath;
  NMSceneObjectType m_objectType;
  qreal m_scaleX = 1.0;
  qreal m_scaleY = 1.0;
  bool m_selected = false;
  bool m_locked = false;
  QString m_parentObjectId;
  QStringList m_childObjectIds;
  QStringList m_tags;
};

/**
 * @brief Transform gizmo for moving objects
 */
class NMTransformGizmo : public QGraphicsItemGroup {
public:
  enum class GizmoMode { Move, Rotate, Scale };
  enum class HandleType { XAxis, YAxis, XYPlane, Corner, Rotation };

  explicit NMTransformGizmo(QGraphicsItem *parent = nullptr);

  void setMode(GizmoMode mode);
  [[nodiscard]] GizmoMode mode() const { return m_mode; }

  void setTargetObjectId(const QString &objectId);
  [[nodiscard]] QString targetObjectId() const { return m_targetObjectId; }

  void updatePosition();
  void beginHandleDrag(HandleType type, const QPointF &scenePos);
  void updateHandleDrag(const QPointF &scenePos);
  void endHandleDrag();

private:
  void createMoveGizmo();
  void createRotateGizmo();
  void createScaleGizmo();
  void clearGizmo();
  class NMSceneObject *resolveTarget() const;

  GizmoMode m_mode = GizmoMode::Move;
  QString m_targetObjectId;
  bool m_isDragging = false;
  HandleType m_activeHandle = HandleType::XYPlane;
  QPointF m_dragStartScenePos;
  QPointF m_dragStartTargetPos;
  qreal m_dragStartRotation = 0.0;
  qreal m_dragStartScaleX = 1.0;
  qreal m_dragStartScaleY = 1.0;
  qreal m_dragStartDistance = 1.0;
};

/**
 * @brief Graphics scene for the scene view
 */
class NMSceneGraphicsScene : public QGraphicsScene {
  Q_OBJECT

public:
  explicit NMSceneGraphicsScene(QObject *parent = nullptr);
  ~NMSceneGraphicsScene() override;

  void setGridVisible(bool visible);
  [[nodiscard]] bool isGridVisible() const { return m_gridVisible; }

  void setGridSize(qreal size);
  [[nodiscard]] qreal gridSize() const { return m_gridSize; }
  void setStageGuidesVisible(bool visible);
  void setSafeFrameVisible(bool visible);
  void setBaselineVisible(bool visible);
  void setSnapToGrid(bool enabled);
  [[nodiscard]] bool snapToGrid() const { return m_snapToGrid; }
  [[nodiscard]] QRectF stageRect() const;

  void addSceneObject(NMSceneObject *object);
  void removeSceneObject(const QString &objectId);
  [[nodiscard]] NMSceneObject *findSceneObject(const QString &objectId) const;
  [[nodiscard]] QList<NMSceneObject *> sceneObjects() const {
    return m_sceneObjects;
  }
  [[nodiscard]] QPointF getObjectPosition(const QString &objectId) const;
  bool setObjectPosition(const QString &objectId, const QPointF &pos);
  bool setObjectRotation(const QString &objectId, qreal degrees);
  bool setObjectScale(const QString &objectId, qreal scaleX, qreal scaleY);
  bool setObjectOpacity(const QString &objectId, qreal opacity);
  bool setObjectVisible(const QString &objectId, bool visible);
  bool setObjectLocked(const QString &objectId, bool locked);
  bool setObjectZOrder(const QString &objectId, qreal zValue);
  bool reparentObject(const QString &objectId, const QString &newParentId);
  [[nodiscard]] qreal getObjectRotation(const QString &objectId) const;
  [[nodiscard]] QPointF getObjectScale(const QString &objectId) const;
  [[nodiscard]] bool isObjectLocked(const QString &objectId) const;

  void selectObject(const QString &objectId);
  void clearSelection();
  [[nodiscard]] NMSceneObject *selectedObject() const;
  [[nodiscard]] QString selectedObjectId() const { return m_selectedObjectId; }

  void setGizmoMode(NMTransformGizmo::GizmoMode mode);
  void handleItemPositionChange(const QString &objectId, const QPointF &newPos);

signals:
  void objectSelected(const QString &objectId);
  void objectPositionChanged(const QString &objectId, const QPointF &position);
  void objectMoveFinished(const QString &objectId, const QPointF &oldPosition,
                          const QPointF &newPosition);
  void objectTransformFinished(const QString &objectId,
                               const QPointF &oldPosition,
                               const QPointF &newPosition, qreal oldRotation,
                               qreal newRotation, qreal oldScaleX,
                               qreal newScaleX, qreal oldScaleY,
                               qreal newScaleY);
  void deleteRequested(const QString &objectId);

protected:
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void updateGizmo();
  void resetDragTracking();

  bool m_gridVisible = true;
  qreal m_gridSize = 32.0;
  bool m_stageGuidesVisible = true;
  bool m_safeFrameVisible = true;
  bool m_baselineVisible = true;
  bool m_snapToGrid = false;
  QSizeF m_stageSize = QSizeF(1280.0, 720.0);
  // Scene owns NMSceneObject items; this list is non-owning and maintained only
  // via addSceneObject/removeSceneObject to avoid dangling references.
  QList<NMSceneObject *> m_sceneObjects;
  QString m_selectedObjectId;
  NMTransformGizmo *m_gizmo = nullptr;
  QString m_draggingObjectId;
  QPointF m_dragStartPos;
  bool m_isDraggingObject = false;
};

/**
 * @brief Info overlay widget showing cursor and object info
 */
class NMSceneInfoOverlay : public QWidget {
  Q_OBJECT

public:
  explicit NMSceneInfoOverlay(QWidget *parent = nullptr);

  void setCursorPosition(const QPointF &pos);
  void setSceneInfo(const QString &sceneId);
  void setPlayModeActive(bool active);
  void setSelectedObjectInfo(const QString &name, const QPointF &pos);
  void clearSelectedObjectInfo();

private:
  void updateDisplay();

  QLabel *m_sceneLabel = nullptr;
  QLabel *m_cursorLabel = nullptr;
  QLabel *m_objectLabel = nullptr;
  QPointF m_cursorPos;
  QString m_sceneId;
  QString m_objectName;
  QPointF m_objectPos;
  bool m_hasSelection = false;
  bool m_playModeActive = false;
};

/**
 * @brief Graphics view with pan and zoom support
 */
class NMSceneGraphicsView : public QGraphicsView {
  Q_OBJECT

public:
  explicit NMSceneGraphicsView(QWidget *parent = nullptr);

  void setZoomLevel(qreal zoom);
  [[nodiscard]] qreal zoomLevel() const { return m_zoomLevel; }

  void centerOnScene();
  void fitToScene();

signals:
  void zoomChanged(qreal newZoom);
  void cursorPositionChanged(const QPointF &scenePos);
  void assetsDropped(const QStringList &paths, const QPointF &scenePos);
  void contextMenuRequested(const QPoint &globalPos, const QPointF &scenePos);
  void dragActiveChanged(bool active);

protected:
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  qreal m_zoomLevel = 1.0;
  bool m_isPanning = false;
  QPoint m_lastPanPoint;
};

/**
 * @brief Scene View panel for visual scene editing
 */
class NMSceneViewPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMSceneViewPanel(QWidget *parent = nullptr);
  ~NMSceneViewPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;
  void onResize(int width, int height) override;

  /**
   * @brief Get the graphics scene
   */
  [[nodiscard]] NMSceneGraphicsScene *graphicsScene() const { return m_scene; }

  /**
   * @brief Get the graphics view
   */
  [[nodiscard]] NMSceneGraphicsView *graphicsView() const { return m_view; }

  /**
   * @brief Set grid visibility
   */
  void setGridVisible(bool visible);

  /**
   * @brief Set zoom level
   */
  void setZoomLevel(qreal zoom);

  /**
   * @brief Set gizmo mode
   */
  void setGizmoMode(NMTransformGizmo::GizmoMode mode);

  /**
   * @brief Create a new scene object with given ID and type.
   */
  bool createObject(const QString &id, NMSceneObjectType type,
                    const QPointF &pos = QPointF(0, 0), qreal scale = 1.0);

  /**
   * @brief Delete an object by ID.
   */
  bool deleteObject(const QString &id);

  /**
   * @brief Move an object to a position.
   */
  bool moveObject(const QString &id, const QPointF &pos);

  bool rotateObject(const QString &id, qreal rotation);
  bool scaleObject(const QString &id, qreal scaleX, qreal scaleY);
  bool setObjectOpacity(const QString &id, qreal opacity);
  bool setObjectVisible(const QString &id, bool visible);
  bool setObjectLocked(const QString &id, bool locked);
  bool setObjectZOrder(const QString &id, qreal zValue);
  bool reparentObject(const QString &id, const QString &newParentId);
  bool applyObjectTransform(const QString &id, const QPointF &pos,
                            qreal rotation, qreal scaleX, qreal scaleY);
  bool renameObject(const QString &id, const QString &name);

  /**
   * @brief Select object by ID.
   */
  void selectObjectById(const QString &id);

  NMSceneObject *findObjectById(const QString &id) const;

  bool loadSceneDocument(const QString &sceneId);
  bool saveSceneDocument();
  [[nodiscard]] QString currentSceneId() const { return m_currentSceneId; }
  bool setObjectAsset(const QString &id, const QString &assetPath);
  bool addObjectFromAsset(const QString &assetPath, const QPointF &scenePos);
  bool addObjectFromAsset(const QString &assetPath, const QPointF &scenePos,
                          NMSceneObjectType type);
  void setBreadcrumbContext(const QString &projectName,
                            const QString &graphName, const QString &nodeId,
                            const QString &sceneId, bool playModeActive);
  void setFocusModeActive(bool active);
  void setStoryPreview(const QString &speaker, const QString &text,
                       const QStringList &choices);
  void clearStoryPreview();

  /**
   * @brief Enable/disable animation preview mode
   */
  void setAnimationPreviewMode(bool enabled);

  /**
   * @brief Check if animation preview mode is active
   */
  [[nodiscard]] bool isAnimationPreviewMode() const {
    return m_animationPreviewMode;
  }

signals:
  void objectSelected(const QString &objectId);
  void objectDoubleClicked(const QString &objectId);
  void sceneObjectsChanged();
  void objectNameChanged(const QString &objectId, const QString &name);
  void objectPositionChanged(const QString &objectId, const QPointF &position);
  void objectTransformFinished(const QString &objectId, const QPointF &oldPos,
                               const QPointF &newPos, qreal oldRotation,
                               qreal newRotation, qreal oldScaleX,
                               qreal newScaleX, qreal oldScaleY,
                               qreal newScaleY);
  void sceneChanged(const QString &sceneId);
  void focusModeRequested(bool enabled);

private slots:
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();
  void onCenterScene();
  void onFitScene();
  void onToggleGrid();
  void onGizmoModeMove();
  void onGizmoModeRotate();
  void onGizmoModeScale();
  void onCursorPositionChanged(const QPointF &scenePos);
  void onAssetsDropped(const QStringList &paths, const QPointF &scenePos);
  void onSceneObjectSelected(const QString &objectId);
  void onObjectPositionChanged(const QString &objectId,
                               const QPointF &position);
  void onObjectMoveFinished(const QString &objectId, const QPointF &oldPos,
                            const QPointF &newPos);
  void onObjectTransformFinished(const QString &objectId, const QPointF &oldPos,
                                 const QPointF &newPos, qreal oldRotation,
                                 qreal newRotation, qreal oldScaleX,
                                 qreal newScaleX, qreal oldScaleY,
                                 qreal newScaleY);
  void onDeleteRequested(const QString &objectId);
  void onContextMenuRequested(const QPoint &globalPos, const QPointF &scenePos);
  void onDragActiveChanged(bool active);

  // Play mode integration
  void onPlayModeCurrentNodeChanged(const QString &nodeId);
  void onPlayModeDialogueChanged(const QString &speaker, const QString &text);
  void onPlayModeChoicesChanged(const QStringList &choices);
  void onPlayModeChanged(int mode);
  void applyRuntimeSnapshot(const ::NovelMind::editor::SceneSnapshot &snapshot);
  void syncRuntimeSelection();
  void clearRuntimePreview();

private:
  void setupToolBar();
  void setupContent();
  void updateInfoOverlay();
  void updateBreadcrumb();
  void syncCameraToPreview();
  void frameSelected();
  void frameAll();
  void toggleGrid();
  void toggleSelectionVisibility();
  bool canEditScene() const;
  SceneObjectSnapshot snapshotFromObject(const NMSceneObject *obj) const;
  QString generateObjectId(NMSceneObjectType type) const;
  void copySelectedObject();
  bool pasteClipboardObject();
  bool duplicateSelectedObject();
  void deleteSelectedObject();
  void renameSelectedObject();
  void toggleSelectedVisibility();
  void toggleSelectedLocked();
  void captureEditorObjectsForRuntime();
  void hideEditorObjectsForRuntime();
  void restoreEditorObjectsAfterRuntime();
  void updateRuntimePreviewVisibility();
  void updatePreviewOverlayVisibility();
  void applyEditorPreview();
  QString normalizeAssetPath(const QString &assetPath) const;
  NMSceneObjectType guessObjectTypeForAsset(const QString &assetPath) const;
  QPixmap loadPixmapForAsset(const QString &hint, NMSceneObjectType type);

  // Scene validation helpers (P5.3 - runtime object allocation validation)
  bool validateSceneReady() const;
  void showSceneNotLoadedError();
  void showSceneInvalidError();
  void showRuntimeObjectCreationError(const QString &objectId,
                                      const QString &reason);

  NMSceneGraphicsScene *m_scene = nullptr;
  NMSceneGraphicsView *m_view = nullptr;
  NMSceneGLViewport *m_glViewport = nullptr;
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  QAction *m_focusModeAction = nullptr;
  QWidget *m_breadcrumbBar = nullptr;
  QLabel *m_breadcrumbLabel = nullptr;
  QFrame *m_dropHint = nullptr;
  SceneObjectSnapshot m_sceneClipboard;
  bool m_sceneClipboardValid = false;
  QString m_breadcrumbProject;
  QString m_breadcrumbGraph;
  QString m_breadcrumbNode;
  QString m_breadcrumbScene;
  NMSceneInfoOverlay *m_infoOverlay = nullptr;
  NMPlayPreviewOverlay *m_playOverlay = nullptr;
  QLabel *m_fontWarning = nullptr;
  QStringList m_runtimeObjectIds;
  std::atomic<bool> m_runtimePreviewActive{false};
  bool m_gridVisibleBeforeRuntime = true;
  bool m_renderRuntimeSceneObjects = false;
  QHash<QString, bool> m_editorVisibility;
  QHash<QString, qreal> m_editorOpacity;
  QString m_editorVisibilitySceneId;
  QString m_editorSelectionBeforeRuntime;
  QHash<QString, QPixmap> m_textureCache;
  QString m_assetsRoot;
  QString m_currentSceneId;
  // INVARIANT: Use atomic flags to prevent save race conditions
  std::atomic<bool> m_isLoadingScene{false};
  std::atomic<bool> m_playModeActive{false};
  bool m_followPlayModeNodes = true;
  std::atomic<bool> m_suppressSceneSave{false};
  QString m_sceneIdBeforePlay;
  bool m_editorPreviewActive = false;
  QString m_editorPreviewSpeaker;
  QString m_editorPreviewText;
  QStringList m_editorPreviewChoices;
  bool m_animationPreviewMode = false;
};

} // namespace NovelMind::editor::qt
