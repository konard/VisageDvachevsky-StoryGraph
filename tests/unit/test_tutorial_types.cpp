/**
 * @file test_tutorial_types.cpp
 * @brief Unit tests for tutorial type utilities
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/guided_learning/tutorial_types.hpp"

using namespace NovelMind::editor::guided_learning;

// ============================================================================
// TutorialLevel tests
// ============================================================================

TEST_CASE("tutorialLevelToString converts levels correctly", "[tutorial][types]") {
  REQUIRE(std::string(tutorialLevelToString(TutorialLevel::Beginner)) == "Beginner");
  REQUIRE(std::string(tutorialLevelToString(TutorialLevel::Intermediate)) == "Intermediate");
  REQUIRE(std::string(tutorialLevelToString(TutorialLevel::Advanced)) == "Advanced");
}

TEST_CASE("parseTutorialLevel parses valid strings", "[tutorial][types]") {
  auto beginner = parseTutorialLevel("Beginner");
  REQUIRE(beginner.has_value());
  REQUIRE(*beginner == TutorialLevel::Beginner);

  auto intermediate = parseTutorialLevel("intermediate");
  REQUIRE(intermediate.has_value());
  REQUIRE(*intermediate == TutorialLevel::Intermediate);

  auto advanced = parseTutorialLevel("ADVANCED");
  REQUIRE(advanced.has_value());
  REQUIRE(*advanced == TutorialLevel::Advanced);
}

TEST_CASE("parseTutorialLevel returns nullopt for invalid strings", "[tutorial][types]") {
  auto invalid = parseTutorialLevel("expert");
  REQUIRE_FALSE(invalid.has_value());

  auto empty = parseTutorialLevel("");
  REQUIRE_FALSE(empty.has_value());
}

// ============================================================================
// TutorialTrigger tests
// ============================================================================

TEST_CASE("tutorialTriggerToString converts triggers correctly", "[tutorial][types]") {
  REQUIRE(std::string(tutorialTriggerToString(TutorialTrigger::Manual)) == "Manual");
  REQUIRE(std::string(tutorialTriggerToString(TutorialTrigger::FirstRun)) == "FirstRun");
  REQUIRE(std::string(tutorialTriggerToString(TutorialTrigger::Contextual)) == "Contextual");
  REQUIRE(std::string(tutorialTriggerToString(TutorialTrigger::OnError)) == "OnError");
}

TEST_CASE("parseTutorialTrigger parses valid strings", "[tutorial][types]") {
  auto manual = parseTutorialTrigger("manual");
  REQUIRE(manual.has_value());
  REQUIRE(*manual == TutorialTrigger::Manual);

  auto firstRun = parseTutorialTrigger("FirstRun");
  REQUIRE(firstRun.has_value());
  REQUIRE(*firstRun == TutorialTrigger::FirstRun);

  auto firstRunUnderscore = parseTutorialTrigger("first_run");
  REQUIRE(firstRunUnderscore.has_value());
  REQUIRE(*firstRunUnderscore == TutorialTrigger::FirstRun);

  auto contextual = parseTutorialTrigger("contextual");
  REQUIRE(contextual.has_value());
  REQUIRE(*contextual == TutorialTrigger::Contextual);

  auto onError = parseTutorialTrigger("on_error");
  REQUIRE(onError.has_value());
  REQUIRE(*onError == TutorialTrigger::OnError);
}

// ============================================================================
// HintType tests
// ============================================================================

TEST_CASE("hintTypeToString converts hint types correctly", "[tutorial][types]") {
  REQUIRE(std::string(hintTypeToString(HintType::Tooltip)) == "Tooltip");
  REQUIRE(std::string(hintTypeToString(HintType::Callout)) == "Callout");
  REQUIRE(std::string(hintTypeToString(HintType::Spotlight)) == "Spotlight");
  REQUIRE(std::string(hintTypeToString(HintType::EmptyState)) == "EmptyState");
  REQUIRE(std::string(hintTypeToString(HintType::Inline)) == "Inline");
  REQUIRE(std::string(hintTypeToString(HintType::Dialog)) == "Dialog");
}

TEST_CASE("parseHintType parses valid strings", "[tutorial][types]") {
  auto tooltip = parseHintType("tooltip");
  REQUIRE(tooltip.has_value());
  REQUIRE(*tooltip == HintType::Tooltip);

  auto callout = parseHintType("Callout");
  REQUIRE(callout.has_value());
  REQUIRE(*callout == HintType::Callout);

  auto emptyState = parseHintType("empty_state");
  REQUIRE(emptyState.has_value());
  REQUIRE(*emptyState == HintType::EmptyState);
}

TEST_CASE("parseHintType returns nullopt for invalid strings", "[tutorial][types]") {
  auto invalid = parseHintType("popup");
  REQUIRE_FALSE(invalid.has_value());
}

// ============================================================================
// CalloutPosition tests
// ============================================================================

TEST_CASE("calloutPositionToString converts positions correctly", "[tutorial][types]") {
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::Auto)) == "Auto");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::Top)) == "Top");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::Bottom)) == "Bottom");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::Left)) == "Left");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::Right)) == "Right");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::TopLeft)) == "TopLeft");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::TopRight)) == "TopRight");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::BottomLeft)) == "BottomLeft");
  REQUIRE(std::string(calloutPositionToString(CalloutPosition::BottomRight)) == "BottomRight");
}

TEST_CASE("parseCalloutPosition parses valid strings", "[tutorial][types]") {
  auto autoPos = parseCalloutPosition("auto");
  REQUIRE(autoPos.has_value());
  REQUIRE(*autoPos == CalloutPosition::Auto);

  auto top = parseCalloutPosition("Top");
  REQUIRE(top.has_value());
  REQUIRE(*top == CalloutPosition::Top);

  auto topLeft = parseCalloutPosition("top_left");
  REQUIRE(topLeft.has_value());
  REQUIRE(*topLeft == CalloutPosition::TopLeft);

  auto bottomRight = parseCalloutPosition("BottomRight");
  REQUIRE(bottomRight.has_value());
  REQUIRE(*bottomRight == CalloutPosition::BottomRight);
}

// ============================================================================
// State enum tests
// ============================================================================

TEST_CASE("stepStateToString converts step states correctly", "[tutorial][types]") {
  REQUIRE(std::string(stepStateToString(StepState::Pending)) == "Pending");
  REQUIRE(std::string(stepStateToString(StepState::Active)) == "Active");
  REQUIRE(std::string(stepStateToString(StepState::Completed)) == "Completed");
  REQUIRE(std::string(stepStateToString(StepState::Skipped)) == "Skipped");
}

TEST_CASE("tutorialStateToString converts tutorial states correctly", "[tutorial][types]") {
  REQUIRE(std::string(tutorialStateToString(TutorialState::NotStarted)) == "NotStarted");
  REQUIRE(std::string(tutorialStateToString(TutorialState::InProgress)) == "InProgress");
  REQUIRE(std::string(tutorialStateToString(TutorialState::Completed)) == "Completed");
  REQUIRE(std::string(tutorialStateToString(TutorialState::Disabled)) == "Disabled");
}

// ============================================================================
// Data structure tests
// ============================================================================

TEST_CASE("TutorialStep default values are sensible", "[tutorial][types]") {
  TutorialStep step;

  REQUIRE(step.id.empty());
  REQUIRE(step.content.empty());
  REQUIRE(step.hintType == HintType::Callout);
  REQUIRE(step.position == CalloutPosition::Auto);
  REQUIRE(step.showBackButton == true);
  REQUIRE(step.showSkipButton == true);
  REQUIRE(step.showDontShowAgain == true);
  REQUIRE(step.autoHide == false);
}

TEST_CASE("ContextualHint default values are sensible", "[tutorial][types]") {
  ContextualHint hint;

  REQUIRE(hint.id.empty());
  REQUIRE(hint.content.empty());
  REQUIRE(hint.hintType == HintType::Tooltip);
  REQUIRE(hint.position == CalloutPosition::Auto);
  REQUIRE(hint.maxShowCount == 3);
  REQUIRE(hint.showOncePerSession == false);
  REQUIRE(hint.autoHide == true);
}

TEST_CASE("TutorialDefinition default values are sensible", "[tutorial][types]") {
  TutorialDefinition tutorial;

  REQUIRE(tutorial.id.empty());
  REQUIRE(tutorial.title.empty());
  REQUIRE(tutorial.level == TutorialLevel::Beginner);
  REQUIRE(tutorial.trigger == TutorialTrigger::Manual);
  REQUIRE(tutorial.steps.empty());
  REQUIRE(tutorial.prerequisiteTutorials.empty());
  REQUIRE(tutorial.estimatedMinutes == 5);
}

TEST_CASE("TutorialProgress default values are sensible", "[tutorial][types]") {
  TutorialProgress progress;

  REQUIRE(progress.tutorialId.empty());
  REQUIRE(progress.state == TutorialState::NotStarted);
  REQUIRE(progress.currentStepIndex == 0);
  REQUIRE(progress.disabled == false);
  REQUIRE(progress.neverShowAgain == false);
}

TEST_CASE("GuidedLearningProgress default values are sensible", "[tutorial][types]") {
  GuidedLearningProgress progress;

  REQUIRE(progress.globallyDisabled == false);
  REQUIRE(progress.hintsEnabled == true);
  REQUIRE(progress.walkthroughsOnFirstRun == true);
  REQUIRE(progress.tutorials.empty());
  REQUIRE(progress.hints.empty());
}
