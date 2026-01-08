#pragma once

/**
 * @file scene_registry.hpp
 * @brief Centralized Scene Registry System for NovelMind Editor
 *
 * Provides centralized management of scenes in a project:
 * - Scene registration and unregistration
 * - Scene metadata tracking (name, path, timestamps, tags)
 * - Scene ID validation
 * - Thumbnail generation and caching
 * - Persistence via scene_registry.json
 *
 * Addresses issue #206: Implement Scene Registry System
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>

namespace NovelMind::editor {

/**
 * @brief Metadata for a registered scene
 */
struct SceneMetadata {
  QString id;            ///< Unique ID of the scene
  QString name;          ///< Human-readable display name
  QString documentPath;  ///< Relative path to .nmscene file
  QString thumbnailPath; ///< Relative path to thumbnail image
  QDateTime created;     ///< Creation timestamp
  QDateTime modified;    ///< Last modification timestamp
  QStringList tags;      ///< Tags for categorization
  QString description;   ///< Optional description

  /**
   * @brief Convert metadata to JSON object
   */
  [[nodiscard]] QJsonObject toJson() const;

  /**
   * @brief Load metadata from JSON object
   */
  static SceneMetadata fromJson(const QJsonObject &json);
};

/**
 * @brief Centralized registry for managing scenes in a project
 *
 * SceneRegistry provides:
 * - Registration and lookup of scenes by ID
 * - Automatic thumbnail generation
 * - Validation of scene references
 * - Persistence to scene_registry.json
 *
 * Usage:
 * @code
 * SceneRegistry registry;
 * registry.load("/path/to/project");
 *
 * // Register a new scene
 * QString sceneId = registry.registerScene("My Scene");
 *
 * // Check if scene exists
 * if (registry.sceneExists(sceneId)) {
 *     auto metadata = registry.getSceneMetadata(sceneId);
 *     // ...
 * }
 *
 * registry.save("/path/to/project");
 * @endcode
 */
class SceneRegistry : public QObject {
  Q_OBJECT

public:
  explicit SceneRegistry(QObject *parent = nullptr);
  ~SceneRegistry() override;

  // ==========================================================================
  // Scene Management
  // ==========================================================================

  /**
   * @brief Register a new scene and return its ID
   * @param name Human-readable name for the scene
   * @param basePath Optional base path for the scene file (relative to Scenes/)
   * @return Unique scene ID
   */
  QString registerScene(const QString &name, const QString &basePath = QString());

  /**
   * @brief Check if a scene with the given ID exists
   * @param sceneId Scene ID to check
   * @return true if scene is registered
   */
  [[nodiscard]] bool sceneExists(const QString &sceneId) const;

  /**
   * @brief Get metadata for a registered scene
   * @param sceneId Scene ID
   * @return Scene metadata (empty if not found)
   */
  [[nodiscard]] SceneMetadata getSceneMetadata(const QString &sceneId) const;

  /**
   * @brief Get path to the .nmscene document
   * @param sceneId Scene ID
   * @return Relative path to document, or empty if not found
   */
  [[nodiscard]] QString getSceneDocumentPath(const QString &sceneId) const;

  /**
   * @brief Get path to the scene's thumbnail
   * @param sceneId Scene ID
   * @return Relative path to thumbnail, or empty if not found
   */
  [[nodiscard]] QString getSceneThumbnailPath(const QString &sceneId) const;

  /**
   * @brief Rename a scene
   * @param sceneId Current scene ID
   * @param newName New display name
   * @return true on success
   */
  bool renameScene(const QString &sceneId, const QString &newName);

  /**
   * @brief Unregister a scene (removes from registry but not files)
   * @param sceneId Scene ID to unregister
   * @return true on success
   */
  bool unregisterScene(const QString &sceneId);

  /**
   * @brief Get list of all registered scene IDs
   * @return List of scene IDs
   */
  [[nodiscard]] QStringList getAllSceneIds() const;

  /**
   * @brief Get scenes filtered by tags
   * @param tags Tags to filter by (empty for all)
   * @return List of matching scene metadata
   */
  [[nodiscard]] QList<SceneMetadata> getScenes(const QStringList &tags = QStringList()) const;

  /**
   * @brief Update metadata for an existing scene
   * @param sceneId Scene ID
   * @param metadata Updated metadata
   * @return true on success
   */
  bool updateSceneMetadata(const QString &sceneId, const SceneMetadata &metadata);

  // ==========================================================================
  // Thumbnail Management
  // ==========================================================================

  /**
   * @brief Generate thumbnail for a scene from its .nmscene document
   * @param sceneId Scene ID
   * @param size Thumbnail size (default 256x144 for 16:9)
   * @return true on success
   */
  bool generateThumbnail(const QString &sceneId, const QSize &size = QSize(256, 144));

  /**
   * @brief Clear all cached thumbnails
   */
  void clearThumbnailCache();

  /**
   * @brief Get absolute path to thumbnail for a scene
   * @param sceneId Scene ID
   * @return Absolute path to thumbnail, or empty if not found
   */
  [[nodiscard]] QString getAbsoluteThumbnailPath(const QString &sceneId) const;

  // ==========================================================================
  // Validation
  // ==========================================================================

  /**
   * @brief Validate all scenes in the registry
   * @return List of error messages (empty if all valid)
   */
  [[nodiscard]] QStringList validateScenes() const;

  /**
   * @brief Find .nmscene files not registered in the registry
   * @return List of orphaned file paths
   */
  [[nodiscard]] QStringList findOrphanedScenes() const;

  /**
   * @brief Find scene IDs with missing .nmscene documents
   * @return List of scene IDs with broken references
   */
  [[nodiscard]] QStringList findBrokenReferences() const;

  // ==========================================================================
  // Persistence
  // ==========================================================================

  /**
   * @brief Load registry from project
   * @param projectPath Path to project root
   * @return true on success
   */
  bool load(const QString &projectPath);

  /**
   * @brief Save registry to project
   * @param projectPath Path to project root (uses loaded path if empty)
   * @return true on success
   */
  bool save(const QString &projectPath = QString());

  /**
   * @brief Export registry to JSON
   * @return JSON object containing registry data
   */
  [[nodiscard]] QJsonObject toJson() const;

  /**
   * @brief Import registry from JSON
   * @param json JSON object containing registry data
   * @return true on success
   */
  bool fromJson(const QJsonObject &json);

  /**
   * @brief Get the project path this registry is associated with
   * @return Project path
   */
  [[nodiscard]] QString projectPath() const { return m_projectPath; }

  /**
   * @brief Check if registry has been modified since last save
   * @return true if modified
   */
  [[nodiscard]] bool isModified() const { return m_modified; }

  /**
   * @brief Get the number of registered scenes
   * @return Number of scenes
   */
  [[nodiscard]] int sceneCount() const { return m_scenes.size(); }

signals:
  /**
   * @brief Emitted when a new scene is registered
   * @param sceneId ID of the registered scene
   */
  void sceneRegistered(const QString &sceneId);

  /**
   * @brief Emitted when a scene is renamed
   * @param sceneId ID of the scene (unchanged)
   * @param newName New display name
   */
  void sceneRenamed(const QString &sceneId, const QString &newName);

  /**
   * @brief Emitted when a scene is unregistered
   * @param sceneId ID of the unregistered scene
   */
  void sceneUnregistered(const QString &sceneId);

  /**
   * @brief Emitted when scene metadata changes
   * @param sceneId ID of the modified scene
   */
  void sceneMetadataChanged(const QString &sceneId);

  /**
   * @brief Emitted when a scene's thumbnail is updated
   * @param sceneId ID of the scene
   */
  void sceneThumbnailUpdated(const QString &sceneId);

  /**
   * @brief Emitted when the registry is loaded
   */
  void registryLoaded();

  /**
   * @brief Emitted when the registry is saved
   */
  void registrySaved();

private:
  /**
   * @brief Generate a unique scene ID from a base name
   * @param baseName Base name for the ID
   * @return Unique scene ID
   */
  QString generateUniqueSceneId(const QString &baseName) const;

  /**
   * @brief Update the modified timestamp for a scene
   * @param sceneId Scene ID
   */
  void updateModifiedTime(const QString &sceneId);

  /**
   * @brief Sanitize a name for use as a file-safe ID
   * @param name Name to sanitize
   * @return Sanitized ID string
   */
  static QString sanitizeForId(const QString &name);

  QHash<QString, SceneMetadata> m_scenes; ///< Scene metadata by ID
  QString m_projectPath;                  ///< Project root path
  QString m_registryFilePath;             ///< Path to scene_registry.json
  bool m_modified = false;                ///< Dirty flag

  static constexpr const char *REGISTRY_VERSION = "1.0";
  static constexpr const char *REGISTRY_FILENAME = "scene_registry.json";
  static constexpr const char *THUMBNAILS_DIR = "Scenes/.thumbnails";
};

} // namespace NovelMind::editor
