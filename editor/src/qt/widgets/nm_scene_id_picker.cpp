#include "NovelMind/editor/qt/widgets/nm_scene_id_picker.hpp"
#include "NovelMind/editor/qt/style/nm_style_manager.hpp"
#include "NovelMind/editor/scene_registry.hpp"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>

namespace NovelMind::editor::qt {

NMSceneIdPicker::NMSceneIdPicker(SceneRegistry *registry, QWidget *parent)
    : QWidget(parent), m_registry(registry) {
  setupUI();

  if (m_registry) {
    // Connect to registry signals to auto-update
    connect(m_registry, &SceneRegistry::sceneRegistered, this,
            &NMSceneIdPicker::onSceneRegistryChanged);
    connect(m_registry, &SceneRegistry::sceneUnregistered, this,
            &NMSceneIdPicker::onSceneRegistryChanged);
    connect(m_registry, &SceneRegistry::sceneRenamed, this,
            &NMSceneIdPicker::onSceneRegistryChanged);
    connect(m_registry, &SceneRegistry::sceneThumbnailUpdated, this,
            [this]() { updateThumbnail(); });

    // Initial population
    refreshSceneList();
  }
}

void NMSceneIdPicker::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(8);

  // Combo box with validation icon
  m_comboLayout = new QHBoxLayout();
  m_comboLayout->setSpacing(4);

  m_sceneCombo = new QComboBox(this);
  m_sceneCombo->setMinimumWidth(200);
  m_sceneCombo->setEditable(false);
  m_sceneCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  // Style the combo box
  const QColor bgColor = NMStyleManager::getColor(NMStyleColor::BackgroundDark);
  const QColor textColor = NMStyleManager::getColor(NMStyleColor::TextPrimary);
  const QColor borderColor =
      NMStyleManager::getColor(NMStyleColor::BorderLight);
  const QColor highlightColor =
      NMStyleManager::getColor(NMStyleColor::AccentPrimary);

  m_sceneCombo->setStyleSheet(QString("QComboBox {"
                                      "  background-color: %1;"
                                      "  color: %2;"
                                      "  border: 1px solid %3;"
                                      "  border-radius: 4px;"
                                      "  padding: 4px 8px;"
                                      "  min-height: 24px;"
                                      "}"
                                      "QComboBox:hover {"
                                      "  border-color: %4;"
                                      "}"
                                      "QComboBox::drop-down {"
                                      "  border: none;"
                                      "  width: 20px;"
                                      "}"
                                      "QComboBox::down-arrow {"
                                      "  image: none;"
                                      "  border-left: 4px solid transparent;"
                                      "  border-right: 4px solid transparent;"
                                      "  border-top: 6px solid %2;"
                                      "  margin-right: 4px;"
                                      "}")
                                  .arg(bgColor.name())
                                  .arg(textColor.name())
                                  .arg(borderColor.name())
                                  .arg(highlightColor.name()));

  connect(m_sceneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMSceneIdPicker::onSceneSelectionChanged);

  // Validation icon
  m_validationIcon = new QLabel(this);
  m_validationIcon->setFixedSize(16, 16);
  m_validationIcon->setScaledContents(true);

  m_comboLayout->addWidget(m_sceneCombo, 1);
  m_comboLayout->addWidget(m_validationIcon);
  m_mainLayout->addLayout(m_comboLayout);

  // Preview section with thumbnail and info
  m_previewLayout = new QHBoxLayout();
  m_previewLayout->setSpacing(8);

  m_thumbnailLabel = new QLabel(this);
  m_thumbnailLabel->setFixedSize(80, 45); // 16:9 aspect ratio
  m_thumbnailLabel->setScaledContents(true);
  m_thumbnailLabel->setStyleSheet(QString("QLabel {"
                                          "  background-color: %1;"
                                          "  border: 1px solid %2;"
                                          "  border-radius: 4px;"
                                          "}")
                                      .arg(bgColor.name())
                                      .arg(borderColor.name()));

  m_sceneInfoLabel = new QLabel(this);
  m_sceneInfoLabel->setWordWrap(true);
  m_sceneInfoLabel->setStyleSheet(
      QString("QLabel { color: %1; font-size: 11px; }")
          .arg(NMStyleManager::getColor(NMStyleColor::TextSecondary).name()));

  m_previewLayout->addWidget(m_thumbnailLabel);
  m_previewLayout->addWidget(m_sceneInfoLabel, 1);
  m_mainLayout->addLayout(m_previewLayout);

  // Quick action buttons
  m_actionsLayout = new QHBoxLayout();
  m_actionsLayout->setSpacing(4);

  m_createButton = new QPushButton("Create New", this);
  m_createButton->setMaximumWidth(100);
  m_editButton = new QPushButton("Edit Scene", this);
  m_editButton->setMaximumWidth(100);
  m_locateButton = new QPushButton("Locate", this);
  m_locateButton->setMaximumWidth(80);

  // Style buttons
  QString buttonStyle = QString("QPushButton {"
                                "  background-color: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  border-radius: 4px;"
                                "  padding: 4px 8px;"
                                "  font-size: 11px;"
                                "}"
                                "QPushButton:hover {"
                                "  background-color: %4;"
                                "  border-color: %4;"
                                "}"
                                "QPushButton:pressed {"
                                "  background-color: %5;"
                                "}")
                            .arg(bgColor.name())
                            .arg(textColor.name())
                            .arg(borderColor.name())
                            .arg(highlightColor.name())
                            .arg(highlightColor.darker(120).name());

  m_createButton->setStyleSheet(buttonStyle);
  m_editButton->setStyleSheet(buttonStyle);
  m_locateButton->setStyleSheet(buttonStyle);

  connect(m_createButton, &QPushButton::clicked, this,
          &NMSceneIdPicker::onCreateNewClicked);
  connect(m_editButton, &QPushButton::clicked, this,
          &NMSceneIdPicker::onEditSceneClicked);
  connect(m_locateButton, &QPushButton::clicked, this,
          &NMSceneIdPicker::onLocateClicked);

  m_actionsLayout->addWidget(m_createButton);
  m_actionsLayout->addWidget(m_editButton);
  m_actionsLayout->addWidget(m_locateButton);
  m_actionsLayout->addStretch();
  m_mainLayout->addLayout(m_actionsLayout);

  // Initially disable edit/locate buttons
  m_editButton->setEnabled(false);
  m_locateButton->setEnabled(false);
}

void NMSceneIdPicker::setSceneId(const QString &sceneId) {
  if (m_currentSceneId == sceneId) {
    return;
  }

  m_updating = true;
  m_currentSceneId = sceneId;

  // Find and select in combo box
  int index = m_sceneCombo->findData(sceneId);
  if (index >= 0) {
    m_sceneCombo->setCurrentIndex(index);
  } else if (!sceneId.isEmpty()) {
    // Scene ID not in registry - add as invalid entry
    m_sceneCombo->insertItem(0, QString("⚠ %1 (not found)").arg(sceneId),
                             sceneId);
    m_sceneCombo->setCurrentIndex(0);
  } else {
    m_sceneCombo->setCurrentIndex(0); // Select "(none)"
  }

  updateThumbnail();
  updateValidationState();
  m_updating = false;
}

QString NMSceneIdPicker::sceneId() const { return m_currentSceneId; }

void NMSceneIdPicker::refreshSceneList() {
  if (!m_registry) {
    return;
  }

  m_updating = true;
  QString previousSelection = m_currentSceneId;

  m_sceneCombo->clear();
  m_sceneCombo->addItem("(none)", QString());

  // Get all scenes from registry
  QList<SceneMetadata> scenes = m_registry->getScenes();

  // Sort scenes by name
  std::sort(scenes.begin(), scenes.end(),
            [](const SceneMetadata &a, const SceneMetadata &b) {
              return a.name < b.name;
            });

  // Add scenes to combo box
  for (const SceneMetadata &meta : scenes) {
    QString displayText = QString("%1 (%2)").arg(meta.name, meta.id);
    m_sceneCombo->addItem(displayText, meta.id);
  }

  // Restore previous selection if it still exists
  if (!previousSelection.isEmpty()) {
    setSceneId(previousSelection);
  }

  m_updating = false;
}

void NMSceneIdPicker::setReadOnly(bool readOnly) {
  m_readOnly = readOnly;
  m_sceneCombo->setEnabled(!readOnly);
  m_createButton->setEnabled(!readOnly);
  m_editButton->setEnabled(!readOnly && !m_currentSceneId.isEmpty());
  m_locateButton->setEnabled(!readOnly && !m_currentSceneId.isEmpty());
}

void NMSceneIdPicker::onSceneSelectionChanged(int index) {
  if (m_updating) {
    return;
  }

  QString newSceneId = m_sceneCombo->itemData(index).toString();
  if (newSceneId != m_currentSceneId) {
    m_currentSceneId = newSceneId;
    updateThumbnail();
    updateValidationState();
    emit sceneIdChanged(m_currentSceneId);
  }

  // Update button states
  bool hasScene = !m_currentSceneId.isEmpty();
  m_editButton->setEnabled(!m_readOnly && hasScene);
  m_locateButton->setEnabled(!m_readOnly && hasScene);
}

void NMSceneIdPicker::onCreateNewClicked() {
  emit createNewSceneRequested();
}

void NMSceneIdPicker::onEditSceneClicked() {
  if (!m_currentSceneId.isEmpty()) {
    emit editSceneRequested(m_currentSceneId);
  }
}

void NMSceneIdPicker::onLocateClicked() {
  if (!m_currentSceneId.isEmpty()) {
    emit locateSceneRequested(m_currentSceneId);
  }
}

void NMSceneIdPicker::onSceneRegistryChanged() { refreshSceneList(); }

void NMSceneIdPicker::updateThumbnail() {
  if (m_currentSceneId.isEmpty() || !m_registry) {
    m_thumbnailLabel->clear();
    m_sceneInfoLabel->setText("No scene selected");
    return;
  }

  if (!m_registry->sceneExists(m_currentSceneId)) {
    m_thumbnailLabel->clear();
    m_sceneInfoLabel->setText(
        QString("<span style='color: #ff6b6b;'>Scene not found in "
                "registry</span>"));
    return;
  }

  // Load thumbnail
  QString thumbnailPath = m_registry->getAbsoluteThumbnailPath(m_currentSceneId);
  if (!thumbnailPath.isEmpty() && QFileInfo::exists(thumbnailPath)) {
    QPixmap pixmap(thumbnailPath);
    if (!pixmap.isNull()) {
      m_thumbnailLabel->setPixmap(pixmap);
    } else {
      m_thumbnailLabel->setText("Preview\nN/A");
    }
  } else {
    m_thumbnailLabel->setText("Preview\nN/A");
  }

  // Update scene info
  SceneMetadata meta = m_registry->getSceneMetadata(m_currentSceneId);
  QString docPath = meta.documentPath;
  QString modifiedTime = "Unknown";
  if (meta.modified.isValid()) {
    // Show relative time (e.g., "2h ago", "3 days ago")
    qint64 secondsAgo = meta.modified.secsTo(QDateTime::currentDateTime());
    if (secondsAgo < 60) {
      modifiedTime = "Just now";
    } else if (secondsAgo < 3600) {
      modifiedTime = QString("%1m ago").arg(secondsAgo / 60);
    } else if (secondsAgo < 86400) {
      modifiedTime = QString("%1h ago").arg(secondsAgo / 3600);
    } else {
      modifiedTime = QString("%1 days ago").arg(secondsAgo / 86400);
    }
  }

  QString infoText = QString("<b>%1</b><br>%2<br><small>Modified: %3</small>")
                         .arg(meta.name, docPath, modifiedTime);

  m_sceneInfoLabel->setText(infoText);
}

void NMSceneIdPicker::updateValidationState() {
  if (m_currentSceneId.isEmpty()) {
    m_validationIcon->clear();
    return;
  }

  if (!m_registry || !m_registry->sceneExists(m_currentSceneId)) {
    // Invalid scene reference - show warning icon
    m_validationIcon->setText("⚠");
    m_validationIcon->setStyleSheet("QLabel { color: #ff6b6b; font-size: 16px; "
                                    "font-weight: bold; }");
    m_validationIcon->setToolTip("Scene not found in registry");
  } else {
    // Valid scene reference - show check icon
    m_validationIcon->setText("✓");
    m_validationIcon->setStyleSheet("QLabel { color: #51cf66; font-size: 16px; "
                                    "font-weight: bold; }");
    m_validationIcon->setToolTip("Valid scene reference");
  }
}

QString NMSceneIdPicker::getDisplayName(const QString &sceneId) const {
  if (sceneId.isEmpty()) {
    return "(none)";
  }

  if (!m_registry || !m_registry->sceneExists(sceneId)) {
    return QString("⚠ %1 (not found)").arg(sceneId);
  }

  SceneMetadata meta = m_registry->getSceneMetadata(sceneId);
  return QString("%1 (%2)").arg(meta.name, sceneId);
}

} // namespace NovelMind::editor::qt
