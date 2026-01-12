#include "NovelMind/editor/qt/panels/inspector_property_editor.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/widgets/nm_scene_id_picker.hpp"
#include "NovelMind/editor/scene_registry.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

InspectorPropertyEditor::InspectorPropertyEditor(NMInspectorPanel* panel) : m_panel(panel) {}

void InspectorPropertyEditor::createSceneObjectProperties(NMSceneObject* object, bool editable,
                                                          SceneRegistry* registry) {
  if (!object) {
    return;
  }

  const QString typeName = [object]() {
    switch (object->objectType()) {
    case NMSceneObjectType::Background:
      return QString("Background");
    case NMSceneObjectType::Character:
      return QString("Character");
    case NMSceneObjectType::UI:
      return QString("UI");
    case NMSceneObjectType::Effect:
      return QString("Effect");
    }
    return QString("Scene Object");
  }();

  // Create property groups
  createGeneralGroup(object, editable);

  QPointF lastScale(object->scaleX(), object->scaleY());
  bool lockAspectRatio = false;
  createTransformGroup(object, editable, lastScale, lockAspectRatio);

  createRenderingGroup(object, editable);
  createTagsGroup(object, editable);
}

void InspectorPropertyEditor::createGeneralGroup(NMSceneObject* object, bool editable) {
  auto* generalGroup = m_panel->addGroup(QObject::tr("General"));
  generalGroup->addProperty(QObject::tr("ID"), object->id());

  if (editable) {
    if (auto* nameEdit = generalGroup->addEditableProperty(
            "name", QObject::tr("Name"), NMPropertyType::String, object->name())) {
      // Track property widget - needs to be accessible via m_panel
    }
    if (auto* assetEdit = generalGroup->addEditableProperty(
            "asset", QObject::tr("Asset"), NMPropertyType::Asset, object->assetPath())) {
      // Track property widget
    }
  } else {
    generalGroup->addProperty(QObject::tr("Name"), object->name());
    generalGroup->addProperty(QObject::tr("Asset"), object->assetPath());
  }
}

void InspectorPropertyEditor::createTransformGroup(NMSceneObject* object, bool editable,
                                                   QPointF& lastScale, bool& lockAspectRatio) {
  auto* transformGroup = m_panel->addGroup(QObject::tr("Transform"));
  const QPointF pos = object->pos();

  // Store last scale for aspect ratio lock
  lastScale = QPointF(object->scaleX(), object->scaleY());

  if (editable) {
    // Position controls
    if (auto* xEdit =
            transformGroup->addEditableProperty("position_x", QObject::tr("Position X"),
                                                NMPropertyType::Float, QString::number(pos.x()))) {
      // Configure spinbox for position
      if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(xEdit)) {
        spinBox->setRange(-10000.0, 10000.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setToolTip(QObject::tr("X position in pixels"));
      }
    }
    if (auto* yEdit =
            transformGroup->addEditableProperty("position_y", QObject::tr("Position Y"),
                                                NMPropertyType::Float, QString::number(pos.y()))) {
      if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(yEdit)) {
        spinBox->setRange(-10000.0, 10000.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setToolTip(QObject::tr("Y position in pixels"));
      }
    }

    // Add reset position button
    transformGroup->addResetButton("reset_position", QVariant("0,0"));

    // Rotation control
    if (auto* rotEdit = transformGroup->addEditableProperty("rotation", QObject::tr("Rotation"),
                                                            NMPropertyType::Float,
                                                            QString::number(object->rotation()))) {
      if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(rotEdit)) {
        spinBox->setRange(-360.0, 360.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setSuffix(QString::fromUtf8("°"));
        spinBox->setWrapping(true);
        spinBox->setToolTip(QObject::tr("Rotation in degrees"));
      }
    }

    // Add reset rotation button
    transformGroup->addResetButton("reset_rotation", QVariant("0"));

    // Scale controls
    if (auto* sxEdit = transformGroup->addEditableProperty("scale_x", QObject::tr("Scale X"),
                                                           NMPropertyType::Float,
                                                           QString::number(object->scaleX()))) {
      if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(sxEdit)) {
        spinBox->setRange(0.01, 100.0);
        spinBox->setSingleStep(0.1);
        spinBox->setDecimals(2);
        spinBox->setToolTip(QObject::tr("Scale on X axis (1.0 = original size)"));
      }
    }
    if (auto* syEdit = transformGroup->addEditableProperty("scale_y", QObject::tr("Scale Y"),
                                                           NMPropertyType::Float,
                                                           QString::number(object->scaleY()))) {
      if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(syEdit)) {
        spinBox->setRange(0.01, 100.0);
        spinBox->setSingleStep(0.1);
        spinBox->setDecimals(2);
        spinBox->setToolTip(QObject::tr("Scale on Y axis (1.0 = original size)"));
      }
    }

    // Add lock aspect ratio checkbox
    auto* lockAspectRatioCheck = new QCheckBox(QObject::tr("Lock Aspect Ratio"));
    lockAspectRatioCheck->setChecked(lockAspectRatio);
    lockAspectRatioCheck->setToolTip(
        QObject::tr("When enabled, changing one scale value will\nproportionally adjust "
                    "the other"));

    // Add checkbox to group
    auto* lockRow = new QWidget();
    auto* lockLayout = new QHBoxLayout(lockRow);
    lockLayout->setContentsMargins(100 + 8, 0, 0, 0); // Align with other controls
    lockLayout->setSpacing(8);
    lockLayout->addWidget(lockAspectRatioCheck);
    lockLayout->addStretch();
    transformGroup->addProperty(QString(), lockRow);

    // Add reset scale button
    transformGroup->addResetButton("reset_scale", QVariant("1,1"));
  } else {
    transformGroup->addProperty(QObject::tr("Position X"), QString::number(pos.x()));
    transformGroup->addProperty(QObject::tr("Position Y"), QString::number(pos.y()));
    transformGroup->addProperty(QObject::tr("Rotation"), QString::number(object->rotation()));
    transformGroup->addProperty(QObject::tr("Scale X"), QString::number(object->scaleX()));
    transformGroup->addProperty(QObject::tr("Scale Y"), QString::number(object->scaleY()));
  }
}

void InspectorPropertyEditor::createRenderingGroup(NMSceneObject* object, bool editable) {
  auto* renderGroup = m_panel->addGroup(QObject::tr("Rendering"));
  if (editable) {
    if (auto* visibleEdit = renderGroup->addEditableProperty(
            "visible", QObject::tr("Visible"), NMPropertyType::Boolean,
            object->isVisible() ? "true" : "false")) {
      // Track property widget
    }
    if (auto* alphaEdit =
            renderGroup->addEditableProperty("alpha", QObject::tr("Alpha"), NMPropertyType::Float,
                                             QString::number(object->opacity()))) {
      // Track property widget
    }
    if (auto* zEdit =
            renderGroup->addEditableProperty("z", QObject::tr("Z-Order"), NMPropertyType::Integer,
                                             QString::number(object->zValue()))) {
      // Track property widget
    }
    if (auto* lockEdit = renderGroup->addEditableProperty("locked", QObject::tr("Locked"),
                                                          NMPropertyType::Boolean,
                                                          object->isLocked() ? "true" : "false")) {
      // Track property widget
    }
  } else {
    renderGroup->addProperty(QObject::tr("Visible"), object->isVisible() ? "true" : "false");
    renderGroup->addProperty(QObject::tr("Alpha"), QString::number(object->opacity()));
    renderGroup->addProperty(QObject::tr("Z-Order"), QString::number(object->zValue()));
    renderGroup->addProperty(QObject::tr("Locked"), object->isLocked() ? "true" : "false");
  }
}

void InspectorPropertyEditor::createTagsGroup(NMSceneObject* object, bool editable) {
  auto* tagsGroup = m_panel->addGroup(QObject::tr("Tags"));
  if (editable) {
    // Create a widget for tag editing with add/remove functionality
    auto* tagsWidget = new QWidget();
    auto* tagsLayout = new QVBoxLayout(tagsWidget);
    tagsLayout->setContentsMargins(0, 0, 0, 0);

    // Display current tags
    const QStringList tags = object->tags();
    auto* tagsListLabel =
        new QLabel(tags.isEmpty() ? QObject::tr("(no tags)") : tags.join(", "), tagsWidget);
    tagsListLabel->setWordWrap(true);
    tagsLayout->addWidget(tagsListLabel);

    // Add tag input
    auto* addTagLayout = new QHBoxLayout();
    auto* tagInput = new QLineEdit(tagsWidget);
    tagInput->setPlaceholderText(QObject::tr("Add tag..."));
    auto* addButton = new QPushButton(QObject::tr("Add"), tagsWidget);
    addTagLayout->addWidget(tagInput);
    addTagLayout->addWidget(addButton);
    tagsLayout->addLayout(addTagLayout);

    // Remove tag buttons for each existing tag
    for (const QString& tag : tags) {
      auto* tagLayout = new QHBoxLayout();
      auto* tagLabel = new QLabel(tag, tagsWidget);
      auto* removeButton = new QPushButton(QObject::tr("Remove"), tagsWidget);
      removeButton->setMaximumWidth(80);
      tagLayout->addWidget(tagLabel);
      tagLayout->addWidget(removeButton);
      tagsLayout->addLayout(tagLayout);
    }

    tagsGroup->addProperty(QObject::tr("Tag Editor"), tagsWidget);
  } else {
    const QStringList tags = object->tags();
    tagsGroup->addProperty(QObject::tr("Tags"),
                           tags.isEmpty() ? QObject::tr("(no tags)") : tags.join(", "));
  }
}

void InspectorPropertyEditor::createStoryGraphNodeProperties(NMGraphNodeItem* node, bool editable,
                                                             SceneRegistry* registry) {
  if (!node) {
    return;
  }

  createNodeGeneralGroup(node, editable);
  createNodeContentGroup(node, editable);
  createNodeBranchMappingGroup(node, editable);
  createNodeScriptGroup(node, editable);
  createNodeSceneGroup(node, editable, registry);
  createNodeConditionGroup(node, editable);
}

void InspectorPropertyEditor::createNodeGeneralGroup(NMGraphNodeItem* node, bool editable) {
  auto* generalGroup = m_panel->addGroup(QObject::tr("General"));
  generalGroup->addProperty(QObject::tr("ID"), node->nodeIdString());
  if (editable) {
    if (auto* titleEdit = generalGroup->addEditableProperty(
            "title", QObject::tr("Title"), NMPropertyType::String, node->title())) {
      // Track property widget
    }
    if (auto* typeEdit = generalGroup->addEditableProperty(
            "type", QObject::tr("Type"), NMPropertyType::String, node->nodeType())) {
      // Track property widget
    }
  } else {
    generalGroup->addProperty(QObject::tr("Title"), node->title());
    generalGroup->addProperty(QObject::tr("Type"), node->nodeType());
  }
}

void InspectorPropertyEditor::createNodeContentGroup(NMGraphNodeItem* node, bool editable) {
  if (!node->nodeType().contains("Dialogue", Qt::CaseInsensitive) &&
      !node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
    return;
  }

  auto* contentGroup = m_panel->addGroup(QObject::tr("Content"));
  const QString speakerValue = node->dialogueSpeaker();
  const QString textValue = node->dialogueText();
  const QString choicesValue = node->choiceOptions().join("\n");

  if (editable) {
    if (auto* speakerEdit = contentGroup->addEditableProperty(
            "speaker", QObject::tr("Speaker"), NMPropertyType::String, speakerValue)) {
      // Track property widget
    }
    if (auto* textEdit = contentGroup->addEditableProperty("text", QObject::tr("Text"),
                                                           NMPropertyType::MultiLine, textValue)) {
      // Track property widget
    }
    if (node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
      if (auto* choicesEdit = contentGroup->addEditableProperty(
              "choices", QObject::tr("Choices"), NMPropertyType::MultiLine, choicesValue)) {
        // Track property widget
      }
    }
  } else {
    contentGroup->addProperty(QObject::tr("Speaker"),
                              speakerValue.isEmpty() ? QObject::tr("Narrator") : speakerValue);
    contentGroup->addProperty(QObject::tr("Text"), textValue);
    if (node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
      contentGroup->addProperty(QObject::tr("Choices"), choicesValue);
    }
  }
}

void InspectorPropertyEditor::createNodeBranchMappingGroup(NMGraphNodeItem* node, bool editable) {
  // Choice node branching UI - shows which choice option leads to which target
  if (!node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
    return;
  }

  auto* branchGroup = m_panel->addGroup(QObject::tr("Branch Mapping"));

  const QStringList choiceOptions = node->choiceOptions();
  const QHash<QString, QString> choiceTargets = node->choiceTargets();

  if (choiceOptions.isEmpty()) {
    branchGroup->addProperty(QObject::tr("Info"),
                             QObject::tr("Add choices above to configure branching"));
  } else {
    // Build a display string showing choice → target mappings
    QString mappingDisplay;
    for (int i = 0; i < choiceOptions.size(); ++i) {
      const QString& option = choiceOptions[i];
      const QString target = choiceTargets.value(option);
      QString targetDisplay = target.isEmpty() ? QObject::tr("(not connected)") : target;
      // Truncate long option text for display
      QString optionDisplay = option;
      if (optionDisplay.length() > 25) {
        optionDisplay = optionDisplay.left(22) + "...";
      }
      mappingDisplay += QString("%1. %2 → %3").arg(i + 1).arg(optionDisplay).arg(targetDisplay);
      if (i < choiceOptions.size() - 1) {
        mappingDisplay += "\n";
      }
    }

    if (editable) {
      // Show the mapping as editable multiline (format:
      // "OptionText=TargetNodeId")
      QString editableMapping;
      for (const QString& option : choiceOptions) {
        const QString target = choiceTargets.value(option);
        editableMapping += option + "=" + target + "\n";
      }
      editableMapping = editableMapping.trimmed();

      if (auto* mappingEdit =
              branchGroup->addEditableProperty("choiceTargets", QObject::tr("Choice → Target"),
                                               NMPropertyType::MultiLine, editableMapping)) {
        // Track property widget
      }
    } else {
      branchGroup->addProperty(QObject::tr("Mapping"), mappingDisplay);
    }

    // Add helper text
    branchGroup->addProperty(QObject::tr("Help"),
                             QObject::tr("Connect edges from this node to target nodes.\n"
                                         "Each connection is automatically mapped to the next "
                                         "choice option."));
  }
}

void InspectorPropertyEditor::createNodeScriptGroup(NMGraphNodeItem* node, bool editable) {
  if (!node->nodeType().contains("Script", Qt::CaseInsensitive)) {
    return;
  }

  auto* scriptGroup = m_panel->addGroup(QObject::tr("Script"));
  scriptGroup->addProperty(QObject::tr("File"), node->scriptPath());
  if (editable) {
    if (auto* scriptEdit = scriptGroup->addEditableProperty(
            "scriptPath", QObject::tr("Path"), NMPropertyType::Asset, node->scriptPath())) {
      // Track property widget
    }
  }
}

void InspectorPropertyEditor::createNodeSceneGroup(NMGraphNodeItem* node, bool editable,
                                                   SceneRegistry* registry) {
  // Scene node handling - provides Scene ID picker with quick actions
  if (!node->isSceneNode()) {
    return;
  }

  auto* sceneGroup = m_panel->addGroup(QObject::tr("Scene Properties"));

  if (editable && registry) {
    // Use the dedicated Scene ID picker widget
    auto* scenePicker = new NMSceneIdPicker(registry, m_panel);
    scenePicker->setSceneId(node->sceneId());

    // Track property widget
    sceneGroup->addProperty(QObject::tr("Scene ID"), scenePicker);
  } else {
    // Read-only mode - show scene ID as text
    const QString sceneId = node->sceneId();
    QString displayValue = sceneId.isEmpty() ? QObject::tr("(not set)") : sceneId;

    // Add validation indicator
    if (!sceneId.isEmpty() && registry && !registry->sceneExists(sceneId)) {
      displayValue += " ⚠ (not found)";
    }

    sceneGroup->addProperty(QObject::tr("Scene ID"), displayValue);
  }

  // Additional scene properties
  if (editable) {
    if (auto* embeddedEdit = sceneGroup->addEditableProperty(
            "hasEmbeddedDialogue", QObject::tr("Has Embedded Dialogue"), NMPropertyType::Boolean,
            node->hasEmbeddedDialogue() ? "true" : "false")) {
      // Track property widget
    }
  } else {
    sceneGroup->addProperty(QObject::tr("Has Embedded Dialogue"),
                            node->hasEmbeddedDialogue() ? QObject::tr("Yes") : QObject::tr("No"));
  }

  // Dialogue count (read-only informational property)
  sceneGroup->addProperty(QObject::tr("Dialogue Count"), QString::number(node->dialogueCount()));

  // Thumbnail path (usually managed by SceneRegistry, but can be shown)
  if (!node->thumbnailPath().isEmpty()) {
    sceneGroup->addProperty(QObject::tr("Thumbnail"), node->thumbnailPath());
  }
}

void InspectorPropertyEditor::createNodeConditionGroup(NMGraphNodeItem* node, bool editable) {
  // Condition node handling - provides UI for editing condition expression and
  // output branches
  if (!node->nodeType().contains("Condition", Qt::CaseInsensitive)) {
    return;
  }

  auto* conditionGroup = m_panel->addGroup(QObject::tr("Condition"));
  const QString expressionValue = node->conditionExpression();
  const QString outputsValue = node->conditionOutputs().join("\n");

  if (editable) {
    // Expression editor for entering condition logic
    if (auto* exprEdit =
            conditionGroup->addEditableProperty("conditionExpression", QObject::tr("Expression"),
                                                NMPropertyType::MultiLine, expressionValue)) {
      // Track property widget
    }

    // Output path labels (branch names like "true", "false", or custom)
    if (auto* outputsEdit = conditionGroup->addEditableProperty(
            "conditionOutputs", QObject::tr("Output Paths (one per line)"),
            NMPropertyType::MultiLine, outputsValue)) {
      // Track property widget
    }
  } else {
    conditionGroup->addProperty(QObject::tr("Expression"), expressionValue.isEmpty()
                                                               ? QObject::tr("(no expression)")
                                                               : expressionValue);
    conditionGroup->addProperty(QObject::tr("Outputs"),
                                outputsValue.isEmpty() ? QObject::tr("true, false") : outputsValue);
  }

  // Condition branch mapping UI - shows which output leads to which target
  auto* branchGroup = m_panel->addGroup(QObject::tr("Branch Mapping"));

  const QStringList conditionOutputs = node->conditionOutputs();
  const QHash<QString, QString> conditionTargets = node->conditionTargets();

  // Use default "true/false" if no outputs defined
  QStringList outputs =
      conditionOutputs.isEmpty() ? QStringList{"true", "false"} : conditionOutputs;

  // Build display showing output → target mappings
  QString mappingDisplay;
  for (int i = 0; i < outputs.size(); ++i) {
    const QString& output = outputs[i];
    const QString target = conditionTargets.value(output);
    QString targetDisplay = target.isEmpty() ? QObject::tr("(not connected)") : target;
    mappingDisplay += QString("%1 → %2").arg(output).arg(targetDisplay);
    if (i < outputs.size() - 1) {
      mappingDisplay += "\n";
    }
  }

  if (editable) {
    // Show the mapping as editable multiline (format:
    // "OutputLabel=TargetNodeId")
    QString editableMapping;
    for (const QString& output : outputs) {
      const QString target = conditionTargets.value(output);
      editableMapping += output + "=" + target + "\n";
    }
    editableMapping = editableMapping.trimmed();

    if (auto* mappingEdit =
            branchGroup->addEditableProperty("conditionTargets", QObject::tr("Output → Target"),
                                             NMPropertyType::MultiLine, editableMapping)) {
      // Track property widget
    }
  } else {
    branchGroup->addProperty(QObject::tr("Mapping"), mappingDisplay);
  }

  // Add helper text explaining the condition logic
  QString helpText = QObject::tr("Expression is evaluated at runtime.\n"
                                 "Connect edges from this node to target nodes.\n"
                                 "First connection = first output (e.g., 'true').");
  branchGroup->addProperty(QObject::tr("Help"), helpText);
}

void InspectorPropertyEditor::createMultiObjectProperties(const QList<NMSceneObject*>& objects,
                                                          bool editable) {
  // This is handled by InspectorComponentList
  // Left empty as it's delegated to the component list module
}

} // namespace NovelMind::editor::qt
