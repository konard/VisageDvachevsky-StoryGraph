/**
 * @file nm_bezier_curve_editor_dialog.cpp
 * @brief Implementation of the Bezier Curve Editor Dialog
 */

#include "NovelMind/editor/qt/nm_bezier_curve_editor_dialog.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QDialogButtonBox>
#include <QGraphicsSceneMouseEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <cmath>

namespace NovelMind::editor::qt {

// =============================================================================
// NMBezierCurveView Implementation
// =============================================================================

// Canvas size constant - moved here to avoid dependency on dialog class
static constexpr int BEZIER_CANVAS_SIZE = 300;

NMBezierCurveView::NMBezierCurveView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent) {
  setRenderHint(QPainter::Antialiasing);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::StrongFocus);
  setBackgroundBrush(QBrush(QColor("#262626")));
  setFixedSize(BEZIER_CANVAS_SIZE + 2, BEZIER_CANVAS_SIZE + 2);
}

void NMBezierCurveView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  emit viewResized();
}

// =============================================================================
// NMBezierControlPointItem Implementation
// =============================================================================

NMBezierControlPointItem::NMBezierControlPointItem(PointType type, qreal x,
                                                   qreal y,
                                                   QGraphicsItem *parent)
    : QGraphicsEllipseItem(-6, -6, 12, 12, parent), m_type(type) {

  setPos(x, y);
  setAcceptHoverEvents(true);
  setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable |
           QGraphicsItem::ItemSendsGeometryChanges);

  // Set colors based on point type
  if (type == ControlPoint1 || type == ControlPoint2) {
    m_normalColor = QColor("#ff9900"); // Orange for control points
    m_hoverColor = QColor("#ffbb44");
  } else {
    m_normalColor = QColor("#ffffff"); // White for start/end
    m_hoverColor = QColor("#ffffff");
    setFlag(QGraphicsItem::ItemIsMovable, false);
  }

  setBrush(QBrush(m_normalColor));
  setPen(QPen(QColor("#000000"), 1));
  setZValue(10);
}

void NMBezierControlPointItem::mousePressEvent(
    QGraphicsSceneMouseEvent *event) {
  if (!m_draggable)
    return;
  m_dragging = true;
  QGraphicsEllipseItem::mousePressEvent(event);
}

void NMBezierControlPointItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (!m_draggable || !m_dragging)
    return;

  QGraphicsEllipseItem::mouseMoveEvent(event);
  emit positionChanged(m_type, pos());
}

void NMBezierControlPointItem::mouseReleaseEvent(
    QGraphicsSceneMouseEvent *event) {
  if (m_dragging) {
    m_dragging = false;
    emit dragFinished();
  }
  QGraphicsEllipseItem::mouseReleaseEvent(event);
}

void NMBezierControlPointItem::hoverEnterEvent(
    QGraphicsSceneHoverEvent *event) {
  setBrush(QBrush(m_hoverColor));
  setCursor(Qt::SizeAllCursor);
  QGraphicsEllipseItem::hoverEnterEvent(event);
}

void NMBezierControlPointItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent *event) {
  setBrush(QBrush(m_normalColor));
  unsetCursor();
  QGraphicsEllipseItem::hoverLeaveEvent(event);
}

// =============================================================================
// NMBezierCurveEditorDialog Implementation
// =============================================================================

NMBezierCurveEditorDialog::NMBezierCurveEditorDialog(const Keyframe *keyframe,
                                                     QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Bezier Curve Editor"));
  setModal(true);
  setMinimumWidth(400);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  // Initialize control points from keyframe if available
  if (keyframe) {
    if (keyframe->easing == EasingType::Custom) {
      // Use existing handle data
      m_controlPoint1 = QPointF(keyframe->handleOutX, keyframe->handleOutY);
      m_controlPoint2 =
          QPointF(1.0f + keyframe->handleInX, 1.0f + keyframe->handleInY);
    } else {
      // Set defaults based on easing type
      switch (keyframe->easing) {
      case EasingType::EaseIn:
      case EasingType::EaseInQuad:
        m_controlPoint1 = QPointF(0.42f, 0.0f);
        m_controlPoint2 = QPointF(1.0f, 1.0f);
        break;
      case EasingType::EaseOut:
      case EasingType::EaseOutQuad:
        m_controlPoint1 = QPointF(0.0f, 0.0f);
        m_controlPoint2 = QPointF(0.58f, 1.0f);
        break;
      case EasingType::EaseInOut:
      case EasingType::EaseInOutQuad:
        m_controlPoint1 = QPointF(0.42f, 0.0f);
        m_controlPoint2 = QPointF(0.58f, 1.0f);
        break;
      default:
        m_controlPoint1 = QPointF(0.0f, 0.0f);
        m_controlPoint2 = QPointF(1.0f, 1.0f);
        break;
      }
    }
  }

  buildUi();
  updateCurveVisualization();
}

void NMBezierCurveEditorDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->setSpacing(12);

  // Curve canvas area
  auto *canvasLayout = new QHBoxLayout();
  canvasLayout->addStretch();

  m_curveScene = new QGraphicsScene(this);
  m_curveView = new NMBezierCurveView(m_curveScene, this);
  connect(m_curveView, &NMBezierCurveView::viewResized, this,
          &NMBezierCurveEditorDialog::onViewResized);
  canvasLayout->addWidget(m_curveView);
  canvasLayout->addStretch();

  mainLayout->addLayout(canvasLayout);

  // Presets group
  auto *presetsGroup = new QGroupBox(tr("Presets"), this);
  setupPresetButtons(presetsGroup);
  mainLayout->addWidget(presetsGroup);

  // Control points group
  auto *controlsGroup = new QGroupBox(tr("Control Points"), this);
  setupSpinBoxes(controlsGroup);
  mainLayout->addWidget(controlsGroup);

  // Dialog buttons
  auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
    m_result.easingType = EasingType::Custom;
    // Convert control points back to handle format
    m_result.handleOutX = static_cast<float>(m_controlPoint1.x());
    m_result.handleOutY = static_cast<float>(m_controlPoint1.y());
    m_result.handleInX = static_cast<float>(m_controlPoint2.x() - 1.0);
    m_result.handleInY = static_cast<float>(m_controlPoint2.y() - 1.0);
    accept();
  });
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);

  // Initialize scene
  m_curveScene->setSceneRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);

  // Draw grid
  QRectF rect = usableRect();
  QPen gridPen(QColor("#404040"), 1);
  QPen axisPen(QColor("#606060"), 1);

  // Grid lines
  for (int i = 0; i <= 10; ++i) {
    qreal x = rect.left() + (rect.width() * i / 10);
    qreal y = rect.top() + (rect.height() * i / 10);
    m_curveScene->addLine(x, rect.top(), x, rect.bottom(), gridPen)
        ->setZValue(-2);
    m_curveScene->addLine(rect.left(), y, rect.right(), y, gridPen)
        ->setZValue(-2);
  }

  // Axis lines (border)
  m_curveScene->addRect(rect, axisPen)->setZValue(-1);

  // Diagonal reference line (linear interpolation)
  QPen diagonalPen(QColor("#555555"), 1, Qt::DashLine);
  m_curveScene
      ->addLine(rect.left(), rect.bottom(), rect.right(), rect.top(),
                diagonalPen)
      ->setZValue(-1);

  // Create start/end point indicators (not draggable)
  QPointF startScenePos = normalizedToScene(0.0, 0.0);
  QPointF endScenePos = normalizedToScene(1.0, 1.0);

  auto *startPoint =
      new NMBezierControlPointItem(NMBezierControlPointItem::StartPoint,
                                   startScenePos.x(), startScenePos.y());
  startPoint->setDraggable(false);
  m_curveScene->addItem(startPoint);

  auto *endPoint = new NMBezierControlPointItem(
      NMBezierControlPointItem::EndPoint, endScenePos.x(), endScenePos.y());
  endPoint->setDraggable(false);
  m_curveScene->addItem(endPoint);

  // Create control point handles
  QPointF cp1Scene =
      normalizedToScene(m_controlPoint1.x(), m_controlPoint1.y());
  QPointF cp2Scene =
      normalizedToScene(m_controlPoint2.x(), m_controlPoint2.y());

  // Handle lines
  m_handleLine1 = m_curveScene->addLine(
      startScenePos.x(), startScenePos.y(), cp1Scene.x(), cp1Scene.y(),
      QPen(QColor("#888888"), 1, Qt::DashLine));
  m_handleLine1->setZValue(5);

  m_handleLine2 = m_curveScene->addLine(
      endScenePos.x(), endScenePos.y(), cp2Scene.x(), cp2Scene.y(),
      QPen(QColor("#888888"), 1, Qt::DashLine));
  m_handleLine2->setZValue(5);

  // Control points (draggable)
  m_cp1Item = new NMBezierControlPointItem(
      NMBezierControlPointItem::ControlPoint1, cp1Scene.x(), cp1Scene.y());
  connect(m_cp1Item, &NMBezierControlPointItem::positionChanged, this,
          &NMBezierCurveEditorDialog::onControlPointMoved);
  m_curveScene->addItem(m_cp1Item);

  m_cp2Item = new NMBezierControlPointItem(
      NMBezierControlPointItem::ControlPoint2, cp2Scene.x(), cp2Scene.y());
  connect(m_cp2Item, &NMBezierControlPointItem::positionChanged, this,
          &NMBezierCurveEditorDialog::onControlPointMoved);
  m_curveScene->addItem(m_cp2Item);

  // Initial curve path
  m_curvePathItem =
      m_curveScene->addPath(QPainterPath(), QPen(QColor("#33cc66"), 2.5));
  m_curvePathItem->setZValue(1);
}

void NMBezierCurveEditorDialog::setupPresetButtons(QWidget *container) {
  auto *layout = new QHBoxLayout(container);

  m_linearBtn = new QPushButton(tr("Linear"), container);
  connect(m_linearBtn, &QPushButton::clicked, this,
          &NMBezierCurveEditorDialog::onPresetLinear);
  layout->addWidget(m_linearBtn);

  m_easeInBtn = new QPushButton(tr("Ease In"), container);
  connect(m_easeInBtn, &QPushButton::clicked, this,
          &NMBezierCurveEditorDialog::onPresetEaseIn);
  layout->addWidget(m_easeInBtn);

  m_easeOutBtn = new QPushButton(tr("Ease Out"), container);
  connect(m_easeOutBtn, &QPushButton::clicked, this,
          &NMBezierCurveEditorDialog::onPresetEaseOut);
  layout->addWidget(m_easeOutBtn);

  m_easeInOutBtn = new QPushButton(tr("Ease In-Out"), container);
  connect(m_easeInOutBtn, &QPushButton::clicked, this,
          &NMBezierCurveEditorDialog::onPresetEaseInOut);
  layout->addWidget(m_easeInOutBtn);
}

void NMBezierCurveEditorDialog::setupSpinBoxes(QWidget *container) {
  auto *layout = new QGridLayout(container);

  // Control Point 1
  layout->addWidget(new QLabel(tr("Control Point 1:"), container), 0, 0);

  m_cp1XSpin = new QDoubleSpinBox(container);
  m_cp1XSpin->setRange(0.0, 1.0);
  m_cp1XSpin->setSingleStep(0.01);
  m_cp1XSpin->setDecimals(2);
  m_cp1XSpin->setValue(m_controlPoint1.x());
  connect(m_cp1XSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMBezierCurveEditorDialog::onSpinBoxChanged);
  layout->addWidget(new QLabel("X:", container), 0, 1);
  layout->addWidget(m_cp1XSpin, 0, 2);

  m_cp1YSpin = new QDoubleSpinBox(container);
  m_cp1YSpin->setRange(-0.5, 1.5); // Allow overshooting
  m_cp1YSpin->setSingleStep(0.01);
  m_cp1YSpin->setDecimals(2);
  m_cp1YSpin->setValue(m_controlPoint1.y());
  connect(m_cp1YSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMBezierCurveEditorDialog::onSpinBoxChanged);
  layout->addWidget(new QLabel("Y:", container), 0, 3);
  layout->addWidget(m_cp1YSpin, 0, 4);

  // Control Point 2
  layout->addWidget(new QLabel(tr("Control Point 2:"), container), 1, 0);

  m_cp2XSpin = new QDoubleSpinBox(container);
  m_cp2XSpin->setRange(0.0, 1.0);
  m_cp2XSpin->setSingleStep(0.01);
  m_cp2XSpin->setDecimals(2);
  m_cp2XSpin->setValue(m_controlPoint2.x());
  connect(m_cp2XSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMBezierCurveEditorDialog::onSpinBoxChanged);
  layout->addWidget(new QLabel("X:", container), 1, 1);
  layout->addWidget(m_cp2XSpin, 1, 2);

  m_cp2YSpin = new QDoubleSpinBox(container);
  m_cp2YSpin->setRange(-0.5, 1.5); // Allow overshooting
  m_cp2YSpin->setSingleStep(0.01);
  m_cp2YSpin->setDecimals(2);
  m_cp2YSpin->setValue(m_controlPoint2.y());
  connect(m_cp2YSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMBezierCurveEditorDialog::onSpinBoxChanged);
  layout->addWidget(new QLabel("Y:", container), 1, 3);
  layout->addWidget(m_cp2YSpin, 1, 4);
}

void NMBezierCurveEditorDialog::onControlPointMoved(
    NMBezierControlPointItem::PointType type, QPointF newPos) {
  QPointF normalized = sceneToNormalized(newPos);

  // Clamp X to valid range
  normalized.setX(std::clamp(normalized.x(), 0.0, 1.0));

  if (type == NMBezierControlPointItem::ControlPoint1) {
    m_controlPoint1 = normalized;
  } else if (type == NMBezierControlPointItem::ControlPoint2) {
    m_controlPoint2 = normalized;
  }

  updateSpinBoxesFromControlPoints();
  updateCurveVisualization();
}

void NMBezierCurveEditorDialog::onPresetLinear() {
  setControlPoints(QPointF(0.0, 0.0), QPointF(1.0, 1.0));
}

void NMBezierCurveEditorDialog::onPresetEaseIn() {
  setControlPoints(QPointF(0.42, 0.0), QPointF(1.0, 1.0));
}

void NMBezierCurveEditorDialog::onPresetEaseOut() {
  setControlPoints(QPointF(0.0, 0.0), QPointF(0.58, 1.0));
}

void NMBezierCurveEditorDialog::onPresetEaseInOut() {
  setControlPoints(QPointF(0.42, 0.0), QPointF(0.58, 1.0));
}

void NMBezierCurveEditorDialog::onSpinBoxChanged() {
  m_controlPoint1 = QPointF(m_cp1XSpin->value(), m_cp1YSpin->value());
  m_controlPoint2 = QPointF(m_cp2XSpin->value(), m_cp2YSpin->value());

  // Update visual positions
  QPointF cp1Scene =
      normalizedToScene(m_controlPoint1.x(), m_controlPoint1.y());
  QPointF cp2Scene =
      normalizedToScene(m_controlPoint2.x(), m_controlPoint2.y());

  m_cp1Item->setPos(cp1Scene);
  m_cp2Item->setPos(cp2Scene);

  updateCurveVisualization();
}

void NMBezierCurveEditorDialog::onViewResized() { updateCurveVisualization(); }

void NMBezierCurveEditorDialog::setControlPoints(const QPointF &cp1,
                                                 const QPointF &cp2) {
  m_controlPoint1 = cp1;
  m_controlPoint2 = cp2;

  // Update visual positions
  QPointF cp1Scene = normalizedToScene(cp1.x(), cp1.y());
  QPointF cp2Scene = normalizedToScene(cp2.x(), cp2.y());

  m_cp1Item->setPos(cp1Scene);
  m_cp2Item->setPos(cp2Scene);

  updateSpinBoxesFromControlPoints();
  updateCurveVisualization();
}

void NMBezierCurveEditorDialog::updateSpinBoxesFromControlPoints() {
  // Block signals to avoid recursion
  m_cp1XSpin->blockSignals(true);
  m_cp1YSpin->blockSignals(true);
  m_cp2XSpin->blockSignals(true);
  m_cp2YSpin->blockSignals(true);

  m_cp1XSpin->setValue(m_controlPoint1.x());
  m_cp1YSpin->setValue(m_controlPoint1.y());
  m_cp2XSpin->setValue(m_controlPoint2.x());
  m_cp2YSpin->setValue(m_controlPoint2.y());

  m_cp1XSpin->blockSignals(false);
  m_cp1YSpin->blockSignals(false);
  m_cp2XSpin->blockSignals(false);
  m_cp2YSpin->blockSignals(false);
}

void NMBezierCurveEditorDialog::updateCurveVisualization() {
  if (!m_curvePathItem)
    return;

  QRectF rect = usableRect();
  QPointF startScenePos = normalizedToScene(0.0, 0.0);
  QPointF endScenePos = normalizedToScene(1.0, 1.0);
  QPointF cp1Scene =
      normalizedToScene(m_controlPoint1.x(), m_controlPoint1.y());
  QPointF cp2Scene =
      normalizedToScene(m_controlPoint2.x(), m_controlPoint2.y());

  // Update handle lines
  m_handleLine1->setLine(startScenePos.x(), startScenePos.y(), cp1Scene.x(),
                         cp1Scene.y());
  m_handleLine2->setLine(endScenePos.x(), endScenePos.y(), cp2Scene.x(),
                         cp2Scene.y());

  // Build bezier path
  QPainterPath curvePath;
  curvePath.moveTo(startScenePos);
  curvePath.cubicTo(cp1Scene, cp2Scene, endScenePos);

  m_curvePathItem->setPath(curvePath);
}

QPointF NMBezierCurveEditorDialog::controlPoint1() const {
  return m_controlPoint1;
}

QPointF NMBezierCurveEditorDialog::controlPoint2() const {
  return m_controlPoint2;
}

QPointF NMBezierCurveEditorDialog::normalizedToScene(qreal x, qreal y) const {
  QRectF rect = usableRect();
  // x: 0 -> left, 1 -> right
  // y: 0 -> bottom, 1 -> top (inverted Y)
  qreal sceneX = rect.left() + x * rect.width();
  qreal sceneY = rect.bottom() - y * rect.height();
  return QPointF(sceneX, sceneY);
}

QPointF
NMBezierCurveEditorDialog::sceneToNormalized(const QPointF &scenePos) const {
  QRectF rect = usableRect();
  qreal x = (scenePos.x() - rect.left()) / rect.width();
  qreal y = (rect.bottom() - scenePos.y()) / rect.height();
  return QPointF(x, y);
}

QRectF NMBezierCurveEditorDialog::usableRect() const {
  return QRectF(MARGIN, MARGIN, CANVAS_SIZE - 2 * MARGIN,
                CANVAS_SIZE - 2 * MARGIN);
}

bool NMBezierCurveEditorDialog::getEasing(QWidget *parent,
                                          const Keyframe *keyframe,
                                          BezierCurveResult &outResult) {
  NMBezierCurveEditorDialog dialog(keyframe, parent);
  if (dialog.exec() == QDialog::Accepted) {
    outResult = dialog.result();
    return true;
  }
  return false;
}

} // namespace NovelMind::editor::qt
