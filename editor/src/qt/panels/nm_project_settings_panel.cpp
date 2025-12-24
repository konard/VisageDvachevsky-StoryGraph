#include "NovelMind/editor/qt/panels/nm_project_settings_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"

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
  m_aspectRatioMode->setToolTip(
      tr("How to handle aspect ratio differences:\n"
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
  m_textSpeedSlider->setToolTip(tr("Characters per second for typewriter effect"));

  m_textSpeedLabel = new QLabel("50 cps", speedRow);
  m_textSpeedLabel->setMinimumWidth(60);
  connect(m_textSpeedSlider, &QSlider::valueChanged, this,
          [this](int value) { m_textSpeedLabel->setText(QString("%1 cps").arg(value)); });

  speedRowLayout->addWidget(m_textSpeedSlider, 1);
  speedRowLayout->addWidget(m_textSpeedLabel);
  speedLayout->addRow(tr("Text Speed:"), speedRow);

  m_enableTypewriter = new QCheckBox(tr("Enable Typewriter Effect"), speedGroup);
  m_enableTypewriter->setChecked(true);
  speedLayout->addRow("", m_enableTypewriter);

  m_autoAdvanceDelay = new QSpinBox(speedGroup);
  m_autoAdvanceDelay->setRange(500, 10000);
  m_autoAdvanceDelay->setSingleStep(100);
  m_autoAdvanceDelay->setValue(2000);
  m_autoAdvanceDelay->setSuffix(" ms");
  m_autoAdvanceDelay->setToolTip(tr("Delay before auto-advancing to next line"));
  speedLayout->addRow(tr("Auto-Advance Delay:"), m_autoAdvanceDelay);

  layout->addWidget(speedGroup);

  // Skip/History group
  auto *skipGroup = new QGroupBox(tr("Skip && History"), parent);
  auto *skipLayout = new QFormLayout(skipGroup);

  m_enableSkip = new QCheckBox(tr("Enable Skip Mode"), skipGroup);
  m_enableSkip->setChecked(true);
  skipLayout->addRow("", m_enableSkip);

  m_skipOnlyRead = new QCheckBox(tr("Skip Only Previously Read Text"), skipGroup);
  m_skipOnlyRead->setChecked(true);
  m_skipOnlyRead->setToolTip(tr("When enabled, unread text cannot be skipped"));
  skipLayout->addRow("", m_skipOnlyRead);

  m_historyLength = new QSpinBox(skipGroup);
  m_historyLength->setRange(10, 500);
  m_historyLength->setValue(100);
  m_historyLength->setSuffix(tr(" entries"));
  m_historyLength->setToolTip(tr("Maximum number of dialogue entries in backlog"));
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

  m_showLanguageSelector = new QCheckBox(tr("Show Language Selector in Game Menu"), uiGroup);
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
  m_buildProfileCombo->addItems({tr("Release"), tr("Debug"), tr("Demo"),
                                  tr("Steam"), tr("itch.io")});
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
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Development Workflow group
  auto *workflowGroup = new QGroupBox(tr("Development Workflow"), parent);
  auto *workflowLayout = new QFormLayout(workflowGroup);

  m_workflowCombo = new QComboBox(workflowGroup);
  m_workflowCombo->addItems({
      tr("Visual-First"),
      tr("Code-First"),
      tr("Hybrid")
  });
  m_workflowCombo->setToolTip(
      tr("Choose the primary development approach for scene creation"));
  workflowLayout->addRow(tr("Default Workflow:"), m_workflowCombo);

  m_allowMixedWorkflows = new QCheckBox(
      tr("Allow per-scene workflow override"), workflowGroup);
  m_allowMixedWorkflows->setChecked(true);
  m_allowMixedWorkflows->setToolTip(
      tr("When enabled, individual scenes can use a different workflow than the default"));
  workflowLayout->addRow("", m_allowMixedWorkflows);

  layout->addWidget(workflowGroup);

  // Workflow description
  m_workflowDescription = new QLabel(parent);
  m_workflowDescription->setWordWrap(true);
  m_workflowDescription->setStyleSheet("color: #888; padding: 8px; background: #2a2a2a; border-radius: 4px;");
  updateWorkflowDescription();
  layout->addWidget(m_workflowDescription);

  // Connect workflow combo to update description
  connect(m_workflowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
    updateWorkflowDescription();
    onSettingChanged();
  });

  // Workflow help section
  auto *helpGroup = new QGroupBox(tr("Workflow Comparison"), parent);
  auto *helpLayout = new QVBoxLayout(helpGroup);

  auto *helpLabel = new QLabel(
      tr("<b>Visual-First:</b> Create scenes visually with embedded dialogue graphs. "
         "Best for rapid prototyping and non-programmers.<br><br>"
         "<b>Code-First:</b> Define scenes in NMScript with optional visual layout. "
         "Best for version control and complex logic.<br><br>"
         "<b>Hybrid:</b> Mix both approaches per scene. "
         "Best for team collaboration and flexibility."),
      helpGroup);
  helpLabel->setWordWrap(true);
  helpLabel->setTextFormat(Qt::RichText);
  helpLayout->addWidget(helpLabel);

  layout->addWidget(helpGroup);
  layout->addStretch();
}

void NMProjectSettingsPanel::updateWorkflowDescription() {
  if (!m_workflowDescription || !m_workflowCombo) return;

  int idx = m_workflowCombo->currentIndex();
  QString desc;
  switch (idx) {
    case 0: // Visual-First
      desc = tr("Visual-First: Create scenes visually using the Scene View and "
                "embedded dialogue graphs. Double-click a Scene Node to edit its "
                "visual layout, or right-click and choose 'Edit Dialogue Flow' to "
                "add dialogue sequences. No coding required.");
      break;
    case 1: // Code-First
      desc = tr("Code-First: Define dialogue and game logic in NMScript (.nms) files. "
                "Create Scene Nodes in the Story Graph to define structure, then "
                "right-click and choose 'Open Script' to write the scene logic. "
                "Optionally configure visual layout in Scene View.");
      break;
    case 2: // Hybrid
      desc = tr("Hybrid: Use both Visual-First and Code-First approaches as needed. "
                "Simple scenes can use embedded dialogue graphs, while complex "
                "scenes with branching logic can use NMScript. Each scene can "
                "choose its own workflow.");
      break;
  }
  m_workflowDescription->setText(desc);
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

  // Workflow (note: m_workflowCombo is connected in setupWorkflowTab with description update)
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

  // Load resolution
  if (m_resolutionCombo) {
    QString resolution = QString::fromStdString(meta.targetResolution);
    int idx = m_resolutionCombo->findText(resolution, Qt::MatchStartsWith);
    if (idx >= 0) {
      m_resolutionCombo->setCurrentIndex(idx);
    }
  }

  // Load fullscreen default
  if (m_fullscreenDefault) {
    m_fullscreenDefault->setChecked(meta.fullscreenDefault);
  }

  // Load default locale
  if (m_defaultLocaleCombo) {
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
}

} // namespace NovelMind::editor::qt
