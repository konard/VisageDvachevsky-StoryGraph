#include <catch2/catch_test_macros.hpp>

#include "NovelMind/input/input_manager.hpp"

using namespace NovelMind;
using namespace NovelMind::input;

// ============================================================================
// Input Manager Tests (Issue #187 - P2)
// ============================================================================

TEST_CASE("InputManager - Initialization", "[input][manager]") {
  InputManager input;

  SECTION("All keys start in released state") {
    CHECK(input.isKeyDown(Key::Space) == false);
    CHECK(input.isKeyDown(Key::Enter) == false);
    CHECK(input.isKeyDown(Key::A) == false);
    CHECK(input.isKeyDown(Key::Z) == false);
    CHECK(input.isKeyDown(Key::Escape) == false);
  }

  SECTION("All mouse buttons start in released state") {
    CHECK(input.isMouseButtonDown(MouseButton::Left) == false);
    CHECK(input.isMouseButtonDown(MouseButton::Right) == false);
    CHECK(input.isMouseButtonDown(MouseButton::Middle) == false);
  }

  SECTION("Initial mouse position") {
    i32 mouseX = input.getMouseX();
    i32 mouseY = input.getMouseY();

    // Mouse position should be initialized (values may vary)
    (void)mouseX;
    (void)mouseY;
  }
}

TEST_CASE("InputManager - Key state queries", "[input][keys]") {
  InputManager input;

  SECTION("All letter keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::A));
    CHECK_FALSE(input.isKeyDown(Key::B));
    CHECK_FALSE(input.isKeyDown(Key::C));
    CHECK_FALSE(input.isKeyDown(Key::D));
    CHECK_FALSE(input.isKeyDown(Key::E));
    CHECK_FALSE(input.isKeyDown(Key::F));
    CHECK_FALSE(input.isKeyDown(Key::G));
    CHECK_FALSE(input.isKeyDown(Key::H));
    CHECK_FALSE(input.isKeyDown(Key::I));
    CHECK_FALSE(input.isKeyDown(Key::J));
    CHECK_FALSE(input.isKeyDown(Key::K));
    CHECK_FALSE(input.isKeyDown(Key::L));
    CHECK_FALSE(input.isKeyDown(Key::M));
    CHECK_FALSE(input.isKeyDown(Key::N));
    CHECK_FALSE(input.isKeyDown(Key::O));
    CHECK_FALSE(input.isKeyDown(Key::P));
    CHECK_FALSE(input.isKeyDown(Key::Q));
    CHECK_FALSE(input.isKeyDown(Key::R));
    CHECK_FALSE(input.isKeyDown(Key::S));
    CHECK_FALSE(input.isKeyDown(Key::T));
    CHECK_FALSE(input.isKeyDown(Key::U));
    CHECK_FALSE(input.isKeyDown(Key::V));
    CHECK_FALSE(input.isKeyDown(Key::W));
    CHECK_FALSE(input.isKeyDown(Key::X));
    CHECK_FALSE(input.isKeyDown(Key::Y));
    CHECK_FALSE(input.isKeyDown(Key::Z));
  }

  SECTION("All number keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::Num0));
    CHECK_FALSE(input.isKeyDown(Key::Num1));
    CHECK_FALSE(input.isKeyDown(Key::Num2));
    CHECK_FALSE(input.isKeyDown(Key::Num3));
    CHECK_FALSE(input.isKeyDown(Key::Num4));
    CHECK_FALSE(input.isKeyDown(Key::Num5));
    CHECK_FALSE(input.isKeyDown(Key::Num6));
    CHECK_FALSE(input.isKeyDown(Key::Num7));
    CHECK_FALSE(input.isKeyDown(Key::Num8));
    CHECK_FALSE(input.isKeyDown(Key::Num9));
  }

  SECTION("All function keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::F1));
    CHECK_FALSE(input.isKeyDown(Key::F2));
    CHECK_FALSE(input.isKeyDown(Key::F3));
    CHECK_FALSE(input.isKeyDown(Key::F4));
    CHECK_FALSE(input.isKeyDown(Key::F5));
    CHECK_FALSE(input.isKeyDown(Key::F6));
    CHECK_FALSE(input.isKeyDown(Key::F7));
    CHECK_FALSE(input.isKeyDown(Key::F8));
    CHECK_FALSE(input.isKeyDown(Key::F9));
    CHECK_FALSE(input.isKeyDown(Key::F10));
    CHECK_FALSE(input.isKeyDown(Key::F11));
    CHECK_FALSE(input.isKeyDown(Key::F12));
  }

  SECTION("Special keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::Space));
    CHECK_FALSE(input.isKeyDown(Key::Enter));
    CHECK_FALSE(input.isKeyDown(Key::Escape));
    CHECK_FALSE(input.isKeyDown(Key::Backspace));
    CHECK_FALSE(input.isKeyDown(Key::Tab));
  }

  SECTION("Arrow keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::Up));
    CHECK_FALSE(input.isKeyDown(Key::Down));
    CHECK_FALSE(input.isKeyDown(Key::Left));
    CHECK_FALSE(input.isKeyDown(Key::Right));
  }

  SECTION("Modifier keys can be queried") {
    CHECK_FALSE(input.isKeyDown(Key::LShift));
    CHECK_FALSE(input.isKeyDown(Key::RShift));
    CHECK_FALSE(input.isKeyDown(Key::LCtrl));
    CHECK_FALSE(input.isKeyDown(Key::RCtrl));
    CHECK_FALSE(input.isKeyDown(Key::LAlt));
    CHECK_FALSE(input.isKeyDown(Key::RAlt));
  }
}

TEST_CASE("InputManager - Key press detection", "[input][keys][pressed]") {
  InputManager input;

  SECTION("isKeyPressed returns false for non-pressed keys") {
    CHECK_FALSE(input.isKeyPressed(Key::Space));
    CHECK_FALSE(input.isKeyPressed(Key::Enter));
    CHECK_FALSE(input.isKeyPressed(Key::A));
  }

  SECTION("isKeyReleased returns false for non-released keys") {
    CHECK_FALSE(input.isKeyReleased(Key::Space));
    CHECK_FALSE(input.isKeyReleased(Key::Enter));
    CHECK_FALSE(input.isKeyReleased(Key::A));
  }
}

TEST_CASE("InputManager - Mouse button state", "[input][mouse]") {
  InputManager input;

  SECTION("Mouse button down state") {
    CHECK_FALSE(input.isMouseButtonDown(MouseButton::Left));
    CHECK_FALSE(input.isMouseButtonDown(MouseButton::Right));
    CHECK_FALSE(input.isMouseButtonDown(MouseButton::Middle));
  }

  SECTION("Mouse button pressed state") {
    CHECK_FALSE(input.isMouseButtonPressed(MouseButton::Left));
    CHECK_FALSE(input.isMouseButtonPressed(MouseButton::Right));
    CHECK_FALSE(input.isMouseButtonPressed(MouseButton::Middle));
  }

  SECTION("Mouse button released state") {
    CHECK_FALSE(input.isMouseButtonReleased(MouseButton::Left));
    CHECK_FALSE(input.isMouseButtonReleased(MouseButton::Right));
    CHECK_FALSE(input.isMouseButtonReleased(MouseButton::Middle));
  }
}

TEST_CASE("InputManager - Mouse position", "[input][mouse][position]") {
  InputManager input;

  SECTION("Get mouse X coordinate") {
    i32 x = input.getMouseX();
    // X coordinate should be a valid integer (value may vary)
    (void)x;
  }

  SECTION("Get mouse Y coordinate") {
    i32 y = input.getMouseY();
    // Y coordinate should be a valid integer (value may vary)
    (void)y;
  }

  SECTION("Mouse coordinates are independent") {
    i32 firstX = input.getMouseX();
    i32 firstY = input.getMouseY();

    // Multiple queries should be consistent
    i32 secondX = input.getMouseX();
    i32 secondY = input.getMouseY();

    CHECK(firstX == secondX);
    CHECK(firstY == secondY);
  }
}

TEST_CASE("InputManager - Text input", "[input][text]") {
  InputManager input;

  SECTION("Initial text input is empty") {
    const std::string &text = input.getTextInput();
    CHECK(text.empty());
  }

  SECTION("Start and stop text input") {
    input.startTextInput();
    // Verify no crash

    input.stopTextInput();
    // Verify no crash
  }

  SECTION("Clear text input") {
    input.clearTextInput();
    const std::string &text = input.getTextInput();
    CHECK(text.empty());
  }

  SECTION("Text input lifecycle") {
    input.startTextInput();
    input.clearTextInput();
    input.stopTextInput();
    // Verify no crash through full lifecycle
  }
}

TEST_CASE("InputManager - Update function", "[input][update]") {
  InputManager input;

  SECTION("Update does not crash") {
    input.update();
    input.update();
    input.update();
    // Verify multiple updates work
  }

  SECTION("Update maintains state") {
    input.update();

    // State should remain consistent
    CHECK_FALSE(input.isKeyDown(Key::Space));
    CHECK_FALSE(input.isMouseButtonDown(MouseButton::Left));
  }
}

TEST_CASE("InputManager - Key enumeration coverage", "[input][keys]") {
  SECTION("Unknown key is defined") {
    Key unknown = Key::Unknown;
    CHECK(unknown == Key::Unknown);
  }

  SECTION("All keyboard sections represented") {
    // Letters
    CHECK(Key::A != Key::Unknown);
    CHECK(Key::Z != Key::Unknown);

    // Numbers
    CHECK(Key::Num0 != Key::Unknown);
    CHECK(Key::Num9 != Key::Unknown);

    // Function keys
    CHECK(Key::F1 != Key::Unknown);
    CHECK(Key::F12 != Key::Unknown);

    // Special keys
    CHECK(Key::Space != Key::Unknown);
    CHECK(Key::Enter != Key::Unknown);
    CHECK(Key::Escape != Key::Unknown);
    CHECK(Key::Backspace != Key::Unknown);
    CHECK(Key::Tab != Key::Unknown);

    // Arrow keys
    CHECK(Key::Up != Key::Unknown);
    CHECK(Key::Down != Key::Unknown);
    CHECK(Key::Left != Key::Unknown);
    CHECK(Key::Right != Key::Unknown);

    // Modifiers
    CHECK(Key::LShift != Key::Unknown);
    CHECK(Key::RShift != Key::Unknown);
    CHECK(Key::LCtrl != Key::Unknown);
    CHECK(Key::RCtrl != Key::Unknown);
    CHECK(Key::LAlt != Key::Unknown);
    CHECK(Key::RAlt != Key::Unknown);
  }
}

TEST_CASE("InputManager - Mouse button enumeration", "[input][mouse]") {
  SECTION("All mouse buttons are defined") {
    CHECK(MouseButton::Left != MouseButton::Right);
    CHECK(MouseButton::Right != MouseButton::Middle);
    CHECK(MouseButton::Left != MouseButton::Middle);
  }
}

TEST_CASE("InputManager - Multiple instances", "[input][manager]") {
  SECTION("Multiple input managers can coexist") {
    InputManager input1;
    InputManager input2;

    CHECK_FALSE(input1.isKeyDown(Key::A));
    CHECK_FALSE(input2.isKeyDown(Key::A));

    CHECK_FALSE(input1.isMouseButtonDown(MouseButton::Left));
    CHECK_FALSE(input2.isMouseButtonDown(MouseButton::Left));
  }
}

TEST_CASE("InputManager - Edge cases", "[input][edge_cases]") {
  InputManager input;

  SECTION("Query same key multiple times") {
    for (int i = 0; i < 10; ++i) {
      CHECK_FALSE(input.isKeyDown(Key::Space));
      CHECK_FALSE(input.isKeyPressed(Key::Space));
      CHECK_FALSE(input.isKeyReleased(Key::Space));
    }
  }

  SECTION("Query all three mouse button states") {
    CHECK_FALSE(input.isMouseButtonDown(MouseButton::Left));
    CHECK_FALSE(input.isMouseButtonPressed(MouseButton::Left));
    CHECK_FALSE(input.isMouseButtonReleased(MouseButton::Left));
  }

  SECTION("Text input state changes") {
    input.startTextInput();
    input.startTextInput(); // Start again while active
    input.stopTextInput();
    input.stopTextInput(); // Stop again while inactive
    // Verify no crash from redundant calls
  }
}
