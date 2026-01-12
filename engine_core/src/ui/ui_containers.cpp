/**
 * @file ui_containers.cpp
 * @brief Container and layout widget implementations
 */

#include "NovelMind/ui/ui_framework.hpp"

#include <algorithm>

namespace NovelMind::ui {

// ============================================================================
// Container Implementation
// ============================================================================

Container::Container(const std::string& id) : Widget(id) {}

void Container::addChild(std::shared_ptr<Widget> child) {
  if (child) {
    child->setParent(this);
    m_children.push_back(std::move(child));
  }
}

void Container::removeChild(const std::string& id) {
  m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                                  [&id](const auto& child) { return child->getId() == id; }),
                   m_children.end());
}

void Container::removeChild(Widget* child) {
  m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                                  [child](const auto& c) { return c.get() == child; }),
                   m_children.end());
}

void Container::clearChildren() {
  m_children.clear();
}

Widget* Container::findChild(const std::string& id) {
  for (auto& child : m_children) {
    if (child->getId() == id) {
      return child.get();
    }

    // Recursive search in containers
    if (auto* container = dynamic_cast<Container*>(child.get())) {
      if (auto* found = container->findChild(id)) {
        return found;
      }
    }
  }
  return nullptr;
}

void Container::update(f64 deltaTime) {
  Widget::update(deltaTime);

  for (auto& child : m_children) {
    if (child->isVisible()) {
      child->update(deltaTime);
    }
  }
}

void Container::render(renderer::IRenderer& renderer) {
  if (!m_visible) {
    return;
  }

  Widget::render(renderer);

  for (auto& child : m_children) {
    if (child->isVisible()) {
      child->render(renderer);
    }
  }
}

void Container::layout() {
  layoutChildren();
}

bool Container::handleEvent(UIEvent& event) {
  if (!m_visible || !m_enabled) {
    return false;
  }

  // Reverse order for proper z-order handling
  for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
    if ((*it)->isVisible() && (*it)->handleEvent(event)) {
      return true;
    }
  }

  return Widget::handleEvent(event);
}

Rect Container::measure(f32 availableWidth, f32 availableHeight) {
  f32 contentWidth = 0.0f;
  f32 contentHeight = 0.0f;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    Rect childSize = child->measure(availableWidth, availableHeight);

    if (m_layoutDirection == LayoutDirection::Horizontal) {
      contentWidth += childSize.width + m_spacing;
      contentHeight = std::max(contentHeight, childSize.height);
    } else {
      contentWidth = std::max(contentWidth, childSize.width);
      contentHeight += childSize.height + m_spacing;
    }
  }

  // Remove extra spacing
  if (!m_children.empty()) {
    if (m_layoutDirection == LayoutDirection::Horizontal) {
      contentWidth -= m_spacing;
    } else {
      contentHeight -= m_spacing;
    }
  }

  // Add padding
  contentWidth += m_style.padding.left + m_style.padding.right;
  contentHeight += m_style.padding.top + m_style.padding.bottom;

  return {0, 0, contentWidth, contentHeight};
}

void Container::layoutChildren() {
  // Base implementation - simple stacking
  f32 x = m_bounds.x + m_style.padding.left;
  f32 y = m_bounds.y + m_style.padding.top;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    Rect measured = child->measure(m_bounds.width - m_style.padding.left - m_style.padding.right,
                                   m_bounds.height - m_style.padding.top - m_style.padding.bottom);

    child->setBounds({x, y, measured.width, measured.height});
    child->layout();

    if (m_layoutDirection == LayoutDirection::Horizontal) {
      x += measured.width + m_spacing;
    } else {
      y += measured.height + m_spacing;
    }
  }
}

// ============================================================================
// HBox Implementation
// ============================================================================

HBox::HBox(const std::string& id) : Container(id) {
  m_layoutDirection = LayoutDirection::Horizontal;
}

void HBox::layoutChildren() {
  f32 availableWidth = m_bounds.width - m_style.padding.left - m_style.padding.right;
  f32 availableHeight = m_bounds.height - m_style.padding.top - m_style.padding.bottom;

  // Calculate total flex grow
  f32 totalFlexGrow = 0.0f;
  f32 fixedWidth = 0.0f;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    if (child->getFlexGrow() > 0.0f) {
      totalFlexGrow += child->getFlexGrow();
    } else {
      Rect measured = child->measure(availableWidth, availableHeight);
      fixedWidth += measured.width;
    }
  }

  // Account for spacing
  f32 spacing =
      m_spacing * static_cast<f32>(std::count_if(m_children.begin(), m_children.end(),
                                                 [](const auto& c) { return c->isVisible(); }) -
                                   1);
  f32 flexSpace = std::max(0.0f, availableWidth - fixedWidth - spacing);

  // Layout children
  f32 x = m_bounds.x + m_style.padding.left;
  f32 y = m_bounds.y + m_style.padding.top;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    f32 childWidth;
    if (child->getFlexGrow() > 0.0f && totalFlexGrow > 0.0f) {
      childWidth = flexSpace * (child->getFlexGrow() / totalFlexGrow);
    } else {
      Rect measured = child->measure(availableWidth, availableHeight);
      childWidth = measured.width;
    }

    f32 childHeight = availableHeight;
    f32 childY = y;

    // Vertical alignment
    if (child->getVerticalAlignment() != Alignment::Stretch) {
      Rect measured = child->measure(childWidth, availableHeight);
      childHeight = measured.height;

      switch (child->getVerticalAlignment()) {
      case Alignment::Center:
        childY = y + (availableHeight - childHeight) / 2.0f;
        break;
      case Alignment::End:
        childY = y + availableHeight - childHeight;
        break;
      default:
        break;
      }
    }

    child->setBounds({x, childY, childWidth, childHeight});
    child->layout();

    x += childWidth + m_spacing;
  }
}

// ============================================================================
// VBox Implementation
// ============================================================================

VBox::VBox(const std::string& id) : Container(id) {
  m_layoutDirection = LayoutDirection::Vertical;
}

void VBox::layoutChildren() {
  f32 availableWidth = m_bounds.width - m_style.padding.left - m_style.padding.right;
  f32 availableHeight = m_bounds.height - m_style.padding.top - m_style.padding.bottom;

  // Calculate total flex grow
  f32 totalFlexGrow = 0.0f;
  f32 fixedHeight = 0.0f;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    if (child->getFlexGrow() > 0.0f) {
      totalFlexGrow += child->getFlexGrow();
    } else {
      Rect measured = child->measure(availableWidth, availableHeight);
      fixedHeight += measured.height;
    }
  }

  // Account for spacing
  f32 spacing =
      m_spacing * static_cast<f32>(std::count_if(m_children.begin(), m_children.end(),
                                                 [](const auto& c) { return c->isVisible(); }) -
                                   1);
  f32 flexSpace = std::max(0.0f, availableHeight - fixedHeight - spacing);

  // Layout children
  f32 x = m_bounds.x + m_style.padding.left;
  f32 y = m_bounds.y + m_style.padding.top;

  for (auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    f32 childHeight;
    if (child->getFlexGrow() > 0.0f && totalFlexGrow > 0.0f) {
      childHeight = flexSpace * (child->getFlexGrow() / totalFlexGrow);
    } else {
      Rect measured = child->measure(availableWidth, availableHeight);
      childHeight = measured.height;
    }

    f32 childWidth = availableWidth;
    f32 childX = x;

    // Horizontal alignment
    if (child->getHorizontalAlignment() != Alignment::Stretch) {
      Rect measured = child->measure(availableWidth, childHeight);
      childWidth = measured.width;

      switch (child->getHorizontalAlignment()) {
      case Alignment::Center:
        childX = x + (availableWidth - childWidth) / 2.0f;
        break;
      case Alignment::End:
        childX = x + availableWidth - childWidth;
        break;
      default:
        break;
      }
    }

    child->setBounds({childX, y, childWidth, childHeight});
    child->layout();

    y += childHeight + m_spacing;
  }
}

// ============================================================================
// Grid Implementation
// ============================================================================

Grid::Grid(const std::string& id) : Container(id) {}

void Grid::layoutChildren() {
  if (m_columns <= 0 || m_children.empty()) {
    return;
  }

  f32 availableWidth = m_bounds.width - m_style.padding.left - m_style.padding.right;
  f32 availableHeight = m_bounds.height - m_style.padding.top - m_style.padding.bottom;

  f32 cellWidth = (availableWidth - m_columnSpacing * static_cast<f32>(m_columns - 1)) /
                  static_cast<f32>(m_columns);

  // Calculate row heights
  std::vector<f32> rowHeights;
  size_t visibleCount = static_cast<size_t>(std::count_if(
      m_children.begin(), m_children.end(), [](const auto& c) { return c->isVisible(); }));
  i32 rows = (static_cast<i32>(visibleCount) + m_columns - 1) / m_columns;

  for (i32 row = 0; row < rows; ++row) {
    f32 maxHeight = 0.0f;
    for (i32 col = 0; col < m_columns; ++col) {
      size_t idx = static_cast<size_t>(row * m_columns + col);
      if (idx >= m_children.size() || !m_children[idx]->isVisible()) {
        continue;
      }

      Rect measured = m_children[idx]->measure(cellWidth, availableHeight);
      maxHeight = std::max(maxHeight, measured.height);
    }
    rowHeights.push_back(maxHeight);
  }

  // Layout
  f32 y = m_bounds.y + m_style.padding.top;
  size_t idx = 0;

  for (i32 row = 0; row < rows; ++row) {
    f32 x = m_bounds.x + m_style.padding.left;

    for (i32 col = 0; col < m_columns; ++col) {
      // Find next visible child
      while (idx < m_children.size() && !m_children[idx]->isVisible()) {
        ++idx;
      }

      if (idx >= m_children.size()) {
        break;
      }

      m_children[idx]->setBounds({x, y, cellWidth, rowHeights[static_cast<size_t>(row)]});
      m_children[idx]->layout();

      x += cellWidth + m_columnSpacing;
      ++idx;
    }

    y += rowHeights[static_cast<size_t>(row)] + m_rowSpacing;
  }
}

// ============================================================================
// ScrollPanel Implementation
// ============================================================================

ScrollPanel::ScrollPanel(const std::string& id) : Container(id) {}

void ScrollPanel::setScrollX(f32 x) {
  m_scrollX = std::max(0.0f, std::min(x, m_contentWidth - m_bounds.width));
}

void ScrollPanel::setScrollY(f32 y) {
  m_scrollY = std::max(0.0f, std::min(y, m_contentHeight - m_bounds.height));
}

void ScrollPanel::render(renderer::IRenderer& renderer) {
  if (!m_visible) {
    return;
  }

  // Render background
  Widget::render(renderer);

  // Clipping is handled when a ClipRect renderer extension is available.
  // The clip region would be set to m_bounds for content containment.

  // Render children with scroll offset
  for (auto& child : m_children) {
    if (child->isVisible()) {
      // Temporarily offset child position
      Rect originalBounds = child->getBounds();
      Rect scrolledBounds = originalBounds;
      scrolledBounds.x -= m_scrollX;
      scrolledBounds.y -= m_scrollY;
      child->setBounds(scrolledBounds);

      child->render(renderer);

      child->setBounds(originalBounds);
    }
  }

  // Clip region restoration is handled when available.

  // Draw scrollbars
  if (m_verticalScroll && m_contentHeight > m_bounds.height) {
    f32 scrollbarWidth = 8.0f;
    f32 scrollbarHeight = (m_bounds.height / m_contentHeight) * m_bounds.height;
    f32 scrollbarY = (m_scrollY / m_contentHeight) * m_bounds.height;

    renderer::Color scrollbarColor{100, 100, 100, 200};
    renderer::Rect scrollbarRect{m_bounds.x + m_bounds.width - scrollbarWidth,
                                 m_bounds.y + scrollbarY, scrollbarWidth, scrollbarHeight};
    renderer.fillRect(scrollbarRect, scrollbarColor);
  }
}

bool ScrollPanel::handleEvent(UIEvent& event) {
  if (event.type == UIEventType::Scroll) {
    if (m_verticalScroll) {
      setScrollY(m_scrollY - event.deltaY * 30.0f);
    }
    if (m_horizontalScroll) {
      setScrollX(m_scrollX - event.deltaX * 30.0f);
    }
    event.consume();
    return true;
  }

  // Adjust mouse coordinates for scrolling
  UIEvent adjustedEvent = event;
  adjustedEvent.mouseX += m_scrollX;
  adjustedEvent.mouseY += m_scrollY;

  return Container::handleEvent(adjustedEvent);
}

void ScrollPanel::layoutChildren() {
  Container::layoutChildren();

  // Calculate content size
  m_contentWidth = 0.0f;
  m_contentHeight = 0.0f;

  for (const auto& child : m_children) {
    if (!child->isVisible()) {
      continue;
    }

    const Rect& bounds = child->getBounds();
    m_contentWidth = std::max(m_contentWidth, bounds.x + bounds.width - m_bounds.x);
    m_contentHeight = std::max(m_contentHeight, bounds.y + bounds.height - m_bounds.y);
  }
}

// ============================================================================
// Panel Implementation
// ============================================================================

Panel::Panel(const std::string& id) : Container(id) {}

void Panel::render(renderer::IRenderer& renderer) {
  if (!m_visible) {
    return;
  }

  // Render panel background with border
  Widget::render(renderer);

  // Render children
  for (auto& child : m_children) {
    if (child->isVisible()) {
      child->render(renderer);
    }
  }
}

} // namespace NovelMind::ui
