#pragma once

/**
 * @file scene_template_manager.hpp
 * @brief Scene Template System for NovelMind Editor
 *
 * Provides templates for creating common scene types:
 * - Empty Scene: Blank canvas for custom scenes
 * - Dialogue Scene: Background + 2 character positions + dialogue UI
 * - Choice Scene: Background + character + choice menu layout
 * - Cutscene: Fullscreen background, no UI elements
 * - Title Screen: Logo position + menu button layout
 *
 * Templates can be:
 * - Built-in: Shipped with the editor in resources/templates/scenes/
 * - User-defined: Stored per-project in project_path/templates/scenes/
 *
 * Addresses issue #216: Create Scene Templates for Common Workflows
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/scene_document.hpp"
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace NovelMind::editor {

/**
 * @brief Template type indicating origin
 */
enum class SceneTemplateType : uint8_t {
  BuiltIn, ///< Shipped with the editor
  User     ///< Created by user (project-specific)
};

/**
 * @brief Metadata for a scene template
 */
struct SceneTemplateMetadata {
  QString id;                      ///< Unique identifier
  QString name;                    ///< Human-readable display name
  QString description;             ///< Description of what template contains
  QString category;                ///< Category for grouping (e.g., "Standard", "Visual Novel")
  SceneTemplateType type;          ///< Built-in or user-created
  QString previewPath;             ///< Path to preview image
  QStringList tags;                ///< Tags for filtering/searching
  QString author;                  ///< Template author
  QString version;                 ///< Template version
  QDateTime created;               ///< Creation timestamp
  QDateTime modified;              ///< Last modification timestamp

  /**
   * @brief Convert metadata to JSON
   */
  [[nodiscard]] QJsonObject toJson() const;

  /**
   * @brief Load metadata from JSON
   */
  static SceneTemplateMetadata fromJson(const QJsonObject &json);
};

/**
 * @brief Complete scene template with content and metadata
 */
struct SceneTemplate {
  SceneTemplateMetadata metadata;
  SceneDocument content;           ///< The actual scene structure

  /**
   * @brief Convert complete template to JSON
   */
  [[nodiscard]] QJsonObject toJson() const;

  /**
   * @brief Load complete template from JSON
   */
  static SceneTemplate fromJson(const QJsonObject &json);
};

/**
 * @brief Manager for scene templates
 *
 * SceneTemplateManager handles loading, caching, and instantiating
 * scene templates. It supports both built-in templates that ship
 * with the editor and user-created templates stored per-project.
 *
 * Usage:
 * @code
 * SceneTemplateManager manager;
 * manager.loadBuiltInTemplates();
 * manager.loadUserTemplates("/path/to/project");
 *
 * auto templates = manager.getAvailableTemplates();
 * auto template = manager.getTemplate("dialogue_scene");
 *
 * // Create a new scene from template
 * auto sceneDoc = manager.instantiateTemplate("dialogue_scene", "my_scene_id");
 * @endcode
 */
class SceneTemplateManager : public QObject {
  Q_OBJECT

public:
  explicit SceneTemplateManager(QObject *parent = nullptr);
  ~SceneTemplateManager() override;

  // ==========================================================================
  // Template Loading
  // ==========================================================================

  /**
   * @brief Load all built-in templates from editor resources
   * @return Number of templates loaded
   */
  int loadBuiltInTemplates();

  /**
   * @brief Load user-defined templates from a project
   * @param projectPath Path to project root
   * @return Number of user templates loaded
   */
  int loadUserTemplates(const QString &projectPath);

  /**
   * @brief Reload all templates (both built-in and user)
   */
  void reloadAllTemplates();

  /**
   * @brief Clear all loaded templates
   */
  void clearTemplates();

  // ==========================================================================
  // Template Query
  // ==========================================================================

  /**
   * @brief Get list of all available template IDs
   * @return List of template IDs
   */
  [[nodiscard]] QStringList getAvailableTemplateIds() const;

  /**
   * @brief Get list of available templates with metadata
   * @param category Optional category filter
   * @return List of template metadata
   */
  [[nodiscard]] QList<SceneTemplateMetadata>
  getAvailableTemplates(const QString &category = QString()) const;

  /**
   * @brief Get list of all template categories
   * @return Sorted list of unique categories
   */
  [[nodiscard]] QStringList getCategories() const;

  /**
   * @brief Get a specific template by ID
   * @param templateId Template identifier
   * @return Template if found, empty optional otherwise
   */
  [[nodiscard]] std::optional<SceneTemplate>
  getTemplate(const QString &templateId) const;

  /**
   * @brief Get template metadata by ID
   * @param templateId Template identifier
   * @return Metadata if found, empty metadata otherwise
   */
  [[nodiscard]] SceneTemplateMetadata
  getTemplateMetadata(const QString &templateId) const;

  /**
   * @brief Check if a template exists
   * @param templateId Template identifier
   * @return true if template is loaded
   */
  [[nodiscard]] bool hasTemplate(const QString &templateId) const;

  /**
   * @brief Get preview image for a template
   * @param templateId Template identifier
   * @return Preview pixmap, or placeholder if not found
   */
  [[nodiscard]] QPixmap getTemplatePreview(const QString &templateId) const;

  /**
   * @brief Get number of loaded templates
   * @return Template count
   */
  [[nodiscard]] int templateCount() const { return m_templates.size(); }

  // ==========================================================================
  // Template Instantiation
  // ==========================================================================

  /**
   * @brief Create a new scene document from a template
   * @param templateId Template to use
   * @param sceneId ID for the new scene
   * @return SceneDocument configured from template, or error
   */
  [[nodiscard]] Result<SceneDocument>
  instantiateTemplate(const QString &templateId, const QString &sceneId) const;

  /**
   * @brief Create a scene from template and save to file
   * @param templateId Template to use
   * @param sceneId ID for the new scene
   * @param outputPath Path to save the .nmscene file
   * @return Success or error
   */
  [[nodiscard]] Result<void>
  createSceneFromTemplate(const QString &templateId, const QString &sceneId,
                          const QString &outputPath) const;

  // ==========================================================================
  // User Template Management
  // ==========================================================================

  /**
   * @brief Save a scene as a user template
   * @param scene Scene document to save as template
   * @param name Display name for the template
   * @param description Description of the template
   * @param projectPath Project to save template in
   * @return ID of saved template, or error
   */
  [[nodiscard]] Result<QString>
  saveAsUserTemplate(const SceneDocument &scene, const QString &name,
                     const QString &description, const QString &projectPath);

  /**
   * @brief Delete a user template
   * @param templateId Template to delete
   * @param projectPath Project containing the template
   * @return Success or error
   */
  [[nodiscard]] Result<void>
  deleteUserTemplate(const QString &templateId, const QString &projectPath);

  /**
   * @brief Update an existing user template
   * @param templateId Template to update
   * @param scene New scene content
   * @param projectPath Project containing the template
   * @return Success or error
   */
  [[nodiscard]] Result<void>
  updateUserTemplate(const QString &templateId, const SceneDocument &scene,
                     const QString &projectPath);

  // ==========================================================================
  // Configuration
  // ==========================================================================

  /**
   * @brief Set path to built-in templates directory
   * @param path Resource path (e.g., ":/templates/scenes")
   */
  void setBuiltInTemplatesPath(const QString &path);

  /**
   * @brief Get path to built-in templates directory
   * @return Resource path
   */
  [[nodiscard]] QString builtInTemplatesPath() const {
    return m_builtInTemplatesPath;
  }

  /**
   * @brief Get the user templates directory name
   * @return Relative directory name within projects
   */
  static constexpr const char *userTemplatesDir() {
    return "templates/scenes";
  }

signals:
  /**
   * @brief Emitted when templates are reloaded
   */
  void templatesReloaded();

  /**
   * @brief Emitted when a new user template is created
   * @param templateId ID of the new template
   */
  void userTemplateCreated(const QString &templateId);

  /**
   * @brief Emitted when a user template is deleted
   * @param templateId ID of the deleted template
   */
  void userTemplateDeleted(const QString &templateId);

  /**
   * @brief Emitted when a user template is updated
   * @param templateId ID of the updated template
   */
  void userTemplateUpdated(const QString &templateId);

private:
  /**
   * @brief Load a single template from file
   * @param filePath Path to template JSON file
   * @param type Template type (built-in or user)
   * @return Loaded template, or nullopt on failure
   */
  std::optional<SceneTemplate> loadTemplateFromFile(const QString &filePath,
                                                    SceneTemplateType type);

  /**
   * @brief Generate a unique template ID from a name
   * @param name Template name
   * @return Sanitized ID
   */
  QString generateTemplateId(const QString &name) const;

  /**
   * @brief Create built-in templates programmatically
   * These are used if resource files are not available
   */
  void createDefaultBuiltInTemplates();

  /**
   * @brief Create the empty scene template
   */
  SceneTemplate createEmptySceneTemplate() const;

  /**
   * @brief Create the dialogue scene template
   */
  SceneTemplate createDialogueSceneTemplate() const;

  /**
   * @brief Create the choice scene template
   */
  SceneTemplate createChoiceSceneTemplate() const;

  /**
   * @brief Create the cutscene template
   */
  SceneTemplate createCutsceneTemplate() const;

  /**
   * @brief Create the title screen template
   */
  SceneTemplate createTitleScreenTemplate() const;

  /**
   * @brief Generate placeholder preview for a template
   * @param templateName Name to display on preview
   * @return Generated preview pixmap
   */
  QPixmap generatePlaceholderPreview(const QString &templateName) const;

  QHash<QString, SceneTemplate> m_templates; ///< Loaded templates by ID
  QHash<QString, QPixmap> m_previewCache;    ///< Cached preview images
  QString m_builtInTemplatesPath = ":/templates/scenes";
  QString m_currentProjectPath;

  static constexpr const char *TEMPLATE_FILE_EXTENSION = ".nmscene_template";
  static constexpr int PREVIEW_WIDTH = 256;
  static constexpr int PREVIEW_HEIGHT = 144;
};

} // namespace NovelMind::editor
