#include "NovelMind/editor/qt/panels/nm_project_settings_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMProjectSettingsPanel::NMProjectSettingsPanel(QWidget *parent)
    : NMDockPanel("Project Settings", parent) {}

NMProjectSettingsPanel::~NMProjectSettingsPanel() = default;

void NMProjectSettingsPanel::onInitialize() {
  setupUI();
  connectSignals();
  loadFromProject();
}

void NMProjectSettingsPanel::onShutdown() {}

void NMProjectSettingsPanel::onUpdate([[maybe_unused]] double deltaTime) {}

void NMProjectSettingsPanel::setupUI() {
  auto *mainWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(mainWidget);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Tab widget for different settings categories
  m_tabWidget = new QTabWidget(mainWidget);

  // Workflow tab first - most important for scene development approach
  auto *workflowTab = new QWidget();
  setupWorkflowTab(workflowTab);
  m_tabWidget->addTab(workflowTab, tr("Workflow"));

  auto *displayTab = new QWidget();
  setupDisplayTab(displayTab);
  m_tabWidget->addTab(displayTab, tr("Display"));

  auto *textTab = new QWidget();
  setupTextTab(textTab);
  m_tabWidget->addTab(textTab, tr("Text && Dialogue"));

  auto *localizationTab = new QWidget();
  setupLocalizationTab(localizationTab);
  m_tabWidget->addTab(localizationTab, tr("Localization"));

  auto *buildTab = new QWidget();
  setupBuildProfilesTab(buildTab);
  m_tabWidget->addTab(buildTab, tr("Build Profiles"));

  layout->addWidget(m_tabWidget, 1);

  // Bottom button bar
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_revertButton = new QPushButton(tr("Revert"), mainWidget);
  m_revertButton->setEnabled(false);
  m_revertButton->setToolTip(tr("Discard all unsaved changes"));
  connect(m_revertButton, &QPushButton::clicked, this,
          &NMProjectSettingsPanel::onRevertClicked);

  m_applyButton = new QPushButton(tr("Apply"), mainWidget);
  m_applyButton->setEnabled(false);
  m_applyButton->setObjectName("NMPrimaryButton");
  m_applyButton->setToolTip(tr("Save all settings to project"));
  connect(m_applyButton, &QPushButton::clicked, this,
          &NMProjectSettingsPanel::onApplyClicked);

  buttonLayout->addWidget(m_revertButton);
  buttonLayout->addWidget(m_applyButton);
  layout->addLayout(buttonLayout);

  setContentWidget(mainWidget);
}

void NMProjectSettingsPanel::setupDisplayTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Resolution group
  auto *resolutionGroup = new QGroupBox(tr("Resolution"), parent);
  auto *resolutionLayout = new QFormLayout(resolutionGroup);

  m_resolutionCombo = new QComboBox(resolutionGroup);
  m_resolutionCombo->addItems({"1920x1080 (Full HD)", "1280x720 (HD)",
                               "2560x1440 (QHD)", "3840x2160 (4K)", "1600x900",
                               "1366x768", "800x600", "1024x768"});
  m_resolutionCombo->setToolTip(
      tr("Base resolution for rendering.\nThe game will scale to fit "
         "different screen sizes."));
  resolutionLayout->addRow(tr("Base Resolution:"), m_resolutionCombo);

  m_aspectRatioMode = new QComboBox(resolutionGroup);
  m_aspectRatioMode->addItems(
      {tr("Letterbox/Pillarbox"), tr("Stretch to Fill"), tr("Crop to Fill")});
  m_aspectRatioMode->setToolTip(tr("How to handle aspect ratio differences:\n"
                                   "- Letterbox: Add black bars\n"
                                   "- Stretch: May distort image\n"
                                   "- Crop: May cut off edges"));
  resolutionLayout->addRow(tr("Aspect Ratio Mode:"), m_aspectRatioMode);

  layout->addWidget(resolutionGroup);

  // Safe Area group
  auto *safeAreaGroup = new QGroupBox(tr("Safe Area (pixels)"), parent);
  safeAreaGroup->setToolTip(
      tr("Margins where important content should not be placed.\n"
         "Useful for TV displays with overscan."));
  auto *safeAreaLayout = new QGridLayout(safeAreaGroup);

  m_safeAreaTop = new QSpinBox(safeAreaGroup);
  m_safeAreaTop->setRange(0, 200);
  m_safeAreaTop->setSuffix(" px");
  safeAreaLayout->addWidget(new QLabel(tr("Top:")), 0, 0);
  safeAreaLayout->addWidget(m_safeAreaTop, 0, 1);

  m_safeAreaBottom = new QSpinBox(safeAreaGroup);
  m_safeAreaBottom->setRange(0, 200);
  m_safeAreaBottom->setSuffix(" px");
  safeAreaLayout->addWidget(new QLabel(tr("Bottom:")), 1, 0);
  safeAreaLayout->addWidget(m_safeAreaBottom, 1, 1);

  m_safeAreaLeft = new QSpinBox(safeAreaGroup);
  m_safeAreaLeft->setRange(0, 200);
  m_safeAreaLeft->setSuffix(" px");
  safeAreaLayout->addWidget(new QLabel(tr("Left:")), 0, 2);
  safeAreaLayout->addWidget(m_safeAreaLeft, 0, 3);

  m_safeAreaRight = new QSpinBox(safeAreaGroup);
  m_safeAreaRight->setRange(0, 200);
  m_safeAreaRight->setSuffix(" px");
  safeAreaLayout->addWidget(new QLabel(tr("Right:")), 1, 2);
  safeAreaLayout->addWidget(m_safeAreaRight, 1, 3);

  layout->addWidget(safeAreaGroup);

  // Window options
  auto *windowGroup = new QGroupBox(tr("Window Options"), parent);
  auto *windowLayout = new QVBoxLayout(windowGroup);

  m_fullscreenDefault = new QCheckBox(tr("Start in Fullscreen"), windowGroup);
  windowLayout->addWidget(m_fullscreenDefault);

  m_allowWindowResize = new QCheckBox(tr("Allow Window Resizing"), windowGroup);
  m_allowWindowResize->setChecked(true);
  windowLayout->addWidget(m_allowWindowResize);

  layout->addWidget(windowGroup);
  layout->addStretch();
}

void NMProjectSettingsPanel::setupTextTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Font Theme group
  auto *fontGroup = new QGroupBox(tr("Typography"), parent);
  auto *fontLayout = new QFormLayout(fontGroup);

  m_fontThemeCombo = new QComboBox(fontGroup);
  m_fontThemeCombo->addItems({tr("Default"), tr("Modern"), tr("Classic"),
                              tr("Retro Pixel"), tr("Elegant Serif"),
                              tr("Custom...")});
  m_fontThemeCombo->setToolTip(
      tr("Pre-configured font combinations for dialogue, UI, and names"));
  fontLayout->addRow(tr("Font Theme:"), m_fontThemeCombo);

  layout->addWidget(fontGroup);

  // Text Speed group
  auto *speedGroup = new QGroupBox(tr("Text Display"), parent);
  auto *speedLayout = new QFormLayout(speedGroup);

  auto *speedRow = new QWidget(speedGroup);
  auto *speedRowLayout = new QHBoxLayout(speedRow);
  speedRowLayout->setContentsMargins(0, 0, 0, 0);

  m_textSpeedSlider = new QSlider(Qt::Horizontal, speedRow);
  m_textSpeedSlider->setRange(1, 100);
  m_textSpeedSlider->setValue(50);
  m_textSpeedSlider->setToolTip(
      tr("Characters per second for typewriter effect"));

  m_textSpeedLabel = new QLabel("50 cps", speedRow);
  m_textSpeedLabel->setMinimumWidth(60);
  connect(m_textSpeedSlider, &QSlider::valueChanged, this, [this](int value) {
    m_textSpeedLabel->setText(QString("%1 cps").arg(value));
  });

  speedRowLayout->addWidget(m_textSpeedSlider, 1);
  speedRowLayout->addWidget(m_textSpeedLabel);
  speedLayout->addRow(tr("Text Speed:"), speedRow);

  m_enableTypewriter =
      new QCheckBox(tr("Enable Typewriter Effect"), speedGroup);
  m_enableTypewriter->setChecked(true);
  speedLayout->addRow("", m_enableTypewriter);

  m_autoAdvanceDelay = new QSpinBox(speedGroup);
  m_autoAdvanceDelay->setRange(500, 10000);
  m_autoAdvanceDelay->setSingleStep(100);
  m_autoAdvanceDelay->setValue(2000);
  m_autoAdvanceDelay->setSuffix(" ms");
  m_autoAdvanceDelay->setToolTip(
      tr("Delay before auto-advancing to next line"));
  speedLayout->addRow(tr("Auto-Advance Delay:"), m_autoAdvanceDelay);

  layout->addWidget(speedGroup);

  // Skip/History group
  auto *skipGroup = new QGroupBox(tr("Skip && History"), parent);
  auto *skipLayout = new QFormLayout(skipGroup);

  m_enableSkip = new QCheckBox(tr("Enable Skip Mode"), skipGroup);
  m_enableSkip->setChecked(true);
  skipLayout->addRow("", m_enableSkip);

  m_skipOnlyRead =
      new QCheckBox(tr("Skip Only Previously Read Text"), skipGroup);
  m_skipOnlyRead->setChecked(true);
  m_skipOnlyRead->setToolTip(tr("When enabled, unread text cannot be skipped"));
  skipLayout->addRow("", m_skipOnlyRead);

  m_historyLength = new QSpinBox(skipGroup);
  m_historyLength->setRange(10, 500);
  m_historyLength->setValue(100);
  m_historyLength->setSuffix(tr(" entries"));
  m_historyLength->setToolTip(
      tr("Maximum number of dialogue entries in backlog"));
  skipLayout->addRow(tr("History Length:"), m_historyLength);

  layout->addWidget(skipGroup);
  layout->addStretch();
}

void NMProjectSettingsPanel::setupLocalizationTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Language settings
  auto *langGroup = new QGroupBox(tr("Language Settings"), parent);
  auto *langLayout = new QFormLayout(langGroup);

  m_defaultLocaleCombo = new QComboBox(langGroup);
  m_defaultLocaleCombo->addItems(
      {"en (English)", "ja (Japanese)", "zh-CN (Chinese Simplified)",
       "zh-TW (Chinese Traditional)", "ko (Korean)", "ru (Russian)",
       "es (Spanish)", "fr (French)", "de (German)", "pt-BR (Portuguese)"});
  m_defaultLocaleCombo->setToolTip(tr("Primary language for the visual novel"));
  langLayout->addRow(tr("Default Language:"), m_defaultLocaleCombo);

  m_fallbackLocaleCombo = new QComboBox(langGroup);
  m_fallbackLocaleCombo->addItems(
      {"en (English)", "ja (Japanese)", "zh-CN (Chinese Simplified)",
       "zh-TW (Chinese Traditional)", "ko (Korean)", "ru (Russian)",
       "es (Spanish)", "fr (French)", "de (German)", "pt-BR (Portuguese)"});
  m_fallbackLocaleCombo->setToolTip(
      tr("Language to use when a translation is missing"));
  langLayout->addRow(tr("Fallback Language:"), m_fallbackLocaleCombo);

  m_availableLocales = new QLineEdit(langGroup);
  m_availableLocales->setPlaceholderText("en, ja, zh-CN, ko");
  m_availableLocales->setToolTip(
      tr("Comma-separated list of all available languages"));
  langLayout->addRow(tr("Available Languages:"), m_availableLocales);

  layout->addWidget(langGroup);

  // UI options
  auto *uiGroup = new QGroupBox(tr("UI Options"), parent);
  auto *uiLayout = new QVBoxLayout(uiGroup);

  m_showLanguageSelector =
      new QCheckBox(tr("Show Language Selector in Game Menu"), uiGroup);
  m_showLanguageSelector->setChecked(true);
  uiLayout->addWidget(m_showLanguageSelector);

  layout->addWidget(uiGroup);
  layout->addStretch();
}

void NMProjectSettingsPanel::setupBuildProfilesTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Profile selection
  auto *profileGroup = new QGroupBox(tr("Build Profiles"), parent);
  auto *profileLayout = new QVBoxLayout(profileGroup);

  auto *profileSelectRow = new QHBoxLayout();
  m_buildProfileCombo = new QComboBox(profileGroup);
  m_buildProfileCombo->addItems(
      {tr("Release"), tr("Debug"), tr("Demo"), tr("Steam"), tr("itch.io")});
  m_buildProfileCombo->setToolTip(tr("Select a build profile to view/edit"));
  profileSelectRow->addWidget(m_buildProfileCombo, 1);

  m_addProfileBtn = new QPushButton("+", profileGroup);
  m_addProfileBtn->setMaximumWidth(30);
  m_addProfileBtn->setToolTip(tr("Add new profile"));
  profileSelectRow->addWidget(m_addProfileBtn);

  m_removeProfileBtn = new QPushButton("-", profileGroup);
  m_removeProfileBtn->setMaximumWidth(30);
  m_removeProfileBtn->setToolTip(tr("Remove selected profile"));
  profileSelectRow->addWidget(m_removeProfileBtn);

  profileLayout->addLayout(profileSelectRow);

  auto *profileForm = new QFormLayout();

  m_profileName = new QLineEdit(profileGroup);
  m_profileName->setPlaceholderText(tr("Profile name"));
  profileForm->addRow(tr("Name:"), m_profileName);

  profileLayout->addLayout(profileForm);
  layout->addWidget(profileGroup);

  // Profile info
  auto *infoLabel = new QLabel(
      tr("Build profiles control export settings like:\n"
         "- Target platforms\n"
         "- Asset compression\n"
         "- Debug symbols\n"
         "- DRM/copy protection\n\n"
         "Configure detailed build settings in Build Settings panel."),
      parent);
  infoLabel->setWordWrap(true);
  infoLabel->setStyleSheet("color: #888; font-style: italic;");
  layout->addWidget(infoLabel);

  layout->addStretch();
}

void NMProjectSettingsPanel::setupWorkflowTab(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Header
  auto *header = new QLabel(tr("Workflow Mode"), parent);
  header->setStyleSheet("font-size: 14pt; font-weight: bold;");
  layout->addWidget(header);

  auto *description = new QLabel(
      tr("Choose the source of truth for your story content. "
         "This affects how editing works across Script Editor and Story Graph."),
      parent);
  description->setWordWrap(true);
  description->setStyleSheet("color: #a0a0a0;");
  layout->addWidget(description);

  layout->addSpacing(16);

  // Mode Selection with Radio Buttons
  auto *modeGroup = new QGroupBox(tr("Select Mode"), parent);
  auto *modeLayout = new QVBoxLayout(modeGroup);

  m_workflowButtonGroup = new QButtonGroup(this);

  m_scriptModeRadio = new QRadioButton(tr("Script Mode (Code-First)"), modeGroup);
  m_scriptModeRadio->setToolTip(
      tr("NMScript files are the source of truth. Story Graph is read-only."));
  m_graphModeRadio = new QRadioButton(tr("Graph Mode (Visual-First)"), modeGroup);
  m_graphModeRadio->setToolTip(
      tr("Story Graph visual data is the source of truth. Script files are for export/backup."));
  m_mixedModeRadio = new QRadioButton(tr("Mixed Mode (Hybrid)"), modeGroup);
  m_mixedModeRadio->setToolTip(
      tr("Script provides base content, Graph can override. Graph wins on conflicts."));

  // Add to button group for mutual exclusivity
  m_workflowButtonGroup->addButton(m_scriptModeRadio, 1);  // Script = index 1
  m_workflowButtonGroup->addButton(m_graphModeRadio, 0);   // Graph = index 0 (Visual-First)
  m_workflowButtonGroup->addButton(m_mixedModeRadio, 2);   // Mixed = index 2

  modeLayout->addWidget(m_graphModeRadio);   // Visual-First first (most common)
  modeLayout->addWidget(m_scriptModeRadio);  // Code-First second
  modeLayout->addWidget(m_mixedModeRadio);   // Hybrid third

  layout->addWidget(modeGroup);

  // Mode Description (dynamic based on selection)
  m_workflowDescription = new QLabel(parent);
  m_workflowDescription->setWordWrap(true);
  m_workflowDescription->setStyleSheet(
      "background-color: #2d2d2d; "
      "padding: 12px; "
      "border-radius: 4px; "
      "color: #e0e0e0;");
  layout->addWidget(m_workflowDescription);

  // Warning Box
  m_workflowWarningLabel = new QLabel(
      tr("<b>⚠️ Warning:</b> Changing workflow mode may affect how your project loads. "
         "Make sure to backup your project before switching modes."),
      parent);
  m_workflowWarningLabel->setWordWrap(true);
  m_workflowWarningLabel->setStyleSheet(
      "background-color: #3d2d1f; "
      "border-left: 4px solid #ff9800; "
      "padding: 12px; "
      "color: #ffc107;");
  m_workflowWarningLabel->setTextFormat(Qt::RichText);
  layout->addWidget(m_workflowWarningLabel);

  layout->addSpacing(8);

  // Per-scene override option
  m_allowMixedWorkflows =
      new QCheckBox(tr("Allow per-scene workflow override"), parent);
  m_allowMixedWorkflows->setChecked(true);
  m_allowMixedWorkflows->setToolTip(
      tr("When enabled, individual scenes can use a different workflow than "
         "the default"));
  layout->addWidget(m_allowMixedWorkflows);

  // Connect radio buttons to update description
  connect(m_workflowButtonGroup,
          QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
          [this]([[maybe_unused]] QAbstractButton *button) {
            updateWorkflowDescription();
            onSettingChanged();
          });

  // Set initial selection to Visual-First (Graph mode)
  m_graphModeRadio->setChecked(true);
  updateWorkflowDescription();

  // Workflow help section
  auto *helpGroup = new QGroupBox(tr("Workflow Comparison"), parent);
  auto *helpLayout = new QVBoxLayout(helpGroup);

  auto *helpLabel =
      new QLabel(tr("<b>Graph Mode (Visual-First):</b> Create scenes visually with embedded "
                    "dialogue graphs. "
                    "Best for rapid prototyping and non-programmers.<br><br>"
                    "<b>Script Mode (Code-First):</b> Define scenes in NMScript with "
                    "optional visual layout. "
                    "Best for version control and complex logic.<br><br>"
                    "<b>Mixed Mode (Hybrid):</b> Mix both approaches per scene. "
                    "Best for team collaboration and flexibility."),
                 helpGroup);
  helpLabel->setWordWrap(true);
  helpLabel->setTextFormat(Qt::RichText);
  helpLayout->addWidget(helpLabel);

  layout->addWidget(helpGroup);
  layout->addStretch();
}

void NMProjectSettingsPanel::updateWorkflowDescription() {
  if (!m_workflowDescription)
    return;

  QString desc;
  if (m_scriptModeRadio && m_scriptModeRadio->isChecked()) {
    desc = tr(
        "<b>Script Mode (Recommended for programmers)</b><br><br>"
        "✅ <b>Source of Truth:</b> .nms script files<br>"
        "✅ <b>Story Graph:</b> Read-only visualization<br>"
        "✅ <b>Best for:</b> Text-based editing, full control, version control friendly<br><br>"
        "Use \"Sync to Graph\" to update visual representation after script changes.");
  } else if (m_graphModeRadio && m_graphModeRadio->isChecked()) {
    desc = tr(
        "<b>Graph Mode (Recommended for visual designers)</b><br><br>"
        "✅ <b>Source of Truth:</b> Story Graph visual editor<br>"
        "✅ <b>Script Files:</b> Read-only export/backup<br>"
        "✅ <b>Best for:</b> Visual editing, non-programmers, drag-and-drop workflow<br><br>"
        "Use \"Sync to Script\" to export graph to .nms files for backup.");
  } else if (m_mixedModeRadio && m_mixedModeRadio->isChecked()) {
    desc = tr(
        "<b>Mixed Mode (Advanced users only)</b><br><br>"
        "⚠️ <b>Source of Truth:</b> Scripts provide base, Graph overrides<br>"
        "⚠️ <b>Conflict Resolution:</b> Graph scenes override script scenes with same name<br>"
        "⚠️ <b>Best for:</b> Teams with mixed skill levels, gradual migration<br><br>"
        "<b>Important:</b> Avoid duplicate scene names between Script and Graph.");
  }
  m_workflowDescription->setText(desc);
  m_workflowDescription->setTextFormat(Qt::RichText);
}

void NMProjectSettingsPanel::connectSignals() {
  // Connect all setting widgets to onSettingChanged
  auto connectCombo = [this](QComboBox *combo) {
    if (combo) {
      connect(combo, &QComboBox::currentIndexChanged, this,
              &NMProjectSettingsPanel::onSettingChanged);
    }
  };

  auto connectSpin = [this](QSpinBox *spin) {
    if (spin) {
      connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
              &NMProjectSettingsPanel::onSettingChanged);
    }
  };

  auto connectCheck = [this](QCheckBox *check) {
    if (check) {
      connect(check, &QCheckBox::toggled, this,
              &NMProjectSettingsPanel::onSettingChanged);
    }
  };

  auto connectLine = [this](QLineEdit *line) {
    if (line) {
      connect(line, &QLineEdit::textChanged, this,
              &NMProjectSettingsPanel::onSettingChanged);
    }
  };

  // Workflow (note: radio buttons are connected in setupWorkflowTab with
  // description update)
  connectCheck(m_allowMixedWorkflows);

  // Display
  connectCombo(m_resolutionCombo);
  connectCombo(m_aspectRatioMode);
  connectSpin(m_safeAreaTop);
  connectSpin(m_safeAreaBottom);
  connectSpin(m_safeAreaLeft);
  connectSpin(m_safeAreaRight);
  connectCheck(m_fullscreenDefault);
  connectCheck(m_allowWindowResize);

  // Text
  connectCombo(m_fontThemeCombo);
  if (m_textSpeedSlider) {
    connect(m_textSpeedSlider, &QSlider::valueChanged, this,
            &NMProjectSettingsPanel::onSettingChanged);
  }
  connectSpin(m_autoAdvanceDelay);
  connectCheck(m_enableTypewriter);
  connectCheck(m_enableSkip);
  connectCheck(m_skipOnlyRead);
  connectSpin(m_historyLength);

  // Localization
  connectCombo(m_defaultLocaleCombo);
  connectCombo(m_fallbackLocaleCombo);
  connectLine(m_availableLocales);
  connectCheck(m_showLanguageSelector);

  // Build profiles
  connectCombo(m_buildProfileCombo);
  connectLine(m_profileName);
}

void NMProjectSettingsPanel::onSettingChanged() {
  m_hasUnsavedChanges = true;
  updateApplyButton();
}

void NMProjectSettingsPanel::updateApplyButton() {
  if (m_applyButton) {
    m_applyButton->setEnabled(m_hasUnsavedChanges);
  }
  if (m_revertButton) {
    m_revertButton->setEnabled(m_hasUnsavedChanges);
  }
}

void NMProjectSettingsPanel::onApplyClicked() {
  saveToProject();
  m_hasUnsavedChanges = false;
  updateApplyButton();
  emit settingsChanged();
}

void NMProjectSettingsPanel::onRevertClicked() {
  loadFromProject();
  m_hasUnsavedChanges = false;
  updateApplyButton();
}

void NMProjectSettingsPanel::loadFromProject() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  const auto &meta = pm.getMetadata();

  // Load workflow mode from playbackSourceMode (issue #100, #125)
  // Use radio buttons instead of combo box
  {
    QSignalBlocker blocker(m_workflowButtonGroup);
  // Load workflow mode from playbackSourceMode (issue #100)
  // Use QSignalBlocker to prevent signal loops during programmatic updates
  if (m_workflowCombo) {
    int workflowIndex = 0; // Default to Visual-First (Graph)
    switch (meta.playbackSourceMode) {
    case PlaybackSourceMode::Script:
      if (m_scriptModeRadio) {
        m_scriptModeRadio->setChecked(true);
      }
      break;
    case PlaybackSourceMode::Graph:
      if (m_graphModeRadio) {
        m_graphModeRadio->setChecked(true);
      }
      break;
    case PlaybackSourceMode::Mixed:
      if (m_mixedModeRadio) {
        m_mixedModeRadio->setChecked(true);
      }
      break;
    }
    // Block signals to prevent feedback loop with PlayToolbar
    QSignalBlocker blocker(m_workflowCombo);
    m_workflowCombo->setCurrentIndex(workflowIndex);
    updateWorkflowDescription();
  }

  // Load resolution
  // Use signal blocker to prevent unintended signal emission during load
  if (m_resolutionCombo) {
    QSignalBlocker blocker(m_resolutionCombo);
    QString resolution = QString::fromStdString(meta.targetResolution);
    int idx = m_resolutionCombo->findText(resolution, Qt::MatchStartsWith);
    if (idx >= 0) {
      m_resolutionCombo->setCurrentIndex(idx);
    }
  }

  // Load fullscreen default
  if (m_fullscreenDefault) {
    QSignalBlocker blocker(m_fullscreenDefault);
    m_fullscreenDefault->setChecked(meta.fullscreenDefault);
  }

  // Load default locale
  if (m_defaultLocaleCombo) {
    QSignalBlocker blocker(m_defaultLocaleCombo);
    QString locale = QString::fromStdString(meta.defaultLocale);
    int idx = m_defaultLocaleCombo->findText(locale, Qt::MatchStartsWith);
    if (idx >= 0) {
      m_defaultLocaleCombo->setCurrentIndex(idx);
    }
  }
}

void NMProjectSettingsPanel::saveToProject() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  auto meta = pm.getMetadata();

  // Save workflow mode to playbackSourceMode (issue #100, #125)
  // Determine mode from radio buttons
  PlaybackSourceMode newMode = PlaybackSourceMode::Script; // Default
  if (m_graphModeRadio && m_graphModeRadio->isChecked()) {
    newMode = PlaybackSourceMode::Graph;
  } else if (m_scriptModeRadio && m_scriptModeRadio->isChecked()) {
    newMode = PlaybackSourceMode::Script;
  } else if (m_mixedModeRadio && m_mixedModeRadio->isChecked()) {
    newMode = PlaybackSourceMode::Mixed;
  }

  // Check if mode actually changed
  const bool modeChanged = (meta.playbackSourceMode != newMode);
  meta.playbackSourceMode = newMode;

  // Save resolution
  if (m_resolutionCombo) {
    QString text = m_resolutionCombo->currentText();
    const qsizetype spacePos = text.indexOf(' ');
    QString resolution = spacePos > 0 ? text.left(spacePos) : text;
    meta.targetResolution = resolution.toStdString();
  }

  // Save fullscreen default
  if (m_fullscreenDefault) {
    meta.fullscreenDefault = m_fullscreenDefault->isChecked();
  }

  // Save default locale
  if (m_defaultLocaleCombo) {
    QString text = m_defaultLocaleCombo->currentText();
    const qsizetype spacePos = text.indexOf(' ');
    QString locale = spacePos > 0 ? text.left(spacePos) : text;
    meta.defaultLocale = locale.toStdString();
  }

  pm.setMetadata(meta);
  pm.saveProject();

  // Emit workflow mode changed signal if the mode was changed (issue #125)
  // This allows other panels (like PlayToolbar) to update their UI
  if (modeChanged) {
    emit workflowModeChanged(newMode);
  }
}

void NMProjectSettingsPanel::setWorkflowMode(PlaybackSourceMode mode) {
  // Update the radio buttons to reflect the new mode
  // This is called when PlayToolbar changes the mode
  QSignalBlocker blocker(m_workflowButtonGroup);

  switch (mode) {
  case PlaybackSourceMode::Script:
    if (m_scriptModeRadio) {
      m_scriptModeRadio->setChecked(true);
    }
    break;
  case PlaybackSourceMode::Graph:
    if (m_graphModeRadio) {
      m_graphModeRadio->setChecked(true);
    }
    break;
  case PlaybackSourceMode::Mixed:
    if (m_mixedModeRadio) {
      m_mixedModeRadio->setChecked(true);
    }
    break;
  }

  updateWorkflowDescription();

  // Note: We don't call onSettingChanged() here because this is an external
  // update. The project is already saved by the PlayToolbar.
}

} // namespace NovelMind::editor::qt
