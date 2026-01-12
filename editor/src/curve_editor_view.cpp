#include "NovelMind/editor/curve_editor.hpp"

#include <algorithm>
#include <utility>

namespace NovelMind::editor {

// CurveEditor Implementation
// =============================================================================

CurveEditor::CurveEditor() {}

CurveEditor::~CurveEditor() = default;

void CurveEditor::update(f64 deltaTime) {
  if (m_previewPlaying) {
    m_previewTime += static_cast<f32>(deltaTime);
    if (m_previewTime > 2.0f) {
      m_previewTime = 0.0f;
    }
  }

  handleInput();
}

void CurveEditor::render() {
  // isVisible() check already handled in class definition
  if (!isVisible())
    return;

  renderGrid();
  renderCurve();
  renderPoints();
  renderHandles();

  if (m_config.showPreview) {
    renderPreview();
  }

  if (m_showPresetsPanel) {
    renderPresetsPanel();
  }
}

void CurveEditor::onResize(i32 width, i32 height) {
  m_width = width;
  m_height = height;
}

void CurveEditor::setCurve(AnimationCurve* curve) {
  m_curve = curve;
  m_selectedPoints.clear();
}

void CurveEditor::selectPoint(size_t index) {
  if (std::find(m_selectedPoints.begin(), m_selectedPoints.end(), index) ==
      m_selectedPoints.end()) {
    m_selectedPoints.push_back(index);
  }

  if (m_onPointSelected) {
    m_onPointSelected(index);
  }
}

void CurveEditor::deselectAll() {
  m_selectedPoints.clear();
}

void CurveEditor::setZoom(f32 zoom) {
  m_zoom = std::clamp(zoom, m_config.minZoom, m_config.maxZoom);
}

void CurveEditor::setPan(f32 x, f32 y) {
  m_panX = x;
  m_panY = y;
}

void CurveEditor::fitToView() {
  m_zoom = 1.0f;
  m_panX = 0.0f;
  m_panY = 0.0f;
}

void CurveEditor::resetView() {
  fitToView();
}

void CurveEditor::setConfig(const CurveEditorConfig& config) {
  m_config = config;
}

void CurveEditor::showPresetsPanel() {
  m_showPresetsPanel = true;
}

void CurveEditor::hidePresetsPanel() {
  m_showPresetsPanel = false;
}

void CurveEditor::applyPreset(const std::string& presetId) {
  if (!m_library || !m_curve)
    return;

  const auto* preset = m_library->getCurve(presetId);
  if (preset) {
    *m_curve = *preset;
    if (m_onCurveModified) {
      m_onCurveModified(*m_curve);
    }
  }
}

void CurveEditor::setOnCurveModified(std::function<void(const AnimationCurve&)> callback) {
  m_onCurveModified = std::move(callback);
}

void CurveEditor::setOnPointSelected(std::function<void(size_t)> callback) {
  m_onPointSelected = std::move(callback);
}

void CurveEditor::renderGrid() {
  // Grid rendering implementation
}

void CurveEditor::renderCurve() {
  if (!m_curve)
    return;
  // Curve rendering implementation
}

void CurveEditor::renderPoints() {
  if (!m_curve)
    return;
  // Point rendering implementation
}

void CurveEditor::renderHandles() {
  if (!m_curve)
    return;
  // Handle rendering implementation
}

void CurveEditor::renderPreview() {
  // Preview animation rendering
}

void CurveEditor::renderPresetsPanel() {
  // Presets panel rendering
}

void CurveEditor::handleInput() {
  handlePan();
  handleZoom();
  handlePointDrag();
  handleHandleDrag();
}

void CurveEditor::handlePointDrag() {
  // Point drag implementation
}

void CurveEditor::handleHandleDrag() {
  // Handle drag implementation
}

void CurveEditor::handlePan() {
  // Pan handling implementation
}

void CurveEditor::handleZoom() {
  // Zoom handling implementation
}

renderer::Vec2 CurveEditor::curveToScreen(f32 t, f32 v) const {
  f32 x = (t - m_panX) * m_zoom * static_cast<f32>(m_width);
  f32 y = static_cast<f32>(m_height) - (v - m_panY) * m_zoom * static_cast<f32>(m_height);
  return {x, y};
}

renderer::Vec2 CurveEditor::screenToCurve(f32 x, f32 y) const {
  f32 t = x / (m_zoom * static_cast<f32>(m_width)) + m_panX;
  f32 v = (static_cast<f32>(m_height) - y) / (m_zoom * static_cast<f32>(m_height)) + m_panY;
  return {t, v};
}

i32 CurveEditor::hitTestPoint(f32 x, f32 y) const {
  if (!m_curve)
    return -1;

  const auto& points = m_curve->getPoints();
  for (size_t i = 0; i < points.size(); ++i) {
    renderer::Vec2 screenPos = curveToScreen(points[i].time, points[i].value);
    f32 dx = x - screenPos.x;
    f32 dy = y - screenPos.y;
    if (dx * dx + dy * dy <= m_config.pointRadius * m_config.pointRadius) {
      return static_cast<i32>(i);
    }
  }
  return -1;
}

i32 CurveEditor::hitTestHandle(f32 x, f32 y, bool& isInHandle) const {
  (void)x;
  (void)y;
  isInHandle = false;
  return -1;
}

void CurveEditor::addPointAtScreenPos(f32 x, f32 y) {
  if (!m_curve)
    return;

  renderer::Vec2 curvePos = screenToCurve(x, y);

  CurvePoint point;
  point.time = curvePos.x;
  point.value = curvePos.y;
  m_curve->addPoint(point);

  if (m_onCurveModified) {
    m_onCurveModified(*m_curve);
  }
}

void CurveEditor::deleteSelectedPoints() {
  if (!m_curve)
    return;

  // Sort in reverse to delete from end
  std::sort(m_selectedPoints.rbegin(), m_selectedPoints.rend());

  for (size_t index : m_selectedPoints) {
    m_curve->removePoint(index);
  }

  m_selectedPoints.clear();

  if (m_onCurveModified) {
    m_onCurveModified(*m_curve);
  }
}

void CurveEditor::updatePointPosition(size_t index, f32 t, f32 v) {
  if (!m_curve)
    return;

  auto points = m_curve->getPoints();
  if (index >= points.size())
    return;

  CurvePoint point = points[index];
  point.time = t;
  point.value = v;
  m_curve->updatePoint(index, point);

  if (m_onCurveModified) {
    m_onCurveModified(*m_curve);
  }
}

void CurveEditor::updateHandlePosition(size_t index, bool isInHandle, f32 dx, f32 dy) {
  if (!m_curve)
    return;

  auto points = m_curve->getPoints();
  if (index >= points.size())
    return;

  CurvePoint point = points[index];
  if (isInHandle) {
    point.inHandleX += dx;
    point.inHandleY += dy;
  } else {
    point.outHandleX += dx;
    point.outHandleY += dy;
  }

  m_curve->updatePoint(index, point);

  if (m_onCurveModified) {
    m_onCurveModified(*m_curve);
  }
}

// =============================================================================
// InlineCurveWidget Implementation
// =============================================================================

InlineCurveWidget::InlineCurveWidget(f32 width, f32 height) : m_width(width), m_height(height) {}

void InlineCurveWidget::setCurve(const AnimationCurve& curve) {
  m_curve = curve;
}

void InlineCurveWidget::setSize(f32 width, f32 height) {
  m_width = width;
  m_height = height;
}

void InlineCurveWidget::update(f64 deltaTime) {
  (void)deltaTime;
}

void InlineCurveWidget::render(renderer::IRenderer* renderer, f32 x, f32 y) {
  (void)renderer;
  (void)x;
  (void)y;
  // Inline widget rendering implementation
}

bool InlineCurveWidget::handleClick(f32 x, f32 y) {
  (void)x;
  (void)y;
  return false;
}

bool InlineCurveWidget::handleDrag(f32 x, f32 y, f32 dx, f32 dy) {
  (void)x;
  (void)y;
  (void)dx;
  (void)dy;
  return false;
}

void InlineCurveWidget::handleRelease() {
  m_isDragging = false;
  m_selectedPoint = -1;
}

void InlineCurveWidget::setOnCurveChanged(std::function<void(const AnimationCurve&)> callback) {
  m_onCurveChanged = std::move(callback);
}

} // namespace NovelMind::editor
