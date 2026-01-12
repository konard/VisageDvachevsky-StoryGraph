#include "NovelMind/editor/qt/panels/asset_browser_thumbnails.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_categorizer.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"

#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QPen>

namespace NovelMind::editor::qt {

NMAssetIconProvider::NMAssetIconProvider(NMAssetBrowserPanel *panel,
                                         QSize iconSize)
    : QFileIconProvider(), m_panel(panel), m_iconSize(iconSize) {}

void NMAssetIconProvider::setIconSize(const QSize &size) { m_iconSize = size; }

QIcon NMAssetIconProvider::icon(const QFileInfo &info) const {
  if (!info.isFile()) {
    return QFileIconProvider::icon(info);
  }

  const QString path = info.absoluteFilePath();
  const QString ext = info.suffix();

  // Check for images
  if (isImageExtension(ext)) {
    // Try to get from panel's cache
    if (m_panel && m_panel->m_thumbnailCache.contains(path)) {
      auto *entry = m_panel->m_thumbnailCache.object(path);
      if (entry && isThumbnailValid(path, *entry)) {
        return QIcon(entry->pixmap);
      }
    }

    // PERF-3: Request async loading via lazy loader instead of blocking
    // Return a placeholder icon while the real thumbnail loads
    if (m_panel) {
      // Request thumbnail with normal priority (visible items get high
      // priority)
      m_panel->requestAsyncThumbnail(path, m_iconSize, 5);
    }

    // Return default file icon as placeholder while async loading
    return QFileIconProvider::icon(info);
  }

  // Check for audio files
  if (isAudioExtension(ext)) {
    if (m_panel) {
      QPixmap waveform = generateAudioWaveform(path, m_iconSize);
      if (!waveform.isNull()) {
        return QIcon(waveform);
      }
    }
  }

  return QFileIconProvider::icon(info);
}

QPixmap generateAudioWaveform(const QString &path, const QSize &size) {
  // Create a deterministic placeholder waveform
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Background
  QColor bgColor(60, 60, 80);
  painter.fillRect(pixmap.rect(), bgColor);

  // Border
  painter.setPen(QPen(QColor(100, 100, 120), 1));
  painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));

  // Draw "AUDIO" text
  painter.setPen(QColor(200, 200, 220));
  QFont font = painter.font();
  font.setPointSize(size.height() / 6);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(pixmap.rect(), Qt::AlignCenter, "AUDIO");

  // Draw deterministic waveform bars
  const int barCount = 16;
  const int barWidth = (size.width() - 20) / barCount;
  const int maxBarHeight = size.height() - 30;

  // Use file path hash for deterministic heights
  QByteArray pathData = path.toUtf8();
  uint seed = static_cast<uint>(qHash(pathData));

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(120, 200, 255, 180));

  for (int i = 0; i < barCount; ++i) {
    // Generate deterministic height based on position and seed
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    const double normalized = static_cast<double>(seed % 1000) / 1000.0;
    const int barHeight =
        static_cast<int>(maxBarHeight * normalized * 0.8 + maxBarHeight * 0.2);

    const int x = 10 + i * barWidth;
    const int y = (size.height() - barHeight) / 2;

    painter.drawRect(x, y, barWidth - 2, barHeight);
  }

  return pixmap;
}

bool isThumbnailValid(const QString &path,
                      const ThumbnailCacheEntry &entry) {
  QFileInfo info(path);
  if (!info.exists()) {
    return false;
  }

  // Check if file has been modified
  if (info.lastModified() != entry.lastModified) {
    return false;
  }

  // Check if file size has changed
  if (info.size() != entry.fileSize) {
    return false;
  }

  return true;
}

} // namespace NovelMind::editor::qt
