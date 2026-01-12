#pragma once

/**
 * @file project_manager.hpp
 * @brief Project Management System for NovelMind Editor
 *
 * Provides comprehensive project management for the editor:
 * - Create, open, save, close projects
 * - Project structure and folder layout
 * - Recent projects tracking
 * - Auto-save functionality
 * - Project metadata management
 *
 * Standard Project Layout:
 *   /ProjectName/
 *     project.json        - Project configuration
 *     /Assets/
 *       /Images/
 *       /Audio/
 *       /Fonts/
 *     /scripts/
 *       /generated/       - Auto-generated scripts from Story Graph
 *     /Scenes/
 *     /Localization/
 *     /Build/             - Build output
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/editor/asset_pipeline.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class ProjectManager;
class EventBus;

/**
 * @brief Playback source mode for story content
 *
 * Determines which data source is used during playback:
 * - Script: NMScript files (.nms) are the authoritative source
 * - Graph: Story Graph visual data is the authoritative source
 * - Mixed: Script is used, but Graph data overrides when present
 *
 * This setting is designed to address issue #82, making the playback
 * source explicit and predictable for users.
 */
enum class PlaybackSourceMode : u8 {
  Script, ///< NMScript files are authoritative (default - original behavior)
  Graph,  ///< Story Graph visual data is authoritative
  Mixed   ///< Script + Graph overrides (Graph wins on conflicts)
};

/**
 * @brief Project metadata structure
 */
struct ProjectMetadata {
  std::string name;
  std::string version = "1.0.0";
  std::string author;
  std::string description;
  std::string engineVersion;
  std::string startScene;

  // Timestamps
  u64 createdAt = 0;
  u64 modifiedAt = 0;
  u64 lastOpenedAt = 0;

  // Settings
  std::string defaultLocale = "en";
  std::string targetResolution = "1920x1080";
  bool fullscreenDefault = false;

  // Build settings
  std::string buildPreset = "release";
  std::vector<std::string> targetPlatforms = {"windows", "linux", "macos"};

  // Playback settings (issue #82)
  PlaybackSourceMode playbackSourceMode = PlaybackSourceMode::Script;
};

/**
 * @brief Recent project entry
 */
struct RecentProject {
  std::string name;
  std::string path;
  u64 lastOpened = 0;
  bool exists = true;    // Whether the project still exists on disk
  std::string thumbnail; // Path to project thumbnail if available
};

/**
 * @brief Project folder types
 */
enum class ProjectFolder : u8 {
  Root,
  Assets,
  Images,
  Audio,
  Fonts,
  Scripts,
  ScriptsGenerated, // Auto-generated scripts from Story Graph (issue #85)
  Scenes,
  Localization,
  Build,
  Temp,
  Backup
};

/**
 * @brief Project state
 */
enum class ProjectState : u8 { Closed, Opening, Open, Saving, Closing };

/**
 * @brief Project validation result
 */
struct ProjectValidation {
  bool valid = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<std::string> missingAssets;
  std::vector<std::string> missingScripts;
};

/**
 * @brief Listener interface for project events
 */
class IProjectListener {
public:
  virtual ~IProjectListener() = default;

  virtual void onProjectCreated(const std::string& /*path*/) {}
  virtual void onProjectOpened(const std::string& /*path*/) {}
  virtual void onProjectClosed() {}
  virtual void onProjectSaved() {}
  virtual void onProjectModified() {}
  virtual void onAutoSaveTriggered() {}
};

/**
 * @brief Project manager singleton
 *
 * Responsibilities:
 * - Manage project lifecycle (create, open, save, close)
 * - Maintain project folder structure
 * - Track recent projects
 * - Handle auto-save
 * - Validate project integrity
 */
class ProjectManager {
public:
  ProjectManager();
  ~ProjectManager();

  // Prevent copying
  ProjectManager(const ProjectManager&) = delete;
  ProjectManager& operator=(const ProjectManager&) = delete;

  /**
   * @brief Get singleton instance
   */
  static ProjectManager& instance();

  // =========================================================================
  // Project Lifecycle
  // =========================================================================

  /**
   * @brief Create a new project
   * @param path Directory path for the project
   * @param name Project name
   * @param templateName Optional template to use
   * @return Success or error
   */
  Result<void> createProject(const std::string& path, const std::string& name,
                             const std::string& templateName = "empty");

  /**
   * @brief Open an existing project
   * @param path Path to project directory or project.json
   * @return Success or error
   */
  Result<void> openProject(const std::string& path);

  /**
   * @brief Save the current project
   * @return Success or error
   */
  Result<void> saveProject();

  /**
   * @brief Save project to a new location
   * @param path New project path
   * @return Success or error
   */
  Result<void> saveProjectAs(const std::string& path);

  /**
   * @brief Close the current project
   * @param force If true, close without saving changes
   * @return Success or error
   */
  Result<void> closeProject(bool force = false);

  /**
   * @brief Check if a project is currently open
   */
  [[nodiscard]] bool hasOpenProject() const;

  /**
   * @brief Get current project state
   */
  [[nodiscard]] ProjectState getState() const;

  /**
   * @brief Check if project has unsaved changes
   */
  [[nodiscard]] bool hasUnsavedChanges() const;

  /**
   * @brief Mark project as modified
   */
  void markModified();

  /**
   * @brief Mark project as saved (unmodified)
   */
  void markSaved();

  // =========================================================================
  // Project Information
  // =========================================================================

  /**
   * @brief Get project metadata
   */
  [[nodiscard]] const ProjectMetadata& getMetadata() const;

  /**
   * @brief Update project metadata
   */
  void setMetadata(const ProjectMetadata& metadata);

  /**
   * @brief Get project root path
   */
  [[nodiscard]] std::string getProjectPath() const;

  /**
   * @brief Get project name
   */
  [[nodiscard]] std::string getProjectName() const;

  /**
   * @brief Get start scene id (entry point)
   */
  [[nodiscard]] std::string getStartScene() const;

  /**
   * @brief Set start scene id (entry point)
   */
  void setStartScene(const std::string& sceneId);

  /**
   * @brief Get path to a specific project folder
   */
  [[nodiscard]] std::string getFolderPath(ProjectFolder folder) const;

  /**
   * @brief Get all project files of a certain type
   * @param extension File extension filter (e.g., ".nms", ".png")
   */
  [[nodiscard]] std::vector<std::string> getProjectFiles(const std::string& extension) const;

  // =========================================================================
  // Folder Structure
  // =========================================================================

  /**
   * @brief Create standard project folder structure
   */
  Result<void> createFolderStructure();

  /**
   * @brief Verify project folder structure exists
   */
  [[nodiscard]] bool verifyFolderStructure() const;

  /**
   * @brief Create a folder within the project
   * @param relativePath Path relative to project root
   */
  Result<void> createFolder(const std::string& relativePath);

  /**
   * @brief Check if a path is within the project
   */
  [[nodiscard]] bool isPathInProject(const std::string& path) const;

  /**
   * @brief Convert absolute path to project-relative path
   */
  [[nodiscard]] std::string toRelativePath(const std::string& absolutePath) const;

  /**
   * @brief Convert project-relative path to absolute path
   */
  [[nodiscard]] std::string toAbsolutePath(const std::string& relativePath) const;

  // =========================================================================
  // Recent Projects
  // =========================================================================

  /**
   * @brief Get list of recent projects
   */
  [[nodiscard]] const std::vector<RecentProject>& getRecentProjects() const;

  /**
   * @brief Add a project to recent list
   */
  void addToRecentProjects(const std::string& path);

  /**
   * @brief Remove a project from recent list
   */
  void removeFromRecentProjects(const std::string& path);

  /**
   * @brief Clear recent projects list
   */
  void clearRecentProjects();

  /**
   * @brief Refresh recent projects (check if they still exist)
   */
  void refreshRecentProjects();

  /**
   * @brief Set maximum number of recent projects to track
   */
  void setMaxRecentProjects(size_t count);

  // =========================================================================
  // Auto-Save
  // =========================================================================

  /**
   * @brief Enable or disable auto-save
   */
  void setAutoSaveEnabled(bool enabled);

  /**
   * @brief Check if auto-save is enabled
   */
  [[nodiscard]] bool isAutoSaveEnabled() const;

  /**
   * @brief Set auto-save interval in seconds
   */
  void setAutoSaveInterval(u32 seconds);

  /**
   * @brief Get auto-save interval
   */
  [[nodiscard]] u32 getAutoSaveInterval() const;

  /**
   * @brief Trigger auto-save check (call from main loop)
   * @param deltaTime Time since last update
   */
  void updateAutoSave(f64 deltaTime);

  /**
   * @brief Force an immediate auto-save
   */
  void triggerAutoSave();

  // =========================================================================
  // Validation
  // =========================================================================

  /**
   * @brief Validate project integrity
   */
  [[nodiscard]] ProjectValidation validateProject() const;

  /**
   * @brief Check if project file exists
   */
  [[nodiscard]] static bool isValidProjectPath(const std::string& path);

  /**
   * @brief Get list of available project templates
   */
  [[nodiscard]] static std::vector<std::string> getAvailableTemplates();

  /**
   * @brief Access the asset database for the current project
   */
  [[nodiscard]] AssetDatabase* assetDatabase() { return &m_assetDatabase; }

  // =========================================================================
  // Backup
  // =========================================================================

  /**
   * @brief Create a backup of the current project
   */
  Result<std::string> createBackup();

  /**
   * @brief Restore from a backup
   * @param backupPath Path to backup
   */
  Result<void> restoreFromBackup(const std::string& backupPath);

  /**
   * @brief Get list of available backups
   */
  [[nodiscard]] std::vector<std::string> getAvailableBackups() const;

  /**
   * @brief Set maximum number of backups to keep
   */
  void setMaxBackups(size_t count);

  // =========================================================================
  // Listeners
  // =========================================================================

  /**
   * @brief Add a project listener
   */
  void addListener(IProjectListener* listener);

  /**
   * @brief Remove a project listener
   */
  void removeListener(IProjectListener* listener);

  // =========================================================================
  // Callbacks
  // =========================================================================

  /**
   * @brief Set callback for unsaved changes prompt
   * Returns true if should save, false to discard, nullopt to cancel
   */
  void setOnUnsavedChangesPrompt(std::function<std::optional<bool>()> callback);

private:
  // Internal methods
  Result<void> loadProjectFile(const std::string& path);
  Result<void> saveProjectFile();
  Result<void> createProjectFromTemplate(const std::string& templateName);
  void updateRecentProjects();
  void notifyProjectCreated();
  void notifyProjectOpened();
  void notifyProjectClosed();
  void notifyProjectSaved();
  void notifyProjectModified();

  // Project state
  ProjectState m_state = ProjectState::Closed;
  std::string m_projectPath;
  ProjectMetadata m_metadata;
  bool m_modified = false;

  // Recent projects
  std::vector<RecentProject> m_recentProjects;
  size_t m_maxRecentProjects = 10;

  // Auto-save
  bool m_autoSaveEnabled = true;
  u32 m_autoSaveIntervalSeconds = 300; // 5 minutes
  f64 m_timeSinceLastSave = 0.0;

  // Asset database
  AssetDatabase m_assetDatabase;

  // Backup
  size_t m_maxBackups = 5;

  // Listeners
  std::vector<IProjectListener*> m_listeners;

  // Callbacks
  std::function<std::optional<bool>()> m_onUnsavedChangesPrompt;

  // Singleton instance
  static std::unique_ptr<ProjectManager> s_instance;
};

/**
 * @brief RAII helper for project operations
 */
class ProjectScope {
public:
  explicit ProjectScope(const std::string& projectPath);
  ~ProjectScope();

  [[nodiscard]] bool isValid() const { return m_valid; }

private:
  bool m_valid = false;
};

} // namespace NovelMind::editor
