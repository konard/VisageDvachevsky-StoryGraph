/**
 * @file ui_manager.cpp
 * @brief UI manager implementation
 */

#include "NovelMind/ui/ui_framework.hpp"

#include <algorithm>

namespace NovelMind::ui {

// ============================================================================
// UIManager Implementation
// ============================================================================

UIManager::UIManager() {
  m_theme = Theme::createDarkTheme();
}

UIManager::~UIManager() = default;

void UIManager::setRoot(std::shared_ptr<Widget> root) {
  m_root = std::move(root);
  m_layoutDirty = true;
}

void UIManager::setTheme(const Theme& theme) {
  m_theme = theme;
}

void UIManager::setFocus(Widget* widget) {
  if (m_focusedWidget != widget) {
    if (m_focusedWidget) {
      m_focusedWidget->releaseFocus();
      UIEvent blurEvent;
      blurEvent.type = UIEventType::Blur;
      m_focusedWidget->handleEvent(blurEvent);
    }

    m_focusedWidget = widget;

    if (m_focusedWidget) {
      m_focusedWidget->requestFocus();
      UIEvent focusEvent;
      focusEvent.type = UIEventType::Focus;
      m_focusedWidget->handleEvent(focusEvent);
    }
  }
}

void UIManager::clearFocus() {
  setFocus(nullptr);
}

void UIManager::focusNext() {
  std::vector<Widget*> focusable;
  if (m_root) {
    collectFocusableWidgets(m_root.get(), focusable);
  }

  if (focusable.empty()) {
    return;
  }

  auto it = std::find(focusable.begin(), focusable.end(), m_focusedWidget);
  if (it == focusable.end() || ++it == focusable.end()) {
    setFocus(focusable.front());
  } else {
    setFocus(*it);
  }
}

void UIManager::focusPrevious() {
  std::vector<Widget*> focusable;
  if (m_root) {
    collectFocusableWidgets(m_root.get(), focusable);
  }

  if (focusable.empty()) {
    return;
  }

  auto it = std::find(focusable.begin(), focusable.end(), m_focusedWidget);
  if (it == focusable.end() || it == focusable.begin()) {
    setFocus(focusable.back());
  } else {
    setFocus(*--it);
  }
}

void UIManager::pushModal(std::shared_ptr<Widget> modal) {
  m_modalStack.push_back(std::move(modal));
}

void UIManager::popModal() {
  if (!m_modalStack.empty()) {
    m_modalStack.pop_back();
  }
}

void UIManager::update(f64 deltaTime) {
  if (m_layoutDirty) {
    performLayout();
  }

  if (m_root) {
    m_root->update(deltaTime);
  }

  for (auto& modal : m_modalStack) {
    modal->update(deltaTime);
  }
}

void UIManager::render(renderer::IRenderer& renderer) {
  if (m_root) {
    m_root->render(renderer);
  }

  for (auto& modal : m_modalStack) {
    modal->render(renderer);
  }
}

void UIManager::handleMouseMove(f32 x, f32 y) {
  f32 deltaX = x - m_mouseX;
  f32 deltaY = y - m_mouseY;
  m_mouseX = x;
  m_mouseY = y;

  Widget* newHovered = hitTest(x, y);

  if (newHovered != m_hoveredWidget) {
    if (m_hoveredWidget) {
      UIEvent leaveEvent;
      leaveEvent.type = UIEventType::MouseLeave;
      leaveEvent.mouseX = x;
      leaveEvent.mouseY = y;
      m_hoveredWidget->handleEvent(leaveEvent);
    }

    m_hoveredWidget = newHovered;

    if (m_hoveredWidget) {
      UIEvent enterEvent;
      enterEvent.type = UIEventType::MouseEnter;
      enterEvent.mouseX = x;
      enterEvent.mouseY = y;
      m_hoveredWidget->handleEvent(enterEvent);
    }
  }

  UIEvent moveEvent;
  moveEvent.type = UIEventType::MouseMove;
  moveEvent.mouseX = x;
  moveEvent.mouseY = y;
  moveEvent.deltaX = deltaX;
  moveEvent.deltaY = deltaY;
  moveEvent.shift = m_shiftDown;
  moveEvent.ctrl = m_ctrlDown;
  moveEvent.alt = m_altDown;

  if (m_pressedWidget) {
    m_pressedWidget->handleEvent(moveEvent);
  } else if (m_hoveredWidget) {
    m_hoveredWidget->handleEvent(moveEvent);
  }
}

void UIManager::handleMouseDown(MouseButton button, f32 x, f32 y) {
  m_mouseDown[static_cast<int>(button)] = true;

  Widget* target = hitTest(x, y);

  UIEvent event;
  event.type = UIEventType::MouseDown;
  event.mouseX = x;
  event.mouseY = y;
  event.button = button;
  event.shift = m_shiftDown;
  event.ctrl = m_ctrlDown;
  event.alt = m_altDown;

  if (target) {
    m_pressedWidget = target;
    target->handleEvent(event);

    if (target->isFocusable()) {
      setFocus(target);
    }
  } else {
    clearFocus();
  }
}

void UIManager::handleMouseUp(MouseButton button, f32 x, f32 y) {
  m_mouseDown[static_cast<int>(button)] = false;

  UIEvent upEvent;
  upEvent.type = UIEventType::MouseUp;
  upEvent.mouseX = x;
  upEvent.mouseY = y;
  upEvent.button = button;
  upEvent.shift = m_shiftDown;
  upEvent.ctrl = m_ctrlDown;
  upEvent.alt = m_altDown;

  Widget* target = m_pressedWidget ? m_pressedWidget : hitTest(x, y);
  if (target) {
    target->handleEvent(upEvent);

    // Generate click event if released on same widget
    if (m_pressedWidget && m_pressedWidget == hitTest(x, y)) {
      UIEvent clickEvent;
      clickEvent.type = UIEventType::Click;
      clickEvent.mouseX = x;
      clickEvent.mouseY = y;
      clickEvent.button = button;
      clickEvent.shift = m_shiftDown;
      clickEvent.ctrl = m_ctrlDown;
      clickEvent.alt = m_altDown;
      m_pressedWidget->handleEvent(clickEvent);
    }
  }

  m_pressedWidget = nullptr;
}

void UIManager::handleMouseScroll(f32 deltaX, f32 deltaY) {
  UIEvent event;
  event.type = UIEventType::Scroll;
  event.mouseX = m_mouseX;
  event.mouseY = m_mouseY;
  event.deltaX = deltaX;
  event.deltaY = deltaY;
  event.shift = m_shiftDown;
  event.ctrl = m_ctrlDown;
  event.alt = m_altDown;

  Widget* target = hitTest(m_mouseX, m_mouseY);
  if (target) {
    target->handleEvent(event);
  }
}

void UIManager::handleKeyDown(i32 keyCode) {
  // Update modifier state
  if (keyCode == 16)
    m_shiftDown = true;
  if (keyCode == 17)
    m_ctrlDown = true;
  if (keyCode == 18)
    m_altDown = true;

  // Tab for focus navigation
  if (keyCode == 9) {
    if (m_shiftDown) {
      focusPrevious();
    } else {
      focusNext();
    }
    return;
  }

  UIEvent event;
  event.type = UIEventType::KeyDown;
  event.keyCode = keyCode;
  event.shift = m_shiftDown;
  event.ctrl = m_ctrlDown;
  event.alt = m_altDown;

  if (m_focusedWidget) {
    m_focusedWidget->handleEvent(event);
  }
}

void UIManager::handleKeyUp(i32 keyCode) {
  if (keyCode == 16)
    m_shiftDown = false;
  if (keyCode == 17)
    m_ctrlDown = false;
  if (keyCode == 18)
    m_altDown = false;

  UIEvent event;
  event.type = UIEventType::KeyUp;
  event.keyCode = keyCode;
  event.shift = m_shiftDown;
  event.ctrl = m_ctrlDown;
  event.alt = m_altDown;

  if (m_focusedWidget) {
    m_focusedWidget->handleEvent(event);
  }
}

void UIManager::handleTextInput(char character) {
  UIEvent event;
  event.type = UIEventType::KeyPress;
  event.character = character;
  event.shift = m_shiftDown;
  event.ctrl = m_ctrlDown;
  event.alt = m_altDown;

  if (m_focusedWidget) {
    m_focusedWidget->handleEvent(event);
  }
}

Widget* UIManager::hitTest(f32 x, f32 y) {
  // Check modals first (in reverse order)
  for (auto it = m_modalStack.rbegin(); it != m_modalStack.rend(); ++it) {
    if (Widget* hit = hitTestRecursive(it->get(), x, y)) {
      return hit;
    }
  }

  // Then check root
  if (m_root) {
    return hitTestRecursive(m_root.get(), x, y);
  }

  return nullptr;
}

void UIManager::invalidateLayout() {
  m_layoutDirty = true;
}

void UIManager::performLayout() {
  if (m_root) {
    m_root->layout();
  }

  for (auto& modal : m_modalStack) {
    modal->layout();
  }

  m_layoutDirty = false;
}

Widget* UIManager::hitTestRecursive(Widget* widget, f32 x, f32 y) {
  if (!widget->isVisible()) {
    return nullptr;
  }

  // Check if point is inside widget bounds
  if (!widget->getBounds().contains(x, y)) {
    return nullptr;
  }

  // Check children in reverse order (top-most first)
  if (auto* container = dynamic_cast<Container*>(widget)) {
    const auto& children = container->getChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      if (Widget* hit = hitTestRecursive(it->get(), x, y)) {
        return hit;
      }
    }
  }

  return widget;
}

void UIManager::collectFocusableWidgets(Widget* widget, std::vector<Widget*>& out) {
  if (!widget->isVisible() || !widget->isEnabled()) {
    return;
  }

  if (widget->isFocusable()) {
    out.push_back(widget);
  }

  if (auto* container = dynamic_cast<Container*>(widget)) {
    for (const auto& child : container->getChildren()) {
      collectFocusableWidgets(child.get(), out);
    }
  }
}

} // namespace NovelMind::ui
