#include "scene_graph_detail.hpp"

#include <sstream>

namespace NovelMind::scene::detail {

const float kDefaultDialogueWidth = 1200.0f;
const float kDefaultDialogueHeight = 260.0f;
const float kDefaultDialoguePadding = 24.0f;
const float kDefaultChoiceWidth = 600.0f;
const float kDefaultChoiceHeight = 320.0f;
const float kDefaultChoicePadding = 18.0f;

float parseFloat(const std::optional<std::string>& value, float fallback) {
  if (!value.has_value()) {
    return fallback;
  }
  try {
    return std::stof(*value);
  } catch (...) {
    return fallback;
  }
}

bool parseBool(const std::optional<std::string>& value, bool fallback) {
  if (!value.has_value()) {
    return fallback;
  }
  const std::string& text = *value;
  if (text == "true" || text == "1" || text == "yes" || text == "on") {
    return true;
  }
  if (text == "false" || text == "0" || text == "no" || text == "off") {
    return false;
  }
  return fallback;
}

[[maybe_unused]] NovelMind::renderer::Color parseColor(const std::optional<std::string>& value,
                                                       const NovelMind::renderer::Color& fallback) {
  if (!value.has_value()) {
    return fallback;
  }

  std::stringstream ss(*value);
  int r = 255;
  int g = 255;
  int b = 255;
  int a = 255;
  char comma = ',';
  if (ss >> r >> comma >> g >> comma >> b) {
    if (ss >> comma >> a) {
      return NovelMind::renderer::Color(
          static_cast<NovelMind::u8>(r), static_cast<NovelMind::u8>(g),
          static_cast<NovelMind::u8>(b), static_cast<NovelMind::u8>(a));
    }
    return NovelMind::renderer::Color(static_cast<NovelMind::u8>(r), static_cast<NovelMind::u8>(g),
                                      static_cast<NovelMind::u8>(b), 255);
  }

  return fallback;
}

std::string getTextProperty(const SceneObjectBase& obj, const std::string& key,
                            const std::string& fallback) {
  auto value = obj.getProperty(key);
  if (value.has_value() && !value->empty()) {
    return *value;
  }
  return fallback;
}

std::string defaultFontPath() {
#if defined(_WIN32)
  return "C:\\Windows\\Fonts\\segoeui.ttf";
#elif defined(__APPLE__)
  return "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
  return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

} // namespace NovelMind::scene::detail
