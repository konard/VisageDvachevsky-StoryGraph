#include "NovelMind/editor/qt/nm_scrollable_toolbar.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMScrollableToolBar
// ============================================================================

NMScrollableToolBar::NMScrollableToolBar(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create scroll area for horizontal scrolling
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  // Set a reasonable minimum height for the scroll area
  m_scrollArea->setMinimumHeight(32);
  m_scrollArea->setMaximumHeight(48);

  // Style the scroll bar to be minimal and unobtrusive
  const auto &palette = NMStyleManager::instance().palette();
  m_scrollArea->setStyleSheet(
      QString("QScrollArea { background: transparent; border: none; }"
              "QScrollBar:horizontal { height: 6px; background: %1; }"
              "QScrollBar::handle:horizontal { background: %2; border-radius: "
              "3px; min-width: 20px; }"
              "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal"
              " { width: 0; }"
              "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal"
              " { background: none; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));

  // Create toolbar inside scroll area
  m_toolbar = new QToolBar(this);
  m_toolbar->setMovable(false);
  m_toolbar->setFloatable(false);

  m_scrollArea->setWidget(m_toolbar);
  layout->addWidget(m_scrollArea);
}

void NMScrollableToolBar::setToolBarObjectName(const QString &name) {
  m_toolbar->setObjectName(name);
}

void NMScrollableToolBar::setIconSize(const QSize &size) {
  m_toolbar->setIconSize(size);
}

bool NMScrollableToolBar::isScrollingNeeded() const { return m_scrollNeeded; }

void NMScrollableToolBar::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateScrollIndicators();
}

void NMScrollableToolBar::updateScrollIndicators() {
  if (!m_toolbar || !m_scrollArea) {
    return;
  }

  bool wasScrollNeeded = m_scrollNeeded;
  m_scrollNeeded = m_toolbar->sizeHint().width() > m_scrollArea->width();

  if (wasScrollNeeded != m_scrollNeeded) {
    emit scrollVisibilityChanged(m_scrollNeeded);
  }
}

// ============================================================================
// NMCollapsiblePanel
// ============================================================================

NMCollapsiblePanel::NMCollapsiblePanel(const QString &title, QWidget *parent)
    : QWidget(parent), m_collapsed(false) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  const auto &palette = NMStyleManager::instance().palette();

  // Create header with toggle button
  m_headerWidget = new QWidget(this);
  m_headerWidget->setFixedHeight(24);
  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(4, 2, 4, 2);
  headerLayout->setSpacing(4);

  m_toggleButton = new QPushButton(this);
  m_toggleButton->setFlat(true);
  m_toggleButton->setFixedSize(16, 16);
  m_toggleButton->setText(m_collapsed ? "▶" : "▼");
  m_toggleButton->setStyleSheet(
      QString("QPushButton { background: transparent; color: %1; border: none; "
              "font-size: 10px; }"
              "QPushButton:hover { color: %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary)));

  auto *titleLabel = new QLabel(title, m_headerWidget);
  titleLabel->setStyleSheet(
      QString("color: %1; font-weight: 600; font-size: 11px;")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));

  headerLayout->addWidget(m_toggleButton);
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();

  m_headerWidget->setStyleSheet(
      QString("QWidget { background-color: %1; border-bottom: 1px solid %2; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgDark))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));

  mainLayout->addWidget(m_headerWidget);

  // Create content container with scroll area
  m_contentContainer = new QWidget(this);
  auto *contentLayout = new QVBoxLayout(m_contentContainer);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  mainLayout->addWidget(m_contentContainer);

  // Connect toggle
  connect(m_toggleButton, &QPushButton::clicked, this,
          &NMCollapsiblePanel::toggle);

  // Make header clickable
  m_headerWidget->setCursor(Qt::PointingHandCursor);
  m_headerWidget->installEventFilter(this);
}

void NMCollapsiblePanel::setContentWidget(QWidget *widget) {
  if (m_contentWidget) {
    m_contentContainer->layout()->removeWidget(m_contentWidget);
    m_contentWidget->setParent(nullptr);
  }

  m_contentWidget = widget;

  if (m_contentWidget) {
    m_contentContainer->layout()->addWidget(m_contentWidget);
    updateVisibility();
  }
}

void NMCollapsiblePanel::setCollapsed(bool collapsed) {
  if (m_collapsed != collapsed) {
    m_collapsed = collapsed;
    updateVisibility();
    emit collapsedChanged(m_collapsed);
  }
}

void NMCollapsiblePanel::toggle() { setCollapsed(!m_collapsed); }

void NMCollapsiblePanel::updateVisibility() {
  if (m_toggleButton) {
    m_toggleButton->setText(m_collapsed ? "▶" : "▼");
  }

  if (m_contentContainer) {
    m_contentContainer->setVisible(!m_collapsed);
  }
}

// ============================================================================
// NMScrollableContent
// ============================================================================

NMScrollableContent::NMScrollableContent(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  // Style the scroll bar
  const auto &palette = NMStyleManager::instance().palette();
  m_scrollArea->setStyleSheet(
      QString("QScrollArea { background: transparent; border: none; }"
              "QScrollBar:vertical { width: 6px; background: %1; }"
              "QScrollBar::handle:vertical { background: %2; border-radius: "
              "3px; min-height: 20px; }"
              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical "
              "{ height: 0; }"
              "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical "
              "{ background: none; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));

  layout->addWidget(m_scrollArea);
}

void NMScrollableContent::setContentWidget(QWidget *widget) {
  m_scrollArea->setWidget(widget);
}

} // namespace NovelMind::editor::qt
