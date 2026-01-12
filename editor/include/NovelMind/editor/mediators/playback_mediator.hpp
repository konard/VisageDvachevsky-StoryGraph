#pragma once

/**
 * @file playback_mediator.hpp
 * @brief Mediator for coordinating playback/animation across panels
 *
 * The PlaybackMediator handles:
 * - Timeline ↔ SceneView animation synchronization
 * - Animation preview mode coordination
 */

#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include <QObject>
#include <vector>

namespace NovelMind::editor::qt {
class NMSceneViewPanel;
class NMTimelinePanel;
} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for playback/animation coordination
 */
class PlaybackMediator : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the playback mediator
   */
  PlaybackMediator(qt::NMSceneViewPanel* sceneView, qt::NMTimelinePanel* timeline,
                   QObject* parent = nullptr);

  ~PlaybackMediator() override;

  /**
   * @brief Initialize event subscriptions
   */
  void initialize();

  /**
   * @brief Shutdown and cleanup subscriptions
   */
  void shutdown();

private:
  void onTimelineFrameChanged(const events::TimelineFrameChangedEvent& event);
  void onTimelinePlaybackStateChanged(const events::TimelinePlaybackStateChangedEvent& event);
  void onSetAnimationPreviewMode(const events::SetAnimationPreviewModeEvent& event);

  qt::NMSceneViewPanel* m_sceneView = nullptr;
  qt::NMTimelinePanel* m_timeline = nullptr;

  std::vector<EventSubscription> m_subscriptions;
};

} // namespace NovelMind::editor::mediators
