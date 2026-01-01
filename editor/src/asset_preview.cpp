#include "NovelMind/editor/asset_preview.hpp"
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace NovelMind::editor {

AssetPreviewManager::AssetPreviewManager() = default;

AssetPreviewManager::~AssetPreviewManager() = default;

AssetPreviewManager &AssetPreviewManager::instance() {
  // Thread-safe in C++11+ due to magic statics
  static AssetPreviewManager instance;
  return instance;
}

// ============================================================================
// Preview Generation
// ============================================================================

AssetPreview AssetPreviewManager::getPreview(const std::string &assetPath,
                                             u32 thumbnailSize) {
  namespace fs = std::filesystem;

  // Check cache first (use shared lock for read access)
  {
    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    auto cacheIt = m_cache.find(assetPath);
    if (cacheIt != m_cache.end()) {
      // Check if file was modified
      std::error_code ec;
      auto modTime =
          fs::last_write_time(assetPath, ec).time_since_epoch().count();
      if (!ec && cacheIt->second.preview.assetModifiedTime ==
                     static_cast<u64>(modTime)) {
        // Need to upgrade to unique lock for writing stats
        lock.unlock();
        std::unique_lock<std::shared_mutex> writeLock(m_cacheMutex);
        auto writeIt = m_cache.find(assetPath);
        if (writeIt != m_cache.end()) {
          writeIt->second.lastAccessed = static_cast<u64>(
              std::chrono::steady_clock::now().time_since_epoch().count());
          writeIt->second.accessCount++;
          m_cacheHits++;
          return writeIt->second.preview;
        }
      }
    }
  }

  {
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_cacheMisses++;
  }
  return generatePreview(assetPath, thumbnailSize);
}

void AssetPreviewManager::requestPreview(const PreviewRequest &request) {
  std::lock_guard<std::mutex> lock(m_requestMutex);
  m_pendingRequests.push_back(request);
}

Result<ThumbnailData>
AssetPreviewManager::generateImageThumbnail(const std::string &imagePath,
                                            u32 width, u32 height) {
  namespace fs = std::filesystem;

  if (!fs::exists(imagePath)) {
    return Result<ThumbnailData>::error("Image file not found: " + imagePath);
  }

  ThumbnailData thumbnail;
  thumbnail.width = width;
  thumbnail.height = height;

  const int widthInt = static_cast<int>(
      std::min<u32>(width, static_cast<u32>(std::numeric_limits<int>::max())));
  const int heightInt = static_cast<int>(
      std::min<u32>(height, static_cast<u32>(std::numeric_limits<int>::max())));

  QImage image(QString::fromStdString(imagePath));
  if (!image.isNull()) {
    QImage scaled = image.scaled(widthInt, heightInt, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    QImage canvas(widthInt, heightInt, QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    QPointF offset((widthInt - scaled.width()) / 2.0,
                   (heightInt - scaled.height()) / 2.0);
    painter.drawImage(offset, scaled);
    painter.end();

    thumbnail.pixels.resize(width * height * 4);
    const uchar *data = canvas.constBits();
    std::copy(data, data + thumbnail.pixels.size(), thumbnail.pixels.begin());
    thumbnail.valid = true;
  } else {
    thumbnail.pixels.resize(width * height * 4, 0);
    thumbnail.valid = false;
  }

  thumbnail.generatedAt = static_cast<u64>(
      std::chrono::steady_clock::now().time_since_epoch().count());

  return Result<ThumbnailData>::ok(std::move(thumbnail));
}

Result<WaveformData>
AssetPreviewManager::generateAudioWaveform(const std::string &audioPath,
                                           u32 sampleCount) {
  namespace fs = std::filesystem;

  if (!fs::exists(audioPath)) {
    return Result<WaveformData>::error("Audio file not found: " + audioPath);
  }

  WaveformData waveform;
  waveform.samples.resize(sampleCount);

  std::ifstream file(audioPath, std::ios::binary);
  if (!file.is_open()) {
    return Result<WaveformData>::error("Failed to open audio file: " +
                                       audioPath);
  }

  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    return Result<WaveformData>::error("Audio file is empty: " + audioPath);
  }

  const size_t step = std::max<size_t>(1, bytes.size() / sampleCount);
  for (u32 i = 0; i < sampleCount; ++i) {
    const size_t index = std::min<size_t>(bytes.size() - 1, i * step);
    const unsigned char value = bytes[index];
    waveform.samples[i] = static_cast<f32>(value) / 255.0f;
  }

  waveform.duration = 0.0f;
  waveform.sampleRate = 0;
  waveform.channels = 0;
  waveform.valid = true;

  return Result<WaveformData>::ok(std::move(waveform));
}

Result<FontPreviewData>
AssetPreviewManager::generateFontPreview(const std::string &fontPath,
                                         u32 thumbnailSize) {
  namespace fs = std::filesystem;

  if (!fs::exists(fontPath)) {
    return Result<FontPreviewData>::error("Font file not found: " + fontPath);
  }

  FontPreviewData preview;

  const QString qPath = QString::fromStdString(fontPath);
  int fontId = QFontDatabase::addApplicationFont(qPath);
  QString family;
  if (fontId >= 0) {
    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty()) {
      family = families.first();
    }
  }

  if (family.isEmpty()) {
    preview.familyName = fs::path(fontPath).stem().string();
    preview.styleName = "Regular";
  } else {
    preview.familyName = family.toStdString();
    preview.styleName = "Regular";
  }
  preview.isMonospace = QFontDatabase::isFixedPitch(family);

  const int sizeInt = static_cast<int>(std::min<u32>(
      thumbnailSize, static_cast<u32>(std::numeric_limits<int>::max())));
  QImage image(sizeInt, sizeInt, QImage::Format_RGBA8888);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::TextAntialiasing);
  QFont font(family.isEmpty() ? "Sans Serif" : family);
  font.setPointSizeF(sizeInt / 5.0);
  painter.setFont(font);
  painter.setPen(QColor(220, 220, 220));
  const QString sampleText = "AaBb 123";
  painter.drawText(image.rect(), Qt::AlignCenter, sampleText);
  painter.end();

  preview.thumbnail.width = thumbnailSize;
  preview.thumbnail.height = thumbnailSize;
  preview.thumbnail.pixels.resize(thumbnailSize * thumbnailSize * 4);
  const uchar *data = image.constBits();
  std::copy(data, data + preview.thumbnail.pixels.size(),
            preview.thumbnail.pixels.begin());
  preview.thumbnail.valid = true;

  return Result<FontPreviewData>::ok(std::move(preview));
}

AssetPreviewType AssetPreviewManager::getAssetType(const std::string &path) {
  namespace fs = std::filesystem;

  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Image formats
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".gif" || ext == ".tga" || ext == ".webp") {
    return AssetPreviewType::Image;
  }

  // Audio formats
  if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" ||
      ext == ".aac" || ext == ".m4a") {
    return AssetPreviewType::Audio;
  }

  // Font formats
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return AssetPreviewType::Font;
  }

  // Video formats
  if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".webm" ||
      ext == ".mov") {
    return AssetPreviewType::Video;
  }

  // Script formats
  if (ext == ".nms" || ext == ".json" || ext == ".xml") {
    return AssetPreviewType::Script;
  }

  // Scene formats
  if (ext == ".scene" || ext == ".graph") {
    return AssetPreviewType::Scene;
  }

  return AssetPreviewType::Unknown;
}

// ============================================================================
// Cache Management
// ============================================================================

bool AssetPreviewManager::hasCachedPreview(const std::string &assetPath) const {
  std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
  return m_cache.find(assetPath) != m_cache.end();
}

void AssetPreviewManager::invalidateCache(const std::string &assetPath) {
  std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
  auto it = m_cache.find(assetPath);
  if (it != m_cache.end()) {
    m_currentCacheSize -= estimatePreviewSize(it->second.preview);
    m_cache.erase(it);
  }
}

void AssetPreviewManager::clearCache() {
  std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
  m_cache.clear();
  m_currentCacheSize = 0;
  m_cacheHits = 0;
  m_cacheMisses = 0;
}

void AssetPreviewManager::setMaxCacheSize(size_t bytes) {
  std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
  m_maxCacheSize = bytes;
  while (m_currentCacheSize > m_maxCacheSize && !m_cache.empty()) {
    evictLRULocked(); // Lock is already held
  }
}

size_t AssetPreviewManager::getCacheSize() const {
  std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
  return m_currentCacheSize;
}

AssetPreviewManager::CacheStats AssetPreviewManager::getCacheStats() const {
  std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
  return {m_cache.size(), m_currentCacheSize, m_cacheHits, m_cacheMisses};
}

// ============================================================================
// Processing
// ============================================================================

void AssetPreviewManager::processPendingRequests() {
  std::vector<PreviewRequest> requests;

  {
    std::lock_guard<std::mutex> lock(m_requestMutex);
    std::swap(requests, m_pendingRequests);
  }

  for (auto &request : requests) {
    try {
      AssetPreview preview =
          getPreview(request.assetPath, request.thumbnailWidth);

      if (request.onComplete) {
        request.onComplete(preview);
      }
    } catch (const std::exception &e) {
      if (request.onError) {
        request.onError(e.what());
      }
    }
  }
}

size_t AssetPreviewManager::getPendingRequestCount() const {
  std::lock_guard<std::mutex> lock(m_requestMutex);
  return m_pendingRequests.size();
}

// ============================================================================
// Configuration
// ============================================================================

void AssetPreviewManager::setDefaultThumbnailSize(u32 size) {
  m_defaultThumbnailSize = size;
}

void AssetPreviewManager::setFontSampleText(const std::string &text) {
  m_fontSampleText = text;
}

const std::string &AssetPreviewManager::getFontSampleText() const {
  return m_fontSampleText;
}

// ============================================================================
// Private Methods
// ============================================================================

AssetPreview AssetPreviewManager::generatePreview(const std::string &assetPath,
                                                  u32 thumbnailSize) {
  namespace fs = std::filesystem;

  AssetPreview preview;
  preview.assetPath = assetPath;
  preview.type = getAssetType(assetPath);

  // Get file info
  std::error_code ec;
  if (fs::exists(assetPath, ec)) {
    preview.fileSize = fs::file_size(assetPath, ec);
    preview.assetModifiedTime = static_cast<u64>(
        fs::last_write_time(assetPath, ec).time_since_epoch().count());
  }

  preview.format = fs::path(assetPath).extension().string();

  // Generate type-specific preview
  switch (preview.type) {
  case AssetPreviewType::Image: {
    auto result =
        generateImageThumbnail(assetPath, thumbnailSize, thumbnailSize);
    if (result.isOk()) {
      preview.thumbnail = result.value();
    }
    break;
  }

  case AssetPreviewType::Audio: {
    auto result = generateAudioWaveform(assetPath, 200);
    if (result.isOk()) {
      preview.waveform = result.value();
    }
    // Also generate a generic audio icon thumbnail
    preview.thumbnail.width = thumbnailSize;
    preview.thumbnail.height = thumbnailSize;
    preview.thumbnail.valid = true;
    break;
  }

  case AssetPreviewType::Font: {
    auto result = generateFontPreview(assetPath, thumbnailSize);
    if (result.isOk()) {
      preview.fontPreview = result.value();
      preview.thumbnail = preview.fontPreview.thumbnail;
    }
    break;
  }

  default:
    // Generate generic icon thumbnail based on type
    preview.thumbnail.width = thumbnailSize;
    preview.thumbnail.height = thumbnailSize;
    preview.thumbnail.valid = true;
    break;
  }

  // Cache the preview
  addToCache(assetPath, preview);

  return preview;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

void AssetPreviewManager::addToCache(const std::string &path,
                                     const AssetPreview &preview) {
  std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
  size_t previewSize = estimatePreviewSize(preview);

  // Evict if necessary (evictLRU assumes lock is already held)
  while (m_currentCacheSize + previewSize > m_maxCacheSize &&
         !m_cache.empty()) {
    evictLRULocked();
  }

  PreviewCacheEntry entry;
  entry.preview = preview;
  entry.lastAccessed = static_cast<u64>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  entry.accessCount = 1;

  m_cache[path] = std::move(entry);
  m_currentCacheSize += previewSize;
}

// Internal helper - assumes m_cacheMutex is already locked
void AssetPreviewManager::evictLRULocked() {
  if (m_cache.empty()) {
    return;
  }

  // Find least recently used entry
  auto lruIt = m_cache.begin();
  for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
    if (it->second.lastAccessed < lruIt->second.lastAccessed) {
      lruIt = it;
    }
  }

  m_currentCacheSize -= estimatePreviewSize(lruIt->second.preview);
  m_cache.erase(lruIt);
}

void AssetPreviewManager::evictLRU() {
  // For external callers - acquires lock
  std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
  evictLRULocked();
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

size_t
AssetPreviewManager::estimatePreviewSize(const AssetPreview &preview) const {
  size_t size = sizeof(AssetPreview);

  // Thumbnail size
  size += preview.thumbnail.pixels.size();

  // Waveform size
  size += preview.waveform.samples.size() * sizeof(f32);

  // Font preview
  size += preview.fontPreview.thumbnail.pixels.size();

  // Strings and metadata
  size += preview.assetPath.size();
  size += preview.format.size();

  for (const auto &pair : preview.metadata) {
    size += pair.first.size() + pair.second.size();
  }

  return size;
}

} // namespace NovelMind::editor
