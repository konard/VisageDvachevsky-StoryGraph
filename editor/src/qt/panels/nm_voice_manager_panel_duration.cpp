/**
 * @file nm_voice_manager_panel_duration.cpp
 * @brief Duration probing implementation for Voice Manager Panel
 *
 * This file contains the async duration probing functionality extracted from
 * nm_voice_manager_panel.cpp for better modularity and maintainability.
 *
 * Refactored as part of issue #487 to split large monolithic file.
 */

#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QTimer>

#ifdef QT_DEBUG
#include <QDebug>
#endif

namespace NovelMind::editor::qt {

// ============================================================================
// DurationProbeTask implementation (issue #469)
// ============================================================================

DurationProbeTask::DurationProbeTask(const QString &path,
                                     std::shared_ptr<std::atomic<bool>> cancelled,
                                     QObject *receiver)
    : m_path(path), m_cancelled(cancelled), m_receiver(receiver) {
  setAutoDelete(true); // QThreadPool will delete this task when done
}

void DurationProbeTask::run() {
  // Check if task was cancelled before starting
  if (m_cancelled->load()) {
    return;
  }

  // Use QMediaPlayer to probe duration in background thread
  // Note: QMediaPlayer must be created in the thread where it will be used
  QMediaPlayer player;

  // Set up synchronization for duration reading
  QEventLoop loop;
  double duration = -1.0;
  bool finished = false;

  // Connect to get duration
  QObject::connect(&player, &QMediaPlayer::durationChanged,
                   [&duration, &loop, &finished](qint64 durationMs) {
    if (durationMs > 0 && !finished) {
      duration = static_cast<double>(durationMs) / 1000.0;
      finished = true;
      loop.quit();
    }
  });

  // Connect to handle errors
  QObject::connect(&player, &QMediaPlayer::errorOccurred,
                   [&loop, &finished]([[maybe_unused]] QMediaPlayer::Error error,
                                      [[maybe_unused]] const QString& errorString) {
    if (!finished) {
      finished = true;
      loop.quit();
    }
  });

  // Set source and wait for duration
  player.setSource(QUrl::fromLocalFile(m_path));

  // Wait for duration or error (with timeout)
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, [&loop, &finished]() {
    if (!finished) {
      finished = true;
      loop.quit();
    }
  });
  timeout.start(5000); // 5 second timeout per file

  // Check cancellation periodically
  QTimer cancelCheck;
  QObject::connect(&cancelCheck, &QTimer::timeout, [this, &loop, &finished]() {
    if (m_cancelled->load() && !finished) {
      finished = true;
      loop.quit();
    }
  });
  cancelCheck.start(100); // Check every 100ms

  loop.exec();

  // Stop timers
  timeout.stop();
  cancelCheck.stop();

  // Emit result if valid and not cancelled
  if (duration > 0 && !m_cancelled->load() && m_receiver) {
    // Use QMetaObject::invokeMethod to safely call across threads
    QMetaObject::invokeMethod(m_receiver, "durationProbedInternal",
                              Qt::QueuedConnection,
                              Q_ARG(QString, m_path),
                              Q_ARG(double, duration));
  }
}

// ============================================================================
// Async duration probing
// ============================================================================

void NMVoiceManagerPanel::startDurationProbing() {
  if (!m_manifest || !m_probeThreadPool) {
    return;
  }

  // RACE-1 fix: Atomic check-and-cancel for thread safety
  bool wasProbing = m_isProbing.exchange(false);

  if (wasProbing) {
    // Cancel current probes
    std::lock_guard<std::mutex> lock(m_probeMutex);
    for (auto& [path, cancelled] : m_activeProbeTasks) {
      cancelled->store(true);
    }
    m_activeProbeTasks.clear();
    m_probeQueue.clear();
  }

  // Build queue under lock to prevent concurrent modification
  {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    m_probeQueue.clear();

    // Copy manifest lines to avoid iterator invalidation during iteration
    auto lines = m_manifest->getLines();

    for (const auto& line : lines) {
      auto* localeFile = line.getFile(m_currentLocale.toStdString());
      if (localeFile && !localeFile->filePath.empty()) {
        QString filePath = QString::fromStdString(localeFile->filePath);
        // Check if we have a valid cached duration
        double cached = getCachedDuration(filePath);
        if (cached <= 0 && !m_activeProbeTasks.contains(filePath)) {
          m_probeQueue.enqueue(filePath);
        }
      }
    }
  }

  // Start probing with atomic guard to prevent double-start
  bool queueNotEmpty = false;
  {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    queueNotEmpty = !m_probeQueue.isEmpty();
  }

  if (queueNotEmpty) {
    bool expected = false;
    if (m_isProbing.compare_exchange_strong(expected, true)) {
      // Successfully acquired probing lock
      processNextDurationProbe();
    }
    // If compare_exchange fails, another thread started probing - that's fine
  }
}

void NMVoiceManagerPanel::processNextDurationProbe() {
  // Check atomic flag first
  if (!m_isProbing.load() || !m_probeThreadPool) {
    return;
  }

  // Process multiple files up to MAX_CONCURRENT_PROBES
  std::vector<QString> filesToProbe;

  {
    std::lock_guard<std::mutex> lock(m_probeMutex);

    // Check if queue is empty
    if (m_probeQueue.isEmpty()) {
      // Check if there are still active tasks
      if (m_activeProbeTasks.isEmpty()) {
        m_isProbing.store(false);
        // Update the list with newly probed durations (outside lock to avoid deadlock)
        QMetaObject::invokeMethod(this, &NMVoiceManagerPanel::updateDurationsInList,
                                  Qt::QueuedConnection);
      }
      return;
    }

    // Dequeue up to MAX_CONCURRENT_PROBES files
    int slotsAvailable = MAX_CONCURRENT_PROBES - m_activeProbeTasks.size();
    while (!m_probeQueue.isEmpty() && slotsAvailable > 0) {
      QString nextFile = m_probeQueue.dequeue();
      // Skip if already being probed
      if (!m_activeProbeTasks.contains(nextFile)) {
        filesToProbe.push_back(nextFile);
        slotsAvailable--;
      }
    }
  }

  // Submit tasks outside lock
  for (const QString& filePath : filesToProbe) {
    // Create cancellation token
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    {
      std::lock_guard<std::mutex> lock(m_probeMutex);
      m_activeProbeTasks[filePath] = cancelled;
    }

    // Create and submit task
    auto* task = new DurationProbeTask(filePath, cancelled, this);
    m_probeThreadPool->start(task);

    if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
      qDebug() << "Started duration probe for:" << filePath;
#endif
    }
  }
}

void NMVoiceManagerPanel::onProbeDurationFinished() {
  // Legacy function - no longer used with thread pool implementation
  // Kept for compatibility, does nothing
}

void NMVoiceManagerPanel::onDurationProbed(const QString &filePath, double duration) {
  if (!m_manifest || filePath.isEmpty() || duration <= 0) {
    // Remove from active tasks
    {
      std::lock_guard<std::mutex> lock(m_probeMutex);
      m_activeProbeTasks.remove(filePath);
    }
    // Try to process next batch
    processNextDurationProbe();
    return;
  }

  // Cache the duration
  cacheDuration(filePath, duration);

  // Update the duration in manifest for current locale
  std::string currentFilePath = filePath.toStdString();
  for (auto& line : m_manifest->getLines()) {
    auto* localeFile = line.getFile(m_currentLocale.toStdString());
    if (localeFile && localeFile->filePath == currentFilePath) {
      // Get mutable line to update
      auto* mutableLine = m_manifest->getLineMutable(line.id);
      if (mutableLine) {
        auto& mutableLocaleFile = mutableLine->getOrCreateFile(m_currentLocale.toStdString());
        mutableLocaleFile.duration = static_cast<float>(duration);
      }
      break;
    }
  }

  // Remove from active tasks
  {
    std::lock_guard<std::mutex> lock(m_probeMutex);
    m_activeProbeTasks.remove(filePath);
  }

  if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
    qDebug() << "Duration probed:" << filePath << "=" << duration << "s";
#endif
  }

  // Continue with next batch
  processNextDurationProbe();
}

double NMVoiceManagerPanel::getCachedDuration(const QString& filePath) const {
  auto it = m_durationCache.find(filePath.toStdString());
  if (it == m_durationCache.end()) {
    return 0.0;
  }

  // Check if file has been modified
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return 0.0;
  }

  qint64 currentMtime = fileInfo.lastModified().toMSecsSinceEpoch();
  if (currentMtime != it->second.mtime) {
    return 0.0; // Cache invalidated
  }

  return it->second.duration;
}

void NMVoiceManagerPanel::cacheDuration(const QString& filePath, double duration) {
  QFileInfo fileInfo(filePath);
  DurationCacheEntry entry;
  entry.duration = duration;
  entry.mtime = fileInfo.lastModified().toMSecsSinceEpoch();

  m_durationCache[filePath.toStdString()] = entry;
}

void NMVoiceManagerPanel::updateDurationsInList() {
  if (!m_voiceTree || !m_manifest)
    return;

  // Update durations in the tree widget items
  for (int i = 0; i < m_voiceTree->topLevelItemCount(); ++i) {
    auto* item = m_voiceTree->topLevelItem(i);
    if (!item)
      continue;

    QString dialogueId = item->data(0, Qt::UserRole).toString();
    auto* line = m_manifest->getLine(dialogueId.toStdString());
    if (line) {
      auto* localeFile = line->getFile(m_currentLocale.toStdString());
      if (localeFile && localeFile->duration > 0) {
        qint64 durationMs = static_cast<qint64>(localeFile->duration * 1000);
        item->setText(6, formatDuration(durationMs)); // Column 6 is duration
      }
    }
  }
}

} // namespace NovelMind::editor::qt
