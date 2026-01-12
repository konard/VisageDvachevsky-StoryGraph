/**
 * @file ui_theme.cpp
 * @brief Theme implementation
 */

#include "NovelMind/ui/ui_framework.hpp"

namespace NovelMind::ui {

// ============================================================================
// Theme Implementation
// ============================================================================

Theme::Theme() {
  // Set default style
  m_defaultStyle = Style{};
}

void Theme::setStyle(const std::string& name, const Style& style) {
  m_styles[name] = style;
}

const Style& Theme::getStyle(const std::string& name) const {
  auto it = m_styles.find(name);
  return (it != m_styles.end()) ? it->second : m_defaultStyle;
}

bool Theme::hasStyle(const std::string& name) const {
  return m_styles.find(name) != m_styles.end();
}

Theme Theme::createDarkTheme() {
  Theme theme;

  // Default style
  Style defaultStyle;
  defaultStyle.backgroundColor = renderer::Color{40, 40, 40, 255};
  defaultStyle.foregroundColor = renderer::Color{255, 255, 255, 255};
  defaultStyle.borderColor = renderer::Color{80, 80, 80, 255};
  defaultStyle.textColor = renderer::Color{220, 220, 220, 255};
  theme.setStyle("default", defaultStyle);

  // Button style
  Style buttonStyle = defaultStyle;
  buttonStyle.backgroundColor = renderer::Color{60, 60, 60, 255};
  buttonStyle.hoverColor = renderer::Color{80, 80, 80, 255};
  buttonStyle.activeColor = renderer::Color{100, 100, 100, 255};
  buttonStyle.borderWidth = 1.0f;
  buttonStyle.borderRadius = 4.0f;
  buttonStyle.padding = Insets::symmetric(16.0f, 8.0f);
  theme.setStyle("button", buttonStyle);

  // Label style
  Style labelStyle = defaultStyle;
  labelStyle.backgroundColor = renderer::Color{0, 0, 0, 0};
  labelStyle.padding = Insets::all(4.0f);
  theme.setStyle("label", labelStyle);

  // Panel style
  Style panelStyle = defaultStyle;
  panelStyle.backgroundColor = renderer::Color{50, 50, 50, 255};
  panelStyle.borderWidth = 1.0f;
  panelStyle.borderColor = renderer::Color{70, 70, 70, 255};
  panelStyle.borderRadius = 4.0f;
  panelStyle.padding = Insets::all(8.0f);
  theme.setStyle("panel", panelStyle);

  // Input style
  Style inputStyle = defaultStyle;
  inputStyle.backgroundColor = renderer::Color{30, 30, 30, 255};
  inputStyle.borderWidth = 1.0f;
  inputStyle.borderColor = renderer::Color{80, 80, 80, 255};
  inputStyle.borderRadius = 4.0f;
  inputStyle.padding = Insets::symmetric(8.0f, 6.0f);
  theme.setStyle("input", inputStyle);

  // Checkbox style
  Style checkboxStyle = defaultStyle;
  checkboxStyle.backgroundColor = renderer::Color{0, 0, 0, 0};
  checkboxStyle.accentColor = renderer::Color{0, 120, 215, 255};
  theme.setStyle("checkbox", checkboxStyle);

  // Slider style
  Style sliderStyle = defaultStyle;
  sliderStyle.backgroundColor = renderer::Color{60, 60, 60, 255};
  sliderStyle.accentColor = renderer::Color{0, 120, 215, 255};
  sliderStyle.borderRadius = 4.0f;
  theme.setStyle("slider", sliderStyle);

  return theme;
}

Theme Theme::createLightTheme() {
  Theme theme;

  // Default style
  Style defaultStyle;
  defaultStyle.backgroundColor = renderer::Color{245, 245, 245, 255};
  defaultStyle.foregroundColor = renderer::Color{0, 0, 0, 255};
  defaultStyle.borderColor = renderer::Color{200, 200, 200, 255};
  defaultStyle.textColor = renderer::Color{30, 30, 30, 255};
  theme.setStyle("default", defaultStyle);

  // Button style
  Style buttonStyle = defaultStyle;
  buttonStyle.backgroundColor = renderer::Color{225, 225, 225, 255};
  buttonStyle.hoverColor = renderer::Color{210, 210, 210, 255};
  buttonStyle.activeColor = renderer::Color{195, 195, 195, 255};
  buttonStyle.borderWidth = 1.0f;
  buttonStyle.borderRadius = 4.0f;
  buttonStyle.padding = Insets::symmetric(16.0f, 8.0f);
  theme.setStyle("button", buttonStyle);

  // Label style
  Style labelStyle = defaultStyle;
  labelStyle.backgroundColor = renderer::Color{0, 0, 0, 0};
  labelStyle.padding = Insets::all(4.0f);
  theme.setStyle("label", labelStyle);

  // Panel style
  Style panelStyle = defaultStyle;
  panelStyle.backgroundColor = renderer::Color{255, 255, 255, 255};
  panelStyle.borderWidth = 1.0f;
  panelStyle.borderColor = renderer::Color{220, 220, 220, 255};
  panelStyle.borderRadius = 4.0f;
  panelStyle.padding = Insets::all(8.0f);
  theme.setStyle("panel", panelStyle);

  // Input style
  Style inputStyle = defaultStyle;
  inputStyle.backgroundColor = renderer::Color{255, 255, 255, 255};
  inputStyle.borderWidth = 1.0f;
  inputStyle.borderColor = renderer::Color{180, 180, 180, 255};
  inputStyle.borderRadius = 4.0f;
  inputStyle.padding = Insets::symmetric(8.0f, 6.0f);
  theme.setStyle("input", inputStyle);

  return theme;
}

} // namespace NovelMind::ui
