/**
 * @file nm_timeline_panel.cpp
 * @brief Timeline editor panel - Core implementation
 *
 * This file has been refactored to improve maintainability by splitting
 * functionality into focused modules:
 * - timeline_renderer.cpp: Rendering logic
 * - timeline_keyframes.cpp: Keyframe handling
 * - timeline_interaction.cpp: User interaction
 * - timeline_viewport.cpp: Viewport management
 *
 * This file now contains only the core panel initialization and UI setup.
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_bezier_curve_editor_dialog.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_keyframe_item.hpp"
#include "NovelMind/editor/qt/performance_metrics.hpp"

#include <QAction>
#include <QBrush>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMutexLocker>
#include <QPen>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// =============================================================================
// NMTimelinePanel Core Implementation
// =============================================================================

NMTimelinePanel::NMTimelinePanel(QWidget* parent) : NMDockPanel("Timeline", parent) {
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
  // Clean up tracks (protected by mutex for thread safety)
  QMutexLocker locker(&m_tracksMutex);
  qDeleteAll(m_tracks);
  m_tracks.clear();
}

void NMTimelinePanel::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget());
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

  auto& iconMgr = NMIconManager::instance();

  // Playback controls
  m_btnPlay = new QPushButton(m_toolbar);
  m_btnPlay->setIcon(iconMgr.getIcon("play", 16));
  m_btnPlay->setCheckable(true);
  m_btnPlay->setToolTip("Play/Pause (Space)");
  m_btnPlay->setFlat(true);
  connect(m_btnPlay, &QPushButton::clicked, this, &NMTimelinePanel::togglePlayback);

  m_btnStop = new QPushButton(m_toolbar);
  m_btnStop->setIcon(iconMgr.getIcon("stop", 16));
  m_btnStop->setToolTip("Stop");
  m_btnStop->setFlat(true);
  connect(m_btnStop, &QPushButton::clicked, this, &NMTimelinePanel::stopPlayback);

  m_btnStepBack = new QPushButton(m_toolbar);
  m_btnStepBack->setIcon(iconMgr.getIcon("step-backward", 16));
  m_btnStepBack->setToolTip("Step Backward");
  m_btnStepBack->setFlat(true);
  connect(m_btnStepBack, &QPushButton::clicked, this, &NMTimelinePanel::stepBackward);

  m_btnStepForward = new QPushButton(m_toolbar);
  m_btnStepForward->setIcon(iconMgr.getIcon("step-forward", 16));
  m_btnStepForward->setToolTip("Step Forward");
  m_btnStepForward->setFlat(true);
  connect(m_btnStepForward, &QPushButton::clicked, this, &NMTimelinePanel::stepForward);

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
  connect(m_btnZoomFit, &QPushButton::clicked, this, &NMTimelinePanel::zoomToFit);

  // Snap to grid action
  m_snapToGridAction = new QAction("Snap to Grid", m_toolbar);
  m_snapToGridAction->setCheckable(true);
  m_snapToGridAction->setChecked(m_snapToGrid);
  m_snapToGridAction->setShortcut(QKeySequence("Ctrl+G"));
  m_snapToGridAction->setToolTip("Snap to Grid (Ctrl+G)");
  connect(m_snapToGridAction, &QAction::toggled, this, &NMTimelinePanel::setSnapToGrid);

  // Grid interval combo
  m_gridIntervalCombo = new QComboBox(m_toolbar);
  m_gridIntervalCombo->addItem("1 frame", 1);
  m_gridIntervalCombo->addItem("5 frames", 5);
  m_gridIntervalCombo->addItem("10 frames", 10);
  m_gridIntervalCombo->addItem("30 frames", 30);
  m_gridIntervalCombo->setCurrentIndex(1); // Default to 5 frames
  m_gridIntervalCombo->setToolTip("Grid Interval");
  connect(m_gridIntervalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int /*index*/) {
            int interval = m_gridIntervalCombo->currentData().toInt();
            setGridSize(interval);
          });

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
  m_toolbar->addSeparator();
  m_toolbar->addAction(m_snapToGridAction);
  m_toolbar->addWidget(new QLabel("Grid:", m_toolbar));
  m_toolbar->addWidget(m_gridIntervalCombo);
}

void NMTimelinePanel::setupPlaybackControls() {
  // Additional playback controls can be added here
}

void NMTimelinePanel::setupTrackView() {
  m_timelineScene = new QGraphicsScene(this);
  m_timelineView = new QGraphicsView(m_timelineScene, contentWidget());
  m_timelineView->setObjectName("TimelineView");
  m_timelineView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_timelineView->setDragMode(QGraphicsView::NoDrag);
  m_timelineView->setFocusPolicy(Qt::StrongFocus);

  // Install event filter for keyboard and mouse handling
  m_timelineView->installEventFilter(this);
  m_timelineView->viewport()->installEventFilter(this);

  // Create playhead
  m_playheadItem = m_timelineScene->addLine(TRACK_HEADER_WIDTH, 0, TRACK_HEADER_WIDTH, 1000,
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

} // namespace NovelMind::editor::qt
