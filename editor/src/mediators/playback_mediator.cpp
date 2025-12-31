#include "NovelMind/editor/mediators/playback_mediator.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"

#include <QDebug>

namespace NovelMind::editor::mediators {

PlaybackMediator::PlaybackMediator(qt::NMSceneViewPanel *sceneView,
                                   qt::NMTimelinePanel *timeline,
                                   QObject *parent)
    : QObject(parent), m_sceneView(sceneView), m_timeline(timeline) {}

PlaybackMediator::~PlaybackMediator() { shutdown(); }

void PlaybackMediator::initialize() {
  auto &bus = EventBus::instance();

  // Timeline frame changes
  m_subscriptions.push_back(bus.subscribe<events::TimelineFrameChangedEvent>(
      [this](const events::TimelineFrameChangedEvent &event) {
        onTimelineFrameChanged(event);
      }));

  // Timeline playback state changes
  m_subscriptions.push_back(
      bus.subscribe<events::TimelinePlaybackStateChangedEvent>(
          [this](const events::TimelinePlaybackStateChangedEvent &event) {
            onTimelinePlaybackStateChanged(event);
          }));

  // Animation preview mode
  m_subscriptions.push_back(bus.subscribe<events::SetAnimationPreviewModeEvent>(
      [this](const events::SetAnimationPreviewModeEvent &event) {
        onSetAnimationPreviewMode(event);
      }));

  qDebug() << "[PlaybackMediator] Initialized with"
           << m_subscriptions.size() << "subscriptions";
}

void PlaybackMediator::shutdown() {
  auto &bus = EventBus::instance();
  for (const auto &sub : m_subscriptions) {
    bus.unsubscribe(sub);
  }
  m_subscriptions.clear();
  qDebug() << "[PlaybackMediator] Shutdown complete";
}

void PlaybackMediator::onTimelineFrameChanged(
    const events::TimelineFrameChangedEvent &event) {
  if (!m_sceneView || !m_sceneView->isAnimationPreviewMode()) {
    return;
  }

  // Update scene view with the current frame
  // Animation adapter would apply interpolated values to scene objects
  // For now, we trigger a viewport update
  if (auto *view = m_sceneView->graphicsView()) {
    view->viewport()->update();
  }

  qDebug() << "[PlaybackMediator] Timeline frame changed to:" << event.frame;
}

void PlaybackMediator::onTimelinePlaybackStateChanged(
    const events::TimelinePlaybackStateChangedEvent &event) {
  qDebug() << "[PlaybackMediator] Timeline playback state:"
           << (event.playing ? "playing" : "stopped");

  // Publish status message
  events::StatusMessageEvent statusEvent;
  if (event.playing) {
    statusEvent.message = QObject::tr("Animation preview playing...");
    statusEvent.timeoutMs = 0;
  } else {
    statusEvent.message = QObject::tr("Animation preview stopped");
    statusEvent.timeoutMs = 2000;
  }
  EventBus::instance().publish(statusEvent);
}

void PlaybackMediator::onSetAnimationPreviewMode(
    const events::SetAnimationPreviewModeEvent &event) {
  if (m_sceneView) {
    m_sceneView->setAnimationPreviewMode(event.enabled);
    qDebug() << "[PlaybackMediator] Animation preview mode:"
             << (event.enabled ? "enabled" : "disabled");
  }
}

} // namespace NovelMind::editor::mediators
