#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/guided_learning/anchor_registry.hpp"
#include "NovelMind/editor/guided_learning/tutorial_subsystem.hpp"
#include "NovelMind/editor/mediators/panel_mediators.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_settings_dialog.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/settings_registry.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace NovelMind::editor::qt {

NMMainWindow::NMMainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("NovelMind Editor");
  setMinimumSize(1280, 720);
  resize(1920, 1080);

  // Enable docking with nesting and grouping
  setDockNestingEnabled(true);
  setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks |
                 GroupedDragging);
}

NMMainWindow::~NMMainWindow() { shutdown(); }

bool NMMainWindow::initialize() {
  if (m_initialized)
    return true;

  // Initialize settings registry
  m_settingsRegistry = std::make_unique<editor::NMSettingsRegistry>();
  m_settingsRegistry->registerEditorDefaults();
  m_settingsRegistry->registerProjectDefaults();

  // Log registered settings count for debugging
  NOVELMIND_LOG_INFO("Registered " +
                     std::to_string(m_settingsRegistry->getAllDefinitions().size()) +
                     " settings");

  // Load user settings
  QString configPath =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  QString userSettingsPath = configPath + "/NovelMind/editor_settings.json";
  auto loadResult =
      m_settingsRegistry->loadUserSettings(userSettingsPath.toStdString());
  if (loadResult.isError()) {
    NOVELMIND_LOG_WARN(std::string("Failed to load user settings: ") +
                       loadResult.error());
  }

  // Initialize undo/redo system
  NMUndoManager::instance().initialize();

  setupMenuBar();
  setupToolBar();
  setupStatusBar();
  setupPanels();
  configureDocking();
  setupConnections();
  setupShortcuts();

  // Restore layout or use default
  QSettings settings("NovelMind", "Editor");
  if (settings.contains("mainwindow/geometry")) {
    restoreLayout();
  } else {
    createDefaultLayout();
  }

  // Start update timer
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &NMMainWindow::onUpdateTick);
  m_updateTimer->start(UPDATE_INTERVAL_MS);

  updateStatusBarContext();

  // Initialize the guided learning / tutorial subsystem
  {
    using namespace NovelMind::editor::guided_learning;

    // Register the main window center anchor for tutorials that target
    // the overall window (e.g., welcome dialogs)
    NMAnchorRegistry::instance().registerAnchor(
        "main_window.center", centralWidget() ? centralWidget() : this,
        "Main editor window center", "main_window");

    TutorialSubsystemConfig tutorialConfig;
    tutorialConfig.tutorialDefinitionsPath = ":/tutorials"; // Qt resources
    tutorialConfig.enabledByDefault = true;
    tutorialConfig.hintsEnabledByDefault = true;
    tutorialConfig.walkthroughsOnFirstRunByDefault = true;
    tutorialConfig.verboseLogging = false;

    auto tutorialResult =
        NMTutorialSubsystem::instance().initialize(this, tutorialConfig);
    if (tutorialResult.isError()) {
      NOVELMIND_LOG_WARN(
          std::string("Failed to initialize tutorial subsystem: ") +
          tutorialResult.error());
    } else {
      NOVELMIND_LOG_INFO("Tutorial subsystem initialized successfully");
    }
  }

  m_initialized = true;
  return true;
}

void NMMainWindow::shutdown() {
  if (!m_initialized)
    return;

  if (m_updateTimer) {
    m_updateTimer->stop();
    m_updateTimer->deleteLater();
    m_updateTimer = nullptr;
  }

  // Shutdown tutorial subsystem
  if (guided_learning::NMTutorialSubsystem::hasInstance()) {
    guided_learning::NMTutorialSubsystem::instance().shutdown();
  }

  // Save user settings
  if (m_settingsRegistry) {
    QString configPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString userSettingsPath = configPath + "/NovelMind/editor_settings.json";
    auto saveResult =
        m_settingsRegistry->saveUserSettings(userSettingsPath.toStdString());
    if (saveResult.isError()) {
      NOVELMIND_LOG_WARN(std::string("Failed to save user settings: ") +
                         saveResult.error());
    }
  }

  NMPlayModeController::instance().shutdown();

  saveLayout();

  m_initialized = false;
}

void NMMainWindow::showSettingsDialog() {
  if (!m_settingsRegistry) {
    NOVELMIND_LOG_ERROR("Settings registry not initialized");
    NMMessageDialog::showError(
        this, tr("Settings Error"),
        tr("The settings system failed to initialize. Please restart the "
           "editor."));
    return;
  }

  if (m_settingsRegistry->getAllDefinitions().empty()) {
    NOVELMIND_LOG_WARN("No settings registered");
    NMMessageDialog::showWarning(this, tr("No Settings"),
                                 tr("No settings are currently available."));
    return;
  }

  NMSettingsDialog dialog(m_settingsRegistry.get(), this);
  dialog.exec();
}

} // namespace NovelMind::editor::qt
