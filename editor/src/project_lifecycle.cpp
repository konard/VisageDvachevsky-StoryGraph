#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/project_json.hpp"
#include <ctime>
#include <fstream>

namespace NovelMind::editor {

// ============================================================================
// Project Lifecycle
// ============================================================================

Result<void> ProjectManager::createProject(const std::string& path, const std::string& name,
                                           const std::string& templateName) {
  if (m_state != ProjectState::Closed) {
    return Result<void>::error("A project is already open. Close it first.");
  }

  namespace fs = std::filesystem;

  // Check if path exists
  if (fs::exists(path)) {
    // Check if it's empty
    if (!fs::is_empty(path)) {
      return Result<void>::error("Directory is not empty: " + path);
    }
  } else {
    // Create directory
    std::error_code ec;
    if (!fs::create_directories(path, ec)) {
      return Result<void>::error("Failed to create directory: " + ec.message());
    }
  }

  m_projectPath = fs::absolute(path).string();
  m_metadata = ProjectMetadata();
  m_metadata.name = name;
  m_metadata.createdAt = static_cast<u64>(std::time(nullptr));
  m_metadata.modifiedAt = m_metadata.createdAt;
  m_metadata.lastOpenedAt = m_metadata.createdAt;
  m_metadata.engineVersion = "0.2.0";

  // Create folder structure
  auto result = createFolderStructure();
  if (result.isError()) {
    return result;
  }

  // Create from template if specified
  if (templateName != "empty" && !templateName.empty()) {
    result = createProjectFromTemplate(templateName);
    if (result.isError()) {
      // Clean up on failure
      fs::remove_all(m_projectPath);
      return result;
    }
  }

  // Save project file
  result = saveProjectFile();
  if (result.isError()) {
    return result;
  }

  m_state = ProjectState::Open;
  m_modified = false;

  m_assetDatabase.initialize(m_projectPath);

  addToRecentProjects(m_projectPath);
  notifyProjectCreated();

  return Result<void>::ok();
}

Result<void> ProjectManager::openProject(const std::string& path) {
  if (m_state != ProjectState::Closed) {
    auto closeResult = closeProject();
    if (closeResult.isError()) {
      return closeResult;
    }
  }

  m_state = ProjectState::Opening;

  namespace fs = std::filesystem;

  std::string projectFilePath = path;

  // If path is a directory, look for project.json
  if (fs::is_directory(path)) {
    projectFilePath = (fs::path(path) / "project.json").string();
  }

  if (!fs::exists(projectFilePath)) {
    m_state = ProjectState::Closed;
    return Result<void>::error("Project file not found: " + projectFilePath);
  }

  auto result = loadProjectFile(projectFilePath);
  if (result.isError()) {
    m_state = ProjectState::Closed;
    return result;
  }

  m_projectPath = fs::path(projectFilePath).parent_path().string();
  m_metadata.lastOpenedAt = static_cast<u64>(std::time(nullptr));

  // Verify folder structure
  if (!verifyFolderStructure()) {
    // Try to repair
    auto repairResult = createFolderStructure();
    if (repairResult.isError()) {
      m_state = ProjectState::Closed;
      return Result<void>::error("Project folder structure is invalid and could not be "
                                 "repaired");
    }
  }

  m_state = ProjectState::Open;
  m_modified = false;
  m_timeSinceLastSave = 0.0;

  m_assetDatabase.initialize(m_projectPath);

  addToRecentProjects(m_projectPath);
  notifyProjectOpened();

  return Result<void>::ok();
}

Result<void> ProjectManager::saveProject() {
  if (m_state != ProjectState::Open) {
    return Result<void>::error("No project is open");
  }

  m_state = ProjectState::Saving;

  m_metadata.modifiedAt = static_cast<u64>(std::time(nullptr));

  auto result = saveProjectFile();
  if (result.isError()) {
    m_state = ProjectState::Open;
    return result;
  }

  m_state = ProjectState::Open;
  m_modified = false;
  m_timeSinceLastSave = 0.0;

  notifyProjectSaved();

  return Result<void>::ok();
}

Result<void> ProjectManager::saveProjectAs(const std::string& path) {
  if (m_state != ProjectState::Open) {
    return Result<void>::error("No project is open");
  }

  namespace fs = std::filesystem;

  // Copy current project to new location
  std::error_code ec;
  fs::copy(m_projectPath, path, fs::copy_options::recursive | fs::copy_options::overwrite_existing,
           ec);
  if (ec) {
    return Result<void>::error("Failed to copy project: " + ec.message());
  }

  m_projectPath = fs::absolute(path).string();

  return saveProject();
}

Result<void> ProjectManager::closeProject(bool force) {
  if (m_state == ProjectState::Closed) {
    return Result<void>::ok();
  }

  if (!force && m_modified) {
    if (m_onUnsavedChangesPrompt) {
      auto promptResult = m_onUnsavedChangesPrompt();
      if (!promptResult.has_value()) {
        // User cancelled
        return Result<void>::error("Operation cancelled by user");
      }
      if (promptResult.value()) {
        // User wants to save
        auto saveResult = saveProject();
        if (saveResult.isError()) {
          return saveResult;
        }
      }
    }
  }

  m_state = ProjectState::Closing;

  // Clear project state
  m_assetDatabase.close();
  m_projectPath.clear();
  m_metadata = ProjectMetadata();
  m_modified = false;
  m_timeSinceLastSave = 0.0;

  m_state = ProjectState::Closed;

  notifyProjectClosed();

  return Result<void>::ok();
}

bool ProjectManager::hasOpenProject() const {
  return m_state == ProjectState::Open;
}

ProjectState ProjectManager::getState() const {
  return m_state;
}

bool ProjectManager::hasUnsavedChanges() const {
  return m_modified;
}

void ProjectManager::markModified() {
  if (!m_modified) {
    m_modified = true;
    notifyProjectModified();
  }
}

void ProjectManager::markSaved() {
  m_modified = false;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

Result<void> ProjectManager::loadProjectFile(const std::string& path) {
  // Use robust JSON parser with validation
  auto result = ProjectJsonHandler::loadFromFile(path, m_metadata);
  if (result.isError()) {
    return Result<void>::error("Failed to load project file: " + path + " - " + result.error());
  }

  return Result<void>::ok();
}

Result<void> ProjectManager::saveProjectFile() {
  namespace fs = std::filesystem;

  fs::path projectFile = fs::path(m_projectPath) / "project.json";

  // Use atomic write with validation
  auto result = ProjectJsonHandler::saveToFile(projectFile.string(), m_metadata);
  if (result.isError()) {
    return Result<void>::error("Failed to save project file: " + result.error());
  }

  return Result<void>::ok();
}

} // namespace NovelMind::editor
