#pragma once

/**
 * @file property_mediator.hpp
 * @brief Mediator for coordinating property changes across panels
 *
 * The PropertyMediator handles:
 * - Inspector → SceneView property updates
 * - SceneView → Inspector property synchronization
 * - Inspector → StoryGraph property updates
 * - Curve editor integration
 */

#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include <QObject>
#include <memory>
#include <vector>

namespace NovelMind::editor::qt {
class NMSceneViewPanel;
class NMInspectorPanel;
class NMStoryGraphPanel;
class NMCurveEditorPanel;
class NMScriptEditorPanel;
class NMScriptDocPanel;
} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for property change coordination
 *
 * This class handles property synchronization between inspector,
 * scene view, story graph, and other panels that display/edit properties.
 */
class PropertyMediator : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the property mediator
   */
  PropertyMediator(qt::NMSceneViewPanel* sceneView, qt::NMInspectorPanel* inspector,
                   qt::NMStoryGraphPanel* storyGraph, qt::NMCurveEditorPanel* curveEditor,
                   qt::NMScriptEditorPanel* scriptEditor, qt::NMScriptDocPanel* scriptDoc,
                   QObject* parent = nullptr);

  ~PropertyMediator() override;

  /**
   * @brief Initialize event subscriptions
   */
  void initialize();

  /**
   * @brief Shutdown and cleanup subscriptions
   */
  void shutdown();

private:
  void onInspectorPropertyChanged(const events::InspectorPropertyChangedEvent& event);
  void onSceneObjectPositionChanged(const events::SceneObjectPositionChangedEvent& event);
  void onSceneObjectTransformFinished(const events::SceneObjectTransformFinishedEvent& event);
  void onUpdateInspectorProperty(const events::UpdateInspectorPropertyEvent& event);
  void onOpenCurveEditorRequested(const events::OpenCurveEditorRequestedEvent& event);
  void onCurveChanged(const events::CurveChangedEvent& event);
  void onScriptDocHtmlChanged(const events::ScriptDocHtmlChangedEvent& event);

  void applyPropertyToSceneObject(const QString& objectId, const QString& propertyName,
                                  const QString& value);
  void applyPropertyToStoryGraphNode(const QString& nodeId, const QString& propertyName,
                                     const QString& value);

  qt::NMSceneViewPanel* m_sceneView = nullptr;
  qt::NMInspectorPanel* m_inspector = nullptr;
  qt::NMStoryGraphPanel* m_storyGraph = nullptr;
  qt::NMCurveEditorPanel* m_curveEditor = nullptr;
  qt::NMScriptEditorPanel* m_scriptEditor = nullptr;
  qt::NMScriptDocPanel* m_scriptDoc = nullptr;

  std::vector<EventSubscription> m_subscriptions;
  bool m_processingProperty = false; // Prevent feedback loops
};

} // namespace NovelMind::editor::mediators
