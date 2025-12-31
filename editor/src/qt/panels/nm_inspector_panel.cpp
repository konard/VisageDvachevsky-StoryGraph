#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/core/property_system.hpp"
#include "NovelMind/editor/inspector_binding.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMInspectorPanel
// ============================================================================

NMInspectorPanel::NMInspectorPanel(QWidget *parent)
    : NMDockPanel(tr("Inspector"), parent) {
  setPanelId("Inspector");

  // Inspector needs adequate width for property labels and edit controls
  // and height to show at least a few property groups without excessive
  // scrolling
  setMinimumPanelSize(280, 200);

  setupContent();
}

NMInspectorPanel::~NMInspectorPanel() = default;

void NMInspectorPanel::onInitialize() { showNoSelection(); }

void NMInspectorPanel::onUpdate(double /*deltaTime*/) {
  // No continuous update needed
}

void NMInspectorPanel::clear() {
  // Remove all groups
  for (auto *group : m_groups) {
    m_mainLayout->removeWidget(group);
    delete group;
  }
  m_groups.clear();
  m_propertyWidgets.clear();

  m_headerLabel->clear();
}

void NMInspectorPanel::inspectObject(const QString &objectType,
                                     const QString &objectId, bool editable) {
  clear();
  m_noSelectionLabel->hide();
  m_currentObjectId = objectId;
  m_editMode = editable;

  // Set header
  m_headerLabel->setText(
      QString("<b>%1</b><br><span style='color: gray;'>%2</span>")
          .arg(objectType)
          .arg(objectId));
  m_headerLabel->show();

  // Add demo properties based on type
  auto *transformGroup = addGroup(tr("Transform"));

  if (m_editMode) {
    transformGroup->addEditableProperty(tr("Position X"), NMPropertyType::Float,
                                        "0.0");
    transformGroup->addEditableProperty(tr("Position Y"), NMPropertyType::Float,
                                        "0.0");
    transformGroup->addEditableProperty(tr("Rotation"), NMPropertyType::Float,
                                        "0.0");
    transformGroup->addEditableProperty(tr("Scale X"), NMPropertyType::Float,
                                        "1.0");
    transformGroup->addEditableProperty(tr("Scale Y"), NMPropertyType::Float,
                                        "1.0");
  } else {
    transformGroup->addProperty(tr("Position X"), "0.0");
    transformGroup->addProperty(tr("Position Y"), "0.0");
    transformGroup->addProperty(tr("Rotation"), "0.0");
    transformGroup->addProperty(tr("Scale X"), "1.0");
    transformGroup->addProperty(tr("Scale Y"), "1.0");
  }

  connect(transformGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  auto *renderGroup = addGroup(tr("Rendering"));

  if (m_editMode) {
    renderGroup->addEditableProperty(tr("Visible"), NMPropertyType::Boolean,
                                     "true");
    renderGroup->addEditableProperty(tr("Alpha"), NMPropertyType::Float, "1.0");
    renderGroup->addEditableProperty(tr("Z-Order"), NMPropertyType::Integer,
                                     "0");
    renderGroup->addEditableProperty(
        tr("Blend Mode"), NMPropertyType::Enum, "Normal",
        {"Normal", "Additive", "Multiply", "Screen", "Overlay"});
    renderGroup->addEditableProperty(tr("Tint Color"), NMPropertyType::Color,
                                     "#FFFFFF");
  } else {
    renderGroup->addProperty(tr("Visible"), "true");
    renderGroup->addProperty(tr("Alpha"), "1.0");
    renderGroup->addProperty(tr("Z-Order"), "0");
  }

  connect(renderGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  if (objectType == "Dialogue" || objectType == "Choice") {
    auto *dialogueGroup = addGroup(tr("Dialogue"));

    if (m_editMode) {
      dialogueGroup->addEditableProperty(tr("Speaker"), NMPropertyType::String,
                                         "Narrator");
      dialogueGroup->addEditableProperty(tr("Text"), NMPropertyType::String,
                                         objectId);
      dialogueGroup->addEditableProperty(tr("Voice Clip"),
                                         NMPropertyType::Asset, "");
    } else {
      dialogueGroup->addProperty(tr("Speaker"), "Narrator");
      dialogueGroup->addProperty(tr("Text"), objectId);
      dialogueGroup->addProperty(tr("Voice Clip"), "(none)");
    }

    connect(dialogueGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  // Add spacer at the end
  m_mainLayout->addStretch();
}

void NMInspectorPanel::inspectSceneObject(NMSceneObject *object,
                                          bool editable) {
  if (!object) {
    showNoSelection();
    return;
  }

  clear();
  m_noSelectionLabel->hide();
  m_multiEditMode = false;
  m_currentObjectId = object->id();
  m_currentObjectIds.clear();
  m_editMode = editable;

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

  m_headerLabel->setText(
      QString("<b>%1</b><br><span style='color: gray;'>%2</span>")
          .arg(typeName)
          .arg(object->id()));
  m_headerLabel->show();

  auto *generalGroup = addGroup(tr("General"));
  generalGroup->addProperty(tr("ID"), object->id());
  if (m_editMode) {
    if (auto *nameEdit = generalGroup->addEditableProperty(
            "name", tr("Name"), NMPropertyType::String, object->name())) {
      trackPropertyWidget("name", nameEdit);
    }
    if (auto *assetEdit = generalGroup->addEditableProperty(
            "asset", tr("Asset"), NMPropertyType::Asset, object->assetPath())) {
      trackPropertyWidget("asset", assetEdit);
    }
  } else {
    generalGroup->addProperty(tr("Name"), object->name());
    generalGroup->addProperty(tr("Asset"), object->assetPath());
  }
  connect(generalGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  auto *transformGroup = addGroup(tr("Transform"));
  const QPointF pos = object->pos();

  // Store last scale for aspect ratio lock
  m_lastScale = QPointF(object->scaleX(), object->scaleY());

  if (m_editMode) {
    // Position controls
    if (auto *xEdit = transformGroup->addEditableProperty(
            "position_x", tr("Position X"), NMPropertyType::Float,
            QString::number(pos.x()))) {
      trackPropertyWidget("position_x", xEdit);
      // Configure spinbox for position
      if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(xEdit)) {
        spinBox->setRange(-10000.0, 10000.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setToolTip(tr("X position in pixels"));
      }
    }
    if (auto *yEdit = transformGroup->addEditableProperty(
            "position_y", tr("Position Y"), NMPropertyType::Float,
            QString::number(pos.y()))) {
      trackPropertyWidget("position_y", yEdit);
      if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(yEdit)) {
        spinBox->setRange(-10000.0, 10000.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setToolTip(tr("Y position in pixels"));
      }
    }

    // Add reset position button
    transformGroup->addResetButton("reset_position", QVariant("0,0"));

    // Rotation control
    if (auto *rotEdit = transformGroup->addEditableProperty(
            "rotation", tr("Rotation"), NMPropertyType::Float,
            QString::number(object->rotation()))) {
      trackPropertyWidget("rotation", rotEdit);
      if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(rotEdit)) {
        spinBox->setRange(-360.0, 360.0);
        spinBox->setSingleStep(1.0);
        spinBox->setDecimals(1);
        spinBox->setSuffix(QString::fromUtf8("°"));
        spinBox->setWrapping(true);
        spinBox->setToolTip(tr("Rotation in degrees"));
      }
    }

    // Add reset rotation button
    transformGroup->addResetButton("reset_rotation", QVariant("0"));

    // Scale controls
    if (auto *sxEdit = transformGroup->addEditableProperty(
            "scale_x", tr("Scale X"), NMPropertyType::Float,
            QString::number(object->scaleX()))) {
      trackPropertyWidget("scale_x", sxEdit);
      if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(sxEdit)) {
        spinBox->setRange(0.01, 100.0);
        spinBox->setSingleStep(0.1);
        spinBox->setDecimals(2);
        spinBox->setToolTip(tr("Scale on X axis (1.0 = original size)"));
      }
    }
    if (auto *syEdit = transformGroup->addEditableProperty(
            "scale_y", tr("Scale Y"), NMPropertyType::Float,
            QString::number(object->scaleY()))) {
      trackPropertyWidget("scale_y", syEdit);
      if (auto *spinBox = qobject_cast<QDoubleSpinBox *>(syEdit)) {
        spinBox->setRange(0.01, 100.0);
        spinBox->setSingleStep(0.1);
        spinBox->setDecimals(2);
        spinBox->setToolTip(tr("Scale on Y axis (1.0 = original size)"));
      }
    }

    // Add lock aspect ratio checkbox
    auto *lockAspectRatioCheck = new QCheckBox(tr("Lock Aspect Ratio"));
    lockAspectRatioCheck->setChecked(m_lockAspectRatio);
    lockAspectRatioCheck->setToolTip(
        tr("When enabled, changing one scale value will\nproportionally adjust the other"));
    connect(lockAspectRatioCheck, &QCheckBox::toggled, this,
            [this](bool checked) { m_lockAspectRatio = checked; });
    trackPropertyWidget("lock_aspect_ratio", lockAspectRatioCheck);

    // Add checkbox to group
    auto *lockRow = new QWidget(m_scrollContent);
    auto *lockLayout = new QHBoxLayout(lockRow);
    lockLayout->setContentsMargins(100 + 8, 0, 0, 0); // Align with other controls
    lockLayout->setSpacing(8);
    lockLayout->addWidget(lockAspectRatioCheck);
    lockLayout->addStretch();
    transformGroup->addProperty(QString(), lockRow);

    // Add reset scale button
    transformGroup->addResetButton("reset_scale", QVariant("1,1"));
  } else {
    transformGroup->addProperty(tr("Position X"), QString::number(pos.x()));
    transformGroup->addProperty(tr("Position Y"), QString::number(pos.y()));
    transformGroup->addProperty(tr("Rotation"),
                                QString::number(object->rotation()));
    transformGroup->addProperty(tr("Scale X"),
                                QString::number(object->scaleX()));
    transformGroup->addProperty(tr("Scale Y"),
                                QString::number(object->scaleY()));
  }
  connect(transformGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  auto *renderGroup = addGroup(tr("Rendering"));
  if (m_editMode) {
    if (auto *visibleEdit = renderGroup->addEditableProperty(
            "visible", tr("Visible"), NMPropertyType::Boolean,
            object->isVisible() ? "true" : "false")) {
      trackPropertyWidget("visible", visibleEdit);
    }
    if (auto *alphaEdit = renderGroup->addEditableProperty(
            "alpha", tr("Alpha"), NMPropertyType::Float,
            QString::number(object->opacity()))) {
      trackPropertyWidget("alpha", alphaEdit);
    }
    if (auto *zEdit = renderGroup->addEditableProperty(
            "z", tr("Z-Order"), NMPropertyType::Integer,
            QString::number(object->zValue()))) {
      trackPropertyWidget("z", zEdit);
    }
    if (auto *lockEdit = renderGroup->addEditableProperty(
            "locked", tr("Locked"), NMPropertyType::Boolean,
            object->isLocked() ? "true" : "false")) {
      trackPropertyWidget("locked", lockEdit);
    }
  } else {
    renderGroup->addProperty(tr("Visible"),
                             object->isVisible() ? "true" : "false");
    renderGroup->addProperty(tr("Alpha"), QString::number(object->opacity()));
    renderGroup->addProperty(tr("Z-Order"), QString::number(object->zValue()));
    renderGroup->addProperty(tr("Locked"),
                             object->isLocked() ? "true" : "false");
  }
  connect(renderGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  // Tags group for tag editing
  auto *tagsGroup = addGroup(tr("Tags"));
  if (m_editMode) {
    // Create a widget for tag editing with add/remove functionality
    auto *tagsWidget = new QWidget(m_scrollContent);
    auto *tagsLayout = new QVBoxLayout(tagsWidget);
    tagsLayout->setContentsMargins(0, 0, 0, 0);

    // Display current tags
    const QStringList tags = object->tags();
    auto *tagsListLabel = new QLabel(
        tags.isEmpty() ? tr("(no tags)") : tags.join(", "), tagsWidget);
    tagsListLabel->setWordWrap(true);
    tagsLayout->addWidget(tagsListLabel);

    // Add tag input
    auto *addTagLayout = new QHBoxLayout();
    auto *tagInput = new QLineEdit(tagsWidget);
    tagInput->setPlaceholderText(tr("Add tag..."));
    auto *addButton = new QPushButton(tr("Add"), tagsWidget);
    addTagLayout->addWidget(tagInput);
    addTagLayout->addWidget(addButton);
    tagsLayout->addLayout(addTagLayout);

    // Connect add button
    connect(addButton, &QPushButton::clicked, [this, tagInput, object]() {
      const QString tag = tagInput->text().trimmed();
      if (!tag.isEmpty() && !object->hasTag(tag)) {
        object->addTag(tag);
        tagInput->clear();
        // Refresh inspector to show updated tags
        inspectSceneObject(object, m_editMode);
      }
    });

    // Remove tag buttons for each existing tag
    for (const QString &tag : tags) {
      auto *tagLayout = new QHBoxLayout();
      auto *tagLabel = new QLabel(tag, tagsWidget);
      auto *removeButton = new QPushButton(tr("Remove"), tagsWidget);
      removeButton->setMaximumWidth(80);
      tagLayout->addWidget(tagLabel);
      tagLayout->addWidget(removeButton);
      tagsLayout->addLayout(tagLayout);

      connect(removeButton, &QPushButton::clicked, [this, tag, object]() {
        object->removeTag(tag);
        // Refresh inspector to show updated tags
        inspectSceneObject(object, m_editMode);
      });
    }

    tagsGroup->addProperty(tr("Tag Editor"), tagsWidget);
  } else {
    const QStringList tags = object->tags();
    tagsGroup->addProperty(tr("Tags"),
                           tags.isEmpty() ? tr("(no tags)") : tags.join(", "));
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::inspectStoryGraphNode(NMGraphNodeItem *node,
                                             bool editable) {
  if (!node) {
    showNoSelection();
    return;
  }

  clear();
  m_noSelectionLabel->hide();
  m_multiEditMode = false;
  m_currentObjectId = node->nodeIdString();
  m_currentObjectIds.clear();
  m_editMode = editable;

  m_headerLabel->setText(
      QString("<b>%1</b><br><span style='color: gray;'>%2</span>")
          .arg(node->nodeType())
          .arg(node->nodeIdString()));
  m_headerLabel->show();

  auto *generalGroup = addGroup(tr("General"));
  generalGroup->addProperty(tr("ID"), node->nodeIdString());
  if (m_editMode) {
    if (auto *titleEdit = generalGroup->addEditableProperty(
            "title", tr("Title"), NMPropertyType::String, node->title())) {
      trackPropertyWidget("title", titleEdit);
    }
    if (auto *typeEdit = generalGroup->addEditableProperty(
            "type", tr("Type"), NMPropertyType::String, node->nodeType())) {
      trackPropertyWidget("type", typeEdit);
    }
  } else {
    generalGroup->addProperty(tr("Title"), node->title());
    generalGroup->addProperty(tr("Type"), node->nodeType());
  }
  connect(generalGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  if (node->nodeType().contains("Dialogue", Qt::CaseInsensitive) ||
      node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
    auto *contentGroup = addGroup(tr("Content"));
    const QString speakerValue = node->dialogueSpeaker();
    const QString textValue = node->dialogueText();
    const QString choicesValue = node->choiceOptions().join("\n");

    if (m_editMode) {
      if (auto *speakerEdit = contentGroup->addEditableProperty(
              "speaker", tr("Speaker"), NMPropertyType::String, speakerValue)) {
        trackPropertyWidget("speaker", speakerEdit);
      }
      if (auto *textEdit = contentGroup->addEditableProperty(
              "text", tr("Text"), NMPropertyType::MultiLine, textValue)) {
        trackPropertyWidget("text", textEdit);
      }
      if (node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
        if (auto *choicesEdit = contentGroup->addEditableProperty(
                "choices", tr("Choices"), NMPropertyType::MultiLine,
                choicesValue)) {
          trackPropertyWidget("choices", choicesEdit);
        }
      }
    } else {
      contentGroup->addProperty(tr("Speaker"), speakerValue.isEmpty()
                                                   ? tr("Narrator")
                                                   : speakerValue);
      contentGroup->addProperty(tr("Text"), textValue);
      if (node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
        contentGroup->addProperty(tr("Choices"), choicesValue);
      }
    }
    connect(contentGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  // Choice node branching UI - shows which choice option leads to which target
  if (node->nodeType().contains("Choice", Qt::CaseInsensitive)) {
    auto *branchGroup = addGroup(tr("Branch Mapping"));

    const QStringList choiceOptions = node->choiceOptions();
    const QHash<QString, QString> choiceTargets = node->choiceTargets();

    if (choiceOptions.isEmpty()) {
      branchGroup->addProperty(tr("Info"),
                               tr("Add choices above to configure branching"));
    } else {
      // Build a display string showing choice → target mappings
      QString mappingDisplay;
      for (int i = 0; i < choiceOptions.size(); ++i) {
        const QString &option = choiceOptions[i];
        const QString target = choiceTargets.value(option);
        QString targetDisplay = target.isEmpty() ? tr("(not connected)")
                                                 : target;
        // Truncate long option text for display
        QString optionDisplay = option;
        if (optionDisplay.length() > 25) {
          optionDisplay = optionDisplay.left(22) + "...";
        }
        mappingDisplay += QString("%1. %2 → %3")
                              .arg(i + 1)
                              .arg(optionDisplay)
                              .arg(targetDisplay);
        if (i < choiceOptions.size() - 1) {
          mappingDisplay += "\n";
        }
      }

      if (m_editMode) {
        // Show the mapping as editable multiline (format: "OptionText=TargetNodeId")
        QString editableMapping;
        for (const QString &option : choiceOptions) {
          const QString target = choiceTargets.value(option);
          editableMapping += option + "=" + target + "\n";
        }
        editableMapping = editableMapping.trimmed();

        if (auto *mappingEdit = branchGroup->addEditableProperty(
                "choiceTargets", tr("Choice → Target"),
                NMPropertyType::MultiLine, editableMapping)) {
          trackPropertyWidget("choiceTargets", mappingEdit);
        }
      } else {
        branchGroup->addProperty(tr("Mapping"), mappingDisplay);
      }

      // Add helper text
      branchGroup->addProperty(tr("Help"),
          tr("Connect edges from this node to target nodes.\n"
             "Each connection is automatically mapped to the next choice option."));
    }

    connect(branchGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  if (node->nodeType().contains("Script", Qt::CaseInsensitive)) {
    auto *scriptGroup = addGroup(tr("Script"));
    scriptGroup->addProperty(tr("File"), node->scriptPath());
    if (m_editMode) {
      if (auto *scriptEdit = scriptGroup->addEditableProperty(
              "scriptPath", tr("Path"), NMPropertyType::Asset,
              node->scriptPath())) {
        trackPropertyWidget("scriptPath", scriptEdit);
      }
    }
    connect(scriptGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  // Condition node handling - provides UI for editing condition expression and
  // output branches
  if (node->nodeType().contains("Condition", Qt::CaseInsensitive)) {
    auto *conditionGroup = addGroup(tr("Condition"));
    const QString expressionValue = node->conditionExpression();
    const QString outputsValue = node->conditionOutputs().join("\n");

    if (m_editMode) {
      // Expression editor for entering condition logic
      if (auto *exprEdit = conditionGroup->addEditableProperty(
              "conditionExpression", tr("Expression"),
              NMPropertyType::MultiLine, expressionValue)) {
        trackPropertyWidget("conditionExpression", exprEdit);
      }

      // Output path labels (branch names like "true", "false", or custom)
      if (auto *outputsEdit = conditionGroup->addEditableProperty(
              "conditionOutputs", tr("Output Paths (one per line)"),
              NMPropertyType::MultiLine, outputsValue)) {
        trackPropertyWidget("conditionOutputs", outputsEdit);
      }
    } else {
      conditionGroup->addProperty(tr("Expression"), expressionValue.isEmpty()
                                                        ? tr("(no expression)")
                                                        : expressionValue);
      conditionGroup->addProperty(tr("Outputs"), outputsValue.isEmpty()
                                                     ? tr("true, false")
                                                     : outputsValue);
    }

    connect(conditionGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);

    // Condition branch mapping UI - shows which output leads to which target
    auto *branchGroup = addGroup(tr("Branch Mapping"));

    const QStringList conditionOutputs = node->conditionOutputs();
    const QHash<QString, QString> conditionTargets = node->conditionTargets();

    // Use default "true/false" if no outputs defined
    QStringList outputs = conditionOutputs.isEmpty()
                              ? QStringList{"true", "false"}
                              : conditionOutputs;

    // Build display showing output → target mappings
    QString mappingDisplay;
    for (int i = 0; i < outputs.size(); ++i) {
      const QString &output = outputs[i];
      const QString target = conditionTargets.value(output);
      QString targetDisplay = target.isEmpty() ? tr("(not connected)") : target;
      mappingDisplay += QString("%1 → %2").arg(output).arg(targetDisplay);
      if (i < outputs.size() - 1) {
        mappingDisplay += "\n";
      }
    }

    if (m_editMode) {
      // Show the mapping as editable multiline (format: "OutputLabel=TargetNodeId")
      QString editableMapping;
      for (const QString &output : outputs) {
        const QString target = conditionTargets.value(output);
        editableMapping += output + "=" + target + "\n";
      }
      editableMapping = editableMapping.trimmed();

      if (auto *mappingEdit = branchGroup->addEditableProperty(
              "conditionTargets", tr("Output → Target"),
              NMPropertyType::MultiLine, editableMapping)) {
        trackPropertyWidget("conditionTargets", mappingEdit);
      }
    } else {
      branchGroup->addProperty(tr("Mapping"), mappingDisplay);
    }

    // Add helper text explaining the condition logic
    QString helpText = tr("Expression is evaluated at runtime.\n"
                          "Connect edges from this node to target nodes.\n"
                          "First connection = first output (e.g., 'true').");
    branchGroup->addProperty(tr("Help"), helpText);

    connect(branchGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::inspectMultipleObjects(
    const QList<NMSceneObject *> &objects, bool editable) {
  if (objects.isEmpty()) {
    showNoSelection();
    return;
  }

  clear();
  m_noSelectionLabel->hide();
  m_multiEditMode = true;
  m_editMode = editable;

  // Store object IDs
  m_currentObjectIds.clear();
  for (auto *obj : objects) {
    if (obj) {
      m_currentObjectIds.append(obj->id());
    }
  }

  // Set header showing multi-selection
  m_headerLabel->setText(
      QString("<b>%1 Objects Selected</b>").arg(objects.size()));
  m_headerLabel->show();

  // Use InspectorBindingManager to handle multi-object editing
  auto &inspector = InspectorBindingManager::instance();

  // Prepare targets for binding manager
  std::vector<std::string> objectIds;
  std::vector<void *> objectPtrs;

  for (auto *obj : objects) {
    if (obj) {
      objectIds.push_back(obj->id().toStdString());
      objectPtrs.push_back(static_cast<void *>(obj));
    }
  }

  inspector.inspectSceneObjects(objectIds, objectPtrs);

  // Get property groups from binding manager
  auto groups = inspector.getPropertyGroups();

  for (const auto &group : groups) {
    auto *uiGroup = addGroup(QString::fromStdString(group.name));

    for (const auto *prop : group.properties) {
      if (!prop)
        continue;

      const auto &meta = prop->getMeta();

      // Skip hidden or ID properties
      if (hasFlag(meta.flags, PropertyFlags::Hidden) || meta.name == "id") {
        continue;
      }

      // Get property value (will be MultipleValues if values differ)
      auto value = inspector.getPropertyValue(meta.name);
      QString valueStr = QString::fromStdString(PropertyUtils::toString(value));

      // Determine property type
      NMPropertyType propType = NMPropertyType::String;
      switch (meta.type) {
      case PropertyType::Bool:
        propType = NMPropertyType::Boolean;
        break;
      case PropertyType::Int:
      case PropertyType::Int64:
        propType = NMPropertyType::Integer;
        break;
      case PropertyType::Float:
      case PropertyType::Double:
        propType = NMPropertyType::Float;
        break;
      case PropertyType::Vector2:
        propType = NMPropertyType::Vector2;
        break;
      case PropertyType::Vector3:
        propType = NMPropertyType::Vector3;
        break;
      case PropertyType::Color:
        propType = NMPropertyType::Color;
        break;
      case PropertyType::Enum:
        propType = NMPropertyType::Enum;
        break;
      case PropertyType::AssetRef:
        propType = NMPropertyType::Asset;
        break;
      case PropertyType::CurveRef:
        propType = NMPropertyType::Curve;
        break;
      default:
        propType = NMPropertyType::String;
        break;
      }

      if (m_editMode && !hasFlag(meta.flags, PropertyFlags::ReadOnly)) {
        // Add editable property
        QStringList enumOptions;
        if (meta.type == PropertyType::Enum) {
          for (const auto &opt : meta.enumOptions) {
            enumOptions.append(QString::fromStdString(opt.second));
          }
        }

        if (auto *widget = uiGroup->addEditableProperty(
                QString::fromStdString(meta.name),
                QString::fromStdString(meta.displayName), propType, valueStr,
                enumOptions)) {
          trackPropertyWidget(QString::fromStdString(meta.name), widget);

          // Special styling for "multiple values" placeholder
          if (valueStr == "<multiple values>") {
            if (auto *lineEdit = qobject_cast<QLineEdit *>(widget)) {
              lineEdit->setPlaceholderText("<multiple values>");
              lineEdit->clear();
            }
          }
        }
      } else {
        // Read-only property
        uiGroup->addProperty(QString::fromStdString(meta.displayName),
                             valueStr);
      }
    }

    connect(uiGroup, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::onGroupPropertyChanged(const QString &propertyName,
                                              const QString &newValue) {
  // In multi-edit mode, apply changes through InspectorBindingManager
  if (m_multiEditMode) {
    auto &inspector = InspectorBindingManager::instance();
    auto error = inspector.setPropertyValueFromString(
        propertyName.toStdString(), newValue.toStdString());

    if (error.has_value()) {
      // ERR-1 fix: Show error to user via status bar instead of just logging
      QString errorMsg = QString::fromStdString(error.value());
      qWarning() << "Failed to set property:" << propertyName
                 << "Error:" << errorMsg;
      // Emit signal for status bar or use inline notification
      emit propertyError(propertyName, errorMsg);
    }
    return;
  }

  // Handle reset button signals
  if (propertyName == "reset_position") {
    emit propertyChanged(m_currentObjectId, "position_x", "0");
    emit propertyChanged(m_currentObjectId, "position_y", "0");
    // Update UI spinboxes
    updatePropertyValue("position_x", "0");
    updatePropertyValue("position_y", "0");
    return;
  }

  if (propertyName == "reset_rotation") {
    emit propertyChanged(m_currentObjectId, "rotation", "0");
    updatePropertyValue("rotation", "0");
    return;
  }

  if (propertyName == "reset_scale") {
    emit propertyChanged(m_currentObjectId, "scale_x", "1");
    emit propertyChanged(m_currentObjectId, "scale_y", "1");
    updatePropertyValue("scale_x", "1");
    updatePropertyValue("scale_y", "1");
    m_lastScale = QPointF(1.0, 1.0);
    return;
  }

  // Handle aspect ratio lock for scale changes
  if (m_lockAspectRatio && (propertyName == "scale_x" || propertyName == "scale_y")) {
    double newScale = newValue.toDouble();

    // Calculate the ratio change
    if (propertyName == "scale_x" && m_lastScale.x() > 0.0001) {
      double ratio = newScale / m_lastScale.x();
      double newScaleY = m_lastScale.y() * ratio;

      // Update scale_y proportionally
      updatePropertyValue("scale_y", QString::number(newScaleY, 'f', 2));
      emit propertyChanged(m_currentObjectId, "scale_x", newValue);
      emit propertyChanged(m_currentObjectId, "scale_y", QString::number(newScaleY, 'f', 2));

      m_lastScale = QPointF(newScale, newScaleY);
      return;
    } else if (propertyName == "scale_y" && m_lastScale.y() > 0.0001) {
      double ratio = newScale / m_lastScale.y();
      double newScaleX = m_lastScale.x() * ratio;

      // Update scale_x proportionally
      updatePropertyValue("scale_x", QString::number(newScaleX, 'f', 2));
      emit propertyChanged(m_currentObjectId, "scale_x", QString::number(newScaleX, 'f', 2));
      emit propertyChanged(m_currentObjectId, "scale_y", newValue);

      m_lastScale = QPointF(newScaleX, newScale);
      return;
    }
  }

  // Update last scale tracking for non-locked changes
  if (propertyName == "scale_x") {
    m_lastScale.setX(newValue.toDouble());
  } else if (propertyName == "scale_y") {
    m_lastScale.setY(newValue.toDouble());
  }

  // Single-object mode: emit signal
  emit propertyChanged(m_currentObjectId, propertyName, newValue);
}

void NMInspectorPanel::updatePropertyValue(const QString &propertyName,
                                           const QString &newValue) {
  auto it = m_propertyWidgets.find(propertyName);
  if (it == m_propertyWidgets.end()) {
    return;
  }

  QWidget *widget = it.value();
  QSignalBlocker blocker(widget);
  if (auto *lineEdit = qobject_cast<QLineEdit *>(widget)) {
    // Only update if value has changed and widget doesn't have focus
    // to preserve undo history and cursor position during user editing
    if (lineEdit->text() != newValue && !lineEdit->hasFocus()) {
      int cursorPos = lineEdit->cursorPosition();
      lineEdit->setText(newValue);
      // Restore cursor position if still valid
      lineEdit->setCursorPosition(qMin(cursorPos, newValue.length()));
    }
  } else if (auto *spinBox = qobject_cast<QSpinBox *>(widget)) {
    spinBox->setValue(newValue.toInt());
  } else if (auto *doubleSpinBox = qobject_cast<QDoubleSpinBox *>(widget)) {
    doubleSpinBox->setValue(newValue.toDouble());
  } else if (auto *checkBox = qobject_cast<QCheckBox *>(widget)) {
    checkBox->setChecked(newValue.toLower() == "true" || newValue == "1");
  } else if (auto *comboBox = qobject_cast<QComboBox *>(widget)) {
    comboBox->setCurrentText(newValue);
  } else if (auto *textEdit = qobject_cast<QPlainTextEdit *>(widget)) {
    // Only update if value has changed and widget doesn't have focus
    // to preserve undo history and cursor position during user editing
    if (textEdit->toPlainText() != newValue && !textEdit->hasFocus()) {
      // Save cursor position and selection
      QTextCursor cursor = textEdit->textCursor();
      int cursorPos = cursor.position();
      int anchorPos = cursor.anchor();

      textEdit->setPlainText(newValue);

      // Restore cursor position if still valid
      if (cursorPos <= newValue.length()) {
        cursor.setPosition(qMin(anchorPos, newValue.length()));
        cursor.setPosition(qMin(cursorPos, newValue.length()),
                          QTextCursor::KeepAnchor);
        textEdit->setTextCursor(cursor);
      }
    }
  } else if (auto *button = qobject_cast<QPushButton *>(widget)) {
    // Check if this is a curve button or asset button
    if (button->text() == tr("Edit Curve...")) {
      button->setProperty("curveId", newValue);
    } else {
      button->setText(newValue.isEmpty() ? "(Select Asset)" : newValue);
    }
  } else {
    // Handle vector widgets (Vector2/Vector3)
    // Vector widgets are container QWidgets with child spinboxes
    QList<QDoubleSpinBox *> spinBoxes =
        widget->findChildren<QDoubleSpinBox *>();
    if (!spinBoxes.isEmpty()) {
      QStringList components = newValue.split(',');
      for (int i = 0; i < spinBoxes.size() && i < components.size(); ++i) {
        QSignalBlocker spinBoxBlocker(spinBoxes[i]);
        spinBoxes[i]->setValue(components[i].toDouble());
      }
    }
  }
}

void NMInspectorPanel::trackPropertyWidget(const QString &propertyName,
                                           QWidget *widget) {
  if (!propertyName.isEmpty() && widget) {
    m_propertyWidgets.insert(propertyName, widget);
  }
}

NMPropertyGroup *NMInspectorPanel::addGroup(const QString &title) {
  auto *group = new NMPropertyGroup(title, m_scrollContent);
  m_mainLayout->addWidget(group);
  m_groups.append(group);
  return group;
}

void NMInspectorPanel::showNoSelection() {
  clear();
  m_headerLabel->hide();
  m_noSelectionLabel->show();
}

void NMInspectorPanel::setupContent() {
  auto *container = new QWidget(this);
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);

  // Header
  m_headerLabel = new QLabel(container);
  m_headerLabel->setObjectName("InspectorHeader");
  m_headerLabel->setWordWrap(true);
  m_headerLabel->setTextFormat(Qt::RichText);
  m_headerLabel->setMargin(8);
  m_headerLabel->hide();
  containerLayout->addWidget(m_headerLabel);

  // Scroll area
  m_scrollArea = new QScrollArea(container);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  m_scrollContent = new QWidget(m_scrollArea);
  m_mainLayout = new QVBoxLayout(m_scrollContent);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(8);
  m_mainLayout->setAlignment(Qt::AlignTop);

  m_scrollArea->setWidget(m_scrollContent);
  containerLayout->addWidget(m_scrollArea, 1);

  // No selection label
  m_noSelectionLabel =
      new QLabel(tr("Select an object to view its properties"), container);
  m_noSelectionLabel->setObjectName("InspectorEmptyState");
  m_noSelectionLabel->setAlignment(Qt::AlignCenter);
  m_noSelectionLabel->setWordWrap(true);

  const auto &palette = NMStyleManager::instance().palette();
  m_noSelectionLabel->setStyleSheet(
      QString("color: %1; padding: 20px;")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));

  m_mainLayout->addWidget(m_noSelectionLabel);

  setContentWidget(container);
}

} // namespace NovelMind::editor::qt
