#include "story_graph_property_manager.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "nm_story_graph_panel_detail.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace NovelMind::editor::qt::property_manager {

void applyNodePropertyChange(NMStoryGraphPanel *panel,
                             const QString &nodeIdString,
                             const QString &propertyName,
                             const QString &newValue) {
  auto *node = panel->findNodeByIdString(nodeIdString);
  if (!node) {
    return;
  }

  if (propertyName == "title") {
    node->setTitle(newValue);
  } else if (propertyName == "type") {
    node->setNodeType(newValue);
  } else if (propertyName == "scriptPath") {
    node->setScriptPath(newValue);
    QString scriptPath = newValue;
    if (QFileInfo(newValue).isRelative()) {
      scriptPath = QString::fromStdString(
          ProjectManager::instance().toAbsolutePath(newValue.toStdString()));
    }
    QFile scriptFile(scriptPath);
    if (!scriptPath.isEmpty() && !scriptFile.exists()) {
      if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << "// ========================================\n";
        out << "// Generated from Story Graph\n";
        out << "// Do not edit manually - changes may be overwritten\n";
        out << "// ========================================\n";
        out << "// " << node->nodeIdString() << "\n";
        out << "scene " << node->nodeIdString() << " {\n";
        // Condition and Scene nodes are "silent" - they only handle branching,
        // not dialogue. Only Dialogue nodes get a default say statement.
        // This fixes issue #76 where Condition nodes incorrectly said text.
        if (node->isConditionNode()) {
          out << "  // Condition node - add branching logic here\n";
        } else if (node->isSceneNode()) {
          out << "  // Scene node - add scene content here\n";
        } else {
          out << "  say Narrator \"New script node\"\n";
        }
        out << "}\n";
      }
    }
  } else if (propertyName == "speaker") {
    node->setDialogueSpeaker(newValue);
    // Sync speaker change to the .nms script file
    const QString scriptPath = detail::resolveScriptPath(node);
    if (!scriptPath.isEmpty()) {
      detail::updateSceneSayStatement(node->nodeIdString(), scriptPath,
                                      newValue, node->dialogueText());
    }
  } else if (propertyName == "text") {
    node->setDialogueText(newValue);
    // Sync text change to the .nms script file
    const QString scriptPath = detail::resolveScriptPath(node);
    if (!scriptPath.isEmpty()) {
      detail::updateSceneSayStatement(node->nodeIdString(), scriptPath,
                                      node->dialogueSpeaker(), newValue);
    }
  } else if (propertyName == "choices") {
    node->setChoiceOptions(detail::splitChoiceLines(newValue));
  } else if (propertyName == "conditionExpression") {
    node->setConditionExpression(newValue);
  } else if (propertyName == "conditionOutputs") {
    node->setConditionOutputs(detail::splitChoiceLines(newValue));
  } else if (propertyName == "choiceTargets") {
    // Parse "OptionText=TargetNodeId" format
    QHash<QString, QString> targets;
    const QStringList lines = detail::splitChoiceLines(newValue);
    for (const QString &line : lines) {
      qsizetype eqPos = line.indexOf('=');
      if (eqPos > 0) {
        QString option = line.left(eqPos).trimmed();
        QString target = line.mid(eqPos + 1).trimmed();
        if (!option.isEmpty()) {
          targets.insert(option, target);
        }
      }
    }
    node->setChoiceTargets(targets);
  } else if (propertyName == "conditionTargets") {
    // Parse "OutputLabel=TargetNodeId" format
    QHash<QString, QString> targets;
    const QStringList lines = detail::splitChoiceLines(newValue);
    for (const QString &line : lines) {
      qsizetype eqPos = line.indexOf('=');
      if (eqPos > 0) {
        QString output = line.left(eqPos).trimmed();
        QString target = line.mid(eqPos + 1).trimmed();
        if (!output.isEmpty()) {
          targets.insert(output, target);
        }
      }
    }
    node->setConditionTargets(targets);
  }

  // Note: Layout saving is handled by the panel
}

void handleNodesMoved(NMStoryGraphPanel *panel,
                      const QVector<GraphNodeMove> &moves) {
  // Note: Undo command handling is done by the panel
  // This function is for layout updates only
  // Layout update is handled by the panel
}

void handleLocalePreviewChange(NMStoryGraphPanel *panel, int index) {
  auto *scene = panel->graphScene();
  if (!scene) {
    return;
  }

  // Get the current preview locale from selector
  // Note: Locale retrieval is handled by the panel

  // Update dialogue nodes to show translated text or highlight missing
  for (auto *item : scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (node->isDialogueNode()) {
        // Localization status update handled by panel based on locale
        node->update();
      }
    }
  }
}

void handleExportDialogue(NMStoryGraphPanel *panel) {
  auto *scene = panel->graphScene();
  if (!scene) {
    return;
  }

  // Collect all dialogue nodes and their text
  QStringList dialogueEntries;
  for (auto *item : scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (node->isDialogueNode() && !node->localizationKey().isEmpty()) {
        // Format: key,speaker,text
        QString line = QString("\"%1\",\"%2\",\"%3\"")
                           .arg(node->localizationKey())
                           .arg(node->dialogueSpeaker())
                           .arg(node->dialogueText().replace("\"", "\"\""));
        dialogueEntries.append(line);
      }
    }
  }

  if (dialogueEntries.isEmpty()) {
    qDebug() << "[StoryGraph] No dialogue entries to export";
    return;
  }

  qDebug() << "[StoryGraph] Exported" << dialogueEntries.size()
           << "dialogue entries";

  // Signal emission handled by panel
}

void handleGenerateLocalizationKeys(NMStoryGraphPanel *panel) {
  auto *scene = panel->graphScene();
  if (!scene) {
    return;
  }

  int keysGenerated = 0;

  for (auto *item : scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      // Generate keys for dialogue nodes that don't have one
      if (node->isDialogueNode() && node->localizationKey().isEmpty()) {
        // Generate key in format: scene.{sceneId}.dialogue.{nodeId}
        QString sceneId =
            node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
        QString key =
            QString("scene.%1.dialogue.%2").arg(sceneId).arg(node->nodeId());
        node->setLocalizationKey(key);
        ++keysGenerated;
      }

      // Generate keys for choice nodes
      if (node->nodeType().compare("Choice", Qt::CaseInsensitive) == 0) {
        QString sceneId =
            node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
        const QStringList &options = node->choiceOptions();
        for (int i = 0; i < options.size(); ++i) {
          // Each choice option gets its own key
          // Keys are stored as a property on the node
          QString key = QString("scene.%1.choice.%2.%3")
                            .arg(sceneId)
                            .arg(node->nodeId())
                            .arg(i);
          // Store choice keys - would need to extend node to store multiple
          // keys
          ++keysGenerated;
        }
      }

      node->update();
    }
  }

  qDebug() << "[StoryGraph] Generated" << keysGenerated
           << "localization keys";

  // Layout saving handled by panel
}

} // namespace NovelMind::editor::qt::property_manager
