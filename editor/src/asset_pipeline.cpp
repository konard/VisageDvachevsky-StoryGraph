/**
 * @file asset_pipeline.cpp
 * @brief Asset Pipeline implementation
 */

#include "NovelMind/editor/asset_pipeline.hpp"
#include <QImage>
#include <QString>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace NovelMind::editor {

namespace fs = std::filesystem;

// ============================================================================
// Utility Functions
// ============================================================================

std::string generateUniqueAssetId() {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());
  static std::uniform_int_distribution<u64> dist;

  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(16) << dist(gen);
  return ss.str();
}

std::string getFileExtension(const std::string &path) {
  fs::path p(path);
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

const char *assetTypeToString(AssetType type) {
  switch (type) {
  case AssetType::Image:
    return "image";
  case AssetType::Audio:
    return "audio";
  case AssetType::Font:
    return "font";
  case AssetType::Script:
    return "script";
  case AssetType::Scene:
    return "scene";
  case AssetType::Localization:
    return "localization";
  case AssetType::Data:
    return "data";
  default:
    return "unknown";
  }
}

AssetType stringToAssetType(const std::string &str) {
  if (str == "image")
    return AssetType::Image;
  if (str == "audio")
    return AssetType::Audio;
  if (str == "font")
    return AssetType::Font;
  if (str == "script")
    return AssetType::Script;
  if (str == "scene")
    return AssetType::Scene;
  if (str == "localization")
    return AssetType::Localization;
  if (str == "data")
    return AssetType::Data;
  return AssetType::Unknown;
}

// ============================================================================
// ImageImporter
// ============================================================================

ImageImporter::ImageImporter() {}

std::vector<std::string> ImageImporter::getSupportedExtensions() const {
  return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tga"};
}

AssetType ImageImporter::getAssetType() const { return AssetType::Image; }

bool ImageImporter::canImport(const std::string &path) const {
  std::string ext = getFileExtension(path);
  auto extensions = getSupportedExtensions();
  return std::find(extensions.begin(), extensions.end(), ext) !=
         extensions.end();
}

Result<AssetMetadata> ImageImporter::import(const std::string &sourcePath,
                                            const std::string &destPath,
                                            AssetDatabase *database) {
  if (!fs::exists(sourcePath)) {
    std::string errorMsg = "Failed to import image: Source file not found\n\n";
    errorMsg += "What went wrong: The image file '" + sourcePath + "' does not exist.\n\n";
    errorMsg += "How to fix:\n";
    errorMsg += "  - Check if the file path is correct\n";
    errorMsg += "  - Verify the file hasn't been moved or deleted\n";
    errorMsg += "  - Ensure you have read permissions for the file location";
    return Result<AssetMetadata>::error(errorMsg);
  }

  // Create destination directory if needed
  fs::path dest(destPath);
  fs::create_directories(dest.parent_path());

  // Process image (copy with optional conversion)
  auto processResult = processImage(sourcePath, destPath);
  if (!processResult.isOk()) {
    return Result<AssetMetadata>::error(processResult.error());
  }

  // Create metadata
  AssetMetadata metadata;
  metadata.id = generateUniqueAssetId();
  metadata.name = fs::path(sourcePath).stem().string();
  metadata.sourcePath = sourcePath;
  metadata.importedPath = destPath;
  metadata.type = AssetType::Image;
  metadata.sourceModifiedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           fs::last_write_time(sourcePath).time_since_epoch())
                           .count());
  metadata.importedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
  metadata.fileSize = fs::file_size(sourcePath);

  // Generate thumbnail
  if (database) {
    std::string thumbnailPath =
        database->getThumbnailsPath() + "/" + metadata.id + ".png";
    generateThumbnail(sourcePath, thumbnailPath);
    metadata.thumbnailPath = thumbnailPath;
  }

  return Result<AssetMetadata>::ok(std::move(metadata));
}

Result<AssetMetadata> ImageImporter::reimport(const AssetMetadata &existing,
                                              AssetDatabase *database) {
  return import(existing.sourcePath, existing.importedPath, database);
}

std::string ImageImporter::getDefaultSettingsJson() const {
  return R"({
        "compression": "png",
        "generateMipmaps": false,
        "premultiplyAlpha": true,
        "maxWidth": 4096,
        "maxHeight": 4096,
        "powerOfTwo": false,
        "compressionQuality": 0.8
    })";
}

void ImageImporter::setSettings(const ImageImportSettings &settings) {
  m_settings = settings;
}

const ImageImportSettings &ImageImporter::getSettings() const {
  return m_settings;
}

Result<void> ImageImporter::processImage(const std::string &sourcePath,
                                         const std::string &destPath) {
  try {
    if (sourcePath == destPath) {
      return Result<void>::ok();
    }
    // For now, just copy the file
    // In production, would use stb_image/stb_image_write for processing
    fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    std::string errorMsg = "Failed to process image file\n\n";
    errorMsg += "What went wrong: Could not copy or convert the image file.\n";
    errorMsg += "Source: " + sourcePath + "\n";
    errorMsg += "Destination: " + destPath + "\n";
    errorMsg += "System error: " + std::string(e.what()) + "\n\n";
    errorMsg += "How to fix:\n";
    errorMsg += "  - Ensure you have write permissions for the destination folder\n";
    errorMsg += "  - Check if there is enough disk space\n";
    errorMsg += "  - Verify the destination path is valid\n";
    errorMsg += "  - Try closing any programs that might be using the file";
    return Result<void>::error(errorMsg);
  }
}

Result<void>
ImageImporter::generateThumbnail(const std::string &sourcePath,
                                 const std::string &thumbnailPath) {
  // Generate a scaled thumbnail if possible, otherwise fall back to copy.
  try {
    fs::create_directories(fs::path(thumbnailPath).parent_path());
    QImage image(QString::fromStdString(sourcePath));
    if (!image.isNull()) {
      QImage scaled =
          image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      if (scaled.save(QString::fromStdString(thumbnailPath))) {
        return Result<void>::ok();
      }
    }
    fs::copy(sourcePath, thumbnailPath, fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    std::string errorMsg = "Failed to generate thumbnail for image\n\n";
    errorMsg += "What went wrong: Could not create a thumbnail preview of the image.\n";
    errorMsg += "Source: " + sourcePath + "\n";
    errorMsg += "Thumbnail path: " + thumbnailPath + "\n";
    errorMsg += "System error: " + std::string(e.what()) + "\n\n";
    errorMsg += "How to fix:\n";
    errorMsg += "  - The asset will still be imported, but won't have a thumbnail preview\n";
    errorMsg += "  - Check if the image format is supported\n";
    errorMsg += "  - Ensure write permissions for the thumbnails directory";
    return Result<void>::error(errorMsg);
  }
}

// ============================================================================
// AudioImporter
// ============================================================================

AudioImporter::AudioImporter() {}

std::vector<std::string> AudioImporter::getSupportedExtensions() const {
  return {".wav", ".mp3", ".ogg", ".flac", ".aiff", ".opus"};
}

AssetType AudioImporter::getAssetType() const { return AssetType::Audio; }

bool AudioImporter::canImport(const std::string &path) const {
  std::string ext = getFileExtension(path);
  auto extensions = getSupportedExtensions();
  return std::find(extensions.begin(), extensions.end(), ext) !=
         extensions.end();
}

Result<AssetMetadata> AudioImporter::import(const std::string &sourcePath,
                                            const std::string &destPath,
                                            AssetDatabase * /*database*/) {
  if (!fs::exists(sourcePath)) {
    std::string errorMsg = "Failed to import audio: Source file not found\n\n";
    errorMsg += "What went wrong: The audio file '" + sourcePath + "' does not exist.\n\n";
    errorMsg += "How to fix:\n";
    errorMsg += "  - Check if the file path is correct\n";
    errorMsg += "  - Verify the file hasn't been moved or deleted\n";
    errorMsg += "  - Ensure you have read permissions for the file location\n";
    errorMsg += "  - Supported formats: .wav, .mp3, .ogg, .flac, .aiff, .opus";
    return Result<AssetMetadata>::error(errorMsg);
  }

  // Create destination directory if needed
  fs::path dest(destPath);
  fs::create_directories(dest.parent_path());

  // Process audio (copy with optional conversion)
  auto processResult = processAudio(sourcePath, destPath);
  if (!processResult.isOk()) {
    return Result<AssetMetadata>::error(processResult.error());
  }

  // Create metadata
  AssetMetadata metadata;
  metadata.id = generateUniqueAssetId();
  metadata.name = fs::path(sourcePath).stem().string();
  metadata.sourcePath = sourcePath;
  metadata.importedPath = destPath;
  metadata.type = AssetType::Audio;
  metadata.sourceModifiedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           fs::last_write_time(sourcePath).time_since_epoch())
                           .count());
  metadata.importedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
  metadata.fileSize = fs::file_size(sourcePath);

  return Result<AssetMetadata>::ok(std::move(metadata));
}

Result<AssetMetadata> AudioImporter::reimport(const AssetMetadata &existing,
                                              AssetDatabase *database) {
  return import(existing.sourcePath, existing.importedPath, database);
}

std::string AudioImporter::getDefaultSettingsJson() const {
  return R"({
        "format": "ogg",
        "streaming": false,
        "quality": 0.7,
        "mono": false,
        "sampleRate": 44100,
        "normalize": false
    })";
}

void AudioImporter::setSettings(const AudioImportSettings &settings) {
  m_settings = settings;
}

const AudioImportSettings &AudioImporter::getSettings() const {
  return m_settings;
}

Result<void> AudioImporter::processAudio(const std::string &sourcePath,
                                         const std::string &destPath) {
  try {
    if (sourcePath == destPath) {
      return Result<void>::ok();
    }
    // For now, just copy the file
    // In production, would use a library like dr_libs or miniaudio for
    // processing
    fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to process audio: ") +
                               e.what());
  }
}

// ============================================================================
// FontImporter
// ============================================================================

FontImporter::FontImporter() {}

std::vector<std::string> FontImporter::getSupportedExtensions() const {
  return {".ttf", ".otf", ".woff", ".woff2"};
}

AssetType FontImporter::getAssetType() const { return AssetType::Font; }

bool FontImporter::canImport(const std::string &path) const {
  std::string ext = getFileExtension(path);
  auto extensions = getSupportedExtensions();
  return std::find(extensions.begin(), extensions.end(), ext) !=
         extensions.end();
}

Result<AssetMetadata> FontImporter::import(const std::string &sourcePath,
                                           const std::string &destPath,
                                           AssetDatabase * /*database*/) {
  if (!fs::exists(sourcePath)) {
    return Result<AssetMetadata>::error("Source file does not exist: " +
                                        sourcePath);
  }

  // Create destination directory if needed
  fs::path dest(destPath);
  fs::create_directories(dest.parent_path());

  // Process font (copy with optional atlas generation)
  auto processResult = processFont(sourcePath, destPath);
  if (!processResult.isOk()) {
    return Result<AssetMetadata>::error(processResult.error());
  }

  // Create metadata
  AssetMetadata metadata;
  metadata.id = generateUniqueAssetId();
  metadata.name = fs::path(sourcePath).stem().string();
  metadata.sourcePath = sourcePath;
  metadata.importedPath = destPath;
  metadata.type = AssetType::Font;
  metadata.sourceModifiedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           fs::last_write_time(sourcePath).time_since_epoch())
                           .count());
  metadata.importedTime =
      static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
  metadata.fileSize = fs::file_size(sourcePath);

  return Result<AssetMetadata>::ok(std::move(metadata));
}

Result<AssetMetadata> FontImporter::reimport(const AssetMetadata &existing,
                                             AssetDatabase *database) {
  return import(existing.sourcePath, existing.importedPath, database);
}

std::string FontImporter::getDefaultSettingsJson() const {
  return R"({
        "sizes": [12, 14, 16, 18, 24, 32, 48],
        "charset": "ascii",
        "antialiased": true,
        "padding": 2,
        "generateSDF": false
    })";
}

void FontImporter::setSettings(const FontImportSettings &settings) {
  m_settings = settings;
}

const FontImportSettings &FontImporter::getSettings() const {
  return m_settings;
}

Result<void> FontImporter::processFont(const std::string &sourcePath,
                                       const std::string &destPath) {
  try {
    if (sourcePath == destPath) {
      return Result<void>::ok();
    }
    // For now, just copy the file
    // In production, would use FreeType to generate font atlases
    fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to process font: ") +
                               e.what());
  }
}

// ============================================================================
// AssetDatabase
// ============================================================================

AssetDatabase::AssetDatabase() {
  // Register default importers
  registerImporter(std::make_unique<ImageImporter>());
  registerImporter(std::make_unique<AudioImporter>());
  registerImporter(std::make_unique<FontImporter>());
}

AssetDatabase::~AssetDatabase() {
  if (m_initialized) {
    close();
  }
}

Result<void> AssetDatabase::initialize(const std::string &projectPath) {
  if (m_initialized) {
    close();
  }

  m_projectPath = projectPath;

  // Create necessary directories
  try {
    fs::create_directories(getAssetsPath());
    fs::create_directories(getThumbnailsPath());
  } catch (const fs::filesystem_error &e) {
    return Result<void>::error(std::string("Failed to create directories: ") +
                               e.what());
  }

  // Try to load existing database
  if (fs::exists(getDatabasePath())) {
    auto loadResult = load();
    if (!loadResult.isOk()) {
      // Database corrupted, start fresh
      m_assets.clear();
      m_pathToId.clear();
    }
  }

  m_initialized = true;
  return Result<void>::ok();
}

Result<void> AssetDatabase::save() {
  if (!m_initialized) {
    return Result<void>::error("Database not initialized");
  }

  try {
    // Serialize a simple text database for now.
    std::ofstream file(getDatabasePath());
    if (!file) {
      return Result<void>::error("Failed to open database file for writing");
    }

    file << "# NovelMind Asset Database\n";
    file << "# Version: 1.0\n";
    file << "asset_count=" << m_assets.size() << "\n";

    for (const auto &[id, metadata] : m_assets) {
      file << "\n[asset:" << id << "]\n";
      file << "name=" << metadata.name << "\n";
      file << "source=" << metadata.sourcePath << "\n";
      file << "imported=" << metadata.importedPath << "\n";
      file << "type=" << assetTypeToString(metadata.type) << "\n";
    }

    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to save database: ") +
                               e.what());
  }
}

Result<void> AssetDatabase::load() {
  if (m_projectPath.empty()) {
    return Result<void>::error("Project path not set");
  }

  std::string dbPath = getDatabasePath();
  if (!fs::exists(dbPath)) {
    return Result<void>::error("Database file does not exist");
  }

  // In production, would deserialize from JSON or binary format
  // For now, just return success (data loaded)
  return Result<void>::ok();
}

void AssetDatabase::close() {
  if (m_initialized) {
    save();
    m_assets.clear();
    m_pathToId.clear();
    m_initialized = false;
  }
}

// ============================================================================
// Asset Registration
// ============================================================================

void AssetDatabase::registerAsset(const AssetMetadata &metadata) {
  m_assets[metadata.id] = metadata;
  m_pathToId[metadata.importedPath] = metadata.id;

  fireAssetChanged(
      {AssetChangeType::Added, metadata.id, metadata.importedPath, ""});
}

void AssetDatabase::unregisterAsset(const std::string &assetId) {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    std::string path = it->second.importedPath;
    m_pathToId.erase(path);
    m_assets.erase(it);

    fireAssetChanged({AssetChangeType::Deleted, assetId, path, ""});
  }
}

void AssetDatabase::updateAsset(const AssetMetadata &metadata) {
  m_assets[metadata.id] = metadata;

  fireAssetChanged(
      {AssetChangeType::Modified, metadata.id, metadata.importedPath, ""});
}

// ============================================================================
// Asset Lookup
// ============================================================================

std::optional<AssetMetadata>
AssetDatabase::getAsset(const std::string &assetId) const {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<AssetMetadata>
AssetDatabase::getAssetByPath(const std::string &path) const {
  auto idIt = m_pathToId.find(path);
  if (idIt != m_pathToId.end()) {
    return getAsset(idIt->second);
  }
  return std::nullopt;
}

std::vector<AssetMetadata>
AssetDatabase::getAssetsByType(AssetType type) const {
  std::vector<AssetMetadata> result;
  for (const auto &[id, metadata] : m_assets) {
    if (metadata.type == type) {
      result.push_back(metadata);
    }
  }
  return result;
}

std::vector<AssetMetadata>
AssetDatabase::getAssetsByTag(const std::string &tag) const {
  std::vector<AssetMetadata> result;
  for (const auto &[id, metadata] : m_assets) {
    if (std::find(metadata.tags.begin(), metadata.tags.end(), tag) !=
        metadata.tags.end()) {
      result.push_back(metadata);
    }
  }
  return result;
}

std::vector<AssetMetadata>
AssetDatabase::searchAssets(const std::string &query) const {
  std::vector<AssetMetadata> result;
  std::string lowerQuery = query;
  std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                 ::tolower);

  for (const auto &[id, metadata] : m_assets) {
    std::string lowerName = metadata.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   ::tolower);

    if (lowerName.find(lowerQuery) != std::string::npos) {
      result.push_back(metadata);
    }
  }
  return result;
}

const std::unordered_map<std::string, AssetMetadata> &
AssetDatabase::getAllAssets() const {
  return m_assets;
}

// ============================================================================
// Import Operations
// ============================================================================

Result<AssetMetadata>
AssetDatabase::importAsset(const std::string &sourcePath) {
  std::string fileName = fs::path(sourcePath).filename().string();
  std::string destPath = getAssetsPath() + "/" + fileName;
  return importAssetToPath(sourcePath, destPath);
}

Result<AssetMetadata>
AssetDatabase::importAssetToPath(const std::string &sourcePath,
                                 const std::string &destPath) {
  IAssetImporter *importer = getImporterForFile(sourcePath);
  if (!importer) {
    return Result<AssetMetadata>::error("No importer found for file: " +
                                        sourcePath);
  }

  auto result = importer->import(sourcePath, destPath, this);
  if (!result.isOk()) {
    return result;
  }

  auto existing = getAssetByPath(destPath);
  if (existing) {
    AssetMetadata updated = result.value();
    updated.id = existing->id;
    updateAsset(updated);
    return Result<AssetMetadata>::ok(std::move(updated));
  }

  registerAsset(result.value());
  return result;
}

Result<AssetMetadata> AssetDatabase::reimportAsset(const std::string &assetId) {
  auto metadata = getAsset(assetId);
  if (!metadata) {
    return Result<AssetMetadata>::error("Asset not found: " + assetId);
  }

  IAssetImporter *importer = getImporterForFile(metadata->sourcePath);
  if (!importer) {
    return Result<AssetMetadata>::error("No importer found for asset");
  }

  auto result = importer->reimport(*metadata, this);
  if (result.isOk()) {
    updateAsset(result.value());
    fireAssetChanged({AssetChangeType::Reimported, assetId,
                      result.value().importedPath, ""});
  }

  return result;
}

Result<void> AssetDatabase::reimportAllOfType(AssetType type) {
  auto assets = getAssetsByType(type);
  for (const auto &asset : assets) {
    auto result = reimportAsset(asset.id);
    if (!result.isOk()) {
      // Log error but continue with other assets
    }
  }
  return Result<void>::ok();
}

void AssetDatabase::checkForChanges() {
  for (auto &[id, metadata] : m_assets) {
    if (!fs::exists(metadata.sourcePath)) {
      continue;
    }

    u64 currentModTime = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::seconds>(
            fs::last_write_time(metadata.sourcePath).time_since_epoch())
            .count());

    if (currentModTime != metadata.sourceModifiedTime) {
      // File has changed, reimport
      reimportAsset(id);
    }
  }
}

std::vector<std::string> AssetDatabase::getOutdatedAssets() const {
  std::vector<std::string> outdated;
  for (const auto &[id, metadata] : m_assets) {
    if (!fs::exists(metadata.sourcePath)) {
      continue;
    }

    u64 currentModTime = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::seconds>(
            fs::last_write_time(metadata.sourcePath).time_since_epoch())
            .count());

    if (currentModTime != metadata.sourceModifiedTime) {
      outdated.push_back(id);
    }
  }
  return outdated;
}

// ============================================================================
// Dependency Tracking
// ============================================================================

void AssetDatabase::addDependency(const std::string &assetId,
                                  const std::string &dependsOnId) {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    auto &deps = it->second.dependsOn;
    if (std::find(deps.begin(), deps.end(), dependsOnId) == deps.end()) {
      deps.push_back(dependsOnId);
    }
  }

  auto depIt = m_assets.find(dependsOnId);
  if (depIt != m_assets.end()) {
    auto &refs = depIt->second.referencedBy;
    if (std::find(refs.begin(), refs.end(), assetId) == refs.end()) {
      refs.push_back(assetId);
    }
  }
}

void AssetDatabase::removeDependency(const std::string &assetId,
                                     const std::string &dependsOnId) {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    auto &deps = it->second.dependsOn;
    deps.erase(std::remove(deps.begin(), deps.end(), dependsOnId), deps.end());
  }

  auto depIt = m_assets.find(dependsOnId);
  if (depIt != m_assets.end()) {
    auto &refs = depIt->second.referencedBy;
    refs.erase(std::remove(refs.begin(), refs.end(), assetId), refs.end());
  }
}

std::vector<std::string>
AssetDatabase::getDependents(const std::string &assetId) const {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    return it->second.referencedBy;
  }
  return {};
}

std::vector<std::string>
AssetDatabase::getDependencies(const std::string &assetId) const {
  auto it = m_assets.find(assetId);
  if (it != m_assets.end()) {
    return it->second.dependsOn;
  }
  return {};
}

// ============================================================================
// Events
// ============================================================================

void AssetDatabase::setOnAssetChanged(OnAssetChanged callback) {
  m_onAssetChanged = std::move(callback);
}

// ============================================================================
// Importers
// ============================================================================

void AssetDatabase::registerImporter(std::unique_ptr<IAssetImporter> importer) {
  m_importers.push_back(std::move(importer));
}

IAssetImporter *
AssetDatabase::getImporterForFile(const std::string &path) const {
  for (const auto &importer : m_importers) {
    if (importer->canImport(path)) {
      return importer.get();
    }
  }
  return nullptr;
}

// ============================================================================
// Project Paths
// ============================================================================

std::string AssetDatabase::getAssetsPath() const {
  return m_projectPath + "/Assets";
}

std::string AssetDatabase::getThumbnailsPath() const {
  return m_projectPath + "/Assets/.thumbnails";
}

std::string AssetDatabase::getDatabasePath() const {
  return m_projectPath + "/assets.nmdb";
}

// ============================================================================
// Private Helpers
// ============================================================================

void AssetDatabase::fireAssetChanged(const AssetChangeEvent &event) {
  if (m_onAssetChanged) {
    m_onAssetChanged(event);
  }
}

std::string AssetDatabase::generateAssetId(const std::string & /*path*/) {
  return generateUniqueAssetId();
}

std::string AssetDatabase::computeChecksum(const std::string &path) {
  // In production, would compute actual checksum (MD5, SHA, etc.)
  // For now, return empty
  (void)path;
  return "";
}

AssetType AssetDatabase::detectAssetType(const std::string &path) const {
  IAssetImporter *importer = getImporterForFile(path);
  if (importer) {
    return importer->getAssetType();
  }
  return AssetType::Unknown;
}

void AssetDatabase::scanDirectory(const std::string &path) {
  try {
    for (const auto &entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        std::string filePath = entry.path().string();
        if (!getAssetByPath(filePath)) {
          // New file found, import it
          importAsset(filePath);
        }
      }
    }
  } catch (const fs::filesystem_error &) {
    // Ignore filesystem errors during scan
  }
}

} // namespace NovelMind::editor
