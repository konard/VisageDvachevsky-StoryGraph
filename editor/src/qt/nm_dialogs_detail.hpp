#pragma once

/**
 * @file nm_dialogs_detail.hpp
 * @brief D4: Standard Dialog System Helpers
 *
 * Provides unified dialog styling and behavior for the NovelMind Editor.
 * This ensures consistent UX across all dialogs:
 * - Standard button layouts (primary action on right, Cancel on left)
 * - Consistent styling and colors
 * - Input validation visual feedback
 * - Keyboard navigation (Enter confirms, Escape cancels)
 * - Modal behavior with backdrop dimming
 */

#include <QString>
#include <functional>

class QDialog;
class QLineEdit;
class QPushButton;
class QWidget;
class QHBoxLayout;

namespace NovelMind::editor::qt::detail {

/**
 * @brief D4: Apply standard dialog frame styling
 *
 * Sets consistent background, border, and widget styles
 * for all NovelMind dialogs.
 */
void applyDialogFrameStyle(QDialog* dialog);

/**
 * @brief D4: Animate dialog entrance
 *
 * Provides smooth fade-in animation when dialog appears.
 */
void animateDialogIn(QDialog* dialog);

/**
 * @brief D4: Create standard button bar layout
 *
 * Returns a horizontal layout with standard button placement:
 * - Secondary buttons (Cancel, etc.) on the left
 * - Primary button (OK, Save, Create, etc.) on the right
 * - Consistent spacing and sizing
 *
 * @param primaryText Text for primary button (e.g., "OK", "Save")
 * @param secondaryText Text for secondary button (e.g., "Cancel")
 * @param outPrimary Output pointer to primary button
 * @param outSecondary Output pointer to secondary button
 * @param parent Parent widget
 * @return Configured button bar layout
 */
QHBoxLayout* createStandardButtonBar(const QString& primaryText, const QString& secondaryText,
                                     QPushButton** outPrimary, QPushButton** outSecondary,
                                     QWidget* parent);

/**
 * @brief D4: Apply validation styling to input field
 *
 * Shows visual feedback when input validation fails:
 * - Red border for invalid input
 * - Green border for valid input
 * - Tooltip with error message
 *
 * @param lineEdit The input field to style
 * @param isValid Whether the current input is valid
 * @param errorMessage Error message to show (empty for valid state)
 */
void applyValidationStyle(QLineEdit* lineEdit, bool isValid,
                          const QString& errorMessage = QString());

/**
 * @brief D4: Setup input validation with callback
 *
 * Connects a validation function to an input field that updates
 * visual feedback in real-time as the user types.
 *
 * @param lineEdit The input field to validate
 * @param validator Function that returns true if input is valid
 * @param errorMessageProvider Function that returns error message for invalid
 * input
 * @param onValidChanged Callback when validity state changes
 */
void setupInputValidation(QLineEdit* lineEdit, std::function<bool(const QString&)> validator,
                          std::function<QString(const QString&)> errorMessageProvider = nullptr,
                          std::function<void(bool)> onValidChanged = nullptr);

/**
 * @brief D4: Configure dialog for standard keyboard behavior
 *
 * Sets up:
 * - Enter key triggers primary button
 * - Escape key closes dialog with rejection
 * - Tab order between widgets
 *
 * @param dialog The dialog to configure
 * @param primaryButton The primary action button
 */
void setupDialogKeyboardBehavior(QDialog* dialog, QPushButton* primaryButton);

/**
 * @brief D7: Standard dialog minimum sizes
 */
constexpr int DIALOG_MIN_WIDTH = 400;
constexpr int DIALOG_MIN_HEIGHT = 200;
constexpr int DIALOG_BUTTON_MIN_WIDTH = 80;
constexpr int DIALOG_BUTTON_HEIGHT = 32;
constexpr int DIALOG_MARGIN = 16;
constexpr int DIALOG_SPACING = 12;

} // namespace NovelMind::editor::qt::detail
