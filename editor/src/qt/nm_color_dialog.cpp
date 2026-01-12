#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "nm_dialogs_detail.hpp"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMColorDialog::NMColorDialog(QWidget* parent, const QColor& initial, const QString& title)
    : QDialog(parent) {
  setWindowTitle(title.isEmpty() ? tr("Select Color") : title);
  setModal(true);
  setObjectName("NMColorDialog");
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  m_preview = new QFrame(this);
  m_preview->setFixedHeight(32);
  m_preview->setFrameShape(QFrame::StyledPanel);
  layout->addWidget(m_preview);

  auto* formLayout = new QFormLayout();
  formLayout->setSpacing(8);

  m_redSpin = new QSpinBox(this);
  m_greenSpin = new QSpinBox(this);
  m_blueSpin = new QSpinBox(this);
  m_redSpin->setRange(0, 255);
  m_greenSpin->setRange(0, 255);
  m_blueSpin->setRange(0, 255);

  formLayout->addRow(tr("Red"), m_redSpin);
  formLayout->addRow(tr("Green"), m_greenSpin);
  formLayout->addRow(tr("Blue"), m_blueSpin);

  m_hexEdit = new QLineEdit(this);
  m_hexEdit->setPlaceholderText(tr("#RRGGBB"));
  formLayout->addRow(tr("Hex"), m_hexEdit);

  layout->addLayout(formLayout);

  auto* buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  auto& iconMgr = NMIconManager::instance();

  m_okButton = new QPushButton(tr("OK"), this);
  m_okButton->setIcon(iconMgr.getIcon("status-success", 16));
  m_okButton->setObjectName("NMPrimaryButton");
  m_okButton->setDefault(true);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setIcon(iconMgr.getIcon("file-close", 16));
  m_cancelButton->setObjectName("NMSecondaryButton");

  connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  buttonLayout->addWidget(m_cancelButton);
  buttonLayout->addWidget(m_okButton);

  layout->addLayout(buttonLayout);

  connect(m_redSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this]() { updatePreview(); });
  connect(m_greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this]() { updatePreview(); });
  connect(m_blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this]() { updatePreview(); });
  connect(m_hexEdit, &QLineEdit::editingFinished, this, &NMColorDialog::syncFromHex);

  setColor(initial);
  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

void NMColorDialog::setColor(const QColor& color, bool updateFields) {
  QColor safeColor = color.isValid() ? color : QColor(255, 255, 255);
  if (updateFields) {
    const QSignalBlocker blockerRed(m_redSpin);
    const QSignalBlocker blockerGreen(m_greenSpin);
    const QSignalBlocker blockerBlue(m_blueSpin);
    const QSignalBlocker blockerHex(m_hexEdit);

    m_redSpin->setValue(safeColor.red());
    m_greenSpin->setValue(safeColor.green());
    m_blueSpin->setValue(safeColor.blue());
    m_hexEdit->setText(safeColor.name(QColor::HexRgb));
  }
  updatePreview();
}

QColor NMColorDialog::currentColor() const {
  return QColor(m_redSpin->value(), m_greenSpin->value(), m_blueSpin->value());
}

void NMColorDialog::syncFromHex() {
  QString text = m_hexEdit->text().trimmed();
  if (text.startsWith('#')) {
    text.remove(0, 1);
  }
  if (text.size() != 6) {
    m_hexEdit->setText(currentColor().name(QColor::HexRgb));
    return;
  }

  bool ok = false;
  const int value = text.toInt(&ok, 16);
  if (!ok) {
    m_hexEdit->setText(currentColor().name(QColor::HexRgb));
    return;
  }

  const QColor color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
  setColor(color);
}

void NMColorDialog::updatePreview() {
  const auto& palette = NMStyleManager::instance().palette();
  const QString colorName = currentColor().name(QColor::HexRgb);
  if (m_preview) {
    m_preview->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                 .arg(colorName)
                                 .arg(NMStyleManager::colorToStyleString(palette.borderLight)));
  }
  if (m_hexEdit) {
    const QSignalBlocker blocker(m_hexEdit);
    m_hexEdit->setText(colorName);
  }
}

QColor NMColorDialog::getColor(const QColor& initial, QWidget* parent, const QString& title,
                               bool* ok) {
  NMColorDialog dialog(parent, initial, title);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  return result == QDialog::Accepted ? dialog.currentColor() : initial;
}

} // namespace NovelMind::editor::qt
