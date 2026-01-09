#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/nm_dock_title_bar.hpp"
#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/guided_learning/anchor_registry.hpp"

#include <QDockWidget>
#include <QFocusEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWidget>

namespace NovelMind::editor::qt {

using guided_learning::ScopedAnchorRegistration;

NMDockPanel::NMDockPanel(const QString &title, QWidget *parent)
    : QDockWidget(title, parent) {
  // Set default dock widget features
  setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
              QDockWidget::DockWidgetFloatable);

  // Enable focus tracking
  setFocusPolicy(Qt::StrongFocus);

  // Set default minimum size to prevent UI overlap when docked
  // This ensures panels cannot be resized too small, preventing
  // text fields from overlapping buttons, headers from covering content, etc.
  setMinimumPanelSize(defaultMinimumSize());

  // Ensure every panel has a concrete content widget by default.
  setContentWidget(new QWidget(this));

  // Setup custom title bar (Issue #293)
  setupCustomTitleBar();
}

NMDockPanel::~NMDockPanel() {
  // Clean up anchor registrations before shutdown
  m_elementAnchors.clear();
  m_panelAnchor.reset();
  onShutdown();
}

void NMDockPanel::setPanelId(const QString &id) {
  m_panelId = id;

  // Register this panel with the guided learning anchor registry.
  // This allows the tutorial system to show hints attached to this panel.
  if (!id.isEmpty()) {
    std::string anchorId = id.toStdString() + ".panel";
    std::string description = windowTitle().toStdString() + " panel";
    m_panelAnchor = std::make_unique<ScopedAnchorRegistration>(
        anchorId, this, description, id.toStdString());
  }
}

void NMDockPanel::registerAnchor(const std::string &elementId, QWidget *widget,
                                 const std::string &description) {
  if (m_panelId.isEmpty() || !widget) {
    return;
  }

  std::string anchorId = m_panelId.toStdString() + "." + elementId;
  std::string desc =
      description.empty() ? (elementId + " element") : description;

  m_elementAnchors.push_back(std::make_unique<ScopedAnchorRegistration>(
      anchorId, widget, desc, m_panelId.toStdString()));
}

void NMDockPanel::onUpdate(double /*deltaTime*/) {
  // Default implementation does nothing
  // Subclasses override this for continuous updates
}

void NMDockPanel::onInitialize() {
  // Default implementation does nothing
  // Subclasses override this for initialization
}

void NMDockPanel::onShutdown() {
  // Default implementation does nothing
  // Subclasses override this for cleanup
}

void NMDockPanel::onFocusGained() {
  // Default implementation does nothing
}

void NMDockPanel::onFocusLost() {
  // Default implementation does nothing
}

void NMDockPanel::onResize(int /*width*/, int /*height*/) {
  // Default implementation does nothing
}

void NMDockPanel::setContentWidget(QWidget *widget) {
  m_contentWidget = widget;
  setWidget(widget);
}

void NMDockPanel::focusInEvent(QFocusEvent *event) {
  QDockWidget::focusInEvent(event);
  onFocusGained();
  emit focusGained();

  // Update custom title bar active state (Issue #293)
  if (m_customTitleBar) {
    m_customTitleBar->setActive(true);
  }

  // Issue #173: Publish focus event through EventBus for panel synchronization
  // This allows other panels to react to focus changes without tight coupling
  if (!m_panelId.isEmpty()) {
    PanelFocusChangedEvent focusEvent;
    focusEvent.panelName = m_panelId.toStdString();
    focusEvent.hasFocus = true;
    focusEvent.source = m_panelId.toStdString();
    EventBus::instance().publish(focusEvent);
  }
}

void NMDockPanel::focusOutEvent(QFocusEvent *event) {
  QDockWidget::focusOutEvent(event);
  onFocusLost();
  emit focusLost();

  // Update custom title bar active state (Issue #293)
  if (m_customTitleBar) {
    m_customTitleBar->setActive(false);
  }

  // Issue #173: Publish focus event through EventBus for panel synchronization
  if (!m_panelId.isEmpty()) {
    PanelFocusChangedEvent focusEvent;
    focusEvent.panelName = m_panelId.toStdString();
    focusEvent.hasFocus = false;
    focusEvent.source = m_panelId.toStdString();
    EventBus::instance().publish(focusEvent);
  }
}

void NMDockPanel::resizeEvent(QResizeEvent *event) {
  QDockWidget::resizeEvent(event);
  onResize(event->size().width(), event->size().height());
}

void NMDockPanel::showEvent(QShowEvent *event) {
  // INVARIANT: Prevent re-entrance to avoid nested initialization
  if (m_inShowEvent) {
    return;
  }
  m_inShowEvent = true;

  QDockWidget::showEvent(event);

  if (!m_initialized) {
    m_initialized = true;
    onInitialize();
  }

  m_inShowEvent = false;
}

void NMDockPanel::setMinimumPanelSize(int width, int height) {
  setMinimumPanelSize(QSize(width, height));
}

void NMDockPanel::setMinimumPanelSize(const QSize &size) {
  // Set minimum size on the dock widget itself
  setMinimumSize(size);

  // Also set minimum size on the content widget if it exists
  // This provides a hint to the layout system
  if (m_contentWidget) {
    m_contentWidget->setMinimumSize(size.width() - 4, size.height() - 4);
  }
}

void NMDockPanel::setupCustomTitleBar() {
  // Create custom title bar widget
  m_customTitleBar = new NMDockTitleBar(this);
  m_customTitleBar->setTitle(windowTitle());

  // Set as the dock widget's title bar
  setTitleBarWidget(m_customTitleBar);

  // Connect signals
  connectTitleBarSignals();
}

void NMDockPanel::connectTitleBarSignals() {
  if (!m_customTitleBar) {
    return;
  }

  // Float button
  connect(m_customTitleBar, &NMDockTitleBar::floatClicked, this, [this]() {
    setFloating(!isFloating());
  });

  // Close button
  connect(m_customTitleBar, &NMDockTitleBar::closeClicked, this,
          &QDockWidget::close);

  // Pin button - prevent panel from being moved/floated
  connect(m_customTitleBar, &NMDockTitleBar::pinClicked, this,
          [this](bool pinned) {
            if (pinned) {
              // Disable moving and floating when pinned
              setFeatures(QDockWidget::DockWidgetClosable);
            } else {
              // Restore normal features
              setFeatures(QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetFloatable);
            }
          });

  // Minimize button - collapse to title bar only
  connect(m_customTitleBar, &NMDockTitleBar::minimizeClicked, this,
          [this](bool minimized) {
            if (m_contentWidget) {
              m_contentWidget->setVisible(!minimized);
            }
          });

  // Double-click title bar to toggle float
  connect(m_customTitleBar, &NMDockTitleBar::titleDoubleClicked, this,
          [this]() { setFloating(!isFloating()); });
}

void NMDockPanel::setPanelIcon(const QIcon &icon) {
  if (m_customTitleBar) {
    m_customTitleBar->setIcon(icon);
  }
}

void NMDockPanel::setShowSettingsButton(bool show) {
  if (m_customTitleBar) {
    m_customTitleBar->setShowSettings(show);
  }
}

void NMDockPanel::setTitleBarHeight(int height) {
  if (m_customTitleBar) {
    m_customTitleBar->setTitleBarHeight(height);
  }
}

} // namespace NovelMind::editor::qt
