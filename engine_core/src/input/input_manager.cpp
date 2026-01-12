#include "NovelMind/input/input_manager.hpp"

#ifdef NOVELMIND_HAS_SDL2
#include <SDL.h>
#endif

namespace NovelMind::input {

namespace {

#ifdef NOVELMIND_HAS_SDL2
SDL_Scancode toScancode(Key key) {
  switch (key) {
  case Key::Space:
    return SDL_SCANCODE_SPACE;
  case Key::Enter:
    return SDL_SCANCODE_RETURN;
  case Key::Escape:
    return SDL_SCANCODE_ESCAPE;
  case Key::Backspace:
    return SDL_SCANCODE_BACKSPACE;
  case Key::Tab:
    return SDL_SCANCODE_TAB;
  case Key::Up:
    return SDL_SCANCODE_UP;
  case Key::Down:
    return SDL_SCANCODE_DOWN;
  case Key::Left:
    return SDL_SCANCODE_LEFT;
  case Key::Right:
    return SDL_SCANCODE_RIGHT;
  case Key::A:
    return SDL_SCANCODE_A;
  case Key::B:
    return SDL_SCANCODE_B;
  case Key::C:
    return SDL_SCANCODE_C;
  case Key::D:
    return SDL_SCANCODE_D;
  case Key::E:
    return SDL_SCANCODE_E;
  case Key::F:
    return SDL_SCANCODE_F;
  case Key::G:
    return SDL_SCANCODE_G;
  case Key::H:
    return SDL_SCANCODE_H;
  case Key::I:
    return SDL_SCANCODE_I;
  case Key::J:
    return SDL_SCANCODE_J;
  case Key::K:
    return SDL_SCANCODE_K;
  case Key::L:
    return SDL_SCANCODE_L;
  case Key::M:
    return SDL_SCANCODE_M;
  case Key::N:
    return SDL_SCANCODE_N;
  case Key::O:
    return SDL_SCANCODE_O;
  case Key::P:
    return SDL_SCANCODE_P;
  case Key::Q:
    return SDL_SCANCODE_Q;
  case Key::R:
    return SDL_SCANCODE_R;
  case Key::S:
    return SDL_SCANCODE_S;
  case Key::T:
    return SDL_SCANCODE_T;
  case Key::U:
    return SDL_SCANCODE_U;
  case Key::V:
    return SDL_SCANCODE_V;
  case Key::W:
    return SDL_SCANCODE_W;
  case Key::X:
    return SDL_SCANCODE_X;
  case Key::Y:
    return SDL_SCANCODE_Y;
  case Key::Z:
    return SDL_SCANCODE_Z;
  case Key::Num0:
    return SDL_SCANCODE_0;
  case Key::Num1:
    return SDL_SCANCODE_1;
  case Key::Num2:
    return SDL_SCANCODE_2;
  case Key::Num3:
    return SDL_SCANCODE_3;
  case Key::Num4:
    return SDL_SCANCODE_4;
  case Key::Num5:
    return SDL_SCANCODE_5;
  case Key::Num6:
    return SDL_SCANCODE_6;
  case Key::Num7:
    return SDL_SCANCODE_7;
  case Key::Num8:
    return SDL_SCANCODE_8;
  case Key::Num9:
    return SDL_SCANCODE_9;
  case Key::F1:
    return SDL_SCANCODE_F1;
  case Key::F2:
    return SDL_SCANCODE_F2;
  case Key::F3:
    return SDL_SCANCODE_F3;
  case Key::F4:
    return SDL_SCANCODE_F4;
  case Key::F5:
    return SDL_SCANCODE_F5;
  case Key::F6:
    return SDL_SCANCODE_F6;
  case Key::F7:
    return SDL_SCANCODE_F7;
  case Key::F8:
    return SDL_SCANCODE_F8;
  case Key::F9:
    return SDL_SCANCODE_F9;
  case Key::F10:
    return SDL_SCANCODE_F10;
  case Key::F11:
    return SDL_SCANCODE_F11;
  case Key::F12:
    return SDL_SCANCODE_F12;
  case Key::LShift:
    return SDL_SCANCODE_LSHIFT;
  case Key::RShift:
    return SDL_SCANCODE_RSHIFT;
  case Key::LCtrl:
    return SDL_SCANCODE_LCTRL;
  case Key::RCtrl:
    return SDL_SCANCODE_RCTRL;
  case Key::LAlt:
    return SDL_SCANCODE_LALT;
  case Key::RAlt:
    return SDL_SCANCODE_RALT;
  default:
    return SDL_SCANCODE_UNKNOWN;
  }
}
#endif

size_t mouseIndex(MouseButton button) {
  switch (button) {
  case MouseButton::Left:
    return 0;
  case MouseButton::Right:
    return 1;
  case MouseButton::Middle:
    return 2;
  }
  return 0;
}

} // namespace

InputManager::InputManager() : m_mouseX(0), m_mouseY(0) {}

InputManager::~InputManager() = default;

void InputManager::update() {
  m_keyPressed.fill(false);
  m_keyReleased.fill(false);
  m_mousePressed.fill(false);
  m_mouseReleased.fill(false);

#ifdef NOVELMIND_HAS_SDL2
  SDL_PumpEvents();
  const Uint8* keys = SDL_GetKeyboardState(nullptr);

  for (size_t i = 0; i < kKeyCount; ++i) {
    SDL_Scancode scancode = toScancode(static_cast<Key>(i));
    bool down = scancode != SDL_SCANCODE_UNKNOWN && keys[scancode] != 0;
    m_keyPressed[i] = down && !m_keyDown[i];
    m_keyReleased[i] = !down && m_keyDown[i];
    m_keyDown[i] = down;
  }

  int x = 0;
  int y = 0;
  Uint32 buttons = SDL_GetMouseState(&x, &y);
  m_mouseX = x;
  m_mouseY = y;

  auto updateButton = [&](size_t index, Uint32 mask) {
    bool down = (buttons & mask) != 0;
    m_mousePressed[index] = down && !m_mouseDown[index];
    m_mouseReleased[index] = !down && m_mouseDown[index];
    m_mouseDown[index] = down;
  };

  updateButton(0, SDL_BUTTON(SDL_BUTTON_LEFT));
  updateButton(1, SDL_BUTTON(SDL_BUTTON_RIGHT));
  updateButton(2, SDL_BUTTON(SDL_BUTTON_MIDDLE));

  if (m_textInputActive) {
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_TEXTINPUT, SDL_TEXTINPUT) > 0) {
      m_textInput += event.text.text;
    }
  }
#endif
}

bool InputManager::isKeyDown(Key key) const {
  size_t index = static_cast<size_t>(key);
  return index < kKeyCount ? m_keyDown[index] : false;
}

bool InputManager::isKeyPressed(Key key) const {
  size_t index = static_cast<size_t>(key);
  return index < kKeyCount ? m_keyPressed[index] : false;
}

bool InputManager::isKeyReleased(Key key) const {
  size_t index = static_cast<size_t>(key);
  return index < kKeyCount ? m_keyReleased[index] : false;
}

bool InputManager::isMouseButtonDown(MouseButton button) const {
  size_t index = mouseIndex(button);
  return index < kMouseCount ? m_mouseDown[index] : false;
}

bool InputManager::isMouseButtonPressed(MouseButton button) const {
  size_t index = mouseIndex(button);
  return index < kMouseCount ? m_mousePressed[index] : false;
}

bool InputManager::isMouseButtonReleased(MouseButton button) const {
  size_t index = mouseIndex(button);
  return index < kMouseCount ? m_mouseReleased[index] : false;
}

i32 InputManager::getMouseX() const {
  return m_mouseX;
}

i32 InputManager::getMouseY() const {
  return m_mouseY;
}

void InputManager::startTextInput() {
  m_textInputActive = true;
  m_textInput.clear();
#ifdef NOVELMIND_HAS_SDL2
  SDL_StartTextInput();
#endif
}

void InputManager::stopTextInput() {
  m_textInputActive = false;
#ifdef NOVELMIND_HAS_SDL2
  SDL_StopTextInput();
#endif
}

const std::string& InputManager::getTextInput() const {
  return m_textInput;
}

void InputManager::clearTextInput() {
  m_textInput.clear();
}

} // namespace NovelMind::input
