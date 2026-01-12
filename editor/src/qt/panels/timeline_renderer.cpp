/**
 * @file timeline_renderer.cpp
 * @brief Timeline rendering logic for NMTimelinePanel
 *
 * Extracted from nm_timeline_panel.cpp as part of refactoring to improve
 * maintainability. Handles all rendering-related functionality including:
 * - Track visualization
 * - Keyframe rendering
 * - Playhead rendering
 * - Grid and ruler rendering
 * - Frame label caching
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_keyframe_item.hpp"
#include "NovelMind/editor/qt/performance_metrics.hpp"

#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPen>

namespace NovelMind::editor::qt {

/**
 * @brief Get or create cached frame label string (PERF-3 optimization)
 *
 * Avoids repeated QString::number() allocations during renderTracks().
 * The cache is lazily populated and bounded to prevent memory issues.
 *
 * @param frame Frame number to get label for
 * @return Cached QString reference for the frame number
 */
const QString& NMTimelinePanel::getCachedFrameLabel(int frame) const {
  auto it = m_frameLabelCache.find(frame);
  if (it != m_frameLabelCache.end()) {
    return it.value();
  }

  // Cache miss - create and cache the label
  // Limit cache size to prevent unbounded growth
  if (m_frameLabelCache.size() >= m_frameLabelCacheMaxSize) {
    // Simple eviction: clear half the cache when full
    // In practice, timeline frames are usually contiguous, so this is rare
    QList<int> keys = m_frameLabelCache.keys();
    for (int i = 0; i < keys.size() / 2; ++i) {
      m_frameLabelCache.remove(keys[i]);
    }
  }

  // Insert and return reference to cached value
  m_frameLabelCache.insert(frame, QString::number(frame));
  return m_frameLabelCache[frame];
}

void NMTimelinePanel::renderTracks() {
  QElapsedTimer timer;
  timer.start();

  // Clear existing track visualization (except playhead)
  QList<QGraphicsItem*> items = m_timelineScene->items();
  for (QGraphicsItem* item : items) {
    if (item != m_playheadItem) {
      m_timelineScene->removeItem(item);
      delete item;
    }
  }

  // Clear keyframe item map
  m_keyframeItems.clear();

  int y = TIMELINE_MARGIN;
  int trackIndex = 0;

  // PERF-3: Cache commonly used colors/pens as static to avoid repeated
  // allocations
  static const QPen rulerPen(QColor("#606060"));
  static const QColor labelColor("#a0a0a0");
  static const QPen noPen(Qt::NoPen);
  static const QBrush trackBgBrush(QColor("#2d2d2d"));
  static const QColor nameLabelColor("#e0e0e0");

  // Draw frame ruler
  for (int frame = 0; frame <= m_totalFrames; frame += 10) {
    int x = frameToX(frame);
    m_timelineScene->addLine(x, 0, x, 10, rulerPen);

    if (frame % 30 == 0) // Every second
    {
      // PERF-3: Use cached frame label instead of QString::number()
      QGraphicsTextItem* label = m_timelineScene->addText(getCachedFrameLabel(frame));
      label->setPos(x - 10, -20);
      label->setDefaultTextColor(labelColor);
    }
  }

  // Draw tracks
  for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it, ++trackIndex) {
    TimelineTrack* track = it.value();

    // Track background
    m_timelineScene->addRect(TRACK_HEADER_WIDTH, y, frameToX(m_totalFrames) - TRACK_HEADER_WIDTH,
                             TRACK_HEIGHT, noPen, trackBgBrush);

    // Track header
    m_timelineScene->addRect(0, y, TRACK_HEADER_WIDTH, TRACK_HEIGHT, noPen,
                             QBrush(track->color.darker(150)));

    QGraphicsTextItem* nameLabel = m_timelineScene->addText(track->name);
    nameLabel->setPos(8, y + 8);
    nameLabel->setDefaultTextColor(nameLabelColor);

    // Draw keyframes using custom items
    for (const Keyframe& kf : track->keyframes) {
      int kfX = frameToX(kf.frame);

      // Create custom keyframe item
      NMKeyframeItem* kfItem = new NMKeyframeItem(trackIndex, kf.frame, track->color);
      kfItem->setPos(kfX, y + TRACK_HEIGHT / 2);
      kfItem->setSnapToGrid(m_snapToGrid);
      kfItem->setGridSize(m_gridSize);
      kfItem->setEasingType(static_cast<int>(kf.easing));

      // Set coordinate conversion functions
      kfItem->setFrameConverter([this](int x) { return this->xToFrame(x); },
                                [this](int f) { return this->frameToX(f); });

      // Connect signals
      connect(kfItem, &NMKeyframeItem::clicked, this, &NMTimelinePanel::onKeyframeClicked);
      connect(kfItem, &NMKeyframeItem::moved, this, &NMTimelinePanel::onKeyframeMoved);
      connect(kfItem, &NMKeyframeItem::doubleClicked, this,
              &NMTimelinePanel::onKeyframeDoubleClicked);
      connect(kfItem, &NMKeyframeItem::dragStarted, this, &NMTimelinePanel::onKeyframeDragStarted);
      connect(kfItem, &NMKeyframeItem::dragEnded, this, &NMTimelinePanel::onKeyframeDragEnded);

      // Add to scene
      m_timelineScene->addItem(kfItem);

      // Store in map
      KeyframeId id;
      id.trackIndex = trackIndex;
      id.frame = kf.frame;
      m_keyframeItems[id] = kfItem;

      // Restore selection state
      if (m_selectedKeyframes.contains(id)) {
        kfItem->setSelected(true);
      }
    }

    y += TRACK_HEIGHT;
  }

  // Update scene rect
  m_timelineScene->setSceneRect(0, -30, frameToX(m_totalFrames) + 100, y + TIMELINE_MARGIN);

  updatePlayhead();

  // Record performance metrics
  double renderTimeMs = static_cast<double>(timer.elapsed());
  int itemCount = static_cast<int>(m_timelineScene->items().count());
  recordRenderMetrics(renderTimeMs, itemCount);
}

void NMTimelinePanel::updatePlayhead() {
  int x = frameToX(m_currentFrame);
  m_playheadItem->setLine(x, 0, x,
                          static_cast<qreal>(m_tracks.size() * TRACK_HEIGHT + TIMELINE_MARGIN * 2));
}

void NMTimelinePanel::updateFrameDisplay() {
  int totalSeconds = m_currentFrame / m_fps;
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  int frames = m_currentFrame % m_fps;

  m_timeLabel->setText(QString("%1:%2.%3")
                           .arg(minutes, 2, 10, QChar('0'))
                           .arg(seconds, 2, 10, QChar('0'))
                           .arg(frames, 2, 10, QChar('0')));
}

void NMTimelinePanel::invalidateRenderCache() {
  m_dataVersion.fetch_add(1);
  if (m_renderCache) {
    m_renderCache->invalidateAll();
  }
}

void NMTimelinePanel::invalidateTrackCache(int trackIndex) {
  m_dataVersion.fetch_add(1);
  if (m_renderCache) {
    m_renderCache->invalidateTrack(trackIndex);
  }
}

void NMTimelinePanel::recordRenderMetrics(double renderTimeMs, int itemCount) {
  m_lastRenderTimeMs = renderTimeMs;
  m_lastSceneItemCount = itemCount;

  // Record to performance metrics system
  PerformanceMetrics::instance().recordTiming(PerformanceMetrics::METRIC_RENDER_TRACKS,
                                              renderTimeMs);
  PerformanceMetrics::instance().recordCount(PerformanceMetrics::METRIC_SCENE_ITEMS, itemCount);

  // Report cache stats if enabled
  if (m_renderCache) {
    auto stats = m_renderCache->getStats();
    PerformanceMetrics::instance().recordCount(PerformanceMetrics::METRIC_TIMELINE_CACHE_HIT,
                                               static_cast<int>(stats.hitRate() * 100));
  }
}

} // namespace NovelMind::editor::qt
