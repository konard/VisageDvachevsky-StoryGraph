#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_json.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<ProjectManager> ProjectManager::s_instance = nullptr;

ProjectManager::ProjectManager() {
  m_recentProjects.reserve(m_maxRecentProjects);
}

ProjectManager::~ProjectManager() {
  if (m_state == ProjectState::Open) {
    closeProject(true);
  }
}

ProjectManager &ProjectManager::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<ProjectManager>();
  }
  return *s_instance;
}

// ============================================================================
// Project Information
// ============================================================================

const ProjectMetadata &ProjectManager::getMetadata() const {
  return m_metadata;
}

void ProjectManager::setMetadata(const ProjectMetadata &metadata) {
  m_metadata = metadata;
  markModified();
}

std::string ProjectManager::getProjectPath() const { return m_projectPath; }

std::string ProjectManager::getProjectName() const { return m_metadata.name; }

std::string ProjectManager::getStartScene() const {
  return m_metadata.startScene;
}

void ProjectManager::setStartScene(const std::string &sceneId) {
  if (m_metadata.startScene == sceneId) {
    return;
  }
  m_metadata.startScene = sceneId;
  markModified();
}

// ============================================================================
// Recent Projects
// ============================================================================

const std::vector<RecentProject> &ProjectManager::getRecentProjects() const {
  return m_recentProjects;
}

void ProjectManager::addToRecentProjects(const std::string &path) {
  namespace fs = std::filesystem;

  // Remove existing entry for this path
  removeFromRecentProjects(path);

  // Add new entry at front
  RecentProject recent;
  recent.path = path;
  recent.lastOpened = static_cast<u64>(std::time(nullptr));
  recent.exists = fs::exists(path);

  // Try to get project name from metadata
  fs::path projectFile = fs::path(path) / "project.json";
  if (fs::exists(projectFile)) {
    // Would parse project name from file
    recent.name = fs::path(path).filename().string();
  } else {
    recent.name = fs::path(path).filename().string();
  }

  m_recentProjects.insert(m_recentProjects.begin(), recent);

  // Trim to max size
  if (m_recentProjects.size() > m_maxRecentProjects) {
    m_recentProjects.resize(m_maxRecentProjects);
  }
}

void ProjectManager::removeFromRecentProjects(const std::string &path) {
  m_recentProjects.erase(std::remove_if(m_recentProjects.begin(),
                                        m_recentProjects.end(),
                                        [&path](const RecentProject &p) {
                                          return p.path == path;
                                        }),
                         m_recentProjects.end());
}

void ProjectManager::clearRecentProjects() { m_recentProjects.clear(); }

void ProjectManager::refreshRecentProjects() {
  namespace fs = std::filesystem;

  for (auto &project : m_recentProjects) {
    project.exists = fs::exists(project.path);
  }
}

void ProjectManager::setMaxRecentProjects(size_t count) {
  m_maxRecentProjects = count;
  if (m_recentProjects.size() > m_maxRecentProjects) {
    m_recentProjects.resize(m_maxRecentProjects);
  }
}

// ============================================================================
// Auto-Save
// ============================================================================

void ProjectManager::setAutoSaveEnabled(bool enabled) {
  m_autoSaveEnabled = enabled;
}

bool ProjectManager::isAutoSaveEnabled() const { return m_autoSaveEnabled; }

void ProjectManager::setAutoSaveInterval(u32 seconds) {
  m_autoSaveIntervalSeconds = seconds;
}

u32 ProjectManager::getAutoSaveInterval() const {
  return m_autoSaveIntervalSeconds;
}

void ProjectManager::updateAutoSave(f64 deltaTime) {
  if (!m_autoSaveEnabled || m_state != ProjectState::Open || !m_modified) {
    return;
  }

  m_timeSinceLastSave += deltaTime;

  if (m_timeSinceLastSave >= static_cast<f64>(m_autoSaveIntervalSeconds)) {
    triggerAutoSave();
  }
}

void ProjectManager::triggerAutoSave() {
  if (m_state != ProjectState::Open) {
    return;
  }

  // Create backup before auto-save
  createBackup();

  // Save project
  auto result = saveProject();
  if (result.isOk()) {
    for (auto *listener : m_listeners) {
      listener->onAutoSaveTriggered();
    }
  }
}

// ============================================================================
// Validation
// ============================================================================

ProjectValidation ProjectManager::validateProject() const {
  ProjectValidation validation;

  if (m_projectPath.empty()) {
    validation.valid = false;
    validation.errors.push_back("No project is open");
    return validation;
  }

  // Use ProjectIntegrityChecker for comprehensive validation
  ProjectIntegrityChecker checker;
  checker.setProjectPath(m_projectPath);

  // Configure for full validation
  IntegrityCheckConfig config;
  config.checkScenes = true;
  config.checkAssets = true;
  config.checkVoiceLines = true;
  config.checkLocalization = true;
  config.checkStoryGraph = true;
  config.checkScripts = true;
  config.checkResources = true;
  config.checkConfiguration = true;
  config.reportUnreferencedAssets = true;
  config.reportUnreachableNodes = true;
  config.reportCycles = true;
  config.reportMissingTranslations = true;
  checker.setConfig(config);

  // Run validation
  IntegrityReport report = checker.runFullCheck();

  // Convert IntegrityReport to ProjectValidation
  validation.valid = report.passed;

  for (const auto &issue : report.issues) {
    std::string location;
    if (!issue.filePath.empty()) {
      location = issue.filePath;
      if (issue.lineNumber > 0) {
        location += ":" + std::to_string(issue.lineNumber);
      }
    }

    std::string message = issue.message;
    if (!location.empty()) {
      message += " (" + location + ")";
    }

    switch (issue.severity) {
    case IssueSeverity::Critical:
    case IssueSeverity::Error:
      validation.errors.push_back(message);
      break;
    case IssueSeverity::Warning:
      validation.warnings.push_back(message);
      break;
    case IssueSeverity::Info:
      // Info messages are not included in basic validation
      break;
    }

    // Track missing assets and scripts
    if (issue.category == IssueCategory::Asset && issue.code == "A002") {
      // Extract asset name from message
      size_t pos = message.find("not found: ");
      if (pos != std::string::npos) {
        validation.missingAssets.push_back(message.substr(pos + 11));
      }
    }
    if (issue.category == IssueCategory::Scene && issue.code == "S002") {
      // Extract scene name from message
      size_t pos = message.find("undefined scene: ");
      if (pos != std::string::npos) {
        validation.missingScripts.push_back(message.substr(pos + 17));
      }
    }
  }

  return validation;
}

bool ProjectManager::isValidProjectPath(const std::string &path) {
  namespace fs = std::filesystem;

  fs::path projectFile = fs::path(path);

  // If it's a directory, look for project.json
  if (fs::is_directory(path)) {
    projectFile = projectFile / "project.json";
  }

  return fs::exists(projectFile) && fs::is_regular_file(projectFile);
}

// ============================================================================
// Backup
// ============================================================================

Result<std::string> ProjectManager::createBackup() {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return Result<std::string>::error("No project is open");
  }

  fs::path backupDir = fs::path(m_projectPath) / ".backup";
  if (!fs::exists(backupDir)) {
    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec) {
      return Result<std::string>::error("Failed to create backup directory: " +
                                        ec.message());
    }
  }

  // Create timestamped backup name
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << "backup_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
  std::string backupName = ss.str();

  fs::path backupPath = backupDir / backupName;

  std::error_code ec;
  fs::create_directory(backupPath, ec);
  if (ec) {
    return Result<std::string>::error("Failed to create backup: " +
                                      ec.message());
  }

  // Copy project files (excluding backup and build directories)
  for (const auto &entry : fs::directory_iterator(m_projectPath)) {
    if (entry.path().filename() == ".backup" ||
        entry.path().filename() == ".temp" ||
        entry.path().filename() == "Build") {
      continue;
    }

    fs::copy(entry.path(), backupPath / entry.path().filename(),
             fs::copy_options::recursive, ec);
    if (ec) {
      // Log warning but continue
      ec.clear();
    }
  }

  // Trim old backups
  auto backups = getAvailableBackups();
  if (backups.size() > m_maxBackups) {
    for (size_t i = m_maxBackups; i < backups.size(); ++i) {
      fs::remove_all(backups[i], ec);
    }
  }

  return Result<std::string>::ok(backupPath.string());
}

Result<void> ProjectManager::restoreFromBackup(const std::string &backupPath) {
  namespace fs = std::filesystem;

  if (!fs::exists(backupPath)) {
    return Result<void>::error("Backup not found: " + backupPath);
  }

  if (m_projectPath.empty()) {
    return Result<void>::error("No project is open");
  }

  std::error_code ec;
  fs::path backupRoot(backupPath);
  fs::path projectRoot(m_projectPath);

  for (const auto &entry : fs::directory_iterator(backupRoot)) {
    fs::path target = projectRoot / entry.path().filename();
    fs::copy(entry.path(), target,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing,
             ec);
    if (ec) {
      return Result<void>::error("Failed to restore backup: " + ec.message());
    }
  }

  markModified();
  return Result<void>::ok();
}

std::vector<std::string> ProjectManager::getAvailableBackups() const {
  namespace fs = std::filesystem;

  std::vector<std::string> backups;

  if (m_projectPath.empty()) {
    return backups;
  }

  fs::path backupDir = fs::path(m_projectPath) / ".backup";
  if (!fs::exists(backupDir)) {
    return backups;
  }

  for (const auto &entry : fs::directory_iterator(backupDir)) {
    if (entry.is_directory()) {
      backups.push_back(entry.path().string());
    }
  }

  // Sort by name (newest first due to timestamp naming)
  std::sort(backups.begin(), backups.end(), std::greater<>());

  return backups;
}

void ProjectManager::setMaxBackups(size_t count) { m_maxBackups = count; }

// ============================================================================
// Listeners
// ============================================================================

void ProjectManager::addListener(IProjectListener *listener) {
  if (listener && std::find(m_listeners.begin(), m_listeners.end(), listener) ==
                      m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void ProjectManager::removeListener(IProjectListener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}

void ProjectManager::setOnUnsavedChangesPrompt(
    std::function<std::optional<bool>()> callback) {
  m_onUnsavedChangesPrompt = std::move(callback);
}

// ============================================================================
// Private Methods - Notification
// ============================================================================

void ProjectManager::notifyProjectCreated() {
  for (auto *listener : m_listeners) {
    listener->onProjectCreated(m_projectPath);
  }
}

void ProjectManager::notifyProjectOpened() {
  for (auto *listener : m_listeners) {
    listener->onProjectOpened(m_projectPath);
  }
}

void ProjectManager::notifyProjectClosed() {
  for (auto *listener : m_listeners) {
    listener->onProjectClosed();
  }
}

void ProjectManager::notifyProjectSaved() {
  for (auto *listener : m_listeners) {
    listener->onProjectSaved();
  }
}

void ProjectManager::notifyProjectModified() {
  for (auto *listener : m_listeners) {
    listener->onProjectModified();
  }
}

// ============================================================================
// ProjectScope Implementation
// ============================================================================

ProjectScope::ProjectScope(const std::string &projectPath) {
  auto result = ProjectManager::instance().openProject(projectPath);
  m_valid = result.isOk();
}

ProjectScope::~ProjectScope() {
  if (m_valid) {
    ProjectManager::instance().closeProject(true);
  }
}

} // namespace NovelMind::editor
