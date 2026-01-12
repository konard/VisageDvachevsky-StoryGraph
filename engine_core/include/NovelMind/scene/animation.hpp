#pragma once

/**
 * @file animation.hpp
 * @brief Unified Animation/Tween framework for scene objects
 *
 * This module provides a comprehensive tweening and animation system
 * for animating properties of scene objects (position, scale, alpha, etc.).
 *
 * Features:
 * - Easing functions (linear, ease-in, ease-out, ease-in-out, etc.)
 * - Tween types (position, scale, rotation, alpha, color)
 * - Animation timeline for chaining/parallel animations
 * - Callbacks for completion events
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::scene {

/**
 * @brief Easing function types for smooth animations
 */
enum class EaseType : u8 {
  Linear,          // No easing
  EaseInQuad,      // Quadratic ease in
  EaseOutQuad,     // Quadratic ease out
  EaseInOutQuad,   // Quadratic ease in-out
  EaseInCubic,     // Cubic ease in
  EaseOutCubic,    // Cubic ease out
  EaseInOutCubic,  // Cubic ease in-out
  EaseInSine,      // Sinusoidal ease in
  EaseOutSine,     // Sinusoidal ease out
  EaseInOutSine,   // Sinusoidal ease in-out
  EaseInExpo,      // Exponential ease in
  EaseOutExpo,     // Exponential ease out
  EaseInOutExpo,   // Exponential ease in-out
  EaseInBack,      // Back ease in (overshoot)
  EaseOutBack,     // Back ease out (overshoot)
  EaseInOutBack,   // Back ease in-out
  EaseInBounce,    // Bounce ease in
  EaseOutBounce,   // Bounce ease out
  EaseInOutBounce, // Bounce ease in-out
  EaseInElastic,   // Elastic ease in
  EaseOutElastic,  // Elastic ease out
  EaseInOutElastic // Elastic ease in-out
};

/**
 * @brief Calculate easing value for given progress
 * @param type The easing function to use
 * @param t Progress from 0.0 to 1.0
 * @return Eased value from 0.0 to 1.0
 */
[[nodiscard]] inline f32 ease(EaseType type, f32 t) {
  // Clamp t to [0, 1]
  t = std::max(0.0f, std::min(1.0f, t));

  constexpr f32 PI = 3.14159265358979323846f;
  constexpr f32 c1 = 1.70158f;
  constexpr f32 c2 = c1 * 1.525f;
  constexpr f32 c3 = c1 + 1.0f;
  constexpr f32 c4 = (2.0f * PI) / 3.0f;
  constexpr f32 c5 = (2.0f * PI) / 4.5f;

  switch (type) {
  case EaseType::Linear:
    return t;

  case EaseType::EaseInQuad:
    return t * t;

  case EaseType::EaseOutQuad:
    return 1.0f - (1.0f - t) * (1.0f - t);

  case EaseType::EaseInOutQuad:
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

  case EaseType::EaseInCubic:
    return t * t * t;

  case EaseType::EaseOutCubic:
    return 1.0f - std::pow(1.0f - t, 3.0f);

  case EaseType::EaseInOutCubic:
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

  case EaseType::EaseInSine:
    return 1.0f - std::cos((t * PI) / 2.0f);

  case EaseType::EaseOutSine:
    return std::sin((t * PI) / 2.0f);

  case EaseType::EaseInOutSine:
    return -(std::cos(PI * t) - 1.0f) / 2.0f;

  case EaseType::EaseInExpo:
    return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);

  case EaseType::EaseOutExpo:
    return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);

  case EaseType::EaseInOutExpo:
    if (t == 0.0f)
      return 0.0f;
    if (t == 1.0f)
      return 1.0f;
    return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
                    : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;

  case EaseType::EaseInBack:
    return c3 * t * t * t - c1 * t * t;

  case EaseType::EaseOutBack: {
    f32 t1 = t - 1.0f;
    return 1.0f + c3 * t1 * t1 * t1 + c1 * t1 * t1;
  }

  case EaseType::EaseInOutBack:
    return t < 0.5f
               ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
               : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) /
                     2.0f;

  case EaseType::EaseOutBounce: {
    constexpr f32 n1 = 7.5625f;
    constexpr f32 d1 = 2.75f;
    if (t < 1.0f / d1) {
      return n1 * t * t;
    } else if (t < 2.0f / d1) {
      t -= 1.5f / d1;
      return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
      t -= 2.25f / d1;
      return n1 * t * t + 0.9375f;
    } else {
      t -= 2.625f / d1;
      return n1 * t * t + 0.984375f;
    }
  }

  case EaseType::EaseInBounce:
    return 1.0f - ease(EaseType::EaseOutBounce, 1.0f - t);

  case EaseType::EaseInOutBounce:
    return t < 0.5f ? (1.0f - ease(EaseType::EaseOutBounce, 1.0f - 2.0f * t)) / 2.0f
                    : (1.0f + ease(EaseType::EaseOutBounce, 2.0f * t - 1.0f)) / 2.0f;

  case EaseType::EaseInElastic:
    if (t == 0.0f)
      return 0.0f;
    if (t == 1.0f)
      return 1.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);

  case EaseType::EaseOutElastic:
    if (t == 0.0f)
      return 0.0f;
    if (t == 1.0f)
      return 1.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;

  case EaseType::EaseInOutElastic:
    if (t == 0.0f)
      return 0.0f;
    if (t == 1.0f)
      return 1.0f;
    return t < 0.5f
               ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f
               : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) /
                         2.0f +
                     1.0f;
  }

  return t;
}

/**
 * @brief Animation state
 */
enum class AnimationState : u8 {
  Idle,     // Not started
  Running,  // Currently animating
  Paused,   // Paused mid-animation
  Completed // Animation finished
};

/**
 * @brief Base class for all tween animations
 */
class Tween {
public:
  using CompletionCallback = std::function<void()>;

  Tween(f32 duration, EaseType easing = EaseType::Linear)
      : m_duration(duration), m_elapsed(0.0f), m_easing(easing), m_state(AnimationState::Idle),
        m_loops(1), m_currentLoop(0), m_yoyo(false), m_forward(true) {}

  virtual ~Tween() = default;

  /**
   * @brief Start the animation
   */
  virtual void start() {
    m_state = AnimationState::Running;
    m_elapsed = 0.0f;
    m_currentLoop = 0;
    m_forward = true;
  }

  /**
   * @brief Pause the animation
   */
  void pause() {
    if (m_state == AnimationState::Running) {
      m_state = AnimationState::Paused;
    }
  }

  /**
   * @brief Resume a paused animation
   */
  void resume() {
    if (m_state == AnimationState::Paused) {
      m_state = AnimationState::Running;
    }
  }

  /**
   * @brief Stop the animation
   */
  void stop() { m_state = AnimationState::Completed; }

  /**
   * @brief Reset the animation to the beginning
   */
  void reset() {
    m_elapsed = 0.0f;
    m_currentLoop = 0;
    m_forward = true;
    m_state = AnimationState::Idle;
  }

  /**
   * @brief Update the animation
   * @param deltaTime Time since last update in seconds
   * @return true if still running, false if completed
   */
  bool update(f64 deltaTime) {
    if (m_state != AnimationState::Running) {
      return m_state != AnimationState::Completed;
    }

    m_elapsed += static_cast<f32>(deltaTime);

    f32 t = m_elapsed / m_duration;

    if (t >= 1.0f) {
      t = 1.0f;
      m_elapsed = 0.0f;

      if (m_yoyo) {
        m_forward = !m_forward;
      }

      ++m_currentLoop;

      if (m_loops > 0 && m_currentLoop >= m_loops) {
        // Animation complete
        applyProgress(m_forward ? 1.0f : 0.0f);
        m_state = AnimationState::Completed;

        if (m_onComplete) {
          m_onComplete();
        }

        return false;
      }
    }

    // Apply easing and update
    f32 progress = m_forward ? t : (1.0f - t);
    f32 easedProgress = ease(m_easing, progress);
    applyProgress(easedProgress);

    return true;
  }

  /**
   * @brief Set number of loops (0 = infinite)
   */
  Tween& setLoops(i32 loops) {
    m_loops = loops;
    return *this;
  }

  /**
   * @brief Enable yoyo mode (reverse after each loop)
   */
  Tween& setYoyo(bool yoyo) {
    m_yoyo = yoyo;
    return *this;
  }

  /**
   * @brief Set completion callback
   */
  Tween& onComplete(CompletionCallback callback) {
    m_onComplete = std::move(callback);
    return *this;
  }

  /**
   * @brief Check if animation is complete
   */
  [[nodiscard]] bool isComplete() const { return m_state == AnimationState::Completed; }

  /**
   * @brief Check if animation is running
   */
  [[nodiscard]] bool isRunning() const { return m_state == AnimationState::Running; }

  /**
   * @brief Get animation state
   */
  [[nodiscard]] AnimationState getState() const { return m_state; }

  /**
   * @brief Get current progress (0.0 to 1.0)
   */
  [[nodiscard]] f32 getProgress() const {
    return m_duration > 0.0f ? m_elapsed / m_duration : 1.0f;
  }

protected:
  /**
   * @brief Apply animation progress to the target
   * @param progress Eased progress value from 0.0 to 1.0
   */
  virtual void applyProgress(f32 progress) = 0;

  f32 m_duration;
  f32 m_elapsed;
  EaseType m_easing;
  AnimationState m_state;
  i32 m_loops;
  i32 m_currentLoop;
  bool m_yoyo;
  bool m_forward;
  CompletionCallback m_onComplete;
};

/**
 * @brief Tween for animating a single float value
 */
class FloatTween : public Tween {
public:
  FloatTween(f32* target, f32 from, f32 to, f32 duration, EaseType easing = EaseType::Linear)
      : Tween(duration, easing), m_target(target), m_from(from), m_to(to) {}

  void start() override {
    Tween::start();
    if (m_target) {
      *m_target = m_from;
    }
  }

protected:
  void applyProgress(f32 progress) override {
    if (m_target) {
      *m_target = m_from + (m_to - m_from) * progress;
    }
  }

private:
  f32* m_target;
  f32 m_from;
  f32 m_to;
};

/**
 * @brief Tween for animating a 2D position
 */
class PositionTween : public Tween {
public:
  PositionTween(f32* targetX, f32* targetY, f32 fromX, f32 fromY, f32 toX, f32 toY, f32 duration,
                EaseType easing = EaseType::Linear)
      : Tween(duration, easing), m_targetX(targetX), m_targetY(targetY), m_fromX(fromX),
        m_fromY(fromY), m_toX(toX), m_toY(toY) {}

  void start() override {
    Tween::start();
    if (m_targetX)
      *m_targetX = m_fromX;
    if (m_targetY)
      *m_targetY = m_fromY;
  }

protected:
  void applyProgress(f32 progress) override {
    if (m_targetX)
      *m_targetX = m_fromX + (m_toX - m_fromX) * progress;
    if (m_targetY)
      *m_targetY = m_fromY + (m_toY - m_fromY) * progress;
  }

private:
  f32* m_targetX;
  f32* m_targetY;
  f32 m_fromX, m_fromY;
  f32 m_toX, m_toY;
};

/**
 * @brief Tween for animating color (RGBA)
 */
class ColorTween : public Tween {
public:
  ColorTween(renderer::Color* target, const renderer::Color& from, const renderer::Color& to,
             f32 duration, EaseType easing = EaseType::Linear)
      : Tween(duration, easing), m_target(target), m_from(from), m_to(to) {}

  void start() override {
    Tween::start();
    if (m_target)
      *m_target = m_from;
  }

protected:
  void applyProgress(f32 progress) override {
    if (m_target) {
      m_target->r = static_cast<u8>(m_from.r + (m_to.r - m_from.r) * progress);
      m_target->g = static_cast<u8>(m_from.g + (m_to.g - m_from.g) * progress);
      m_target->b = static_cast<u8>(m_from.b + (m_to.b - m_from.b) * progress);
      m_target->a = static_cast<u8>(m_from.a + (m_to.a - m_from.a) * progress);
    }
  }

private:
  renderer::Color* m_target;
  renderer::Color m_from;
  renderer::Color m_to;
};

/**
 * @brief Tween with callback for custom animations
 */
class CallbackTween : public Tween {
public:
  using UpdateCallback = std::function<void(f32)>;

  CallbackTween(UpdateCallback callback, f32 duration, EaseType easing = EaseType::Linear)
      : Tween(duration, easing), m_callback(std::move(callback)) {}

protected:
  void applyProgress(f32 progress) override {
    if (m_callback) {
      m_callback(progress);
    }
  }

private:
  UpdateCallback m_callback;
};

/**
 * @brief Animation timeline for sequencing and grouping tweens
 */
class AnimationTimeline {
public:
  AnimationTimeline() = default;

  /**
   * @brief Add a tween to run in sequence
   */
  AnimationTimeline& append(std::unique_ptr<Tween> tween) {
    if (tween) {
      m_sequence.push_back(std::move(tween));
    }
    return *this;
  }

  /**
   * @brief Add a tween to run in parallel with the last one
   */
  AnimationTimeline& join(std::unique_ptr<Tween> tween) {
    if (tween) {
      m_parallel.push_back(std::move(tween));
    }
    return *this;
  }

  /**
   * @brief Add a delay before the next tween
   */
  AnimationTimeline& delay(f32 seconds) {
    auto delayTween = std::make_unique<CallbackTween>([](f32) {}, seconds);
    m_sequence.push_back(std::move(delayTween));
    return *this;
  }

  /**
   * @brief Start the timeline
   */
  void start() {
    m_currentIndex = 0;
    m_running = true;
    startCurrentGroup();
  }

  /**
   * @brief Update the timeline
   */
  bool update(f64 deltaTime) {
    if (!m_running) {
      return false;
    }

    // Update parallel animations
    bool allParallelComplete = true;
    for (auto& tween : m_activeParallel) {
      if (tween && tween->update(deltaTime)) {
        allParallelComplete = false;
      }
    }

    // Update current sequence tween
    if (m_currentIndex < m_sequence.size()) {
      auto& current = m_sequence[m_currentIndex];
      if (current && !current->update(deltaTime)) {
        // Current tween complete, move to next
        ++m_currentIndex;
        startCurrentGroup();
      }
      return true;
    }

    // All sequence done, wait for parallel to finish
    if (!allParallelComplete) {
      return true;
    }

    m_running = false;
    if (m_onComplete) {
      m_onComplete();
    }
    return false;
  }

  /**
   * @brief Stop the timeline
   */
  void stop() { m_running = false; }

  /**
   * @brief Check if running
   */
  [[nodiscard]] bool isRunning() const { return m_running; }

  /**
   * @brief Set completion callback
   */
  AnimationTimeline& onComplete(std::function<void()> callback) {
    m_onComplete = std::move(callback);
    return *this;
  }

private:
  void startCurrentGroup() {
    if (m_currentIndex < m_sequence.size()) {
      auto& tween = m_sequence[m_currentIndex];
      if (tween) {
        tween->start();
      }
    }

    // Start all parallel tweens
    m_activeParallel = std::move(m_parallel);
    m_parallel.clear();
    for (auto& tween : m_activeParallel) {
      if (tween) {
        tween->start();
      }
    }
  }

  std::vector<std::unique_ptr<Tween>> m_sequence;
  std::vector<std::unique_ptr<Tween>> m_parallel;
  std::vector<std::unique_ptr<Tween>> m_activeParallel;
  size_t m_currentIndex = 0;
  bool m_running = false;
  std::function<void()> m_onComplete;
};

/**
 * @brief Animation manager for tracking active animations
 */
class AnimationManager {
public:
  /**
   * @brief Add a tween to be managed
   */
  void add(const std::string& id, std::unique_ptr<Tween> tween) {
    if (tween) {
      tween->start();
      m_tweens[id] = std::move(tween);
    }
  }

  /**
   * @brief Add a timeline to be managed
   */
  void add(const std::string& id, std::unique_ptr<AnimationTimeline> timeline) {
    if (timeline) {
      timeline->start();
      m_timelines[id] = std::move(timeline);
    }
  }

  /**
   * @brief Update all active animations
   */
  void update(f64 deltaTime) {
    // Update tweens and remove completed ones
    for (auto it = m_tweens.begin(); it != m_tweens.end();) {
      if (it->second && it->second->isComplete()) {
        it = m_tweens.erase(it);
      } else {
        if (it->second) {
          it->second->update(deltaTime);
        }
        ++it;
      }
    }

    // Update timelines and remove completed ones
    for (auto it = m_timelines.begin(); it != m_timelines.end();) {
      if (it->second && !it->second->isRunning()) {
        it = m_timelines.erase(it);
      } else {
        if (it->second) {
          it->second->update(deltaTime);
        }
        ++it;
      }
    }
  }

  /**
   * @brief Stop and remove an animation by ID
   */
  void stop(const std::string& id) {
    auto tweenIt = m_tweens.find(id);
    if (tweenIt != m_tweens.end()) {
      if (tweenIt->second) {
        tweenIt->second->stop();
      }
      m_tweens.erase(tweenIt);
      return;
    }

    auto timelineIt = m_timelines.find(id);
    if (timelineIt != m_timelines.end()) {
      if (timelineIt->second) {
        timelineIt->second->stop();
      }
      m_timelines.erase(timelineIt);
    }
  }

  /**
   * @brief Stop all animations
   */
  void stopAll() {
    m_tweens.clear();
    m_timelines.clear();
  }

  /**
   * @brief Check if an animation exists
   */
  [[nodiscard]] bool has(const std::string& id) const {
    return m_tweens.find(id) != m_tweens.end() || m_timelines.find(id) != m_timelines.end();
  }

  /**
   * @brief Get number of active animations
   */
  [[nodiscard]] size_t count() const { return m_tweens.size() + m_timelines.size(); }

private:
  std::unordered_map<std::string, std::unique_ptr<Tween>> m_tweens;
  std::unordered_map<std::string, std::unique_ptr<AnimationTimeline>> m_timelines;
};

} // namespace NovelMind::scene
