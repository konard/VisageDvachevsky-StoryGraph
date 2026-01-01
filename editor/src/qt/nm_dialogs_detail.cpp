/**
 * @file nm_dialogs_detail.cpp
 * @brief D4: Standard Dialog System Implementation
 */

#include "nm_dialogs_detail.hpp"

#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

namespace NovelMind::editor::qt::detail {

void applyDialogFrameStyle(QDialog *dialog) {
  if (!dialog) {
    return;
  }
  const auto &p = NMStyleManager::instance().palette();

  // D4: Enhanced dialog styling for consistent UX
  dialog->setStyleSheet(
      QString(R"(
    QDialog {
      background-color: %1;
      border: 1px solid %2;
    }
    QLabel#NMMessageText {
      color: %3;
      font-size: 13px;
    }
    QLabel#NMDialogTitle {
      color: %3;
      font-size: 16px;
      font-weight: bold;
    }
    QLabel#NMDialogSubtitle {
      color: %10;
      font-size: 12px;
    }
    QPushButton#NMPrimaryButton {
      background-color: %4;
      color: %5;
      border: none;
      border-radius: 4px;
      padding: 8px 16px;
      font-weight: 600;
      min-width: %11px;
      min-height: %12px;
    }
    QPushButton#NMPrimaryButton:hover {
      background-color: %6;
    }
    QPushButton#NMPrimaryButton:pressed {
      background-color: %4;
    }
    QPushButton#NMPrimaryButton:disabled {
      background-color: %7;
      color: %10;
    }
    QPushButton#NMSecondaryButton {
      background-color: %7;
      color: %5;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 8px 16px;
      min-width: %11px;
      min-height: %12px;
    }
    QPushButton#NMSecondaryButton:hover {
      background-color: %9;
      border-color: %4;
    }
    QPushButton#NMSecondaryButton:pressed {
      background-color: %7;
    }
    QLineEdit {
      background-color: %7;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 6px 10px;
      color: %3;
    }
    QLineEdit:focus {
      border-color: %4;
    }
    QLineEdit#NMValidInput {
      border-color: #4caf50;
    }
    QLineEdit#NMInvalidInput {
      border-color: #f44336;
    }
    QComboBox {
      background-color: %7;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 6px 10px;
      color: %3;
    }
    QComboBox:focus {
      border-color: %4;
    }
    QComboBox::drop-down {
      border: none;
      width: 20px;
    }
    QSpinBox, QDoubleSpinBox {
      background-color: %7;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 6px 10px;
      color: %3;
    }
    QSpinBox:focus, QDoubleSpinBox:focus {
      border-color: %4;
    }
  )")
          .arg(NMStyleManager::colorToStyleString(p.bgDark))
          .arg(NMStyleManager::colorToStyleString(p.borderLight))
          .arg(NMStyleManager::colorToStyleString(p.textPrimary))
          .arg(NMStyleManager::colorToStyleString(p.accentPrimary))
          .arg(NMStyleManager::colorToStyleString(p.textPrimary))
          .arg(NMStyleManager::colorToStyleString(p.accentHover))
          .arg(NMStyleManager::colorToStyleString(p.bgMedium))
          .arg(NMStyleManager::colorToStyleString(p.borderLight))
          .arg(NMStyleManager::colorToStyleString(p.bgLight))
          .arg(NMStyleManager::colorToStyleString(p.textSecondary))
          .arg(DIALOG_BUTTON_MIN_WIDTH)
          .arg(DIALOG_BUTTON_HEIGHT));
}

void animateDialogIn(QDialog *dialog) {
  if (!dialog) {
    return;
  }
  dialog->setWindowOpacity(0.0);
  QTimer::singleShot(0, dialog, [dialog]() {
    if (!dialog || !dialog->isVisible()) {
      return;
    }
    auto *anim = new QPropertyAnimation(dialog, "windowOpacity", dialog);
    anim->setDuration(160);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  });
}

QHBoxLayout *createStandardButtonBar(const QString &primaryText,
                                     const QString &secondaryText,
                                     QPushButton **outPrimary,
                                     QPushButton **outSecondary,
                                     QWidget *parent) {
  auto *layout = new QHBoxLayout();
  layout->setContentsMargins(0, DIALOG_SPACING, 0, 0);
  layout->setSpacing(DIALOG_SPACING);

  // D4: Secondary button on left, primary on right (platform standard)
  auto *secondaryButton = new QPushButton(secondaryText, parent);
  secondaryButton->setObjectName("NMSecondaryButton");
  secondaryButton->setMinimumWidth(DIALOG_BUTTON_MIN_WIDTH);
  secondaryButton->setMinimumHeight(DIALOG_BUTTON_HEIGHT);

  auto *primaryButton = new QPushButton(primaryText, parent);
  primaryButton->setObjectName("NMPrimaryButton");
  primaryButton->setMinimumWidth(DIALOG_BUTTON_MIN_WIDTH);
  primaryButton->setMinimumHeight(DIALOG_BUTTON_HEIGHT);
  primaryButton->setDefault(true);

  layout->addWidget(secondaryButton);
  layout->addStretch();
  layout->addWidget(primaryButton);

  if (outPrimary) {
    *outPrimary = primaryButton;
  }
  if (outSecondary) {
    *outSecondary = secondaryButton;
  }

  return layout;
}

void applyValidationStyle(QLineEdit *lineEdit, bool isValid,
                          const QString &errorMessage) {
  if (!lineEdit) {
    return;
  }

  if (isValid) {
    lineEdit->setObjectName("NMValidInput");
    lineEdit->setToolTip(QString());
  } else {
    lineEdit->setObjectName("NMInvalidInput");
    lineEdit->setToolTip(errorMessage);
  }

  // Force style refresh
  lineEdit->style()->unpolish(lineEdit);
  lineEdit->style()->polish(lineEdit);
}

void setupInputValidation(
    QLineEdit *lineEdit, std::function<bool(const QString &)> validator,
    std::function<QString(const QString &)> errorMessageProvider,
    std::function<void(bool)> onValidChanged) {
  if (!lineEdit || !validator) {
    return;
  }

  // Track previous validity state to only call callback on changes
  auto *prevValid = new bool(true);

  QObject::connect(lineEdit, &QLineEdit::textChanged,
                   [lineEdit, validator, errorMessageProvider, onValidChanged,
                    prevValid](const QString &text) {
                     bool isValid = validator(text);
                     QString errorMsg;
                     if (!isValid && errorMessageProvider) {
                       errorMsg = errorMessageProvider(text);
                     }
                     applyValidationStyle(lineEdit, isValid, errorMsg);

                     if (onValidChanged && isValid != *prevValid) {
                       onValidChanged(isValid);
                     }
                     *prevValid = isValid;
                   });

  // Clean up tracking variable when widget is destroyed
  QObject::connect(lineEdit, &QObject::destroyed,
                   [prevValid]() { delete prevValid; });
}

void setupDialogKeyboardBehavior(QDialog *dialog, QPushButton *primaryButton) {
  if (!dialog) {
    return;
  }

  // D4: Enter key triggers primary button
  if (primaryButton) {
    primaryButton->setDefault(true);
    primaryButton->setAutoDefault(true);
  }

  // Escape key behavior is automatic in QDialog
}

} // namespace NovelMind::editor::qt::detail
