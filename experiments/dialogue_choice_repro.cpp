// Reproduction script for issue #73: Transition from Dialogue to Choice not working
// The issue: chain dialogue -> choice -> condition auto-generated,
// but transition to choice doesn't happen, playback ends on dialogue.
// After goto node_8, the scene with choice should open, but engine doesn't transition.

#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/script_runtime.hpp"
#include <iostream>

using namespace NovelMind::scripting;

// This script mimics the issue: node_7 has dialogue and goto node_8,
// node_8 has a choice block
static const char *SCRIPT = R"(
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

int main() {
  std::cout << "=== Issue #73 Reproduction: Dialogue->Choice transition ===\n\n";

  Lexer lexer;
  auto tokens = lexer.tokenize(SCRIPT);
  if (!tokens.isOk()) {
    std::cerr << "Lexer error: " << tokens.error() << "\n";
    return 1;
  }

  Parser parser;
  auto parse = parser.parse(tokens.value());
  if (!parse.isOk()) {
    std::cerr << "Parse error: " << parse.error() << "\n";
    return 1;
  }

  Compiler compiler;
  auto compiled = compiler.compile(parse.value());
  if (!compiled.isOk()) {
    std::cerr << "Compile error: " << compiled.error() << "\n";
    return 1;
  }

  ScriptRuntime runtime;

  // Print compiled instructions for debugging
  std::cout << "\n=== Compiled Instructions ===\n";
  const auto &instrs = compiled.value().instructions;
  for (size_t i = 0; i < instrs.size(); ++i) {
    std::cout << i << ": opcode=" << static_cast<int>(instrs[i].opcode)
              << " operand=" << instrs[i].operand << "\n";
  }
  std::cout << "\n=== Scene Entry Points ===\n";
  for (const auto &[name, ep] : compiled.value().sceneEntryPoints) {
    std::cout << name << " -> " << ep << "\n";
  }
  std::cout << "\n";

  // Track all events
  runtime.setEventCallback([](const ScriptEvent &event) {
    switch (event.type) {
    case ScriptEventType::SceneChange:
      std::cout << "[Event] SceneChange: " << event.name << "\n";
      break;
    case ScriptEventType::DialogueStart:
      std::cout << "[Event] DialogueStart: speaker='" << event.name
                << "' text='" << asString(event.value) << "'\n";
      break;
    case ScriptEventType::ChoiceStart:
      std::cout << "[Event] ChoiceStart\n";
      break;
    case ScriptEventType::ChoiceSelected:
      std::cout << "[Event] ChoiceSelected: " << event.name << "\n";
      break;
    default:
      break;
    }
  });

  auto load = runtime.load(compiled.value());
  if (!load.isOk()) {
    std::cerr << "Load error: " << load.error() << "\n";
    return 1;
  }

  // Start from node_7
  auto gotoResult = runtime.gotoScene("node_7");
  if (!gotoResult.isOk()) {
    std::cerr << "Goto error: " << gotoResult.error() << "\n";
    return 1;
  }

  std::cout << "\n--- Starting execution from node_7 ---\n\n";

  // Simulate update loop
  for (int i = 0; i < 20; ++i) {
    runtime.update(0.016);
    auto state = runtime.getState();
    auto &vm = runtime.getVM();
    std::cout << "Tick " << i << ": state=" << static_cast<int>(state)
              << " scene=" << runtime.getCurrentScene()
              << " IP=" << vm.getIP()
              << " waiting=" << vm.isWaiting();

    if (state == RuntimeState::WaitingInput) {
      std::cout << " (WaitingInput - dialogue)";
    } else if (state == RuntimeState::WaitingChoice) {
      std::cout << " (WaitingChoice - choice menu)";
    } else if (state == RuntimeState::Halted) {
      std::cout << " (Halted - execution ended)";
    } else if (state == RuntimeState::Running) {
      std::cout << " (Running)";
    }
    std::cout << "\n";

    // After WaitingInput, simulate user clicking to continue from dialogue
    if (state == RuntimeState::WaitingInput) {
      std::cout << "\n>>> User clicks to continue from dialogue <<<\n\n";
      runtime.continueExecution();
    }

    // Check if we successfully reached WaitingChoice (the expected behavior)
    if (state == RuntimeState::WaitingChoice) {
      std::cout << "\n=== SUCCESS: Reached choice state! Choices: ===\n";
      for (const auto &choice : runtime.getCurrentChoices()) {
        std::cout << "  - " << choice << "\n";
      }
      break;
    }

    // Check if halted prematurely
    if (state == RuntimeState::Halted) {
      std::cout << "\n=== FAILURE: Execution halted before reaching choice! ===\n";
      std::cout << "This reproduces issue #73.\n";
      break;
    }
  }

  return 0;
}
