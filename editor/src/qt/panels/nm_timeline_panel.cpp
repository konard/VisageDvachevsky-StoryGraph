#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_keyframe_item.hpp"
#include "NovelMind/editor/qt/performance_metrics.hpp"

#include <QBrush>
#include <QMutexLocker>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMutexLocker>
#include <QPen>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <climits>
#include <cmath>

namespace NovelMind::editor::qt {

// Forward declaration for easing function
static float applyEasingFunction(float t, EasingType easing);

// Helper function for cubic Bezier curve evaluation
static float evaluateCubicBezier(float t, float p0, float p1, float p2,
                                 float p3) {
  // Standard cubic Bezier formula: B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 +
  // 3*(1-t)*t^2*P2 + t^3*P3
  float oneMinusT = 1.0f - t;
  return oneMinusT * oneMinusT * oneMinusT * p0 +
         3.0f * oneMinusT * oneMinusT * t * p1 + 3.0f * oneMinusT * t * t * p2 +
         t * t * t * p3;
}

// Helper function to find t for a given x in a cubic Bezier curve (Newton's
// method)
static float solveBezierX(float x, float p0x, float p1x, float p2x, float p3x) {
  // Use Newton-Raphson iteration to find t such that Bezier_x(t) = x
  float t = x; // Initial guess
  for (int i = 0; i < 8; ++i) {
    float currentX = evaluateCubicBezier(t, p0x, p1x, p2x, p3x);
    if (std::abs(currentX - x) < 0.001f) {
      break; // Close enough
    }
    // Derivative of cubic Bezier
    float derivative = 3.0f * (1.0f - t) * (1.0f - t) * (p1x - p0x) +
                       6.0f * (1.0f - t) * t * (p2x - p1x) +
                       3.0f * t * t * (p3x - p2x);
    if (std::abs(derivative) < 0.00001f) {
      break; // Avoid division by zero
    }
    t -= (currentX - x) / derivative;
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to [0,1]
  }
  return t;
}

// =============================================================================
// TimelineTrack Implementation
// =============================================================================

void TimelineTrack::addKeyframe(int frame, const QVariant &value,
                                EasingType easing) {
  // Check if keyframe already exists
  for (auto &kf : keyframes) {
    if (kf.frame == frame) {
      kf.value = value;
      kf.easing = easing;
      return;
    }
  }

  // Add new keyframe
  Keyframe newKeyframe;
  newKeyframe.frame = frame;
  newKeyframe.value = value;
  newKeyframe.easing = easing;
  keyframes.append(newKeyframe);

  // Sort keyframes by frame
  std::sort(
      keyframes.begin(), keyframes.end(),
      [](const Keyframe &a, const Keyframe &b) { return a.frame < b.frame; });
}

void TimelineTrack::removeKeyframe(int frame) {
  keyframes.erase(
      std::remove_if(keyframes.begin(), keyframes.end(),
                     [frame](const Keyframe &kf) { return kf.frame == frame; }),
      keyframes.end());
}

void TimelineTrack::moveKeyframe(int fromFrame, int toFrame) {
  Keyframe *kf = getKeyframe(fromFrame);
  if (kf && fromFrame != toFrame) {
    // Store keyframe data
    QVariant value = kf->value;
    EasingType easing = kf->easing;
    bool selected = kf->isSelected;

    // Remove old keyframe
    removeKeyframe(fromFrame);

    // Add at new position
    addKeyframe(toFrame, value, easing);

    // Restore selection state
    Keyframe *newKf = getKeyframe(toFrame);
    if (newKf) {
      newKf->isSelected = selected;
    }
  }
}

Keyframe *TimelineTrack::getKeyframe(int frame) {
  for (auto &kf : keyframes) {
    if (kf.frame == frame)
      return &kf;
  }
  return nullptr;
}

Keyframe TimelineTrack::interpolate(int frame) const {
  if (keyframes.isEmpty()) {
    return Keyframe();
  }

  // If only one keyframe, return it
  if (keyframes.size() == 1) {
    return keyframes.first();
  }

  // Find exact match first
  for (const auto &kf : keyframes) {
    if (kf.frame == frame)
      return kf;
  }

  // Find surrounding keyframes for interpolation
  const Keyframe *prevKf = nullptr;
  const Keyframe *nextKf = nullptr;

  for (int i = 0; i < keyframes.size(); ++i) {
    if (keyframes[i].frame <= frame) {
      prevKf = &keyframes[i];
    }
    if (keyframes[i].frame >= frame && !nextKf) {
      nextKf = &keyframes[i];
      break;
    }
  }

  // Before first keyframe - return first
  if (!prevKf) {
    return keyframes.first();
  }

  // After last keyframe - return last
  if (!nextKf) {
    return keyframes.last();
  }

  // At exact keyframe position
  if (prevKf->frame == frame) {
    return *prevKf;
  }
  if (nextKf->frame == frame) {
    return *nextKf;
  }

  // Interpolate between prevKf and nextKf
  Keyframe result;
  result.frame = frame;
  result.easing = prevKf->easing;

  // Calculate interpolation factor (0 to 1)
  // Safe division with epsilon check to prevent division by zero
  float frameDiff = static_cast<float>(nextKf->frame - prevKf->frame);
  if (frameDiff < 0.0001f) {
    // Same frame or extremely close - no interpolation needed
    return *prevKf;
  }
  float t = static_cast<float>(frame - prevKf->frame) / frameDiff;
  t = std::clamp(t, 0.0f, 1.0f); // Additional safety clamp

  // Apply easing function to t
  double easedT;
  if (prevKf->easing == EasingType::Custom) {
    // Use Bezier curve data from keyframe handles
    // Construct cubic Bezier curve from handles
    // P0 = (0, 0), P1 = (handleOutX, handleOutY)
    // P2 = (1 + handleInX, 1 + handleInY), P3 = (1, 1)
    float p0x = 0.0f, p0y = 0.0f;
    float p1x = prevKf->handleOutX, p1y = prevKf->handleOutY;
    float p2x = 1.0f + nextKf->handleInX, p2y = 1.0f + nextKf->handleInY;
    float p3x = 1.0f, p3y = 1.0f;

    // Solve for the parameter value that gives us the current x position
    float bezierT = solveBezierX(t, p0x, p1x, p2x, p3x);

    // Evaluate the y value at that parameter
    easedT =
        static_cast<double>(evaluateCubicBezier(bezierT, p0y, p1y, p2y, p3y));
  } else {
    // Use standard easing function
    easedT = static_cast<double>(applyEasingFunction(t, prevKf->easing));
  }

  // Interpolate value based on type
  int typeId = prevKf->value.typeId();

  if (typeId == QMetaType::Double || typeId == QMetaType::Int) {
    // Numeric interpolation
    double startVal = prevKf->value.toDouble();
    double endVal = nextKf->value.toDouble();
    result.value = startVal + (endVal - startVal) * easedT;
  } else if (typeId == QMetaType::QPointF) {
    // Point interpolation
    QPointF startPt = prevKf->value.toPointF();
    QPointF endPt = nextKf->value.toPointF();
    result.value = QPointF(startPt.x() + (endPt.x() - startPt.x()) * easedT,
                           startPt.y() + (endPt.y() - startPt.y()) * easedT);
  } else if (typeId == QMetaType::QColor) {
    // Color interpolation
    QColor startColor = prevKf->value.value<QColor>();
    QColor endColor = nextKf->value.value<QColor>();
    result.value = QColor(
        static_cast<int>(
            static_cast<double>(startColor.red()) +
            static_cast<double>(endColor.red() - startColor.red()) * easedT),
        static_cast<int>(
            static_cast<double>(startColor.green()) +
            static_cast<double>(endColor.green() - startColor.green()) *
                easedT),
        static_cast<int>(
            static_cast<double>(startColor.blue()) +
            static_cast<double>(endColor.blue() - startColor.blue()) * easedT),
        static_cast<int>(
            static_cast<double>(startColor.alpha()) +
            static_cast<double>(endColor.alpha() - startColor.alpha()) *
                easedT));
  } else {
    // For unsupported types, use step interpolation (use prev value)
    result.value = prevKf->value;
  }

  return result;
}

// Helper function to apply easing
static float applyEasingFunction(float t, EasingType easing) {
  // Clamp t to [0, 1]
  t = std::max(0.0f, std::min(1.0f, t));

  switch (easing) {
  case EasingType::Linear:
    return t;

  case EasingType::EaseIn:
  case EasingType::EaseInQuad:
    return t * t;

  case EasingType::EaseOut:
  case EasingType::EaseOutQuad:
    return t * (2.0f - t);

  case EasingType::EaseInOut:
  case EasingType::EaseInOutQuad:
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

  case EasingType::EaseInCubic:
    return t * t * t;

  case EasingType::EaseOutCubic: {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
  }

  case EasingType::EaseInOutCubic:
    return t < 0.5f
               ? 4.0f * t * t * t
               : 1.0f + (t - 1.0f) * (2.0f * (t - 1.0f)) * (2.0f * (t - 1.0f));

  case EasingType::EaseInElastic: {
    if (t == 0.0f || t == 1.0f)
      return t;
    float p = 0.3f;
    return -std::pow(2.0f, 10.0f * (t - 1.0f)) *
           std::sin((t - 1.0f - p / 4.0f) * (2.0f * 3.14159f) / p);
  }

  case EasingType::EaseOutElastic: {
    if (t == 0.0f || t == 1.0f)
      return t;
    float p = 0.3f;
    return std::pow(2.0f, -10.0f * t) *
               std::sin((t - p / 4.0f) * (2.0f * 3.14159f) / p) +
           1.0f;
  }

  case EasingType::EaseInBounce:
    return 1.0f - applyEasingFunction(1.0f - t, EasingType::EaseOutBounce);

  case EasingType::EaseOutBounce: {
    if (t < (1.0f / 2.75f)) {
      return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
      t -= 1.5f / 2.75f;
      return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
      t -= 2.25f / 2.75f;
      return 7.5625f * t * t + 0.9375f;
    } else {
      t -= 2.625f / 2.75f;
      return 7.5625f * t * t + 0.984375f;
    }
  }

  case EasingType::Step:
    return t < 1.0f ? 0.0f : 1.0f;

  case EasingType::Custom:
    // Use Bezier curve data from keyframe handles
    // This is intentionally left as a simplified fallback since full Bezier
    // interpolation requires access to both keyframes (start and end) and their
    // handles. The actual Bezier curve interpolation should be implemented in
    // TimelineTrack::interpolate() where both keyframes are available.
    // For standalone easing function, use cubic ease-in-out as approximation.
    return t < 0.5f ? 4.0f * t * t * t
                    : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

  default:
    return t;
  }
}

QList<Keyframe *> TimelineTrack::selectedKeyframes() {
  QList<Keyframe *> selected;
  for (auto &kf : keyframes) {
    if (kf.isSelected) {
      selected.append(&kf);
    }
  }
  return selected;
}

void TimelineTrack::selectKeyframesInRange(int startFrame, int endFrame) {
  for (auto &kf : keyframes) {
    if (kf.frame >= startFrame && kf.frame <= endFrame) {
      kf.isSelected = true;
    }
  }
}

void TimelineTrack::clearSelection() {
  for (auto &kf : keyframes) {
    kf.isSelected = false;
  }
}

// =============================================================================
// NMTimelinePanel Implementation
// =============================================================================

NMTimelinePanel::NMTimelinePanel(QWidget *parent)
    : NMDockPanel("Timeline", parent) {
  // Timeline needs width for multiple tracks and height for playback controls
  setMinimumPanelSize(350, 180);

  // Initialize render cache with proper config
  TimelineRenderCacheConfig cacheConfig;
  cacheConfig.maxMemoryBytes = 32 * 1024 * 1024; // 32 MB
  cacheConfig.tileWidth = 256;
  cacheConfig.tileHeight = TRACK_HEIGHT;
  cacheConfig.enableCache = true;
  m_renderCache = std::make_unique<TimelineRenderCache>(cacheConfig, this);
}

NMTimelinePanel::~NMTimelinePanel() = default;

void NMTimelinePanel::onInitialize() {
  setupUI();

  // Create default tracks
  addTrack(TimelineTrackType::Audio, "Background Music");
  addTrack(TimelineTrackType::Animation, "Character Animation");
  addTrack(TimelineTrackType::Event, "Story Events");
}

void NMTimelinePanel::onShutdown() {
  // Clean up tracks
  qDeleteAll(m_tracks);
  m_tracks.clear();
}

void NMTimelinePanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget());
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  setupPlaybackControls();

  setupTrackView();
  mainLayout->addWidget(m_timelineView, 1);
}

void NMTimelinePanel::setupToolbar() {
  m_toolbar = new QToolBar(contentWidget());
  m_toolbar->setObjectName("TimelineToolbar");

  auto &iconMgr = NMIconManager::instance();

  // Playback controls
  m_btnPlay = new QPushButton(m_toolbar);
  m_btnPlay->setIcon(iconMgr.getIcon("play", 16));
  m_btnPlay->setCheckable(true);
  m_btnPlay->setToolTip("Play/Pause (Space)");
  m_btnPlay->setFlat(true);
  connect(m_btnPlay, &QPushButton::clicked, this,
          &NMTimelinePanel::togglePlayback);

  m_btnStop = new QPushButton(m_toolbar);
  m_btnStop->setIcon(iconMgr.getIcon("stop", 16));
  m_btnStop->setToolTip("Stop");
  m_btnStop->setFlat(true);
  connect(m_btnStop, &QPushButton::clicked, this,
          &NMTimelinePanel::stopPlayback);

  m_btnStepBack = new QPushButton(m_toolbar);
  m_btnStepBack->setIcon(iconMgr.getIcon("step-backward", 16));
  m_btnStepBack->setToolTip("Step Backward");
  m_btnStepBack->setFlat(true);
  connect(m_btnStepBack, &QPushButton::clicked, this,
          &NMTimelinePanel::stepBackward);

  m_btnStepForward = new QPushButton(m_toolbar);
  m_btnStepForward->setIcon(iconMgr.getIcon("step-forward", 16));
  m_btnStepForward->setToolTip("Step Forward");
  m_btnStepForward->setFlat(true);
  connect(m_btnStepForward, &QPushButton::clicked, this,
          &NMTimelinePanel::stepForward);

  // Frame display
  m_frameSpinBox = new QSpinBox(m_toolbar);
  m_frameSpinBox->setMinimum(0);
  m_frameSpinBox->setMaximum(m_totalFrames);
  m_frameSpinBox->setValue(m_currentFrame);
  m_frameSpinBox->setToolTip("Current Frame");
  connect(m_frameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &NMTimelinePanel::setCurrentFrame);

  m_timeLabel = new QLabel("00:00.00", m_toolbar);
  m_timeLabel->setMinimumWidth(60);

  // Zoom controls
  m_btnZoomIn = new QPushButton(m_toolbar);
  m_btnZoomIn->setIcon(iconMgr.getIcon("zoom-in", 16));
  m_btnZoomIn->setToolTip("Zoom In");
  m_btnZoomIn->setFlat(true);
  connect(m_btnZoomIn, &QPushButton::clicked, this, &NMTimelinePanel::zoomIn);

  m_btnZoomOut = new QPushButton(m_toolbar);
  m_btnZoomOut->setIcon(iconMgr.getIcon("zoom-out", 16));
  m_btnZoomOut->setToolTip("Zoom Out");
  m_btnZoomOut->setFlat(true);
  connect(m_btnZoomOut, &QPushButton::clicked, this, &NMTimelinePanel::zoomOut);

  m_btnZoomFit = new QPushButton(m_toolbar);
  m_btnZoomFit->setIcon(iconMgr.getIcon("zoom-fit", 16));
  m_btnZoomFit->setToolTip("Zoom to Fit");
  m_btnZoomFit->setFlat(true);
  connect(m_btnZoomFit, &QPushButton::clicked, this,
          &NMTimelinePanel::zoomToFit);

  // Add widgets to toolbar
  m_toolbar->addWidget(m_btnPlay);
  m_toolbar->addWidget(m_btnStop);
  m_toolbar->addSeparator();
  m_toolbar->addWidget(m_btnStepBack);
  m_toolbar->addWidget(m_btnStepForward);
  m_toolbar->addSeparator();
  m_toolbar->addWidget(new QLabel("Frame:", m_toolbar));
  m_toolbar->addWidget(m_frameSpinBox);
  m_toolbar->addWidget(m_timeLabel);
  m_toolbar->addSeparator();
  m_toolbar->addWidget(m_btnZoomIn);
  m_toolbar->addWidget(m_btnZoomOut);
  m_toolbar->addWidget(m_btnZoomFit);
}

void NMTimelinePanel::setupPlaybackControls() {
  // Additional playback controls can be added here
}

void NMTimelinePanel::setupTrackView() {
  m_timelineScene = new QGraphicsScene(this);
  m_timelineView = new QGraphicsView(m_timelineScene, contentWidget());
  m_timelineView->setObjectName("TimelineView");
  m_timelineView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_timelineView->setDragMode(QGraphicsView::ScrollHandDrag);
  m_timelineView->setFocusPolicy(Qt::StrongFocus);

  // Install event filter for keyboard handling
  m_timelineView->installEventFilter(this);

  // Create playhead
  m_playheadItem =
      m_timelineScene->addLine(TRACK_HEADER_WIDTH, 0, TRACK_HEADER_WIDTH, 1000,
                               QPen(QColor("#ff0000"), 2));
  m_playheadItem->setZValue(100); // Always on top

  renderTracks();
}

void NMTimelinePanel::onUpdate(double deltaTime) {
  if (!m_playing)
    return;

  m_playbackTime += deltaTime;
  int newFrame = static_cast<int>(m_playbackTime * m_fps);

  if (newFrame != m_currentFrame) {
    if (newFrame >= m_playbackEndFrame) {
      if (m_loop) {
        m_playbackTime = static_cast<double>(m_playbackStartFrame) / m_fps;
        newFrame = m_playbackStartFrame;
      } else {
        stopPlayback();
        return;
      }
    }

    setCurrentFrame(newFrame);
  }
}

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

void NMTimelinePanel::stepForward() { setCurrentFrame(m_currentFrame + 1); }

void NMTimelinePanel::stepBackward() { setCurrentFrame(m_currentFrame - 1); }

void NMTimelinePanel::addTrack(TimelineTrackType type, const QString &name) {
  {
    QMutexLocker locker(&m_tracksMutex);
    if (m_tracks.contains(name))
      return;

    TimelineTrack *track = new TimelineTrack();
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

void NMTimelinePanel::jumpToNextKeyframe() {
  // Find the next keyframe after current frame across all tracks
  int nextFrame = INT_MAX;

  for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
    TimelineTrack *track = it.value();
    if (!track || !track->visible) {
      continue;
    }
    for (const Keyframe &kf : track->keyframes) {
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
    TimelineTrack *track = it.value();
    if (!track || !track->visible) {
      continue;
    }
    for (const Keyframe &kf : track->keyframes) {
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

  for (const KeyframeId &id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack *track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track || track->locked) {
      continue;
    }

    for (const KeyframeId &id : m_selectedKeyframes) {
      // Bounds check against the atomic copy
      if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
        continue;
      }

      QString trackName = trackNamesCopy.at(id.trackIndex);

      // Get track while still holding lock
      TimelineTrack *track = m_tracks.value(trackName, nullptr);
      if (!track || track->locked) {
        continue;
      }

      Keyframe *kf = track->getKeyframe(id.frame);
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
  } // Lock released here

  // Add duplicated keyframes (outside lock to avoid holding lock during undo)
  for (const auto &pair : toDuplicate) {
    auto *cmd = new AddKeyframeCommand(this, pair.first, pair.second);
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

  for (const KeyframeId &id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack *track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track || track->locked) {
      continue;
    }

      Keyframe *kf = track->getKeyframe(id.frame);
      if (kf) {
        EasingType oldEasing = kf->easing;
        kf->easing = easing;
        easingChanges.append(qMakePair(trackName, id.frame));

        // Create undo command (simplified - ideally would batch these)
        Q_UNUSED(oldEasing);
      }
    }
  } // Lock released here

  // Emit signals outside lock
  for (const auto &change : easingChanges) {
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
  for (const KeyframeId &id : m_selectedKeyframes) {
    if (id.frame < minFrame) {
      minFrame = id.frame;
    }
  }

  // Copy keyframes with relative frame offsets
  for (const KeyframeId &id : m_selectedKeyframes) {
    if (id.trackIndex < 0 || id.trackIndex >= trackNamesCopy.size()) {
      continue;
    }
    QString trackName = trackNamesCopy.at(id.trackIndex);

    // Get track with mutex protection
    TimelineTrack *track = nullptr;
    {
      QMutexLocker locker(&m_tracksMutex);
      track = m_tracks.value(trackName, nullptr);
    }

    if (!track) {
      continue;
    }

      Keyframe *kf = track->getKeyframe(id.frame);
      if (kf) {
        KeyframeCopy copy;
        copy.relativeFrame = kf->frame - minFrame;
        copy.value = kf->value;
        copy.easing = kf->easing;
        m_keyframeClipboard.append(copy);
      }
    }
  } // Lock released here
}

void NMTimelinePanel::pasteKeyframes() {
  if (m_keyframeClipboard.isEmpty()) {
    return;
  }

  // Get thread-safe atomic copy of track names to prevent TOCTOU race condition
  const QStringList trackNamesCopy = getTrackNamesSafe();

  // Get the first selected track or first visible track
  QString targetTrack;
  for (const KeyframeId &id : m_selectedKeyframes) {
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
  for (const KeyframeCopy &copy : m_keyframeClipboard) {
    KeyframeSnapshot snapshot;
    snapshot.frame = m_currentFrame + copy.relativeFrame;
    snapshot.value = copy.value;
    snapshot.easingType = static_cast<int>(copy.easing);

    auto *cmd = new AddKeyframeCommand(this, targetTrack, snapshot);
    NMUndoManager::instance().pushCommand(cmd);
  }

  renderTracks();
}

void NMTimelinePanel::onPlayModeFrameChanged(int frame) {
  setCurrentFrame(frame);
}

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

void NMTimelinePanel::removeTrack(const QString &name) {
  {
    QMutexLocker locker(&m_tracksMutex);
    if (!m_tracks.contains(name))
      return;

    delete m_tracks[name];
    m_tracks.remove(name);
  } // Lock released here

  renderTracks();
}

void NMTimelinePanel::addKeyframeAtCurrent(const QString &trackName,
                                           const QVariant &value) {
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
  auto *cmd = new AddKeyframeCommand(this, trackName, snapshot);
  NMUndoManager::instance().pushCommand(cmd);

  renderTracks();

  emit keyframeModified(trackName, m_currentFrame);
}

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

void NMTimelinePanel::updatePlayhead() {
  int x = frameToX(m_currentFrame);
  m_playheadItem->setLine(
      x, 0, x,
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

/**
 * @brief Get or create cached frame label string (PERF-3 optimization)
 *
 * Avoids repeated QString::number() allocations during renderTracks().
 * The cache is lazily populated and bounded to prevent memory issues.
 *
 * @param frame Frame number to get label for
 * @return Cached QString reference for the frame number
 */
const QString &NMTimelinePanel::getCachedFrameLabel(int frame) const {
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
  QList<QGraphicsItem *> items = m_timelineScene->items();
  for (QGraphicsItem *item : items) {
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
      QGraphicsTextItem *label =
          m_timelineScene->addText(getCachedFrameLabel(frame));
      label->setPos(x - 10, -20);
      label->setDefaultTextColor(labelColor);
    }
  }

  // Draw tracks
  for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it, ++trackIndex) {
    TimelineTrack *track = it.value();

    // Track background
    m_timelineScene->addRect(TRACK_HEADER_WIDTH, y,
                             frameToX(m_totalFrames) - TRACK_HEADER_WIDTH,
                             TRACK_HEIGHT, noPen, trackBgBrush);

    // Track header
    m_timelineScene->addRect(0, y, TRACK_HEADER_WIDTH, TRACK_HEIGHT, noPen,
                             QBrush(track->color.darker(150)));

    QGraphicsTextItem *nameLabel = m_timelineScene->addText(track->name);
    nameLabel->setPos(8, y + 8);
    nameLabel->setDefaultTextColor(nameLabelColor);

    // Draw keyframes using custom items
    for (const Keyframe &kf : track->keyframes) {
      int kfX = frameToX(kf.frame);

      // Create custom keyframe item
      NMKeyframeItem *kfItem =
          new NMKeyframeItem(trackIndex, kf.frame, track->color);
      kfItem->setPos(kfX, y + TRACK_HEIGHT / 2);
      kfItem->setSnapToGrid(m_snapToGrid);
      kfItem->setGridSize(m_gridSize);

      // Set coordinate conversion functions
      kfItem->setFrameConverter([this](int x) { return this->xToFrame(x); },
                                [this](int f) { return this->frameToX(f); });

      // Connect signals
      connect(kfItem, &NMKeyframeItem::clicked, this,
              &NMTimelinePanel::onKeyframeClicked);
      connect(kfItem, &NMKeyframeItem::moved, this,
              &NMTimelinePanel::onKeyframeMoved);
      connect(kfItem, &NMKeyframeItem::doubleClicked, this,
              &NMTimelinePanel::onKeyframeDoubleClicked);

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
  m_timelineScene->setSceneRect(0, -30, frameToX(m_totalFrames) + 100,
                                y + TIMELINE_MARGIN);

  updatePlayhead();

  // Record performance metrics
  double renderTimeMs = static_cast<double>(timer.elapsed());
  int itemCount = static_cast<int>(m_timelineScene->items().count());
  recordRenderMetrics(renderTimeMs, itemCount);
}

int NMTimelinePanel::frameToX(int frame) const {
  return TRACK_HEADER_WIDTH + frame * m_pixelsPerFrame;
}

int NMTimelinePanel::xToFrame(int x) const {
  return (x - TRACK_HEADER_WIDTH) / m_pixelsPerFrame;
}

// =============================================================================
// Selection Management
// =============================================================================

void NMTimelinePanel::selectKeyframe(const KeyframeId &id, bool additive) {
  if (!additive) {
    // Clear previous selection
    m_selectedKeyframes.clear();
  }

  if (m_selectedKeyframes.contains(id)) {
    // Toggle if already selected in additive mode
    if (additive) {
      m_selectedKeyframes.remove(id);
    }
  } else {
    m_selectedKeyframes.insert(id);
  }

  updateSelectionVisuals();
}

void NMTimelinePanel::clearSelection() {
  m_selectedKeyframes.clear();
  updateSelectionVisuals();
}

void NMTimelinePanel::updateSelectionVisuals() {
  for (auto it = m_keyframeItems.begin(); it != m_keyframeItems.end(); ++it) {
    bool selected = m_selectedKeyframes.contains(it.key());
    it.value()->setSelected(selected);
  }

  // Update data model selection state
  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end();
       ++trackIt, ++trackIndex) {
    TimelineTrack *track = trackIt.value();
    for (auto &kf : track->keyframes) {
      KeyframeId id;
      id.trackIndex = trackIndex;
      id.frame = kf.frame;
      kf.isSelected = m_selectedKeyframes.contains(id);
    }
  }
}

// =============================================================================
// Keyframe Event Handlers
// =============================================================================

void NMTimelinePanel::onKeyframeClicked(bool additiveSelection,
                                        const KeyframeId &id) {
  selectKeyframe(id, additiveSelection);
}

void NMTimelinePanel::onKeyframeMoved(int oldFrame, int newFrame,
                                      int trackIndex) {
  // Find the track
  int currentTrackIndex = 0;
  TimelineTrack *targetTrack = nullptr;

  for (auto it = m_tracks.begin(); it != m_tracks.end();
       ++it, ++currentTrackIndex) {
    if (currentTrackIndex == trackIndex) {
      targetTrack = it.value();
      break;
    }
  }

  if (!targetTrack)
    return;

  // Create and push move command
  auto *cmd = new TimelineKeyframeMoveCommand(this, targetTrack->name, oldFrame,
                                              newFrame);
  NMUndoManager::instance().pushCommand(cmd);

  // Update selection to new position
  KeyframeId oldId;
  oldId.trackIndex = trackIndex;
  oldId.frame = oldFrame;

  KeyframeId newId;
  newId.trackIndex = trackIndex;
  newId.frame = newFrame;

  if (m_selectedKeyframes.contains(oldId)) {
    m_selectedKeyframes.remove(oldId);
    m_selectedKeyframes.insert(newId);
  }

  // Re-render to update positions
  renderTracks();

  emit keyframeMoved(targetTrack->name, oldFrame, newFrame);
}

void NMTimelinePanel::onKeyframeDoubleClicked(int trackIndex, int frame) {
  showEasingDialog(trackIndex, frame);
}

// =============================================================================
// Easing Dialog
// =============================================================================

void NMTimelinePanel::showEasingDialog(int trackIndex, int frame) {
  // Find the track and keyframe
  int currentTrackIndex = 0;
  TimelineTrack *targetTrack = nullptr;
  Keyframe *targetKeyframe = nullptr;

  for (auto it = m_tracks.begin(); it != m_tracks.end();
       ++it, ++currentTrackIndex) {
    if (currentTrackIndex == trackIndex) {
      targetTrack = it.value();
      targetKeyframe = targetTrack->getKeyframe(frame);
      break;
    }
  }

  if (!targetTrack || !targetKeyframe)
    return;

  // Create easing selection dialog
  QDialog dialog(this);
  dialog.setWindowTitle("Select Easing Type");
  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QListWidget *easingList = new QListWidget(&dialog);
  easingList->addItem("Linear");
  easingList->addItem("Ease In");
  easingList->addItem("Ease Out");
  easingList->addItem("Ease In Out");
  easingList->addItem("Ease In Quad");
  easingList->addItem("Ease Out Quad");
  easingList->addItem("Ease In Out Quad");
  easingList->addItem("Ease In Cubic");
  easingList->addItem("Ease Out Cubic");
  easingList->addItem("Ease In Out Cubic");
  easingList->addItem("Ease In Elastic");
  easingList->addItem("Ease Out Elastic");
  easingList->addItem("Ease In Bounce");
  easingList->addItem("Ease Out Bounce");
  easingList->addItem("Step");
  easingList->addItem("Custom");

  // Select current easing
  easingList->setCurrentRow(static_cast<int>(targetKeyframe->easing));

  layout->addWidget(easingList);

  QDialogButtonBox *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  layout->addWidget(buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    int selectedIndex = easingList->currentRow();
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(EasingType::Custom) + 1) {
      // Create undo command for easing change
      int oldEasing = static_cast<int>(targetKeyframe->easing);
      int newEasing = selectedIndex;

      if (oldEasing != newEasing) {
        auto *cmd = new ChangeKeyframeEasingCommand(
            this, targetTrack->name, frame, oldEasing, newEasing);
        NMUndoManager::instance().pushCommand(cmd);
      }

      emit keyframeEasingChanged(targetTrack->name, frame,
                                 static_cast<EasingType>(newEasing));
    }
  }
}

// =============================================================================
// Delete Selected Keyframes
// =============================================================================

void NMTimelinePanel::deleteSelectedKeyframes() {
  if (m_selectedKeyframes.isEmpty())
    return;

  // Create macro command for deleting multiple keyframes
  if (m_selectedKeyframes.size() > 1) {
    NMUndoManager::instance().beginMacro("Delete Keyframes");
  }

  // Delete keyframes from data model
  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end();
       ++trackIt, ++trackIndex) {
    TimelineTrack *track = trackIt.value();

    // Collect frames to delete for this track
    QVector<int> framesToDelete;
    for (const KeyframeId &id : m_selectedKeyframes) {
      if (id.trackIndex == trackIndex) {
        framesToDelete.append(id.frame);
      }
    }

    // Delete the keyframes with undo support
    for (int frame : framesToDelete) {
      // Capture keyframe state before deleting
      if (auto *kf = track->getKeyframe(frame)) {
        KeyframeSnapshot snapshot;
        snapshot.frame = frame;
        snapshot.value = kf->value;
        snapshot.easingType = static_cast<int>(kf->easing);
        snapshot.handleInX = kf->handleInX;
        snapshot.handleInY = kf->handleInY;
        snapshot.handleOutX = kf->handleOutX;
        snapshot.handleOutY = kf->handleOutY;

        // Create and push delete command
        auto *cmd = new DeleteKeyframeCommand(this, track->name, snapshot);
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

// =============================================================================
// Event Filter for Keyboard
// =============================================================================

bool NMTimelinePanel::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if (keyEvent->key() == Qt::Key_Delete ||
        keyEvent->key() == Qt::Key_Backspace) {
      deleteSelectedKeyframes();
      return true;
    }
  }

  return NMDockPanel::eventFilter(obj, event);
}

// =============================================================================
// Track Access Methods
// =============================================================================

TimelineTrack *NMTimelinePanel::getTrack(const QString &name) const {
  QMutexLocker locker(&m_tracksMutex);
  auto it = m_tracks.find(name);
  if (it != m_tracks.end()) {
    return it.value();
  }
  return nullptr;
}

// =============================================================================
// Render Cache and Performance Metrics
// =============================================================================

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
  PerformanceMetrics::instance().recordTiming(
      PerformanceMetrics::METRIC_RENDER_TRACKS, renderTimeMs);
  PerformanceMetrics::instance().recordCount(
      PerformanceMetrics::METRIC_SCENE_ITEMS, itemCount);

  // Report cache stats if enabled
  if (m_renderCache) {
    auto stats = m_renderCache->getStats();
    PerformanceMetrics::instance().recordCount(
        PerformanceMetrics::METRIC_TIMELINE_CACHE_HIT,
        static_cast<int>(stats.hitRate() * 100));
  }
}

QStringList NMTimelinePanel::getTrackNamesSafe() const {
  QMutexLocker locker(&m_tracksMutex);
  return m_tracks.keys();
}

} // namespace NovelMind::editor::qt
