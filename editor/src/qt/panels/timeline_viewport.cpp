/**
 * @file timeline_viewport.cpp
 * @brief Viewport management logic for NMTimelinePanel
 *
 * Extracted from nm_timeline_panel.cpp as part of refactoring to improve
 * maintainability. Handles all viewport-related functionality including:
 * - Zoom operations (zoom in, zoom out, zoom to fit)
 * - Frame/coordinate conversion (frameToX, xToFrame)
 * - Playback control methods
 * - Track management operations
 * - Snap to grid settings
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"

#include <QMutexLocker>

namespace NovelMind::editor::qt {

// =============================================================================
// Coordinate Conversion
// =============================================================================

int NMTimelinePanel::frameToX(int frame) const {
  return TRACK_HEADER_WIDTH + frame * m_pixelsPerFrame;
}

int NMTimelinePanel::xToFrame(int x) const {
  return (x - TRACK_HEADER_WIDTH) / m_pixelsPerFrame;
}

// =============================================================================
// Zoom Operations
// =============================================================================

void NMTimelinePanel::zoomIn() {
  m_zoom *= 1.2f;
  m_pixelsPerFrame = static_cast<int>(4 * m_zoom);
  renderTracks();
}

void NMTimelinePanel::zoomOut() {
  m_zoom /= 1.2f;
  if (m_zoom < 0.1f)
    m_zoom = 0.1f;
  m_pixelsPerFrame = static_cast<int>(4 * m_zoom);
  renderTracks();
}

void NMTimelinePanel::zoomToFit() {
  m_zoom = 1.0f;
  m_pixelsPerFrame = 4;
  renderTracks();
}

// =============================================================================
// Playback Control Methods
// =============================================================================

void NMTimelinePanel::setCurrentFrame(int frame) {
  if (frame < 0)
    frame = 0;
  if (frame > m_totalFrames)
    frame = m_totalFrames;

  m_currentFrame = frame;
  m_frameSpinBox->blockSignals(true);
  m_frameSpinBox->setValue(frame);
  m_frameSpinBox->blockSignals(false);

  updatePlayhead();
  updateFrameDisplay();

  emit frameChanged(frame);
}

void NMTimelinePanel::togglePlayback() {
  m_playing = !m_playing;

  if (m_playing) {
    m_playbackTime = static_cast<double>(m_currentFrame) / m_fps;
    m_btnPlay->setText("\u23F8"); // Pause symbol
  } else {
    m_btnPlay->setText("\u25B6"); // Play symbol
  }

  emit playbackStateChanged(m_playing);
}

void NMTimelinePanel::stopPlayback() {
  m_playing = false;
  m_btnPlay->setChecked(false);
  m_btnPlay->setText("\u25B6");
  setCurrentFrame(m_playbackStartFrame);
  emit playbackStateChanged(false);
}

void NMTimelinePanel::stepForward() {
  setCurrentFrame(m_currentFrame + 1);
}

void NMTimelinePanel::stepBackward() {
  setCurrentFrame(m_currentFrame - 1);
}

void NMTimelinePanel::onPlayModeFrameChanged(int frame) {
  setCurrentFrame(frame);
}

// =============================================================================
// Track Management
// =============================================================================

void NMTimelinePanel::addTrack(TimelineTrackType type, const QString& name) {
  {
    QMutexLocker locker(&m_tracksMutex);
    if (m_tracks.contains(name))
      return;

    TimelineTrack* track = new TimelineTrack();
    track->name = name;
    track->type = type;

    // Assign color based on type
    switch (type) {
    case TimelineTrackType::Audio:
      track->color = QColor("#4CAF50");
      break;
    case TimelineTrackType::Animation:
      track->color = QColor("#2196F3");
      break;
    case TimelineTrackType::Event:
      track->color = QColor("#FF9800");
      break;
    case TimelineTrackType::Camera:
      track->color = QColor("#9C27B0");
      break;
    case TimelineTrackType::Character:
      track->color = QColor("#F44336");
      break;
    case TimelineTrackType::Effect:
      track->color = QColor("#00BCD4");
      break;
    case TimelineTrackType::Dialogue:
      track->color = QColor("#8BC34A");
      break;
    case TimelineTrackType::Variable:
      track->color = QColor("#9E9E9E");
      break;
    }

    m_tracks[name] = track;
  } // Lock released here

  renderTracks();
}

void NMTimelinePanel::removeTrack(const QString& name) {
  {
    QMutexLocker locker(&m_tracksMutex);
    if (!m_tracks.contains(name))
      return;

    delete m_tracks[name];
    m_tracks.remove(name);
  } // Lock released here

  renderTracks();
}

TimelineTrack* NMTimelinePanel::getTrack(const QString& name) const {
  QMutexLocker locker(&m_tracksMutex);
  auto it = m_tracks.find(name);
  if (it != m_tracks.end()) {
    return it.value();
  }
  return nullptr;
}

QStringList NMTimelinePanel::getTrackNamesSafe() const {
  QMutexLocker locker(&m_tracksMutex);
  return m_tracks.keys();
}

// =============================================================================
// Grid and Snapping
// =============================================================================

void NMTimelinePanel::setSnapToGrid(bool enabled) {
  if (m_snapToGrid == enabled) {
    return;
  }
  m_snapToGrid = enabled;
  renderTracks();
}

void NMTimelinePanel::setGridSize(int frames) {
  if (frames < 1) {
    frames = 1;
  }
  if (m_gridSize == frames) {
    return;
  }
  m_gridSize = frames;
  renderTracks();
}

} // namespace NovelMind::editor::qt
