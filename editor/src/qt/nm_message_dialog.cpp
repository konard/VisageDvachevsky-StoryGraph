#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "nm_dialogs_detail.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

namespace {

QString dialogButtonText(NMDialogButton button) {
  switch (button) {
  case NMDialogButton::Ok:
    return QObject::tr("OK");
  case NMDialogButton::Cancel:
    return QObject::tr("Cancel");
  case NMDialogButton::Yes:
    return QObject::tr("Yes");
  case NMDialogButton::No:
    return QObject::tr("No");
  case NMDialogButton::Save:
    return QObject::tr("Save");
  case NMDialogButton::Discard:
    return QObject::tr("Discard");
  case NMDialogButton::Close:
    return QObject::tr("Close");
  case NMDialogButton::None:
    break;
  }
  return QObject::tr("OK");
}

QString dialogIconText(NMMessageType type) {
  switch (type) {
  case NMMessageType::Info:
    return "i";
  case NMMessageType::Warning:
    return "!";
  case NMMessageType::Error:
    return "x";
  case NMMessageType::Question:
    return "?";
  }
  return "i";
}

QColor dialogIconColor(NMMessageType type) {
  const auto& palette = NMStyleManager::instance().palette();
  switch (type) {
  case NMMessageType::Info:
    return palette.info;
  case NMMessageType::Warning:
    return palette.warning;
  case NMMessageType::Error:
    return palette.error;
  case NMMessageType::Question:
    return palette.accentPrimary;
  }
  return palette.info;
}

} // namespace

NMMessageDialog::NMMessageDialog(QWidget* parent, const QString& title, const QString& message,
                                 NMMessageType type, const QList<NMDialogButton>& buttons,
                                 NMDialogButton defaultButton)
    : QDialog(parent) {
  setWindowTitle(title);
  setModal(true);
  setObjectName("NMMessageDialog");
  setMinimumWidth(320);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  buildUi(message, type, buttons, defaultButton);
  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

void NMMessageDialog::buildUi(const QString& message, NMMessageType type,
                              const QList<NMDialogButton>& buttons, NMDialogButton defaultButton) {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(8);

  auto* contentLayout = new QHBoxLayout();
  contentLayout->setSpacing(10);

  auto* iconLabel = new QLabel(this);
  iconLabel->setFixedSize(28, 28);
  iconLabel->setAlignment(Qt::AlignCenter);
  iconLabel->setText(dialogIconText(type));
  const QColor iconColor = dialogIconColor(type);
  iconLabel->setStyleSheet(QString("background-color: %1; color: %2; "
                                   "border-radius: 16px; font-weight: 700;")
                               .arg(NMStyleManager::colorToStyleString(iconColor))
                               .arg(NMStyleManager::colorToStyleString(
                                   NMStyleManager::instance().palette().textPrimary)));

  auto* messageLabel = new QLabel(message, this);
  messageLabel->setWordWrap(true);
  messageLabel->setObjectName("NMMessageText");

  contentLayout->addWidget(iconLabel, 0, Qt::AlignTop);
  contentLayout->addWidget(messageLabel, 1);
  mainLayout->addLayout(contentLayout);

  auto* buttonsLayout = new QHBoxLayout();
  buttonsLayout->addStretch();

  for (const auto button : buttons) {
    auto* btn = new QPushButton(dialogButtonText(button), this);
    btn->setObjectName(button == defaultButton ? "NMPrimaryButton" : "NMSecondaryButton");
    if (button == defaultButton) {
      btn->setDefault(true);
      btn->setAutoDefault(true);
    }
    connect(btn, &QPushButton::clicked, this, [this, button]() {
      m_choice = button;
      accept();
    });
    buttonsLayout->addWidget(btn);
  }

  mainLayout->addLayout(buttonsLayout);
}

NMDialogButton NMMessageDialog::showInfo(QWidget* parent, const QString& title,
                                         const QString& message) {
  NMMessageDialog dialog(parent, title, message, NMMessageType::Info, {NMDialogButton::Ok},
                         NMDialogButton::Ok);
  dialog.exec();
  return dialog.choice();
}

NMDialogButton NMMessageDialog::showWarning(QWidget* parent, const QString& title,
                                            const QString& message) {
  NMMessageDialog dialog(parent, title, message, NMMessageType::Warning, {NMDialogButton::Ok},
                         NMDialogButton::Ok);
  dialog.exec();
  return dialog.choice();
}

NMDialogButton NMMessageDialog::showError(QWidget* parent, const QString& title,
                                          const QString& message) {
  NMMessageDialog dialog(parent, title, message, NMMessageType::Error, {NMDialogButton::Ok},
                         NMDialogButton::Ok);
  dialog.exec();
  return dialog.choice();
}

NMDialogButton NMMessageDialog::showQuestion(QWidget* parent, const QString& title,
                                             const QString& message,
                                             const QList<NMDialogButton>& buttons,
                                             NMDialogButton defaultButton) {
  NMMessageDialog dialog(parent, title, message, NMMessageType::Question, buttons, defaultButton);
  dialog.exec();
  return dialog.choice();
}

} // namespace NovelMind::editor::qt
