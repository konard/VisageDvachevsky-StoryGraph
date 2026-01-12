#pragma once

#include "NovelMind/core/types.hpp"
#include <array>
#include <string>

namespace NovelMind::input {

enum class Key {
  Unknown = 0,
  Space,
  Enter,
  Escape,
  Backspace,
  Tab,
  Up,
  Down,
  Left,
  Right,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  LShift,
  RShift,
  LCtrl,
  RCtrl,
  LAlt,
  RAlt
};

enum class MouseButton { Left, Right, Middle };

class InputManager {
public:
  InputManager();
  ~InputManager();

  void update();

  [[nodiscard]] bool isKeyDown(Key key) const;
  [[nodiscard]] bool isKeyPressed(Key key) const;
  [[nodiscard]] bool isKeyReleased(Key key) const;

  [[nodiscard]] bool isMouseButtonDown(MouseButton button) const;
  [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const;
  [[nodiscard]] bool isMouseButtonReleased(MouseButton button) const;

  [[nodiscard]] i32 getMouseX() const;
  [[nodiscard]] i32 getMouseY() const;

  void startTextInput();
  void stopTextInput();
  [[nodiscard]] const std::string& getTextInput() const;
  void clearTextInput();

private:
  static constexpr size_t kKeyCount = static_cast<size_t>(Key::RAlt) + 1;
  static constexpr size_t kMouseCount = 3;

  std::array<bool, kKeyCount> m_keyDown{};
  std::array<bool, kKeyCount> m_keyPressed{};
  std::array<bool, kKeyCount> m_keyReleased{};
  std::array<bool, kMouseCount> m_mouseDown{};
  std::array<bool, kMouseCount> m_mousePressed{};
  std::array<bool, kMouseCount> m_mouseReleased{};

  i32 m_mouseX;
  i32 m_mouseY;

  bool m_textInputActive = false;
  std::string m_textInput;
};

} // namespace NovelMind::input
