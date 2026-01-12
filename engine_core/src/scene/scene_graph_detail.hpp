#pragma once

#include "NovelMind/scene/scene_graph.hpp"
#include <optional>
#include <string>

namespace NovelMind::scene::detail {

extern const float kDefaultDialogueWidth;
extern const float kDefaultDialogueHeight;
extern const float kDefaultDialoguePadding;
extern const float kDefaultChoiceWidth;
extern const float kDefaultChoiceHeight;
extern const float kDefaultChoicePadding;

float parseFloat(const std::optional<std::string>& value, float fallback);
bool parseBool(const std::optional<std::string>& value, bool fallback);
[[maybe_unused]] NovelMind::renderer::Color parseColor(const std::optional<std::string>& value,
                                                       const NovelMind::renderer::Color& fallback);
std::string getTextProperty(const SceneObjectBase& obj, const std::string& key,
                            const std::string& fallback);
std::string defaultFontPath();

} // namespace NovelMind::scene::detail
