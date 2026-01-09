#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

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

NMStyleManager::NMStyleManager() : QObject(nullptr) {
  m_palette = createDarkPalette();
}

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
  applyTheme(Theme::Dark);
}

void NMStyleManager::applyLightTheme() {
  applyTheme(Theme::Light);
}

void NMStyleManager::applyTheme(Theme theme) {
  if (!m_app)
    return;

  m_currentTheme = theme;

  // Update palette based on theme
  if (theme == Theme::Light) {
    m_palette = createLightPalette();
  } else {
    m_palette = createDarkPalette();
  }

  // Update icon colors based on theme
  auto &iconMgr = NMIconManager::instance();
  if (theme == Theme::Light) {
    iconMgr.setDefaultColor(QColor(50, 50, 50));  // Dark gray for light theme
  } else {
    iconMgr.setDefaultColor(QColor(220, 220, 220));  // Light gray for dark theme
  }

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

EditorPalette NMStyleManager::createDarkPalette() const {
  EditorPalette p;

  // Background colors (Layered surfaces)
  p.bgDarkest = QColor(0x0d, 0x10, 0x14);
  p.bgDark = QColor(0x14, 0x18, 0x1e);
  p.bgMedium = QColor(0x1c, 0x21, 0x29);
  p.bgLight = QColor(0x26, 0x2d, 0x38);
  p.bgElevated = QColor(0x2e, 0x36, 0x43);

  // Text colors
  p.textPrimary = QColor(0xe8, 0xed, 0xf3);
  p.textSecondary = QColor(0x9a, 0xa7, 0xb8);
  p.textMuted = QColor(0x6c, 0x76, 0x84);
  p.textDisabled = QColor(0x4a, 0x52, 0x5e);
  p.textInverse = QColor(0x0d, 0x10, 0x14);

  // Accent colors
  p.accentPrimary = QColor(0x3b, 0x9e, 0xff);
  p.accentHover = QColor(0x5c, 0xb3, 0xff);
  p.accentActive = QColor(0x28, 0x82, 0xe0);
  p.accentSubtle = QColor(0x1a, 0x3a, 0x5c);

  // Semantic/Status colors
  p.error = QColor(0xe5, 0x4d, 0x42);
  p.errorSubtle = QColor(0x3a, 0x1f, 0x1f);
  p.warning = QColor(0xf5, 0xa6, 0x23);
  p.warningSubtle = QColor(0x3a, 0x32, 0x1a);
  p.success = QColor(0x3d, 0xc9, 0x7e);
  p.successSubtle = QColor(0x1a, 0x3a, 0x2a);
  p.info = QColor(0x4a, 0x9e, 0xff);
  p.infoSubtle = QColor(0x1a, 0x2a, 0x3a);

  // Border colors
  p.borderDark = QColor(0x0a, 0x0d, 0x10);
  p.borderDefault = QColor(0x2a, 0x32, 0x3e);
  p.borderLight = QColor(0x38, 0x42, 0x50);
  p.borderFocus = QColor(0x3b, 0x9e, 0xff);

  // Graph/Node specific colors
  p.nodeDefault = QColor(0x28, 0x2e, 0x38);
  p.nodeSelected = QColor(0x2d, 0x7c, 0xcf);
  p.nodeHover = QColor(0x32, 0x3a, 0x46);
  p.nodeExecution = QColor(0x48, 0xc7, 0x6e);
  p.connectionLine = QColor(0x5a, 0x66, 0x74);
  p.connectionActive = QColor(0x3b, 0x9e, 0xff);
  p.gridLine = QColor(0x1e, 0x24, 0x2c);
  p.gridMajor = QColor(0x2a, 0x32, 0x3c);

  // Timeline/Keyframe colors
  p.keyframeDefault = QColor(0x5c, 0xb3, 0xff);
  p.keyframeSelected = QColor(0xff, 0xc1, 0x07);
  p.keyframeTangent = QColor(0x9f, 0x7a, 0xea);
  p.playhead = QColor(0xe5, 0x4d, 0x42);
  p.timelineTrack = QColor(0x1c, 0x21, 0x29);
  p.timelineTrackAlt = QColor(0x22, 0x28, 0x32);

  // Audio/Waveform colors
  p.waveformFill = QColor(0x3b, 0x9e, 0xff);
  p.waveformStroke = QColor(0x5c, 0xb3, 0xff);
  p.waveformBackground = QColor(0x14, 0x18, 0x1e);
  p.recordingActive = QColor(0xe5, 0x4d, 0x42);

  // Special UI elements
  p.scrollbarThumb = QColor(0x3a, 0x44, 0x52);
  p.scrollbarThumbHover = QColor(0x4a, 0x56, 0x66);
  p.scrollbarTrack = QColor(0x14, 0x18, 0x1e);
  p.dragHighlight = QColor(0x3b, 0x9e, 0xff);
  p.dropZone = QColor(0x1a, 0x3a, 0x5c);

  return p;
}

EditorPalette NMStyleManager::createLightPalette() const {
  EditorPalette p;

  // Background colors (Layered surfaces) - inverted for light theme
  p.bgDarkest = QColor(0xfa, 0xfa, 0xfa);      // Near white (base background)
  p.bgDark = QColor(0xf5, 0xf5, 0xf5);         // Light gray (panel backgrounds)
  p.bgMedium = QColor(0xff, 0xff, 0xff);       // White (elevated surfaces)
  p.bgLight = QColor(0xe8, 0xe8, 0xe8);        // Hover states
  p.bgElevated = QColor(0xff, 0xff, 0xff);     // Popups, dropdowns, tooltips

  // Text colors - dark for light background
  p.textPrimary = QColor(0x1e, 0x1e, 0x1e);    // Near black (high contrast)
  p.textSecondary = QColor(0x64, 0x64, 0x64);  // Dark gray (secondary text)
  p.textMuted = QColor(0x96, 0x96, 0x96);      // Medium gray (muted text)
  p.textDisabled = QColor(0xb4, 0xb4, 0xb4);   // Light gray (disabled text)
  p.textInverse = QColor(0xff, 0xff, 0xff);    // White text on dark backgrounds

  // Accent colors - slightly adjusted for light theme
  p.accentPrimary = QColor(0x3b, 0x82, 0xf6);  // Blue (selection, focus)
  p.accentHover = QColor(0x25, 0x63, 0xeb);    // Darker blue (hover)
  p.accentActive = QColor(0x1d, 0x4e, 0xd8);   // Even darker blue (active)
  p.accentSubtle = QColor(0xdb, 0xea, 0xfe);   // Light blue background

  // Semantic/Status colors - vivid for light theme
  p.error = QColor(0xef, 0x44, 0x44);          // Red (error states)
  p.errorSubtle = QColor(0xfe, 0xe2, 0xe2);    // Light red background
  p.warning = QColor(0xea, 0xb3, 0x08);        // Yellow/Orange (warning)
  p.warningSubtle = QColor(0xfe, 0xf3, 0xc7);  // Light yellow background
  p.success = QColor(0x22, 0xc5, 0x5e);        // Green (success)
  p.successSubtle = QColor(0xd1, 0xfa, 0xe5);  // Light green background
  p.info = QColor(0x3b, 0x82, 0xf6);           // Blue (info)
  p.infoSubtle = QColor(0xdb, 0xea, 0xfe);     // Light blue background

  // Border colors - medium gray for light theme
  p.borderDark = QColor(0xb4, 0xb4, 0xb4);     // Darker gray (strong borders)
  p.borderDefault = QColor(0xc8, 0xc8, 0xc8);  // Default borders
  p.borderLight = QColor(0xe0, 0xe0, 0xe0);    // Subtle borders
  p.borderFocus = QColor(0x3b, 0x82, 0xf6);    // Blue focus ring

  // Graph/Node specific colors
  p.nodeDefault = QColor(0xff, 0xff, 0xff);
  p.nodeSelected = QColor(0x60, 0xa5, 0xfa);
  p.nodeHover = QColor(0xf0, 0xf0, 0xf0);
  p.nodeExecution = QColor(0x22, 0xc5, 0x5e);
  p.connectionLine = QColor(0x96, 0x96, 0x96);
  p.connectionActive = QColor(0x3b, 0x82, 0xf6);
  p.gridLine = QColor(0xe8, 0xe8, 0xe8);
  p.gridMajor = QColor(0xd0, 0xd0, 0xd0);

  // Timeline/Keyframe colors
  p.keyframeDefault = QColor(0x3b, 0x82, 0xf6);
  p.keyframeSelected = QColor(0xea, 0xb3, 0x08);
  p.keyframeTangent = QColor(0xa8, 0x55, 0xf7);
  p.playhead = QColor(0xef, 0x44, 0x44);
  p.timelineTrack = QColor(0xf5, 0xf5, 0xf5);
  p.timelineTrackAlt = QColor(0xff, 0xff, 0xff);

  // Audio/Waveform colors
  p.waveformFill = QColor(0x3b, 0x82, 0xf6);
  p.waveformStroke = QColor(0x25, 0x63, 0xeb);
  p.waveformBackground = QColor(0xf5, 0xf5, 0xf5);
  p.recordingActive = QColor(0xef, 0x44, 0x44);

  // Special UI elements
  p.scrollbarThumb = QColor(0xc8, 0xc8, 0xc8);
  p.scrollbarThumbHover = QColor(0xb4, 0xb4, 0xb4);
  p.scrollbarTrack = QColor(0xf5, 0xf5, 0xf5);
  p.dragHighlight = QColor(0x3b, 0x82, 0xf6);
  p.dropZone = QColor(0xdb, 0xea, 0xfe);

  return p;
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
