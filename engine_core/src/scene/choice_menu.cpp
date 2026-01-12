#include "NovelMind/scene/choice_menu.hpp"
#include <algorithm>

namespace NovelMind::Scene {

ChoiceMenu::ChoiceMenu(const std::string& id)
    : scene::SceneObject(id), m_style(), m_bounds{0.0f, 0.0f, 400.0f, 300.0f},
      m_highlightedIndex(-1), m_selectedIndex(-1), m_onSelect(nullptr) {}

ChoiceMenu::~ChoiceMenu() = default;

void ChoiceMenu::setStyle(const ChoiceMenuStyle& style) {
  m_style = style;
}

const ChoiceMenuStyle& ChoiceMenu::getStyle() const {
  return m_style;
}

void ChoiceMenu::setBounds(f32 x, f32 y, f32 width, f32 height) {
  m_bounds = renderer::Rect{x, y, width, height};
}

renderer::Rect ChoiceMenu::getBounds() const {
  return m_bounds;
}

void ChoiceMenu::setOptions(const std::vector<ChoiceOption>& options) {
  m_options = options;
  m_optionHighlight.resize(options.size(), 0.0f);
  m_highlightedIndex = getFirstEnabledIndex();
  m_selectedIndex = -1;
}

void ChoiceMenu::addOption(const std::string& text, bool enabled) {
  ChoiceOption option;
  option.text = text;
  option.enabled = enabled;
  option.visible = true;
  m_options.push_back(std::move(option));
  m_optionHighlight.push_back(0.0f);

  if (m_highlightedIndex < 0 && enabled) {
    m_highlightedIndex = static_cast<i32>(m_options.size()) - 1;
  }
}

void ChoiceMenu::clearOptions() {
  m_options.clear();
  m_optionHighlight.clear();
  m_highlightedIndex = -1;
  m_selectedIndex = -1;
}

size_t ChoiceMenu::getOptionCount() const {
  return m_options.size();
}

const ChoiceOption& ChoiceMenu::getOption(size_t index) const {
  return m_options[index];
}

void ChoiceMenu::setOptionEnabled(size_t index, bool enabled) {
  if (index < m_options.size()) {
    m_options[index].enabled = enabled;
  }
}

void ChoiceMenu::setOptionVisible(size_t index, bool visible) {
  if (index < m_options.size()) {
    m_options[index].visible = visible;
  }
}

void ChoiceMenu::setHighlightedIndex(i32 index) {
  if (index < 0 || index >= static_cast<i32>(m_options.size())) {
    m_highlightedIndex = -1;
    return;
  }

  size_t idx = static_cast<size_t>(index);
  if (m_options[idx].enabled && m_options[idx].visible) {
    m_highlightedIndex = index;
  }
}

i32 ChoiceMenu::getHighlightedIndex() const {
  return m_highlightedIndex;
}

void ChoiceMenu::highlightPrevious() {
  m_highlightedIndex = getNextEnabledIndex(m_highlightedIndex, -1);
}

void ChoiceMenu::highlightNext() {
  m_highlightedIndex = getNextEnabledIndex(m_highlightedIndex, 1);
}

bool ChoiceMenu::selectHighlighted() {
  return selectOption(m_highlightedIndex);
}

bool ChoiceMenu::selectOption(i32 index) {
  if (index < 0 || index >= static_cast<i32>(m_options.size())) {
    return false;
  }

  size_t idx = static_cast<size_t>(index);
  if (!m_options[idx].enabled || !m_options[idx].visible) {
    return false;
  }

  m_selectedIndex = index;

  if (m_onSelect) {
    m_onSelect(index);
  }

  return true;
}

i32 ChoiceMenu::getOptionAtPosition(f32 mouseX, f32 mouseY) const {
  for (size_t i = 0; i < m_options.size(); ++i) {
    if (!m_options[i].visible) {
      continue;
    }

    renderer::Rect optionRect = getOptionRect(i);

    if (mouseX >= optionRect.x && mouseX < optionRect.x + optionRect.width &&
        mouseY >= optionRect.y && mouseY < optionRect.y + optionRect.height) {
      return static_cast<i32>(i);
    }
  }

  return -1;
}

void ChoiceMenu::setOnSelect(SelectionCallback callback) {
  m_onSelect = std::move(callback);
}

i32 ChoiceMenu::getSelectedIndex() const {
  return m_selectedIndex;
}

void ChoiceMenu::resetSelection() {
  m_selectedIndex = -1;
  m_highlightedIndex = getFirstEnabledIndex();
}

void ChoiceMenu::update(f64 deltaTime) {
  SceneObject::update(deltaTime);

  // Update hover animations
  f32 dt = static_cast<f32>(deltaTime);

  for (size_t i = 0; i < m_optionHighlight.size(); ++i) {
    f32 target = (static_cast<i32>(i) == m_highlightedIndex) ? 1.0f : 0.0f;
    f32& current = m_optionHighlight[i];

    if (current < target) {
      current = std::min(current + dt * m_style.hoverTransitionSpeed, target);
    } else if (current > target) {
      current = std::max(current - dt * m_style.hoverTransitionSpeed, target);
    }
  }
}

void ChoiceMenu::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_options.empty()) {
    return;
  }

  // Calculate actual menu height based on visible options
  f32 menuHeight = calculateMenuHeight();

  // Center menu vertically if it's smaller than bounds
  f32 menuY = m_bounds.y;
  if (menuHeight < m_bounds.height) {
    menuY += (m_bounds.height - menuHeight) * 0.5f;
  }

  // Draw background
  renderer::Rect menuRect{m_bounds.x, menuY, m_bounds.width, menuHeight};
  renderer::Color bgColor = m_style.backgroundColor;
  bgColor.a = static_cast<u8>(bgColor.a * m_alpha);
  renderer.fillRect(menuRect, bgColor);

  // Draw border
  if (m_style.borderWidth > 0.0f) {
    renderer::Color borderColor = m_style.borderColor;
    borderColor.a = static_cast<u8>(borderColor.a * m_alpha);
    renderer.drawRect(menuRect, borderColor);
  }

  // Draw options
  f32 currentY = menuY + m_style.paddingTop;

  for (size_t i = 0; i < m_options.size(); ++i) {
    const auto& option = m_options[i];

    if (!option.visible) {
      continue;
    }

    renderer::Rect optionRect{m_bounds.x + m_style.paddingLeft, currentY,
                              m_bounds.width - m_style.paddingLeft - m_style.paddingRight,
                              m_style.optionHeight};

    // Determine background color based on state
    renderer::Color optionBgColor;
    if (static_cast<i32>(i) == m_selectedIndex) {
      optionBgColor = m_style.selectedColor;
    } else if (m_optionHighlight[i] > 0.0f) {
      // Interpolate between no highlight and full highlight
      optionBgColor = m_style.highlightColor;
      optionBgColor.a = static_cast<u8>(optionBgColor.a * m_optionHighlight[i]);
    } else {
      optionBgColor = renderer::Color::Transparent;
    }

    // Apply alpha
    optionBgColor.a = static_cast<u8>(optionBgColor.a * m_alpha);

    if (optionBgColor.a > 0) {
      renderer.fillRect(optionRect, optionBgColor);
    }

    // Draw text color indicator (actual text rendering needs font support)
    renderer::Color textColor = option.enabled ? m_style.textColor : m_style.disabledColor;
    textColor.a = static_cast<u8>(textColor.a * m_alpha);

    // For now, draw a small indicator bar
    renderer::Rect indicatorRect{optionRect.x + 5.0f, optionRect.y + optionRect.height * 0.3f, 4.0f,
                                 optionRect.height * 0.4f};
    renderer.fillRect(indicatorRect, textColor);

    currentY += m_style.optionHeight + m_style.optionSpacing;
  }
}

f32 ChoiceMenu::calculateMenuHeight() const {
  f32 height = m_style.paddingTop + m_style.paddingBottom;

  size_t visibleCount = 0;
  for (const auto& option : m_options) {
    if (option.visible) {
      ++visibleCount;
    }
  }

  if (visibleCount > 0) {
    height += static_cast<f32>(visibleCount) * m_style.optionHeight;
    height += static_cast<f32>(visibleCount - 1) * m_style.optionSpacing;
  }

  return height;
}

renderer::Rect ChoiceMenu::getOptionRect(size_t index) const {
  f32 menuHeight = calculateMenuHeight();
  f32 menuY = m_bounds.y;
  if (menuHeight < m_bounds.height) {
    menuY += (m_bounds.height - menuHeight) * 0.5f;
  }

  f32 currentY = menuY + m_style.paddingTop;

  for (size_t i = 0; i < m_options.size(); ++i) {
    if (!m_options[i].visible) {
      continue;
    }

    if (i == index) {
      return renderer::Rect{m_bounds.x + m_style.paddingLeft, currentY,
                            m_bounds.width - m_style.paddingLeft - m_style.paddingRight,
                            m_style.optionHeight};
    }

    currentY += m_style.optionHeight + m_style.optionSpacing;
  }

  return renderer::Rect{0, 0, 0, 0};
}

i32 ChoiceMenu::getFirstEnabledIndex() const {
  for (size_t i = 0; i < m_options.size(); ++i) {
    if (m_options[i].enabled && m_options[i].visible) {
      return static_cast<i32>(i);
    }
  }
  return -1;
}

i32 ChoiceMenu::getNextEnabledIndex(i32 current, i32 direction) const {
  if (m_options.empty()) {
    return -1;
  }

  i32 count = static_cast<i32>(m_options.size());
  i32 index = current;

  // Wrap around
  for (i32 i = 0; i < count; ++i) {
    index += direction;

    if (index < 0) {
      index = count - 1;
    } else if (index >= count) {
      index = 0;
    }

    size_t idx = static_cast<size_t>(index);
    if (m_options[idx].enabled && m_options[idx].visible) {
      return index;
    }
  }

  return current;
}

} // namespace NovelMind::Scene
