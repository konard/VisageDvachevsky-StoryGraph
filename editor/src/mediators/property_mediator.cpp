#include "NovelMind/editor/mediators/property_mediator.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QDebug>

namespace NovelMind::editor::mediators {

PropertyMediator::PropertyMediator(qt::NMSceneViewPanel* sceneView, qt::NMInspectorPanel* inspector,
                                   qt::NMStoryGraphPanel* storyGraph,
                                   qt::NMCurveEditorPanel* curveEditor,
                                   qt::NMScriptEditorPanel* scriptEditor,
                                   qt::NMScriptDocPanel* scriptDoc, QObject* parent)
    : QObject(parent), m_sceneView(sceneView), m_inspector(inspector), m_storyGraph(storyGraph),
      m_curveEditor(curveEditor), m_scriptEditor(scriptEditor), m_scriptDoc(scriptDoc) {}

PropertyMediator::~PropertyMediator() {
  shutdown();
}

void PropertyMediator::initialize() {
  auto& bus = EventBus::instance();

  // Property change events from inspector
  m_subscriptions.push_back(bus.subscribe<events::InspectorPropertyChangedEvent>(
      [this](const events::InspectorPropertyChangedEvent& event) {
        onInspectorPropertyChanged(event);
      }));

  // Scene object position changes from scene view
  m_subscriptions.push_back(bus.subscribe<events::SceneObjectPositionChangedEvent>(
      [this](const events::SceneObjectPositionChangedEvent& event) {
        onSceneObjectPositionChanged(event);
      }));

  // Scene object transform finished
  m_subscriptions.push_back(bus.subscribe<events::SceneObjectTransformFinishedEvent>(
      [this](const events::SceneObjectTransformFinishedEvent& event) {
        onSceneObjectTransformFinished(event);
      }));

  // Update inspector property (from other panels)
  m_subscriptions.push_back(bus.subscribe<events::UpdateInspectorPropertyEvent>(
      [this](const events::UpdateInspectorPropertyEvent& event) {
        onUpdateInspectorProperty(event);
      }));

  // Curve editor requests
  m_subscriptions.push_back(bus.subscribe<events::OpenCurveEditorRequestedEvent>(
      [this](const events::OpenCurveEditorRequestedEvent& event) {
        onOpenCurveEditorRequested(event);
      }));

  // Curve changes
  m_subscriptions.push_back(bus.subscribe<events::CurveChangedEvent>(
      [this](const events::CurveChangedEvent& event) { onCurveChanged(event); }));

  // Script doc HTML changes
  m_subscriptions.push_back(bus.subscribe<events::ScriptDocHtmlChangedEvent>(
      [this](const events::ScriptDocHtmlChangedEvent& event) { onScriptDocHtmlChanged(event); }));

  // =========================================================================
  // Issue #203: Connect Qt signals to EventBus for panel integration
  // This bridges the gap between Qt's signal/slot mechanism and the EventBus
  // pattern used by mediators.
  // =========================================================================

  // Connect Inspector Panel's propertyChanged signal to publish EventBus event
  if (m_inspector) {
    connect(m_inspector, &qt::NMInspectorPanel::propertyChanged, this,
            [](const QString& objectId, const QString& propertyName, const QString& newValue) {
              qDebug() << "[PropertyMediator] Publishing "
                          "InspectorPropertyChangedEvent for property:"
                       << propertyName << "on" << objectId;

              events::InspectorPropertyChangedEvent event;
              event.objectId = objectId;
              event.propertyName = propertyName;
              event.newValue = newValue;

              EventBus::instance().publish(event);
            });
  }

  qDebug() << "[PropertyMediator] Initialized with" << m_subscriptions.size() << "subscriptions";
}

void PropertyMediator::shutdown() {
  auto& bus = EventBus::instance();
  for (const auto& sub : m_subscriptions) {
    bus.unsubscribe(sub);
  }
  m_subscriptions.clear();
  qDebug() << "[PropertyMediator] Shutdown complete";
}

void PropertyMediator::onInspectorPropertyChanged(
    const events::InspectorPropertyChangedEvent& event) {
  if (m_processingProperty || event.objectId.isEmpty()) {
    return;
  }
  m_processingProperty = true;

  qDebug() << "[PropertyMediator] Property changed:" << event.propertyName << "=" << event.newValue
           << "on" << event.objectId;

  // Check if this is a curve editor open request
  if (event.propertyName.endsWith(":openCurveEditor")) {
    events::OpenCurveEditorRequestedEvent curveEvent;
    curveEvent.propertyName =
        event.propertyName.left(event.propertyName.length() - QString(":openCurveEditor").length());
    curveEvent.curveData = event.newValue;
    EventBus::instance().publish(curveEvent);
    m_processingProperty = false;
    return;
  }

  // Try to apply to scene object first
  if (m_sceneView && m_sceneView->findObjectById(event.objectId)) {
    applyPropertyToSceneObject(event.objectId, event.propertyName, event.newValue);
  }
  // Otherwise try story graph node
  else if (m_storyGraph && m_storyGraph->findNodeByIdString(event.objectId)) {
    applyPropertyToStoryGraphNode(event.objectId, event.propertyName, event.newValue);
  }

  m_processingProperty = false;
}

void PropertyMediator::applyPropertyToSceneObject(const QString& objectId,
                                                  const QString& propertyName,
                                                  const QString& value) {
  if (!m_sceneView) {
    return;
  }

  auto* obj = m_sceneView->findObjectById(objectId);
  if (!obj) {
    return;
  }

  if (propertyName == "name") {
    m_sceneView->renameObject(objectId, value);
    m_sceneView->selectObjectById(objectId);
  } else if (propertyName == "asset") {
    m_sceneView->setObjectAsset(objectId, value);
    m_sceneView->selectObjectById(objectId);
  } else if (propertyName == "position_x" || propertyName == "position_y") {
    QPointF pos = obj->pos();
    if (propertyName == "position_x") {
      pos.setX(value.toDouble());
    } else {
      pos.setY(value.toDouble());
    }
    m_sceneView->moveObject(objectId, pos);
  } else if (propertyName == "rotation") {
    m_sceneView->rotateObject(objectId, value.toDouble());
  } else if (propertyName == "scale_x" || propertyName == "scale_y") {
    QPointF scale = m_sceneView->graphicsScene()->getObjectScale(objectId);
    if (propertyName == "scale_x") {
      scale.setX(value.toDouble());
    } else {
      scale.setY(value.toDouble());
    }
    m_sceneView->scaleObject(objectId, scale.x(), scale.y());
  } else if (propertyName == "visible") {
    const bool newVisible = (value.toLower() == "true" || value == "1");
    const bool oldVisible = obj->isVisible();
    if (oldVisible != newVisible) {
      auto* cmd =
          new qt::ToggleObjectVisibilityCommand(m_sceneView, objectId, oldVisible, newVisible);
      qt::NMUndoManager::instance().pushCommand(cmd);
    }
  } else if (propertyName == "alpha") {
    m_sceneView->setObjectOpacity(objectId, value.toDouble());
  } else if (propertyName == "z") {
    m_sceneView->setObjectZOrder(objectId, value.toDouble());
  } else if (propertyName == "locked") {
    const bool newLocked = (value.toLower() == "true" || value == "1");
    const bool oldLocked = obj->isLocked();
    if (oldLocked != newLocked) {
      auto* cmd = new qt::ToggleObjectLockedCommand(m_sceneView, objectId, oldLocked, newLocked);
      qt::NMUndoManager::instance().pushCommand(cmd);
    }
  }

  // Update inspector with the new value
  if (m_inspector && m_inspector->currentObjectId() == objectId) {
    m_inspector->updatePropertyValue(propertyName, value);
  }
}

void PropertyMediator::applyPropertyToStoryGraphNode(const QString& nodeId,
                                                     const QString& propertyName,
                                                     const QString& value) {
  if (!m_storyGraph) {
    return;
  }

  m_storyGraph->applyNodePropertyChange(nodeId, propertyName, value);

  // If script path changed, open the script
  if (propertyName == "scriptPath" && m_scriptEditor) {
    m_scriptEditor->openScript(value);
  }

  // Update scene preview if dialogue properties changed
  if (m_sceneView) {
    if (auto* node = m_storyGraph->findNodeByIdString(nodeId)) {
      m_sceneView->setStoryPreview(node->dialogueSpeaker(), node->dialogueText(),
                                   node->choiceOptions());
    }
  }

  // Update inspector with the new value
  if (m_inspector && m_inspector->currentObjectId() == nodeId) {
    m_inspector->updatePropertyValue(propertyName, value);
  }
}

void PropertyMediator::onSceneObjectPositionChanged(
    const events::SceneObjectPositionChangedEvent& event) {
  if (m_processingProperty) {
    return;
  }
  m_processingProperty = true;

  if (m_inspector && m_inspector->currentObjectId() == event.objectId) {
    m_inspector->updatePropertyValue("position_x", QString::number(event.newPosition.x()));
    m_inspector->updatePropertyValue("position_y", QString::number(event.newPosition.y()));
  }

  m_processingProperty = false;
}

void PropertyMediator::onSceneObjectTransformFinished(
    const events::SceneObjectTransformFinishedEvent& event) {
  if (m_processingProperty) {
    return;
  }
  m_processingProperty = true;

  if (m_inspector && m_inspector->currentObjectId() == event.objectId) {
    m_inspector->updatePropertyValue("position_x", QString::number(event.newPosition.x()));
    m_inspector->updatePropertyValue("position_y", QString::number(event.newPosition.y()));
    m_inspector->updatePropertyValue("rotation", QString::number(event.newRotation));
    m_inspector->updatePropertyValue("scale_x", QString::number(event.newScaleX));
    m_inspector->updatePropertyValue("scale_y", QString::number(event.newScaleY));
  }

  m_processingProperty = false;
}

void PropertyMediator::onUpdateInspectorProperty(
    const events::UpdateInspectorPropertyEvent& event) {
  if (m_processingProperty) {
    return;
  }
  m_processingProperty = true;

  if (m_inspector && m_inspector->currentObjectId() == event.objectId) {
    m_inspector->updatePropertyValue(event.propertyName, event.value);
  }

  m_processingProperty = false;
}

void PropertyMediator::onOpenCurveEditorRequested(
    const events::OpenCurveEditorRequestedEvent& event) {
  if (!m_curveEditor) {
    return;
  }

  qDebug() << "[PropertyMediator] Opening curve editor for:" << event.propertyName;

  m_curveEditor->setCurve(event.curveData);
  m_curveEditor->show();
  m_curveEditor->raise();
  m_curveEditor->setFocus();
}

void PropertyMediator::onCurveChanged(const events::CurveChangedEvent& event) {
  qDebug() << "[PropertyMediator] Curve changed:" << event.curveId;
  // Curve editing triggers document modification tracking through the undo system
}

void PropertyMediator::onScriptDocHtmlChanged(const events::ScriptDocHtmlChangedEvent& event) {
  if (m_scriptDoc) {
    m_scriptDoc->setDocHtml(event.docHtml);
  }
}

} // namespace NovelMind::editor::mediators
