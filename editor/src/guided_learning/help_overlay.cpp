/**
 * @file help_overlay.cpp
 * @brief Implementation of the Help Overlay widget
 */

#include "NovelMind/editor/guided_learning/help_overlay.hpp"
#include "NovelMind/editor/guided_learning/anchor_registry.hpp"
#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <algorithm>

namespace NovelMind::editor::guided_learning {

NMHelpOverlay::NMHelpOverlay(QWidget* parent)
    : QWidget(parent), m_spotlightOpacity(1.0), m_calloutOpacity(1.0) {
  setObjectName("NMHelpOverlay");

  // Make overlay transparent to mouse events when not showing anything
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_TranslucentBackground, true);

  // Ensure overlay is on top
  setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_ShowWithoutActivating);

  // Initialize default fonts
  m_style.titleFont = QFont("Segoe UI", 11, QFont::DemiBold);
  m_style.contentFont = QFont("Segoe UI", 10);
  m_style.buttonFont = QFont("Segoe UI", 9);

  // Position update timer for tracking anchor movement
  m_positionUpdateTimer = new QTimer(this);
  m_positionUpdateTimer->setInterval(100); // 10 FPS update
  connect(m_positionUpdateTimer, &QTimer::timeout, this, &NMHelpOverlay::updateHintPositions);

  hide();
}

NMHelpOverlay::~NMHelpOverlay() {
  // Clean up auto-hide timers
  m_autoHideTimers.clear();
}

void NMHelpOverlay::showTutorialStep(const std::string& stepId, const std::string& title,
                                     const std::string& content, const std::string& anchorId,
                                     HintType hintType, CalloutPosition position, int currentStep,
                                     int totalSteps, bool showBack, bool showSkip,
                                     bool showDontShowAgain) {
  // Get anchor rect
  auto anchorRect = NMAnchorRegistry::instance().getAnchorRect(anchorId);
  if (!anchorRect) {
    qWarning() << "Anchor not found for step:" << QString::fromStdString(stepId);
    return;
  }

  ActiveHint hint;
  hint.id = stepId;
  hint.title = title;
  hint.content = content;
  hint.type = hintType;
  hint.position = position;
  hint.targetRect = *anchorRect;

  hint.showBackButton = showBack;
  hint.showNextButton = true;
  hint.showSkipButton = showSkip;
  hint.showCloseButton = true;
  hint.showDontShowAgain = showDontShowAgain;

  hint.showStepIndicator = totalSteps > 1;
  hint.currentStep = currentStep;
  hint.totalSteps = totalSteps;

  // Calculate callout position
  QSize contentSize = calculateContentSize(hint);
  if (position == CalloutPosition::Auto) {
    position = determineAutoPosition(hint.targetRect, contentSize);
    hint.position = position;
  }
  hint.calloutRect = calculateCalloutRect(hint.targetRect, contentSize, position);

  // Remove any existing hint with the same ID
  m_activeHints.erase(std::remove_if(m_activeHints.begin(), m_activeHints.end(),
                                     [&](const ActiveHint& h) { return h.id == stepId; }),
                      m_activeHints.end());

  m_activeHints.push_back(hint);

  // Enable mouse events
  setAttribute(Qt::WA_TransparentForMouseEvents, false);

  // Resize to cover parent
  if (parentWidget()) {
    setGeometry(parentWidget()->rect());
  }

  show();
  raise();
  update();

  // Start position update timer
  m_positionUpdateTimer->start();
}

void NMHelpOverlay::showHint(const std::string& hintId, const std::string& content,
                             const std::string& anchorId, HintType hintType,
                             CalloutPosition position, bool autoHide, int autoHideMs) {
  // Get anchor rect
  auto anchorRect = NMAnchorRegistry::instance().getAnchorRect(anchorId);
  if (!anchorRect) {
    qWarning() << "Anchor not found for hint:" << QString::fromStdString(hintId);
    return;
  }

  ActiveHint hint;
  hint.id = hintId;
  hint.content = content;
  hint.type = hintType;
  hint.position = position;
  hint.targetRect = *anchorRect;

  hint.showBackButton = false;
  hint.showNextButton = false;
  hint.showSkipButton = false;
  hint.showCloseButton = true;
  hint.showDontShowAgain = false;

  hint.autoHide = autoHide;
  hint.autoHideMs = autoHideMs;

  // Calculate callout position
  QSize contentSize = calculateContentSize(hint);
  if (position == CalloutPosition::Auto) {
    position = determineAutoPosition(hint.targetRect, contentSize);
    hint.position = position;
  }
  hint.calloutRect = calculateCalloutRect(hint.targetRect, contentSize, position);

  // Remove any existing hint with the same ID
  m_activeHints.erase(std::remove_if(m_activeHints.begin(), m_activeHints.end(),
                                     [&](const ActiveHint& h) { return h.id == hintId; }),
                      m_activeHints.end());

  m_activeHints.push_back(hint);

  // For tooltips, keep mouse events transparent
  if (hintType == HintType::Tooltip) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
  } else {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
  }

  // Resize to cover parent
  if (parentWidget()) {
    setGeometry(parentWidget()->rect());
  }

  show();
  raise();
  update();

  // Start auto-hide timer if needed
  if (autoHide && autoHideMs > 0) {
    startAutoHideTimer(hintId, autoHideMs);
  }

  // Start position update timer
  m_positionUpdateTimer->start();
}

void NMHelpOverlay::hideHint(const std::string& hintId) {
  cancelAutoHideTimer(hintId);

  m_activeHints.erase(std::remove_if(m_activeHints.begin(), m_activeHints.end(),
                                     [&](const ActiveHint& h) { return h.id == hintId; }),
                      m_activeHints.end());

  if (m_activeHints.empty()) {
    hide();
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_positionUpdateTimer->stop();
  } else {
    update();
  }
}

void NMHelpOverlay::hideAll() {
  for (const auto& hint : m_activeHints) {
    cancelAutoHideTimer(hint.id);
  }
  m_activeHints.clear();
  hide();
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_positionUpdateTimer->stop();
}

bool NMHelpOverlay::hasVisibleHints() const {
  return !m_activeHints.empty();
}

bool NMHelpOverlay::isHintVisible(const std::string& hintId) const {
  return std::any_of(m_activeHints.begin(), m_activeHints.end(),
                     [&](const ActiveHint& h) { return h.id == hintId; });
}

void NMHelpOverlay::setStyle(const OverlayStyle& style) {
  m_style = style;
  update();
}

void NMHelpOverlay::setSpotlightOpacity(qreal opacity) {
  m_spotlightOpacity = opacity;
  update();
}

void NMHelpOverlay::setCalloutOpacity(qreal opacity) {
  m_calloutOpacity = opacity;
  update();
}

void NMHelpOverlay::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  if (m_activeHints.empty())
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Paint spotlight for the first hint (typically tutorial step)
  if (!m_activeHints.empty() && m_activeHints[0].type == HintType::Spotlight) {
    paintSpotlight(painter, m_activeHints[0].targetRect);
  } else if (!m_activeHints.empty() && m_activeHints[0].type == HintType::Callout) {
    // For callouts, also show a subtle spotlight
    paintSpotlight(painter, m_activeHints[0].targetRect);
  }

  // Paint all callouts/tooltips
  for (const auto& hint : m_activeHints) {
    if (hint.type == HintType::Tooltip) {
      paintTooltip(painter, hint);
    } else {
      paintCallout(painter, hint);
    }
  }
}

void NMHelpOverlay::paintSpotlight(QPainter& painter, const QRect& targetRect) {
  // Create a path that covers the entire widget except the target
  QPainterPath path;
  path.addRect(rect());

  // Create rounded rect for the target area
  QRect spotlightRect = targetRect.adjusted(-m_style.spotlightPadding, -m_style.spotlightPadding,
                                            m_style.spotlightPadding, m_style.spotlightPadding);

  QPainterPath targetPath;
  targetPath.addRoundedRect(spotlightRect, m_style.spotlightCornerRadius,
                            m_style.spotlightCornerRadius);

  // Subtract target from overlay
  path = path.subtracted(targetPath);

  // Fill with dim color
  QColor dimColor = m_style.spotlightDimColor;
  qreal newAlpha = static_cast<qreal>(dimColor.alphaF()) * m_spotlightOpacity;
  dimColor.setAlphaF(static_cast<float>(newAlpha));
  painter.fillPath(path, dimColor);

  // Draw subtle border around target
  painter.setPen(QPen(m_style.calloutBorder, 2));
  painter.drawRoundedRect(spotlightRect, m_style.spotlightCornerRadius,
                          m_style.spotlightCornerRadius);
}

void NMHelpOverlay::paintCallout(QPainter& painter, const ActiveHint& hint) {
  const QRect& calloutRect = hint.calloutRect;

  // Save painter state
  painter.save();
  painter.setOpacity(m_calloutOpacity);

  // Draw callout background
  QPainterPath bgPath;
  bgPath.addRoundedRect(calloutRect, m_style.calloutCornerRadius, m_style.calloutCornerRadius);

  painter.fillPath(bgPath, m_style.calloutBackground);
  painter.setPen(QPen(m_style.calloutBorder, 1));
  painter.drawPath(bgPath);

  // Draw arrow pointing to target
  QPoint arrowTip;
  QPolygon arrowPolygon;

  int arrowSize = m_style.calloutArrowSize;
  QRect targetRect = hint.targetRect;

  switch (hint.position) {
  case CalloutPosition::Bottom:
  case CalloutPosition::BottomLeft:
  case CalloutPosition::BottomRight:
    arrowTip = QPoint(targetRect.center().x(), targetRect.bottom());
    arrowPolygon << QPoint(arrowTip.x() - arrowSize, calloutRect.top()) << arrowTip
                 << QPoint(arrowTip.x() + arrowSize, calloutRect.top());
    break;

  case CalloutPosition::Top:
  case CalloutPosition::TopLeft:
  case CalloutPosition::TopRight:
    arrowTip = QPoint(targetRect.center().x(), targetRect.top());
    arrowPolygon << QPoint(arrowTip.x() - arrowSize, calloutRect.bottom()) << arrowTip
                 << QPoint(arrowTip.x() + arrowSize, calloutRect.bottom());
    break;

  case CalloutPosition::Left:
    arrowTip = QPoint(targetRect.left(), targetRect.center().y());
    arrowPolygon << QPoint(calloutRect.right(), arrowTip.y() - arrowSize) << arrowTip
                 << QPoint(calloutRect.right(), arrowTip.y() + arrowSize);
    break;

  case CalloutPosition::Right:
    arrowTip = QPoint(targetRect.right(), targetRect.center().y());
    arrowPolygon << QPoint(calloutRect.left(), arrowTip.y() - arrowSize) << arrowTip
                 << QPoint(calloutRect.left(), arrowTip.y() + arrowSize);
    break;

  default:
    break;
  }

  if (!arrowPolygon.isEmpty()) {
    QPainterPath arrowPath;
    arrowPath.addPolygon(arrowPolygon);
    painter.fillPath(arrowPath, m_style.calloutBackground);
    painter.setPen(QPen(m_style.calloutBorder, 1));
    painter.drawPolygon(arrowPolygon);
  }

  // Draw content
  int padding = m_style.calloutPadding;
  int y = calloutRect.top() + padding;
  int contentWidth = calloutRect.width() - 2 * padding;

  // Title
  if (!hint.title.empty()) {
    painter.setFont(m_style.titleFont);
    painter.setPen(m_style.calloutTitleText);

    QRect titleRect(calloutRect.left() + padding, y, contentWidth, 24);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromStdString(hint.title));
    y += 28;
  }

  // Content text
  painter.setFont(m_style.contentFont);
  painter.setPen(m_style.calloutText);

  QRect contentRect(calloutRect.left() + padding, y, contentWidth,
                    calloutRect.height() - (y - calloutRect.top()) - padding - 40);
  QRect boundingRect;
  painter.drawText(contentRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   QString::fromStdString(hint.content), &boundingRect);
  y = boundingRect.bottom() + 16;

  // Step indicator
  if (hint.showStepIndicator) {
    painter.setFont(m_style.contentFont);
    painter.setPen(QColor(150, 150, 150));

    QString stepText = QString("Step %1 of %2").arg(hint.currentStep + 1).arg(hint.totalSteps);
    QRect stepRect(calloutRect.left() + padding, y, contentWidth, 20);
    painter.drawText(stepRect, Qt::AlignLeft | Qt::AlignVCenter, stepText);
    y += 24;
  }

  // Buttons
  QRect buttonArea(calloutRect.left() + padding, calloutRect.bottom() - padding - 28, contentWidth,
                   28);
  paintButtons(painter, hint, buttonArea);

  painter.restore();
}

void NMHelpOverlay::paintTooltip(QPainter& painter, const ActiveHint& hint) {
  const QRect& calloutRect = hint.calloutRect;

  painter.save();
  painter.setOpacity(m_calloutOpacity);

  // Draw tooltip background
  QPainterPath bgPath;
  bgPath.addRoundedRect(calloutRect, 4, 4);

  painter.fillPath(bgPath, m_style.tooltipBackground);
  painter.setPen(QPen(m_style.tooltipBorder, 1));
  painter.drawPath(bgPath);

  // Draw content
  int padding = m_style.tooltipPadding;
  QRect contentRect = calloutRect.adjusted(padding, padding, -padding, -padding);

  painter.setFont(m_style.contentFont);
  painter.setPen(m_style.tooltipText);
  painter.drawText(contentRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                   QString::fromStdString(hint.content));

  painter.restore();
}

void NMHelpOverlay::paintButtons(QPainter& painter, const ActiveHint& hint, QRect& buttonArea) {
  int buttonWidth = 70;
  int buttonHeight = 24;
  int buttonSpacing = 8;

  int x = buttonArea.right() - buttonWidth;
  int y = buttonArea.top();

  m_buttonRects = ButtonRects();

  // Next button (primary)
  if (hint.showNextButton) {
    QRect nextRect(x, y, buttonWidth, buttonHeight);
    m_buttonRects.nextButton = nextRect;

    QColor bgColor = (m_hoveredButton == "next") ? m_style.buttonPrimaryBackgroundHover
                                                 : m_style.buttonPrimaryBackground;
    painter.fillRect(nextRect, bgColor);
    painter.setFont(m_style.buttonFont);
    painter.setPen(m_style.buttonPrimaryText);
    painter.drawText(nextRect, Qt::AlignCenter, QString::fromStdString(hint.nextButtonText));

    x -= buttonWidth + buttonSpacing;
  }

  // Skip button
  if (hint.showSkipButton) {
    QRect skipRect(x, y, buttonWidth, buttonHeight);
    m_buttonRects.skipButton = skipRect;

    QColor bgColor =
        (m_hoveredButton == "skip") ? m_style.buttonBackgroundHover : m_style.buttonBackground;
    painter.fillRect(skipRect, bgColor);
    painter.setFont(m_style.buttonFont);
    painter.setPen(m_style.buttonText);
    painter.drawText(skipRect, Qt::AlignCenter, "Skip");

    x -= buttonWidth + buttonSpacing;
  }

  // Back button
  if (hint.showBackButton) {
    QRect backRect(x, y, buttonWidth, buttonHeight);
    m_buttonRects.backButton = backRect;

    QColor bgColor =
        (m_hoveredButton == "back") ? m_style.buttonBackgroundHover : m_style.buttonBackground;
    painter.fillRect(backRect, bgColor);
    painter.setFont(m_style.buttonFont);
    painter.setPen(m_style.buttonText);
    painter.drawText(backRect, Qt::AlignCenter, QString::fromStdString(hint.backButtonText));
  }

  // Close button (X in top right)
  if (hint.showCloseButton) {
    QRect closeRect(buttonArea.right() - 20, buttonArea.top() - buttonArea.height() - 30, 20, 20);
    m_buttonRects.closeButton = closeRect;

    painter.setFont(m_style.buttonFont);
    painter.setPen((m_hoveredButton == "close") ? m_style.buttonPrimaryBackground
                                                : m_style.buttonText);
    painter.drawText(closeRect, Qt::AlignCenter,
                     QString::fromUtf8("\u00D7")); // ×
  }
}

void NMHelpOverlay::paintStepIndicator(QPainter& painter, const ActiveHint& hint,
                                       const QRect& area) {
  Q_UNUSED(painter);
  Q_UNUSED(hint);
  Q_UNUSED(area);
  // Step indicator is now painted inline in paintCallout
}

QRect NMHelpOverlay::calculateCalloutRect(const QRect& targetRect, const QSize& contentSize,
                                          CalloutPosition position) {
  int margin = 16;
  int arrowOffset = m_style.calloutArrowSize + 4;

  QRect calloutRect;
  calloutRect.setSize(contentSize);

  switch (position) {
  case CalloutPosition::Top:
    calloutRect.moveBottomLeft(
        QPoint(targetRect.center().x() - contentSize.width() / 2, targetRect.top() - arrowOffset));
    break;

  case CalloutPosition::Bottom:
    calloutRect.moveTopLeft(QPoint(targetRect.center().x() - contentSize.width() / 2,
                                   targetRect.bottom() + arrowOffset));
    break;

  case CalloutPosition::Left:
    calloutRect.moveTopRight(QPoint(targetRect.left() - arrowOffset,
                                    targetRect.center().y() - contentSize.height() / 2));
    break;

  case CalloutPosition::Right:
    calloutRect.moveTopLeft(QPoint(targetRect.right() + arrowOffset,
                                   targetRect.center().y() - contentSize.height() / 2));
    break;

  case CalloutPosition::TopLeft:
    calloutRect.moveBottomRight(
        QPoint(targetRect.left() + contentSize.width() / 2, targetRect.top() - arrowOffset));
    break;

  case CalloutPosition::TopRight:
    calloutRect.moveBottomLeft(
        QPoint(targetRect.right() - contentSize.width() / 2, targetRect.top() - arrowOffset));
    break;

  case CalloutPosition::BottomLeft:
    calloutRect.moveTopRight(
        QPoint(targetRect.left() + contentSize.width() / 2, targetRect.bottom() + arrowOffset));
    break;

  case CalloutPosition::BottomRight:
    calloutRect.moveTopLeft(
        QPoint(targetRect.right() - contentSize.width() / 2, targetRect.bottom() + arrowOffset));
    break;

  default:
    calloutRect.moveTopLeft(QPoint(targetRect.right() + arrowOffset,
                                   targetRect.center().y() - contentSize.height() / 2));
    break;
  }

  // Ensure callout stays within screen bounds
  QRect screenRect = this->rect();
  if (calloutRect.left() < margin) {
    calloutRect.moveLeft(margin);
  }
  if (calloutRect.right() > screenRect.right() - margin) {
    calloutRect.moveRight(screenRect.right() - margin);
  }
  if (calloutRect.top() < margin) {
    calloutRect.moveTop(margin);
  }
  if (calloutRect.bottom() > screenRect.bottom() - margin) {
    calloutRect.moveBottom(screenRect.bottom() - margin);
  }

  return calloutRect;
}

CalloutPosition NMHelpOverlay::determineAutoPosition(const QRect& targetRect,
                                                     const QSize& contentSize) {
  QRect screenRect = this->rect();

  int spaceTop = targetRect.top();
  int spaceBottom = screenRect.height() - targetRect.bottom();
  int spaceLeft = targetRect.left();
  int spaceRight = screenRect.width() - targetRect.right();

  int margin = 16;
  int arrowOffset = m_style.calloutArrowSize + 4;

  // Prefer positions with most space
  struct Option {
    CalloutPosition pos;
    int space;
  };

  std::vector<Option> options;

  if (spaceBottom >= contentSize.height() + arrowOffset + margin) {
    options.push_back({CalloutPosition::Bottom, spaceBottom});
  }
  if (spaceTop >= contentSize.height() + arrowOffset + margin) {
    options.push_back({CalloutPosition::Top, spaceTop});
  }
  if (spaceRight >= contentSize.width() + arrowOffset + margin) {
    options.push_back({CalloutPosition::Right, spaceRight});
  }
  if (spaceLeft >= contentSize.width() + arrowOffset + margin) {
    options.push_back({CalloutPosition::Left, spaceLeft});
  }

  if (options.empty()) {
    return CalloutPosition::Bottom; // Default fallback
  }

  // Sort by available space (descending)
  std::sort(options.begin(), options.end(),
            [](const Option& a, const Option& b) { return a.space > b.space; });

  return options[0].pos;
}

QSize NMHelpOverlay::calculateContentSize(const ActiveHint& hint) {
  int width = m_style.calloutMaxWidth;
  int padding = m_style.calloutPadding;

  QFontMetrics titleMetrics(m_style.titleFont);
  QFontMetrics contentMetrics(m_style.contentFont);

  int height = padding * 2;

  // Title
  if (!hint.title.empty()) {
    height += titleMetrics.height() + 8;
  }

  // Content
  QRect contentBounds =
      contentMetrics.boundingRect(QRect(0, 0, width - 2 * padding, 1000), Qt::TextWordWrap,
                                  QString::fromStdString(hint.content));
  height += contentBounds.height() + 16;

  // Step indicator
  if (hint.showStepIndicator) {
    height += 24;
  }

  // Buttons
  if (hint.showNextButton || hint.showBackButton || hint.showSkipButton) {
    height += 36;
  }

  return QSize(width, height);
}

void NMHelpOverlay::mousePressEvent(QMouseEvent* event) {
  // Only check button interactions if we have active hints
  if (!m_activeHints.empty()) {
    if (m_buttonRects.nextButton.contains(event->pos())) {
      emit nextClicked();
      return;
    }
    if (m_buttonRects.backButton.contains(event->pos())) {
      emit backClicked();
      return;
    }
    if (m_buttonRects.skipButton.contains(event->pos())) {
      emit skipClicked();
      return;
    }
    if (m_buttonRects.closeButton.contains(event->pos())) {
      emit closeClicked();
      return;
    }
    if (m_buttonRects.dontShowAgainCheckbox.contains(event->pos())) {
      m_dontShowAgainChecked = !m_dontShowAgainChecked;
      emit dontShowAgainToggled(m_dontShowAgainChecked);
      update();
      return;
    }
  }

  // If clicked outside callout, could optionally close
  QWidget::mousePressEvent(event);
}

void NMHelpOverlay::mouseMoveEvent(QMouseEvent* event) {
  QString prevHovered;
  if (m_hoveredButton) {
    prevHovered = QString::fromStdString(*m_hoveredButton);
  }

  m_hoveredButton.reset();

  if (m_buttonRects.nextButton.contains(event->pos())) {
    m_hoveredButton = "next";
  } else if (m_buttonRects.backButton.contains(event->pos())) {
    m_hoveredButton = "back";
  } else if (m_buttonRects.skipButton.contains(event->pos())) {
    m_hoveredButton = "skip";
  } else if (m_buttonRects.closeButton.contains(event->pos())) {
    m_hoveredButton = "close";
  }

  QString newHovered;
  if (m_hoveredButton) {
    newHovered = QString::fromStdString(*m_hoveredButton);
  }

  if (prevHovered != newHovered) {
    update();
  }

  QWidget::mouseMoveEvent(event);
}

void NMHelpOverlay::mouseReleaseEvent(QMouseEvent* event) {
  QWidget::mouseReleaseEvent(event);
}

void NMHelpOverlay::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateHintPositions();
}

bool NMHelpOverlay::event(QEvent* event) {
  if (event->type() == QEvent::ParentChange || event->type() == QEvent::Move) {
    updateHintPositions();
  }
  return QWidget::event(event);
}

void NMHelpOverlay::updateHintPositions() {
  // Look up anchor IDs from hints to get current positions
  // Note: We need to store anchor ID in the hint for this to work
  // For now, just update based on stored target rect
  // This is a placeholder for future anchor position tracking
  if (!m_activeHints.empty()) {
    update();
  }
}

void NMHelpOverlay::animateIn() {
  if (!m_style.enableAnimations) {
    m_spotlightOpacity = 1.0;
    m_calloutOpacity = 1.0;
    update();
    return;
  }

  m_spotlightOpacity = 0.0;
  m_calloutOpacity = 0.0;

  m_spotlightAnimation = std::make_unique<QPropertyAnimation>(this, "spotlightOpacity");
  m_spotlightAnimation->setDuration(m_style.animationDurationMs);
  m_spotlightAnimation->setStartValue(0.0);
  m_spotlightAnimation->setEndValue(1.0);
  m_spotlightAnimation->start();

  m_calloutAnimation = std::make_unique<QPropertyAnimation>(this, "calloutOpacity");
  m_calloutAnimation->setDuration(m_style.animationDurationMs);
  m_calloutAnimation->setStartValue(0.0);
  m_calloutAnimation->setEndValue(1.0);
  m_calloutAnimation->start();
}

void NMHelpOverlay::animateOut(std::function<void()> onComplete) {
  if (!m_style.enableAnimations) {
    if (onComplete)
      onComplete();
    return;
  }

  m_spotlightAnimation = std::make_unique<QPropertyAnimation>(this, "spotlightOpacity");
  m_spotlightAnimation->setDuration(m_style.animationDurationMs);
  m_spotlightAnimation->setStartValue(1.0);
  m_spotlightAnimation->setEndValue(0.0);
  m_spotlightAnimation->start();

  m_calloutAnimation = std::make_unique<QPropertyAnimation>(this, "calloutOpacity");
  m_calloutAnimation->setDuration(m_style.animationDurationMs);
  m_calloutAnimation->setStartValue(1.0);
  m_calloutAnimation->setEndValue(0.0);

  if (onComplete) {
    connect(m_calloutAnimation.get(), &QPropertyAnimation::finished, this, onComplete);
  }

  m_calloutAnimation->start();
}

void NMHelpOverlay::startAutoHideTimer(const std::string& hintId, int ms) {
  cancelAutoHideTimer(hintId);

  auto timer = std::make_unique<QTimer>(this);
  timer->setSingleShot(true);
  connect(timer.get(), &QTimer::timeout, this, [this, hintId]() {
    hideHint(hintId);
    emit hintAutoHidden(QString::fromStdString(hintId));
  });
  timer->start(ms);

  m_autoHideTimers[hintId] = std::move(timer);
}

void NMHelpOverlay::cancelAutoHideTimer(const std::string& hintId) {
  auto it = m_autoHideTimers.find(hintId);
  if (it != m_autoHideTimers.end()) {
    it->second->stop();
    m_autoHideTimers.erase(it);
  }
}

} // namespace NovelMind::editor::guided_learning
