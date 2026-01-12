#pragma once

/**
 * @file performance_metrics.hpp
 * @brief Simple performance profiling for editor components
 *
 * Provides:
 * - Time measurements for renderTracks(), thumbnail loading, etc.
 * - Scene item count tracking
 * - Memory usage estimation
 * - Thread-safe access to metrics
 */

#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor::qt {

/**
 * @brief Statistics for a single metric
 */
struct MetricStats {
  QString name;
  int sampleCount = 0;
  double totalMs = 0.0;
  double minMs = std::numeric_limits<double>::max();
  double maxMs = 0.0;
  double avgMs = 0.0;
  double lastMs = 0.0;

  void addSample(double ms) {
    sampleCount++;
    totalMs += ms;
    if (ms < minMs)
      minMs = ms;
    if (ms > maxMs)
      maxMs = ms;
    avgMs = totalMs / sampleCount;
    lastMs = ms;
  }

  void reset() {
    sampleCount = 0;
    totalMs = 0.0;
    minMs = std::numeric_limits<double>::max();
    maxMs = 0.0;
    avgMs = 0.0;
    lastMs = 0.0;
  }
};

/**
 * @brief RAII timer for measuring code blocks
 */
class ScopedTimer {
public:
  using Clock = std::chrono::steady_clock;

  explicit ScopedTimer(const QString& name, bool enabled = true);
  ~ScopedTimer();

  /**
   * @brief Get elapsed time so far (without stopping)
   */
  double elapsedMs() const;

  /**
   * @brief Stop timing and return elapsed time
   */
  double stop();

  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
  QString m_name;
  Clock::time_point m_start;
  bool m_enabled;
  bool m_stopped = false;
};

/**
 * @brief Performance metrics collector for editor
 *
 * Thread-safe singleton for collecting and reporting
 * performance metrics across editor components.
 */
class PerformanceMetrics : public QObject {
  Q_OBJECT

public:
  static PerformanceMetrics& instance();

  /**
   * @brief Enable/disable metrics collection
   */
  void setEnabled(bool enabled);
  bool isEnabled() const { return m_enabled.load(); }

  /**
   * @brief Record a timing measurement
   */
  void recordTiming(const QString& name, double ms);

  /**
   * @brief Record a count metric (e.g., scene item count)
   */
  void recordCount(const QString& name, int count);

  /**
   * @brief Get statistics for a metric
   */
  MetricStats getStats(const QString& name) const;

  /**
   * @brief Get all metric names
   */
  QStringList metricNames() const;

  /**
   * @brief Reset all metrics
   */
  void reset();

  /**
   * @brief Reset a specific metric
   */
  void reset(const QString& name);

  /**
   * @brief Get formatted summary string
   */
  QString getSummary() const;

  /**
   * @brief Log current metrics to console
   */
  void logMetrics() const;

  // Predefined metric names for consistency
  static constexpr const char* METRIC_RENDER_TRACKS = "Timeline.renderTracks";
  static constexpr const char* METRIC_THUMBNAIL_LOAD = "AssetBrowser.thumbnailLoad";
  static constexpr const char* METRIC_THUMBNAIL_CACHE_HIT = "AssetBrowser.thumbnailCacheHit";
  static constexpr const char* METRIC_SCENE_ITEMS = "Timeline.sceneItemCount";
  static constexpr const char* METRIC_CACHE_SIZE_KB = "AssetBrowser.cacheSizeKB";
  static constexpr const char* METRIC_TIMELINE_CACHE_HIT = "Timeline.cacheHitRate";

signals:
  /**
   * @brief Emitted when a metric exceeds a threshold
   */
  void metricThresholdExceeded(const QString& name, double value, double threshold);

private:
  PerformanceMetrics();
  ~PerformanceMetrics() override = default;

  PerformanceMetrics(const PerformanceMetrics&) = delete;
  PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;

  mutable QMutex m_mutex;
  std::unordered_map<QString, MetricStats> m_timingStats;
  std::unordered_map<QString, int> m_countStats;
  std::atomic<bool> m_enabled{false};
};

/**
 * @brief Macro for easy scoped timing
 */
#ifdef NOVELMIND_PROFILING_ENABLED
#define NM_PROFILE_SCOPE(name) NovelMind::editor::qt::ScopedTimer _nm_timer##__LINE__(name)
#define NM_PROFILE_FUNCTION() NovelMind::editor::qt::ScopedTimer _nm_timer_func(__func__)
#else
#define NM_PROFILE_SCOPE(name) (void)0
#define NM_PROFILE_FUNCTION() (void)0
#endif

} // namespace NovelMind::editor::qt
