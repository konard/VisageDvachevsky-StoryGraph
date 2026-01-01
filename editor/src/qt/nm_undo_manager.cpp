#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include <utility>

namespace NovelMind::editor::qt {

// =============================================================================
// NMUndoManager Implementation
// =============================================================================

NMUndoManager::NMUndoManager() : QObject(nullptr) {}

NMUndoManager::~NMUndoManager() { shutdown(); }

NMUndoManager &NMUndoManager::instance() {
  static NMUndoManager instance;
  return instance;
}

void NMUndoManager::initialize() {
  if (m_initialized)
    return;

  m_undoStack = std::make_unique<QUndoStack>();

  // Default undo limit (0 = unlimited)
  m_undoStack->setUndoLimit(100);

  // Connect signals
  connect(m_undoStack.get(), &QUndoStack::canUndoChanged, this,
          &NMUndoManager::canUndoChanged);
  connect(m_undoStack.get(), &QUndoStack::canRedoChanged, this,
          &NMUndoManager::canRedoChanged);
  connect(m_undoStack.get(), &QUndoStack::undoTextChanged, this,
          &NMUndoManager::undoTextChanged);
  connect(m_undoStack.get(), &QUndoStack::redoTextChanged, this,
          &NMUndoManager::redoTextChanged);
  connect(m_undoStack.get(), &QUndoStack::cleanChanged, this,
          &NMUndoManager::cleanChanged);
  connect(m_undoStack.get(), &QUndoStack::indexChanged, this,
          &NMUndoManager::indexChanged);

  m_initialized = true;
  core::Logger::instance().info("Undo/Redo system initialized");
}

void NMUndoManager::shutdown() {
  if (!m_initialized)
    return;

  // Disconnect all signals before destroying the stack
  if (m_undoStack) {
    disconnect(m_undoStack.get(), nullptr, this, nullptr);
  }

  m_undoStack.reset();
  m_initialized = false;
}

void NMUndoManager::pushCommand(QUndoCommand *command) {
  if (!m_initialized || !m_undoStack) {
    core::Logger::instance().error("Undo manager not initialized");
    delete command;
    return;
  }

  m_undoStack->push(command);
}

bool NMUndoManager::canUndo() const {
  return m_undoStack && m_undoStack->canUndo();
}

bool NMUndoManager::canRedo() const {
  return m_undoStack && m_undoStack->canRedo();
}

QString NMUndoManager::undoText() const {
  return m_undoStack ? m_undoStack->undoText() : QString();
}

QString NMUndoManager::redoText() const {
  return m_undoStack ? m_undoStack->redoText() : QString();
}

void NMUndoManager::beginMacro(const QString &text) {
  if (m_undoStack) {
    m_undoStack->beginMacro(text);
  }
}

void NMUndoManager::endMacro() {
  if (m_undoStack) {
    m_undoStack->endMacro();
  }
}

void NMUndoManager::clear() {
  if (m_undoStack) {
    m_undoStack->clear();
  }
}

void NMUndoManager::setClean() {
  if (m_undoStack) {
    m_undoStack->setClean();
  }
}

bool NMUndoManager::isClean() const {
  return m_undoStack && m_undoStack->isClean();
}

void NMUndoManager::setUndoLimit(int limit) {
  if (m_undoStack) {
    m_undoStack->setUndoLimit(limit);
  }
}

void NMUndoManager::undo() {
  if (m_undoStack && m_undoStack->canUndo()) {
    m_undoStack->undo();
    core::Logger::instance().info("Undo: " +
                                  m_undoStack->undoText().toStdString());
  }
}

void NMUndoManager::redo() {
  if (m_undoStack && m_undoStack->canRedo()) {
    m_undoStack->redo();
    core::Logger::instance().info("Redo: " +
                                  m_undoStack->redoText().toStdString());
  }
}

// =============================================================================
// PropertyChangeCommand Implementation
// =============================================================================

PropertyChangeCommand::PropertyChangeCommand(const QString &objectName,
                                             const QString &propertyName,
                                             const PropertyValue &oldValue,
                                             const PropertyValue &newValue,
                                             ApplyFn apply,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_apply(std::move(apply)), m_objectName(objectName),
      m_propertyName(propertyName), m_oldValue(oldValue), m_newValue(newValue) {
  setText(QString("Change %1.%2").arg(objectName, propertyName));
}

void PropertyChangeCommand::applyValue(const PropertyValue &value,
                                       bool isUndo) {
  if (!m_apply) {
    core::Logger::instance().error(
        "PropertyChangeCommand has no apply function for " +
        m_objectName.toStdString() + "." + m_propertyName.toStdString());
    return;
  }
  m_apply(value, isUndo);
}

void PropertyChangeCommand::undo() { applyValue(m_oldValue, true); }

void PropertyChangeCommand::redo() { applyValue(m_newValue, false); }

// =============================================================================
// AddObjectCommand Implementation
// =============================================================================

AddObjectCommand::AddObjectCommand(NMSceneViewPanel *panel,
                                   SceneObjectSnapshot snapshot,
                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_snapshot(std::move(snapshot)) {
  setText(QString("Add %1").arg(m_snapshot.name));
}

void AddObjectCommand::undo() {
  auto *panel = m_panel.data();
  if (!panel) {
    return;
  }
  panel->deleteObject(m_snapshot.id);
}

void AddObjectCommand::redo() {
  auto *panel = m_panel.data();
  if (!panel) {
    return;
  }

  auto *scene = panel->graphicsScene();
  if (!scene) {
    core::Logger::instance().error(
        "Cannot add object: SceneView panel has no scene");
    return;
  }

  // If the object already exists (redo after undo), just move/rename it.
  if (!scene->findSceneObject(m_snapshot.id)) {
    panel->createObject(m_snapshot.id, m_snapshot.type, m_snapshot.position,
                        1.0);
  } else {
    panel->moveObject(m_snapshot.id, m_snapshot.position);
    if (auto *obj = scene->findSceneObject(m_snapshot.id)) {
      obj->setScaleXY(m_snapshot.scaleX, m_snapshot.scaleY);
    }
  }

  if (auto *obj = scene->findSceneObject(m_snapshot.id)) {
    obj->setName(m_snapshot.name);
    obj->setRotation(m_snapshot.rotation);
    obj->setScaleXY(m_snapshot.scaleX, m_snapshot.scaleY);
    obj->setOpacity(m_snapshot.opacity);
    obj->setVisible(m_snapshot.visible);
    obj->setZValue(m_snapshot.zValue);
    if (!m_snapshot.assetPath.isEmpty()) {
      panel->setObjectAsset(m_snapshot.id, m_snapshot.assetPath);
    }
  }
  panel->selectObjectById(m_snapshot.id);
  m_created = true;
}

// =============================================================================
// DeleteObjectCommand Implementation
// =============================================================================

DeleteObjectCommand::DeleteObjectCommand(NMSceneViewPanel *panel,
                                         SceneObjectSnapshot snapshot,
                                         QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_snapshot(std::move(snapshot)) {
  setText(QString("Delete %1").arg(m_snapshot.name));
}

void DeleteObjectCommand::undo() {
  auto *panel = m_panel.data();
  if (!panel) {
    return;
  }

  // Restore object with captured state
  auto *scene = panel->graphicsScene();
  if (!scene || scene->findSceneObject(m_snapshot.id)) {
    return;
  }

  panel->createObject(m_snapshot.id, m_snapshot.type, m_snapshot.position, 1.0);
  if (auto *obj = scene->findSceneObject(m_snapshot.id)) {
    obj->setName(m_snapshot.name);
    obj->setRotation(m_snapshot.rotation);
    obj->setScaleXY(m_snapshot.scaleX, m_snapshot.scaleY);
    obj->setOpacity(m_snapshot.opacity);
    obj->setVisible(m_snapshot.visible);
    obj->setZValue(m_snapshot.zValue);
    if (!m_snapshot.assetPath.isEmpty()) {
      panel->setObjectAsset(m_snapshot.id, m_snapshot.assetPath);
    }
  }
  panel->selectObjectById(m_snapshot.id);
}

void DeleteObjectCommand::redo() {
  auto *panel = m_panel.data();
  if (!panel) {
    return;
  }
  panel->deleteObject(m_snapshot.id);
}

// =============================================================================
// TransformObjectCommand Implementation
// =============================================================================

TransformObjectCommand::TransformObjectCommand(
    NMSceneViewPanel *panel, const QString &objectId,
    const QPointF &oldPosition, const QPointF &newPosition, qreal oldRotation,
    qreal newRotation, qreal oldScaleX, qreal newScaleX, qreal oldScaleY,
    qreal newScaleY, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_objectId(objectId),
      m_oldPosition(oldPosition), m_newPosition(newPosition),
      m_oldRotation(oldRotation), m_newRotation(newRotation),
      m_oldScaleX(oldScaleX), m_newScaleX(newScaleX), m_oldScaleY(oldScaleY),
      m_newScaleY(newScaleY) {
  setText(QString("Transform %1").arg(objectId));
}

void TransformObjectCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->applyObjectTransform(m_objectId, m_oldPosition, m_oldRotation,
                                m_oldScaleX, m_oldScaleY);
}

void TransformObjectCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->applyObjectTransform(m_objectId, m_newPosition, m_newRotation,
                                m_newScaleX, m_newScaleY);
}

bool TransformObjectCommand::mergeWith(const QUndoCommand *other) {
  // Merge consecutive transform commands on the same object
  if (other->id() != id())
    return false;

  const auto *transformCommand =
      static_cast<const TransformObjectCommand *>(other);

  if (transformCommand->m_objectId != m_objectId ||
      transformCommand->m_panel != m_panel) {
    return false;
  }

  m_newPosition = transformCommand->m_newPosition;
  m_newRotation = transformCommand->m_newRotation;
  m_newScaleX = transformCommand->m_newScaleX;
  m_newScaleY = transformCommand->m_newScaleY;
  return true;
}

// =============================================================================
// ToggleObjectVisibilityCommand Implementation
// =============================================================================

ToggleObjectVisibilityCommand::ToggleObjectVisibilityCommand(
    NMSceneViewPanel *panel, const QString &objectId, bool oldVisible,
    bool newVisible, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_objectId(objectId),
      m_oldVisible(oldVisible), m_newVisible(newVisible) {
  setText(QString("Toggle Visibility: %1").arg(objectId));
}

void ToggleObjectVisibilityCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->setObjectVisible(m_objectId, m_oldVisible);
}

void ToggleObjectVisibilityCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->setObjectVisible(m_objectId, m_newVisible);
}

// =============================================================================
// ToggleObjectLockedCommand Implementation
// =============================================================================

ToggleObjectLockedCommand::ToggleObjectLockedCommand(NMSceneViewPanel *panel,
                                                     const QString &objectId,
                                                     bool oldLocked,
                                                     bool newLocked,
                                                     QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_objectId(objectId),
      m_oldLocked(oldLocked), m_newLocked(newLocked) {
  setText(QString("Toggle Locked: %1").arg(objectId));
}

void ToggleObjectLockedCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->setObjectLocked(m_objectId, m_oldLocked);
}

void ToggleObjectLockedCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->setObjectLocked(m_objectId, m_newLocked);
}

// =============================================================================
// ReparentObjectCommand Implementation
// =============================================================================

ReparentObjectCommand::ReparentObjectCommand(NMSceneViewPanel *panel,
                                             const QString &objectId,
                                             const QString &oldParentId,
                                             const QString &newParentId,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_objectId(objectId),
      m_oldParentId(oldParentId), m_newParentId(newParentId) {
  setText(QString("Reparent: %1").arg(objectId));
}

void ReparentObjectCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->reparentObject(m_objectId, m_oldParentId);
}

void ReparentObjectCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->reparentObject(m_objectId, m_newParentId);
}

bool ReparentObjectCommand::mergeWith(const QUndoCommand *other) {
  if (other->id() != id()) {
    return false;
  }

  const auto *otherReparent = static_cast<const ReparentObjectCommand *>(other);
  if (otherReparent->m_objectId != m_objectId) {
    return false;
  }

  // Merge consecutive reparenting of the same object
  m_newParentId = otherReparent->m_newParentId;
  return true;
}

// =============================================================================
// CreateGraphNodeCommand Implementation
// =============================================================================

CreateGraphNodeCommand::CreateGraphNodeCommand(NMStoryGraphScene *scene,
                                               const QString &nodeType,
                                               const QPointF &position,
                                               const QString &title,
                                               QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene) {
  m_snapshot.type = nodeType;
  m_snapshot.title = title.isEmpty() ? QString("New %1").arg(nodeType) : title;
  m_snapshot.position = position;
  setText(QString("Create %1 Node").arg(nodeType));
}

void CreateGraphNodeCommand::undo() {
  if (!m_scene) {
    return;
  }
  if (auto *node = m_scene->findNode(m_snapshot.id)) {
    m_scene->removeNode(node);
  }
}

void CreateGraphNodeCommand::redo() {
  if (!m_scene) {
    return;
  }

  if (auto *existing = m_scene->findNode(m_snapshot.id)) {
    existing->setPos(m_snapshot.position);
    existing->setTitle(m_snapshot.title);
    existing->setNodeType(m_snapshot.type);
    existing->setScriptPath(m_snapshot.scriptPath);
    existing->setDialogueSpeaker(m_snapshot.speaker);
    existing->setDialogueText(m_snapshot.dialogueText);
    existing->setChoiceOptions(m_snapshot.choices);
    return;
  }

  auto *node =
      m_scene->addNode(m_snapshot.title, m_snapshot.type, m_snapshot.position,
                       m_snapshot.id, m_snapshot.idString);
  if (node) {
    if (m_snapshot.id == 0) {
      m_snapshot.id = node->nodeId();
    }
    if (m_snapshot.idString.isEmpty()) {
      m_snapshot.idString = node->nodeIdString();
    } else {
      node->setNodeIdString(m_snapshot.idString);
    }
    if (m_snapshot.scriptPath.isEmpty()) {
      m_snapshot.scriptPath = node->scriptPath();
    } else {
      node->setScriptPath(m_snapshot.scriptPath);
    }
    node->setDialogueSpeaker(m_snapshot.speaker);
    node->setDialogueText(m_snapshot.dialogueText);
    node->setChoiceOptions(m_snapshot.choices);
  }
}

// =============================================================================
// DeleteGraphNodeCommand Implementation
// =============================================================================

DeleteGraphNodeCommand::DeleteGraphNodeCommand(NMStoryGraphScene *scene,
                                               uint64_t nodeId,
                                               QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene) {
  if (m_scene) {
    if (auto *node = m_scene->findNode(nodeId)) {
      m_snapshot.id = nodeId;
      m_snapshot.idString = node->nodeIdString();
      m_snapshot.title = node->title();
      m_snapshot.type = node->nodeType();
      m_snapshot.position = node->pos();
      m_snapshot.scriptPath = node->scriptPath();
      m_snapshot.speaker = node->dialogueSpeaker();
      m_snapshot.dialogueText = node->dialogueText();
      m_snapshot.choices = node->choiceOptions();

      const auto connections = m_scene->findConnectionsForNode(node);
      for (auto *conn : connections) {
        if (conn && conn->startNode() && conn->endNode()) {
          m_connections.push_back(
              {conn->startNode()->nodeId(), conn->endNode()->nodeId()});
        }
      }
      setText(QString("Delete Node %1").arg(m_snapshot.idString));
    }
  }
}

void DeleteGraphNodeCommand::undo() {
  if (!m_scene || m_snapshot.id == 0) {
    return;
  }

  auto *node =
      m_scene->addNode(m_snapshot.title, m_snapshot.type, m_snapshot.position,
                       m_snapshot.id, m_snapshot.idString);
  if (!node) {
    return;
  }
  node->setScriptPath(m_snapshot.scriptPath);
  node->setDialogueSpeaker(m_snapshot.speaker);
  node->setDialogueText(m_snapshot.dialogueText);
  node->setChoiceOptions(m_snapshot.choices);

  for (const auto &conn : m_connections) {
    m_scene->addConnection(conn.fromId, conn.toId);
  }
  m_removed = false;
}

void DeleteGraphNodeCommand::redo() {
  if (!m_scene || m_snapshot.id == 0) {
    return;
  }

  if (auto *node = m_scene->findNode(m_snapshot.id)) {
    m_scene->removeNode(node);
    m_removed = true;
  }
}

// =============================================================================
// ConnectGraphNodesCommand Implementation
// =============================================================================

ConnectGraphNodesCommand::ConnectGraphNodesCommand(NMStoryGraphScene *scene,
                                                   uint64_t sourceNodeId,
                                                   uint64_t targetNodeId,
                                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene) {
  m_connection.fromId = sourceNodeId;
  m_connection.toId = targetNodeId;
  setText(QString("Connect %1 -> %2").arg(sourceNodeId).arg(targetNodeId));
}

void ConnectGraphNodesCommand::undo() {
  if (!m_scene) {
    return;
  }
  m_scene->removeConnection(m_connection.fromId, m_connection.toId);
  m_connected = false;
}

void ConnectGraphNodesCommand::redo() {
  if (!m_scene) {
    return;
  }
  if (!m_scene->hasConnection(m_connection.fromId, m_connection.toId)) {
    m_scene->addConnection(m_connection.fromId, m_connection.toId);
  }
  m_connected = true;
}

// =============================================================================
// DisconnectGraphNodesCommand Implementation
// =============================================================================

DisconnectGraphNodesCommand::DisconnectGraphNodesCommand(
    NMStoryGraphScene *scene, uint64_t sourceNodeId, uint64_t targetNodeId,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene) {
  m_connection.fromId = sourceNodeId;
  m_connection.toId = targetNodeId;
  setText(QString("Disconnect %1 -> %2").arg(sourceNodeId).arg(targetNodeId));
}

void DisconnectGraphNodesCommand::undo() {
  if (!m_scene) {
    return;
  }
  if (!m_scene->hasConnection(m_connection.fromId, m_connection.toId)) {
    m_scene->addConnection(m_connection.fromId, m_connection.toId);
  }
  m_disconnected = false;
}

void DisconnectGraphNodesCommand::redo() {
  if (!m_scene) {
    return;
  }
  m_scene->removeConnection(m_connection.fromId, m_connection.toId);
  m_disconnected = true;
}

// =============================================================================
// MoveGraphNodesCommand Implementation
// =============================================================================

MoveGraphNodesCommand::MoveGraphNodesCommand(
    NMStoryGraphScene *scene, const QVector<GraphNodeMove> &moves,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_moves(moves) {
  setText("Move Graph Nodes");
}

void MoveGraphNodesCommand::undo() {
  if (!m_scene) {
    return;
  }
  for (const auto &move : m_moves) {
    if (auto *node = m_scene->findNode(move.nodeId)) {
      node->setPos(move.oldPos);
    }
  }
}

void MoveGraphNodesCommand::redo() {
  if (!m_scene) {
    return;
  }
  for (const auto &move : m_moves) {
    if (auto *node = m_scene->findNode(move.nodeId)) {
      node->setPos(move.newPos);
    }
  }
}

bool MoveGraphNodesCommand::mergeWith(const QUndoCommand *other) {
  if (other->id() != id()) {
    return false;
  }
  const auto *moveCmd = static_cast<const MoveGraphNodesCommand *>(other);
  if (moveCmd->m_scene != m_scene) {
    return false;
  }

  // Merge by replacing newPos for matching IDs and appending new ones
  QHash<uint64_t, GraphNodeMove> merged;
  for (const auto &m : m_moves) {
    merged.insert(m.nodeId, m);
  }
  for (const auto &m : moveCmd->m_moves) {
    auto existing =
        merged.value(m.nodeId, GraphNodeMove{m.nodeId, QPointF(), QPointF()});
    existing.newPos = m.newPos;
    if (existing.oldPos == QPointF()) {
      existing.oldPos = m.oldPos;
    }
    merged.insert(m.nodeId, existing);
  }
  m_moves = merged.values().toVector();
  return true;
}

// =============================================================================
// Timeline Commands Implementation
// =============================================================================

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"

TimelineKeyframeMoveCommand::TimelineKeyframeMoveCommand(
    NMTimelinePanel *panel, const QString &trackName, int oldFrame,
    int newFrame, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_trackName(trackName),
      m_oldFrame(oldFrame), m_newFrame(newFrame) {
  setText(QString("Move Keyframe in %1").arg(trackName));
}

void TimelineKeyframeMoveCommand::undo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->moveKeyframe(m_newFrame, m_oldFrame);
}

void TimelineKeyframeMoveCommand::redo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->moveKeyframe(m_oldFrame, m_newFrame);
}

bool TimelineKeyframeMoveCommand::mergeWith(const QUndoCommand *other) {
  if (other->id() != id()) {
    return false;
  }
  const auto *moveCmd = static_cast<const TimelineKeyframeMoveCommand *>(other);
  if (moveCmd->m_panel != m_panel || moveCmd->m_trackName != m_trackName) {
    return false;
  }
  // Merge consecutive moves of the same keyframe
  if (moveCmd->m_oldFrame == m_newFrame) {
    m_newFrame = moveCmd->m_newFrame;
    return true;
  }
  return false;
}

AddKeyframeCommand::AddKeyframeCommand(NMTimelinePanel *panel,
                                       const QString &trackName,
                                       const KeyframeSnapshot &snapshot,
                                       QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_trackName(trackName),
      m_snapshot(snapshot) {
  setText(QString("Add Keyframe to %1").arg(trackName));
}

void AddKeyframeCommand::undo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->removeKeyframe(m_snapshot.frame);
}

void AddKeyframeCommand::redo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->addKeyframe(m_snapshot.frame, m_snapshot.value,
                     static_cast<EasingType>(m_snapshot.easingType));
  if (auto *kf = track->getKeyframe(m_snapshot.frame)) {
    kf->handleInX = m_snapshot.handleInX;
    kf->handleInY = m_snapshot.handleInY;
    kf->handleOutX = m_snapshot.handleOutX;
    kf->handleOutY = m_snapshot.handleOutY;
  }
}

DeleteKeyframeCommand::DeleteKeyframeCommand(NMTimelinePanel *panel,
                                             const QString &trackName,
                                             const KeyframeSnapshot &snapshot,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_trackName(trackName),
      m_snapshot(snapshot) {
  setText(QString("Delete Keyframe from %1").arg(trackName));
}

void DeleteKeyframeCommand::undo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->addKeyframe(m_snapshot.frame, m_snapshot.value,
                     static_cast<EasingType>(m_snapshot.easingType));
  if (auto *kf = track->getKeyframe(m_snapshot.frame)) {
    kf->handleInX = m_snapshot.handleInX;
    kf->handleInY = m_snapshot.handleInY;
    kf->handleOutX = m_snapshot.handleOutX;
    kf->handleOutY = m_snapshot.handleOutY;
  }
}

void DeleteKeyframeCommand::redo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  track->removeKeyframe(m_snapshot.frame);
}

ChangeKeyframeEasingCommand::ChangeKeyframeEasingCommand(
    NMTimelinePanel *panel, const QString &trackName, int frame, int oldEasing,
    int newEasing, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_trackName(trackName),
      m_frame(frame), m_oldEasing(oldEasing), m_newEasing(newEasing) {
  setText(QString("Change Keyframe Easing in %1").arg(trackName));
}

void ChangeKeyframeEasingCommand::undo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  if (auto *kf = track->getKeyframe(m_frame)) {
    kf->easing = static_cast<EasingType>(m_oldEasing);
  }
}

void ChangeKeyframeEasingCommand::redo() {
  if (!m_panel) {
    return;
  }
  auto *track = m_panel->getTrack(m_trackName);
  if (!track) {
    return;
  }
  if (auto *kf = track->getKeyframe(m_frame)) {
    kf->easing = static_cast<EasingType>(m_newEasing);
  }
}

// =============================================================================
// Localization Commands Implementation
// =============================================================================

#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"

AddLocalizationKeyCommand::AddLocalizationKeyCommand(
    NMLocalizationPanel *panel, const QString &key, const QString &defaultValue,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_key(key),
      m_defaultValue(defaultValue) {
  setText(QString("Add Localization Key '%1'").arg(key));
}

void AddLocalizationKeyCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->deleteKey(m_key);
}

void AddLocalizationKeyCommand::redo() {
  if (!m_panel) {
    return;
  }
  // On first redo, the UI already added the key, so skip it
  if (!m_firstRedo) {
    m_panel->addKey(m_key, m_defaultValue);
  }
  m_firstRedo = false;
}

DeleteLocalizationKeyCommand::DeleteLocalizationKeyCommand(
    NMLocalizationPanel *panel, const QString &key,
    const QHash<QString, QString> &translations, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_key(key),
      m_translations(translations) {
  setText(QString("Delete Localization Key '%1'").arg(key));
}

void DeleteLocalizationKeyCommand::undo() {
  if (!m_panel) {
    return;
  }

  // First, add the key back with the default locale value
  QString defaultLocale = m_panel->property("defaultLocale").toString();
  if (defaultLocale.isEmpty()) {
    defaultLocale = "en";
  }
  m_panel->addKey(m_key, m_translations.value(defaultLocale, ""));

  // Now restore all translations for all locales
  for (auto it = m_translations.constBegin(); it != m_translations.constEnd();
       ++it) {
    const QString &locale = it.key();
    const QString &value = it.value();
    m_panel->setTranslationValue(m_key, locale, value);
  }
}

void DeleteLocalizationKeyCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->deleteKey(m_key);
}

ChangeTranslationCommand::ChangeTranslationCommand(
    NMLocalizationPanel *panel, const QString &key, const QString &locale,
    const QString &oldValue, const QString &newValue, QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_key(key), m_locale(locale),
      m_oldValue(oldValue), m_newValue(newValue) {
  setText(QString("Change Translation '%1' [%2]").arg(key, locale));
}

void ChangeTranslationCommand::undo() {
  if (!m_panel) {
    return;
  }
  // Set the translation value in the panel's internal data structure
  // and emit the signal to notify other components
  m_panel->setTranslationValue(m_key, m_locale, m_oldValue);
  emit m_panel->translationChanged(m_key, m_locale, m_oldValue);
}

void ChangeTranslationCommand::redo() {
  if (!m_panel) {
    return;
  }
  // Set the translation value in the panel's internal data structure
  // and emit the signal to notify other components
  m_panel->setTranslationValue(m_key, m_locale, m_newValue);
  emit m_panel->translationChanged(m_key, m_locale, m_newValue);
}

bool ChangeTranslationCommand::mergeWith(const QUndoCommand *other) {
  // Only merge with same command type (ensured by matching id())
  const auto *otherCmd = static_cast<const ChangeTranslationCommand *>(other);

  // Only merge if same key and locale
  if (otherCmd->m_key != m_key || otherCmd->m_locale != m_locale) {
    return false;
  }

  // Merge: keep old value from this command, new value from other
  // This combines consecutive edits (e.g., typing "Hello" character by
  // character) into a single undo operation
  m_newValue = otherCmd->m_newValue;
  return true;
}

// =============================================================================
// Curve Editor Commands Implementation
// =============================================================================

#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"

AddCurvePointCommand::AddCurvePointCommand(NMCurveEditorPanel *panel,
                                           CurvePointId pointId, qreal time,
                                           qreal value, int interpolation,
                                           QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel) {
  m_snapshot.id = pointId;
  m_snapshot.time = time;
  m_snapshot.value = value;
  m_snapshot.interpolation = interpolation;
  setText("Add Curve Point");
}

void AddCurvePointCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->curveData().removePoint(m_snapshot.id);
  emit m_panel->curveChanged(m_panel->curveId());
}

void AddCurvePointCommand::redo() {
  if (!m_panel) {
    return;
  }
  // On first redo, the point may already be added by UI
  if (!m_firstRedo) {
    m_panel->curveData().addPoint(
        m_snapshot.time, m_snapshot.value,
        static_cast<CurveInterpolation>(m_snapshot.interpolation));
  }
  m_firstRedo = false;
  emit m_panel->curveChanged(m_panel->curveId());
}

DeleteCurvePointCommand::DeleteCurvePointCommand(
    NMCurveEditorPanel *panel, const CurvePointSnapshot &snapshot,
    QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_snapshot(snapshot) {
  setText("Delete Curve Point");
}

void DeleteCurvePointCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->curveData().addPoint(
      m_snapshot.time, m_snapshot.value,
      static_cast<CurveInterpolation>(m_snapshot.interpolation));
  emit m_panel->curveChanged(m_panel->curveId());
}

void DeleteCurvePointCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->curveData().removePoint(m_snapshot.id);
  emit m_panel->curveChanged(m_panel->curveId());
}

MoveCurvePointCommand::MoveCurvePointCommand(NMCurveEditorPanel *panel,
                                             CurvePointId pointId,
                                             qreal oldTime, qreal oldValue,
                                             qreal newTime, qreal newValue,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel), m_pointId(pointId),
      m_oldTime(oldTime), m_oldValue(oldValue), m_newTime(newTime),
      m_newValue(newValue) {
  setText("Move Curve Point");
}

void MoveCurvePointCommand::undo() {
  if (!m_panel) {
    return;
  }
  m_panel->curveData().updatePoint(m_pointId, m_oldTime, m_oldValue);
  emit m_panel->curveChanged(m_panel->curveId());
}

void MoveCurvePointCommand::redo() {
  if (!m_panel) {
    return;
  }
  m_panel->curveData().updatePoint(m_pointId, m_newTime, m_newValue);
  emit m_panel->curveChanged(m_panel->curveId());
}

bool MoveCurvePointCommand::mergeWith(const QUndoCommand *other) {
  if (other->id() != id()) {
    return false;
  }
  const auto *moveCmd = static_cast<const MoveCurvePointCommand *>(other);
  if (moveCmd->m_panel != m_panel || moveCmd->m_pointId != m_pointId) {
    return false;
  }
  // Merge consecutive moves of the same point
  m_newTime = moveCmd->m_newTime;
  m_newValue = moveCmd->m_newValue;
  return true;
}

CurveEditCommand::CurveEditCommand(NMCurveEditorPanel *panel,
                                   const QString &description,
                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_panel(panel) {
  setText(description);
}

void CurveEditCommand::undo() {
  if (!m_panel) {
    return;
  }
  // Apply old values in reverse order
  for (auto it = m_changes.rbegin(); it != m_changes.rend(); ++it) {
    m_panel->curveData().updatePoint(it->id, it->oldTime, it->oldValue);
  }
  emit m_panel->curveChanged(m_panel->curveId());
}

void CurveEditCommand::redo() {
  if (!m_panel) {
    return;
  }
  // Apply new values
  for (const auto &change : m_changes) {
    m_panel->curveData().updatePoint(change.id, change.newTime,
                                     change.newValue);
  }
  emit m_panel->curveChanged(m_panel->curveId());
}

void CurveEditCommand::addPointChange(CurvePointId pointId, qreal oldTime,
                                      qreal oldValue, qreal newTime,
                                      qreal newValue) {
  m_changes.append({pointId, oldTime, oldValue, newTime, newValue});
}

} // namespace NovelMind::editor::qt
