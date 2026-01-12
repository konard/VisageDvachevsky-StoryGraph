#pragma once

/**
 * @file asset_pipeline.hpp
 * @brief Asset Pipeline - Import, manage, and track assets
 *
 * Provides:
 * - Asset importers for different file types
 * - Asset database for tracking resources
 * - Hot reload support
 * - Import settings management
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class AssetDatabase;

/**
 * @brief Asset type enumeration
 */
enum class AssetType : u8 { Unknown, Image, Audio, Font, Script, Scene, Localization, Data };

/**
 * @brief Image compression format
 */
enum class ImageCompression : u8 {
  None, // Uncompressed
  DXT,  // Desktop GPU compression
  ETC2, // Mobile GPU compression
  ASTC, // Advanced GPU compression
  PNG,  // PNG compression (lossless)
  JPEG  // JPEG compression (lossy)
};

/**
 * @brief Audio format for export
 */
enum class AudioFormat : u8 {
  WAV, // Uncompressed
  OGG, // Ogg Vorbis
  MP3, // MP3 (if licensing permits)
  OPUS // Opus codec
};

/**
 * @brief Import settings for images
 */
struct ImageImportSettings {
  ImageCompression compression = ImageCompression::PNG;
  bool generateMipmaps = false;
  bool premultiplyAlpha = true;
  i32 maxWidth = 4096;
  i32 maxHeight = 4096;
  bool powerOfTwo = false;
  f32 compressionQuality = 0.8f;

  // Sprite sheet settings
  bool isSpriteSheet = false;
  i32 spriteWidth = 0;
  i32 spriteHeight = 0;
};

/**
 * @brief Import settings for audio
 */
struct AudioImportSettings {
  AudioFormat format = AudioFormat::OGG;
  bool streaming = false; // Large files should stream
  f32 quality = 0.7f;     // Compression quality
  bool mono = false;      // Force mono (for 3D audio)
  i32 sampleRate = 44100; // Target sample rate
  bool normalize = false; // Normalize volume
};

/**
 * @brief Import settings for fonts
 */
struct FontImportSettings {
  std::vector<i32> sizes = {12, 14, 16, 18, 24, 32, 48};
  std::string charset = "ascii"; // ascii, latin1, unicode
  bool antialiased = true;
  i32 padding = 2;
  bool generateSDF = false; // Signed Distance Field
};

/**
 * @brief Asset metadata stored in database
 */
struct AssetMetadata {
  std::string id;           // Unique asset ID
  std::string name;         // Display name
  std::string sourcePath;   // Original file path
  std::string importedPath; // Path in project assets
  AssetType type = AssetType::Unknown;
  u64 sourceModifiedTime = 0; // Last modification time of source
  u64 importedTime = 0;       // Time of last import
  u64 fileSize = 0;
  std::string checksum; // File checksum for change detection

  // References
  std::vector<std::string> dependsOn;    // Assets this depends on
  std::vector<std::string> referencedBy; // Assets that reference this

  // Type-specific settings stored as JSON
  std::string importSettingsJson;

  // Tags for organization
  std::vector<std::string> tags;

  // Thumbnail path (generated during import)
  std::string thumbnailPath;
};

/**
 * @brief Base class for asset importers
 */
class IAssetImporter {
public:
  virtual ~IAssetImporter() = default;

  /**
   * @brief Get supported file extensions
   */
  [[nodiscard]] virtual std::vector<std::string> getSupportedExtensions() const = 0;

  /**
   * @brief Get the asset type this importer produces
   */
  [[nodiscard]] virtual AssetType getAssetType() const = 0;

  /**
   * @brief Check if a file can be imported by this importer
   */
  [[nodiscard]] virtual bool canImport(const std::string& path) const = 0;

  /**
   * @brief Import an asset
   * @param sourcePath Path to source file
   * @param destPath Path to write imported asset
   * @param database Asset database for registration
   * @return Metadata of imported asset or error
   */
  [[nodiscard]] virtual Result<AssetMetadata>
  import(const std::string& sourcePath, const std::string& destPath, AssetDatabase* database) = 0;

  /**
   * @brief Reimport an asset (update existing)
   */
  [[nodiscard]] virtual Result<AssetMetadata> reimport(const AssetMetadata& existing,
                                                       AssetDatabase* database) = 0;

  /**
   * @brief Get default import settings as JSON
   */
  [[nodiscard]] virtual std::string getDefaultSettingsJson() const = 0;
};

/**
 * @brief Image asset importer
 */
class ImageImporter : public IAssetImporter {
public:
  ImageImporter();
  ~ImageImporter() override = default;

  [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override;
  [[nodiscard]] AssetType getAssetType() const override;
  [[nodiscard]] bool canImport(const std::string& path) const override;
  [[nodiscard]] Result<AssetMetadata> import(const std::string& sourcePath,
                                             const std::string& destPath,
                                             AssetDatabase* database) override;
  [[nodiscard]] Result<AssetMetadata> reimport(const AssetMetadata& existing,
                                               AssetDatabase* database) override;
  [[nodiscard]] std::string getDefaultSettingsJson() const override;

  /**
   * @brief Set import settings for next import
   */
  void setSettings(const ImageImportSettings& settings);
  [[nodiscard]] const ImageImportSettings& getSettings() const;

private:
  Result<void> processImage(const std::string& sourcePath, const std::string& destPath);
  Result<void> generateThumbnail(const std::string& sourcePath, const std::string& thumbnailPath);

  ImageImportSettings m_settings;
};

/**
 * @brief Audio asset importer
 */
class AudioImporter : public IAssetImporter {
public:
  AudioImporter();
  ~AudioImporter() override = default;

  [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override;
  [[nodiscard]] AssetType getAssetType() const override;
  [[nodiscard]] bool canImport(const std::string& path) const override;
  [[nodiscard]] Result<AssetMetadata> import(const std::string& sourcePath,
                                             const std::string& destPath,
                                             AssetDatabase* database) override;
  [[nodiscard]] Result<AssetMetadata> reimport(const AssetMetadata& existing,
                                               AssetDatabase* database) override;
  [[nodiscard]] std::string getDefaultSettingsJson() const override;

  void setSettings(const AudioImportSettings& settings);
  [[nodiscard]] const AudioImportSettings& getSettings() const;

private:
  Result<void> processAudio(const std::string& sourcePath, const std::string& destPath);

  AudioImportSettings m_settings;
};

/**
 * @brief Font asset importer
 */
class FontImporter : public IAssetImporter {
public:
  FontImporter();
  ~FontImporter() override = default;

  [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override;
  [[nodiscard]] AssetType getAssetType() const override;
  [[nodiscard]] bool canImport(const std::string& path) const override;
  [[nodiscard]] Result<AssetMetadata> import(const std::string& sourcePath,
                                             const std::string& destPath,
                                             AssetDatabase* database) override;
  [[nodiscard]] Result<AssetMetadata> reimport(const AssetMetadata& existing,
                                               AssetDatabase* database) override;
  [[nodiscard]] std::string getDefaultSettingsJson() const override;

  void setSettings(const FontImportSettings& settings);
  [[nodiscard]] const FontImportSettings& getSettings() const;

private:
  Result<void> processFont(const std::string& sourcePath, const std::string& destPath);

  FontImportSettings m_settings;
};

/**
 * @brief Asset change event type
 */
enum class AssetChangeType : u8 { Added, Modified, Deleted, Moved, Reimported };

/**
 * @brief Asset change event
 */
struct AssetChangeEvent {
  AssetChangeType type;
  std::string assetId;
  std::string path;
  std::string oldPath; // For moves
};

/**
 * @brief Callback for asset changes
 */
using OnAssetChanged = std::function<void(const AssetChangeEvent&)>;

/**
 * @brief Asset Database - tracks and manages all project assets
 *
 * Features:
 * - Asset registration and lookup
 * - Dependency tracking
 * - Change detection
 * - Hot reload support
 * - Search and filtering
 */
class AssetDatabase {
public:
  AssetDatabase();
  ~AssetDatabase();

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize database for a project
   */
  Result<void> initialize(const std::string& projectPath);

  /**
   * @brief Save database to disk
   */
  Result<void> save();

  /**
   * @brief Load database from disk
   */
  Result<void> load();

  /**
   * @brief Close database
   */
  void close();

  // =========================================================================
  // Asset Registration
  // =========================================================================

  /**
   * @brief Register an asset
   */
  void registerAsset(const AssetMetadata& metadata);

  /**
   * @brief Unregister an asset
   */
  void unregisterAsset(const std::string& assetId);

  /**
   * @brief Update asset metadata
   */
  void updateAsset(const AssetMetadata& metadata);

  // =========================================================================
  // Asset Lookup
  // =========================================================================

  /**
   * @brief Get asset by ID
   */
  [[nodiscard]] std::optional<AssetMetadata> getAsset(const std::string& assetId) const;

  /**
   * @brief Get asset by path
   */
  [[nodiscard]] std::optional<AssetMetadata> getAssetByPath(const std::string& path) const;

  /**
   * @brief Get all assets of a type
   */
  [[nodiscard]] std::vector<AssetMetadata> getAssetsByType(AssetType type) const;

  /**
   * @brief Get all assets with a tag
   */
  [[nodiscard]] std::vector<AssetMetadata> getAssetsByTag(const std::string& tag) const;

  /**
   * @brief Search assets by name
   */
  [[nodiscard]] std::vector<AssetMetadata> searchAssets(const std::string& query) const;

  /**
   * @brief Get all assets
   */
  [[nodiscard]] const std::unordered_map<std::string, AssetMetadata>& getAllAssets() const;

  // =========================================================================
  // Import Operations
  // =========================================================================

  /**
   * @brief Import a file into the project
   */
  Result<AssetMetadata> importAsset(const std::string& sourcePath);

  /**
   * @brief Import a file into a specific destination path
   */
  Result<AssetMetadata> importAssetToPath(const std::string& sourcePath,
                                          const std::string& destPath);

  /**
   * @brief Reimport an asset
   */
  Result<AssetMetadata> reimportAsset(const std::string& assetId);

  /**
   * @brief Reimport all assets of a type
   */
  Result<void> reimportAllOfType(AssetType type);

  /**
   * @brief Check for and process asset changes
   */
  void checkForChanges();

  /**
   * @brief Get assets that need reimporting
   */
  [[nodiscard]] std::vector<std::string> getOutdatedAssets() const;

  // =========================================================================
  // Dependency Tracking
  // =========================================================================

  /**
   * @brief Add a dependency between assets
   */
  void addDependency(const std::string& assetId, const std::string& dependsOnId);

  /**
   * @brief Remove a dependency
   */
  void removeDependency(const std::string& assetId, const std::string& dependsOnId);

  /**
   * @brief Get assets that depend on a given asset
   */
  [[nodiscard]] std::vector<std::string> getDependents(const std::string& assetId) const;

  /**
   * @brief Get assets that a given asset depends on
   */
  [[nodiscard]] std::vector<std::string> getDependencies(const std::string& assetId) const;

  // =========================================================================
  // Events
  // =========================================================================

  /**
   * @brief Subscribe to asset changes
   */
  void setOnAssetChanged(OnAssetChanged callback);

  // =========================================================================
  // Importers
  // =========================================================================

  /**
   * @brief Register an importer
   */
  void registerImporter(std::unique_ptr<IAssetImporter> importer);

  /**
   * @brief Get importer for file type
   */
  [[nodiscard]] IAssetImporter* getImporterForFile(const std::string& path) const;

  // =========================================================================
  // Project Paths
  // =========================================================================

  [[nodiscard]] const std::string& getProjectPath() const { return m_projectPath; }
  [[nodiscard]] std::string getAssetsPath() const;
  [[nodiscard]] std::string getThumbnailsPath() const;
  [[nodiscard]] std::string getDatabasePath() const;

private:
  void fireAssetChanged(const AssetChangeEvent& event);
  std::string generateAssetId(const std::string& path);
  std::string computeChecksum(const std::string& path);
  AssetType detectAssetType(const std::string& path) const;
  void scanDirectory(const std::string& path);

  std::string m_projectPath;
  std::unordered_map<std::string, AssetMetadata> m_assets;
  std::unordered_map<std::string, std::string> m_pathToId; // path -> assetId lookup
  std::vector<std::unique_ptr<IAssetImporter>> m_importers;
  OnAssetChanged m_onAssetChanged;
  bool m_initialized = false;
};

/**
 * @brief Helper to generate unique asset IDs
 */
[[nodiscard]] std::string generateUniqueAssetId();

/**
 * @brief Get file extension in lowercase
 */
[[nodiscard]] std::string getFileExtension(const std::string& path);

/**
 * @brief Convert asset type to string
 */
[[nodiscard]] const char* assetTypeToString(AssetType type);

/**
 * @brief Convert string to asset type
 */
[[nodiscard]] AssetType stringToAssetType(const std::string& str);

} // namespace NovelMind::editor
