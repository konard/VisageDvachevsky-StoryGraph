/**
 * @file ui_widget.cpp
 * @brief Base widget implementation
 */

#include "NovelMind/ui/ui_framework.hpp"

#include <algorithm>

namespace NovelMind::ui {

// ============================================================================
// Widget Implementation
// ============================================================================

Widget::Widget(const std::string& id) : m_id(id) {}

void Widget::setParent(Widget* parent) {
  m_parent = parent;
}

void Widget::setBounds(const Rect& bounds) {
  m_bounds = bounds;
}

void Widget::setPosition(f32 x, f32 y) {
  m_bounds.x = x;
  m_bounds.y = y;
}

void Widget::setSize(f32 width, f32 height) {
  m_bounds.width = width;
  m_bounds.height = height;
}

void Widget::setConstraints(const SizeConstraints& constraints) {
  m_constraints = constraints;
}

void Widget::setAlignment(Alignment horizontal, Alignment vertical) {
  m_horizontalAlign = horizontal;
  m_verticalAlign = vertical;
}

void Widget::setVisible(bool visible) {
  m_visible = visible;
}

void Widget::setEnabled(bool enabled) {
  m_enabled = enabled;
}

void Widget::setStyle(const Style& style) {
  m_style = style;
}

void Widget::requestFocus() {
  // Request through parent or UIManager
  m_focused = true;
}

void Widget::releaseFocus() {
  m_focused = false;
}

void Widget::on(UIEventType type, EventHandler handler) {
  m_eventHandlers[type] = std::move(handler);
}

void Widget::off(UIEventType type) {
  m_eventHandlers.erase(type);
}

void Widget::update(f64 /*deltaTime*/) {
  // Base implementation does nothing
}

void Widget::render(renderer::IRenderer& renderer) {
  if (!m_visible) {
    return;
  }

  renderBackground(renderer);
}

void Widget::layout() {
  // Base implementation does nothing
}

bool Widget::handleEvent(UIEvent& event) {
  if (!m_visible || !m_enabled) {
    return false;
  }

  // Update hover state
  if (event.type == UIEventType::MouseEnter) {
    m_hovered = true;
  } else if (event.type == UIEventType::MouseLeave) {
    m_hovered = false;
  }

  // Update pressed state
  if (event.type == UIEventType::MouseDown) {
    m_pressed = true;
  } else if (event.type == UIEventType::MouseUp) {
    m_pressed = false;
  }

  fireEvent(event);
  return event.consumed;
}

Rect Widget::measure(f32 availableWidth, f32 availableHeight) {
  f32 width = m_constraints.preferredWidth > 0 ? m_constraints.preferredWidth
                                               : std::min(availableWidth, m_constraints.maxWidth);
  f32 height = m_constraints.preferredHeight > 0
                   ? m_constraints.preferredHeight
                   : std::min(availableHeight, m_constraints.maxHeight);

  width = std::max(width, m_constraints.minWidth);
  height = std::max(height, m_constraints.minHeight);

  return {0, 0, width, height};
}

void Widget::fireEvent(UIEvent& event) {
  auto it = m_eventHandlers.find(event.type);
  if (it != m_eventHandlers.end() && it->second) {
    it->second(event);
  }
}

void Widget::renderBackground(renderer::IRenderer& renderer) {
  renderer::Color bgColor = m_style.backgroundColor;

  if (!m_enabled) {
    bgColor = m_style.disabledColor;
  } else if (m_pressed) {
    bgColor = m_style.activeColor;
  } else if (m_hovered) {
    bgColor = m_style.hoverColor;
  }

  bgColor.a = static_cast<u8>(bgColor.a * m_style.opacity);

  if (bgColor.a > 0) {
    renderer::Rect rect{m_bounds.x, m_bounds.y, m_bounds.width, m_bounds.height};
    renderer.fillRect(rect, bgColor);
  }

  // Draw border
  if (m_style.borderWidth > 0) {
    renderer::Color borderColor = m_style.borderColor;
    borderColor.a = static_cast<u8>(borderColor.a * m_style.opacity);
    // Border rendering would go here
  }
}

} // namespace NovelMind::ui
