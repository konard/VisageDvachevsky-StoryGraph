#pragma once

/**
 * @file curve_editor.hpp
 * @brief Animation Curves Editor for NovelMind
 *
 * Professional curve editor for animation easing:
 * - Bezier curve editing with control points
 * - Preset curves (Linear, Ease, etc.)
 * - Custom curve creation and saving
 * - Curve preview and testing
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/scene/animation.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief A point on the curve
 */
struct CurvePoint {
  f32 time;  // 0.0 - 1.0
  f32 value; // 0.0 - 1.0

  // Bezier handles (relative to point position)
  f32 inHandleX = -0.1f;
  f32 inHandleY = 0.0f;
  f32 outHandleX = 0.1f;
  f32 outHandleY = 0.0f;

  // Handle modes
  enum class HandleMode : u8 {
    Free,    // Handles move independently
    Aligned, // Handles are aligned (smooth)
    Vector,  // Handles are aligned and same length
    Broken   // No tangent continuity
  };
  HandleMode handleMode = HandleMode::Aligned;

  // UI state
  bool selected = false;
};

/**
 * @brief An animation curve
 */
class AnimationCurve {
public:
  AnimationCurve();
  AnimationCurve(const std::string& name);
  ~AnimationCurve() = default;

  [[nodiscard]] const std::string& getName() const { return m_name; }
  void setName(const std::string& name) { m_name = name; }

  [[nodiscard]] const std::string& getId() const { return m_id; }

  // Points
  void addPoint(const CurvePoint& point);
  void removePoint(size_t index);
  void updatePoint(size_t index, const CurvePoint& point);
  [[nodiscard]] const std::vector<CurvePoint>& getPoints() const { return m_points; }
  [[nodiscard]] size_t getPointCount() const { return m_points.size(); }

  // Evaluation
  [[nodiscard]] f32 evaluate(f32 t) const;
  [[nodiscard]] f32 evaluateDerivative(f32 t) const;

  // Sampling
  [[nodiscard]] std::vector<renderer::Vec2> sample(i32 numSamples = 100) const;

  // Presets
  static AnimationCurve createLinear();
  static AnimationCurve createEaseIn();
  static AnimationCurve createEaseOut();
  static AnimationCurve createEaseInOut();
  static AnimationCurve createEaseInQuad();
  static AnimationCurve createEaseOutQuad();
  static AnimationCurve createEaseInOutQuad();
  static AnimationCurve createEaseInCubic();
  static AnimationCurve createEaseOutCubic();
  static AnimationCurve createEaseInOutCubic();
  static AnimationCurve createEaseInBack();
  static AnimationCurve createEaseOutBack();
  static AnimationCurve createEaseInOutBack();
  static AnimationCurve createEaseInBounce();
  static AnimationCurve createEaseOutBounce();
  static AnimationCurve createEaseInOutBounce();
  static AnimationCurve createEaseInElastic();
  static AnimationCurve createEaseOutElastic();
  static AnimationCurve createEaseInOutElastic();
  static AnimationCurve createStep();
  static AnimationCurve createSmooth();

  // Validation
  [[nodiscard]] bool isValid() const;
  void normalize(); // Ensure points are in valid range and sorted

  // Serialization
  Result<void> save(const std::string& path) const;
  static Result<AnimationCurve> load(const std::string& path);
  [[nodiscard]] std::string toJson() const;
  static Result<AnimationCurve> fromJson(const std::string& json);

private:
  f32 evaluateBezierSegment(const CurvePoint& p0, const CurvePoint& p1, f32 localT) const;
  i32 findSegment(f32 t) const;

  std::string m_id;
  std::string m_name;
  std::vector<CurvePoint> m_points;
};

/**
 * @brief Curve library for managing custom curves
 */
class CurveLibrary {
public:
  CurveLibrary();
  ~CurveLibrary() = default;

  // Curve management
  void addCurve(const AnimationCurve& curve);
  void removeCurve(const std::string& id);
  void updateCurve(const std::string& id, const AnimationCurve& curve);
  [[nodiscard]] const AnimationCurve* getCurve(const std::string& id) const;
  [[nodiscard]] std::vector<std::string> getCurveIds() const;
  [[nodiscard]] std::vector<std::string> getCurveNames() const;

  // Categories
  void setCurveCategory(const std::string& curveId, const std::string& category);
  [[nodiscard]] std::vector<std::string> getCurvesInCategory(const std::string& category) const;
  [[nodiscard]] std::vector<std::string> getCategories() const;

  // Presets
  void loadPresets();
  [[nodiscard]] std::vector<std::string> getPresetIds() const;

  // Persistence
  Result<void> saveLibrary(const std::string& path);
  Result<void> loadLibrary(const std::string& path);

private:
  std::unordered_map<std::string, AnimationCurve> m_curves;
  std::unordered_map<std::string, std::string> m_curveCategories;
  std::vector<std::string> m_presetIds;
};

/**
 * @brief Curve editor widget configuration
 */
struct CurveEditorConfig {
  // Display
  bool showGrid = true;
  bool showLabels = true;
  bool showHandles = true;
  bool showPreview = true;

  // Grid
  i32 gridDivisionsX = 10;
  i32 gridDivisionsY = 10;

  // Colors (u8 values: 0-255)
  renderer::Color backgroundColor = {38, 38, 38, 255};
  renderer::Color gridColor = {64, 64, 64, 255};
  renderer::Color curveColor = {51, 204, 102, 255};
  renderer::Color pointColor = {255, 255, 255, 255};
  renderer::Color selectedPointColor = {255, 153, 0, 255};
  renderer::Color handleColor = {153, 153, 153, 255};
  renderer::Color handleLineColor = {128, 128, 128, 128};

  // Point size
  f32 pointRadius = 5.0f;
  f32 handleRadius = 3.0f;
  f32 curveThickness = 2.0f;

  // Snapping
  bool snapToGrid = false;
  f32 snapThreshold = 0.05f;

  // Zoom limits
  f32 minZoom = 0.5f;
  f32 maxZoom = 10.0f;
};

/**
 * @brief Curve Editor Panel
 *
 * Provides visual editing of animation curves:
 * - Click to add points
 * - Drag points and handles
 * - Select from preset curves
 * - Save custom curves to library
 */
class CurveEditor {
public:
  CurveEditor();
  ~CurveEditor();

  void update(f64 deltaTime);
  void render();
  void onResize(i32 width, i32 height);

  // Curve management
  void setCurve(AnimationCurve* curve);
  [[nodiscard]] AnimationCurve* getCurve() const { return m_curve; }

  void setEditingEnabled(bool enabled) { m_editingEnabled = enabled; }
  [[nodiscard]] bool isEditingEnabled() const { return m_editingEnabled; }

  // Selection
  void selectPoint(size_t index);
  void deselectAll();
  [[nodiscard]] const std::vector<size_t>& getSelectedPoints() const { return m_selectedPoints; }

  // View
  void setZoom(f32 zoom);
  [[nodiscard]] f32 getZoom() const { return m_zoom; }

  void setPan(f32 x, f32 y);
  [[nodiscard]] renderer::Vec2 getPan() const { return {m_panX, m_panY}; }

  void fitToView();
  void resetView();

  // Configuration
  void setConfig(const CurveEditorConfig& config);
  [[nodiscard]] const CurveEditorConfig& getConfig() const { return m_config; }

  // Curve library
  void setCurveLibrary(CurveLibrary* library) { m_library = library; }
  [[nodiscard]] CurveLibrary* getCurveLibrary() const { return m_library; }

  // Presets panel
  void showPresetsPanel();
  void hidePresetsPanel();
  [[nodiscard]] bool isPresetsPanelVisible() const { return m_showPresetsPanel; }

  // Apply preset
  void applyPreset(const std::string& presetId);

  // Callbacks
  void setOnCurveModified(std::function<void(const AnimationCurve&)> callback);
  void setOnPointSelected(std::function<void(size_t)> callback);

private:
  void renderGrid();
  void renderCurve();
  void renderPoints();
  void renderHandles();
  void renderPreview();
  void renderPresetsPanel();

  void handleInput();
  void handlePointDrag();
  void handleHandleDrag();
  void handlePan();
  void handleZoom();

  // Coordinate conversion
  renderer::Vec2 curveToScreen(f32 t, f32 v) const;
  renderer::Vec2 screenToCurve(f32 x, f32 y) const;

  // Hit testing
  i32 hitTestPoint(f32 x, f32 y) const;
  i32 hitTestHandle(f32 x, f32 y, bool& isInHandle) const;

  // Point manipulation
  void addPointAtScreenPos(f32 x, f32 y);
  void deleteSelectedPoints();
  void updatePointPosition(size_t index, f32 t, f32 v);
  void updateHandlePosition(size_t index, bool isInHandle, f32 dx, f32 dy);

  AnimationCurve* m_curve = nullptr;
  CurveLibrary* m_library = nullptr;
  CurveEditorConfig m_config;

  bool m_editingEnabled = true;

  // View state
  f32 m_zoom = 1.0f;
  f32 m_panX = 0.0f;
  f32 m_panY = 0.0f;
  i32 m_width = 800;
  i32 m_height = 600;

  // Selection
  std::vector<size_t> m_selectedPoints;

  // UI state
  bool m_showPresetsPanel = false;
  std::string m_hoveredPreset;

  // Preview animation
  f32 m_previewTime = 0.0f;
  bool m_previewPlaying = false;

  // Panel state (would come from EditorPanel base class)
  bool m_visible = true;

  bool isVisible() const { return m_visible; }

  // Callbacks
  std::function<void(const AnimationCurve&)> m_onCurveModified;
  std::function<void(size_t)> m_onPointSelected;
};

/**
 * @brief Inline curve editor widget (embeddable in other panels)
 */
class InlineCurveWidget {
public:
  InlineCurveWidget(f32 width, f32 height);
  ~InlineCurveWidget() = default;

  void setCurve(const AnimationCurve& curve);
  [[nodiscard]] const AnimationCurve& getCurve() const { return m_curve; }

  void setSize(f32 width, f32 height);

  void update(f64 deltaTime);
  void render(renderer::IRenderer* renderer, f32 x, f32 y);

  bool handleClick(f32 x, f32 y);
  bool handleDrag(f32 x, f32 y, f32 dx, f32 dy);
  void handleRelease();

  void setOnCurveChanged(std::function<void(const AnimationCurve&)> callback);

private:
  AnimationCurve m_curve;
  f32 m_width;
  f32 m_height;

  i32 m_selectedPoint = -1;
  bool m_isDragging = false;

  std::function<void(const AnimationCurve&)> m_onCurveChanged;
};

} // namespace NovelMind::editor
