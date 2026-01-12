#pragma once

/**
 * @file character_sprite.hpp
 * @brief Character sprite scene object for visual novel characters
 *
 * This module provides the CharacterSprite class for displaying
 * and animating character sprites in visual novel scenes.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/scene/scene_object.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace NovelMind::Scene {

/**
 * @brief Character position presets
 */
enum class CharacterPosition { Left, Center, Right, Custom };

/**
 * @brief Represents a character sprite in a visual novel scene
 *
 * CharacterSprite handles:
 * - Multiple sprite variations (expressions/poses)
 * - Position presets (left, center, right)
 * - Smooth position transitions
 * - Alpha blending for fade effects
 * - Flip/mirror support
 */
class CharacterSprite : public scene::SceneObject {
public:
  /**
   * @brief Create a character sprite
   * @param id Unique identifier for this character instance
   * @param characterId The character definition ID (from NM Script)
   */
  CharacterSprite(const std::string& id, const std::string& characterId);
  ~CharacterSprite() override;

  /**
   * @brief Set the character's display name
   */
  void setDisplayName(const std::string& name);

  /**
   * @brief Get the character's display name
   */
  [[nodiscard]] const std::string& getDisplayName() const;

  /**
   * @brief Set the character definition ID
   */
  void setCharacterId(const std::string& characterId);

  /**
   * @brief Get the character definition ID
   */
  [[nodiscard]] const std::string& getCharacterId() const;

  /**
   * @brief Set the name color for dialogue display
   */
  void setNameColor(const renderer::Color& color);

  /**
   * @brief Get the name color
   */
  [[nodiscard]] const renderer::Color& getNameColor() const;

  /**
   * @brief Add a sprite texture for a specific expression/pose
   * @param expressionId Identifier for this expression (e.g., "happy", "sad")
   * @param texture The texture to use for this expression
   */
  void addExpression(const std::string& expressionId, std::shared_ptr<renderer::Texture> texture);

  /**
   * @brief Set the current expression
   * @param expressionId The expression to display
   * @param immediate If true, change immediately; if false, use transition
   */
  void setExpression(const std::string& expressionId, bool immediate = true);

  /**
   * @brief Get the current expression ID
   */
  [[nodiscard]] const std::string& getCurrentExpression() const;

  /**
   * @brief Set position using a preset
   * @param position The preset position
   * @param screenWidth The screen width for calculating position
   * @param screenHeight The screen height for calculating position
   */
  void setPresetPosition(CharacterPosition position, f32 screenWidth, f32 screenHeight);

  /**
   * @brief Set whether the sprite should be flipped horizontally
   */
  void setFlipped(bool flipped);

  /**
   * @brief Check if the sprite is flipped
   */
  [[nodiscard]] bool isFlipped() const;

  /**
   * @brief Set the anchor point (0,0 = top-left, 0.5,1 = bottom-center)
   */
  void setAnchor(f32 anchorX, f32 anchorY);

  /**
   * @brief Start a smooth movement to a new position
   * @param targetX Target X position
   * @param targetY Target Y position
   * @param duration Duration of the movement in seconds
   */
  void moveTo(f32 targetX, f32 targetY, f32 duration);

  /**
   * @brief Check if currently moving
   */
  [[nodiscard]] bool isMoving() const;

  /**
   * @brief Update character state
   */
  void update(f64 deltaTime) override;

  /**
   * @brief Render the character sprite
   */
  void render(renderer::IRenderer& renderer) override;

private:
  std::string m_characterId;
  std::string m_displayName;
  renderer::Color m_nameColor;

  std::unordered_map<std::string, std::shared_ptr<renderer::Texture>> m_expressions;
  std::string m_currentExpression;

  bool m_flipped;
  f32 m_anchorX;
  f32 m_anchorY;

  // Movement animation
  bool m_moving;
  f32 m_moveStartX;
  f32 m_moveStartY;
  f32 m_moveTargetX;
  f32 m_moveTargetY;
  f32 m_moveDuration;
  f32 m_moveElapsed;
};

} // namespace NovelMind::Scene
