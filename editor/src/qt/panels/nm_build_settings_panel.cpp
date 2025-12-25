/**
 * @file nm_build_settings_panel.cpp
 * @brief Build Settings Panel implementation for NovelMind Editor
 *
 * Provides UI for:
 * - Build configuration (platform, profile, output)
 * - Build size preview and warnings
 * - Build execution with progress tracking
 * - Diagnostics and log viewing
 */

#include "NovelMind/editor/qt/panels/nm_build_settings_panel.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/build_system.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

namespace NovelMind::editor::qt {

// ============================================================================
// Constructor / Destructor
// ============================================================================

NMBuildSettingsPanel::NMBuildSettingsPanel(QWidget *parent)
    : NMDockPanel("Build Settings", parent) {}

NMBuildSettingsPanel::~NMBuildSettingsPanel() = default;

// ============================================================================
// Lifecycle
// ============================================================================

void NMBuildSettingsPanel::onInitialize() { setupUI(); }

void NMBuildSettingsPanel::onShutdown() {
  // Cancel any ongoing build
  if (m_buildStatus == BuildStatus::Preparing ||
      m_buildStatus == BuildStatus::Copying ||
      m_buildStatus == BuildStatus::Compiling ||
      m_buildStatus == BuildStatus::Packaging) {
    cancelBuild();
  }
}

void NMBuildSettingsPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // Update UI based on build status
  if (m_buildButton) {
    m_buildButton->setEnabled(m_buildStatus == BuildStatus::Idle ||
                              m_buildStatus == BuildStatus::Complete ||
                              m_buildStatus == BuildStatus::Failed ||
                              m_buildStatus == BuildStatus::Cancelled);
  }

  if (m_cancelButton) {
    m_cancelButton->setEnabled(m_buildStatus == BuildStatus::Preparing ||
                               m_buildStatus == BuildStatus::Copying ||
                               m_buildStatus == BuildStatus::Compiling ||
                               m_buildStatus == BuildStatus::Packaging);
  }
}

// ============================================================================
// UI Setup
// ============================================================================

void NMBuildSettingsPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget());
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create tab widget for different sections
  m_tabWidget = new QTabWidget(contentWidget());

  // Settings Tab
  QWidget *settingsTab = new QWidget();
  setupBuildSettings();
  m_tabWidget->addTab(settingsTab, "Settings");

  // Warnings Tab
  QWidget *warningsTab = new QWidget();
  setupWarningsTab();
  m_tabWidget->addTab(warningsTab, "Warnings");

  // Log Tab
  QWidget *logTab = new QWidget();
  setupLogTab();
  m_tabWidget->addTab(logTab, "Log");

  mainLayout->addWidget(m_tabWidget);

  // Build status section at bottom
  QWidget *statusWidget = new QWidget(contentWidget());
  QVBoxLayout *statusLayout = new QVBoxLayout(statusWidget);
  statusLayout->setContentsMargins(8, 8, 8, 8);
  statusLayout->setSpacing(8);

  // Progress bar
  m_progressBar = new QProgressBar(statusWidget);
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  m_progressBar->setTextVisible(true);
  m_progressBar->setFormat("%p% - Idle");
  statusLayout->addWidget(m_progressBar);

  // Status label
  m_statusLabel = new QLabel("Ready to build", statusWidget);
  m_statusLabel->setStyleSheet("color: #888;");
  statusLayout->addWidget(m_statusLabel);

  // Button row
  QHBoxLayout *buttonLayout = new QHBoxLayout();

  m_buildButton = new QPushButton("Build Project", statusWidget);
  m_buildButton->setMinimumHeight(36);
  m_buildButton->setStyleSheet(
      "QPushButton { background-color: #0078d4; color: white; font-weight: bold; "
      "border-radius: 4px; padding: 8px 16px; }"
      "QPushButton:hover { background-color: #1084d8; }"
      "QPushButton:pressed { background-color: #006cbd; }"
      "QPushButton:disabled { background-color: #555; color: #888; }");
  connect(m_buildButton, &QPushButton::clicked, this,
          &NMBuildSettingsPanel::onBuildClicked);
  buttonLayout->addWidget(m_buildButton);

  m_cancelButton = new QPushButton("Cancel", statusWidget);
  m_cancelButton->setMinimumHeight(36);
  m_cancelButton->setEnabled(false);
  m_cancelButton->setStyleSheet(
      "QPushButton { background-color: #d83b01; color: white; font-weight: bold; "
      "border-radius: 4px; padding: 8px 16px; }"
      "QPushButton:hover { background-color: #ea4a12; }"
      "QPushButton:pressed { background-color: #c73000; }"
      "QPushButton:disabled { background-color: #555; color: #888; }");
  connect(m_cancelButton, &QPushButton::clicked, this,
          &NMBuildSettingsPanel::onCancelClicked);
  buttonLayout->addWidget(m_cancelButton);

  statusLayout->addLayout(buttonLayout);
  mainLayout->addWidget(statusWidget);

  // Initialize with default values
  updateSizePreview();
}

void NMBuildSettingsPanel::setupBuildSettings() {
  QWidget *settingsTab = m_tabWidget->widget(0);
  QVBoxLayout *layout = new QVBoxLayout(settingsTab);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(12);

  // Platform & Profile Section
  QGroupBox *platformGroup = new QGroupBox("Platform && Profile", settingsTab);
  QFormLayout *platformLayout = new QFormLayout(platformGroup);
  platformLayout->setSpacing(8);

  m_platformSelector = new QComboBox(platformGroup);
  m_platformSelector->addItems(
      {"Windows", "Linux", "macOS", "Web (WASM)", "Android", "iOS"});
  m_platformSelector->setCurrentIndex(0);
  connect(m_platformSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMBuildSettingsPanel::onPlatformChanged);
  platformLayout->addRow("Target Platform:", m_platformSelector);

  m_profileSelector = new QComboBox(platformGroup);
  m_profileSelector->addItems({"Debug", "Release", "Distribution"});
  m_profileSelector->setCurrentIndex(1); // Default to Release
  connect(m_profileSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMBuildSettingsPanel::onProfileChanged);
  platformLayout->addRow("Build Profile:", m_profileSelector);

  layout->addWidget(platformGroup);

  // Output Section
  QGroupBox *outputGroup = new QGroupBox("Output", settingsTab);
  QVBoxLayout *outputMainLayout = new QVBoxLayout(outputGroup);
  QFormLayout *outputFormLayout = new QFormLayout();
  outputFormLayout->setSpacing(8);

  // Output directory with browse button
  QHBoxLayout *outputPathLayout = new QHBoxLayout();
  m_outputPathEdit = new QLineEdit("./build/", outputGroup);
  m_outputPathEdit->setPlaceholderText("Select output directory...");
  outputPathLayout->addWidget(m_outputPathEdit);

  m_browseBtn = new QPushButton("Browse...", outputGroup);
  connect(m_browseBtn, &QPushButton::clicked, this,
          &NMBuildSettingsPanel::onBrowseOutput);
  outputPathLayout->addWidget(m_browseBtn);

  outputFormLayout->addRow("Output Directory:", outputPathLayout);

  // Build name
  QLineEdit *buildNameEdit = new QLineEdit("MyVisualNovel", outputGroup);
  outputFormLayout->addRow("Build Name:", buildNameEdit);

  // Version
  QLineEdit *versionEdit = new QLineEdit("1.0.0", outputGroup);
  outputFormLayout->addRow("Version:", versionEdit);

  outputMainLayout->addLayout(outputFormLayout);
  layout->addWidget(outputGroup);

  // Build Options Section
  QGroupBox *optionsGroup = new QGroupBox("Build Options", settingsTab);
  QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);
  optionsLayout->setSpacing(6);

  m_compressAssets = new QCheckBox("Compress Assets", optionsGroup);
  m_compressAssets->setChecked(true);
  m_compressAssets->setToolTip("Apply compression to reduce pack file size");
  optionsLayout->addWidget(m_compressAssets);

  m_debugBuild = new QCheckBox("Include Debug Info", optionsGroup);
  m_debugBuild->setChecked(false);
  m_debugBuild->setToolTip("Include debug symbols and source maps");
  optionsLayout->addWidget(m_debugBuild);

  QCheckBox *stripUnused = new QCheckBox("Strip Unused Assets", optionsGroup);
  stripUnused->setChecked(true);
  stripUnused->setToolTip("Remove assets not referenced by any script or scene");
  optionsLayout->addWidget(stripUnused);

  QCheckBox *encryptAssets = new QCheckBox("Encrypt Assets", optionsGroup);
  encryptAssets->setChecked(true);
  encryptAssets->setToolTip("Encrypt resource packs (AES-256-GCM)");
  optionsLayout->addWidget(encryptAssets);

  m_includeDevAssets = new QCheckBox("Include Development Assets", optionsGroup);
  m_includeDevAssets->setChecked(false);
  m_includeDevAssets->setToolTip("Include test scenes and debug assets");
  optionsLayout->addWidget(m_includeDevAssets);

  layout->addWidget(optionsGroup);

  // Size Preview Section
  QGroupBox *sizeGroup = new QGroupBox("Estimated Build Size", settingsTab);
  QVBoxLayout *sizeLayout = new QVBoxLayout(sizeGroup);
  QFormLayout *sizeFormLayout = new QFormLayout();
  sizeFormLayout->setSpacing(4);

  m_totalSizeLabel = new QLabel("-- MB", sizeGroup);
  m_totalSizeLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
  sizeFormLayout->addRow("Total Size:", m_totalSizeLabel);

  m_assetsSizeLabel = new QLabel("-- MB", sizeGroup);
  sizeFormLayout->addRow("Assets:", m_assetsSizeLabel);

  m_imagesSizeLabel = new QLabel("-- MB", sizeGroup);
  sizeFormLayout->addRow("Images:", m_imagesSizeLabel);

  m_audioSizeLabel = new QLabel("-- MB", sizeGroup);
  sizeFormLayout->addRow("Audio:", m_audioSizeLabel);

  m_scriptsSizeLabel = new QLabel("-- KB", sizeGroup);
  sizeFormLayout->addRow("Scripts:", m_scriptsSizeLabel);

  m_fileCountLabel = new QLabel("-- files", sizeGroup);
  sizeFormLayout->addRow("File Count:", m_fileCountLabel);

  sizeLayout->addLayout(sizeFormLayout);

  m_refreshPreviewBtn = new QPushButton("Refresh Preview", sizeGroup);
  connect(m_refreshPreviewBtn, &QPushButton::clicked, this,
          &NMBuildSettingsPanel::onRefreshPreview);
  sizeLayout->addWidget(m_refreshPreviewBtn);

  layout->addWidget(sizeGroup);

  layout->addStretch();
}

void NMBuildSettingsPanel::setupWarningsTab() {
  QWidget *warningsTab = m_tabWidget->widget(1);
  QVBoxLayout *layout = new QVBoxLayout(warningsTab);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Warning count label
  m_warningCountLabel = new QLabel("No warnings", warningsTab);
  m_warningCountLabel->setStyleSheet("color: #888; font-style: italic;");
  layout->addWidget(m_warningCountLabel);

  // Warnings tree
  m_warningsTree = new QTreeWidget(warningsTab);
  m_warningsTree->setHeaderLabels({"Type", "Message", "Location"});
  m_warningsTree->setColumnWidth(0, 120);
  m_warningsTree->setColumnWidth(1, 300);
  m_warningsTree->setAlternatingRowColors(true);
  m_warningsTree->setRootIsDecorated(false);
  connect(m_warningsTree, &QTreeWidget::itemDoubleClicked,
          [this](QTreeWidgetItem *item, int) {
            if (item) {
              int row = m_warningsTree->indexOfTopLevelItem(item);
              onWarningDoubleClicked(row);
            }
          });
  layout->addWidget(m_warningsTree);

  // Scan button
  QPushButton *scanBtn = new QPushButton("Scan for Warnings", warningsTab);
  connect(scanBtn, &QPushButton::clicked, this,
          &NMBuildSettingsPanel::updateWarnings);
  layout->addWidget(scanBtn);
}

void NMBuildSettingsPanel::setupLogTab() {
  QWidget *logTab = m_tabWidget->widget(2);
  QVBoxLayout *layout = new QVBoxLayout(logTab);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Log output
  m_logOutput = new QPlainTextEdit(logTab);
  m_logOutput->setReadOnly(true);
  m_logOutput->setFont(QFont("Consolas", 9));
  m_logOutput->setStyleSheet(
      "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
  m_logOutput->setPlaceholderText("Build log will appear here...");
  layout->addWidget(m_logOutput);

  // Clear button
  m_clearLogBtn = new QPushButton("Clear Log", logTab);
  connect(m_clearLogBtn, &QPushButton::clicked,
          [this]() { m_logOutput->clear(); });
  layout->addWidget(m_clearLogBtn);
}

// ============================================================================
// Build Operations
// ============================================================================

BuildSizeInfo NMBuildSettingsPanel::calculateBuildSize() const {
  BuildSizeInfo info;

  // Get project path from somewhere (placeholder)
  QString projectPath = "."; // In real implementation, get from project manager

  try {
    fs::path projectDir(projectPath.toStdString());

    if (!fs::exists(projectDir)) {
      return info;
    }

    // Scan assets directory
    fs::path assetsDir = projectDir / "assets";
    if (fs::exists(assetsDir)) {
      for (const auto &entry : fs::recursive_directory_iterator(assetsDir)) {
        if (entry.is_regular_file()) {
          info.fileCount++;
          auto size = static_cast<qint64>(entry.file_size());
          info.assetsSize += size;
          info.totalSize += size;

          std::string ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

          if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
            info.imagesSize += size;
          } else if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
            info.audioSize += size;
          } else if (ext == ".ttf" || ext == ".otf") {
            info.fontsSize += size;
          } else {
            info.otherSize += size;
          }
        }
      }
    }

    // Scan scripts directory
    fs::path scriptsDir = projectDir / "scripts";
    if (fs::exists(scriptsDir)) {
      for (const auto &entry : fs::recursive_directory_iterator(scriptsDir)) {
        if (entry.is_regular_file()) {
          std::string ext = entry.path().extension().string();
          if (ext == ".nms" || ext == ".nmscript") {
            info.fileCount++;
            auto size = static_cast<qint64>(entry.file_size());
            info.scriptsSize += size;
            info.totalSize += size;
          }
        }
      }
    }

  } catch (const fs::filesystem_error &) {
    // Ignore errors during preview
  }

  return info;
}

QList<BuildWarning> NMBuildSettingsPanel::scanForWarnings() const {
  QList<BuildWarning> warnings;

  // Get project path
  QString projectPath = ".";

  try {
    fs::path projectDir(projectPath.toStdString());

    // Check for missing project.json
    if (!fs::exists(projectDir / "project.json")) {
      BuildWarning w;
      w.type = BuildWarningType::MissingAsset;
      w.message = "Missing project.json configuration file";
      w.filePath = QString::fromStdString((projectDir / "project.json").string());
      w.isCritical = true;
      warnings.append(w);
    }

    // Check for missing required directories
    std::vector<std::string> requiredDirs = {"assets", "scripts"};
    for (const auto &dir : requiredDirs) {
      if (!fs::exists(projectDir / dir)) {
        BuildWarning w;
        w.type = BuildWarningType::MissingAsset;
        w.message = QString("Missing required directory: %1").arg(QString::fromStdString(dir));
        w.filePath = QString::fromStdString((projectDir / dir).string());
        w.isCritical = true;
        warnings.append(w);
      }
    }

    // Check for large files
    fs::path assetsDir = projectDir / "assets";
    if (fs::exists(assetsDir)) {
      for (const auto &entry : fs::recursive_directory_iterator(assetsDir)) {
        if (entry.is_regular_file()) {
          auto size = entry.file_size();
          if (size > 50 * 1024 * 1024) { // > 50MB
            BuildWarning w;
            w.type = BuildWarningType::LargeFile;
            w.message = QString("Large file detected (%1 MB)")
                            .arg(size / (1024 * 1024));
            w.filePath = QString::fromStdString(entry.path().string());
            w.isCritical = false;
            warnings.append(w);
          }
        }
      }
    }

  } catch (const fs::filesystem_error &) {
    // Ignore errors during scanning
  }

  return warnings;
}

void NMBuildSettingsPanel::startBuild() {
  m_buildStatus = BuildStatus::Preparing;

  // Create build system
  auto buildSystem = std::make_shared<editor::BuildSystem>();

  // Configure build
  editor::BuildConfig config;
  config.projectPath = "."; // Get from project manager
  config.outputPath = m_outputPathEdit->text().toStdString();
  config.executableName = "MyVisualNovel"; // Get from UI

  // Set platform
  int platformIndex = m_platformSelector->currentIndex();
  switch (platformIndex) {
  case 0:
    config.platform = editor::BuildPlatform::Windows;
    break;
  case 1:
    config.platform = editor::BuildPlatform::Linux;
    break;
  case 2:
    config.platform = editor::BuildPlatform::MacOS;
    break;
  default:
    config.platform = editor::BuildPlatform::Linux;
    break;
  }

  // Set build type based on profile
  int profileIndex = m_profileSelector->currentIndex();
  switch (profileIndex) {
  case 0:
    config.buildType = editor::BuildType::Debug;
    config.encryptAssets = false;
    config.includeDebugConsole = true;
    break;
  case 1:
    config.buildType = editor::BuildType::Release;
    config.encryptAssets = true;
    config.includeDebugConsole = false;
    break;
  case 2:
    config.buildType = editor::BuildType::Distribution;
    config.encryptAssets = true;
    config.signExecutable = true;
    break;
  }

  // Set options from UI
  config.packAssets = true;
  config.compression = m_compressAssets->isChecked()
                           ? editor::CompressionLevel::Balanced
                           : editor::CompressionLevel::None;

  config.includedLanguages = {"en"}; // Default language
  config.defaultLanguage = "en";

  // Setup callbacks
  buildSystem->setOnProgressUpdate([this](const editor::BuildProgress &progress) {
    QMetaObject::invokeMethod(this, [this, progress]() {
      int percent = static_cast<int>(progress.progress * 100);
      m_progressBar->setValue(percent);

      QString format = QString("%1% - %2")
                           .arg(percent)
                           .arg(QString::fromStdString(progress.currentStep));
      m_progressBar->setFormat(format);

      m_statusLabel->setText(QString::fromStdString(progress.currentTask));

      // Update build status based on progress
      if (progress.isRunning) {
        if (progress.currentStep == "Compile") {
          m_buildStatus = BuildStatus::Compiling;
        } else if (progress.currentStep == "Pack") {
          m_buildStatus = BuildStatus::Packaging;
        } else {
          m_buildStatus = BuildStatus::Copying;
        }
      }
    });
  });

  buildSystem->setOnLogMessage(
      [this](const std::string &message, bool isError) {
        QMetaObject::invokeMethod(this, [this, message, isError]() {
          QString formattedMsg = QString("[%1] %2")
                                     .arg(isError ? "ERROR" : "INFO")
                                     .arg(QString::fromStdString(message));
          appendLog(formattedMsg);
        });
      });

  buildSystem->setOnBuildComplete([this](const editor::BuildResult &result) {
    QMetaObject::invokeMethod(this, [this, result]() {
      if (result.success) {
        m_buildStatus = BuildStatus::Complete;
        m_progressBar->setValue(100);
        m_progressBar->setFormat("100% - Complete");
        m_statusLabel->setText(
            QString("Build completed in %1")
                .arg(QString::fromStdString(
                    editor::BuildUtils::formatDuration(result.buildTimeMs))));

        appendLog(QString("Build successful! Output: %1")
                      .arg(QString::fromStdString(result.outputPath)));
        appendLog(QString("Total size: %1")
                      .arg(QString::fromStdString(
                          editor::BuildUtils::formatFileSize(result.totalSize))));

        emit buildCompleted(true, QString::fromStdString(result.outputPath));

        NMMessageDialog::showInfo(
            this, tr("Build Complete"),
            tr("Build completed successfully!\n\nOutput: %1\nSize: %2\nTime: %3")
                .arg(QString::fromStdString(result.outputPath))
                .arg(QString::fromStdString(
                    editor::BuildUtils::formatFileSize(result.totalSize)))
                .arg(QString::fromStdString(
                    editor::BuildUtils::formatDuration(result.buildTimeMs))));
      } else {
        m_buildStatus = BuildStatus::Failed;
        m_progressBar->setFormat("Failed");
        m_statusLabel->setText(QString::fromStdString(result.errorMessage));

        appendLog(QString("Build failed: %1")
                      .arg(QString::fromStdString(result.errorMessage)));

        emit buildCompleted(false, QString::fromStdString(result.errorMessage));

        NMMessageDialog::showError(
            this, tr("Build Failed"),
            tr("Build failed:\n\n%1")
                .arg(QString::fromStdString(result.errorMessage)));
      }
    });
  });

  // Start build
  appendLog("Starting build...");
  auto result = buildSystem->startBuild(config);

  if (result.isError()) {
    m_buildStatus = BuildStatus::Failed;
    appendLog(QString("Failed to start build: %1")
                  .arg(QString::fromStdString(result.error())));
    NMMessageDialog::showError(
        this, tr("Build Error"),
        tr("Failed to start build:\n\n%1")
            .arg(QString::fromStdString(result.error())));
  } else {
    emit buildStarted();
  }
}

void NMBuildSettingsPanel::cancelBuild() {
  m_buildStatus = BuildStatus::Cancelled;
  m_progressBar->setFormat("Cancelled");
  m_statusLabel->setText("Build cancelled by user");
  appendLog("Build cancelled by user");
}

// ============================================================================
// Slots
// ============================================================================

void NMBuildSettingsPanel::onPlatformChanged(int index) {
  Q_UNUSED(index);
  updateSizePreview();
}

void NMBuildSettingsPanel::onProfileChanged(int index) {
  // Update options based on profile
  switch (index) {
  case 0: // Debug
    m_debugBuild->setChecked(true);
    m_compressAssets->setChecked(false);
    m_includeDevAssets->setChecked(true);
    break;
  case 1: // Release
    m_debugBuild->setChecked(false);
    m_compressAssets->setChecked(true);
    m_includeDevAssets->setChecked(false);
    break;
  case 2: // Distribution
    m_debugBuild->setChecked(false);
    m_compressAssets->setChecked(true);
    m_includeDevAssets->setChecked(false);
    break;
  }

  updateSizePreview();
}

void NMBuildSettingsPanel::onBrowseOutput() {
  QString dir = NMFileDialog::getExistingDirectory(
      this, tr("Select Output Directory"), m_outputPathEdit->text());

  if (!dir.isEmpty()) {
    m_outputPathEdit->setText(dir);
  }
}

void NMBuildSettingsPanel::onBuildClicked() {
  // Check for warnings first
  updateWarnings();

  bool hasCriticalWarnings = false;
  for (const auto &warning : m_warnings) {
    if (warning.isCritical) {
      hasCriticalWarnings = true;
      break;
    }
  }

  if (hasCriticalWarnings) {
    auto result = NMMessageDialog::showQuestion(
        this, tr("Build Warnings"),
        tr("There are critical warnings that may cause the build to fail.\n\n"
           "Do you want to continue anyway?"),
        {NMDialogButton::Yes, NMDialogButton::No},
        NMDialogButton::No);

    if (result != NMDialogButton::Yes) {
      return;
    }
  }

  startBuild();
}

void NMBuildSettingsPanel::onCancelClicked() { cancelBuild(); }

void NMBuildSettingsPanel::onWarningDoubleClicked(int row) {
  if (row >= 0 && row < m_warnings.size()) {
    const auto &warning = m_warnings[row];

    // Emit signal or navigate to the file/location
    emit buildWarningFound(warning);

    // Could open the file in an editor here
    appendLog(QString("Navigate to: %1 (line %2)")
                  .arg(warning.filePath)
                  .arg(warning.lineNumber));
  }
}

void NMBuildSettingsPanel::onRefreshPreview() { updateSizePreview(); }

// ============================================================================
// Helper Methods
// ============================================================================

void NMBuildSettingsPanel::updateSizePreview() {
  m_sizeInfo = calculateBuildSize();

  m_totalSizeLabel->setText(formatSize(m_sizeInfo.totalSize));
  m_assetsSizeLabel->setText(formatSize(m_sizeInfo.assetsSize));
  m_imagesSizeLabel->setText(formatSize(m_sizeInfo.imagesSize));
  m_audioSizeLabel->setText(formatSize(m_sizeInfo.audioSize));
  m_scriptsSizeLabel->setText(formatSize(m_sizeInfo.scriptsSize));
  m_fileCountLabel->setText(QString("%1 files").arg(m_sizeInfo.fileCount));
}

void NMBuildSettingsPanel::updateWarnings() {
  m_warnings = scanForWarnings();

  m_warningsTree->clear();

  for (const auto &warning : m_warnings) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_warningsTree);

    // Type column
    QString typeStr;
    switch (warning.type) {
    case BuildWarningType::MissingAsset:
      typeStr = "Missing Asset";
      break;
    case BuildWarningType::UnusedAsset:
      typeStr = "Unused Asset";
      break;
    case BuildWarningType::MissingTranslation:
      typeStr = "Missing Translation";
      break;
    case BuildWarningType::BrokenReference:
      typeStr = "Broken Reference";
      break;
    case BuildWarningType::LargeFile:
      typeStr = "Large File";
      break;
    case BuildWarningType::UnsupportedFormat:
      typeStr = "Unsupported Format";
      break;
    }
    item->setText(0, typeStr);

    // Message column
    item->setText(1, warning.message);

    // Location column
    item->setText(2, warning.filePath);

    // Color based on severity
    if (warning.isCritical) {
      item->setForeground(0, QBrush(QColor("#ff6b6b")));
      item->setForeground(1, QBrush(QColor("#ff6b6b")));
    } else {
      item->setForeground(0, QBrush(QColor("#ffd93d")));
      item->setForeground(1, QBrush(QColor("#ffd93d")));
    }
  }

  // Update count label
  int criticalCount = 0;
  for (const auto &w : m_warnings) {
    if (w.isCritical)
      criticalCount++;
  }

  if (m_warnings.isEmpty()) {
    m_warningCountLabel->setText("No warnings");
    m_warningCountLabel->setStyleSheet("color: #4caf50; font-style: italic;");
  } else {
    m_warningCountLabel->setText(
        QString("%1 warnings (%2 critical)")
            .arg(m_warnings.size())
            .arg(criticalCount));
    if (criticalCount > 0) {
      m_warningCountLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
    } else {
      m_warningCountLabel->setStyleSheet("color: #ffd93d;");
    }
  }
}

void NMBuildSettingsPanel::appendLog(const QString &message) {
  if (m_logOutput) {
    m_logOutput->appendPlainText(
        QString("[%1] %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
            .arg(message));
  }
}

QString NMBuildSettingsPanel::formatSize(qint64 bytes) const {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  if (unitIndex == 0) {
    return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
  } else {
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
  }
}

} // namespace NovelMind::editor::qt
