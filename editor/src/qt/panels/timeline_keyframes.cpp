/**
 * @file timeline_keyframes.cpp
 * @brief NMTimelinePanel keyframe operation methods
 *
 * Extracted from nm_timeline_panel.cpp as part of refactoring to improve
 * maintainability. Handles NMTimelinePanel keyframe operations including:
 * - Jump to next/previous keyframe
 * - Duplicate selected keyframes
 * - Set easing for selected keyframes
 * - Copy/paste keyframes
 * - Add/delete keyframes
 *
 * TimelineTrack methods are in timeline_track_operations.cpp
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"

#include <QMutexLocker>
#include <climits>

namespace NovelMind::editor::qt {

// =============================================================================
// NMTimelinePanel Keyframe Operations
// =============================================================================

void NMTimelinePanel::jumpToNextKeyframe() {
  // Find the next keyframe after current frame across all tracks
  int nextFrame = INT_MAX;

  for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
    TimelineTrack* track = it.value();
    if (!track || !track->visible) {
      continue;
    }
    for (const Keyframe& kf : track->keyframes) {
      if (kf.frame > m_currentFrame && kf.frame < nextFrame) {
        nextFrame = kf.frame;
      }
    }
  }

  if (nextFrame != INT_MAX) {
    setCurrentFrame(nextFrame);
  }
}

void NMTimelinePanel::jumpToPrevKeyframe() {
  // Find the previous keyframe before current frame across all tracks
  int prevFrame = -1;

  for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
    TimelineTrack* track = it.value();
    if (!track || !track->visible) {
      continue;
    }
    for (const Keyframe& kf : track->keyframes) {
      if (kf.frame < m_currentFrame && kf.frame > prevFrame) {
        prevFrame = kf.frame;
      }
    }
  }

  if (prevFrame >= 0) {
    setCurrentFrame(prevFrame);
  }
}

void NMTimelinePanel::duplicateSelectedKeyframes(int offsetFrames) {
  if (m_selectedKeyframes.isEmpty() || offsetFrames <= 0) {
    return;
  }

  // Get thread-safe atomic copy of track names to prevent TOCTOU race condition
  // This ensures the track names snapshot is consistent throughout the
  // operation
  const QStringList trackNamesCopy = getTrackNamesSafe();

  // Collect keyframes to duplicate
  QVector<QPair<QString, KeyframeSnapshot>> toDuplicate;

  for (const KeyframeId& id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack* track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track || track->locked) {
      continue;
    }

    Keyframe* kf = track->getKeyframe(id.frame);
    if (kf) {
      KeyframeSnapshot snapshot;
      snapshot.frame = kf->frame + offsetFrames;
      snapshot.value = kf->value;
      snapshot.easingType = static_cast<int>(kf->easing);
      snapshot.handleInX = kf->handleInX;
      snapshot.handleInY = kf->handleInY;
      snapshot.handleOutX = kf->handleOutX;
      snapshot.handleOutY = kf->handleOutY;
      toDuplicate.append(qMakePair(trackName, snapshot));
    }
  }

  // Add duplicated keyframes (outside lock to avoid holding lock during undo)
  for (const auto& pair : toDuplicate) {
    auto* cmd = new AddKeyframeCommand(this, pair.first, pair.second);
    NMUndoManager::instance().pushCommand(cmd);
  }

  renderTracks();
}

void NMTimelinePanel::setSelectedKeyframesEasing(EasingType easing) {
  if (m_selectedKeyframes.isEmpty()) {
    return;
  }

  // Get thread-safe atomic copy of track names to prevent TOCTOU race condition
  const QStringList trackNamesCopy = getTrackNamesSafe();

  // Collect changes to emit signals outside of mutex lock
  QVector<QPair<QString, int>> easingChanges;

  for (const KeyframeId& id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack* track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track || track->locked) {
      continue;
    }

    Keyframe* kf = track->getKeyframe(id.frame);
    if (kf) {
      EasingType oldEasing = kf->easing;
      kf->easing = easing;
      easingChanges.append(qMakePair(trackName, id.frame));

      // Create undo command (simplified - ideally would batch these)
      Q_UNUSED(oldEasing);
    }
  }

  // Emit signals outside lock
  for (const auto& change : easingChanges) {
    emit keyframeEasingChanged(change.first, change.second, easing);
  }

  renderTracks();
}

void NMTimelinePanel::copySelectedKeyframes() {
  m_keyframeClipboard.clear();

  if (m_selectedKeyframes.isEmpty()) {
    return;
  }

  // Get thread-safe atomic copy of track names to prevent TOCTOU race condition
  const QStringList trackNamesCopy = getTrackNamesSafe();

  // Find minimum frame to use as reference point
  int minFrame = INT_MAX;
  for (const KeyframeId& id : m_selectedKeyframes) {
    if (id.frame < minFrame) {
      minFrame = id.frame;
    }
  }

  // Copy keyframes with relative frame offsets
  for (const KeyframeId& id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack* track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track) {
      continue;
    }

    Keyframe* kf = track->getKeyframe(id.frame);
    if (kf) {
      KeyframeCopy copy;
      copy.relativeFrame = kf->frame - minFrame;
      copy.value = kf->value;
      copy.easing = kf->easing;
      m_keyframeClipboard.append(copy);
    }
  }
}

void NMTimelinePanel::pasteKeyframes() {
  if (m_keyframeClipboard.isEmpty()) {
    return;
  }

  // Get thread-safe atomic copy of track names to prevent TOCTOU race condition
  const QStringList trackNamesCopy = getTrackNamesSafe();

  // Get the first selected track or first visible track
  QString targetTrack;
  for (const KeyframeId& id : m_selectedKeyframes) {
    if (id.trackIndex >= 0 && id.trackIndex < trackNamesCopy.size()) {
      targetTrack = trackNamesCopy.at(id.trackIndex);
      break;
    }
  }

  if (targetTrack.isEmpty()) {
    // Use first visible track (with mutex protection)
    QMutexLocker locker(&m_tracksMutex);
    for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
      if (it.value()->visible && !it.value()->locked) {
        targetTrack = it.key();
        break;
      }
    }

    if (targetTrack.isEmpty()) {
      // Use first visible track
      for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
        if (it.value()->visible && !it.value()->locked) {
          targetTrack = it.key();
          break;
        }
      }
    }
  } // Lock released here

  if (targetTrack.isEmpty()) {
    return;
  }

  // Paste keyframes at current frame position (outside lock)
  for (const KeyframeCopy& copy : m_keyframeClipboard) {
    KeyframeSnapshot snapshot;
    snapshot.frame = m_currentFrame + copy.relativeFrame;
    snapshot.value = copy.value;
    snapshot.easingType = static_cast<int>(copy.easing);

    auto* cmd = new AddKeyframeCommand(this, targetTrack, snapshot);
    NMUndoManager::instance().pushCommand(cmd);
  }

  renderTracks();
}

void NMTimelinePanel::addKeyframeAtCurrent(const QString& trackName, const QVariant& value) {
  {
    QMutexLocker locker(&m_tracksMutex);
    if (!m_tracks.contains(trackName))
      return;
  } // Lock released here

  // Create snapshot for undo
  KeyframeSnapshot snapshot;
  snapshot.frame = m_currentFrame;
  snapshot.value = value;
  snapshot.easingType = static_cast<int>(EasingType::Linear);
  snapshot.handleInX = 0.0f;
  snapshot.handleInY = 0.0f;
  snapshot.handleOutX = 0.0f;
  snapshot.handleOutY = 0.0f;

  // Create and push add command
  auto* cmd = new AddKeyframeCommand(this, trackName, snapshot);
  NMUndoManager::instance().pushCommand(cmd);

  renderTracks();

  emit keyframeModified(trackName, m_currentFrame);
}

void NMTimelinePanel::deleteSelectedKeyframes() {
  if (m_selectedKeyframes.isEmpty())
    return;

  // Create macro command for deleting multiple keyframes
  if (m_selectedKeyframes.size() > 1) {
    NMUndoManager::instance().beginMacro("Delete Keyframes");
  }

  // Delete keyframes from data model
  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end(); ++trackIt, ++trackIndex) {
    TimelineTrack* track = trackIt.value();

    // Collect frames to delete for this track
    QVector<int> framesToDelete;
    for (const KeyframeId& id : m_selectedKeyframes) {
      if (id.trackIndex == trackIndex) {
        framesToDelete.append(id.frame);
      }
    }

    // Delete the keyframes with undo support
    for (int frame : framesToDelete) {
      // Capture keyframe state before deleting
      if (auto* kf = track->getKeyframe(frame)) {
        KeyframeSnapshot snapshot;
        snapshot.frame = frame;
        snapshot.value = kf->value;
        snapshot.easingType = static_cast<int>(kf->easing);
        snapshot.handleInX = kf->handleInX;
        snapshot.handleInY = kf->handleInY;
        snapshot.handleOutX = kf->handleOutX;
        snapshot.handleOutY = kf->handleOutY;

        // Create and push delete command
        auto* cmd = new DeleteKeyframeCommand(this, track->name, snapshot);
        NMUndoManager::instance().pushCommand(cmd);

        emit keyframeDeleted(track->name, frame);
      }
    }
  }

  if (m_selectedKeyframes.size() > 1) {
    NMUndoManager::instance().endMacro();
  }

  // Clear selection
  m_selectedKeyframes.clear();

  // Re-render
  renderTracks();
}

} // namespace NovelMind::editor::qt
