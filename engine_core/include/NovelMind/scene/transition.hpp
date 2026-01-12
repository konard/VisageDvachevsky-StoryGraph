#pragma once

/**
 * @file transition.hpp
 * @brief Scene transition effects
 *
 * This module provides various transition effects for
 * scene changes in visual novels.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <functional>
#include <memory>

namespace NovelMind::Scene {

/**
 * @brief Transition types available
 */
enum class TransitionType {
  None,
  Fade,
  FadeThrough,
  SlideLeft,
  SlideRight,
  SlideUp,
  SlideDown,
  Dissolve,
  Wipe,
  Zoom
};

/**
 * @brief Base class for scene transitions
 *
 * Transitions interpolate between two states over time,
 * producing visual effects during scene changes.
 */
class ITransition {
public:
  using CompletionCallback = std::function<void()>;

  virtual ~ITransition() = default;

  /**
   * @brief Start the transition
   * @param duration Duration in seconds
   */
  virtual void start(f32 duration) = 0;

  /**
   * @brief Update the transition state
   * @param deltaTime Time since last update
   */
  virtual void update(f64 deltaTime) = 0;

  /**
   * @brief Render the transition effect
   * @param renderer The renderer to use
   */
  virtual void render(renderer::IRenderer& renderer) = 0;

  /**
   * @brief Check if the transition is complete
   */
  [[nodiscard]] virtual bool isComplete() const = 0;

  /**
   * @brief Get the current progress (0.0 to 1.0)
   */
  [[nodiscard]] virtual f32 getProgress() const = 0;

  /**
   * @brief Set callback for when transition completes
   */
  virtual void setOnComplete(CompletionCallback callback) = 0;

  /**
   * @brief Get the transition type
   */
  [[nodiscard]] virtual TransitionType getType() const = 0;
};

/**
 * @brief Fade transition (fade to color, then fade in)
 *
 * Classic visual novel transition that fades the screen
 * to a solid color (usually black or white), then fades in.
 */
class FadeTransition : public ITransition {
public:
  /**
   * @brief Create a fade transition
   * @param fadeColor The color to fade through
   * @param fadeOut If true, fade out; if false, fade in
   */
  explicit FadeTransition(const renderer::Color& fadeColor = renderer::Color::Black,
                          bool fadeOut = true);
  ~FadeTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  /**
   * @brief Set the fade color
   */
  void setFadeColor(const renderer::Color& color);

  /**
   * @brief Set whether this is a fade out or fade in
   */
  void setFadeOut(bool fadeOut);

private:
  renderer::Color m_fadeColor;
  bool m_fadeOut;

  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;

  CompletionCallback m_onComplete;
};

/**
 * @brief Fade through transition (fade out, then fade in)
 *
 * Complete transition that fades out to a color,
 * then fades back in. Useful for full scene changes.
 */
class FadeThroughTransition : public ITransition {
public:
  explicit FadeThroughTransition(const renderer::Color& fadeColor = renderer::Color::Black);
  ~FadeThroughTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  /**
   * @brief Set callback for midpoint (when fully faded)
   * This is where you would swap the scene content
   */
  void setOnMidpoint(CompletionCallback callback);

  /**
   * @brief Check if past midpoint
   */
  [[nodiscard]] bool isPastMidpoint() const;

private:
  renderer::Color m_fadeColor;

  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;
  bool m_pastMidpoint;

  CompletionCallback m_onComplete;
  CompletionCallback m_onMidpoint;
};

/**
 * @brief Slide transition (slide in from a direction)
 */
class SlideTransition : public ITransition {
public:
  enum class Direction { Left, Right, Up, Down };

  explicit SlideTransition(Direction direction = Direction::Left);
  ~SlideTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  /**
   * @brief Set the slide direction
   */
  void setDirection(Direction direction);

  /**
   * @brief Get the current offset for positioning elements
   */
  [[nodiscard]] f32 getOffset() const;

private:
  Direction m_direction;

  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;
  f32 m_offset;
  f32 m_screenSize; // Width or height depending on direction

  CompletionCallback m_onComplete;
};

/**
 * @brief Dissolve transition (pixelated crossfade)
 *
 * Creates a dissolve effect where pixels transition
 * randomly from old to new content.
 */
class DissolveTransition : public ITransition {
public:
  DissolveTransition();
  ~DissolveTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  /**
   * @brief Get the dissolve alpha for blending
   */
  [[nodiscard]] f32 getDissolveAlpha() const;

private:
  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;

  CompletionCallback m_onComplete;
};

/**
 * @brief Wipe transition (reveal using a sliding mask)
 */
class WipeTransition : public ITransition {
public:
  enum class Direction { LeftToRight, RightToLeft, TopToBottom, BottomToTop };

  explicit WipeTransition(const renderer::Color& maskColor = renderer::Color::Black,
                          Direction direction = Direction::LeftToRight);
  ~WipeTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  void setDirection(Direction direction);

private:
  renderer::Color m_maskColor;
  Direction m_direction;
  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;
  CompletionCallback m_onComplete;
};

/**
 * @brief Zoom transition (center zoom mask)
 */
class ZoomTransition : public ITransition {
public:
  explicit ZoomTransition(const renderer::Color& maskColor = renderer::Color::Black,
                          bool zoomIn = true);
  ~ZoomTransition() override;

  void start(f32 duration) override;
  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;

  [[nodiscard]] bool isComplete() const override;
  [[nodiscard]] f32 getProgress() const override;
  void setOnComplete(CompletionCallback callback) override;
  [[nodiscard]] TransitionType getType() const override;

  void setZoomIn(bool zoomIn);

private:
  renderer::Color m_maskColor;
  bool m_zoomIn;
  f32 m_duration;
  f32 m_elapsed;
  bool m_running;
  bool m_complete;
  CompletionCallback m_onComplete;
};

/**
 * @brief Factory function to create transitions
 */
std::unique_ptr<ITransition>
createTransition(TransitionType type, const renderer::Color& color = renderer::Color::Black);

/**
 * @brief Parse transition type from string
 */
TransitionType parseTransitionType(const std::string& name);

/**
 * @brief Get transition type name as string
 */
const char* transitionTypeName(TransitionType type);

} // namespace NovelMind::Scene
