#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QAbstractButton>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QStyleFactory>
#include <QToolButton>

namespace NovelMind::editor::qt {

NMStyleManager &NMStyleManager::instance() {
  static NMStyleManager instance;
  return instance;
}

NMStyleManager::NMStyleManager() : QObject(nullptr) {}

void NMStyleManager::initialize(QApplication *app) {
  m_app = app;

  setupHighDpi();
  setupFonts();
  applyDarkTheme();
}

void NMStyleManager::setupHighDpi() {
  // Get the primary screen's DPI
  if (QGuiApplication::primaryScreen()) {
    double dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
    // Standard DPI is 96, calculate scale factor
    m_uiScale = dpi / 96.0;

    // Clamp to reasonable range
    if (m_uiScale < 1.0)
      m_uiScale = 1.0;
    if (m_uiScale > 3.0)
      m_uiScale = 3.0;
  }

  // Update icon sizes based on scale
  m_toolbarIconSize = static_cast<int>(24 * m_uiScale);
  m_menuIconSize = static_cast<int>(16 * m_uiScale);
}

void NMStyleManager::setupFonts() {
  // Default UI font
#ifdef Q_OS_WIN
  m_defaultFont = QFont("Segoe UI", 9);
  m_monospaceFont = QFont("Consolas", 9);
#elif defined(Q_OS_LINUX)
  m_defaultFont = QFont("Ubuntu", 10);
  m_monospaceFont = QFont("Ubuntu Mono", 10);

  // Fallback if Ubuntu font not available
  if (!QFontDatabase::families().contains("Ubuntu")) {
    m_defaultFont = QFont("Sans", 10);
  }
  if (!QFontDatabase::families().contains("Ubuntu Mono")) {
    m_monospaceFont = QFont("Monospace", 10);
  }
#else
  m_defaultFont = QFont(); // System default
  m_defaultFont.setPointSize(10);
  m_monospaceFont = QFont("Courier", 10);
#endif

  // Apply scale to fonts
  m_defaultFont.setPointSizeF(m_defaultFont.pointSizeF() * m_uiScale);
  m_monospaceFont.setPointSizeF(m_monospaceFont.pointSizeF() * m_uiScale);
}

void NMStyleManager::applyDarkTheme() {
  if (!m_app)
    return;

  // Use Fusion style as base (cross-platform, customizable)
  m_app->setStyle(QStyleFactory::create("Fusion"));

  // Apply default font
  m_app->setFont(m_defaultFont);

  // Apply stylesheet
  m_app->setStyleSheet(getStyleSheet());

  emit themeChanged();
}

void NMStyleManager::setUiScale(double scale) {
  if (scale < 0.5)
    scale = 0.5;
  if (scale > 3.0)
    scale = 3.0;

  if (qFuzzyCompare(m_uiScale, scale))
    return;

  m_uiScale = scale;
  m_toolbarIconSize = static_cast<int>(24 * m_uiScale);
  m_menuIconSize = static_cast<int>(16 * m_uiScale);

  setupFonts();
  applyDarkTheme();

  emit scaleChanged(m_uiScale);
}

void NMStyleManager::configureToolbarButton(QAbstractButton *button,
                                             const QIcon &icon) {
  if (!button)
    return;

  const auto &sizes = instance().buttonSizes();
  button->setMinimumSize(sizes.toolbarButton, sizes.toolbarButton);
  button->setMaximumSize(sizes.toolbarButton, sizes.toolbarButton);

  if (!icon.isNull()) {
    button->setIcon(icon);
  }
}

void NMStyleManager::configureSquareButton(QAbstractButton *button, int size,
                                            const QIcon &icon) {
  if (!button)
    return;

  button->setFixedSize(size, size);

  if (!icon.isNull()) {
    button->setIcon(icon);
  }
}

void NMStyleManager::setButtonSize(QAbstractButton *button, int width,
                                    int height) {
  if (!button)
    return;

  button->setMinimumSize(width, height);
  button->setMaximumSize(width, height);
}

} // namespace NovelMind::editor::qt
