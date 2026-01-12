#include "NovelMind/editor/qt/panels/nm_localization_ui.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace NovelMind::editor::qt {

NMLocalizationUI::NMLocalizationUI(NMLocalizationDataModel& dataModel,
                                   ::NovelMind::localization::LocalizationManager& localization)
    : m_dataModel(dataModel)
    , m_localization(localization) {}

NMLocalizationUI::~NMLocalizationUI() = default;

void NMLocalizationUI::rebuildTable(QTableWidget* table, const QString& currentLocale,
                                    const QString& defaultLocale,
                                    QHash<QString, int>& keyToRowMap) {
  if (!table) {
    return;
  }

  table->blockSignals(true);
  table->setRowCount(0);

  // Clear and rebuild key-to-row mapping
  keyToRowMap.clear();

  const auto& entries = m_dataModel.entries();
  QList<QString> sortedKeys = entries.keys();
  std::sort(sortedKeys.begin(), sortedKeys.end());

  // Pre-reserve capacity for the row map
  keyToRowMap.reserve(sortedKeys.size());

  int row = 0;
  for (const QString& key : sortedKeys) {
    const LocalizationEntry& entry = entries.value(key);

    if (entry.isDeleted) {
      continue;
    }

    table->insertRow(row);

    auto* idItem = new QTableWidgetItem(entry.key);
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, idItem);

    QString sourceText = entry.translations.value(defaultLocale);
    auto* sourceItem = new QTableWidgetItem(sourceText);
    sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 1, sourceItem);

    QString translationText = entry.translations.value(currentLocale);
    auto* translationItem = new QTableWidgetItem(translationText);
    table->setItem(row, 2, translationItem);

    // Store key-to-row mapping for O(1) lookup
    keyToRowMap.insert(key, row);

    row++;
  }

  table->blockSignals(false);
}

void NMLocalizationUI::applyFilters(QTableWidget* table, const QString& searchText,
                                    bool showMissingOnly) {
  if (!table) {
    return;
  }

  const QString lowerSearch = searchText.toLower();

  for (int row = 0; row < table->rowCount(); ++row) {
    auto* idItem = table->item(row, 0);
    auto* sourceItem = table->item(row, 1);
    auto* translationItem = table->item(row, 2);

    if (!idItem) {
      table->setRowHidden(row, true);
      continue;
    }

    const QString key = idItem->text();
    const QString source = sourceItem ? sourceItem->text() : QString();
    const QString translation = translationItem ? translationItem->text() : QString();

    bool isMissing = translation.isEmpty();

    // Apply search filter (case-insensitive)
    bool matchesSearch = lowerSearch.isEmpty() || key.toLower().contains(lowerSearch) ||
                         source.toLower().contains(lowerSearch) ||
                         translation.toLower().contains(lowerSearch);

    // Apply missing filter
    bool matchesMissingFilter = !showMissingOnly || isMissing;

    bool visible = matchesSearch && matchesMissingFilter;
    table->setRowHidden(row, !visible);
  }
}

void NMLocalizationUI::highlightMissingTranslations(QTableWidget* table) {
  if (!table) {
    return;
  }

  auto& style = NMStyleManager::instance();
  const auto& palette = style.palette();

  // Use warning colors for missing translations (more subtle for dark theme)
  const QColor missingBg = palette.warningSubtle;
  const QColor missingText = palette.warning;
  const QColor normalText = palette.textPrimary;
  const QColor mutedText = palette.textMuted;

  for (int row = 0; row < table->rowCount(); ++row) {
    auto* translationItem = table->item(row, 2);
    if (!translationItem) {
      continue;
    }

    const bool isMissing = translationItem->text().isEmpty();

    // Style the translation column
    if (isMissing) {
      translationItem->setBackground(missingBg);
      translationItem->setForeground(missingText);
      QFont font = translationItem->font();
      font.setItalic(true);
      translationItem->setFont(font);
    } else {
      translationItem->setBackground(QBrush()); // Clear custom background
      translationItem->setForeground(normalText);
      QFont font = translationItem->font();
      font.setItalic(false);
      translationItem->setFont(font);
    }

    // Add subtle visual indicator to key column for missing translations
    auto* keyItem = table->item(row, 0);
    if (keyItem) {
      if (isMissing) {
        keyItem->setForeground(mutedText);
        // Add warning indicator
        QVariant stored = keyItem->data(Qt::UserRole);
        if (!stored.isValid()) {
          keyItem->setData(Qt::UserRole, keyItem->text()); // Store original
          keyItem->setText("⚠ " + keyItem->text());
        }
      } else {
        keyItem->setForeground(palette.textPrimary);
        // Restore original text
        QVariant original = keyItem->data(Qt::UserRole);
        if (original.isValid()) {
          keyItem->setText(original.toString());
          keyItem->setData(Qt::UserRole, QVariant()); // Clear
        }
      }
    }

    // Style source column slightly dimmed
    auto* sourceItem = table->item(row, 1);
    if (sourceItem) {
      sourceItem->setForeground(palette.textSecondary);
    }
  }
}

void NMLocalizationUI::updateStatusBar(QLabel* statusLabel, QTableWidget* table, bool isDirty) {
  if (!statusLabel || !table) {
    return;
  }

  auto& style = NMStyleManager::instance();
  const auto& palette = style.palette();

  int totalCount = 0;
  int visibleCount = 0;
  int missingCount = 0;

  for (int row = 0; row < table->rowCount(); ++row) {
    totalCount++;
    if (!table->isRowHidden(row)) {
      visibleCount++;
    }
    auto* translationItem = table->item(row, 2);
    if (translationItem && translationItem->text().isEmpty()) {
      missingCount++;
    }
  }

  // Calculate completion percentage
  int completedCount = totalCount - missingCount;
  int completionPercent = totalCount > 0 ? (completedCount * 100) / totalCount : 100;

  // Build styled status message
  QString status;

  // Show visible/total keys
  status += QString("<span style='color: %1;'>Showing <b>%2</b> of <b>%3</b> keys</span>")
                .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
                .arg(visibleCount)
                .arg(totalCount);

  // Show missing count with color coding
  if (missingCount > 0) {
    status += QString(" <span style='color: %1;'>●</span> ")
                  .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
    status += QString("<span style='color: %1;'>Missing: <b>%2</b></span>")
                  .arg(NMStyleManager::colorToStyleString(palette.warning))
                  .arg(missingCount);
  }

  // Show completion percentage
  QString completionColor = completionPercent == 100
                                ? NMStyleManager::colorToStyleString(palette.success)
                                : NMStyleManager::colorToStyleString(palette.info);

  status += QString(" <span style='color: %1;'>●</span> ")
                .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
  status += QString("<span style='color: %1;'>Complete: <b>%2%</b></span>")
                .arg(completionColor)
                .arg(completionPercent);

  // Show modified indicator
  if (isDirty) {
    status += QString(" <span style='color: %1;'>●</span> ")
                  .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
    status += QString("<span style='color: %1; font-weight: 600;'>● MODIFIED</span>")
                  .arg(NMStyleManager::colorToStyleString(style.panelAccents().localization));
  }

  statusLabel->setText(status);
}

void NMLocalizationUI::updatePreview(QLabel* previewOutput, QTableWidget* table,
                                     const QHash<QString, QString>& previewVariables,
                                     const QString& currentLocale) {
  if (!previewOutput || !table) {
    return;
  }

  // Get currently selected key
  QList<QTableWidgetItem*> selectedItems = table->selectedItems();
  if (selectedItems.isEmpty()) {
    previewOutput->setText(QObject::tr("(select a key to preview)"));
    return;
  }

  int row = selectedItems.first()->row();
  auto* translationItem = table->item(row, 2);
  if (!translationItem) {
    previewOutput->setText("");
    return;
  }

  QString text = translationItem->text();

  // Apply variable interpolation
  std::unordered_map<std::string, std::string> variables;
  for (auto it = previewVariables.begin(); it != previewVariables.end(); ++it) {
    variables[it.key().toStdString()] = it.value().toStdString();
  }

  std::string stdText = text.toStdString();
  std::string interpolated = m_localization.interpolate(stdText, variables);

  previewOutput->setText(QString::fromStdString(interpolated));
}

void NMLocalizationUI::applyRTLLayout(QTableWidget* table, QLabel* previewOutput, bool rtl) {
  Qt::LayoutDirection direction = rtl ? Qt::RightToLeft : Qt::LeftToRight;

  // Apply to table
  if (table) {
    table->setLayoutDirection(direction);

    // Update alignment for translation column
    for (int row = 0; row < table->rowCount(); ++row) {
      auto* translationItem = table->item(row, 2);
      if (translationItem) {
        if (rtl) {
          translationItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        } else {
          translationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
      }
    }
  }

  // Apply to preview
  if (previewOutput) {
    previewOutput->setLayoutDirection(direction);
  }
}

void NMLocalizationUI::updateTableCell(QTableWidget* table, const QString& key, const QString& value,
                                       const QHash<QString, int>& keyToRowMap) {
  if (!table) {
    return;
  }

  // Use O(1) hash lookup instead of O(n) linear search
  auto rowIt = keyToRowMap.find(key);
  if (rowIt != keyToRowMap.end()) {
    int row = rowIt.value();
    // Update the translation cell (column 2)
    auto* valueItem = table->item(row, 2);
    if (valueItem) {
      // Block signals to prevent creating a new undo command
      bool wasBlocked = table->blockSignals(true);
      valueItem->setText(value);
      table->blockSignals(wasBlocked);
    }
  }
}

} // namespace NovelMind::editor::qt
