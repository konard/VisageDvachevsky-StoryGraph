#pragma once

/**
 * @file nm_project_settings_panel.hpp
 * @brief Project Settings panel for configuring project-wide settings
 *
 * This panel provides access to:
 * - Workflow settings (Visual-First, Code-First, Hybrid)
 * - Display settings (resolution, safe area, fullscreen)
 * - Text/dialogue settings (font theme, text speed, auto-advance)
 * - Localization settings (default locale, available locales)
 * - Export/build profiles
 * - Project metadata
 *
 * Signal Flow:
 * - Outgoing: settingsChanged() - emitted when any setting is modified
 * - Uses QSignalBlocker in loadFromProject() to prevent feedback loops with
 *   NMPlayToolbarPanel when loading settings from project metadata
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QWidget>

class QFormLayout;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QTabWidget;
class QSlider;
class QLabel;

namespace NovelMind::editor::qt {

class NMProjectSettingsPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMProjectSettingsPanel(QWidget *parent = nullptr);
  ~NMProjectSettingsPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  // Load/save settings from/to project
  void loadFromProject();
  void saveToProject();

signals:
  void settingsChanged();

private slots:
  void onSettingChanged();
  void onApplyClicked();
  void onRevertClicked();

private:
  void setupUI();
  void setupWorkflowTab(QWidget *parent);
  void setupDisplayTab(QWidget *parent);
  void setupTextTab(QWidget *parent);
  void setupLocalizationTab(QWidget *parent);
  void setupBuildProfilesTab(QWidget *parent);
  void connectSignals();
  void updateApplyButton();
  void updateWorkflowDescription();

  QTabWidget *m_tabWidget = nullptr;

  // Workflow settings (Scene Node development approach)
  QComboBox *m_workflowCombo = nullptr; // Visual-First, Code-First, Hybrid
  QCheckBox *m_allowMixedWorkflows =
      nullptr;                             // Allow per-scene workflow override
  QLabel *m_workflowDescription = nullptr; // Description of selected workflow

  // Display settings
  QComboBox *m_resolutionCombo = nullptr;
  QSpinBox *m_safeAreaTop = nullptr;
  QSpinBox *m_safeAreaBottom = nullptr;
  QSpinBox *m_safeAreaLeft = nullptr;
  QSpinBox *m_safeAreaRight = nullptr;
  QCheckBox *m_fullscreenDefault = nullptr;
  QCheckBox *m_allowWindowResize = nullptr;
  QComboBox *m_aspectRatioMode = nullptr;

  // Text/Dialogue settings
  QComboBox *m_fontThemeCombo = nullptr;
  QSlider *m_textSpeedSlider = nullptr;
  QLabel *m_textSpeedLabel = nullptr;
  QSpinBox *m_autoAdvanceDelay = nullptr;
  QCheckBox *m_enableTypewriter = nullptr;
  QSpinBox *m_historyLength = nullptr;
  QCheckBox *m_enableSkip = nullptr;
  QCheckBox *m_skipOnlyRead = nullptr;

  // Localization settings
  QComboBox *m_defaultLocaleCombo = nullptr;
  QLineEdit *m_availableLocales = nullptr;
  QComboBox *m_fallbackLocaleCombo = nullptr;
  QCheckBox *m_showLanguageSelector = nullptr;

  // Build profiles
  QComboBox *m_buildProfileCombo = nullptr;
  QLineEdit *m_profileName = nullptr;
  QPushButton *m_addProfileBtn = nullptr;
  QPushButton *m_removeProfileBtn = nullptr;

  // Control buttons
  QPushButton *m_applyButton = nullptr;
  QPushButton *m_revertButton = nullptr;

  bool m_hasUnsavedChanges = false;
};

} // namespace NovelMind::editor::qt
