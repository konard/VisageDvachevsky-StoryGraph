/**
 * @file tutorial_types.cpp
 * @brief Implementation of tutorial type utilities
 */

#include "NovelMind/editor/guided_learning/tutorial_types.hpp"
#include <algorithm>
#include <cctype>

namespace NovelMind::editor::guided_learning {

const char* tutorialLevelToString(TutorialLevel level) {
  switch (level) {
  case TutorialLevel::Beginner:
    return "Beginner";
  case TutorialLevel::Intermediate:
    return "Intermediate";
  case TutorialLevel::Advanced:
    return "Advanced";
  }
  return "Unknown";
}

const char* tutorialTriggerToString(TutorialTrigger trigger) {
  switch (trigger) {
  case TutorialTrigger::Manual:
    return "Manual";
  case TutorialTrigger::FirstRun:
    return "FirstRun";
  case TutorialTrigger::Contextual:
    return "Contextual";
  case TutorialTrigger::OnError:
    return "OnError";
  }
  return "Unknown";
}

const char* hintTypeToString(HintType type) {
  switch (type) {
  case HintType::Tooltip:
    return "Tooltip";
  case HintType::Callout:
    return "Callout";
  case HintType::Spotlight:
    return "Spotlight";
  case HintType::EmptyState:
    return "EmptyState";
  case HintType::Inline:
    return "Inline";
  case HintType::Dialog:
    return "Dialog";
  }
  return "Unknown";
}

const char* calloutPositionToString(CalloutPosition pos) {
  switch (pos) {
  case CalloutPosition::Auto:
    return "Auto";
  case CalloutPosition::Top:
    return "Top";
  case CalloutPosition::Bottom:
    return "Bottom";
  case CalloutPosition::Left:
    return "Left";
  case CalloutPosition::Right:
    return "Right";
  case CalloutPosition::TopLeft:
    return "TopLeft";
  case CalloutPosition::TopRight:
    return "TopRight";
  case CalloutPosition::BottomLeft:
    return "BottomLeft";
  case CalloutPosition::BottomRight:
    return "BottomRight";
  }
  return "Unknown";
}

const char* stepStateToString(StepState state) {
  switch (state) {
  case StepState::Pending:
    return "Pending";
  case StepState::Active:
    return "Active";
  case StepState::Completed:
    return "Completed";
  case StepState::Skipped:
    return "Skipped";
  }
  return "Unknown";
}

const char* tutorialStateToString(TutorialState state) {
  switch (state) {
  case TutorialState::NotStarted:
    return "NotStarted";
  case TutorialState::InProgress:
    return "InProgress";
  case TutorialState::Completed:
    return "Completed";
  case TutorialState::Disabled:
    return "Disabled";
  }
  return "Unknown";
}

namespace {

std::string toLower(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

} // anonymous namespace

std::optional<TutorialLevel> parseTutorialLevel(const std::string& str) {
  std::string lower = toLower(str);
  if (lower == "beginner")
    return TutorialLevel::Beginner;
  if (lower == "intermediate")
    return TutorialLevel::Intermediate;
  if (lower == "advanced")
    return TutorialLevel::Advanced;
  return std::nullopt;
}

std::optional<TutorialTrigger> parseTutorialTrigger(const std::string& str) {
  std::string lower = toLower(str);
  if (lower == "manual")
    return TutorialTrigger::Manual;
  if (lower == "firstrun" || lower == "first_run")
    return TutorialTrigger::FirstRun;
  if (lower == "contextual")
    return TutorialTrigger::Contextual;
  if (lower == "onerror" || lower == "on_error")
    return TutorialTrigger::OnError;
  return std::nullopt;
}

std::optional<HintType> parseHintType(const std::string& str) {
  std::string lower = toLower(str);
  if (lower == "tooltip")
    return HintType::Tooltip;
  if (lower == "callout")
    return HintType::Callout;
  if (lower == "spotlight")
    return HintType::Spotlight;
  if (lower == "emptystate" || lower == "empty_state")
    return HintType::EmptyState;
  if (lower == "inline")
    return HintType::Inline;
  if (lower == "dialog")
    return HintType::Dialog;
  return std::nullopt;
}

std::optional<CalloutPosition> parseCalloutPosition(const std::string& str) {
  std::string lower = toLower(str);
  if (lower == "auto")
    return CalloutPosition::Auto;
  if (lower == "top")
    return CalloutPosition::Top;
  if (lower == "bottom")
    return CalloutPosition::Bottom;
  if (lower == "left")
    return CalloutPosition::Left;
  if (lower == "right")
    return CalloutPosition::Right;
  if (lower == "topleft" || lower == "top_left")
    return CalloutPosition::TopLeft;
  if (lower == "topright" || lower == "top_right")
    return CalloutPosition::TopRight;
  if (lower == "bottomleft" || lower == "bottom_left")
    return CalloutPosition::BottomLeft;
  if (lower == "bottomright" || lower == "bottom_right")
    return CalloutPosition::BottomRight;
  return std::nullopt;
}

} // namespace NovelMind::editor::guided_learning
