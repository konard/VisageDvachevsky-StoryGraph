/**
 * @file scene_object_choice.cpp
 * @brief Choice UI scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <utility>

#include "NovelMind/localization/localization_manager.hpp"
#include "NovelMind/renderer/text_layout.hpp"

#include "scene_graph_detail.hpp"

namespace NovelMind::scene {

// ============================================================================
// ChoiceUIObject Implementation
// ============================================================================

ChoiceUIObject::ChoiceUIObject(const std::string& id)
    : SceneObjectBase(id, SceneObjectType::ChoiceUI) {}

void ChoiceUIObject::setChoices(const std::vector<ChoiceOption>& choices) {
  m_choices = choices;
  m_selectedIndex = 0;
}

void ChoiceUIObject::clearChoices() {
  m_choices.clear();
  m_selectedIndex = 0;
}

void ChoiceUIObject::setSelectedIndex(i32 index) {
  if (index >= 0 && index < static_cast<i32>(m_choices.size())) {
    m_selectedIndex = index;
  }
}

void ChoiceUIObject::selectNext() {
  if (m_choices.empty()) {
    return;
  }

  i32 start = m_selectedIndex;
  do {
    m_selectedIndex = (m_selectedIndex + 1) % static_cast<i32>(m_choices.size());
  } while (!m_choices[static_cast<size_t>(m_selectedIndex)].enabled && m_selectedIndex != start);
}

void ChoiceUIObject::selectPrevious() {
  if (m_choices.empty()) {
    return;
  }

  i32 start = m_selectedIndex;
  do {
    m_selectedIndex = (m_selectedIndex - 1 + static_cast<i32>(m_choices.size())) %
                      static_cast<i32>(m_choices.size());
  } while (!m_choices[static_cast<size_t>(m_selectedIndex)].enabled && m_selectedIndex != start);
}

bool ChoiceUIObject::confirm() {
  if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<i32>(m_choices.size())) {
    const auto& choice = m_choices[static_cast<size_t>(m_selectedIndex)];
    if (choice.enabled && m_onSelect) {
      m_onSelect(m_selectedIndex, choice.id);
      return true;
    }
  }
  return false;
}

void ChoiceUIObject::setOnSelect(std::function<void(i32, const std::string&)> callback) {
  m_onSelect = std::move(callback);
}

void ChoiceUIObject::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f) {
    return;
  }

  if (!m_resources) {
    return;
  }

  const bool rtl = detail::parseBool(
      getProperty("rtl"), m_localization ? m_localization->isCurrentLocaleRightToLeft() : false);

  const float width = detail::parseFloat(getProperty("width"), detail::kDefaultChoiceWidth);
  const float height = detail::parseFloat(getProperty("height"), detail::kDefaultChoiceHeight);
  const float padding = detail::parseFloat(getProperty("padding"), detail::kDefaultChoicePadding);

  renderer::Rect rect{m_transform.x - width * m_anchorX, m_transform.y - height * m_anchorY, width,
                      height};

  renderer::Color bg = renderer::Color(20, 20, 20, 200);
  bg.a = static_cast<u8>(bg.a * m_alpha);
  renderer.fillRect(rect, bg);

  std::string fontId = detail::getTextProperty(*this, "fontId", detail::defaultFontPath());
  i32 fontSize = static_cast<i32>(detail::parseFloat(getProperty("fontSize"), 18.0f));
  if (fontId.empty()) {
    return;
  }

  auto fontResult = m_resources->loadFont(fontId, fontSize);
  if (fontResult.isError()) {
    return;
  }

  float y = rect.y + padding;
  for (size_t i = 0; i < m_choices.size(); ++i) {
    const auto& choice = m_choices[i];
    if (!choice.visible) {
      continue;
    }
    renderer::Color color =
        choice.enabled ? renderer::Color::White : renderer::Color(140, 140, 140, 255);
    if (static_cast<i32>(i) == m_selectedIndex) {
      color = renderer::Color(255, 220, 80, 255);
    }
    f32 x = rect.x + padding;
    if (rtl) {
      renderer::TextLayoutEngine layout;
      layout.setFont(fontResult.value());
      renderer::TextStyle style;
      style.size = static_cast<f32>(fontSize);
      layout.setDefaultStyle(style);
      f32 textWidth = layout.measureText(choice.text).first;
      x = rect.x + rect.width - padding - textWidth;
    }
    renderer.drawText(*fontResult.value(), choice.text, x, y, color);
    y += static_cast<float>(fontSize) * 1.4f;
  }
}

SceneObjectState ChoiceUIObject::saveState() const {
  auto state = SceneObjectBase::saveState();
  state.properties["choiceCount"] = std::to_string(m_choices.size());
  for (size_t i = 0; i < m_choices.size(); ++i) {
    std::string prefix = "choice" + std::to_string(i) + "_";
    state.properties[prefix + "id"] = m_choices[i].id;
    state.properties[prefix + "text"] = m_choices[i].text;
    state.properties[prefix + "enabled"] = m_choices[i].enabled ? "true" : "false";
    state.properties[prefix + "visible"] = m_choices[i].visible ? "true" : "false";
  }
  state.properties["selectedIndex"] = std::to_string(m_selectedIndex);
  return state;
}

void ChoiceUIObject::loadState(const SceneObjectState& state) {
  SceneObjectBase::loadState(state);

  m_choices.clear();
  auto countIt = state.properties.find("choiceCount");
  if (countIt != state.properties.end()) {
    size_t count = std::stoul(countIt->second);
    for (size_t i = 0; i < count; ++i) {
      std::string prefix = "choice" + std::to_string(i) + "_";
      ChoiceOption option;

      auto it = state.properties.find(prefix + "id");
      if (it != state.properties.end())
        option.id = it->second;

      it = state.properties.find(prefix + "text");
      if (it != state.properties.end())
        option.text = it->second;

      it = state.properties.find(prefix + "enabled");
      if (it != state.properties.end())
        option.enabled = (it->second == "true");

      it = state.properties.find(prefix + "visible");
      if (it != state.properties.end())
        option.visible = (it->second == "true");

      m_choices.push_back(option);
    }
  }

  auto selIt = state.properties.find("selectedIndex");
  if (selIt != state.properties.end()) {
    m_selectedIndex = std::stoi(selIt->second);
  }
}

} // namespace NovelMind::scene
