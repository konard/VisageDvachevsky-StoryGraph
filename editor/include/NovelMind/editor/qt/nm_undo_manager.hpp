#pragma once

/**
 * @file nm_undo_manager.hpp
 * @brief Centralized undo/redo management using Qt's QUndoStack
 *
 * This manager provides:
 * - Global undo/redo stack for all editor operations
 * - Command pattern implementation for reversible actions
 * - Integration with Qt's undo framework
 * - Undo history visualization
 */

#include "NovelMind/core/property_system.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QStringList>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVariant>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace NovelMind::editor::qt {

// Forward declarations to avoid heavy coupling in the header
class NMSceneViewPanel;
class NMStoryGraphScene;
enum class NMSceneObjectType : int;

struct SceneObjectSnapshot {
  QString id;
  QString name;
  NMSceneObjectType type;
  QPointF position;
  qreal rotation = 0.0;
  qreal scaleX = 1.0;
  qreal scaleY = 1.0;
  qreal opacity = 1.0;
  bool visible = true;
  qreal zValue = 0.0;
  QString assetPath;
};

struct GraphNodeSnapshot {
  uint64_t id = 0;
  QString idString;
  QString title;
  QString type;
  QPointF position;
  QString scriptPath;
  QString speaker;
  QString dialogueText;
  QStringList choices;
};

struct GraphConnectionSnapshot {
  uint64_t fromId = 0;
  uint64_t toId = 0;
};

/**
 * @brief Centralized undo/redo manager (singleton)
 *
 * Manages all undoable operations in the editor using Qt's QUndoStack.
 * All modifications to the scene, story graph, properties, etc. should
 * go through this system to ensure undo/redo support.
 */
class NMUndoManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static NMUndoManager &instance();

  /**
   * @brief Initialize the undo manager
   */
  void initialize();

  /**
   * @brief Shutdown the undo manager
   */
  void shutdown();

  /**
   * @brief Get the underlying QUndoStack
   */
  [[nodiscard]] QUndoStack *undoStack() const { return m_undoStack.get(); }

  /**
   * @brief Push a command onto the undo stack
   * @param command The command to push (takes ownership)
   */
  void pushCommand(QUndoCommand *command);

  /**
   * @brief Check if undo is available
   */
  [[nodiscard]] bool canUndo() const;

  /**
   * @brief Check if redo is available
   */
  [[nodiscard]] bool canRedo() const;

  /**
   * @brief Get the text for the next undo action
   */
  [[nodiscard]] QString undoText() const;

  /**
   * @brief Get the text for the next redo action
   */
  [[nodiscard]] QString redoText() const;

  /**
   * @brief Begin a macro (group of commands)
   * @param text Description of the macro
   */
  void beginMacro(const QString &text);

  /**
   * @brief End the current macro
   */
  void endMacro();

  /**
   * @brief Clear the undo stack
   */
  void clear();

  /**
   * @brief Set the clean state (typically after save)
   */
  void setClean();

  /**
   * @brief Check if the document is clean (no unsaved changes)
   */
  [[nodiscard]] bool isClean() const;

  /**
   * @brief Set the undo limit (0 = unlimited)
   */
  void setUndoLimit(int limit);

public slots:
  /**
   * @brief Perform undo
   */
  void undo();

  /**
   * @brief Perform redo
   */
  void redo();

signals:
  /**
   * @brief Emitted when undo/redo availability changes
   */
  void canUndoChanged(bool canUndo);
  void canRedoChanged(bool canRedo);

  /**
   * @brief Emitted when undo/redo text changes
   */
  void undoTextChanged(const QString &text);
  void redoTextChanged(const QString &text);

  /**
   * @brief Emitted when clean state changes
   */
  void cleanChanged(bool clean);

  /**
   * @brief Emitted when an index changes in the stack
   */
  void indexChanged(int index);

private:
  NMUndoManager();
  ~NMUndoManager() override;

  NMUndoManager(const NMUndoManager &) = delete;
  NMUndoManager &operator=(const NMUndoManager &) = delete;

  std::unique_ptr<QUndoStack> m_undoStack;
  bool m_initialized = false;
};

// =============================================================================
// Common Command Types
// =============================================================================

/**
 * @brief Command for changing a property value
 */
class PropertyChangeCommand : public QUndoCommand {
public:
  using ApplyFn = std::function<void(const PropertyValue &, bool isUndo)>;

  PropertyChangeCommand(const QString &objectName, const QString &propertyName,
                        const PropertyValue &oldValue,
                        const PropertyValue &newValue, ApplyFn apply,
                        QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  void applyValue(const PropertyValue &value, bool isUndo);

  ApplyFn m_apply;
  QString m_objectName;
  QString m_propertyName;
  PropertyValue m_oldValue;
  PropertyValue m_newValue;
};

/**
 * @brief Command for adding an object to the scene
 */
class AddObjectCommand : public QUndoCommand {
public:
  AddObjectCommand(NMSceneViewPanel *panel, SceneObjectSnapshot snapshot,
                   QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMSceneViewPanel> m_panel;
  SceneObjectSnapshot m_snapshot;
  bool m_created = false;
};

/**
 * @brief Command for deleting an object from the scene
 */
class DeleteObjectCommand : public QUndoCommand {
public:
  DeleteObjectCommand(NMSceneViewPanel *panel, SceneObjectSnapshot snapshot,
                      QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMSceneViewPanel> m_panel;
  SceneObjectSnapshot m_snapshot;
};

/**
 * @brief Command for moving/transforming an object
 */
class TransformObjectCommand : public QUndoCommand {
public:
  TransformObjectCommand(NMSceneViewPanel *panel, const QString &objectId,
                         const QPointF &oldPosition, const QPointF &newPosition,
                         qreal oldRotation = 0.0, qreal newRotation = 0.0,
                         qreal oldScaleX = 1.0, qreal newScaleX = 1.0,
                         qreal oldScaleY = 1.0, qreal newScaleY = 1.0,
                         QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 1; } // For command merging

private:
  QPointer<NMSceneViewPanel> m_panel;
  QString m_objectId;
  QPointF m_oldPosition;
  QPointF m_newPosition;
  qreal m_oldRotation = 0.0;
  qreal m_newRotation = 0.0;
  qreal m_oldScaleX = 1.0;
  qreal m_newScaleX = 1.0;
  qreal m_oldScaleY = 1.0;
  qreal m_newScaleY = 1.0;
};

/**
 * @brief Command for toggling object visibility
 */
class ToggleObjectVisibilityCommand : public QUndoCommand {
public:
  ToggleObjectVisibilityCommand(NMSceneViewPanel *panel,
                                const QString &objectId, bool oldVisible,
                                bool newVisible,
                                QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMSceneViewPanel> m_panel;
  QString m_objectId;
  bool m_oldVisible = true;
  bool m_newVisible = true;
};

/**
 * @brief Command for toggling object locked state
 */
class ToggleObjectLockedCommand : public QUndoCommand {
public:
  ToggleObjectLockedCommand(NMSceneViewPanel *panel, const QString &objectId,
                            bool oldLocked, bool newLocked,
                            QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMSceneViewPanel> m_panel;
  QString m_objectId;
  bool m_oldLocked = false;
  bool m_newLocked = false;
};

/**
 * @brief Command for reparenting an object
 */
class ReparentObjectCommand : public QUndoCommand {
public:
  ReparentObjectCommand(NMSceneViewPanel *panel, const QString &objectId,
                        const QString &oldParentId, const QString &newParentId,
                        QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 5; } // Unique ID for command merging

private:
  QPointer<NMSceneViewPanel> m_panel;
  QString m_objectId;
  QString m_oldParentId;
  QString m_newParentId;
};

/**
 * @brief Command for creating a graph node
 */
class CreateGraphNodeCommand : public QUndoCommand {
public:
  CreateGraphNodeCommand(NMStoryGraphScene *scene, const QString &nodeType,
                         const QPointF &position,
                         const QString &title = QString(),
                         QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMStoryGraphScene> m_scene;
  GraphNodeSnapshot m_snapshot;
  bool m_created = false;
};

/**
 * @brief Command for deleting a graph node
 */
class DeleteGraphNodeCommand : public QUndoCommand {
public:
  DeleteGraphNodeCommand(NMStoryGraphScene *scene, uint64_t nodeId,
                         QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMStoryGraphScene> m_scene;
  GraphNodeSnapshot m_snapshot;
  std::vector<GraphConnectionSnapshot> m_connections;
  bool m_removed = false;
};

/**
 * @brief Command for connecting two graph nodes
 */
class ConnectGraphNodesCommand : public QUndoCommand {
public:
  ConnectGraphNodesCommand(NMStoryGraphScene *scene, uint64_t sourceNodeId,
                           uint64_t targetNodeId,
                           QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMStoryGraphScene> m_scene;
  GraphConnectionSnapshot m_connection;
  bool m_connected = false;
};

/**
 * @brief Command for disconnecting two graph nodes
 */
class DisconnectGraphNodesCommand : public QUndoCommand {
public:
  DisconnectGraphNodesCommand(NMStoryGraphScene *scene, uint64_t sourceNodeId,
                              uint64_t targetNodeId,
                              QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMStoryGraphScene> m_scene;
  GraphConnectionSnapshot m_connection;
  bool m_disconnected = false;
};

/**
 * @brief Command for moving one or more graph nodes
 */
class MoveGraphNodesCommand : public QUndoCommand {
public:
  MoveGraphNodesCommand(NMStoryGraphScene *scene,
                        const QVector<GraphNodeMove> &moves,
                        QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 2; }

private:
  QPointer<NMStoryGraphScene> m_scene;
  QVector<GraphNodeMove> m_moves;
};

// =============================================================================
// Timeline Commands
// =============================================================================

// Forward declarations
struct Keyframe;

/**
 * @brief Snapshot of a keyframe's state
 */
struct KeyframeSnapshot {
  int frame = 0;
  QVariant value;
  int easingType = 0; // EasingType as int
  float handleInX = 0.0f;
  float handleInY = 0.0f;
  float handleOutX = 0.0f;
  float handleOutY = 0.0f;
};

/**
 * @brief Command for moving a keyframe
 */
class TimelineKeyframeMoveCommand : public QUndoCommand {
public:
  TimelineKeyframeMoveCommand(NMTimelinePanel *panel, const QString &trackName,
                              int oldFrame, int newFrame,
                              QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 3; }

private:
  QPointer<NMTimelinePanel> m_panel;
  QString m_trackName;
  int m_oldFrame = 0;
  int m_newFrame = 0;
};

/**
 * @brief Command for adding a keyframe
 */
class AddKeyframeCommand : public QUndoCommand {
public:
  AddKeyframeCommand(NMTimelinePanel *panel, const QString &trackName,
                     const KeyframeSnapshot &snapshot,
                     QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMTimelinePanel> m_panel;
  QString m_trackName;
  KeyframeSnapshot m_snapshot;
};

/**
 * @brief Command for deleting a keyframe
 */
class DeleteKeyframeCommand : public QUndoCommand {
public:
  DeleteKeyframeCommand(NMTimelinePanel *panel, const QString &trackName,
                        const KeyframeSnapshot &snapshot,
                        QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMTimelinePanel> m_panel;
  QString m_trackName;
  KeyframeSnapshot m_snapshot;
};

/**
 * @brief Command for changing keyframe easing
 */
class ChangeKeyframeEasingCommand : public QUndoCommand {
public:
  ChangeKeyframeEasingCommand(NMTimelinePanel *panel, const QString &trackName,
                              int frame, int oldEasing, int newEasing,
                              QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMTimelinePanel> m_panel;
  QString m_trackName;
  int m_frame = 0;
  int m_oldEasing = 0;
  int m_newEasing = 0;
};

// =============================================================================
// Localization Commands
// =============================================================================

/**
 * @brief Command for adding a localization key
 */
class AddLocalizationKeyCommand : public QUndoCommand {
public:
  AddLocalizationKeyCommand(NMLocalizationPanel *panel, const QString &key,
                            const QString &defaultValue,
                            QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMLocalizationPanel> m_panel;
  QString m_key;
  QString m_defaultValue;
  bool m_firstRedo = true;
};

/**
 * @brief Command for deleting a localization key
 */
class DeleteLocalizationKeyCommand : public QUndoCommand {
public:
  DeleteLocalizationKeyCommand(NMLocalizationPanel *panel, const QString &key,
                               const QHash<QString, QString> &translations,
                               QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMLocalizationPanel> m_panel;
  QString m_key;
  QHash<QString, QString> m_translations; // locale -> translation
};

/**
 * @brief Command for changing a translation value
 *
 * Supports command merging to combine consecutive edits to the same
 * key/locale into a single undo operation (e.g., typing "Hello" character
 * by character creates one command instead of five).
 */
class ChangeTranslationCommand : public QUndoCommand {
public:
  ChangeTranslationCommand(NMLocalizationPanel *panel, const QString &key,
                           const QString &locale, const QString &oldValue,
                           const QString &newValue,
                           QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 6; } // Unique ID for command merging

private:
  QPointer<NMLocalizationPanel> m_panel;
  QString m_key;
  QString m_locale;
  QString m_oldValue;
  QString m_newValue;
};

// =============================================================================
// Curve Editor Commands
// =============================================================================

// Forward declarations
class CurveData;
using CurvePointId = uint64_t;

/**
 * @brief Snapshot of a curve point's state
 */
struct CurvePointSnapshot {
  CurvePointId id = 0;
  qreal time = 0.0;
  qreal value = 0.0;
  int interpolation = 0; // CurveInterpolation as int
};

/**
 * @brief Command for adding a curve point
 */
class AddCurvePointCommand : public QUndoCommand {
public:
  AddCurvePointCommand(NMCurveEditorPanel *panel, CurvePointId pointId,
                       qreal time, qreal value, int interpolation = 0,
                       QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMCurveEditorPanel> m_panel;
  CurvePointSnapshot m_snapshot;
  bool m_firstRedo = true;
};

/**
 * @brief Command for deleting a curve point
 */
class DeleteCurvePointCommand : public QUndoCommand {
public:
  DeleteCurvePointCommand(NMCurveEditorPanel *panel,
                          const CurvePointSnapshot &snapshot,
                          QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  QPointer<NMCurveEditorPanel> m_panel;
  CurvePointSnapshot m_snapshot;
};

/**
 * @brief Command for moving a curve point
 */
class MoveCurvePointCommand : public QUndoCommand {
public:
  MoveCurvePointCommand(NMCurveEditorPanel *panel, CurvePointId pointId,
                        qreal oldTime, qreal oldValue, qreal newTime,
                        qreal newValue, QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;
  bool mergeWith(const QUndoCommand *other) override;
  int id() const override { return 4; }

private:
  QPointer<NMCurveEditorPanel> m_panel;
  CurvePointId m_pointId = 0;
  qreal m_oldTime = 0.0;
  qreal m_oldValue = 0.0;
  qreal m_newTime = 0.0;
  qreal m_newValue = 0.0;
};

/**
 * @brief Batch command for editing multiple curve points
 */
class CurveEditCommand : public QUndoCommand {
public:
  CurveEditCommand(NMCurveEditorPanel *panel,
                   const QString &description = "Edit Curve",
                   QUndoCommand *parent = nullptr);

  void undo() override;
  void redo() override;

  /**
   * @brief Add a point modification to the batch
   */
  void addPointChange(CurvePointId pointId, qreal oldTime, qreal oldValue,
                      qreal newTime, qreal newValue);

private:
  QPointer<NMCurveEditorPanel> m_panel;
  struct PointChange {
    CurvePointId id;
    qreal oldTime;
    qreal oldValue;
    qreal newTime;
    qreal newValue;
  };
  QVector<PointChange> m_changes;
};

} // namespace NovelMind::editor::qt
