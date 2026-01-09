#pragma once

/**
 * @file nm_style_manager.hpp
 * @brief Style management for NovelMind Editor
 *
 * Provides Unreal Engine-like dark theme styling using Qt Style Sheets (QSS).
 * Manages:
 * - Application-wide dark theme
 * - High-DPI scaling
 * - Custom color palette
 * - Consistent widget styling
 */

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

class QAbstractButton;
class QPushButton;
class QToolButton;
class QIcon;

namespace NovelMind::editor::qt {

/**
 * @brief Spacing constants for consistent layout rhythm
 */
struct SpacingTokens {
  int xxs = 2;   // Extra extra small
  int xs = 4;    // Extra small
  int sm = 8;    // Small
  int md = 12;   // Medium
  int lg = 16;   // Large
  int xl = 24;   // Extra large
  int xxl = 32;  // Extra extra large
  int xxxl = 48; // Triple extra large
};

/**
 * @brief Border radius constants
 */
struct RadiusTokens {
  int none = 0;
  int sm = 2;
  int md = 4;
  int lg = 6;
  int xl = 8;
  int full = 9999; // For pills/circles
};

/**
 * @brief Typography sizes
 */
struct TypographyTokens {
  int captionSize = 8;
  int smallSize = 9;
  int bodySize = 10;
  int labelSize = 11;
  int subtitleSize = 12;
  int titleSize = 14;
  int headingSize = 18;
  int displaySize = 24;
};

/**
 * @brief Standard button sizes for consistent UI
 */
struct ButtonSizeTokens {
  // Standard button heights (width varies by content)
  int small = 22;  // Compact toolbars, toggle buttons, icon-only buttons
  int medium = 28; // Standard toolbar buttons, most UI buttons
  int large = 34;  // Primary action buttons, important actions
  int xlarge = 44; // Touch-friendly or prominent actions

  // Icon sizes for buttons (match icon to button size)
  int iconSmall = 14;
  int iconMedium = 16;
  int iconLarge = 20;
  int iconXLarge = 24;

  // Common square button sizes (width = height)
  int squareSmall = 16;  // Tiny toggles, collapse buttons
  int squareMedium = 24; // Mute/solo buttons, small controls
  int squareLarge = 32;  // Standard square buttons
  int squareXLarge = 40; // Record button, prominent square actions

  // Special purpose sizes
  int toolbarButton = 28; // Standard toolbar button
  int paletteButton = 72; // Scene palette creation buttons (height)
  int paletteButtonWidth = 84; // Scene palette creation buttons (width)
};

/**
 * @brief Panel-specific accent colors for visual identity
 */
struct PanelAccents {
  QColor sceneView{0x2e, 0xc4, 0xb6};    // Teal
  QColor storyGraph{0x6a, 0xa6, 0xff};   // Blue
  QColor inspector{0xf0, 0xb2, 0x4a};    // Orange/Gold
  QColor assetBrowser{0x5f, 0xd1, 0x8a}; // Green
  QColor scriptEditor{0xff, 0x9b, 0x66}; // Coral
  QColor console{0x8e, 0xa1, 0xb5};      // Gray-blue
  QColor playToolbar{0x48, 0xc7, 0x6e};  // Bright green
  QColor timeline{0x9f, 0x7a, 0xea};     // Purple
  QColor curveEditor{0xe8, 0x6a, 0x92};  // Pink
  QColor voiceManager{0x4a, 0xc1, 0xd6}; // Cyan
  QColor localization{0xff, 0xc1, 0x07}; // Yellow
  QColor diagnostics{0xe1, 0x4e, 0x43};  // Red (for warnings/errors)
  QColor hierarchy{0x7c, 0xb3, 0x42};    // Lime
  QColor scenePalette{0xd6, 0x8f, 0xd6}; // Lavender
};

/**
 * @brief Theme selection
 */
enum class Theme {
  Dark,  // Dark theme (default)
  Light  // Light theme
};

/**
 * @brief Color palette for the editor theme
 */
struct EditorPalette {
  // =========================================================================
  // BACKGROUND COLORS (Layered surfaces)
  // =========================================================================
  QColor bgDarkest{0x0d, 0x10, 0x14};  // Base background (deepest layer)
  QColor bgDark{0x14, 0x18, 0x1e};     // Panel backgrounds
  QColor bgMedium{0x1c, 0x21, 0x29};   // Elevated surfaces (cards, inputs)
  QColor bgLight{0x26, 0x2d, 0x38};    // Hover states
  QColor bgElevated{0x2e, 0x36, 0x43}; // Popups, dropdowns, tooltips

  // =========================================================================
  // TEXT COLORS
  // =========================================================================
  QColor textPrimary{0xe8, 0xed, 0xf3};   // Primary text (high contrast)
  QColor textSecondary{0x9a, 0xa7, 0xb8}; // Secondary/hint text
  QColor textMuted{0x6c, 0x76, 0x84};     // Muted/placeholder text
  QColor textDisabled{0x4a, 0x52, 0x5e};  // Disabled text
  QColor textInverse{0x0d, 0x10, 0x14};   // Text on light backgrounds

  // =========================================================================
  // ACCENT COLORS (Primary brand)
  // =========================================================================
  QColor accentPrimary{0x3b, 0x9e, 0xff}; // Selection, focus, links
  QColor accentHover{0x5c, 0xb3, 0xff};   // Hover state
  QColor accentActive{0x28, 0x82, 0xe0};  // Active/pressed state
  QColor accentSubtle{0x1a, 0x3a, 0x5c};  // Subtle accent background

  // =========================================================================
  // SEMANTIC/STATUS COLORS
  // =========================================================================
  QColor error{0xe5, 0x4d, 0x42};         // Error states
  QColor errorSubtle{0x3a, 0x1f, 0x1f};   // Error background
  QColor warning{0xf5, 0xa6, 0x23};       // Warning states
  QColor warningSubtle{0x3a, 0x32, 0x1a}; // Warning background
  QColor success{0x3d, 0xc9, 0x7e};       // Success states
  QColor successSubtle{0x1a, 0x3a, 0x2a}; // Success background
  QColor info{0x4a, 0x9e, 0xff};          // Info states
  QColor infoSubtle{0x1a, 0x2a, 0x3a};    // Info background

  // =========================================================================
  // BORDER COLORS
  // =========================================================================
  QColor borderDark{0x0a, 0x0d, 0x10};    // Strong borders
  QColor borderDefault{0x2a, 0x32, 0x3e}; // Default borders
  QColor borderLight{0x38, 0x42, 0x50};   // Subtle borders
  QColor borderFocus{0x3b, 0x9e, 0xff};   // Focus ring

  // =========================================================================
  // GRAPH/NODE SPECIFIC COLORS
  // =========================================================================
  QColor nodeDefault{0x28, 0x2e, 0x38};
  QColor nodeSelected{0x2d, 0x7c, 0xcf};
  QColor nodeHover{0x32, 0x3a, 0x46};
  QColor nodeExecution{0x48, 0xc7, 0x6e}; // Execution flow highlight
  QColor connectionLine{0x5a, 0x66, 0x74};
  QColor connectionActive{0x3b, 0x9e, 0xff};
  QColor gridLine{0x1e, 0x24, 0x2c};
  QColor gridMajor{0x2a, 0x32, 0x3c};

  // =========================================================================
  // TIMELINE/KEYFRAME COLORS
  // =========================================================================
  QColor keyframeDefault{0x5c, 0xb3, 0xff};
  QColor keyframeSelected{0xff, 0xc1, 0x07};
  QColor keyframeTangent{0x9f, 0x7a, 0xea};
  QColor playhead{0xe5, 0x4d, 0x42};
  QColor timelineTrack{0x1c, 0x21, 0x29};
  QColor timelineTrackAlt{0x22, 0x28, 0x32};

  // =========================================================================
  // AUDIO/WAVEFORM COLORS
  // =========================================================================
  QColor waveformFill{0x3b, 0x9e, 0xff};
  QColor waveformStroke{0x5c, 0xb3, 0xff};
  QColor waveformBackground{0x14, 0x18, 0x1e};
  QColor recordingActive{0xe5, 0x4d, 0x42};

  // =========================================================================
  // SPECIAL UI ELEMENTS
  // =========================================================================
  QColor scrollbarThumb{0x3a, 0x44, 0x52};
  QColor scrollbarThumbHover{0x4a, 0x56, 0x66};
  QColor scrollbarTrack{0x14, 0x18, 0x1e};
  QColor dragHighlight{0x3b, 0x9e, 0xff};
  QColor dropZone{0x1a, 0x3a, 0x5c};
};

/**
 * @brief Manages the editor's visual style and theme
 */
class NMStyleManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static NMStyleManager &instance();

  /**
   * @brief Initialize the style manager and apply the default theme
   * @param app The QApplication instance
   */
  void initialize(QApplication *app);

  /**
   * @brief Apply the dark theme to the application
   */
  void applyDarkTheme();

  /**
   * @brief Apply the light theme to the application
   */
  void applyLightTheme();

  /**
   * @brief Apply a specific theme to the application
   * @param theme Theme to apply
   */
  void applyTheme(Theme theme);

  /**
   * @brief Get the current theme
   */
  [[nodiscard]] Theme currentTheme() const { return m_currentTheme; }

  /**
   * @brief Get the current color palette
   */
  [[nodiscard]] const EditorPalette &palette() const { return m_palette; }

  /**
   * @brief Get spacing tokens
   */
  [[nodiscard]] const SpacingTokens &spacing() const { return m_spacing; }

  /**
   * @brief Get border radius tokens
   */
  [[nodiscard]] const RadiusTokens &radius() const { return m_radius; }

  /**
   * @brief Get typography tokens
   */
  [[nodiscard]] const TypographyTokens &typography() const {
    return m_typography;
  }

  /**
   * @brief Get button size tokens
   */
  [[nodiscard]] const ButtonSizeTokens &buttonSizes() const {
    return m_buttonSizes;
  }

  /**
   * @brief Get panel accent colors
   */
  [[nodiscard]] const PanelAccents &panelAccents() const {
    return m_panelAccents;
  }

  /**
   * @brief Get the default font for the editor
   */
  [[nodiscard]] QFont defaultFont() const { return m_defaultFont; }

  /**
   * @brief Get the monospace font (for code/console)
   */
  [[nodiscard]] QFont monospaceFont() const { return m_monospaceFont; }

  /**
   * @brief Get the icon size for toolbars
   */
  [[nodiscard]] int toolbarIconSize() const { return m_toolbarIconSize; }

  /**
   * @brief Get the icon size for menus
   */
  [[nodiscard]] int menuIconSize() const { return m_menuIconSize; }

  /**
   * @brief Set the UI scale factor (for high-DPI support)
   * @param scale Scale factor (1.0 = 100%, 1.5 = 150%, etc.)
   */
  void setUiScale(double scale);

  /**
   * @brief Get the current UI scale factor
   */
  [[nodiscard]] double uiScale() const { return m_uiScale; }

  /**
   * @brief Get the complete stylesheet for the application
   */
  [[nodiscard]] QString getStyleSheet() const;

  /**
   * @brief Convert a color to a CSS-compatible string
   */
  static QString colorToStyleString(const QColor &color);

  /**
   * @brief Convert a color with alpha to a CSS rgba string
   */
  static QString colorToRgbaString(const QColor &color, int alpha = 255);

  /**
   * @brief Configure a toolbar button with standard size and icon
   * @param button Button to configure (QPushButton or QToolButton)
   * @param icon Icon to set on the button
   */
  static void configureToolbarButton(QAbstractButton *button,
                                       const QIcon &icon = QIcon());

  /**
   * @brief Configure a square button with standard size
   * @param button Button to configure
   * @param size Size variant (squareSmall, squareMedium, squareLarge,
   * squareXLarge)
   * @param icon Optional icon to set
   */
  static void configureSquareButton(QAbstractButton *button, int size,
                                     const QIcon &icon = QIcon());

  /**
   * @brief Set button to a standard size
   * @param button Button to configure
   * @param width Width in pixels
   * @param height Height in pixels
   */
  static void setButtonSize(QAbstractButton *button, int width, int height);

signals:
  /**
   * @brief Emitted when the theme changes
   */
  void themeChanged();

  /**
   * @brief Emitted when the UI scale changes
   */
  void scaleChanged(double newScale);

private:
  NMStyleManager();
  ~NMStyleManager() override = default;

  // Prevent copying
  NMStyleManager(const NMStyleManager &) = delete;
  NMStyleManager &operator=(const NMStyleManager &) = delete;

  void setupFonts();
  void setupHighDpi();
  EditorPalette createDarkPalette() const;
  EditorPalette createLightPalette() const;

  QApplication *m_app = nullptr;
  Theme m_currentTheme = Theme::Dark;
  EditorPalette m_palette;
  SpacingTokens m_spacing;
  RadiusTokens m_radius;
  TypographyTokens m_typography;
  ButtonSizeTokens m_buttonSizes;
  PanelAccents m_panelAccents;
  QFont m_defaultFont;
  QFont m_monospaceFont;
  double m_uiScale = 1.0;
  int m_toolbarIconSize = 24;
  int m_menuIconSize = 16;
};

} // namespace NovelMind::editor::qt
