/**
 * @file main.cpp
 * @brief NovelMind Editor Qt6 Application Entry Point
 *
 * This is the main entry point for the NovelMind Visual Novel Editor.
 * The editor is built with Qt 6 Widgets and provides:
 * - Visual scene editing with WYSIWYG preview
 * - Node-based story graph editor
 * - Asset management and import pipeline
 * - Project build and export system
 *
 * Version: 0.3.0
 */

#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_welcome_dialog.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <iostream>

using namespace NovelMind;
using namespace NovelMind::editor::qt;

/**
 * @brief Print application version and build info
 */
void printVersion() {
  std::cout << "NovelMind Editor v0.3.0\n";
  std::cout << "Built with Qt " << QT_VERSION_STR << " and C++20\n";
  std::cout << "Copyright (c) 2024 NovelMind Contributors\n";
  std::cout << "Licensed under MIT License\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[]) {
  // Enable High-DPI scaling
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  // Create the Qt application
  QApplication app(argc, argv);
  app.setApplicationName("NovelMind Editor");
  app.setApplicationVersion("0.3.0");
  app.setOrganizationName("NovelMind");
  app.setOrganizationDomain("novelmind.io");

  // Set up command line parser
  QCommandLineParser parser;
  parser.setApplicationDescription("NovelMind Visual Novel Editor");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption newProjectOption(QStringList() << "n"
                                                    << "new",
                                      "Create a new project at <path>", "path");
  parser.addOption(newProjectOption);

  QCommandLineOption openProjectOption(QStringList() << "o"
                                                     << "open",
                                       "Open an existing project", "path");
  parser.addOption(openProjectOption);

  QCommandLineOption noWelcomeOption("no-welcome", "Skip the welcome screen");
  parser.addOption(noWelcomeOption);

  QCommandLineOption resetLayoutOption("reset-layout",
                                       "Reset panel layout to defaults");
  parser.addOption(resetLayoutOption);

  QCommandLineOption scaleOption("scale", "Set UI scale factor (0.5-3.0)",
                                 "factor", "1.0");
  parser.addOption(scaleOption);

  // Add positional argument for project file
  parser.addPositionalArgument("project", "Project file to open");

  parser.process(app);

  // Initialize Qt resources from static library
  Q_INIT_RESOURCE(editor_resources);

  // Initialize logger
  core::Logger::instance().setLevel(core::LogLevel::Info);
  core::Logger::instance().info("NovelMind Editor starting...");

  // Initialize style manager and apply dark theme
  NMStyleManager &styleManager = NMStyleManager::instance();
  styleManager.initialize(&app);

  // Apply scale factor if specified
  if (parser.isSet(scaleOption)) {
    bool ok;
    double scale = parser.value(scaleOption).toDouble(&ok);
    if (ok && scale >= 0.5 && scale <= 3.0) {
      styleManager.setUiScale(scale);
    }
  }

  // Create main window
  NMMainWindow mainWindow;

  if (!mainWindow.initialize()) {
    core::Logger::instance().error("Failed to initialize main window");
    return 1;
  }

  // Reset layout if requested
  if (parser.isSet(resetLayoutOption)) {
    mainWindow.resetToDefaultLayout();
  }

  auto resolveTemplateId = [](const QString &templateName) {
    const QString lowered = templateName.trimmed().toLower();
    if (lowered.contains("blank")) {
      return QString("empty");
    }
    if (lowered.contains("visual")) {
      return QString("kinetic_novel");
    }
    if (lowered.contains("dating")) {
      return QString("branching_story");
    }
    if (lowered.contains("mystery")) {
      return QString("branching_story");
    }
    if (lowered.contains("rpg")) {
      return QString("branching_story");
    }
    if (lowered.contains("horror")) {
      return QString("branching_story");
    }
    return QString("empty");
  };

  auto updateRecentProjects = [](const QString &projectPath,
                                 const QString &projectName) {
    if (projectPath.isEmpty()) {
      return;
    }

    struct RecentEntry {
      QString name;
      QString path;
      QString lastOpened;
      QString thumbnail;
    };

    QSettings settings("NovelMind", "Editor");
    QList<RecentEntry> entries;
    const int count = settings.beginReadArray("RecentProjects");
    for (int i = 0; i < count; ++i) {
      settings.setArrayIndex(i);
      RecentEntry entry;
      entry.name = settings.value("name").toString();
      entry.path = settings.value("path").toString();
      entry.lastOpened = settings.value("lastOpened").toString();
      entry.thumbnail = settings.value("thumbnail").toString();
      if (!entry.path.isEmpty()) {
        entries.append(entry);
      }
    }
    settings.endArray();

    const QString normalizedPath = QFileInfo(projectPath).absoluteFilePath();
    for (qsizetype i = entries.size(); i > 0; --i) {
      const qsizetype index = i - 1;
      if (QFileInfo(entries[index].path).absoluteFilePath() == normalizedPath) {
        entries.removeAt(index);
      }
    }

    RecentEntry current;
    current.path = normalizedPath;
    current.name = projectName.isEmpty() ? QFileInfo(normalizedPath).fileName()
                                         : projectName;
    current.lastOpened = QDateTime::currentDateTime().toString(Qt::ISODate);
    current.thumbnail = QString();
    entries.prepend(current);

    const int maxRecent = 10;
    if (entries.size() > maxRecent) {
      entries = entries.mid(0, maxRecent);
    }

    settings.beginWriteArray("RecentProjects");
    for (int i = 0; i < entries.size(); ++i) {
      settings.setArrayIndex(i);
      settings.setValue("name", entries[i].name);
      settings.setValue("path", entries[i].path);
      settings.setValue("lastOpened", entries[i].lastOpened);
      settings.setValue("thumbnail", entries[i].thumbnail);
    }
    settings.endArray();
  };

  auto applyProjectToPanels = [&mainWindow]() {
    auto &projectManager = NovelMind::editor::ProjectManager::instance();
    if (!projectManager.hasOpenProject()) {
      return;
    }

    const QString projectName =
        QString::fromStdString(projectManager.getProjectName());
    mainWindow.updateWindowTitle(projectName);

    const QString assetsRoot = QString::fromStdString(
        projectManager.getFolderPath(NovelMind::editor::ProjectFolder::Assets));
    if (auto *assetPanel = mainWindow.assetBrowserPanel()) {
      assetPanel->setRootPath(assetsRoot);
    }
    if (auto *scriptPanel = mainWindow.scriptEditorPanel()) {
      scriptPanel->refreshFileList();
    }
    if (auto *hierarchyPanel = mainWindow.hierarchyPanel()) {
      hierarchyPanel->refresh();
    }
    if (auto *sceneView = mainWindow.sceneViewPanel()) {
      const std::string startScene = projectManager.getStartScene();
      if (!startScene.empty()) {
        sceneView->loadSceneDocument(QString::fromStdString(startScene));
      } else {
        const QString scenesRoot =
            QString::fromStdString(projectManager.getFolderPath(
                NovelMind::editor::ProjectFolder::Scenes));
        QDir scenesDir(scenesRoot);
        const QStringList scenes =
            scenesDir.entryList(QStringList() << "*.nmscene", QDir::Files);
        if (!scenes.isEmpty()) {
          const QString sceneId = QFileInfo(scenes.first()).completeBaseName();
          sceneView->loadSceneDocument(sceneId);
        }
      }
    }
  };

  auto applyProjectAndRemember = [&applyProjectToPanels, &updateRecentProjects,
                                  &mainWindow]() {
    auto &projectManager = NovelMind::editor::ProjectManager::instance();
    applyProjectToPanels();
    updateRecentProjects(
        QString::fromStdString(projectManager.getProjectPath()),
        QString::fromStdString(projectManager.getProjectName()));
    QTimer::singleShot(0, &mainWindow, []() {
      try {
        if (!NMPlayModeController::instance().loadCurrentProject()) {
          core::Logger::instance().warning(
              "PlayMode runtime did not load for current project");
        }
      } catch (const std::exception &e) {
        core::Logger::instance().error(
            std::string("PlayMode runtime exception: ") + e.what());
      }
    });
  };

  const QStringList templateOptions = {"Blank Project", "Visual Novel",
                                       "Dating Sim",    "Mystery/Detective",
                                       "RPG Story",     "Horror"};

  auto runNewProjectDialog =
      [&mainWindow, &templateOptions, resolveTemplateId,
       applyProjectAndRemember](const QString &preferredTemplate =
                                    QString()) -> bool {
    core::Logger::instance().info("Opening New Project dialog");
    NMNewProjectDialog dialog(&mainWindow);
    dialog.setTemplateOptions(templateOptions);
    dialog.setBaseDirectory(QDir::homePath());
    if (!preferredTemplate.trimmed().isEmpty()) {
      dialog.setTemplate(preferredTemplate.trimmed());
    }

    while (dialog.exec() == QDialog::Accepted) {
      const QString name = dialog.projectName();
      const QString baseDir = dialog.baseDirectory();
      const QString templateName = dialog.templateName();
      const int workflowMode = dialog.workflowMode();
      core::Logger::instance().info("New Project accepted: name='" +
                                    name.toStdString() + "', base='" +
                                    baseDir.toStdString() + "', template='" +
                                    templateName.toStdString() + "', workflow=" +
                                    std::to_string(workflowMode));
      if (name.isEmpty() || baseDir.isEmpty()) {
        NMMessageDialog::showWarning(
            &mainWindow, QObject::tr("New Project"),
            QObject::tr("Please enter a project name and location."));
        continue;
      }

      const QString projectPath = QDir(baseDir).filePath(name.trimmed());
      auto &projectManager = NovelMind::editor::ProjectManager::instance();
      const QString templateId = resolveTemplateId(templateName);
      Result<void> result = Result<void>::error("Unknown error");
      try {
        result = projectManager.createProject(projectPath.toStdString(),
                                              name.toStdString(),
                                              templateId.toStdString());
      } catch (const std::exception &e) {
        result = Result<void>::error(
            std::string("Exception during project creation: ") + e.what());
      }
      if (result.isError()) {
        core::Logger::instance().warning("Create Project Failed: " +
                                         result.error());
        NMMessageDialog::showError(&mainWindow,
                                   QObject::tr("Create Project Failed"),
                                   QString::fromStdString(result.error()));
        continue;
      }

      // Apply workflow mode to project metadata (issue #100)
      auto metadata = projectManager.getMetadata();
      switch (workflowMode) {
      case 0: // Code-Only (Script)
        metadata.playbackSourceMode =
            NovelMind::editor::PlaybackSourceMode::Script;
        break;
      case 1: // Graph-Only
        metadata.playbackSourceMode =
            NovelMind::editor::PlaybackSourceMode::Graph;
        break;
      case 2: // Mixed
        metadata.playbackSourceMode =
            NovelMind::editor::PlaybackSourceMode::Mixed;
        break;
      default:
        metadata.playbackSourceMode =
            NovelMind::editor::PlaybackSourceMode::Script;
        break;
      }
      projectManager.setMetadata(metadata);
      projectManager.saveProject();

      core::Logger::instance().info("Project created at: " +
                                    projectPath.toStdString());
      applyProjectAndRemember();
      core::Logger::instance().info("New Project flow completed");
      return true;
    }

    core::Logger::instance().info("New Project dialog closed without action");
    return false;
  };

  QObject::connect(&mainWindow, &NMMainWindow::newProjectRequested,
                   [&runNewProjectDialog]() { runNewProjectDialog(); });

  // Auto-validation helper (light check on project open)
  auto runAutoValidation = [&mainWindow]() {
    auto &pm = NovelMind::editor::ProjectManager::instance();
    if (!pm.hasOpenProject()) {
      return;
    }

    // Run quick validation check
    NovelMind::editor::ProjectIntegrityChecker checker;
    checker.setProjectPath(pm.getProjectPath());
    auto report = checker.runQuickCheck();

    // Only show critical or error issues in diagnostics
    auto *diagnosticsPanel = mainWindow.diagnosticsPanel();
    int criticalCount = 0;
    for (const auto &issue : report.issues) {
      if (issue.severity == NovelMind::editor::IssueSeverity::Critical ||
          issue.severity == NovelMind::editor::IssueSeverity::Error) {
        QString type =
            (issue.severity == NovelMind::editor::IssueSeverity::Critical)
                ? "Critical"
                : "Error";
        QString message = QString::fromStdString(issue.message);
        QString location;
        if (!issue.filePath.empty()) {
          location = "File:" + QString::fromStdString(issue.filePath);
        }
        diagnosticsPanel->addDiagnosticWithLocation(type, message, location);
        criticalCount++;
      }
    }

    if (criticalCount > 0) {
      mainWindow.setStatusMessage(
          QObject::tr(
              "Project validation found %1 issue(s) - check Diagnostics")
              .arg(criticalCount),
          5000);
    }
  };

  QObject::connect(
      &mainWindow, &NMMainWindow::openProjectRequested,
      [&mainWindow, applyProjectAndRemember, runAutoValidation]() {
        const QString path = NMFileDialog::getExistingDirectory(
            &mainWindow, QObject::tr("Open Project"), QDir::homePath());
        if (path.isEmpty()) {
          return;
        }

        auto &projectManager = NovelMind::editor::ProjectManager::instance();
        auto result = projectManager.openProject(path.toStdString());
        if (result.isError()) {
          NMMessageDialog::showError(&mainWindow,
                                     QObject::tr("Open Project Failed"),
                                     QString::fromStdString(result.error()));
          return;
        }

        applyProjectAndRemember();
        runAutoValidation();
      });

  QObject::connect(
      &mainWindow, &NMMainWindow::saveProjectRequested, [&mainWindow]() {
        auto &projectManager = NovelMind::editor::ProjectManager::instance();
        auto result = projectManager.saveProject();
        if (result.isError()) {
          NMMessageDialog::showError(&mainWindow,
                                     QObject::tr("Save Project Failed"),
                                     QString::fromStdString(result.error()));
        }
      });

  // Handle project opening
  QString projectPath;
  bool skipWelcome = parser.isSet(noWelcomeOption);
  bool projectAlreadyOpened = false;

  if (parser.isSet(newProjectOption)) {
    projectPath = parser.value(newProjectOption);
    skipWelcome = true;

    QFileInfo info(projectPath);
    const QString projectName = info.fileName();
    if (!projectName.isEmpty()) {
      auto &projectManager = NovelMind::editor::ProjectManager::instance();
      auto result = projectManager.createProject(
          projectPath.toStdString(), projectName.toStdString(), "empty");
      if (result.isError()) {
        NMMessageDialog::showError(&mainWindow,
                                   QObject::tr("Create Project Failed"),
                                   QString::fromStdString(result.error()));
        projectPath.clear();
      } else {
        applyProjectAndRemember();
        projectAlreadyOpened = true;
      }
    }
  } else if (parser.isSet(openProjectOption)) {
    projectPath = parser.value(openProjectOption);
    skipWelcome = true;
  } else if (!parser.positionalArguments().isEmpty()) {
    projectPath = parser.positionalArguments().first();
    skipWelcome = true;
  }

  // Show welcome dialog if needed
  if (!skipWelcome) {
    QSettings settings("NovelMind", "Editor");
    bool skipWelcomeInFuture =
        settings.value("skipWelcomeScreen", false).toBool();

    if (!skipWelcomeInFuture) {
      NMWelcomeDialog welcomeDialog;
      if (welcomeDialog.exec() == QDialog::Accepted) {
        if (welcomeDialog.shouldCreateNewProject()) {
          core::Logger::instance().info(
              "User requested new project with template: " +
              welcomeDialog.selectedTemplate().toStdString());
          if (runNewProjectDialog(welcomeDialog.selectedTemplate())) {
            auto &projectManager =
                NovelMind::editor::ProjectManager::instance();
            projectPath =
                QString::fromStdString(projectManager.getProjectPath());
            projectAlreadyOpened = true;
          }
        } else if (!welcomeDialog.selectedProjectPath().isEmpty()) {
          projectPath = welcomeDialog.selectedProjectPath();
        }

        if (welcomeDialog.shouldSkipInFuture()) {
          settings.setValue("skipWelcomeScreen", true);
        }
      } else {
        // User closed welcome dialog - exit application
        core::Logger::instance().info("User closed welcome dialog");
        return 0;
      }
    }
  }

  if (!projectPath.isEmpty() && !projectAlreadyOpened) {
    auto &projectManager = NovelMind::editor::ProjectManager::instance();
    auto result = projectManager.openProject(projectPath.toStdString());
    if (result.isError()) {
      NMMessageDialog::showError(&mainWindow,
                                 QObject::tr("Open Project Failed"),
                                 QString::fromStdString(result.error()));
    } else {
      applyProjectAndRemember();
    }
  }

  // D1: Project Gate - Ensure a project is always loaded before showing editor
  auto &projectManager = NovelMind::editor::ProjectManager::instance();
  if (!projectManager.hasOpenProject()) {
    // No project is open - we must have a project to continue
    core::Logger::instance().warning(
        "No project loaded - returning to project selection");

    // Show project picker in a loop until user opens/creates a project or quits
    while (!projectManager.hasOpenProject()) {
      NMWelcomeDialog projectPicker;
      if (projectPicker.exec() == QDialog::Accepted) {
        if (projectPicker.shouldCreateNewProject()) {
          if (runNewProjectDialog(projectPicker.selectedTemplate())) {
            break; // Project created successfully
          }
        } else if (!projectPicker.selectedProjectPath().isEmpty()) {
          auto result = projectManager.openProject(
              projectPicker.selectedProjectPath().toStdString());
          if (!result.isError()) {
            applyProjectAndRemember();
            break; // Project opened successfully
          }
          NMMessageDialog::showError(&mainWindow,
                                     QObject::tr("Open Project Failed"),
                                     QString::fromStdString(result.error()));
        }
      } else {
        // User closed project picker - exit application (cannot use editor
        // without project)
        core::Logger::instance().info(
            "User declined to select a project - exiting application");
        return 0;
      }
    }
  }

  // Run auto-validation on the now-loaded project
  runAutoValidation();

  // Apply default workspace preset on first project load
  QSettings settings("NovelMind", "Editor");
  if (!settings.value("workspace/presetApplied", false).toBool()) {
    mainWindow.resetToDefaultLayout();
    settings.setValue("workspace/presetApplied", true);
  }

  // Show the main window
  core::Logger::instance().info("Showing main window with project: " +
                                projectManager.getProjectName());
  mainWindow.show();

  core::Logger::instance().info("Editor initialized successfully");

  // Run the application event loop
  int result = app.exec();

  // Shutdown
  mainWindow.shutdown();
  core::Logger::instance().info("NovelMind Editor shut down cleanly");

  return result;
}
