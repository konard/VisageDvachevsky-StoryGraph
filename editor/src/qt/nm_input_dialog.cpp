#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "nm_dialogs_detail.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMInputDialog::NMInputDialog(QWidget *parent, const QString &title,
                             const QString &label, InputType type)
    : QDialog(parent), m_type(type) {
  setWindowTitle(title);
  setModal(true);
  setObjectName("NMInputDialog");
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  m_label = new QLabel(label, this);
  m_label->setWordWrap(true);
  layout->addWidget(m_label);

  if (type == InputType::Text) {
    m_textEdit = new QLineEdit(this);
    layout->addWidget(m_textEdit);
  } else if (type == InputType::Int) {
    m_intSpin = new QSpinBox(this);
    layout->addWidget(m_intSpin);
  } else if (type == InputType::Double) {
    m_doubleSpin = new QDoubleSpinBox(this);
    layout->addWidget(m_doubleSpin);
  } else if (type == InputType::Item) {
    m_comboBox = new QComboBox(this);
    layout->addWidget(m_comboBox);
  } else if (type == InputType::MultiLine) {
    m_multiLineEdit = new QTextEdit(this);
    m_multiLineEdit->setMinimumHeight(100);
    m_multiLineEdit->setAcceptRichText(false);
    layout->addWidget(m_multiLineEdit);
  }

  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_okButton = new QPushButton(tr("OK"), this);
  m_okButton->setObjectName("NMPrimaryButton");
  m_okButton->setDefault(true);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName("NMSecondaryButton");

  connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  buttonLayout->addWidget(m_cancelButton);
  buttonLayout->addWidget(m_okButton);

  layout->addLayout(buttonLayout);

  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

void NMInputDialog::configureText(const QString &text,
                                  QLineEdit::EchoMode mode) {
  if (m_textEdit) {
    m_textEdit->setEchoMode(mode);
    m_textEdit->setText(text);
    m_textEdit->selectAll();
    m_textEdit->setFocus();
  }
}

void NMInputDialog::configureInt(int value, int minValue, int maxValue,
                                 int step) {
  if (m_intSpin) {
    m_intSpin->setRange(minValue, maxValue);
    m_intSpin->setSingleStep(step);
    m_intSpin->setValue(value);
    m_intSpin->setFocus();
  }
}

void NMInputDialog::configureDouble(double value, double minValue,
                                    double maxValue, int decimals) {
  if (m_doubleSpin) {
    m_doubleSpin->setRange(minValue, maxValue);
    m_doubleSpin->setDecimals(decimals);
    m_doubleSpin->setValue(value);
    m_doubleSpin->setFocus();
  }
}

QString NMInputDialog::textValue() const {
  return m_textEdit ? m_textEdit->text() : QString();
}

int NMInputDialog::intValue() const { return m_intSpin ? m_intSpin->value() : 0; }

double NMInputDialog::doubleValue() const {
  return m_doubleSpin ? m_doubleSpin->value() : 0.0;
}

void NMInputDialog::configureItem(const QStringList &items, int current,
                                  bool editable) {
  if (m_comboBox) {
    m_comboBox->setEditable(editable);
    m_comboBox->addItems(items);
    if (current >= 0 && current < items.size()) {
      m_comboBox->setCurrentIndex(current);
    }
    m_comboBox->setFocus();
  }
}

void NMInputDialog::configureMultiLine(const QString &text) {
  if (m_multiLineEdit) {
    m_multiLineEdit->setPlainText(text);
    m_multiLineEdit->selectAll();
    m_multiLineEdit->setFocus();
  }
}

QString NMInputDialog::itemValue() const {
  return m_comboBox ? m_comboBox->currentText() : QString();
}

QString NMInputDialog::multiLineValue() const {
  return m_multiLineEdit ? m_multiLineEdit->toPlainText() : QString();
}

QString NMInputDialog::getText(QWidget *parent, const QString &title,
                               const QString &label,
                               QLineEdit::EchoMode mode,
                               const QString &text, bool *ok) {
  NMInputDialog dialog(parent, title, label, InputType::Text);
  dialog.configureText(text, mode);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  return result == QDialog::Accepted ? dialog.textValue() : QString();
}

int NMInputDialog::getInt(QWidget *parent, const QString &title,
                          const QString &label, int value, int minValue,
                          int maxValue, int step, bool *ok) {
  NMInputDialog dialog(parent, title, label, InputType::Int);
  dialog.configureInt(value, minValue, maxValue, step);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  return result == QDialog::Accepted ? dialog.intValue() : value;
}

double NMInputDialog::getDouble(QWidget *parent, const QString &title,
                                const QString &label, double value,
                                double minValue, double maxValue, int decimals,
                                bool *ok) {
  NMInputDialog dialog(parent, title, label, InputType::Double);
  dialog.configureDouble(value, minValue, maxValue, decimals);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  return result == QDialog::Accepted ? dialog.doubleValue() : value;
}

QString NMInputDialog::getItem(QWidget *parent, const QString &title,
                               const QString &label, const QStringList &items,
                               int current, bool editable, bool *ok) {
  NMInputDialog dialog(parent, title, label, InputType::Item);
  dialog.configureItem(items, current, editable);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  if (result == QDialog::Accepted) {
    return dialog.itemValue();
  }
  return (current >= 0 && current < items.size()) ? items.at(current)
                                                   : QString();
}

QString NMInputDialog::getMultiLineText(QWidget *parent, const QString &title,
                                        const QString &label,
                                        const QString &text, bool *ok) {
  NMInputDialog dialog(parent, title, label, InputType::MultiLine);
  dialog.configureMultiLine(text);
  const int result = dialog.exec();
  if (ok) {
    *ok = (result == QDialog::Accepted);
  }
  return result == QDialog::Accepted ? dialog.multiLineValue() : QString();
}

} // namespace NovelMind::editor::qt
