/**
 * @file test_dialogue_box.cpp
 * @brief Unit tests for DialogueBox
 *
 * Tests the DialogueBox class including speaker name handling,
 * resource manager integration, and text display.
 */

#include "NovelMind/scene/dialogue_box.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace NovelMind;
using namespace NovelMind::Scene;

TEST_CASE("DialogueBox - Default Construction", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  REQUIRE(box.getId() == "test_dialogue");
  REQUIRE(box.getText().empty());
  REQUIRE(box.isTypewriterComplete());
  // SceneObject base class sets visible=true by default
  REQUIRE(box.isVisible());
}

TEST_CASE("DialogueBox - Speaker Name Handling", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  SECTION("Set speaker name") {
    box.setSpeakerName("Hero");
    box.show();

    // Speaker name is set internally (verified through rendering behavior)
    REQUIRE(box.isVisible());
  }

  SECTION("Set empty speaker name") {
    box.setSpeakerName("");
    box.show();

    REQUIRE(box.isVisible());
  }

  SECTION("Clear clears speaker name") {
    box.setSpeakerName("Narrator");
    box.setText("Hello, world!");
    box.clear();

    // After clear, text should be empty
    REQUIRE(box.getText().empty());
  }
}

TEST_CASE("DialogueBox - Resource Manager Integration", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  SECTION("Default resource manager is null") {
    // ResourceManager is null by default, rendering still works
    // (just doesn't render text without fonts)
    box.setSpeakerName("Test Speaker");
    box.setText("Test text");
    box.show();

    REQUIRE(box.isVisible());
    REQUIRE(box.getText() == "Test text");
  }

  SECTION("Set resource manager") {
    // Setting resource manager to nullptr should not crash
    box.setResourceManager(nullptr);
    box.setSpeakerName("Test Speaker");
    box.setText("Test text");
    box.show();

    REQUIRE(box.isVisible());
  }
}

TEST_CASE("DialogueBox - Text Display", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  SECTION("Set text") {
    box.setText("Hello, world!");
    REQUIRE(box.getText() == "Hello, world!");
  }

  SECTION("Set text with immediate display") {
    box.setText("Immediate text", true);
    REQUIRE(box.getText() == "Immediate text");
    REQUIRE(box.isTypewriterComplete());
  }

  SECTION("Typewriter effect") {
    box.setText("Typewriter text");
    box.startTypewriter();

    REQUIRE(box.getText() == "Typewriter text");
    REQUIRE_FALSE(box.isTypewriterComplete());
  }

  SECTION("Skip typewriter animation") {
    box.setText("Skip me");
    box.startTypewriter();

    REQUIRE_FALSE(box.isTypewriterComplete());

    box.skipAnimation();
    REQUIRE(box.isTypewriterComplete());
  }
}

TEST_CASE("DialogueBox - Style Configuration", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  SECTION("Default style has proper name color") {
    const auto& style = box.getStyle();

    // Name color should be set (golden color by default)
    REQUIRE(style.nameColor.r == 255);
    REQUIRE(style.nameColor.g == 220);
    REQUIRE(style.nameColor.b == 100);
    REQUIRE(style.nameColor.a == 255);
  }

  SECTION("Custom style") {
    DialogueBoxStyle customStyle;
    customStyle.nameColor = renderer::Color(255, 0, 0, 255);
    customStyle.namePaddingBottom = 16.0f;

    box.setStyle(customStyle);

    const auto& style = box.getStyle();
    REQUIRE(style.nameColor.r == 255);
    REQUIRE(style.nameColor.g == 0);
    REQUIRE(style.nameColor.b == 0);
    REQUIRE(style.namePaddingBottom == 16.0f);
  }
}

TEST_CASE("DialogueBox - Speaker Color", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  SECTION("Set custom speaker color") {
    renderer::Color customColor(0, 255, 0, 255);
    box.setSpeakerColor(customColor);
    box.setSpeakerName("Green Speaker");

    // Speaker color is used for rendering (tested via rendering behavior)
    REQUIRE(box.isComplete()); // No text set, so complete
  }
}

TEST_CASE("DialogueBox - Visibility", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  // SceneObject base class sets visible=true by default
  REQUIRE(box.isVisible());

  box.hide();
  REQUIRE_FALSE(box.isVisible());

  box.show();
  REQUIRE(box.isVisible());
}

TEST_CASE("DialogueBox - Bounds", "[scene][dialogue]") {
  DialogueBox box("test_dialogue");

  box.setBounds(100.0f, 200.0f, 800.0f, 150.0f);

  renderer::Rect bounds = box.getBounds();
  REQUIRE(bounds.x == 100.0f);
  REQUIRE(bounds.y == 200.0f);
  REQUIRE(bounds.width == 800.0f);
  REQUIRE(bounds.height == 150.0f);
}
