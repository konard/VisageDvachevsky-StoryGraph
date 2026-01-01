#include "NovelMind/editor/qt/qt_event_bus.hpp"

namespace NovelMind::editor::qt {

// Thread-local flag to prevent re-entrance during signal emission
thread_local bool g_publishingEvent = false;

QtEventBus &QtEventBus::instance() {
  static QtEventBus instance;
  return instance;
}

QtEventBus::QtEventBus() : QObject(nullptr) {}

void QtEventBus::publish(const QtEditorEvent &event) {
  // Prevent re-entrance if a signal handler calls publish() again
  if (g_publishingEvent) {
    return;
  }

  g_publishingEvent = true;
  emit eventPublished(event);

  // Emit type-specific signals
  switch (event.type) {
  case QtEditorEventType::SelectionChanged:
    emit selectionChanged(event.data.value("selectedIds").toStringList(),
                          event.data.value("selectionType").toString());
    break;

  case QtEditorEventType::PropertyChanged:
    emit propertyChanged(event.data.value("objectId").toString(),
                         event.data.value("propertyName").toString(),
                         event.data.value("oldValue"),
                         event.data.value("newValue"));
    break;

  case QtEditorEventType::ProjectOpened:
    emit projectOpened(event.data.value("projectPath").toString());
    break;

  case QtEditorEventType::ProjectClosed:
    emit projectClosed();
    break;

  case QtEditorEventType::ProjectSaved:
    emit projectSaved(event.data.value("projectPath").toString());
    break;

  case QtEditorEventType::UndoPerformed:
    emit undoPerformed(event.data.value("actionDescription").toString());
    break;

  case QtEditorEventType::RedoPerformed:
    emit redoPerformed(event.data.value("actionDescription").toString());
    break;

  case QtEditorEventType::PlayModeStarted:
    emit playModeStarted();
    break;

  case QtEditorEventType::PlayModeStopped:
    emit playModeStopped();
    break;

  case QtEditorEventType::LogMessage:
    emit logMessage(event.data.value("message").toString(),
                    event.data.value("source").toString(),
                    event.data.value("level").toInt());
    break;

  case QtEditorEventType::ErrorOccurred:
    emit errorOccurred(event.data.value("message").toString(),
                       event.data.value("details").toString());
    break;

  case QtEditorEventType::GraphNodeAdded:
    emit graphNodeAdded(event.data.value("nodeId").toString(),
                        event.data.value("nodeType").toString(),
                        event.data.value("nodeData").toMap());
    break;

  case QtEditorEventType::GraphNodeRemoved:
    emit graphNodeRemoved(event.data.value("nodeId").toString());
    break;

  case QtEditorEventType::GraphConnectionAdded:
    emit graphConnectionAdded(event.data.value("connectionId").toString(),
                              event.data.value("sourceNodeId").toString(),
                              event.data.value("targetNodeId").toString());
    break;

  case QtEditorEventType::GraphConnectionRemoved:
    emit graphConnectionRemoved(event.data.value("connectionId").toString());
    break;

  case QtEditorEventType::AssetImported:
    emit assetImported(event.data.value("assetPath").toString(),
                       event.data.value("targetDir").toString());
    break;

  case QtEditorEventType::AssetDeleted:
    emit assetDeleted(event.data.value("assetPath").toString());
    break;

  case QtEditorEventType::AssetRenamed:
    emit assetRenamed(event.data.value("oldPath").toString(),
                      event.data.value("newPath").toString());
    break;

  default:
    break;
  }

  g_publishingEvent = false;
}

void QtEventBus::publishSelectionChanged(const QStringList &selectedIds,
                                         const QString &selectionType) {
  QtEditorEvent event;
  event.type = QtEditorEventType::SelectionChanged;
  event.data["selectedIds"] = selectedIds;
  event.data["selectionType"] = selectionType;
  publish(event);
}

void QtEventBus::publishPropertyChanged(const QString &objectId,
                                        const QString &propertyName,
                                        const QVariant &oldValue,
                                        const QVariant &newValue) {
  QtEditorEvent event;
  event.type = QtEditorEventType::PropertyChanged;
  event.data["objectId"] = objectId;
  event.data["propertyName"] = propertyName;
  event.data["oldValue"] = oldValue;
  event.data["newValue"] = newValue;
  publish(event);
}

void QtEventBus::publishLogMessage(const QString &message,
                                   const QString &source, int level) {
  QtEditorEvent event;
  event.type = QtEditorEventType::LogMessage;
  event.data["message"] = message;
  event.data["source"] = source;
  event.data["level"] = level;
  publish(event);
}

void QtEventBus::publishNavigationRequest(const QString &locationString) {
  emit navigationRequested(locationString);
}

void QtEventBus::publishGraphNodeAdded(const QString &nodeId,
                                       const QString &nodeType,
                                       const QVariantMap &nodeData) {
  QtEditorEvent event;
  event.type = QtEditorEventType::GraphNodeAdded;
  event.data["nodeId"] = nodeId;
  event.data["nodeType"] = nodeType;
  event.data["nodeData"] = nodeData;
  publish(event);
}

void QtEventBus::publishGraphNodeRemoved(const QString &nodeId) {
  QtEditorEvent event;
  event.type = QtEditorEventType::GraphNodeRemoved;
  event.data["nodeId"] = nodeId;
  publish(event);
}

void QtEventBus::publishGraphConnectionAdded(const QString &connectionId,
                                             const QString &sourceNodeId,
                                             const QString &targetNodeId) {
  QtEditorEvent event;
  event.type = QtEditorEventType::GraphConnectionAdded;
  event.data["connectionId"] = connectionId;
  event.data["sourceNodeId"] = sourceNodeId;
  event.data["targetNodeId"] = targetNodeId;
  publish(event);
}

void QtEventBus::publishGraphConnectionRemoved(const QString &connectionId) {
  QtEditorEvent event;
  event.type = QtEditorEventType::GraphConnectionRemoved;
  event.data["connectionId"] = connectionId;
  publish(event);
}

void QtEventBus::publishAssetImported(const QString &assetPath,
                                      const QString &targetDir) {
  QtEditorEvent event;
  event.type = QtEditorEventType::AssetImported;
  event.source = "AssetPipeline";
  event.data["assetPath"] = assetPath;
  event.data["targetDir"] = targetDir;
  publish(event);
}

void QtEventBus::publishAssetDeleted(const QString &assetPath) {
  QtEditorEvent event;
  event.type = QtEditorEventType::AssetDeleted;
  event.source = "AssetPipeline";
  event.data["assetPath"] = assetPath;
  publish(event);
}

void QtEventBus::publishAssetRenamed(const QString &oldPath,
                                     const QString &newPath) {
  QtEditorEvent event;
  event.type = QtEditorEventType::AssetRenamed;
  event.source = "AssetPipeline";
  event.data["oldPath"] = oldPath;
  event.data["newPath"] = newPath;
  publish(event);
}

} // namespace NovelMind::editor::qt
