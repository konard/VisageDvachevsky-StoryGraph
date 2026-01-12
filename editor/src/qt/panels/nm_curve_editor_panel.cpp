#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"

#include <QComboBox>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace NovelMind::editor::qt {

// =============================================================================
// CurveData Implementation
// =============================================================================

CurveData::CurveData() {
  createDefault();
}

CurvePointId CurveData::addPoint(qreal time, qreal value, CurveInterpolation interpolation) {
  CurveDataPoint point;
  point.id = m_nextId++;
  point.time = std::clamp(time, 0.0, 1.0);
  point.value = std::clamp(value, 0.0, 1.0);
  point.interpolation = interpolation;

  m_points.push_back(point);
  sortByTime();

  return point.id;
}

bool CurveData::removePoint(CurvePointId id) {
  // Don't allow removing if we'd have less than 2 points
  if (m_points.size() <= 2) {
    return false;
  }

  auto it = std::find_if(m_points.begin(), m_points.end(),
                         [id](const CurveDataPoint& p) { return p.id == id; });
  if (it != m_points.end()) {
    m_points.erase(it);
    return true;
  }
  return false;
}

bool CurveData::updatePoint(CurvePointId id, qreal time, qreal value) {
  CurveDataPoint* point = getPoint(id);
  if (point) {
    point->time = std::clamp(time, 0.0, 1.0);
    point->value = std::clamp(value, 0.0, 1.0);
    sortByTime();
    return true;
  }
  return false;
}

bool CurveData::updatePointInterpolation(CurvePointId id, CurveInterpolation interpolation) {
  CurveDataPoint* point = getPoint(id);
  if (point) {
    point->interpolation = interpolation;
    return true;
  }
  return false;
}

CurveDataPoint* CurveData::getPoint(CurvePointId id) {
  auto it = std::find_if(m_points.begin(), m_points.end(),
                         [id](const CurveDataPoint& p) { return p.id == id; });
  return (it != m_points.end()) ? &(*it) : nullptr;
}

const CurveDataPoint* CurveData::getPoint(CurvePointId id) const {
  auto it = std::find_if(m_points.begin(), m_points.end(),
                         [id](const CurveDataPoint& p) { return p.id == id; });
  return (it != m_points.end()) ? &(*it) : nullptr;
}

qreal CurveData::evaluate(qreal t) const {
  if (m_points.empty()) {
    return 0.0;
  }
  if (m_points.size() == 1) {
    return m_points[0].value;
  }

  t = std::clamp(t, 0.0, 1.0);

  // Find the segment containing t
  for (size_t i = 0; i < m_points.size() - 1; ++i) {
    const auto& p0 = m_points[i];
    const auto& p1 = m_points[i + 1];

    if (t >= p0.time && t <= p1.time) {
      qreal segmentLength = p1.time - p0.time;
      if (segmentLength < 0.0001) {
        return p0.value;
      }

      qreal localT = (t - p0.time) / segmentLength;

      // Apply interpolation based on type
      switch (p0.interpolation) {
      case CurveInterpolation::Linear:
        return p0.value + (p1.value - p0.value) * localT;

      case CurveInterpolation::EaseIn: {
        qreal easedT = localT * localT;
        return p0.value + (p1.value - p0.value) * easedT;
      }

      case CurveInterpolation::EaseOut: {
        qreal easedT = 1.0 - (1.0 - localT) * (1.0 - localT);
        return p0.value + (p1.value - p0.value) * easedT;
      }

      case CurveInterpolation::EaseInOut: {
        qreal easedT =
            localT < 0.5 ? 2.0 * localT * localT : 1.0 - std::pow(-2.0 * localT + 2.0, 2.0) / 2.0;
        return p0.value + (p1.value - p0.value) * easedT;
      }

      case CurveInterpolation::Bezier:
        // Simplified cubic bezier
        qreal t2 = localT * localT;
        qreal t3 = t2 * localT;
        qreal mt = 1.0 - localT;
        qreal mt2 = mt * mt;
        qreal mt3 = mt2 * mt;
        return mt3 * p0.value + 3.0 * mt2 * localT * p0.value + 3.0 * mt * t2 * p1.value +
               t3 * p1.value;
      }
    }
  }

  return m_points.back().value;
}

void CurveData::clear() {
  m_points.clear();
  m_nextId = 1;
}

void CurveData::createDefault() {
  clear();

  // Create default linear curve from (0,0) to (1,1)
  CurveDataPoint start;
  start.id = m_nextId++;
  start.time = 0.0;
  start.value = 0.0;
  start.interpolation = CurveInterpolation::Linear;
  m_points.push_back(start);

  CurveDataPoint end;
  end.id = m_nextId++;
  end.time = 1.0;
  end.value = 1.0;
  end.interpolation = CurveInterpolation::Linear;
  m_points.push_back(end);
}

void CurveData::sortByTime() {
  std::sort(m_points.begin(), m_points.end(),
            [](const CurveDataPoint& a, const CurveDataPoint& b) { return a.time < b.time; });
}

// =============================================================================
// NMCurveView Implementation
// =============================================================================

NMCurveView::NMCurveView(QGraphicsScene* scene, QWidget* parent) : QGraphicsView(scene, parent) {
  setRenderHint(QPainter::Antialiasing);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::StrongFocus);
  setBackgroundBrush(QBrush(QColor("#262626")));
}

void NMCurveView::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  emit viewResized();
}

void NMCurveView::mousePressEvent(QMouseEvent* event) {
  // Check if we clicked on an item
  QGraphicsItem* item = itemAt(event->pos());
  if (item) {
    // Let the item handle the event
    QGraphicsView::mousePressEvent(event);
    return;
  }

  // Pass all mouse press events to the base class
  QGraphicsView::mousePressEvent(event);
}

void NMCurveView::wheelEvent(QWheelEvent* event) {
  // Could implement zoom here if needed
  event->ignore();
}

// =============================================================================
// NMCurveEditorPanel Implementation
// =============================================================================

NMCurveEditorPanel::NMCurveEditorPanel(QWidget* parent) : NMDockPanel("Curve Editor", parent) {}

NMCurveEditorPanel::~NMCurveEditorPanel() = default;

void NMCurveEditorPanel::onInitialize() {
  setupUI();
}

void NMCurveEditorPanel::onShutdown() {}

void NMCurveEditorPanel::setupUI() {
  QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget());
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  // Create scene and view
  m_curveScene = new QGraphicsScene(this);
  m_curveView = new NMCurveView(m_curveScene, contentWidget());
  m_curveView->setObjectName("CurveView");
  mainLayout->addWidget(m_curveView, 1);

  // Connect view signals
  connect(m_curveView, &NMCurveView::viewResized, this, &NMCurveEditorPanel::onViewResized);

  // Install event filter for keyboard handling
  m_curveView->installEventFilter(this);

  // Initial rebuild
  rebuildCurveVisuals();
}

void NMCurveEditorPanel::setupToolbar() {
  m_toolbar = new QToolBar(contentWidget());
  m_toolbar->setObjectName("CurveEditorToolbar");

  auto& iconMgr = NMIconManager::instance();

  // Add Point button
  m_addPointBtn = new QPushButton(m_toolbar);
  m_addPointBtn->setIcon(iconMgr.getIcon("add", 16));
  m_addPointBtn->setToolTip(tr("Add Point (double-click on curve area)"));
  m_addPointBtn->setFlat(true);
  connect(m_addPointBtn, &QPushButton::clicked, this, &NMCurveEditorPanel::onAddPointClicked);
  m_toolbar->addWidget(m_addPointBtn);

  // Delete Point button
  m_deletePointBtn = new QPushButton(m_toolbar);
  m_deletePointBtn->setIcon(iconMgr.getIcon("delete", 16));
  m_deletePointBtn->setToolTip(tr("Delete Selected Points (Delete key)"));
  m_deletePointBtn->setFlat(true);
  connect(m_deletePointBtn, &QPushButton::clicked, this, &NMCurveEditorPanel::onDeletePointClicked);
  m_toolbar->addWidget(m_deletePointBtn);

  m_toolbar->addSeparator();

  // Interpolation combo
  m_interpCombo = new QComboBox(m_toolbar);
  m_interpCombo->addItem(tr("Linear"));
  m_interpCombo->addItem(tr("Ease In"));
  m_interpCombo->addItem(tr("Ease Out"));
  m_interpCombo->addItem(tr("Ease In/Out"));
  m_interpCombo->addItem(tr("Bezier"));
  m_interpCombo->setToolTip(tr("Interpolation Mode"));
  connect(m_interpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMCurveEditorPanel::onInterpolationChanged);
  m_toolbar->addWidget(m_interpCombo);
}

void NMCurveEditorPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // No continuous update needed
}

void NMCurveEditorPanel::setCurve(const QString& curveId) {
  m_curveId = curveId;
  m_selectedPoints.clear();
  rebuildCurveVisuals();
}

void NMCurveEditorPanel::addPointAt(qreal time, qreal value) {
  CurveInterpolation interp = static_cast<CurveInterpolation>(m_interpCombo->currentIndex());
  CurvePointId pointId = m_curveData.addPoint(time, value, interp);

  // Create undo command for adding point
  auto* cmd = new AddCurvePointCommand(this, pointId, time, value, static_cast<int>(interp));
  NMUndoManager::instance().pushCommand(cmd);

  rebuildCurveVisuals();
  emit curveChanged(m_curveId);
}

void NMCurveEditorPanel::deleteSelectedPoints() {
  if (m_selectedPoints.empty()) {
    return;
  }

  // Create macro for deleting multiple points
  if (m_selectedPoints.size() > 1) {
    NMUndoManager::instance().beginMacro("Delete Curve Points");
  }

  // Remove selected points (in reverse order to maintain valid IDs)
  for (auto it = m_selectedPoints.rbegin(); it != m_selectedPoints.rend(); ++it) {
    // Capture point state before deleting
    if (const CurveDataPoint* pt = m_curveData.getPoint(*it)) {
      CurvePointSnapshot snapshot;
      snapshot.id = pt->id;
      snapshot.time = pt->time;
      snapshot.value = pt->value;
      snapshot.interpolation = static_cast<int>(pt->interpolation);

      // Create and push delete command
      auto* cmd = new DeleteCurvePointCommand(this, snapshot);
      NMUndoManager::instance().pushCommand(cmd);
    }

    m_curveData.removePoint(*it);
  }

  if (m_selectedPoints.size() > 1) {
    NMUndoManager::instance().endMacro();
  }

  m_selectedPoints.clear();
  rebuildCurveVisuals();
  emit curveChanged(m_curveId);
}

void NMCurveEditorPanel::selectAllPoints() {
  m_selectedPoints.clear();
  for (const auto& point : m_curveData.points()) {
    m_selectedPoints.push_back(point.id);
  }

  // Update visual selection
  for (auto& [id, item] : m_pointItems) {
    item->setPointSelected(true);
  }
}

void NMCurveEditorPanel::clearSelection() {
  m_selectedPoints.clear();
  for (auto& [id, item] : m_pointItems) {
    item->setPointSelected(false);
  }
}

void NMCurveEditorPanel::onAddPointClicked() {
  // Add point in the middle of the curve
  addPointAt(0.5, 0.5);
}

void NMCurveEditorPanel::onDeletePointClicked() {
  deleteSelectedPoints();
}

void NMCurveEditorPanel::onInterpolationChanged(int index) {
  CurveInterpolation interp = static_cast<CurveInterpolation>(index);

  // Apply to all selected points
  for (CurvePointId id : m_selectedPoints) {
    m_curveData.updatePointInterpolation(id, interp);
  }

  if (!m_selectedPoints.empty()) {
    rebuildCurveVisuals();
    emit curveChanged(m_curveId);
  }
}

void NMCurveEditorPanel::onPointPositionChanged(CurvePointId id, qreal time, qreal value) {
  m_curveData.updatePoint(id, time, value);
  drawCurvePath();
  // Note: Don't emit curveChanged during drag, wait for dragFinished
}

void NMCurveEditorPanel::onPointClicked(CurvePointId id, bool additive) {
  if (!additive) {
    // Clear previous selection
    m_selectedPoints.clear();
    for (auto& [pid, item] : m_pointItems) {
      item->setPointSelected(false);
    }
  }

  // Toggle selection for this point
  auto it = std::find(m_selectedPoints.begin(), m_selectedPoints.end(), id);
  if (it != m_selectedPoints.end()) {
    if (additive) {
      m_selectedPoints.erase(it);
      if (auto itemIt = m_pointItems.find(id); itemIt != m_pointItems.end()) {
        itemIt->second->setPointSelected(false);
      }
    }
  } else {
    m_selectedPoints.push_back(id);
    if (auto itemIt = m_pointItems.find(id); itemIt != m_pointItems.end()) {
      itemIt->second->setPointSelected(true);
    }
  }

  // Capture initial position for potential drag (for undo)
  if (const CurveDataPoint* point = m_curveData.getPoint(id)) {
    m_dragStartPositions[id] = {point->time, point->value};
  }

  // Update interpolation combo to reflect selected point's interpolation
  if (m_selectedPoints.size() == 1) {
    if (const CurveDataPoint* point = m_curveData.getPoint(m_selectedPoints[0])) {
      m_interpCombo->blockSignals(true);
      m_interpCombo->setCurrentIndex(static_cast<int>(point->interpolation));
      m_interpCombo->blockSignals(false);
    }
  }
}

void NMCurveEditorPanel::onPointDragFinished(CurvePointId id) {
  // Create undo command for the move
  if (auto it = m_dragStartPositions.find(id); it != m_dragStartPositions.end()) {
    const CurveDataPoint* point = m_curveData.getPoint(id);
    if (point) {
      qreal oldTime = it->second.first;
      qreal oldValue = it->second.second;
      qreal newTime = point->time;
      qreal newValue = point->value;

      // Only create command if position actually changed
      if (oldTime != newTime || oldValue != newValue) {
        auto* cmd = new MoveCurvePointCommand(this, id, oldTime, oldValue, newTime, newValue);
        NMUndoManager::instance().pushCommand(cmd);
      }
    }

    // Clear the drag start position
    m_dragStartPositions.erase(it);
  }

  // Sort points after drag
  m_curveData.sortByTime();
  rebuildCurveVisuals();
  emit curveChanged(m_curveId);
}

void NMCurveEditorPanel::onAddPointRequested(qreal time, qreal value) {
  addPointAt(time, value);
}

void NMCurveEditorPanel::onViewResized() {
  updateSceneRect();
  rebuildCurveVisuals();
}

void NMCurveEditorPanel::rebuildCurveVisuals() {
  if (!m_curveScene || !m_curveView) {
    return;
  }

  updateSceneRect();
  drawGrid();
  drawCurvePath();
  updatePointItems();
}

void NMCurveEditorPanel::updateSceneRect() {
  if (!m_curveView) {
    return;
  }

  QSize viewSize = m_curveView->viewport()->size();
  m_curveScene->setSceneRect(0, 0, viewSize.width(), viewSize.height());
}

void NMCurveEditorPanel::drawGrid() {
  // Remove old grid lines
  for (auto* line : m_gridLines) {
    m_curveScene->removeItem(line);
    delete line;
  }
  m_gridLines.clear();

  QRectF rect = usableRect();
  QPen gridPen(QColor("#404040"), 1);
  QPen axisPen(QColor("#606060"), 2);

  // Draw vertical grid lines
  for (int i = 0; i <= GRID_DIVISIONS; ++i) {
    qreal x = rect.left() + (rect.width() * i / GRID_DIVISIONS);
    QPen pen = (i == 0 || i == GRID_DIVISIONS) ? axisPen : gridPen;
    auto* line = m_curveScene->addLine(x, rect.top(), x, rect.bottom(), pen);
    line->setZValue(-1);
    m_gridLines.push_back(line);
  }

  // Draw horizontal grid lines
  for (int i = 0; i <= GRID_DIVISIONS; ++i) {
    qreal y = rect.top() + (rect.height() * i / GRID_DIVISIONS);
    QPen pen = (i == 0 || i == GRID_DIVISIONS) ? axisPen : gridPen;
    auto* line = m_curveScene->addLine(rect.left(), y, rect.right(), y, pen);
    line->setZValue(-1);
    m_gridLines.push_back(line);
  }
}

void NMCurveEditorPanel::drawCurvePath() {
  // Remove old curve path
  if (m_curvePathItem) {
    m_curveScene->removeItem(m_curvePathItem);
    delete m_curvePathItem;
    m_curvePathItem = nullptr;
  }

  const auto& points = m_curveData.points();
  if (points.size() < 2) {
    return;
  }

  // Build the curve path by sampling
  QPainterPath curvePath;
  const int numSamples = 100;

  for (int i = 0; i <= numSamples; ++i) {
    qreal t = static_cast<qreal>(i) / numSamples;
    qreal value = m_curveData.evaluate(t);
    QPointF scenePos = normalizedToScene(t, value);

    if (i == 0) {
      curvePath.moveTo(scenePos);
    } else {
      curvePath.lineTo(scenePos);
    }
  }

  m_curvePathItem = m_curveScene->addPath(curvePath, QPen(QColor("#0078d4"), 2));
  m_curvePathItem->setZValue(0);
}

void NMCurveEditorPanel::updatePointItems() {
  // Track which IDs exist in the data model
  std::unordered_map<CurvePointId, bool> existingIds;
  for (const auto& point : m_curveData.points()) {
    existingIds[point.id] = true;
  }

  // Remove items for points that no longer exist
  for (auto it = m_pointItems.begin(); it != m_pointItems.end();) {
    if (existingIds.find(it->first) == existingIds.end()) {
      m_curveScene->removeItem(it->second);
      delete it->second;
      it = m_pointItems.erase(it);
    } else {
      ++it;
    }
  }

  // Create or update items for all points
  for (const auto& point : m_curveData.points()) {
    auto it = m_pointItems.find(point.id);
    if (it == m_pointItems.end()) {
      // Create new item
      auto* item = new NMCurvePointItem(point.id, point.time, point.value);
      item->setCoordinateConverter([this](qreal t, qreal v) { return normalizedToScene(t, v); },
                                   [this](QPointF pos) { return sceneToNormalized(pos); });

      // Connect signals
      connect(item, &NMCurvePointItem::positionChanged, this,
              &NMCurveEditorPanel::onPointPositionChanged);
      connect(item, &NMCurvePointItem::clicked, this, &NMCurveEditorPanel::onPointClicked);
      connect(item, &NMCurvePointItem::dragFinished, this,
              &NMCurveEditorPanel::onPointDragFinished);

      item->updatePositionFromNormalized();
      m_curveScene->addItem(item);
      m_pointItems[point.id] = item;

      // Restore selection state
      bool isSelected = std::find(m_selectedPoints.begin(), m_selectedPoints.end(), point.id) !=
                        m_selectedPoints.end();
      item->setPointSelected(isSelected);
    } else {
      // Update existing item
      NMCurvePointItem* item = it->second;
      item->setCoordinateConverter([this](qreal t, qreal v) { return normalizedToScene(t, v); },
                                   [this](QPointF pos) { return sceneToNormalized(pos); });

      // Update position (time/value might have changed after sort)
      if (const CurveDataPoint* p = m_curveData.getPoint(point.id)) {
        if (item->time() != p->time || item->value() != p->value) {
          // Directly update without triggering signals
          item->setTime(p->time);
          item->setValue(p->value);
        }
      }
      item->updatePositionFromNormalized();
    }
  }
}

QPointF NMCurveEditorPanel::normalizedToScene(qreal time, qreal value) const {
  QRectF rect = usableRect();
  // time: 0 -> left, 1 -> right
  // value: 0 -> bottom, 1 -> top (inverted Y)
  qreal x = rect.left() + time * rect.width();
  qreal y = rect.bottom() - value * rect.height();
  return QPointF(x, y);
}

QPointF NMCurveEditorPanel::sceneToNormalized(QPointF scenePos) const {
  QRectF rect = usableRect();
  qreal time = (scenePos.x() - rect.left()) / rect.width();
  qreal value = (rect.bottom() - scenePos.y()) / rect.height();
  return QPointF(time, value);
}

QRectF NMCurveEditorPanel::usableRect() const {
  if (!m_curveScene) {
    return QRectF();
  }
  QRectF sceneRect = m_curveScene->sceneRect();
  return sceneRect.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN);
}

} // namespace NovelMind::editor::qt
