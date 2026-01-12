#pragma once

/**
 * @file choice_menu.hpp
 * @brief Choice menu scene object for player decisions
 *
 * This module provides the ChoiceMenu class for displaying
 * player choice options in visual novel branching.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/scene/scene_object.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::Scene {

/**
 * @brief Represents a single choice option
 */
struct ChoiceOption {
  std::string text;
  bool enabled{true};
  bool visible{true};
  std::optional<std::string> tooltip;
};

/**
 * @brief Choice menu display style
 */
struct ChoiceMenuStyle {
  renderer::Color backgroundColor{40, 40, 60, 220};
  renderer::Color borderColor{100, 100, 140, 255};
  renderer::Color textColor{255, 255, 255, 255};
  renderer::Color highlightColor{80, 80, 120, 255};
  renderer::Color selectedColor{120, 120, 180, 255};
  renderer::Color disabledColor{120, 120, 120, 180};

  f32 paddingLeft{20.0f};
  f32 paddingRight{20.0f};
  f32 paddingTop{15.0f};
  f32 paddingBottom{15.0f};

  f32 optionSpacing{10.0f};
  f32 optionHeight{50.0f};

  f32 borderWidth{2.0f};
  f32 cornerRadius{8.0f};

  f32 hoverTransitionSpeed{8.0f};
};

/**
 * @brief Displays choice options for player selection
 *
 * ChoiceMenu handles:
 * - Multiple choice options
 * - Keyboard/mouse selection
 * - Hover effects
 * - Disabled options
 * - Conditional visibility
 */
class ChoiceMenu : public scene::SceneObject {
public:
  using SelectionCallback = std::function<void(i32 index)>;

  /**
   * @brief Create a choice menu
   * @param id Unique identifier
   */
  explicit ChoiceMenu(const std::string& id);
  ~ChoiceMenu() override;

  /**
   * @brief Set the menu style
   */
  void setStyle(const ChoiceMenuStyle& style);

  /**
   * @brief Get the current style
   */
  [[nodiscard]] const ChoiceMenuStyle& getStyle() const;

  /**
   * @brief Set the bounding area for the menu
   */
  void setBounds(f32 x, f32 y, f32 width, f32 height);

  /**
   * @brief Get the bounds
   */
  [[nodiscard]] renderer::Rect getBounds() const;

  /**
   * @brief Set the choice options
   */
  void setOptions(const std::vector<ChoiceOption>& options);

  /**
   * @brief Add a choice option
   */
  void addOption(const std::string& text, bool enabled = true);

  /**
   * @brief Clear all options
   */
  void clearOptions();

  /**
   * @brief Get the number of options
   */
  [[nodiscard]] size_t getOptionCount() const;

  /**
   * @brief Get an option by index
   */
  [[nodiscard]] const ChoiceOption& getOption(size_t index) const;

  /**
   * @brief Enable or disable an option
   */
  void setOptionEnabled(size_t index, bool enabled);

  /**
   * @brief Show or hide an option
   */
  void setOptionVisible(size_t index, bool visible);

  /**
   * @brief Set the currently highlighted option
   */
  void setHighlightedIndex(i32 index);

  /**
   * @brief Get the highlighted option index
   */
  [[nodiscard]] i32 getHighlightedIndex() const;

  /**
   * @brief Move highlight up
   */
  void highlightPrevious();

  /**
   * @brief Move highlight down
   */
  void highlightNext();

  /**
   * @brief Select the currently highlighted option
   * @return true if an option was selected
   */
  bool selectHighlighted();

  /**
   * @brief Select an option by index
   * @return true if the option was selectable
   */
  bool selectOption(i32 index);

  /**
   * @brief Check if an option is under a mouse position
   * @return The option index or -1 if none
   */
  [[nodiscard]] i32 getOptionAtPosition(f32 mouseX, f32 mouseY) const;

  /**
   * @brief Set callback for when an option is selected
   */
  void setOnSelect(SelectionCallback callback);

  /**
   * @brief Get the selected option index (-1 if none selected)
   */
  [[nodiscard]] i32 getSelectedIndex() const;

  /**
   * @brief Reset selection state
   */
  void resetSelection();

  /**
   * @brief Update menu state
   */
  void update(f64 deltaTime) override;

  /**
   * @brief Render the choice menu
   */
  void render(renderer::IRenderer& renderer) override;

private:
  [[nodiscard]] f32 calculateMenuHeight() const;
  [[nodiscard]] renderer::Rect getOptionRect(size_t index) const;
  [[nodiscard]] i32 getFirstEnabledIndex() const;
  [[nodiscard]] i32 getNextEnabledIndex(i32 current, i32 direction) const;

  ChoiceMenuStyle m_style;
  renderer::Rect m_bounds;

  std::vector<ChoiceOption> m_options;
  std::vector<f32> m_optionHighlight; // 0.0 to 1.0 for hover animation

  i32 m_highlightedIndex;
  i32 m_selectedIndex;

  SelectionCallback m_onSelect;
};

} // namespace NovelMind::Scene
