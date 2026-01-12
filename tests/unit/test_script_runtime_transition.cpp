// Test case for Issue #73: Transition from Dialogue to Choice not working
// Tests that the ScriptRuntime properly handles dialogue → goto → choice transitions

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/script_runtime.hpp"

using namespace NovelMind::scripting;

// Test script that mimics the issue:
// node_7 has dialogue and goto node_8
// node_8 has a choice block
static const char* DIALOGUE_CHOICE_SCRIPT = R"(
scene node_7 {
    say "This is dialogue in node_7"
    goto node_8
}

scene node_8 {
    choice {
        "Option A" -> {
            say "You chose A"
        }
        "Option B" -> {
            say "You chose B"
        }
    }
}
)";

TEST_CASE("ScriptRuntime dialogue to choice transition", "[scripting][issue-73]") {
  SECTION("After dialogue, goto correctly transitions to choice scene") {
    // Compile the script
    Lexer lexer;
    auto tokens = lexer.tokenize(DIALOGUE_CHOICE_SCRIPT);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto parse = parser.parse(tokens.value());
    REQUIRE(parse.isOk());

    Compiler compiler;
    auto compiled = compiler.compile(parse.value());
    REQUIRE(compiled.isOk());

    // Set up runtime
    ScriptRuntime runtime;

    bool dialogueShown = false;
    bool choiceShown = false;
    std::string dialogueText;

    runtime.setEventCallback([&](const ScriptEvent& event) {
      if (event.type == ScriptEventType::DialogueStart) {
        dialogueShown = true;
        dialogueText = asString(event.value);
      } else if (event.type == ScriptEventType::ChoiceStart) {
        choiceShown = true;
      }
    });

    auto loadResult = runtime.load(compiled.value());
    REQUIRE(loadResult.isOk());

    // Start from node_7
    auto gotoResult = runtime.gotoScene("node_7");
    REQUIRE(gotoResult.isOk());

    // Run updates until we reach WaitingInput (dialogue)
    for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingInput; ++i) {
      runtime.update(0.016);
    }

    REQUIRE(runtime.getState() == RuntimeState::WaitingInput);
    REQUIRE(dialogueShown);
    REQUIRE(dialogueText == "This is dialogue in node_7");
    REQUIRE(runtime.getCurrentScene() == "node_7");

    // User clicks to continue from dialogue
    runtime.continueExecution();

    // Run updates until we reach WaitingChoice (choice menu)
    for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingChoice; ++i) {
      runtime.update(0.016);
    }

    // This is the key assertion for Issue #73
    // After the dialogue, the engine should transition to node_8 and show the choice
    REQUIRE(runtime.getState() == RuntimeState::WaitingChoice);
    REQUIRE(choiceShown);
    REQUIRE(runtime.getCurrentScene() == "node_8");

    // Verify the choices are correct
    auto choices = runtime.getCurrentChoices();
    REQUIRE(choices.size() == 2);
    REQUIRE(choices[0] == "Option A");
    REQUIRE(choices[1] == "Option B");
  }

  SECTION("Multiple dialogue nodes before choice work correctly") {
    const char* script = R"(
scene start {
    say "First dialogue"
    goto middle
}

scene middle {
    say "Second dialogue"
    goto end_choice
}

scene end_choice {
    choice {
        "Yes" -> { say "You said yes" }
        "No" -> { say "You said no" }
    }
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto parse = parser.parse(tokens.value());
    REQUIRE(parse.isOk());

    Compiler compiler;
    auto compiled = compiler.compile(parse.value());
    REQUIRE(compiled.isOk());

    ScriptRuntime runtime;
    REQUIRE(runtime.load(compiled.value()).isOk());
    REQUIRE(runtime.gotoScene("start").isOk());

    // First dialogue
    for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingInput; ++i) {
      runtime.update(0.016);
    }
    REQUIRE(runtime.getState() == RuntimeState::WaitingInput);
    REQUIRE(runtime.getCurrentScene() == "start");
    runtime.continueExecution();

    // Second dialogue
    for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingInput; ++i) {
      runtime.update(0.016);
    }
    REQUIRE(runtime.getState() == RuntimeState::WaitingInput);
    REQUIRE(runtime.getCurrentScene() == "middle");
    runtime.continueExecution();

    // Choice
    for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingChoice; ++i) {
      runtime.update(0.016);
    }
    REQUIRE(runtime.getState() == RuntimeState::WaitingChoice);
    REQUIRE(runtime.getCurrentScene() == "end_choice");

    auto choices = runtime.getCurrentChoices();
    REQUIRE(choices.size() == 2);
  }
}
