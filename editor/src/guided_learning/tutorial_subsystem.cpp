/**
 * @file tutorial_subsystem.cpp
 * @brief Implementation of the Tutorial Subsystem
 */

#include "NovelMind/editor/guided_learning/tutorial_subsystem.hpp"
#include "NovelMind/editor/guided_learning/anchor_registry.hpp"
#include "NovelMind/editor/guided_learning/help_overlay.hpp"
#include "NovelMind/editor/guided_learning/tutorial_manager.hpp"
#include "NovelMind/editor/settings_registry.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace NovelMind::editor::guided_learning {

// Static instance
std::unique_ptr<NMTutorialSubsystem> NMTutorialSubsystem::s_instance;

NMTutorialSubsystem& NMTutorialSubsystem::instance() {
  if (!s_instance) {
    s_instance.reset(new NMTutorialSubsystem());
  }
  return *s_instance;
}

bool NMTutorialSubsystem::hasInstance() {
  return s_instance != nullptr;
}

NMTutorialSubsystem::NMTutorialSubsystem() : QObject(nullptr) {
  setObjectName("NMTutorialSubsystem");
}

NMTutorialSubsystem::~NMTutorialSubsystem() {
  if (m_initialized) {
    shutdown();
  }
}

Result<void> NMTutorialSubsystem::initialize(QWidget* parentWidget,
                                             const TutorialSubsystemConfig& config) {
  if (m_initialized) {
    return Result<void>::error("Tutorial subsystem already initialized");
  }

  if (!parentWidget) {
    return Result<void>::error("Parent widget is null");
  }

  m_config = config;

  qDebug() << "Initializing Tutorial Subsystem...";

  // Create the overlay
  m_overlay = std::make_unique<NMHelpOverlay>(parentWidget);

  // Initialize the tutorial manager
  NMTutorialManager::instance().initialize(m_overlay.get());

  // Set up progress file path
  QString progressPath;
  if (config.userProgressPath.empty()) {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);
    if (!dir.exists()) {
      dir.mkpath(".");
    }
    progressPath = dir.filePath("tutorial_progress.json");
  } else {
    progressPath = QString::fromStdString(config.userProgressPath);
  }

  NMTutorialManager::instance().setProgressFilePath(progressPath.toStdString());

  // Load existing progress
  auto loadResult = NMTutorialManager::instance().loadProgress(progressPath.toStdString());
  if (!loadResult.isOk() && config.verboseLogging) {
    qDebug() << "No existing tutorial progress found (this is normal on first run)";
  }

  // Load tutorial definitions
  QString tutorialPath = QString::fromStdString(config.tutorialDefinitionsPath);
  if (QDir(tutorialPath).isRelative()) {
    // Look in app resources or current dir
    QStringList searchPaths;
    searchPaths << QDir::currentPath() + "/" + tutorialPath;
    searchPaths << QCoreApplication::applicationDirPath() + "/" + tutorialPath;
    searchPaths << ":/tutorials"; // Qt resources

    for (const auto& path : searchPaths) {
      QDir dir(path);
      if (dir.exists()) {
        tutorialPath = path;
        break;
      }
    }
  }

  if (QDir(tutorialPath).exists()) {
    auto loadTutorialsResult =
        NMTutorialManager::instance().loadTutorialsFromDirectory(tutorialPath.toStdString());
    if (loadTutorialsResult.isOk()) {
      qDebug() << "Loaded" << loadTutorialsResult.value() << "tutorials from" << tutorialPath;
    }
  } else if (config.verboseLogging) {
    qDebug() << "Tutorial definitions directory not found:" << tutorialPath;
  }

  // Register settings
  registerSettings();

  // Connect to event bus
  connectToEventBus();

  // Apply initial settings
  NMTutorialManager::instance().setEnabled(config.enabledByDefault);
  NMTutorialManager::instance().setHintsEnabled(config.hintsEnabledByDefault);
  NMTutorialManager::instance().setWalkthroughsOnFirstRunEnabled(
      config.walkthroughsOnFirstRunByDefault);

  m_initialized = true;

  emit initialized();

  qDebug() << "Tutorial Subsystem initialized successfully";

  return Result<void>::ok();
}

void NMTutorialSubsystem::shutdown() {
  if (!m_initialized)
    return;

  qDebug() << "Shutting down Tutorial Subsystem...";

  emit shuttingDown();

  // Save progress
  saveUserPreferences();

  // Disconnect from event bus
  disconnectFromEventBus();

  // Shutdown manager
  NMTutorialManager::instance().shutdown();

  // Destroy overlay
  m_overlay.reset();

  m_initialized = false;

  qDebug() << "Tutorial Subsystem shutdown complete";
}

NMTutorialManager& NMTutorialSubsystem::tutorialManager() {
  return NMTutorialManager::instance();
}

NMAnchorRegistry& NMTutorialSubsystem::anchorRegistry() {
  return NMAnchorRegistry::instance();
}

NMHelpOverlay* NMTutorialSubsystem::helpOverlay() {
  return m_overlay.get();
}

bool NMTutorialSubsystem::startTutorial(const std::string& tutorialId) {
  if (!m_initialized)
    return false;
  return NMTutorialManager::instance().startTutorial(tutorialId);
}

bool NMTutorialSubsystem::showHint(const std::string& hintId) {
  if (!m_initialized)
    return false;
  return NMTutorialManager::instance().showHint(hintId);
}

void NMTutorialSubsystem::hideAll() {
  if (!m_initialized)
    return;
  NMTutorialManager::instance().stopTutorial(false);
  NMTutorialManager::instance().hideAllHints();
}

bool NMTutorialSubsystem::isEnabled() const {
  if (!m_initialized)
    return false;
  return NMTutorialManager::instance().isEnabled();
}

void NMTutorialSubsystem::setEnabled(bool enabled) {
  if (!m_initialized)
    return;
  NMTutorialManager::instance().setEnabled(enabled);
  emit enabledChanged(enabled);
}

void NMTutorialSubsystem::registerSettings() {
  // Register guided learning settings with the editor settings registry
  // These settings will appear in Editor Preferences

  SettingDefinition enabledDef;
  enabledDef.key = "guidedLearning.enabled";
  enabledDef.displayName = "Enable Guided Learning";
  enabledDef.description = "Show tutorials and contextual hints in the editor";
  enabledDef.category = "Editor/Guided Learning";
  enabledDef.type = SettingType::Bool;
  enabledDef.scope = SettingScope::User;
  enabledDef.defaultValue = true;

  SettingDefinition hintsDef;
  hintsDef.key = "guidedLearning.hintsEnabled";
  hintsDef.displayName = "Show Contextual Hints";
  hintsDef.description = "Display helpful hints when panels are empty or errors occur";
  hintsDef.category = "Editor/Guided Learning";
  hintsDef.type = SettingType::Bool;
  hintsDef.scope = SettingScope::User;
  hintsDef.defaultValue = true;

  SettingDefinition firstRunDef;
  firstRunDef.key = "guidedLearning.walkthroughsOnFirstRun";
  firstRunDef.displayName = "Show Walkthroughs on First Run";
  firstRunDef.description = "Automatically show tutorials when using a feature for the first time";
  firstRunDef.category = "Editor/Guided Learning";
  firstRunDef.type = SettingType::Bool;
  firstRunDef.scope = SettingScope::User;
  firstRunDef.defaultValue = true;

  // Get the settings registry - assuming it's available as a singleton
  // For now, we just define the settings; registration would happen
  // when the settings registry is available
}

void NMTutorialSubsystem::applySettings() {
  if (!m_initialized)
    return;

  // Apply settings from the registry
  // This would read from NMSettingsRegistry and apply to TutorialManager
}

Result<void> NMTutorialSubsystem::saveUserPreferences() {
  if (!m_initialized) {
    return Result<void>::error("Subsystem not initialized");
  }

  QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QString progressPath = QDir(appDataPath).filePath("tutorial_progress.json");

  return NMTutorialManager::instance().saveProgress(progressPath.toStdString());
}

Result<void> NMTutorialSubsystem::loadUserPreferences() {
  if (!m_initialized) {
    return Result<void>::error("Subsystem not initialized");
  }

  QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QString progressPath = QDir(appDataPath).filePath("tutorial_progress.json");

  return NMTutorialManager::instance().loadProgress(progressPath.toStdString());
}

void NMTutorialSubsystem::connectToEventBus() {
  auto& bus = EventBus::instance();

  // Subscribe to panel focus changes
  auto panelFocusSub =
      bus.subscribe(EditorEventType::PanelFocusChanged, [this](const EditorEvent& event) {
        if (auto* e = dynamic_cast<const PanelFocusChangedEvent*>(&event)) {
          onPanelFocusChanged(*e);
        }
      });
  m_eventSubscriptions.emplace_back(&bus, panelFocusSub);

  // Subscribe to project events
  auto projectOpenedSub =
      bus.subscribe(EditorEventType::ProjectOpened, [this](const EditorEvent& event) {
        if (auto* e = dynamic_cast<const ProjectEvent*>(&event)) {
          onProjectOpened(*e);
        }
      });
  m_eventSubscriptions.emplace_back(&bus, projectOpenedSub);

  auto projectClosedSub =
      bus.subscribe(EditorEventType::ProjectClosed, [this](const EditorEvent& event) {
        if (auto* e = dynamic_cast<const ProjectEvent*>(&event)) {
          onProjectClosed(*e);
        }
      });
  m_eventSubscriptions.emplace_back(&bus, projectClosedSub);

  // Subscribe to error events
  auto errorSub = bus.subscribe(EditorEventType::ErrorOccurred, [this](const EditorEvent& event) {
    if (auto* e = dynamic_cast<const ErrorEvent*>(&event)) {
      onErrorOccurred(*e);
    }
  });
  m_eventSubscriptions.emplace_back(&bus, errorSub);
}

void NMTutorialSubsystem::disconnectFromEventBus() {
  m_eventSubscriptions.clear();
}

void NMTutorialSubsystem::onPanelFocusChanged(const PanelFocusChangedEvent& event) {
  if (!m_initialized || !event.hasFocus)
    return;

  NMTutorialManager::instance().onPanelOpened(event.panelName);
}

void NMTutorialSubsystem::onProjectOpened(const ProjectEvent& event) {
  if (!m_initialized)
    return;

  // Could trigger "first project opened" tutorial
  (void)event;
}

void NMTutorialSubsystem::onProjectClosed(const ProjectEvent& event) {
  if (!m_initialized)
    return;

  // Hide any active tutorials when project closes
  hideAll();
  (void)event;
}

void NMTutorialSubsystem::onErrorOccurred(const ErrorEvent& event) {
  if (!m_initialized)
    return;

  NMTutorialManager::instance().onErrorOccurred(event.message, event.details);
}

} // namespace NovelMind::editor::guided_learning
