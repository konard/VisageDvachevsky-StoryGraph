#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/inspector_property_editor.hpp"
#include "NovelMind/editor/qt/panels/inspector_component_list.hpp"
#include "NovelMind/editor/qt/panels/inspector_validators.hpp"
#include "NovelMind/editor/qt/panels/inspector_ui.hpp"
#include "NovelMind/core/property_system.hpp"
#include "NovelMind/editor/inspector_binding.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/widgets/nm_scene_id_picker.hpp"
#include "NovelMind/editor/scene_registry.hpp"

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

NMInspectorPanel::NMInspectorPanel(QWidget* parent) : NMDockPanel(tr("Inspector"), parent) {
  setPanelId("Inspector");

  // Inspector needs adequate width for property labels and edit controls
  // and height to show at least a few property groups without excessive
  // scrolling
  setMinimumPanelSize(280, 200);

  // PERF-2: Setup update timer for batching property updates
  m_updateTimer = new QTimer(this);
  m_updateTimer->setSingleShot(true);
  m_updateTimer->setInterval(16); // ~60fps update rate
  connect(m_updateTimer, &QTimer::timeout, this, &NMInspectorPanel::flushPendingUpdates);

  // Initialize refactored modules
  m_propertyEditor = new InspectorPropertyEditor(this);
  m_componentList = new InspectorComponentList(this);
  m_validators = new InspectorValidators(this);
  m_uiManager = new InspectorUI(this);

  setupContent();
}

NMInspectorPanel::~NMInspectorPanel() {
  delete m_propertyEditor;
  delete m_componentList;
  delete m_validators;
  delete m_uiManager;
}

void NMInspectorPanel::onInitialize() {
  showNoSelection();
}

void NMInspectorPanel::onUpdate(double /*deltaTime*/) {
  // No continuous update needed
}

void NMInspectorPanel::clear() {
  // Use UI manager to clear groups
  m_uiManager->clearGroups(m_mainLayout, m_groups, m_headerLabel);
  m_propertyWidgets.clear();

  // PERF-2: Clear dirty flag caches when switching objects
  m_pendingUpdates.clear();
  m_lastKnownValues.clear();
  if (m_updateTimer && m_updateTimer->isActive()) {
    m_updateTimer->stop();
  }
}

void NMInspectorPanel::inspectObject(const QString& objectType, const QString& objectId,
                                     bool editable) {
  clear();
  m_noSelectionLabel->hide();
  m_currentObjectId = objectId;
  m_editMode = editable;

  // Set header
  m_uiManager->setHeader(
      m_headerLabel,
      QString("<b>%1</b><br><span style='color: gray;'>%2</span>").arg(objectType).arg(objectId));

  // Add demo properties based on type
  auto* transformGroup = addGroup(tr("Transform"));

  if (m_editMode) {
    transformGroup->addEditableProperty(tr("Position X"), NMPropertyType::Float, "0.0");
    transformGroup->addEditableProperty(tr("Position Y"), NMPropertyType::Float, "0.0");
    transformGroup->addEditableProperty(tr("Rotation"), NMPropertyType::Float, "0.0");
    transformGroup->addEditableProperty(tr("Scale X"), NMPropertyType::Float, "1.0");
    transformGroup->addEditableProperty(tr("Scale Y"), NMPropertyType::Float, "1.0");
  } else {
    transformGroup->addProperty(tr("Position X"), "0.0");
    transformGroup->addProperty(tr("Position Y"), "0.0");
    transformGroup->addProperty(tr("Rotation"), "0.0");
    transformGroup->addProperty(tr("Scale X"), "1.0");
    transformGroup->addProperty(tr("Scale Y"), "1.0");
  }

  connect(transformGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  auto* renderGroup = addGroup(tr("Rendering"));

  if (m_editMode) {
    renderGroup->addEditableProperty(tr("Visible"), NMPropertyType::Boolean, "true");
    renderGroup->addEditableProperty(tr("Alpha"), NMPropertyType::Float, "1.0");
    renderGroup->addEditableProperty(tr("Z-Order"), NMPropertyType::Integer, "0");
    renderGroup->addEditableProperty(tr("Blend Mode"), NMPropertyType::Enum, "Normal",
                                     {"Normal", "Additive", "Multiply", "Screen", "Overlay"});
    renderGroup->addEditableProperty(tr("Tint Color"), NMPropertyType::Color, "#FFFFFF");
  } else {
    renderGroup->addProperty(tr("Visible"), "true");
    renderGroup->addProperty(tr("Alpha"), "1.0");
    renderGroup->addProperty(tr("Z-Order"), "0");
  }

  connect(renderGroup, &NMPropertyGroup::propertyValueChanged, this,
          &NMInspectorPanel::onGroupPropertyChanged);

  if (objectType == "Dialogue" || objectType == "Choice") {
    auto* dialogueGroup = addGroup(tr("Dialogue"));

    if (m_editMode) {
      dialogueGroup->addEditableProperty(tr("Speaker"), NMPropertyType::String, "Narrator");
      dialogueGroup->addEditableProperty(tr("Text"), NMPropertyType::String, objectId);
      dialogueGroup->addEditableProperty(tr("Voice Clip"), NMPropertyType::Asset, "");
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

void NMInspectorPanel::inspectSceneObject(NMSceneObject* object, bool editable) {
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

  m_uiManager->setHeader(
      m_headerLabel,
      QString("<b>%1</b><br><span style='color: gray;'>%2</span>").arg(typeName).arg(object->id()));

  // Delegate property creation to the property editor module
  m_propertyEditor->createSceneObjectProperties(object, m_editMode, m_sceneRegistry);

  // Connect property group signals
  for (auto* group : m_groups) {
    connect(group, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::inspectStoryGraphNode(NMGraphNodeItem* node, bool editable) {
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

  m_uiManager->setHeader(m_headerLabel, QString("<b>%1</b><br><span style='color: gray;'>%2</span>")
                                            .arg(node->nodeType())
                                            .arg(node->nodeIdString()));

  // Delegate property creation to the property editor module
  m_propertyEditor->createStoryGraphNodeProperties(node, m_editMode, m_sceneRegistry);

  // Note: Scene picker signal connections need special handling
  // Find the scene picker widget and reconnect signals if needed
  if (node->isSceneNode() && m_editMode && m_sceneRegistry) {
    for (auto* group : m_groups) {
      // Find scene picker widget in the group
      auto sceneIdWidgets = group->findChildren<NMSceneIdPicker*>();
      for (auto* scenePicker : sceneIdWidgets) {
        // Track value changes
        connect(scenePicker, &NMSceneIdPicker::sceneIdChanged, this,
                [this, node](const QString& newSceneId) {
                  if (!m_updating) {
                    emit propertyChanged(node->nodeIdString(), "sceneId", newSceneId);
                  }
                });

        // Connect quick action signals to appropriate handlers
        connect(scenePicker, &NMSceneIdPicker::createNewSceneRequested, this, [this]() {
          qDebug() << "Create new scene requested";
          emit createNewSceneRequested();
        });

        connect(scenePicker, &NMSceneIdPicker::editSceneRequested, this,
                [this](const QString& sceneId) {
                  qDebug() << "Edit scene requested:" << sceneId;
                  emit editSceneRequested(sceneId);
                });

        connect(scenePicker, &NMSceneIdPicker::locateSceneRequested, this,
                [this](const QString& sceneId) {
                  qDebug() << "Locate scene requested:" << sceneId;
                  emit locateSceneInGraphRequested(sceneId);
                });

        trackPropertyWidget("sceneId", scenePicker);
      }
    }
  }

  // Connect property group signals
  for (auto* group : m_groups) {
    connect(group, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::inspectMultipleObjects(const QList<NMSceneObject*>& objects, bool editable) {
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
  for (auto* obj : objects) {
    if (obj) {
      m_currentObjectIds.append(obj->id());
    }
  }

  // Set header showing multi-selection
  m_uiManager->setHeader(m_headerLabel, QString("<b>%1 Objects Selected</b>").arg(objects.size()));

  // Delegate to component list module
  m_componentList->createMultiObjectPropertyGroups(objects, m_editMode);

  // Connect property group signals
  for (auto* group : m_groups) {
    connect(group, &NMPropertyGroup::propertyValueChanged, this,
            &NMInspectorPanel::onGroupPropertyChanged);
  }

  m_mainLayout->addStretch();
}

void NMInspectorPanel::onGroupPropertyChanged(const QString& propertyName,
                                              const QString& newValue) {
  // Delegate to validators module
  m_validators->handlePropertyChanged(propertyName, newValue, m_currentObjectId, m_currentObjectIds,
                                      m_multiEditMode, m_lockAspectRatio, m_lastScale);
}

void NMInspectorPanel::updatePropertyValue(const QString& propertyName, const QString& newValue) {
  // PERF-2: Use validators to queue the update
  if (m_validators->queuePropertyUpdate(propertyName, newValue, m_pendingUpdates,
                                        m_lastKnownValues)) {
    // Start or restart the batching timer
    if (m_updateTimer && !m_updateTimer->isActive()) {
      m_updateTimer->start();
    }
  }
}

void NMInspectorPanel::flushPendingUpdates() {
  // PERF-2: Delegate to validators module
  m_validators->flushPendingUpdates(m_pendingUpdates, m_propertyWidgets);

  // Clear pending updates
  m_pendingUpdates.clear();
}

void NMInspectorPanel::trackPropertyWidget(const QString& propertyName, QWidget* widget) {
  if (!propertyName.isEmpty() && widget) {
    m_propertyWidgets.insert(propertyName, widget);
  }
}

NMPropertyGroup* NMInspectorPanel::addGroup(const QString& title) {
  return m_uiManager->addGroup(title, m_scrollContent, m_mainLayout, m_groups);
}

void NMInspectorPanel::showNoSelection() {
  clear();
  m_uiManager->showNoSelection(m_headerLabel, m_noSelectionLabel);
}

void NMInspectorPanel::setupContent() {
  auto* container = m_uiManager->setupContent(this);

  // Get references to UI widgets created by the manager
  m_scrollArea = m_uiManager->getScrollContent()->parentWidget()->findChild<QScrollArea*>();
  m_scrollContent = m_uiManager->getScrollContent();
  m_mainLayout = m_uiManager->getMainLayout();
  m_headerLabel = m_uiManager->getHeaderLabel();
  m_noSelectionLabel = m_uiManager->getNoSelectionLabel();

  setContentWidget(container);
}

} // namespace NovelMind::editor::qt
