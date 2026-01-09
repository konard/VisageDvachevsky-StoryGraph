#pragma once

/**
 * @file panel_events.hpp
 * @brief Domain-specific events for panel communication via EventBus
 *
 * This file defines all events used for communication between editor panels.
 * Using the EventBus pattern instead of direct connections decouples panels
 * from each other, making the codebase more maintainable and extensible.
 *
 * Usage:
 * - Publishers: EventBus::instance().publish(SomeEvent{...});
 * - Subscribers: EventBus::instance().subscribe<SomeEvent>([](const auto& e){...});
 */

#include "NovelMind/editor/event_bus.hpp"
#include <QPointF>
#include <QString>
#include <QStringList>
#include <vector>

namespace NovelMind::editor::events {

// ============================================================================
// Scene Object Events
// ============================================================================

/**
 * @brief Emitted when a scene object is selected in any panel
 */
struct SceneObjectSelectedEvent : EditorEvent {
  QString objectId;
  QString sourcePanel; // e.g., "SceneView", "Hierarchy"
  bool editable = true;

  SceneObjectSelectedEvent() : EditorEvent(EditorEventType::SelectionChanged) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene object selected: " + objectId.toStdString();
  }
};

/**
 * @brief Emitted when a scene object's position changes
 */
struct SceneObjectPositionChangedEvent : EditorEvent {
  QString objectId;
  QPointF newPosition;

  SceneObjectPositionChangedEvent() : EditorEvent(EditorEventType::SceneObjectMoved) {}
};

/**
 * @brief Emitted when a scene object's transform is finalized (drag complete)
 */
struct SceneObjectTransformFinishedEvent : EditorEvent {
  QString objectId;
  QPointF oldPosition;
  QPointF newPosition;
  qreal oldRotation = 0.0;
  qreal newRotation = 0.0;
  qreal oldScaleX = 1.0;
  qreal newScaleX = 1.0;
  qreal oldScaleY = 1.0;
  qreal newScaleY = 1.0;

  SceneObjectTransformFinishedEvent() : EditorEvent(EditorEventType::SceneObjectTransformed) {}
};

/**
 * @brief Emitted when the scene objects collection changes (add/remove)
 */
struct SceneObjectsChangedEvent : EditorEvent {
  SceneObjectsChangedEvent() : EditorEvent(EditorEventType::SceneObjectAdded) {}
};

/**
 * @brief Emitted to request creating a new scene object
 */
struct CreateSceneObjectRequestedEvent : EditorEvent {
  int objectType = 0; // NMSceneObjectType
  QPointF position;
  QString assetPath;

  CreateSceneObjectRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Story Graph Events
// ============================================================================

/**
 * @brief Emitted when a story graph node is selected
 */
struct StoryGraphNodeSelectedEvent : EditorEvent {
  QString nodeIdString;
  QString nodeType;
  QString dialogueSpeaker;
  QString dialogueText;
  QStringList choiceOptions;

  StoryGraphNodeSelectedEvent() : EditorEvent(EditorEventType::SelectionChanged) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Story graph node selected: " + nodeIdString.toStdString();
  }
};

/**
 * @brief Emitted when a story graph node is activated (double-clicked)
 */
struct StoryGraphNodeActivatedEvent : EditorEvent {
  QString nodeIdString;

  StoryGraphNodeActivatedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when a scene node is double-clicked (for scene/timeline editing)
 */
struct SceneNodeDoubleClickedEvent : EditorEvent {
  QString sceneId;

  SceneNodeDoubleClickedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when a script node requires opening
 */
struct ScriptNodeRequestedEvent : EditorEvent {
  QString scriptPath;

  ScriptNodeRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when dialogue flow editing is requested for a scene
 */
struct EditDialogueFlowRequestedEvent : EditorEvent {
  QString sceneId;

  EditDialogueFlowRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when requesting to return to story graph from dialogue graph
 */
struct ReturnToStoryGraphRequestedEvent : EditorEvent {
  ReturnToStoryGraphRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when dialogue count changes for a scene
 */
struct DialogueCountChangedEvent : EditorEvent {
  QString sceneId;
  int count = 0;

  DialogueCountChangedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to request opening a scene script
 */
struct OpenSceneScriptRequestedEvent : EditorEvent {
  QString sceneId;
  QString scriptPath;

  OpenSceneScriptRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Voice/Audio Events
// ============================================================================

/**
 * @brief Emitted when a voice clip assignment is requested
 */
struct VoiceClipAssignRequestedEvent : EditorEvent {
  QString nodeIdString;
  QString currentPath;

  VoiceClipAssignRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when voice auto-detection is requested
 */
struct VoiceAutoDetectRequestedEvent : EditorEvent {
  QString nodeIdString;
  QString localizationKey;

  VoiceAutoDetectRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when a voice clip preview is requested
 */
struct VoiceClipPreviewRequestedEvent : EditorEvent {
  QString nodeIdString;
  QString voicePath;

  VoiceClipPreviewRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when voice recording is requested
 */
struct VoiceRecordingRequestedEvent : EditorEvent {
  QString nodeIdString;
  QString dialogueText;
  QString speaker;

  VoiceRecordingRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Property Events
// ============================================================================

/**
 * @brief Emitted when a property is changed in the inspector
 */
struct InspectorPropertyChangedEvent : EditorEvent {
  QString objectId;
  QString propertyName;
  QString newValue;

  InspectorPropertyChangedEvent() : EditorEvent(EditorEventType::PropertyChanged) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Property '" + propertyName.toStdString() + "' changed on " + objectId.toStdString();
  }
};

/**
 * @brief Emitted to request updating a property value in the inspector
 */
struct UpdateInspectorPropertyEvent : EditorEvent {
  QString objectId;
  QString propertyName;
  QString value;

  UpdateInspectorPropertyEvent() : EditorEvent(EditorEventType::PropertyChanged) {}
};

// ============================================================================
// Asset Events
// ============================================================================

/**
 * @brief Emitted when an asset is selected in the asset browser
 */
struct AssetSelectedEvent : EditorEvent {
  QString path;
  QString type;

  AssetSelectedEvent() : EditorEvent(EditorEventType::AssetModified) {}
};

/**
 * @brief Emitted when an asset is double-clicked in the asset browser
 */
struct AssetDoubleClickedEvent : EditorEvent {
  QString path;

  AssetDoubleClickedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when assets are dropped from the palette
 */
struct AssetsDroppedEvent : EditorEvent {
  QStringList paths;
  int typeHint = -1; // NMSceneObjectType or -1 for auto-detect

  AssetsDroppedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Navigation Events
// ============================================================================

/**
 * @brief Emitted when navigation to a location is requested
 */
struct NavigationRequestedEvent : EditorEvent {
  QString locationString; // Format: "Type:path:line" e.g., "Script:file.nms:42"

  NavigationRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when an issue is activated (e.g., from issues panel)
 */
struct IssueActivatedEvent : EditorEvent {
  QString file;
  int line = 0;

  IssueActivatedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when a diagnostic is activated (e.g., from diagnostics panel)
 */
struct DiagnosticActivatedEvent : EditorEvent {
  QString location;

  DiagnosticActivatedEvent() : EditorEvent(EditorEventType::DiagnosticAdded) {}
};

// ============================================================================
// Script Events
// ============================================================================

/**
 * @brief Emitted when script documentation HTML changes
 */
struct ScriptDocHtmlChangedEvent : EditorEvent {
  QString docHtml;

  ScriptDocHtmlChangedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to request opening a script at a specific location
 */
struct GoToScriptLocationEvent : EditorEvent {
  QString filePath;
  int line = -1;

  GoToScriptLocationEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to navigate from Story Graph node to script definition (Issue #239)
 *
 * This event enables bidirectional navigation between Story Graph and Script Editor.
 * When a user requests to "Open Script Definition" from a graph node, this event
 * is published to navigate to the exact line where the scene is defined.
 */
struct NavigateToScriptDefinitionEvent : EditorEvent {
  QString sceneId;    ///< The scene ID to find in scripts
  QString scriptPath; ///< Optional: known script path if available
  int line = -1;      ///< Optional: line number if known

  NavigateToScriptDefinitionEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Navigate to script definition: " + sceneId.toStdString();
  }
};

// ============================================================================
// Timeline/Animation Events
// ============================================================================

/**
 * @brief Emitted when the timeline frame changes
 */
struct TimelineFrameChangedEvent : EditorEvent {
  int frame = 0;

  TimelineFrameChangedEvent() : EditorEvent(EditorEventType::TimelinePlaybackChanged) {}
};

/**
 * @brief Emitted when timeline playback state changes
 */
struct TimelinePlaybackStateChangedEvent : EditorEvent {
  bool playing = false;

  TimelinePlaybackStateChangedEvent() : EditorEvent(EditorEventType::TimelinePlaybackChanged) {}
};

/**
 * @brief Emitted to open curve editor for a property
 */
struct OpenCurveEditorRequestedEvent : EditorEvent {
  QString propertyName;
  QString curveData;

  OpenCurveEditorRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted when a curve changes in the curve editor
 */
struct CurveChangedEvent : EditorEvent {
  QString curveId;

  CurveChangedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Hierarchy Events
// ============================================================================

/**
 * @brief Emitted when an object is double-clicked in hierarchy
 */
struct HierarchyObjectDoubleClickedEvent : EditorEvent {
  QString objectId;

  HierarchyObjectDoubleClickedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to request refreshing the hierarchy panel
 */
struct RefreshHierarchyRequestedEvent : EditorEvent {
  RefreshHierarchyRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Panel Focus Events
// ============================================================================

/**
 * @brief Emitted when a panel should be shown and focused
 */
struct ShowPanelRequestedEvent : EditorEvent {
  QString panelName;
  bool raisePanel = true;
  bool focusPanel = false;

  ShowPanelRequestedEvent() : EditorEvent(EditorEventType::PanelFocusChanged) {}
};

// ============================================================================
// Status Bar Events
// ============================================================================

/**
 * @brief Emitted to update the status bar message
 */
struct StatusMessageEvent : EditorEvent {
  QString message;
  int timeoutMs = 0;

  StatusMessageEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to update status bar context (selection, node, etc.)
 */
struct StatusContextChangedEvent : EditorEvent {
  QString selectionLabel;
  QString nodeId;
  QString assetPath;

  StatusContextChangedEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Story Preview Events
// ============================================================================

/**
 * @brief Emitted to set story preview in scene view
 */
struct SetStoryPreviewEvent : EditorEvent {
  QString speaker;
  QString text;
  QStringList choices;

  SetStoryPreviewEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to clear story preview in scene view
 */
struct ClearStoryPreviewEvent : EditorEvent {
  ClearStoryPreviewEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Scene Document Events
// ============================================================================

/**
 * @brief Emitted to request loading a scene document
 */
struct LoadSceneDocumentRequestedEvent : EditorEvent {
  QString sceneId;

  LoadSceneDocumentRequestedEvent() : EditorEvent(EditorEventType::Custom) {}
};

/**
 * @brief Emitted to request creating a new scene (Issue #329)
 *
 * This event is published when the Inspector panel or other UI triggers
 * the new scene creation workflow. The main window or project manager
 * should handle this by showing the new scene dialog.
 */
struct CreateSceneRequestedEvent : EditorEvent {
  CreateSceneRequestedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override { return "Create new scene requested"; }
};

/**
 * @brief Emitted to request enabling/disabling animation preview mode
 */
struct SetAnimationPreviewModeEvent : EditorEvent {
  bool enabled = false;

  SetAnimationPreviewModeEvent() : EditorEvent(EditorEventType::Custom) {}
};

// ============================================================================
// Scene Registry Auto-Sync Events (Issue #213)
// ============================================================================

/**
 * @brief Emitted when a new scene is created and registered
 *
 * This event allows panels to respond to new scenes being added to the project.
 * For example, the Story Graph can add the scene to its scene picker dropdown.
 */
struct SceneCreatedEvent : EditorEvent {
  QString sceneId;   ///< ID of the newly created scene
  QString sceneName; ///< Display name of the scene

  SceneCreatedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene created: " + sceneId.toStdString();
  }
};

/**
 * @brief Emitted when a scene is renamed in the Scene Registry
 *
 * This event triggers automatic updates of all Story Graph nodes that reference
 * the renamed scene, ensuring consistency across the project.
 */
struct SceneRenamedEvent : EditorEvent {
  QString sceneId; ///< Scene ID (unchanged)
  QString oldName; ///< Previous display name
  QString newName; ///< New display name

  SceneRenamedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene renamed: " + sceneId.toStdString() + " (" + oldName.toStdString() + " -> " +
           newName.toStdString() + ")";
  }
};

/**
 * @brief Emitted when a scene is deleted/unregistered from the project
 *
 * This event allows panels to validate references and show warnings for
 * orphaned scene references in the Story Graph.
 */
struct SceneDeletedEvent : EditorEvent {
  QString sceneId; ///< ID of the deleted scene

  SceneDeletedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene deleted: " + sceneId.toStdString();
  }
};

/**
 * @brief Emitted when a scene document (.nmscene file) is modified
 *
 * This event triggers thumbnail regeneration and updates in the Story Graph
 * to reflect the latest scene content.
 */
struct SceneDocumentModifiedEvent : EditorEvent {
  QString sceneId; ///< ID of the modified scene

  SceneDocumentModifiedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene document modified: " + sceneId.toStdString();
  }
};

/**
 * @brief Emitted when a scene's thumbnail is updated
 *
 * This event notifies the Story Graph to refresh the thumbnail display
 * for all nodes referencing this scene.
 */
struct SceneThumbnailUpdatedEvent : EditorEvent {
  QString sceneId;       ///< ID of the scene
  QString thumbnailPath; ///< Path to the updated thumbnail image

  SceneThumbnailUpdatedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene thumbnail updated: " + sceneId.toStdString();
  }
};

/**
 * @brief Emitted when scene metadata changes (tags, description, etc.)
 *
 * This event allows panels to update their displays when scene metadata
 * is modified through the Inspector or other panels.
 */
struct SceneMetadataUpdatedEvent : EditorEvent {
  QString sceneId; ///< ID of the scene with updated metadata

  SceneMetadataUpdatedEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Scene metadata updated: " + sceneId.toStdString();
  }
};

/**
 * @brief Request to sync thumbnails for all scene nodes in Story Graph
 *
 * This event can be manually triggered to force a refresh of all
 * scene thumbnails in the Story Graph.
 */
struct SyncSceneToGraphEvent : EditorEvent {
  QString sceneId; ///< Specific scene to sync, or empty for all scenes

  SyncSceneToGraphEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    if (sceneId.isEmpty()) {
      return "Sync all scenes to Story Graph";
    }
    return "Sync scene to Story Graph: " + sceneId.toStdString();
  }
};

} // namespace NovelMind::editor::events
