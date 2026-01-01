#pragma once

/**
 * @file asset_preview.hpp
 * @brief Asset Preview Backend for NovelMind Editor
 *
 * Provides preview generation and caching for assets:
 * - Image thumbnails
 * - Audio waveforms
 * - Font previews
 * - Video thumbnails (if applicable)
 *
 * This is critical for the Asset Browser GUI to display
 * meaningful previews of project assets.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Asset type enumeration
 */
enum class AssetPreviewType : u8 {
  Unknown = 0,
  Image,
  Audio,
  Font,
  Video,
  Script,
  Scene,
  Data
};

/**
 * @brief Thumbnail data structure
 */
struct ThumbnailData {
  std::vector<u8> pixels; // RGBA pixel data
  u32 width = 0;
  u32 height = 0;
  bool valid = false;
  u64 generatedAt = 0;
};

/**
 * @brief Waveform preview data
 */
struct WaveformData {
  std::vector<f32> samples; // Normalized amplitude samples
  f32 duration = 0.0f;      // Duration in seconds
  u32 sampleRate = 0;
  u32 channels = 0;
  bool valid = false;
};

/**
 * @brief Font preview data
 */
struct FontPreviewData {
  ThumbnailData thumbnail;
  std::string familyName;
  std::string styleName;
  bool isMonospace = false;
  std::vector<std::string> supportedCharsets;
};

/**
 * @brief General asset preview data
 */
struct AssetPreview {
  AssetPreviewType type = AssetPreviewType::Unknown;
  std::string assetPath;
  u64 assetModifiedTime = 0;

  // Type-specific data
  ThumbnailData thumbnail;
  WaveformData waveform;
  FontPreviewData fontPreview;

  // Metadata
  u64 fileSize = 0;
  std::string format;
  std::unordered_map<std::string, std::string> metadata;

  [[nodiscard]] bool isValid() const {
    return type != AssetPreviewType::Unknown;
  }
};

/**
 * @brief Preview request structure
 */
struct PreviewRequest {
  std::string assetPath;
  u32 thumbnailWidth = 128;
  u32 thumbnailHeight = 128;
  u32 waveformSamples = 200; // Number of samples for waveform
  bool forceRefresh = false;

  std::function<void(const AssetPreview &)> onComplete;
  std::function<void(const std::string &)> onError;
};

/**
 * @brief Preview cache entry
 */
struct PreviewCacheEntry {
  AssetPreview preview;
  u64 lastAccessed = 0;
  u32 accessCount = 0;
};

/**
 * @brief Asset preview manager
 *
 * Responsibilities:
 * - Generate thumbnails for images
 * - Generate waveform previews for audio
 * - Generate font sample previews
 * - Cache previews for performance
 * - Handle async preview generation
 */
class AssetPreviewManager {
public:
  AssetPreviewManager();
  ~AssetPreviewManager();

  // Prevent copying
  AssetPreviewManager(const AssetPreviewManager &) = delete;
  AssetPreviewManager &operator=(const AssetPreviewManager &) = delete;

  /**
   * @brief Get singleton instance
   */
  static AssetPreviewManager &instance();

  // =========================================================================
  // Preview Generation
  // =========================================================================

  /**
   * @brief Get preview for an asset (sync, may use cache)
   */
  [[nodiscard]] AssetPreview getPreview(const std::string &assetPath,
                                        u32 thumbnailSize = 128);

  /**
   * @brief Request preview generation (async)
   */
  void requestPreview(const PreviewRequest &request);

  /**
   * @brief Generate thumbnail for an image file
   */
  [[nodiscard]] Result<ThumbnailData>
  generateImageThumbnail(const std::string &imagePath, u32 width, u32 height);

  /**
   * @brief Generate waveform preview for audio file
   */
  [[nodiscard]] Result<WaveformData>
  generateAudioWaveform(const std::string &audioPath, u32 sampleCount);

  /**
   * @brief Generate preview for font file
   */
  [[nodiscard]] Result<FontPreviewData>
  generateFontPreview(const std::string &fontPath, u32 thumbnailSize);

  /**
   * @brief Get asset type from file extension
   */
  [[nodiscard]] static AssetPreviewType getAssetType(const std::string &path);

  // =========================================================================
  // Cache Management
  // =========================================================================

  /**
   * @brief Check if preview is cached and valid
   */
  [[nodiscard]] bool hasCachedPreview(const std::string &assetPath) const;

  /**
   * @brief Invalidate cache for an asset
   */
  void invalidateCache(const std::string &assetPath);

  /**
   * @brief Invalidate all cached previews
   */
  void clearCache();

  /**
   * @brief Set maximum cache size in bytes
   */
  void setMaxCacheSize(size_t bytes);

  /**
   * @brief Get current cache size in bytes
   */
  [[nodiscard]] size_t getCacheSize() const;

  /**
   * @brief Get cache statistics
   */
  struct CacheStats {
    size_t entryCount;
    size_t totalBytes;
    size_t hitCount;
    size_t missCount;
  };
  [[nodiscard]] CacheStats getCacheStats() const;

  // =========================================================================
  // Processing
  // =========================================================================

  /**
   * @brief Process pending preview requests (call from main loop)
   */
  void processPendingRequests();

  /**
   * @brief Get number of pending requests
   */
  [[nodiscard]] size_t getPendingRequestCount() const;

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Set default thumbnail size
   */
  void setDefaultThumbnailSize(u32 size);

  /**
   * @brief Set font preview sample text
   */
  void setFontSampleText(const std::string &text);

  /**
   * @brief Get font preview sample text
   */
  [[nodiscard]] const std::string &getFontSampleText() const;

private:
  // Internal methods
  AssetPreview generatePreview(const std::string &assetPath, u32 thumbnailSize);
  void addToCache(const std::string &path, const AssetPreview &preview);
  void evictLRU();
  void evictLRULocked(); // Assumes m_cacheMutex is already locked
  size_t estimatePreviewSize(const AssetPreview &preview) const;

  // Cache (protected by m_cacheMutex for thread-safe access)
  mutable std::shared_mutex m_cacheMutex;
  std::unordered_map<std::string, PreviewCacheEntry> m_cache;
  size_t m_maxCacheSize = 100 * 1024 * 1024; // 100 MB default
  size_t m_currentCacheSize = 0;
  mutable size_t m_cacheHits = 0;
  mutable size_t m_cacheMisses = 0;

  // Pending requests
  std::vector<PreviewRequest> m_pendingRequests;
  mutable std::mutex m_requestMutex;

  // Configuration
  u32 m_defaultThumbnailSize = 128;
  std::string m_fontSampleText =
      "The quick brown fox jumps over the lazy dog. 0123456789";
};

} // namespace NovelMind::editor
