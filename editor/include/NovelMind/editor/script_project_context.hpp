#pragma once

/**
 * @file script_project_context.hpp
 * @brief Project context implementation for script validation
 *
 * Provides asset existence checking for the script validator.
 * This class bridges the gap between the editor's asset management
 * and the scripting system's validation needs.
 */

#include "NovelMind/scripting/validator.hpp"
#include <QString>
#include <filesystem>
#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Project context for asset validation in scripts
 *
 * This class implements the IProjectContext interface to provide
 * asset existence checks for the script validator. It searches for
 * assets in the project's asset directories following the standard
 * project structure.
 */
class ScriptProjectContext : public scripting::IProjectContext {
public:
  /**
   * @brief Construct a project context with project root path
   * @param projectPath Path to the project root directory
   */
  explicit ScriptProjectContext(const QString& projectPath);
  ~ScriptProjectContext() override = default;

  /**
   * @brief Set the project path
   */
  void setProjectPath(const QString& projectPath);

  /**
   * @brief Get the project path
   */
  [[nodiscard]] const QString& projectPath() const { return m_projectPath; }

  // IProjectContext interface implementation
  [[nodiscard]] bool backgroundExists(const std::string& assetId) const override;
  [[nodiscard]] bool audioExists(const std::string& assetPath,
                                 const std::string& mediaType) const override;
  [[nodiscard]] bool characterSpriteExists(const std::string& characterId) const override;

  /**
   * @brief Get all available background asset IDs
   * @return List of background asset identifiers (without extension)
   */
  [[nodiscard]] std::vector<std::string> getAvailableBackgrounds() const;

  /**
   * @brief Get all available audio files for a media type
   * @param mediaType Type of media: "music", "sound", "voice"
   * @return List of audio file names
   */
  [[nodiscard]] std::vector<std::string> getAvailableAudio(const std::string& mediaType) const;

  /**
   * @brief Get all available character sprites
   * @return List of character IDs that have sprite assets
   */
  [[nodiscard]] std::vector<std::string> getAvailableCharacters() const;

private:
  QString m_projectPath;

  // Helper methods
  [[nodiscard]] QString assetsPath() const;
  [[nodiscard]] QString backgroundsPath() const;
  [[nodiscard]] QString audioPath(const std::string& mediaType) const;
  [[nodiscard]] QString spritesPath() const;

  // Check if file exists with any of the given extensions
  [[nodiscard]] bool fileExistsWithExtensions(const QString& directory, const QString& baseName,
                                              const std::vector<QString>& extensions) const;

  // Get all files in directory with given extensions
  [[nodiscard]] std::vector<std::string>
  getFilesInDirectory(const QString& directory, const std::vector<QString>& extensions) const;
};

} // namespace NovelMind::editor
