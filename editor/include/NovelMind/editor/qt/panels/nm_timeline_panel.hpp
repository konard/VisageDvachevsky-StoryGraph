#pragma once

/**
 * @file nm_timeline_panel.hpp
 * @brief Timeline editor for keyframe-based animations and events
 *
 * Provides:
 * - Multiple tracks (audio, animation, events)
 * - Keyframe editing with handles (CRUD operations)
 * - Playback controls
 * - Frame-accurate scrubbing
 * - Grid snapping and easing selection
 * - Working Curve Editor
 * - Synchronization with Play-In-Editor
 * - Track grouping and filtering
 * - Render caching with memory limits and LRU eviction
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/timeline_render_cache.hpp"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QToolBar>
#include <QUndoStack>
#include <QVector>
#include <QWidget>

#include "NovelMind/editor/qt/panels/nm_keyframe_item.hpp"
#include <QMutex>
#include <atomic>
#include <memory>

class QToolBar;
class QPushButton;
class QSlider;
class QLabel;
class QSpinBox;
class QComboBox;
class QGraphicsItem;
class QCheckBox;
class QAction;
class QGraphicsRectItem;

namespace NovelMind::editor::qt {

/**
 * @brief Types of timeline tracks
 */
enum class TimelineTrackType {
  Audio,
  Animation,
  Event,
  Camera,
  Character,
  Effect,
  Dialogue,
  Variable
};

/**
 * @brief Easing function types
 */
enum class EasingType {
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  EaseInQuad,
  EaseOutQuad,
  EaseInOutQuad,
  EaseInCubic,
  EaseOutCubic,
  EaseInOutCubic,
  EaseInElastic,
  EaseOutElastic,
  EaseInBounce,
  EaseOutBounce,
  Step,
  Custom // Uses curve data
};

/**
 * @brief Keyframe data structure
 */
struct Keyframe {
  int frame = 0;
  QVariant value;
  EasingType easing = EasingType::Linear;

  // Bezier handle data for custom curves
  float handleInX = 0.0f;
  float handleInY = 0.0f;
  float handleOutX = 0.0f;
  float handleOutY = 0.0f;

  bool isSelected = false;
};

/**
 * @brief Timeline track containing keyframes
 */
class TimelineTrack {
public:
  QString id;
  QString name;
  TimelineTrackType type;
  bool visible = true;
  bool locked = false;
  bool collapsed = false;
  QColor color;
  QVector<Keyframe> keyframes;
  QString parentTrackId; // For grouping

  void addKeyframe(int frame, const QVariant &value,
                   EasingType easing = EasingType::Linear);
  void removeKeyframe(int frame);
  void moveKeyframe(int fromFrame, int toFrame);
  Keyframe *getKeyframe(int frame);
  [[nodiscard]] Keyframe interpolate(int frame) const;
  [[nodiscard]] QList<Keyframe *> selectedKeyframes();
  void selectKeyframesInRange(int startFrame, int endFrame);
  void clearSelection();
};

/**
 * @brief Timeline editor panel
 *
 * Professional timeline editor similar to those in Unity, Unreal, After
 * Effects. Supports multiple tracks, keyframe editing, playback, and frame
 * scrubbing.
 */
class NMTimelinePanel : public NMDockPanel {
  Q_OBJECT

public:
  /**
   * @brief Construct timeline panel
   * @param parent Parent widget
   */
  explicit NMTimelinePanel(QWidget *parent = nullptr);

  /**
   * @brief Destructor
   */
  ~NMTimelinePanel() override;

  /**
   * @brief Initialize the panel
   */
  void onInitialize() override;

  /**
   * @brief Shutdown the panel
   */
  void onShutdown() override;

  /**
   * @brief Update panel (called every frame)
   */
  void onUpdate(double deltaTime) override;

protected:
  /**
   * @brief Handle keyboard events for timeline
   */
  bool eventFilter(QObject *obj, QEvent *event) override;

signals:
  /**
   * @brief Emitted when playback frame changes
   */
  void frameChanged(int frame);

  /**
   * @brief Emitted when a keyframe is added/modified
   */
  void keyframeModified(const QString &trackName, int frame);

  /**
   * @brief Emitted when playback state changes
   */
  void playbackStateChanged(bool playing);

signals:
  /**
   * @brief Synchronize with play-in-editor mode
   */
  void syncWithPlayMode(bool enabled);
  void keyframeAdded(const QString &trackName, int frame);
  void keyframeDeleted(const QString &trackName, int frame);
  void keyframeMoved(const QString &trackName, int fromFrame, int toFrame);
  void keyframeEasingChanged(const QString &trackName, int frame,
                             EasingType easing);
  void syncFrameRequested(int frame);

public slots:
  /**
   * @brief Set the current frame
   */
  void setCurrentFrame(int frame);

  /**
   * @brief Play/pause timeline
   */
  void togglePlayback();

  /**
   * @brief Stop playback and return to start
   */
  void stopPlayback();

  /**
   * @brief Step forward one frame
   */
  void stepForward();

  /**
   * @brief Step backward one frame
   */
  void stepBackward();

  /**
   * @brief Jump to next/previous keyframe
   */
  void jumpToNextKeyframe();
  void jumpToPrevKeyframe();

  /**
   * @brief Add a new track
   */
  void addTrack(TimelineTrackType type, const QString &name);

  /**
   * @brief Remove a track
   */
  void removeTrack(const QString &name);

  /**
   * @brief Add keyframe at current frame
   */
  void addKeyframeAtCurrent(const QString &trackName, const QVariant &value);

  /**
   * @brief Delete selected keyframes
   */
  void deleteSelectedKeyframes();

  /**
   * @brief Duplicate selected keyframes
   */
  void duplicateSelectedKeyframes(int offsetFrames = 10);

  /**
   * @brief Set easing for selected keyframes
   */
  void setSelectedKeyframesEasing(EasingType easing);

  /**
   * @brief Copy/paste keyframes
   */
  void copySelectedKeyframes();
  void pasteKeyframes();

  /**
   * @brief Select all keyframes in all tracks
   */
  void selectAllKeyframes();

  /**
   * @brief Zoom timeline view
   */
  void zoomIn();
  void zoomOut();
  void zoomToFit();

  /**
   * @brief Receive frame update from play mode
   */
  void onPlayModeFrameChanged(int frame);

  /**
   * @brief Get/set snap to grid
   */
  void setSnapToGrid(bool enabled);
  [[nodiscard]] bool snapToGrid() const { return m_snapToGrid; }

  /**
   * @brief Get/set grid size in frames
   */
  void setGridSize(int frames);
  [[nodiscard]] int gridSize() const { return m_gridSize; }

  /**
   * @brief Get track by name
   */
  [[nodiscard]] TimelineTrack *getTrack(const QString &name) const;

  /**
   * @brief Get all tracks
   */
  const QMap<QString, TimelineTrack *> &getTracks() const { return m_tracks; }

  /**
   * @brief Get current FPS
   */
  [[nodiscard]] int getFPS() const { return m_fps; }

private:
  void setupUI();
  void setupToolbar();
  void setupPlaybackControls();
  void setupTrackView();

  void updatePlayhead();
  void updateFrameDisplay();
  void renderTracks();

  int frameToX(int frame) const;
  int xToFrame(int x) const;

  // Selection management
  void selectKeyframe(const KeyframeId &id, bool additive);
  void selectKeyframeRange(const KeyframeId &fromId, const KeyframeId &toId);
  void clearSelection();
  void updateSelectionVisuals();
  void selectKeyframesInRect(const QRectF &rect);

  // Keyframe item event handlers
  void onKeyframeClicked(bool additiveSelection, bool rangeSelection,
                         const KeyframeId &id);
  void onKeyframeMoved(int oldFrame, int newFrame, int trackIndex);
  void onKeyframeDoubleClicked(int trackIndex, int frame);
  void onKeyframeDragStarted(const KeyframeId &id);
  void onKeyframeDragEnded();

  // Box selection
  void startBoxSelection(const QPointF &pos);
  void updateBoxSelection(const QPointF &pos);
  void endBoxSelection();

  // Easing dialog
  void showEasingDialog(int trackIndex, int frame);

  // UI Components
  QToolBar *m_toolbar = nullptr;
  QPushButton *m_btnPlay = nullptr;
  QPushButton *m_btnStop = nullptr;
  QPushButton *m_btnStepBack = nullptr;
  QPushButton *m_btnStepForward = nullptr;
  QSpinBox *m_frameSpinBox = nullptr;
  QLabel *m_timeLabel = nullptr;
  QPushButton *m_btnZoomIn = nullptr;
  QPushButton *m_btnZoomOut = nullptr;
  QPushButton *m_btnZoomFit = nullptr;

  // Timeline view
  QGraphicsView *m_timelineView = nullptr;
  QGraphicsScene *m_timelineScene = nullptr;
  QGraphicsLineItem *m_playheadItem = nullptr;

  // State
  QMap<QString, TimelineTrack *> m_tracks;
  mutable QMutex m_tracksMutex; // Protects m_tracks from concurrent access
  int m_currentFrame = 0;
  int m_totalFrames = 300; // 10 seconds at 30fps
  int m_fps = 30;
  bool m_playing = false;
  double m_playbackTime = 0.0;
  float m_zoom = 1.0f;
  int m_pixelsPerFrame = 4;

  // Playback
  bool m_loop = true;
  int m_playbackStartFrame = 0;
  int m_playbackEndFrame = 300;

  // Snapping
  bool m_snapToGrid = true;
  int m_gridSize = 5; // frames

  // Curve editor
  QWidget *m_curveEditor = nullptr;
  bool m_curveEditorVisible = false;

  // Easing selector
  QComboBox *m_easingCombo = nullptr;

  // Play mode sync
  bool m_syncWithPlayMode = false;

  // Clipboard for copy/paste
  struct KeyframeCopy {
    int relativeFrame;
    QVariant value;
    EasingType easing;
  };
  QVector<KeyframeCopy> m_keyframeClipboard;

  // Undo stack
  QUndoStack *m_undoStack = nullptr;

  // Selection state
  QSet<KeyframeId> m_selectedKeyframes;
  QMap<KeyframeId, NMKeyframeItem *> m_keyframeItems;
  KeyframeId m_lastClickedKeyframe;

  // Box selection state
  bool m_isBoxSelecting = false;
  QPointF m_boxSelectStart;
  QPointF m_boxSelectEnd;
  QGraphicsRectItem *m_boxSelectRect = nullptr;

  // Multi-select dragging state
  bool m_isDraggingSelection = false;
  QMap<KeyframeId, int> m_dragStartFrames;

  // Snap to grid toolbar controls
  QAction *m_snapToGridAction = nullptr;
  QComboBox *m_gridIntervalCombo = nullptr;

  static constexpr int TRACK_HEIGHT = 32;
  static constexpr int TRACK_HEADER_WIDTH = 150;
  static constexpr int TIMELINE_MARGIN = 20;

  // Render caching for timeline tracks
  std::unique_ptr<TimelineRenderCache> m_renderCache;
  std::atomic<uint64_t> m_dataVersion{0}; // For cache invalidation

  // Performance tracking
  double m_lastRenderTimeMs = 0.0;
  int m_lastSceneItemCount = 0;

  // PERF-3: Frame label cache to avoid QString allocations during rendering
  // Maps frame number to cached label string
  mutable QHash<int, QString> m_frameLabelCache;
  int m_frameLabelCacheMaxSize = 1024; // Limit cache size

  // Helper methods for cached rendering
  void invalidateRenderCache();
  void invalidateTrackCache(int trackIndex);
  void recordRenderMetrics(double renderTimeMs, int itemCount);

  // PERF-3: Get or create cached frame label
  const QString &getCachedFrameLabel(int frame) const;
};

} // namespace NovelMind::editor::qt
