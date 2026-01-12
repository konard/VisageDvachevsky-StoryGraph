#include "NovelMind/editor/qt/nm_dock_title_bar.hpp"
#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>

namespace NovelMind::editor::qt {

NMDockTitleBar::NMDockTitleBar(QWidget* parent) : QWidget(parent) {
  setupUi();
  updateStyle();
}

void NMDockTitleBar::setupUi() {
  // Set fixed height for compact title bar
  setFixedHeight(m_titleBarHeight);
  setMinimumWidth(100);

  // Create main layout with minimal spacing
  m_layout = new QHBoxLayout(this);
  m_layout->setContentsMargins(6, 0, 4, 0);
  m_layout->setSpacing(4);

  // Icon label (16x16 icon)
  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(16, 16);
  m_iconLabel->setScaledContents(true);
  m_layout->addWidget(m_iconLabel);

  // Title label
  m_titleLabel = new QLabel(this);
  m_titleLabel->setStyleSheet("QLabel { font-weight: 500; font-size: 12px; }");
  m_layout->addWidget(m_titleLabel, 1); // Stretch factor 1

  // Create buttons
  QColor buttonColor(180, 180, 180); // Light gray for buttons

  // Pin button
  m_pinButton = new QPushButton(this);
  createButton(m_pinButton, "pin", "Pin panel (lock in place)");
  m_layout->addWidget(m_pinButton);
  connect(m_pinButton, &QPushButton::clicked, this, [this]() {
    setPinned(!m_pinned);
    emit pinClicked(m_pinned);
  });

  // Settings button
  m_settingsButton = new QPushButton(this);
  createButton(m_settingsButton, "settings", "Panel settings");
  m_layout->addWidget(m_settingsButton);
  connect(m_settingsButton, &QPushButton::clicked, this, &NMDockTitleBar::settingsClicked);

  // Minimize button
  m_minimizeButton = new QPushButton(this);
  createButton(m_minimizeButton, "chevron-up", "Minimize panel");
  m_layout->addWidget(m_minimizeButton);
  connect(m_minimizeButton, &QPushButton::clicked, this, [this]() {
    setMinimized(!m_minimized);
    emit minimizeClicked(m_minimized);
  });

  // Float button
  m_floatButton = new QPushButton(this);
  createButton(m_floatButton, "external-link", "Float/Dock panel");
  m_layout->addWidget(m_floatButton);
  connect(m_floatButton, &QPushButton::clicked, this, &NMDockTitleBar::floatClicked);

  // Close button
  m_closeButton = new QPushButton(this);
  createButton(m_closeButton, "file-close", "Close panel");
  m_layout->addWidget(m_closeButton);
  connect(m_closeButton, &QPushButton::clicked, this, &NMDockTitleBar::closeClicked);

  // Initial button visibility
  updateButtonVisibility();
}

void NMDockTitleBar::createButton(QPushButton*& button, const QString& iconName,
                                  const QString& tooltip) {
  if (!button) {
    return;
  }

  auto& iconManager = NMIconManager::instance();
  QColor buttonColor(180, 180, 180);

  button->setIcon(iconManager.getIcon(iconName, 14, buttonColor));
  button->setIconSize(QSize(14, 14));
  button->setFixedSize(18, 18);
  button->setFlat(true);
  button->setToolTip(tooltip);
  button->setStyleSheet(R"(
    QPushButton {
      background: transparent;
      border: none;
      border-radius: 2px;
      padding: 2px;
    }
    QPushButton:hover {
      background: rgba(255, 255, 255, 0.1);
    }
    QPushButton:pressed {
      background: rgba(255, 255, 255, 0.05);
    }
  )");
}

void NMDockTitleBar::setTitle(const QString& title) {
  m_title = title;
  m_titleLabel->setText(title);
}

void NMDockTitleBar::setIcon(const QIcon& icon) {
  m_icon = icon;
  if (!icon.isNull()) {
    m_iconLabel->setPixmap(icon.pixmap(16, 16));
    m_iconLabel->show();
  } else {
    m_iconLabel->hide();
  }
}

void NMDockTitleBar::setPinned(bool pinned) {
  if (m_pinned != pinned) {
    m_pinned = pinned;

    // Update pin button icon
    auto& iconManager = NMIconManager::instance();
    QColor buttonColor(180, 180, 180);
    QString iconName = pinned ? "pin" : "unpin";
    m_pinButton->setIcon(iconManager.getIcon(iconName, 14, buttonColor));
    m_pinButton->setToolTip(pinned ? "Unpin panel" : "Pin panel (lock in place)");

    updateStyle();
  }
}

void NMDockTitleBar::setMinimized(bool minimized) {
  if (m_minimized != minimized) {
    m_minimized = minimized;

    // Update minimize button icon
    auto& iconManager = NMIconManager::instance();
    QColor buttonColor(180, 180, 180);
    QString iconName = minimized ? "chevron-down" : "chevron-up";
    m_minimizeButton->setIcon(iconManager.getIcon(iconName, 14, buttonColor));
    m_minimizeButton->setToolTip(minimized ? "Restore panel" : "Minimize panel");

    updateStyle();
  }
}

void NMDockTitleBar::setShowSettings(bool show) {
  if (m_showSettings != show) {
    m_showSettings = show;
    updateButtonVisibility();
  }
}

void NMDockTitleBar::setActive(bool active) {
  if (m_active != active) {
    m_active = active;
    updateStyle();
  }
}

void NMDockTitleBar::setTitleBarHeight(int height) {
  if (m_titleBarHeight != height && height >= 18 && height <= 32) {
    m_titleBarHeight = height;
    setFixedHeight(height);
  }
}

void NMDockTitleBar::updateStyle() {
  // Update background color based on active state
  QString bgColor = m_active ? "#2d3139" : "#25272e";
  QString accentColor = m_active ? "#3d8fd1" : "transparent";

  setStyleSheet(QString(R"(
    NMDockTitleBar {
      background-color: %1;
      border-bottom: 2px solid %2;
    }
  )")
                    .arg(bgColor)
                    .arg(accentColor));

  // Update title label color
  QColor textColor = m_active ? QColor(220, 220, 220) : QColor(160, 160, 160);
  m_titleLabel->setStyleSheet(QString("QLabel { font-weight: %1; font-size: 12px; color: %2; }")
                                  .arg(m_active ? "600" : "500")
                                  .arg(textColor.name()));

  update();
}

void NMDockTitleBar::updateButtonVisibility() {
  // Settings button is optional
  m_settingsButton->setVisible(m_showSettings);

  // Pin, minimize, float, and close are always visible
  m_pinButton->setVisible(true);
  m_minimizeButton->setVisible(true);
  m_floatButton->setVisible(true);
  m_closeButton->setVisible(true);
}

void NMDockTitleBar::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragStartPos = event->pos();
    m_dragging = true;
  }
  QWidget::mousePressEvent(event);
}

void NMDockTitleBar::mouseMoveEvent(QMouseEvent* event) {
  if (m_dragging && (event->buttons() & Qt::LeftButton)) {
    // Let the dock widget handle dragging
    // We just track that we're dragging
    if ((event->pos() - m_dragStartPos).manhattanLength() > 5) {
      // Significant movement detected
    }
  }
  QWidget::mouseMoveEvent(event);
}

void NMDockTitleBar::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = false;
  }
  QWidget::mouseReleaseEvent(event);
}

void NMDockTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit titleDoubleClicked();
  }
  QWidget::mouseDoubleClickEvent(event);
}

void NMDockTitleBar::paintEvent(QPaintEvent* event) {
  // Paint background with style
  QStyleOption opt;
  opt.initFrom(this);
  QPainter painter(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

  QWidget::paintEvent(event);
}

} // namespace NovelMind::editor::qt
