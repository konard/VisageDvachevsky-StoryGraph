#include "NovelMind/editor/qt/qt_event_bus.hpp"

namespace NovelMind::editor::qt {

QtEventBus &QtEventBus::instance() {
  static QtEventBus instance;
  return instance;
}

QtEventBus::QtEventBus() : QObject(nullptr) {}

void QtEventBus::publish(const QtEditorEvent &event) {
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

  default:
    break;
  }
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

} // namespace NovelMind::editor::qt
