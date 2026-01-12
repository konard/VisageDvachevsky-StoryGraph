#include "NovelMind/editor/qt/performance_metrics.hpp"

#include <QDebug>
#include <QMutexLocker>
#include <QStringList>

namespace NovelMind::editor::qt {

// =============================================================================
// ScopedTimer Implementation
// =============================================================================

ScopedTimer::ScopedTimer(const QString& name, bool enabled) : m_name(name), m_enabled(enabled) {
  if (m_enabled) {
    m_start = Clock::now();
  }
}

ScopedTimer::~ScopedTimer() {
  if (m_enabled && !m_stopped) {
    stop();
  }
}

double ScopedTimer::elapsedMs() const {
  auto now = Clock::now();
  return std::chrono::duration<double, std::milli>(now - m_start).count();
}

double ScopedTimer::stop() {
  if (!m_enabled || m_stopped) {
    return 0.0;
  }

  m_stopped = true;
  double ms = elapsedMs();
  PerformanceMetrics::instance().recordTiming(m_name, ms);
  return ms;
}

// =============================================================================
// PerformanceMetrics Implementation
// =============================================================================

PerformanceMetrics& PerformanceMetrics::instance() {
  static PerformanceMetrics instance;
  return instance;
}

PerformanceMetrics::PerformanceMetrics() : QObject(nullptr) {}

void PerformanceMetrics::setEnabled(bool enabled) {
  m_enabled.store(enabled);
}

void PerformanceMetrics::recordTiming(const QString& name, double ms) {
  if (!m_enabled.load()) {
    return;
  }

  QMutexLocker locker(&m_mutex);

  auto& stats = m_timingStats[name];
  if (stats.name.isEmpty()) {
    stats.name = name;
  }
  stats.addSample(ms);

  // Check for slow operations (> 16ms = 60fps frame budget)
  if (ms > 16.0) {
    locker.unlock();
    emit metricThresholdExceeded(name, ms, 16.0);
  }
}

void PerformanceMetrics::recordCount(const QString& name, int count) {
  if (!m_enabled.load()) {
    return;
  }

  QMutexLocker locker(&m_mutex);
  m_countStats[name] = count;
}

MetricStats PerformanceMetrics::getStats(const QString& name) const {
  QMutexLocker locker(&m_mutex);

  auto it = m_timingStats.find(name);
  if (it != m_timingStats.end()) {
    return it->second;
  }

  MetricStats empty;
  empty.name = name;
  return empty;
}

QStringList PerformanceMetrics::metricNames() const {
  QMutexLocker locker(&m_mutex);

  QStringList names;
  for (const auto& [name, _] : m_timingStats) {
    names.append(name);
  }
  return names;
}

void PerformanceMetrics::reset() {
  QMutexLocker locker(&m_mutex);
  m_timingStats.clear();
  m_countStats.clear();
}

void PerformanceMetrics::reset(const QString& name) {
  QMutexLocker locker(&m_mutex);

  auto it = m_timingStats.find(name);
  if (it != m_timingStats.end()) {
    it->second.reset();
  }

  m_countStats.erase(name);
}

QString PerformanceMetrics::getSummary() const {
  QMutexLocker locker(&m_mutex);

  QString summary;
  summary += "=== Performance Metrics ===\n";

  summary += "\nTiming Metrics:\n";
  for (const auto& [name, stats] : m_timingStats) {
    summary += QString("  %1: avg=%.2f ms, min=%.2f ms, max=%.2f ms, "
                       "samples=%2\n")
                   .arg(name)
                   .arg(stats.avgMs, 0, 'f', 2)
                   .arg(stats.minMs, 0, 'f', 2)
                   .arg(stats.maxMs, 0, 'f', 2)
                   .arg(stats.sampleCount);
  }

  summary += "\nCount Metrics:\n";
  for (const auto& [name, count] : m_countStats) {
    summary += QString("  %1: %2\n").arg(name).arg(count);
  }

  return summary;
}

void PerformanceMetrics::logMetrics() const {
  qDebug().noquote() << getSummary();
}

} // namespace NovelMind::editor::qt
