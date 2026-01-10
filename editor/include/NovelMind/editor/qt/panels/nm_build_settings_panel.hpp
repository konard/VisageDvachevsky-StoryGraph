#pragma once

/**
 * @file nm_build_settings_panel.hpp
 * @brief Build and export settings panel
 *
 * Provides:
 * - Build size preview
 * - Missing asset warnings
 * - Actual build execution with status reporting
 * - Platform selection
 * - Build profiles
 * - Progress monitoring
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QHash>
#include <QWidget>

class QFormLayout;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QTreeWidget;
class QPlainTextEdit;
class QTabWidget;

namespace NovelMind::editor::qt {

/**
 * @brief Build warning types
 */
enum class BuildWarningType {
  MissingAsset,
  UnusedAsset,
  MissingTranslation,
  BrokenReference,
  LargeFile,
  UnsupportedFormat
};

/**
 * @brief Build warning entry
 */
struct BuildWarning {
  BuildWarningType type;
  QString message;
  QString filePath;
  int lineNumber = 0;
  bool isCritical = false;
};

/**
 * @brief Build status enum
 */
enum class BuildStatus {
  Idle,
  Preparing,
  Copying,
  Compiling,
  Packaging,
  Complete,
  Failed,
  Cancelled
};

/**
 * @brief Build size info
 */
struct BuildSizeInfo {
  qint64 totalSize = 0;
  qint64 assetsSize = 0;
  qint64 scriptsSize = 0;
  qint64 audioSize = 0;
  qint64 imagesSize = 0;
  qint64 fontsSize = 0;
  qint64 otherSize = 0;
  int fileCount = 0;
};

class NMBuildSettingsPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMBuildSettingsPanel(QWidget* parent = nullptr);
  ~NMBuildSettingsPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Calculate estimated build size
   */
  BuildSizeInfo calculateBuildSize() const;

  /**
   * @brief Scan for build warnings
   */
  QList<BuildWarning> scanForWarnings() const;

  /**
   * @brief Get current build status
   */
  [[nodiscard]] BuildStatus buildStatus() const { return m_buildStatus; }

  /**
   * @brief Start build process
   */
  void startBuild();

  /**
   * @brief Cancel ongoing build
   */
  void cancelBuild();

signals:
  void buildStarted();
  void buildProgress(int percent, const QString& stage);
  void buildCompleted(bool success, const QString& outputPath);
  void buildWarningFound(const BuildWarning& warning);

private slots:
  void onPlatformChanged(int index);
  void onProfileChanged(int index);
  void onBrowseOutput();
  void onBuildClicked();
  void onCancelClicked();
  void onWarningDoubleClicked(int row);
  void onRefreshPreview();

private:
  void setupUI();
  void setupBuildSettings();
  void setupWarningsTab();
  void setupLogTab();
  void updateSizePreview();
  void updateWarnings();
  void appendLog(const QString& message);
  QString formatSize(qint64 bytes) const;

  // UI Elements
  QTabWidget* m_tabWidget = nullptr;

  // Settings tab
  QComboBox* m_platformSelector = nullptr;
  QComboBox* m_profileSelector = nullptr;
  QLineEdit* m_outputPathEdit = nullptr;
  QPushButton* m_browseBtn = nullptr;
  QCheckBox* m_debugBuild = nullptr;
  QCheckBox* m_includeDevAssets = nullptr;
  QCheckBox* m_compressAssets = nullptr;

  // Code signing
  QCheckBox* m_signExecutable = nullptr;
  QLineEdit* m_signingCertificateEdit = nullptr;
  QPushButton* m_browseCertificateBtn = nullptr;
  QLineEdit* m_signingPasswordEdit = nullptr;
  QLineEdit* m_signingTeamIdEdit = nullptr;
  QLineEdit* m_signingEntitlementsEdit = nullptr;
  QPushButton* m_browseEntitlementsBtn = nullptr;

  // Size preview
  QLabel* m_totalSizeLabel = nullptr;
  QLabel* m_assetsSizeLabel = nullptr;
  QLabel* m_scriptsSizeLabel = nullptr;
  QLabel* m_audioSizeLabel = nullptr;
  QLabel* m_imagesSizeLabel = nullptr;
  QLabel* m_fileCountLabel = nullptr;
  QPushButton* m_refreshPreviewBtn = nullptr;

  // Build controls
  QPushButton* m_buildButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
  QProgressBar* m_progressBar = nullptr;
  QLabel* m_statusLabel = nullptr;

  // Warnings tab
  QTreeWidget* m_warningsTree = nullptr;
  QLabel* m_warningCountLabel = nullptr;

  // Log tab
  QPlainTextEdit* m_logOutput = nullptr;
  QPushButton* m_clearLogBtn = nullptr;

  // State
  BuildStatus m_buildStatus = BuildStatus::Idle;
  QList<BuildWarning> m_warnings;
  BuildSizeInfo m_sizeInfo;
  QString m_outputPath;
};

} // namespace NovelMind::editor::qt
