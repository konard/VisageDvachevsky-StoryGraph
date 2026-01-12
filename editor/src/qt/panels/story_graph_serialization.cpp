#include "story_graph_serialization.hpp"
#include "NovelMind/editor/error_reporter.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "nm_story_graph_panel_detail.hpp"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProgressDialog>
#include <QThread>

namespace NovelMind::editor::qt::serialization {

void SyncToScriptWorker::process() {
  SyncResult result;

  for (int i = 0; i < m_items.size(); ++i) {
    // Check for cancellation
    if (m_cancelled->load()) {
      result.syncErrors << tr("Operation cancelled by user");
      break;
    }

    const auto &item = m_items.at(i);

    // Perform the actual sync (file I/O in background thread)
    bool success = detail::updateSceneSayStatement(
        item.sceneId, item.scriptPath, item.speaker, item.dialogueText);

    if (success) {
      ++result.nodesSynced;
    } else {
      result.syncErrors << tr("Failed to sync node '%1' to '%2'")
                               .arg(item.sceneId, item.scriptPath);
    }

    // Report progress
    emit progressUpdated(i + 1, static_cast<int>(m_items.size()));
  }

  emit finished(result);
}

void syncGraphToScript(NMStoryGraphPanel *panel, QWidget *parent) {
  // Issue #82: Sync Story Graph data to NMScript files
  // Issue #96: Run sync asynchronously to prevent UI freeze
  // This ensures that visual edits made in the Story Graph are persisted
  // to the authoritative NMScript source files without blocking the UI.

  auto *scene = panel->graphScene();
  if (!scene) {
    return;
  }

  // Collect sync items from UI thread (fast operation)
  QList<SyncItem> syncItems;
  int nodesSkipped = 0;

  for (auto *item : scene->items()) {
    auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item);
    if (!node) {
      continue;
    }

    const QString sceneId = node->nodeIdString();
    const QString scriptPath = detail::resolveScriptPath(node);

    if (scriptPath.isEmpty()) {
      // Node has no associated script file - skip
      ++nodesSkipped;
      continue;
    }

    // Sync speaker and dialogue text to script
    const QString speaker = node->dialogueSpeaker();
    const QString dialogueText = node->dialogueText();

    // Only sync if we have meaningful content (not default placeholder)
    if (dialogueText.isEmpty() || dialogueText.trimmed() == "New scene") {
      ++nodesSkipped;
      continue;
    }

    syncItems.append({sceneId, scriptPath, speaker, dialogueText});
  }

  // If nothing to sync, show message immediately
  if (syncItems.isEmpty()) {
    QString message =
        QObject::tr("No nodes needed synchronization.\n"
                    "(%1 node(s) skipped - no script or empty content)")
            .arg(nodesSkipped);
    NMMessageDialog::showInfo(parent, QObject::tr("Sync Graph to Script"),
                              message);
    return;
  }

  // Create progress dialog (Issue #96: keep UI responsive)
  auto *progressDialog = new QProgressDialog(
      QObject::tr("Synchronizing nodes to scripts..."),
      QObject::tr("Cancel"), 0, static_cast<int>(syncItems.size()), parent);
  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setMinimumDuration(0); // Show immediately
  progressDialog->setValue(0);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);

  // Capture initial skip count for final message
  const int initialSkipped = nodesSkipped;

  // Create cancellation flag (shared with worker thread)
  auto cancelled = std::make_shared<std::atomic<bool>>(false);

  // Create worker thread
  auto *workerThread = new QThread(parent);
  auto *worker = new SyncToScriptWorker(syncItems, cancelled);
  worker->moveToThread(workerThread);

  // Connect cancel button to set cancellation flag
  QObject::connect(progressDialog, &QProgressDialog::canceled, parent,
                   [cancelled, progressDialog]() {
                     cancelled->store(true);
                     progressDialog->setLabelText(
                         QObject::tr("Cancelling..."));
                   });

  // Connect signals for progress updates
  QObject::connect(
      worker, &SyncToScriptWorker::progressUpdated, progressDialog,
      [progressDialog](int current, int total) {
        progressDialog->setValue(current);
        progressDialog->setLabelText(
            QObject::tr("Synchronizing node %1 of %2...").arg(current).arg(total));
      });

  // Connect finished signal to handle results
  QObject::connect(
      worker, &SyncToScriptWorker::finished, parent,
      [panel, progressDialog, workerThread, worker,
       initialSkipped](const SyncResult &result) {
        progressDialog->close();
        progressDialog->deleteLater();

        // Clean up worker and thread
        workerThread->quit();
        workerThread->wait();
        worker->deleteLater();
        workerThread->deleteLater();

        // Report results to user
        QString message;
        const int totalSkipped = initialSkipped + result.nodesSkipped;

        if (result.syncErrors.isEmpty()) {
          if (result.nodesSynced > 0) {
            message = QObject::tr("Successfully synchronized %1 node(s) to NMScript "
                             "files.\n"
                             "(%2 node(s) skipped - no script or empty content)")
                          .arg(result.nodesSynced)
                          .arg(totalSkipped);
            qDebug() << "[StoryGraph] Sync to Script:" << result.nodesSynced
                     << "synced," << totalSkipped << "skipped";
          } else {
            message = QObject::tr("No nodes needed synchronization.\n"
                             "(%1 node(s) skipped - no script or empty content)")
                          .arg(totalSkipped);
          }
        } else {
          message = QObject::tr("Synchronization completed with errors:\n\n%1\n\n"
                           "(%2 node(s) synced, %3 failed)")
                        .arg(result.syncErrors.join("\n"))
                        .arg(result.nodesSynced)
                        .arg(result.syncErrors.size());
          ErrorReporter::instance().reportWarning(message.toStdString());
        }

        // Show notification (non-blocking info dialog)
        NMMessageDialog::showInfo(panel, QObject::tr("Sync Graph to Script"),
                                  message);
      });

  // Connect thread started signal to start worker
  QObject::connect(workerThread, &QThread::started, worker,
                   &SyncToScriptWorker::process);

  // Start the worker thread
  workerThread->start();
}

void syncScriptToGraph(NMStoryGraphPanel *panel, QWidget *parent) {
  // Issue #127: Sync NMScript files to Story Graph
  // This parses .nms script files and creates/updates graph nodes

  auto *scene = panel->graphScene();
  if (!scene) {
    NMMessageDialog::showWarning(
        parent, QObject::tr("Sync Script to Graph"),
        QObject::tr("Story Graph scene is not initialized."));
    return;
  }

  // Get the scripts folder path
  const auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showWarning(
        parent, QObject::tr("Sync Script to Graph"),
        QObject::tr("No project is currently open."));
    return;
  }

  const QString scriptsPath =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Scripts));

  if (scriptsPath.isEmpty()) {
    NMMessageDialog::showWarning(
        parent, QObject::tr("Sync Script to Graph"),
        QObject::tr("Could not find scripts folder in project."));
    return;
  }

  // Check if scripts directory exists
  QDir scriptsDir(scriptsPath);
  if (!scriptsDir.exists()) {
    NMMessageDialog::showWarning(
        parent, QObject::tr("Sync Script to Graph"),
        QObject::tr("Scripts folder does not exist:\n%1").arg(scriptsPath));
    return;
  }

  // Find all .nms files recursively
  QStringList nmsFiles;
  QDirIterator it(scriptsPath, QStringList() << "*.nms",
                  QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    nmsFiles.append(it.next());
  }

  if (nmsFiles.isEmpty()) {
    NMMessageDialog::showInfo(
        parent, QObject::tr("Sync Script to Graph"),
        QObject::tr("No .nms script files found in:\n%1").arg(scriptsPath));
    return;
  }

  // Confirm with user before replacing nodes
  auto result = NMMessageDialog::showQuestion(
      parent, QObject::tr("Sync Script to Graph"),
      QObject::tr("This will parse %1 script file(s) and update the Story Graph.\n\n"
             "Existing graph content will be replaced.\n\n"
             "Do you want to continue?")
          .arg(nmsFiles.size()),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  // Parse all script files
  QList<detail::ParsedNode> allNodes;
  QList<QPair<QString, QString>> allEdges;
  QString entryPoint;
  int filesProcessed = 0;
  int parseErrors = 0;
  QStringList errorMessages;

  for (const QString &filePath : nmsFiles) {
    detail::ParseResult parseResult = detail::parseNMScriptFile(filePath);

    if (!parseResult.success) {
      ++parseErrors;
      errorMessages.append(QString("%1: %2").arg(
          QFileInfo(filePath).fileName(), parseResult.errorMessage));
      continue;
    }

    allNodes.append(parseResult.nodes);
    allEdges.append(parseResult.edges);

    // Use first file's entry point if not set
    if (entryPoint.isEmpty() && !parseResult.entryPoint.isEmpty()) {
      entryPoint = parseResult.entryPoint;
    }

    ++filesProcessed;
  }

  if (allNodes.isEmpty()) {
    QString message = QObject::tr("No scenes found in script files.");
    if (!errorMessages.isEmpty()) {
      message +=
          QObject::tr("\n\nParse errors:\n") + errorMessages.join("\n");
    }
    NMMessageDialog::showWarning(parent,
                                 QObject::tr("Sync Script to Graph"), message);
    return;
  }

  // Clear existing graph (Note: This is managed by the panel's m_isRebuilding
  // flag)
  scene->clearGraph();

  // Create nodes from parsed data
  QHash<QString, NMGraphNodeItem *> nodeMap;
  int col = 0;
  int row = 0;
  const qreal horizontalSpacing = 260.0;
  const qreal verticalSpacing = 140.0;

  for (const detail::ParsedNode &parsedNode : allNodes) {
    // Calculate default position (grid layout)
    const QPointF defaultPos(col * horizontalSpacing, row * verticalSpacing);
    ++col;
    if (col >= 4) {
      col = 0;
      ++row;
    }

    // Create node in the scene
    QString nodeType = parsedNode.type;
    if (nodeType.isEmpty()) {
      nodeType = "Scene";
    }

    NMGraphNodeItem *node =
        scene->addNode(parsedNode.id, nodeType, defaultPos, 0, parsedNode.id);

    if (!node) {
      continue;
    }

    // Set node properties
    node->setSceneId(parsedNode.id);

    if (!parsedNode.speaker.isEmpty()) {
      node->setDialogueSpeaker(parsedNode.speaker);
    }
    if (!parsedNode.text.isEmpty()) {
      node->setDialogueText(parsedNode.text);
    }
    if (!parsedNode.choices.isEmpty()) {
      node->setChoiceOptions(parsedNode.choices);

      // Build choice targets mapping
      QHash<QString, QString> choiceTargets;
      for (int i = 0;
           i < parsedNode.choices.size() && i < parsedNode.targets.size();
           ++i) {
        choiceTargets.insert(parsedNode.choices[i], parsedNode.targets[i]);
      }
      node->setChoiceTargets(choiceTargets);
    }
    if (!parsedNode.conditionExpr.isEmpty()) {
      node->setConditionExpression(parsedNode.conditionExpr);
      node->setConditionOutputs(parsedNode.conditionOutputs);
    }

    // Mark entry node
    if (parsedNode.id == entryPoint) {
      node->setEntry(true);
    }

    nodeMap.insert(parsedNode.id, node);
  }

  // Create connections from edges
  int connectionsCreated = 0;
  for (const auto &edge : allEdges) {
    NMGraphNodeItem *fromNode = nodeMap.value(edge.first);
    NMGraphNodeItem *toNode = nodeMap.value(edge.second);

    if (fromNode && toNode) {
      // Check if connection already exists
      if (!scene->hasConnection(fromNode->nodeId(), toNode->nodeId())) {
        scene->addConnection(fromNode, toNode);
        ++connectionsCreated;
      }
    }
  }

  // Fit view to content
  auto *view = panel->graphView();
  if (view && !scene->nodes().isEmpty()) {
    view->fitInView(scene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                    Qt::KeepAspectRatio);
  }

  // Show result
  QString message =
      QObject::tr("Successfully imported %1 node(s) with %2 connection(s) from %3 "
             "file(s).")
          .arg(allNodes.size())
          .arg(connectionsCreated)
          .arg(filesProcessed);

  if (parseErrors > 0) {
    message += QObject::tr("\n\n%1 file(s) had parse errors:\n%2")
                   .arg(parseErrors)
                   .arg(errorMessages.join("\n"));
  }

  qDebug() << "[StoryGraph] Sync Script to Graph completed:" << allNodes.size()
           << "nodes," << connectionsCreated << "connections from"
           << filesProcessed << "files";

  NMMessageDialog::showInfo(parent, QObject::tr("Sync Script to Graph"),
                            message);

  // Note: Entry scene and layout saving is handled by the panel
}

} // namespace NovelMind::editor::qt::serialization

// Include MOC for Q_OBJECT class defined in this file
#include "story_graph_serialization.moc"
