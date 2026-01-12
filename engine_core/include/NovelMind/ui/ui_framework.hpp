#pragma once

/**
 * @file ui_framework.hpp
 * @brief UI Framework for NovelMind
 *
 * Provides a complete UI system for both runtime and editor:
 * - Widget hierarchy
 * - Layout system (vertical/horizontal box, grid)
 * - Event routing (mouse/keyboard)
 * - Themes and styles
 * - Animations
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/input/input_manager.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/scene/animation.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::ui {

// Forward declarations
class Widget;
class Container;
class UIManager;

/**
 * @brief Rectangle structure for bounds
 */
struct Rect {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 width = 0.0f;
  f32 height = 0.0f;

  [[nodiscard]] bool contains(f32 px, f32 py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }

  [[nodiscard]] Rect inset(f32 amount) const {
    return {x + amount, y + amount, width - 2 * amount, height - 2 * amount};
  }
};

/**
 * @brief Padding/margin structure
 */
struct Insets {
  f32 left = 0.0f;
  f32 top = 0.0f;
  f32 right = 0.0f;
  f32 bottom = 0.0f;

  static Insets all(f32 value) { return {value, value, value, value}; }
  static Insets symmetric(f32 horizontal, f32 vertical) {
    return {horizontal, vertical, horizontal, vertical};
  }
};

/**
 * @brief Size constraints for layout
 */
struct SizeConstraints {
  f32 minWidth = 0.0f;
  f32 minHeight = 0.0f;
  f32 maxWidth = std::numeric_limits<f32>::max();
  f32 maxHeight = std::numeric_limits<f32>::max();
  f32 preferredWidth = -1.0f; // -1 = auto
  f32 preferredHeight = -1.0f;
};

/**
 * @brief Alignment options
 */
enum class Alignment : u8 { Start, Center, End, Stretch };

/**
 * @brief Layout direction
 */
enum class LayoutDirection : u8 { Horizontal, Vertical };

/**
 * @brief Mouse button
 */
enum class MouseButton : u8 { Left, Right, Middle };

/**
 * @brief UI Event types
 */
enum class UIEventType : u8 {
  MouseEnter,
  MouseLeave,
  MouseMove,
  MouseDown,
  MouseUp,
  Click,
  DoubleClick,
  KeyDown,
  KeyUp,
  KeyPress,
  Focus,
  Blur,
  Scroll,
  DragStart,
  DragMove,
  DragEnd,
  Drop
};

/**
 * @brief UI Event data
 */
struct UIEvent {
  UIEventType type;
  f32 mouseX = 0.0f;
  f32 mouseY = 0.0f;
  f32 deltaX = 0.0f;
  f32 deltaY = 0.0f;
  MouseButton button = MouseButton::Left;
  i32 keyCode = 0;
  char character = 0;
  bool consumed = false;
  bool shift = false;
  bool ctrl = false;
  bool alt = false;

  void consume() { consumed = true; }
};

/**
 * @brief Style properties for widgets
 */
struct Style {
  // Colors
  renderer::Color backgroundColor{40, 40, 40, 255};
  renderer::Color foregroundColor{255, 255, 255, 255};
  renderer::Color borderColor{100, 100, 100, 255};
  renderer::Color hoverColor{60, 60, 60, 255};
  renderer::Color activeColor{80, 80, 80, 255};
  renderer::Color disabledColor{50, 50, 50, 200};
  renderer::Color accentColor{0, 120, 215, 255};

  // Border
  f32 borderWidth = 0.0f;
  f32 borderRadius = 0.0f;

  // Spacing
  Insets padding = Insets::all(8.0f);
  Insets margin = Insets::all(4.0f);

  // Typography
  std::string fontId = "default";
  f32 fontSize = 14.0f;
  renderer::Color textColor{255, 255, 255, 255};

  // Effects
  f32 opacity = 1.0f;
  bool shadow = false;
  f32 shadowOffsetX = 2.0f;
  f32 shadowOffsetY = 2.0f;
  renderer::Color shadowColor{0, 0, 0, 128};
};

/**
 * @brief Theme - collection of styles for different widget states/types
 */
class Theme {
public:
  Theme();

  void setStyle(const std::string& name, const Style& style);
  [[nodiscard]] const Style& getStyle(const std::string& name) const;
  [[nodiscard]] bool hasStyle(const std::string& name) const;

  // Predefined styles
  [[nodiscard]] const Style& getButtonStyle() const { return getStyle("button"); }
  [[nodiscard]] const Style& getLabelStyle() const { return getStyle("label"); }
  [[nodiscard]] const Style& getPanelStyle() const { return getStyle("panel"); }
  [[nodiscard]] const Style& getInputStyle() const { return getStyle("input"); }

  // Create default dark theme
  static Theme createDarkTheme();
  static Theme createLightTheme();

private:
  std::unordered_map<std::string, Style> m_styles;
  Style m_defaultStyle;
};

/**
 * @brief Base class for all UI widgets
 */
class Widget : public std::enable_shared_from_this<Widget> {
public:
  explicit Widget(const std::string& id = "");
  virtual ~Widget() = default;

  // Identity
  [[nodiscard]] const std::string& getId() const { return m_id; }
  void setId(const std::string& id) { m_id = id; }

  // Hierarchy
  void setParent(Widget* parent);
  [[nodiscard]] Widget* getParent() const { return m_parent; }

  // Geometry
  void setBounds(const Rect& bounds);
  [[nodiscard]] const Rect& getBounds() const { return m_bounds; }
  void setPosition(f32 x, f32 y);
  void setSize(f32 width, f32 height);

  // Size constraints
  void setConstraints(const SizeConstraints& constraints);
  [[nodiscard]] const SizeConstraints& getConstraints() const { return m_constraints; }

  // Layout
  void setAlignment(Alignment horizontal, Alignment vertical);
  [[nodiscard]] Alignment getHorizontalAlignment() const { return m_horizontalAlign; }
  [[nodiscard]] Alignment getVerticalAlignment() const { return m_verticalAlign; }
  void setFlexGrow(f32 grow) { m_flexGrow = grow; }
  [[nodiscard]] f32 getFlexGrow() const { return m_flexGrow; }

  // Visibility
  void setVisible(bool visible);
  [[nodiscard]] bool isVisible() const { return m_visible; }
  void setEnabled(bool enabled);
  [[nodiscard]] bool isEnabled() const { return m_enabled; }

  // Style
  void setStyle(const Style& style);
  [[nodiscard]] const Style& getStyle() const { return m_style; }
  void setStyleProperty(const std::string& property, const std::string& value);

  // Focus
  void setFocusable(bool focusable) { m_focusable = focusable; }
  [[nodiscard]] bool isFocusable() const { return m_focusable; }
  [[nodiscard]] bool isFocused() const { return m_focused; }
  void requestFocus();
  void releaseFocus();

  // State
  [[nodiscard]] bool isHovered() const { return m_hovered; }
  [[nodiscard]] bool isPressed() const { return m_pressed; }

  // Events
  using EventHandler = std::function<void(UIEvent&)>;
  void on(UIEventType type, EventHandler handler);
  void off(UIEventType type);

  // Lifecycle
  virtual void update(f64 deltaTime);
  virtual void render(renderer::IRenderer& renderer);
  virtual void layout();

  // Event handling
  virtual bool handleEvent(UIEvent& event);

  // Measurement
  [[nodiscard]] virtual Rect measure(f32 availableWidth, f32 availableHeight);

  // Tooltip
  void setTooltip(const std::string& tooltip) { m_tooltip = tooltip; }
  [[nodiscard]] const std::string& getTooltip() const { return m_tooltip; }

  // User data
  void setUserData(void* data) { m_userData = data; }
  [[nodiscard]] void* getUserData() const { return m_userData; }

protected:
  void fireEvent(UIEvent& event);
  void renderBackground(renderer::IRenderer& renderer);

  std::string m_id;
  Widget* m_parent = nullptr;
  Rect m_bounds;
  SizeConstraints m_constraints;
  Alignment m_horizontalAlign = Alignment::Start;
  Alignment m_verticalAlign = Alignment::Start;
  f32 m_flexGrow = 0.0f;

  bool m_visible = true;
  bool m_enabled = true;
  bool m_focusable = false;
  bool m_focused = false;
  bool m_hovered = false;
  bool m_pressed = false;

  Style m_style;
  std::string m_tooltip;
  void* m_userData = nullptr;

  std::unordered_map<UIEventType, EventHandler> m_eventHandlers;
};

/**
 * @brief Container widget - can hold child widgets
 */
class Container : public Widget {
public:
  explicit Container(const std::string& id = "");

  // Children management
  void addChild(std::shared_ptr<Widget> child);
  void removeChild(const std::string& id);
  void removeChild(Widget* child);
  void clearChildren();
  [[nodiscard]] Widget* findChild(const std::string& id);
  [[nodiscard]] const std::vector<std::shared_ptr<Widget>>& getChildren() const {
    return m_children;
  }

  // Layout
  void setLayoutDirection(LayoutDirection direction) { m_layoutDirection = direction; }
  [[nodiscard]] LayoutDirection getLayoutDirection() const { return m_layoutDirection; }
  void setSpacing(f32 spacing) { m_spacing = spacing; }
  [[nodiscard]] f32 getSpacing() const { return m_spacing; }

  void update(f64 deltaTime) override;
  void render(renderer::IRenderer& renderer) override;
  void layout() override;
  bool handleEvent(UIEvent& event) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

protected:
  virtual void layoutChildren();

  std::vector<std::shared_ptr<Widget>> m_children;
  LayoutDirection m_layoutDirection = LayoutDirection::Vertical;
  f32 m_spacing = 4.0f;
};

/**
 * @brief Horizontal box layout
 */
class HBox : public Container {
public:
  explicit HBox(const std::string& id = "");
  void layoutChildren() override;
};

/**
 * @brief Vertical box layout
 */
class VBox : public Container {
public:
  explicit VBox(const std::string& id = "");
  void layoutChildren() override;
};

/**
 * @brief Grid layout
 */
class Grid : public Container {
public:
  explicit Grid(const std::string& id = "");

  void setColumns(i32 cols) { m_columns = cols; }
  [[nodiscard]] i32 getColumns() const { return m_columns; }
  void setRowSpacing(f32 spacing) { m_rowSpacing = spacing; }
  void setColumnSpacing(f32 spacing) { m_columnSpacing = spacing; }

  void layoutChildren() override;

private:
  i32 m_columns = 2;
  f32 m_rowSpacing = 4.0f;
  f32 m_columnSpacing = 4.0f;
};

/**
 * @brief Label widget - displays text
 */
class Label : public Widget {
public:
  explicit Label(const std::string& text = "", const std::string& id = "");

  void setText(const std::string& text);
  [[nodiscard]] const std::string& getText() const { return m_text; }

  void setWordWrap(bool wrap) { m_wordWrap = wrap; }
  [[nodiscard]] bool getWordWrap() const { return m_wordWrap; }

  void render(renderer::IRenderer& renderer) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

private:
  std::string m_text;
  bool m_wordWrap = false;
};

/**
 * @brief Button widget
 */
class Button : public Widget {
public:
  explicit Button(const std::string& text = "", const std::string& id = "");

  void setText(const std::string& text) { m_text = text; }
  [[nodiscard]] const std::string& getText() const { return m_text; }

  void setIcon(const std::string& iconId) { m_iconId = iconId; }
  [[nodiscard]] const std::string& getIcon() const { return m_iconId; }

  using ClickHandler = std::function<void()>;
  void onClick(ClickHandler handler) { m_onClick = std::move(handler); }

  void render(renderer::IRenderer& renderer) override;
  bool handleEvent(UIEvent& event) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

private:
  std::string m_text;
  std::string m_iconId;
  ClickHandler m_onClick;
};

/**
 * @brief Text input widget
 */
class TextInput : public Widget {
public:
  explicit TextInput(const std::string& id = "");

  void setText(const std::string& text);
  [[nodiscard]] const std::string& getText() const { return m_text; }

  void setPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
  [[nodiscard]] const std::string& getPlaceholder() const { return m_placeholder; }

  void setPassword(bool password) { m_password = password; }
  [[nodiscard]] bool isPassword() const { return m_password; }

  void setMaxLength(size_t maxLength) { m_maxLength = maxLength; }
  [[nodiscard]] size_t getMaxLength() const { return m_maxLength; }

  using ChangeHandler = std::function<void(const std::string&)>;
  void onChange(ChangeHandler handler) { m_onChange = std::move(handler); }

  using SubmitHandler = std::function<void(const std::string&)>;
  void onSubmit(SubmitHandler handler) { m_onSubmit = std::move(handler); }

  void render(renderer::IRenderer& renderer) override;
  bool handleEvent(UIEvent& event) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

  // Selection helpers
  [[nodiscard]] bool hasSelection() const;
  void clearSelection();
  void setSelection(size_t start, size_t end);
  void selectAll();
  [[nodiscard]] std::string getSelectedText() const;
  void deleteSelection();

private:
  size_t getCursorPosFromX(f32 x);
  void updateSelectionFromMouse(f32 x);
  std::string m_text;
  std::string m_placeholder;
  bool m_password = false;
  size_t m_maxLength = 256;
  size_t m_cursorPos = 0;
  size_t m_selectionStart = 0;
  size_t m_selectionEnd = 0;
  bool m_isDragging = false;
  f32 m_scrollOffset = 0.0f;
  f32 m_cursorBlink = 0.0f;

  ChangeHandler m_onChange;
  SubmitHandler m_onSubmit;
};

/**
 * @brief Checkbox widget
 */
class Checkbox : public Widget {
public:
  explicit Checkbox(const std::string& label = "", const std::string& id = "");

  void setLabel(const std::string& label) { m_label = label; }
  [[nodiscard]] const std::string& getLabel() const { return m_label; }

  void setChecked(bool checked);
  [[nodiscard]] bool isChecked() const { return m_checked; }
  void toggle();

  using ChangeHandler = std::function<void(bool)>;
  void onChange(ChangeHandler handler) { m_onChange = std::move(handler); }

  void render(renderer::IRenderer& renderer) override;
  bool handleEvent(UIEvent& event) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

private:
  std::string m_label;
  bool m_checked = false;
  ChangeHandler m_onChange;
};

/**
 * @brief Slider widget
 */
class Slider : public Widget {
public:
  explicit Slider(const std::string& id = "");

  void setValue(f32 value);
  [[nodiscard]] f32 getValue() const { return m_value; }

  void setRange(f32 min, f32 max);
  [[nodiscard]] f32 getMin() const { return m_min; }
  [[nodiscard]] f32 getMax() const { return m_max; }

  void setStep(f32 step) { m_step = step; }
  [[nodiscard]] f32 getStep() const { return m_step; }

  using ChangeHandler = std::function<void(f32)>;
  void onChange(ChangeHandler handler) { m_onChange = std::move(handler); }

  void render(renderer::IRenderer& renderer) override;
  bool handleEvent(UIEvent& event) override;
  [[nodiscard]] Rect measure(f32 availableWidth, f32 availableHeight) override;

private:
  f32 m_value = 0.0f;
  f32 m_min = 0.0f;
  f32 m_max = 1.0f;
  f32 m_step = 0.0f;
  bool m_dragging = false;
  ChangeHandler m_onChange;
};

/**
 * @brief Scroll panel widget
 */
class ScrollPanel : public Container {
public:
  explicit ScrollPanel(const std::string& id = "");

  void setScrollX(f32 x);
  void setScrollY(f32 y);
  [[nodiscard]] f32 getScrollX() const { return m_scrollX; }
  [[nodiscard]] f32 getScrollY() const { return m_scrollY; }

  void setHorizontalScrollEnabled(bool enabled) { m_horizontalScroll = enabled; }
  void setVerticalScrollEnabled(bool enabled) { m_verticalScroll = enabled; }

  void render(renderer::IRenderer& renderer) override;
  bool handleEvent(UIEvent& event) override;
  void layoutChildren() override;

private:
  f32 m_scrollX = 0.0f;
  f32 m_scrollY = 0.0f;
  f32 m_contentWidth = 0.0f;
  f32 m_contentHeight = 0.0f;
  bool m_horizontalScroll = false;
  bool m_verticalScroll = true;
};

/**
 * @brief Panel widget - simple container with background
 */
class Panel : public Container {
public:
  explicit Panel(const std::string& id = "");
  void render(renderer::IRenderer& renderer) override;
};

/**
 * @brief UI Manager - manages UI hierarchy and events
 */
class UIManager {
public:
  UIManager();
  ~UIManager();

  // Root widget
  void setRoot(std::shared_ptr<Widget> root);
  [[nodiscard]] Widget* getRoot() const { return m_root.get(); }

  // Theme
  void setTheme(const Theme& theme);
  [[nodiscard]] const Theme& getTheme() const { return m_theme; }

  // Focus management
  void setFocus(Widget* widget);
  [[nodiscard]] Widget* getFocusedWidget() const { return m_focusedWidget; }
  void clearFocus();
  void focusNext();
  void focusPrevious();

  // Modal dialogs
  void pushModal(std::shared_ptr<Widget> modal);
  void popModal();
  [[nodiscard]] bool hasModal() const { return !m_modalStack.empty(); }

  // Update and render
  void update(f64 deltaTime);
  void render(renderer::IRenderer& renderer);

  // Event handling
  void handleMouseMove(f32 x, f32 y);
  void handleMouseDown(MouseButton button, f32 x, f32 y);
  void handleMouseUp(MouseButton button, f32 x, f32 y);
  void handleMouseScroll(f32 deltaX, f32 deltaY);
  void handleKeyDown(i32 keyCode);
  void handleKeyUp(i32 keyCode);
  void handleTextInput(char character);

  // Hit testing
  [[nodiscard]] Widget* hitTest(f32 x, f32 y);

  // Layout
  void invalidateLayout();
  void performLayout();

private:
  Widget* hitTestRecursive(Widget* widget, f32 x, f32 y);
  void collectFocusableWidgets(Widget* widget, std::vector<Widget*>& out);

  std::shared_ptr<Widget> m_root;
  std::vector<std::shared_ptr<Widget>> m_modalStack;
  Widget* m_focusedWidget = nullptr;
  Widget* m_hoveredWidget = nullptr;
  Widget* m_pressedWidget = nullptr;

  Theme m_theme;
  bool m_layoutDirty = true;

  // Mouse state
  f32 m_mouseX = 0.0f;
  f32 m_mouseY = 0.0f;
  bool m_mouseDown[3] = {false, false, false};

  // Keyboard modifiers
  bool m_shiftDown = false;
  bool m_ctrlDown = false;
  bool m_altDown = false;
};

} // namespace NovelMind::ui
