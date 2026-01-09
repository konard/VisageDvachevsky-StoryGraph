#include "NovelMind/editor/mediators/workflow_mediator.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_dialogue_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <filesystem>

namespace NovelMind::editor::mediators {

WorkflowMediator::WorkflowMediator(
    qt::NMSceneViewPanel* sceneView, qt::NMStoryGraphPanel* storyGraph,
    qt::NMSceneDialogueGraphPanel* dialogueGraph, qt::NMScriptEditorPanel* scriptEditor,
    qt::NMTimelinePanel* timeline, qt::NMVoiceStudioPanel* voiceStudio,
    qt::NMVoiceManagerPanel* voiceManager, qt::NMInspectorPanel* inspector,
    qt::NMDiagnosticsPanel* diagnostics, qt::NMIssuesPanel* issues, QObject* parent)
    : QObject(parent), m_sceneView(sceneView), m_storyGraph(storyGraph),
      m_dialogueGraph(dialogueGraph), m_scriptEditor(scriptEditor), m_timeline(timeline),
      m_voiceStudio(voiceStudio), m_voiceManager(voiceManager), m_inspector(inspector),
      m_diagnostics(diagnostics), m_issues(issues) {}

WorkflowMediator::~WorkflowMediator() {
  shutdown();
}

void WorkflowMediator::initialize() {
  auto& bus = EventBus::instance();

  // Story graph navigation
  m_subscriptions.push_back(bus.subscribe<events::StoryGraphNodeActivatedEvent>(
      [this](const events::StoryGraphNodeActivatedEvent& event) {
        onStoryGraphNodeActivated(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::SceneNodeDoubleClickedEvent>(
      [this](const events::SceneNodeDoubleClickedEvent& event) {
        onSceneNodeDoubleClicked(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::ScriptNodeRequestedEvent>(
      [this](const events::ScriptNodeRequestedEvent& event) { onScriptNodeRequested(event); }));

  // Dialogue graph navigation
  m_subscriptions.push_back(bus.subscribe<events::EditDialogueFlowRequestedEvent>(
      [this](const events::EditDialogueFlowRequestedEvent& event) {
        onEditDialogueFlowRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::ReturnToStoryGraphRequestedEvent>(
      [this](const events::ReturnToStoryGraphRequestedEvent& event) {
        onReturnToStoryGraphRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::DialogueCountChangedEvent>(
      [this](const events::DialogueCountChangedEvent& event) { onDialogueCountChanged(event); }));

  // Script navigation
  m_subscriptions.push_back(bus.subscribe<events::OpenSceneScriptRequestedEvent>(
      [this](const events::OpenSceneScriptRequestedEvent& event) {
        onOpenSceneScriptRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::GoToScriptLocationEvent>(
      [this](const events::GoToScriptLocationEvent& event) { onGoToScriptLocation(event); }));

  // Issue #239: Navigate to script definition (bidirectional navigation)
  m_subscriptions.push_back(bus.subscribe<events::NavigateToScriptDefinitionEvent>(
      [this](const events::NavigateToScriptDefinitionEvent& event) {
        onNavigateToScriptDefinition(event);
      }));

  // Voice recording workflow
  m_subscriptions.push_back(bus.subscribe<events::VoiceClipAssignRequestedEvent>(
      [this](const events::VoiceClipAssignRequestedEvent& event) {
        onVoiceClipAssignRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::VoiceAutoDetectRequestedEvent>(
      [this](const events::VoiceAutoDetectRequestedEvent& event) {
        onVoiceAutoDetectRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::VoiceClipPreviewRequestedEvent>(
      [this](const events::VoiceClipPreviewRequestedEvent& event) {
        onVoiceClipPreviewRequested(event);
      }));

  m_subscriptions.push_back(bus.subscribe<events::VoiceRecordingRequestedEvent>(
      [this](const events::VoiceRecordingRequestedEvent& event) {
        onVoiceRecordingRequested(event);
      }));

  // Issue/diagnostic navigation
  m_subscriptions.push_back(bus.subscribe<events::IssueActivatedEvent>(
      [this](const events::IssueActivatedEvent& event) { onIssueActivated(event); }));

  m_subscriptions.push_back(bus.subscribe<events::DiagnosticActivatedEvent>(
      [this](const events::DiagnosticActivatedEvent& event) { onDiagnosticActivated(event); }));

  m_subscriptions.push_back(bus.subscribe<events::NavigationRequestedEvent>(
      [this](const events::NavigationRequestedEvent& event) { onNavigationRequested(event); }));

  // Asset workflow
  m_subscriptions.push_back(bus.subscribe<events::AssetDoubleClickedEvent>(
      [this](const events::AssetDoubleClickedEvent& event) { onAssetDoubleClicked(event); }));

  m_subscriptions.push_back(bus.subscribe<events::AssetsDroppedEvent>(
      [this](const events::AssetsDroppedEvent& event) { onAssetsDropped(event); }));

  // Scene document loading
  m_subscriptions.push_back(bus.subscribe<events::LoadSceneDocumentRequestedEvent>(
      [this](const events::LoadSceneDocumentRequestedEvent& event) {
        onLoadSceneDocumentRequested(event);
      }));

  // Scene auto-sync events (issue #213)
  m_subscriptions.push_back(bus.subscribe<events::SceneDocumentModifiedEvent>(
      [this](const events::SceneDocumentModifiedEvent& event) { onSceneDocumentModified(event); }));

  m_subscriptions.push_back(bus.subscribe<events::SceneThumbnailUpdatedEvent>(
      [this](const events::SceneThumbnailUpdatedEvent& event) { onSceneThumbnailUpdated(event); }));

  m_subscriptions.push_back(bus.subscribe<events::SceneRenamedEvent>(
      [this](const events::SceneRenamedEvent& event) { onSceneRenamed(event); }));

  m_subscriptions.push_back(bus.subscribe<events::SceneDeletedEvent>(
      [this](const events::SceneDeletedEvent& event) { onSceneDeleted(event); }));

  // Issue #329: Inspector panel scene workflow connections
  if (m_inspector) {
    // Scene creation request from inspector
    QObject::connect(m_inspector, &qt::NMInspectorPanel::createNewSceneRequested, [this]() {
      qDebug() << "[WorkflowMediator] Scene creation requested from inspector";
      // Publish event for main window/project manager to handle
      EventBus::instance().publish(events::CreateSceneRequestedEvent{});
    });

    // Scene editing request from inspector
    QObject::connect(
        m_inspector, &qt::NMInspectorPanel::editSceneRequested, [this](const QString& sceneId) {
          qDebug() << "[WorkflowMediator] Scene editing requested from inspector:" << sceneId;
          if (m_sceneView) {
            m_sceneView->loadSceneDocument(sceneId);
            m_sceneView->show();
            m_sceneView->raise();
          }
        });

    // Scene location request from inspector (highlight in Story Graph)
    QObject::connect(m_inspector, &qt::NMInspectorPanel::locateSceneInGraphRequested,
                     [this](const QString& sceneId) {
                       qDebug() << "[WorkflowMediator] Scene locate requested from inspector:"
                                << sceneId;
                       if (m_storyGraph) {
                         // Navigate to the node that references this scene
                         // The sceneId is actually the node ID in the Story Graph
                         m_storyGraph->navigateToNode(sceneId);
                       }
                     });
  }

  qDebug() << "[WorkflowMediator] Initialized with" << m_subscriptions.size() << "subscriptions";
}

void WorkflowMediator::shutdown() {
  auto& bus = EventBus::instance();
  for (const auto& sub : m_subscriptions) {
    bus.unsubscribe(sub);
  }
  m_subscriptions.clear();
  qDebug() << "[WorkflowMediator] Shutdown complete";
}

void WorkflowMediator::onStoryGraphNodeActivated(
    const events::StoryGraphNodeActivatedEvent& event) {
  if (!m_sceneView || event.nodeIdString.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Story graph node activated:" << event.nodeIdString;

  auto& playController = qt::NMPlayModeController::instance();
  if (!playController.isPlaying() && !playController.isPaused()) {
    m_sceneView->loadSceneDocument(event.nodeIdString);
  }

  m_sceneView->show();
  m_sceneView->raise();
  m_sceneView->setFocus();
}

void WorkflowMediator::onSceneNodeDoubleClicked(const events::SceneNodeDoubleClickedEvent& event) {
  if (!m_sceneView || !m_timeline) {
    return;
  }

  qDebug() << "[WorkflowMediator] Scene node double-clicked:" << event.sceneId;

  // Load scene in Scene View
  auto& playController = qt::NMPlayModeController::instance();
  if (!playController.isPlaying() && !playController.isPaused()) {
    m_sceneView->loadSceneDocument(event.sceneId);
  }

  // Show Scene View and Timeline panels
  m_sceneView->show();
  m_sceneView->raise();

  m_timeline->show();
  m_timeline->raise();

  // Enable animation preview mode
  m_sceneView->setAnimationPreviewMode(true);

  // Publish status message
  events::StatusMessageEvent statusEvent;
  statusEvent.message =
      QObject::tr("Editing scene: %1 (Timeline and Scene View)").arg(event.sceneId);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onScriptNodeRequested(const events::ScriptNodeRequestedEvent& event) {
  if (!m_scriptEditor) {
    return;
  }

  qDebug() << "[WorkflowMediator] Script node requested:" << event.scriptPath;

  m_scriptEditor->openScript(event.scriptPath);
  m_scriptEditor->show();
  m_scriptEditor->raise();
  m_scriptEditor->setFocus();
}

void WorkflowMediator::onEditDialogueFlowRequested(
    const events::EditDialogueFlowRequestedEvent& event) {
  if (!m_dialogueGraph) {
    return;
  }

  qDebug() << "[WorkflowMediator] Edit dialogue flow requested:" << event.sceneId;

  m_dialogueGraph->loadSceneDialogue(event.sceneId);
  m_dialogueGraph->show();
  m_dialogueGraph->raise();

  events::StatusMessageEvent statusEvent;
  statusEvent.message = QObject::tr("Editing dialogue flow: %1").arg(event.sceneId);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onReturnToStoryGraphRequested(
    const events::ReturnToStoryGraphRequestedEvent&) {
  if (!m_storyGraph) {
    return;
  }

  qDebug() << "[WorkflowMediator] Returning to story graph";

  m_storyGraph->show();
  m_storyGraph->raise();

  events::StatusMessageEvent statusEvent;
  statusEvent.message = QObject::tr("Returned to Story Graph");
  statusEvent.timeoutMs = 2000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onDialogueCountChanged(const events::DialogueCountChangedEvent& event) {
  qDebug() << "[WorkflowMediator] Dialogue count changed for scene:" << event.sceneId
           << "count:" << event.count;

  if (m_storyGraph) {
    auto* node = m_storyGraph->findNodeByIdString(event.sceneId);
    if (node) {
      node->setDialogueCount(event.count);
      node->setHasEmbeddedDialogue(event.count > 0);
      qDebug() << "[WorkflowMediator] Updated dialogue count badge for scene:" << event.sceneId;
    }
  }
}

void WorkflowMediator::onOpenSceneScriptRequested(
    const events::OpenSceneScriptRequestedEvent& event) {
  if (!m_scriptEditor) {
    return;
  }

  qDebug() << "[WorkflowMediator] Open scene script requested for scene:" << event.sceneId
           << "script:" << event.scriptPath;

  m_scriptEditor->openScript(event.scriptPath);
  m_scriptEditor->show();
  m_scriptEditor->raise();

  events::StatusMessageEvent statusEvent;
  statusEvent.message = QObject::tr("Opened script: %1").arg(event.scriptPath);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onGoToScriptLocation(const events::GoToScriptLocationEvent& event) {
  if (!m_scriptEditor) {
    return;
  }

  qDebug() << "[WorkflowMediator] Go to script location:" << event.filePath
           << "line:" << event.line;

  m_scriptEditor->goToLocation(event.filePath, event.line);
  m_scriptEditor->show();
  m_scriptEditor->raise();
}

void WorkflowMediator::onNavigateToScriptDefinition(
    const events::NavigateToScriptDefinitionEvent& event) {
  // Issue #239: Bidirectional navigation from Story Graph to Script Editor
  // Find the scene definition in script files and navigate to that line
  if (!m_scriptEditor) {
    return;
  }

  qDebug() << "[WorkflowMediator] Navigate to script definition for scene:" << event.sceneId
           << "script:" << event.scriptPath;

  // If line is already known, navigate directly
  if (event.line > 0 && !event.scriptPath.isEmpty()) {
    m_scriptEditor->goToLocation(event.scriptPath, event.line);
    m_scriptEditor->show();
    m_scriptEditor->raise();
    return;
  }

  // Otherwise, find the scene definition in the script
  QString scriptPath = event.scriptPath;
  int lineNumber = -1;

  // If script path is provided, search in that file
  if (!scriptPath.isEmpty()) {
    QString absolutePath = scriptPath;
    if (!QFileInfo(scriptPath).isAbsolute()) {
      absolutePath = QString::fromStdString(
          ProjectManager::instance().toAbsolutePath(scriptPath.toStdString()));
    }

    QFile file(absolutePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&file);
      int currentLine = 1;
      const QRegularExpression sceneRe(
          QString("\\bscene\\s+%1\\b").arg(QRegularExpression::escape(event.sceneId)));

      while (!in.atEnd()) {
        QString line = in.readLine();
        if (sceneRe.match(line).hasMatch()) {
          lineNumber = currentLine;
          break;
        }
        currentLine++;
      }
      file.close();
    }
  } else {
    // Search all script files for the scene definition
    const QString scriptsRoot =
        QString::fromStdString(ProjectManager::instance().getFolderPath(ProjectFolder::Scripts));

    if (!scriptsRoot.isEmpty()) {
      namespace fs = std::filesystem;
      fs::path base(scriptsRoot.toStdString());

      if (fs::exists(base)) {
        const QRegularExpression sceneRe(
            QString("\\bscene\\s+%1\\b").arg(QRegularExpression::escape(event.sceneId)));

        for (const auto& entry : fs::recursive_directory_iterator(base)) {
          if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
            continue;
          }

          QFile file(QString::fromStdString(entry.path().string()));
          if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
          }

          QTextStream in(&file);
          int currentLine = 1;

          while (!in.atEnd()) {
            QString line = in.readLine();
            if (sceneRe.match(line).hasMatch()) {
              scriptPath = QString::fromStdString(
                  ProjectManager::instance().toRelativePath(entry.path().string()));
              lineNumber = currentLine;
              break;
            }
            currentLine++;
          }
          file.close();

          if (lineNumber > 0) {
            break; // Found the scene definition
          }
        }
      }
    }
  }

  if (lineNumber > 0 && !scriptPath.isEmpty()) {
    m_scriptEditor->goToLocation(scriptPath, lineNumber);
    m_scriptEditor->show();
    m_scriptEditor->raise();

    events::StatusMessageEvent statusEvent;
    statusEvent.message =
        QObject::tr("Navigated to scene '%1' at line %2").arg(event.sceneId).arg(lineNumber);
    statusEvent.timeoutMs = 3000;
    EventBus::instance().publish(statusEvent);
  } else {
    // Scene not found in any script
    events::StatusMessageEvent statusEvent;
    statusEvent.message = QObject::tr("Scene '%1' not found in script files").arg(event.sceneId);
    statusEvent.timeoutMs = 5000;
    EventBus::instance().publish(statusEvent);

    qWarning() << "[WorkflowMediator] Scene definition not found:" << event.sceneId;
  }
}

void WorkflowMediator::onVoiceClipAssignRequested(
    const events::VoiceClipAssignRequestedEvent& event) {
  if (!m_voiceStudio) {
    return;
  }

  qDebug() << "[WorkflowMediator] Voice clip assign requested for node:" << event.nodeIdString
           << "current:" << event.currentPath;

  m_voiceStudio->show();
  m_voiceStudio->raise();

  events::StatusMessageEvent statusEvent;
  statusEvent.message =
      QObject::tr("Assign voice clip for dialogue node: %1").arg(event.nodeIdString);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onVoiceAutoDetectRequested(
    const events::VoiceAutoDetectRequestedEvent& event) {
  qDebug() << "[WorkflowMediator] Voice auto-detect requested for node:" << event.nodeIdString
           << "key:" << event.localizationKey;

  events::StatusMessageEvent statusEvent;
  statusEvent.message = QObject::tr("Auto-detecting voice for key: %1").arg(event.localizationKey);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);

  if (m_voiceManager) {
    m_voiceManager->show();
    m_voiceManager->raise();
  }
}

void WorkflowMediator::onVoiceClipPreviewRequested(
    const events::VoiceClipPreviewRequestedEvent& event) {
  qDebug() << "[WorkflowMediator] Voice preview requested for node:" << event.nodeIdString
           << "voice:" << event.voicePath;

  events::StatusMessageEvent statusEvent;
  statusEvent.message = QObject::tr("Previewing voice: %1").arg(event.voicePath);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);

  if (m_voiceStudio) {
    m_voiceStudio->show();
    m_voiceStudio->raise();
  }
}

void WorkflowMediator::onVoiceRecordingRequested(
    const events::VoiceRecordingRequestedEvent& event) {
  if (!m_voiceStudio) {
    return;
  }

  qDebug() << "[WorkflowMediator] Voice recording requested for node:" << event.nodeIdString
           << "speaker:" << event.speaker << "text:" << event.dialogueText;

  m_voiceStudio->show();
  m_voiceStudio->raise();

  events::StatusMessageEvent statusEvent;
  statusEvent.message =
      QObject::tr("Recording voice for: %1 - %2").arg(event.speaker, event.dialogueText);
  statusEvent.timeoutMs = 3000;
  EventBus::instance().publish(statusEvent);
}

void WorkflowMediator::onIssueActivated(const events::IssueActivatedEvent& event) {
  if (!m_scriptEditor) {
    return;
  }

  qDebug() << "[WorkflowMediator] Issue activated:" << event.file << "line:" << event.line;

  m_scriptEditor->goToLocation(event.file, event.line);
  m_scriptEditor->show();
  m_scriptEditor->raise();
}

void WorkflowMediator::onDiagnosticActivated(const events::DiagnosticActivatedEvent& event) {
  qDebug() << "[WorkflowMediator] Diagnostic activated:" << event.location;

  // Re-publish as navigation request for unified handling
  events::NavigationRequestedEvent navEvent;
  navEvent.locationString = event.location;
  EventBus::instance().publish(navEvent);
}

void WorkflowMediator::onNavigationRequested(const events::NavigationRequestedEvent& event) {
  if (event.locationString.isEmpty()) {
    qWarning() << "[WorkflowMediator] Empty location string";
    return;
  }

  QStringList parts = event.locationString.split(':');
  if (parts.size() < 2) {
    qWarning() << "[WorkflowMediator] Invalid location format:" << event.locationString;
    return;
  }

  QString typeStr = parts[0].trimmed();

  // Navigate to StoryGraph node
  if (typeStr.compare("StoryGraph", Qt::CaseInsensitive) == 0) {
    if (!m_storyGraph) {
      qWarning() << "[WorkflowMediator] StoryGraph panel not available";
      return;
    }

    QString nodeId = parts[1].trimmed();
    if (nodeId.isEmpty()) {
      qWarning() << "[WorkflowMediator] Empty node ID";
      return;
    }

    qDebug() << "[WorkflowMediator] Navigating to StoryGraph node:" << nodeId;
    if (!m_storyGraph->navigateToNode(nodeId)) {
      qWarning() << "[WorkflowMediator] Failed to navigate to node:" << nodeId;
      if (m_diagnostics) {
        m_diagnostics->addDiagnosticWithLocation(
            "Warning", QObject::tr("Could not find node '%1'").arg(nodeId), event.locationString);
      }
    }
    return;
  }

  // Navigate to Script file
  if (typeStr.compare("Script", Qt::CaseInsensitive) == 0) {
    if (!m_scriptEditor) {
      qWarning() << "[WorkflowMediator] Script editor panel not available";
      return;
    }

    QString filePath = parts[1].trimmed();
    if (filePath.isEmpty()) {
      qWarning() << "[WorkflowMediator] Empty file path";
      return;
    }

    int lineNumber = -1;
    if (parts.size() >= 3) {
      bool ok = false;
      lineNumber = parts[2].trimmed().toInt(&ok);
      if (!ok || lineNumber < 1) {
        lineNumber = -1;
      }
    }

    qDebug() << "[WorkflowMediator] Navigating to Script:" << filePath << "line:" << lineNumber;
    m_scriptEditor->goToLocation(filePath, lineNumber);
    m_scriptEditor->show();
    m_scriptEditor->raise();
    return;
  }

  // Navigate to Asset
  if (typeStr.compare("Asset", Qt::CaseInsensitive) == 0 ||
      typeStr.compare("File", Qt::CaseInsensitive) == 0) {
    QString assetPath = parts[1].trimmed();
    qDebug() << "[WorkflowMediator] Asset/File navigation:" << assetPath;
    // Could open asset browser and select the asset
    return;
  }

  qWarning() << "[WorkflowMediator] Unknown location type:" << typeStr;
}

void WorkflowMediator::onAssetDoubleClicked(const events::AssetDoubleClickedEvent& event) {
  qDebug() << "[WorkflowMediator] Asset double-clicked:" << event.path;

  // Open scripts in script editor
  if (event.path.endsWith(".nms") && m_scriptEditor) {
    m_scriptEditor->openScript(event.path);
    m_scriptEditor->show();
    m_scriptEditor->raise();
    return;
  }

  // For images, add to scene view
  QFileInfo info(event.path);
  const QString ext = info.suffix().toLower();
  const bool isImage =
      (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif");

  if (isImage && m_sceneView) {
    if (auto* scene = m_sceneView->graphicsScene()) {
      if (auto* selected = scene->selectedObject()) {
        m_sceneView->setObjectAsset(selected->id(), event.path);
        m_sceneView->selectObjectById(selected->id());
        return;
      }
    }
    m_sceneView->addObjectFromAsset(event.path, QPointF(0, 0));
  }
}

void WorkflowMediator::onAssetsDropped(const events::AssetsDroppedEvent& event) {
  if (!m_sceneView || event.paths.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Assets dropped:" << event.paths.size()
           << "items, typeHint:" << event.typeHint;

  QPointF basePos(0, 0);
  if (auto* view = m_sceneView->graphicsView()) {
    basePos = view->mapToScene(view->viewport()->rect().center());
  }

  QPointF pos = basePos;
  const QPointF offset(32.0, 32.0);

  for (const QString& path : event.paths) {
    if (event.typeHint < 0) {
      m_sceneView->addObjectFromAsset(path, pos);
    } else {
      m_sceneView->addObjectFromAsset(path, pos,
                                      static_cast<qt::NMSceneObjectType>(event.typeHint));
    }
    pos += offset;
  }
}

void WorkflowMediator::onLoadSceneDocumentRequested(
    const events::LoadSceneDocumentRequestedEvent& event) {
  if (!m_sceneView || event.sceneId.isEmpty()) {
    return;
  }

  auto& playController = qt::NMPlayModeController::instance();
  if (!playController.isPlaying() && !playController.isPaused()) {
    qDebug() << "[WorkflowMediator] Loading scene document:" << event.sceneId;
    m_sceneView->loadSceneDocument(event.sceneId);
  }
}

// ============================================================================
// Scene Auto-Sync Event Handlers (Issue #213)
// ============================================================================

void WorkflowMediator::onSceneDocumentModified(const events::SceneDocumentModifiedEvent& event) {
  if (event.sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Scene document modified:" << event.sceneId
           << "- triggering thumbnail regeneration";

  // Note: Thumbnail regeneration will be handled by SceneRegistry
  // which will emit sceneThumbnailUpdated signal, which in turn
  // will trigger onSceneThumbnailUpdated() below to update Story Graph
}

void WorkflowMediator::onSceneThumbnailUpdated(const events::SceneThumbnailUpdatedEvent& event) {
  if (!m_storyGraph || event.sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Scene thumbnail updated:" << event.sceneId
           << "- updating Story Graph nodes";

  // Request Story Graph to update all nodes referencing this scene
  // This will be handled by the Story Graph panel's own event subscription
  // The Story Graph panel will update thumbnails for all scene nodes with this sceneId
}

void WorkflowMediator::onSceneRenamed(const events::SceneRenamedEvent& event) {
  if (!m_storyGraph || event.sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Scene renamed:" << event.sceneId << "(" << event.oldName << "->"
           << event.newName << ")";

  // The Story Graph panel will handle updating node display names
  // via its own event subscription to SceneRenamedEvent
}

void WorkflowMediator::onSceneDeleted(const events::SceneDeletedEvent& event) {
  if (!m_storyGraph || event.sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[WorkflowMediator] Scene deleted:" << event.sceneId
           << "- Story Graph should validate references";

  // The Story Graph panel will handle validation of orphaned references
  // via its own event subscription to SceneDeletedEvent
  // It will show warnings on nodes with broken scene references
}

} // namespace NovelMind::editor::mediators
