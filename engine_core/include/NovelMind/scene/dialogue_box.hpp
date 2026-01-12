#pragma once

/**
 * @file dialogue_box.hpp
 * @brief Dialogue box scene object for text display
 *
 * This module provides the DialogueBox class for displaying
 * character dialogue with typewriter effects.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/scene/scene_object.hpp"
#include <functional>
#include <optional>
#include <string>

namespace NovelMind::resource {
class ResourceManager;
} // namespace NovelMind::resource

namespace NovelMind::Scene {

/**
 * @brief Text alignment options
 */
enum class TextAlignment { Left, Center, Right };

/**
 * @brief Dialogue box display style
 */
struct DialogueBoxStyle {
  renderer::Color backgroundColor{0, 0, 0, 180};
  renderer::Color borderColor{255, 255, 255, 255};
  renderer::Color textColor{255, 255, 255, 255};
  renderer::Color nameColor{255, 220, 100, 255};

  f32 paddingLeft{20.0f};
  f32 paddingRight{20.0f};
  f32 paddingTop{15.0f};
  f32 paddingBottom{15.0f};

  f32 borderWidth{2.0f};
  f32 cornerRadius{8.0f};

  f32 namePaddingBottom{8.0f};
  TextAlignment textAlignment{TextAlignment::Left};

  f32 typewriterSpeed{30.0f}; // Characters per second
};

/**
 * @brief Displays dialogue text with typewriter effect
 *
 * DialogueBox handles:
 * - Character name display
 * - Text display with typewriter animation
 * - Multiple text styles
 * - Wait for user input indicator
 * - Auto-advance option
 */
class DialogueBox : public scene::SceneObject {
public:
  using CompletionCallback = std::function<void()>;

  /**
   * @brief Create a dialogue box
   * @param id Unique identifier
   */
  explicit DialogueBox(const std::string& id);
  ~DialogueBox() override;

  /**
   * @brief Set the resource manager for font loading
   * @param resources Pointer to the resource manager
   */
  void setResourceManager(resource::ResourceManager* resources);

  /**
   * @brief Set the dialogue box style
   */
  void setStyle(const DialogueBoxStyle& style);

  /**
   * @brief Get the current style
   */
  [[nodiscard]] const DialogueBoxStyle& getStyle() const;

  /**
   * @brief Set the bounding rectangle for the dialogue box
   */
  void setBounds(f32 x, f32 y, f32 width, f32 height);

  /**
   * @brief Get the bounds
   */
  [[nodiscard]] renderer::Rect getBounds() const;

  /**
   * @brief Set the speaker's name (empty for narrator)
   */
  void setSpeakerName(const std::string& name);

  /**
   * @brief Set the speaker's name color
   */
  void setSpeakerColor(const renderer::Color& color);

  /**
   * @brief Set the dialogue text
   * @param text The text to display
   * @param immediate If true, show all text immediately
   */
  void setText(const std::string& text, bool immediate = false);

  /**
   * @brief Get the current text
   */
  [[nodiscard]] const std::string& getText() const;

  /**
   * @brief Get the visible portion of text (for typewriter effect)
   */
  [[nodiscard]] std::string getVisibleText() const;

  /**
   * @brief Check if typewriter animation is complete
   */
  [[nodiscard]] bool isComplete() const;

  /**
   * @brief Skip to end of typewriter animation
   */
  void skipAnimation();

  /**
   * @brief Clear the dialogue box
   */
  void clear();

  /**
   * @brief Show or hide the wait indicator
   */
  void setShowWaitIndicator(bool show);

  /**
   * @brief Check if wait indicator is visible
   */
  [[nodiscard]] bool isWaitIndicatorVisible() const;

  /**
   * @brief Set callback for when text display completes
   */
  void setOnComplete(CompletionCallback callback);

  /**
   * @brief Enable/disable auto-advance mode
   * @param enabled Whether to auto-advance
   * @param delay Delay after text completes before advancing
   */
  void setAutoAdvance(bool enabled, f32 delay = 2.0f);

  /**
   * @brief Check if auto-advance is enabled
   */
  [[nodiscard]] bool isAutoAdvanceEnabled() const;

  /**
   * @brief Set the typewriter speed (characters per second)
   */
  void setTypewriterSpeed(f32 charsPerSecond);

  /**
   * @brief Start the typewriter animation
   */
  void startTypewriter();

  /**
   * @brief Check if typewriter animation is complete
   */
  [[nodiscard]] bool isTypewriterComplete() const;

  /**
   * @brief Check if waiting for user input
   */
  [[nodiscard]] bool isWaitingForInput() const;

  /**
   * @brief Show the dialogue box
   */
  void show();

  /**
   * @brief Hide the dialogue box
   */
  void hide();

  /**
   * @brief Update dialogue box state
   */
  void update(f64 deltaTime) override;

  /**
   * @brief Render the dialogue box
   */
  void render(renderer::IRenderer& renderer) override;

private:
  void updateTypewriter(f64 deltaTime);
  void updateWaitIndicator(f64 deltaTime);

  DialogueBoxStyle m_style;
  renderer::Rect m_bounds;

  std::string m_speakerName;
  renderer::Color m_speakerColor;
  std::string m_text;

  size_t m_visibleCharacters;
  f32 m_typewriterTimer;
  bool m_typewriterComplete;

  bool m_showWaitIndicator;
  f32 m_waitIndicatorTimer;
  bool m_waitIndicatorVisible;

  bool m_autoAdvance;
  f32 m_autoAdvanceDelay;
  f32 m_autoAdvanceTimer;

  CompletionCallback m_onComplete;

  resource::ResourceManager* m_resources = nullptr;
};

} // namespace NovelMind::Scene
