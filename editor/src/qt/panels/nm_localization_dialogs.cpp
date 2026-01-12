#include "NovelMind/editor/qt/panels/nm_localization_dialogs.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_data_model.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_search.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

bool NMLocalizationDialogs::showAddKeyDialog(NMLocalizationPanel* panel,
                                             NMLocalizationSearch& searchHelper, QString& outKey,
                                              QString& outDefaultValue) {
  if (!panel) {
    return false;
  }

  QDialog dialog(panel);
  dialog.setWindowTitle(QObject::tr("Add Localization Key"));
  dialog.setMinimumWidth(400);

  QVBoxLayout* layout = new QVBoxLayout(&dialog);

  QFormLayout* formLayout = new QFormLayout();

  QLineEdit* keyEdit = new QLineEdit(&dialog);
  keyEdit->setPlaceholderText(QObject::tr("e.g., menu.button.start"));
  formLayout->addRow(QObject::tr("Key:"), keyEdit);

  QLineEdit* valueEdit = new QLineEdit(&dialog);
  valueEdit->setPlaceholderText(QObject::tr("Default value (optional)"));
  formLayout->addRow(QObject::tr("Default Value:"), valueEdit);

  QLabel* errorLabel = new QLabel(&dialog);
  errorLabel->setStyleSheet("color: red;");
  errorLabel->hide();

  layout->addLayout(formLayout);
  layout->addWidget(errorLabel);

  QDialogButtonBox* buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok);
  okButton->setEnabled(false);

  layout->addWidget(buttonBox);

  // Validation on key change
  QObject::connect(keyEdit, &QLineEdit::textChanged,
                   [&searchHelper, okButton, errorLabel, keyEdit](const QString& text) {
                     QString error;
                     bool valid = true;

                     if (text.isEmpty()) {
                       valid = false;
                       error = QObject::tr("Key cannot be empty");
                     } else if (!searchHelper.isValidKeyName(text)) {
                       valid = false;
                       error = QObject::tr(
                           "Key must contain only letters, numbers, underscore, dot, or dash");
                     } else if (!searchHelper.isKeyUnique(text)) {
                       valid = false;
                       error = QObject::tr("Key already exists");
                     }

                     okButton->setEnabled(valid);
                     if (!valid && !text.isEmpty()) {
                       errorLabel->setText(error);
                       errorLabel->show();
                     } else {
                       errorLabel->hide();
                     }
                   });

  QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  outKey = keyEdit->text().trimmed();
  outDefaultValue = valueEdit->text();
  return true;
}

bool NMLocalizationDialogs::showPluralFormsDialog(NMLocalizationPanel* panel,
                                                    NMLocalizationDataModel& dataModel,
                                                    const QString& key) {
  using namespace NovelMind::localization;

  if (!panel) {
    return false;
  }

  // Access panel's private members through accessor methods
  const QString currentLocale = panel->currentLocale();

  QDialog dialog(panel);
  dialog.setWindowTitle(QObject::tr("Edit Plural Forms - %1").arg(key));
  dialog.setMinimumWidth(600);
  dialog.setMinimumHeight(400);

  QVBoxLayout* layout = new QVBoxLayout(&dialog);

  // Instructions
  QLabel* infoLabel =
      new QLabel(QObject::tr("Define plural forms for different count values:\n"
                             "Zero (0), One (1), Two (2), Few (3-10), Many (11+), Other (default)"),
                 &dialog);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  // Get current localized string
  LocaleId locale;
  locale.language = currentLocale.toStdString();
  auto& localization = dataModel.localizationManager();
  auto* table = localization.getStringTableMutable(locale);

  QHash<PluralCategory, QLineEdit*> formEdits;

  QFormLayout* formLayout = new QFormLayout();

  const QList<QPair<PluralCategory, QString>> categories = {
      {PluralCategory::Zero, QObject::tr("Zero (count = 0):")},
      {PluralCategory::One, QObject::tr("One (count = 1):")},
      {PluralCategory::Two, QObject::tr("Two (count = 2):")},
      {PluralCategory::Few, QObject::tr("Few (3-10):")},
      {PluralCategory::Many, QObject::tr("Many (11+):")},
      {PluralCategory::Other, QObject::tr("Other (default):")}};

  for (const auto& [category, label] : categories) {
    QLineEdit* edit = new QLineEdit(&dialog);
    if (table) {
      const auto& strings = table->getStrings();
      auto it = strings.find(key.toStdString());
      if (it != strings.end()) {
        auto formIt = it->second.forms.find(category);
        if (formIt != it->second.forms.end()) {
          edit->setText(QString::fromStdString(formIt->second));
        }
      }
    }
    formEdits[category] = edit;
    formLayout->addRow(label, edit);
  }

  layout->addLayout(formLayout);

  QDialogButtonBox* buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttonBox);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  // Save plural forms
  if (table) {
    std::unordered_map<PluralCategory, std::string> forms;
    for (auto it = formEdits.begin(); it != formEdits.end(); ++it) {
      const QString text = it.value()->text();
      if (!text.isEmpty()) {
        forms[it.key()] = text.toStdString();
      }
    }

    if (!forms.empty()) {
      table->addPluralString(key.toStdString(), forms);
      // Note: Panel needs to handle dirty state and rebuild
    }
  }

  return true;
}

} // namespace NovelMind::editor::qt
