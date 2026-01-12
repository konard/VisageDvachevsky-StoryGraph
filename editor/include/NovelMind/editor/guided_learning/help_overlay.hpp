#pragma once

/**
 * @file help_overlay.hpp
 * @brief Help Overlay - visual layer for tutorials and hints
 *
 * Provides the visual rendering layer for the guided learning system.
 * This is a single overlay widget that sits on top of the entire editor
 * and renders spotlights, callouts, and hint bubbles.
 *
 * Key features:
 * - Single overlay layer (no multiple overlapping widgets)
 * - Spotlight effect (dim everything except target)
 * - Callout bubbles with arrows
 * - Minimal, clean, professional appearance
 * - DPI-aware rendering
 * - Smooth animations (optional)
 */

#include "NovelMind/editor/guided_learning/tutorial_types.hpp"
#include <QColor>
#include <QFont>
#include <QPropertyAnimation>
#include <QRect>
#include <QWidget>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::editor::guided_learning {

/**
 * @brief Style configuration for the overlay
 */
struct OverlayStyle {
  // Spotlight
  QColor spotlightDimColor = QColor(0, 0, 0, 180); // Semi-transparent black
  int spotlightPadding = 8;                        // Padding around target
  int spotlightCornerRadius = 4;                   // Rounded corners

  // Callout
  QColor calloutBackground = QColor(45, 45, 48);   // Dark background
  QColor calloutBorder = QColor(78, 78, 82);       // Subtle border
  QColor calloutText = QColor(220, 220, 220);      // Light text
  QColor calloutTitleText = QColor(255, 255, 255); // Brighter title
  int calloutPadding = 16;
  int calloutCornerRadius = 6;
  int calloutMaxWidth = 320;
  int calloutArrowSize = 12;

  // Buttons
  QColor buttonBackground = QColor(62, 62, 66);
  QColor buttonBackgroundHover = QColor(78, 78, 82);
  QColor buttonText = QColor(220, 220, 220);
  QColor buttonPrimaryBackground = QColor(0, 122, 204);
  QColor buttonPrimaryBackgroundHover = QColor(28, 151, 234);
  QColor buttonPrimaryText = QColor(255, 255, 255);
  int buttonPadding = 8;
  int buttonCornerRadius = 4;

  // Tooltip (smaller, simpler)
  QColor tooltipBackground = QColor(60, 60, 63);
  QColor tooltipBorder = QColor(80, 80, 84);
  QColor tooltipText = QColor(200, 200, 200);
  int tooltipPadding = 8;
  int tooltipMaxWidth = 250;

  // Fonts
  QFont titleFont;
  QFont contentFont;
  QFont buttonFont;

  // Animation
  bool enableAnimations = true;
  int animationDurationMs = 200;
};

/**
 * @brief Active callout/hint being displayed
 */
struct ActiveHint {
  std::string id;
  std::string title;
  std::string content;
  HintType type = HintType::Callout;
  CalloutPosition position = CalloutPosition::Auto;

  QRect targetRect;  // Target element rectangle
  QRect calloutRect; // Calculated callout position

  bool showBackButton = false;
  bool showNextButton = true;
  bool showSkipButton = true;
  bool showCloseButton = true;
  bool showDontShowAgain = false;

  std::string nextButtonText = "Next";
  std::string backButtonText = "Back";

  // Step indicator (for tutorials)
  bool showStepIndicator = false;
  int currentStep = 0;
  int totalSteps = 0;

  // Auto-hide timer
  bool autoHide = false;
  int autoHideMs = 0;
};

/**
 * @brief Help Overlay Widget
 *
 * This widget covers the entire editor window and handles all
 * visual rendering for the guided learning system.
 */
class NMHelpOverlay : public QWidget {
  Q_OBJECT

  Q_PROPERTY(qreal spotlightOpacity READ spotlightOpacity WRITE setSpotlightOpacity)
  Q_PROPERTY(qreal calloutOpacity READ calloutOpacity WRITE setCalloutOpacity)

public:
  /**
   * @brief Construct the overlay
   * @param parent Parent widget (usually the main window)
   */
  explicit NMHelpOverlay(QWidget* parent = nullptr);
  ~NMHelpOverlay() override;

  // ========================================================================
  // Display Control
  // ========================================================================

  /**
   * @brief Show a tutorial step
   */
  void showTutorialStep(const std::string& stepId, const std::string& title,
                        const std::string& content, const std::string& anchorId, HintType hintType,
                        CalloutPosition position, int currentStep, int totalSteps, bool showBack,
                        bool showSkip, bool showDontShowAgain);

  /**
   * @brief Show a contextual hint
   */
  void showHint(const std::string& hintId, const std::string& content, const std::string& anchorId,
                HintType hintType, CalloutPosition position, bool autoHide = false,
                int autoHideMs = 5000);

  /**
   * @brief Hide a specific hint/step
   */
  void hideHint(const std::string& hintId);

  /**
   * @brief Hide all hints and steps
   */
  void hideAll();

  /**
   * @brief Check if any hint is currently visible
   */
  [[nodiscard]] bool hasVisibleHints() const;

  /**
   * @brief Check if a specific hint is visible
   */
  [[nodiscard]] bool isHintVisible(const std::string& hintId) const;

  // ========================================================================
  // Style Configuration
  // ========================================================================

  /**
   * @brief Set the overlay style
   */
  void setStyle(const OverlayStyle& style);

  /**
   * @brief Get the current style
   */
  [[nodiscard]] const OverlayStyle& style() const { return m_style; }

  /**
   * @brief Set spotlight opacity (for animations)
   */
  void setSpotlightOpacity(qreal opacity);
  [[nodiscard]] qreal spotlightOpacity() const { return m_spotlightOpacity; }

  /**
   * @brief Set callout opacity (for animations)
   */
  void setCalloutOpacity(qreal opacity);
  [[nodiscard]] qreal calloutOpacity() const { return m_calloutOpacity; }

signals:
  /**
   * @brief User clicked the Next button
   */
  void nextClicked();

  /**
   * @brief User clicked the Back button
   */
  void backClicked();

  /**
   * @brief User clicked the Skip button
   */
  void skipClicked();

  /**
   * @brief User clicked the Close button
   */
  void closeClicked();

  /**
   * @brief User toggled "Don't show again"
   */
  void dontShowAgainToggled(bool checked);

  /**
   * @brief Hint auto-hide timer expired
   */
  void hintAutoHidden(const QString& hintId);

protected:
  // Qt event overrides
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool event(QEvent* event) override;

private:
  // Painting helpers
  void paintSpotlight(QPainter& painter, const QRect& targetRect);
  void paintCallout(QPainter& painter, const ActiveHint& hint);
  void paintTooltip(QPainter& painter, const ActiveHint& hint);
  void paintButtons(QPainter& painter, const ActiveHint& hint, QRect& buttonArea);
  void paintStepIndicator(QPainter& painter, const ActiveHint& hint, const QRect& area);

  // Layout calculation
  QRect calculateCalloutRect(const QRect& targetRect, const QSize& contentSize,
                             CalloutPosition position);
  CalloutPosition determineAutoPosition(const QRect& targetRect, const QSize& contentSize);
  QSize calculateContentSize(const ActiveHint& hint);

  // Button hit testing
  struct ButtonRects {
    QRect nextButton;
    QRect backButton;
    QRect skipButton;
    QRect closeButton;
    QRect dontShowAgainCheckbox;
  };
  ButtonRects m_buttonRects;
  std::optional<std::string> m_hoveredButton;

  // Update hint positions when target moves
  void updateHintPositions();

  // Animation helpers
  void animateIn();
  void animateOut(std::function<void()> onComplete);

  // Auto-hide timer
  void startAutoHideTimer(const std::string& hintId, int ms);
  void cancelAutoHideTimer(const std::string& hintId);

  // State
  std::vector<ActiveHint> m_activeHints;
  OverlayStyle m_style;

  qreal m_spotlightOpacity = 1.0;
  qreal m_calloutOpacity = 1.0;

  std::unique_ptr<QPropertyAnimation> m_spotlightAnimation;
  std::unique_ptr<QPropertyAnimation> m_calloutAnimation;

  // Auto-hide timers (hintId -> timer)
  std::unordered_map<std::string, std::unique_ptr<QTimer>> m_autoHideTimers;

  bool m_dontShowAgainChecked = false;

  // Position update timer
  QTimer* m_positionUpdateTimer = nullptr;
};

} // namespace NovelMind::editor::guided_learning
