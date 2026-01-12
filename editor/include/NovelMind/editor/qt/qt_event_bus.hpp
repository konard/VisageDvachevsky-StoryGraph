#pragma once

/**
 * @file qt_event_bus.hpp
 * @brief Qt-based Event Bus for editor communication
 *
 * Wraps the existing EventBus with Qt signals/slots for seamless
 * integration with Qt widgets.
 */

#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>

namespace NovelMind::editor::qt {

/**
 * @brief Qt-compatible event types
 */
enum class QtEditorEventType {
  SelectionChanged,
  PropertyChanged,
  GraphNodeAdded,
  GraphNodeRemoved,
  GraphConnectionAdded,
  GraphConnectionRemoved,
  ProjectOpened,
  ProjectClosed,
  ProjectSaved,
  UndoPerformed,
  RedoPerformed,
  PlayModeStarted,
  PlayModeStopped,
  LogMessage,
  ErrorOccurred,
  AssetImported,
  AssetDeleted,
  AssetRenamed,
  Custom
};

/**
 * @brief Qt-based event data
 */
struct QtEditorEvent {
  QtEditorEventType type = QtEditorEventType::Custom;
  QString source;
  QVariantMap data;
};

/**
 * @brief Qt Event Bus singleton for editor-wide communication
 *
 * This class provides a Qt signals/slots based event system that integrates
 * with the existing C++ EventBus while providing Qt-native interfaces.
 */
class QtEventBus : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static QtEventBus& instance();

  /**
   * @brief Publish an event
   */
  void publish(const QtEditorEvent& event);

  /**
   * @brief Convenience method to publish selection change
   */
  void publishSelectionChanged(const QStringList& selectedIds, const QString& selectionType);

  /**
   * @brief Convenience method to publish property change
   */
  void publishPropertyChanged(const QString& objectId, const QString& propertyName,
                              const QVariant& oldValue, const QVariant& newValue);

  /**
   * @brief Convenience method to publish log message
   */
  void publishLogMessage(const QString& message, const QString& source, int level);

  /**
   * @brief Convenience method to publish navigation request
   */
  void publishNavigationRequest(const QString& locationString);

  /**
   * @brief Convenience method to publish graph node added
   */
  void publishGraphNodeAdded(const QString& nodeId, const QString& nodeType,
                             const QVariantMap& nodeData);

  /**
   * @brief Convenience method to publish graph node removed
   */
  void publishGraphNodeRemoved(const QString& nodeId);

  /**
   * @brief Convenience method to publish graph connection added
   */
  void publishGraphConnectionAdded(const QString& connectionId, const QString& sourceNodeId,
                                   const QString& targetNodeId);

  /**
   * @brief Convenience method to publish graph connection removed
   */
  void publishGraphConnectionRemoved(const QString& connectionId);

  /**
   * @brief Convenience method to publish asset imported event
   */
  void publishAssetImported(const QString& assetPath, const QString& targetDir);

  /**
   * @brief Convenience method to publish asset deleted event
   */
  void publishAssetDeleted(const QString& assetPath);

  /**
   * @brief Convenience method to publish asset renamed event
   */
  void publishAssetRenamed(const QString& oldPath, const QString& newPath);

signals:
  /**
   * @brief Emitted for all events
   */
  void eventPublished(const QtEditorEvent& event);

  /**
   * @brief Emitted when selection changes
   */
  void selectionChanged(const QStringList& selectedIds, const QString& selectionType);

  /**
   * @brief Emitted when a property changes
   */
  void propertyChanged(const QString& objectId, const QString& propertyName,
                       const QVariant& oldValue, const QVariant& newValue);

  /**
   * @brief Emitted when a project is opened
   */
  void projectOpened(const QString& projectPath);

  /**
   * @brief Emitted when a project is closed
   */
  void projectClosed();

  /**
   * @brief Emitted when a project is saved
   */
  void projectSaved(const QString& projectPath);

  /**
   * @brief Emitted when undo is performed
   */
  void undoPerformed(const QString& actionDescription);

  /**
   * @brief Emitted when redo is performed
   */
  void redoPerformed(const QString& actionDescription);

  /**
   * @brief Emitted when play mode starts
   */
  void playModeStarted();

  /**
   * @brief Emitted when play mode stops
   */
  void playModeStopped();

  /**
   * @brief Emitted for log messages
   */
  void logMessage(const QString& message, const QString& source, int level);

  /**
   * @brief Emitted on errors
   */
  void errorOccurred(const QString& message, const QString& details);

  /**
   * @brief Emitted when navigation to a location is requested
   * @param locationString Location in format "StoryGraph:node_id" or
   * "Script:path:line"
   */
  void navigationRequested(const QString& locationString);

  /**
   * @brief Emitted when a graph node is added
   */
  void graphNodeAdded(const QString& nodeId, const QString& nodeType, const QVariantMap& nodeData);

  /**
   * @brief Emitted when a graph node is removed
   */
  void graphNodeRemoved(const QString& nodeId);

  /**
   * @brief Emitted when a graph connection is added
   */
  void graphConnectionAdded(const QString& connectionId, const QString& sourceNodeId,
                            const QString& targetNodeId);

  /**
   * @brief Emitted when a graph connection is removed
   */
  void graphConnectionRemoved(const QString& connectionId);

  /**
   * @brief Emitted when an asset is imported
   */
  void assetImported(const QString& assetPath, const QString& targetDir);

  /**
   * @brief Emitted when an asset is deleted
   */
  void assetDeleted(const QString& assetPath);

  /**
   * @brief Emitted when an asset is renamed
   */
  void assetRenamed(const QString& oldPath, const QString& newPath);

private:
  QtEventBus();
  ~QtEventBus() override = default;

  // Prevent copying
  QtEventBus(const QtEventBus&) = delete;
  QtEventBus& operator=(const QtEventBus&) = delete;
};

} // namespace NovelMind::editor::qt
