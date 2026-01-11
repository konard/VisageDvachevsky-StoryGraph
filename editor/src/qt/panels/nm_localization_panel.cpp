#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// Key validation: allows alphanumeric, underscore, dot, dash
const QRegularExpression NMLocalizationPanel::s_keyValidationRegex(
    QStringLiteral("^[A-Za-z0-9_.-]+$"));

NMLocalizationPanel::NMLocalizationPanel(QWidget* parent)
    : NMDockPanel("Localization Manager", parent)
    , m_dataModel(m_localization)
    , m_ioHelper(m_localization, m_dataModel)
    , m_searchHelper(m_dataModel)
    , m_uiHelper(m_dataModel, m_localization) {}

NMLocalizationPanel::~NMLocalizationPanel() = default;

void NMLocalizationPanel::onInitialize() {
  setupUI();
}

void NMLocalizationPanel::onShutdown() {}

void NMLocalizationPanel::setupUI() {
  // Delegate UI setup to helper class
  NMLocalizationUISetup::setupUI(this);
  refreshLocales();
}

void NMLocalizationPanel::refreshLocales() {
  if (!m_languageSelector) {
    return;
  }

  m_languageSelector->blockSignals(true);
  m_languageSelector->clear();

  auto& pm = ProjectManager::instance();
  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));

  QStringList localeCodes;
  QDir dir(localizationRoot);
  if (dir.exists()) {
    const QStringList filters = {"*.csv", "*.json", "*.po", "*.xliff", "*.xlf"};
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    for (const auto& info : files) {
      const QString locale = info.baseName();
      if (!locale.isEmpty()) {
        localeCodes.append(locale);
      }
    }
  }

  if (!localeCodes.contains(m_defaultLocale)) {
    localeCodes.prepend(m_defaultLocale);
  }

  localeCodes.removeDuplicates();
  localeCodes.sort();
  m_availableLocales = localeCodes;

  for (const auto& code : localeCodes) {
    m_languageSelector->addItem(code.toUpper(), code);
  }

  m_languageSelector->blockSignals(false);

  if (!localeCodes.isEmpty()) {
    // Load first locale
    m_currentLocale = localeCodes.first();
    auto& pm = ProjectManager::instance();
    const QString localizationRoot =
        QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
    m_ioHelper.loadLocale(m_currentLocale, localizationRoot);
    m_dataModel.syncFromManager(m_defaultLocale, m_currentLocale);
    rebuildTable();
    m_uiHelper.highlightMissingTranslations(m_stringsTable);
    updateStatusBar();
  }
}

void NMLocalizationPanel::rebuildTable() {
  m_uiHelper.rebuildTable(m_stringsTable, m_currentLocale, m_defaultLocale, m_keyToRowMap);
  applyFilters();
  m_uiHelper.highlightMissingTranslations(m_stringsTable);
}

void NMLocalizationPanel::applyFilters() {
  if (!m_stringsTable || !m_searchEdit || !m_showMissingOnly) {
    return;
  }

  const QString searchText = m_searchEdit->text();
  const bool showMissingOnly = m_showMissingOnly->isChecked();

  m_uiHelper.applyFilters(m_stringsTable, searchText, showMissingOnly);
  updateStatusBar();
}

void NMLocalizationPanel::updateStatusBar() {
  m_uiHelper.updateStatusBar(m_statusLabel, m_stringsTable, m_dirty);
}

void NMLocalizationPanel::onSearchTextChanged(const QString& text) {
  Q_UNUSED(text);
  applyFilters();
}

void NMLocalizationPanel::onShowOnlyMissingToggled(bool checked) {
  Q_UNUSED(checked);
  applyFilters();
}

void NMLocalizationPanel::onLocaleChanged(int index) {
  Q_UNUSED(index);
  if (!m_languageSelector) {
    return;
  }

  const QString localeCode = m_languageSelector->currentData().toString();
  if (!localeCode.isEmpty() && localeCode != m_currentLocale) {
    m_currentLocale = localeCode;
    auto& pm = ProjectManager::instance();
    const QString localizationRoot =
        QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
    m_ioHelper.loadLocale(m_currentLocale, localizationRoot);
    m_dataModel.syncFromManager(m_defaultLocale, m_currentLocale);
    rebuildTable();
    m_uiHelper.highlightMissingTranslations(m_stringsTable);
    updateStatusBar();
  }
}

void NMLocalizationPanel::onCellChanged(int row, int column) {
  if (!m_stringsTable || column != 2) {
    return;
  }

  auto* idItem = m_stringsTable->item(row, 0);
  auto* valueItem = m_stringsTable->item(row, 2);
  if (!idItem || !valueItem || m_currentLocale.isEmpty()) {
    return;
  }

  const QString key = idItem->text();
  const QString value = valueItem->text();

  // Capture old value for undo
  QString oldValue;
  const auto& entries = m_dataModel.entries();
  if (entries.contains(key)) {
    oldValue = entries[key].translations.value(m_currentLocale, "");

    // Only create undo command if value changed
    if (oldValue != value) {
      auto* cmd = new ChangeTranslationCommand(this, key, m_currentLocale, oldValue, value);
      NMUndoManager::instance().pushCommand(cmd);
    }

    // Update in-memory entry
    m_dataModel.setTranslationValue(key, m_currentLocale, value);
  }

  // Update manager
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.setString(locale, key.toStdString(), value.toStdString());

  setDirty(true);
  m_uiHelper.highlightMissingTranslations(m_stringsTable);
  updateStatusBar();

  emit translationChanged(key, m_currentLocale, value);
}

void NMLocalizationPanel::onAddKeyClicked() {
  QString key;
  QString defaultValue;

  if (!showAddKeyDialog(key, defaultValue)) {
    return;
  }

  if (m_dataModel.addKey(key, defaultValue, m_defaultLocale, m_currentLocale)) {
    // Create undo command for adding key
    auto* cmd = new AddLocalizationKeyCommand(this, key, defaultValue);
    NMUndoManager::instance().pushCommand(cmd);

    setDirty(true);
    rebuildTable();

    // PERF-4: Use O(1) lookup to select the new row instead of O(n) search
    auto rowIt = m_keyToRowMap.find(key);
    if (rowIt != m_keyToRowMap.end()) {
      int row = rowIt.value();
      auto* item = m_stringsTable->item(row, 0);
      if (item) {
        m_stringsTable->selectRow(row);
        m_stringsTable->scrollToItem(item);
      }
    }
  }
}

bool NMLocalizationPanel::showAddKeyDialog(QString& outKey, QString& outDefaultValue) {
  return NMLocalizationDialogs::showAddKeyDialog(this, m_searchHelper, outKey, outDefaultValue);
}

void NMLocalizationPanel::onDeleteKeyClicked() {
  if (!m_stringsTable) {
    return;
  }

  QList<QTableWidgetItem*> selectedItems = m_stringsTable->selectedItems();
  if (selectedItems.isEmpty()) {
    NMMessageDialog::showInfo(this, tr("Delete Key"), tr("Please select a key to delete."));
    return;
  }

  // Get unique keys from selection
  QSet<QString> keysToDelete;
  for (auto* item : selectedItems) {
    int row = item->row();
    auto* idItem = m_stringsTable->item(row, 0);
    if (idItem) {
      keysToDelete.insert(idItem->text());
    }
  }

  QString message;
  if (keysToDelete.size() == 1) {
    message = tr("Are you sure you want to delete the key '%1'?").arg(*keysToDelete.begin());
  } else {
    message = tr("Are you sure you want to delete %1 keys?").arg(keysToDelete.size());
  }

  NMDialogButton result =
      NMMessageDialog::showQuestion(this, tr("Confirm Delete"), message,
                                    {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  // Create macro for deleting multiple keys
  if (keysToDelete.size() > 1) {
    NMUndoManager::instance().beginMacro("Delete Localization Keys");
  }

  const auto& entries = m_dataModel.entries();
  for (const QString& key : keysToDelete) {
    // Capture translations before deleting
    QHash<QString, QString> translations;
    if (entries.contains(key)) {
      translations = entries[key].translations;
    }

    // Create and push delete command
    auto* cmd = new DeleteLocalizationKeyCommand(this, key, translations);
    NMUndoManager::instance().pushCommand(cmd);

    m_dataModel.deleteKey(key, m_defaultLocale, m_currentLocale);
  }

  if (keysToDelete.size() > 1) {
    NMUndoManager::instance().endMacro();
  }

  setDirty(true);
  rebuildTable();
}

void NMLocalizationPanel::onContextMenu(const QPoint& pos) {
  QMenu menu(this);

  QAction* addAction = menu.addAction(tr("Add Key..."));
  connect(addAction, &QAction::triggered, this, &NMLocalizationPanel::onAddKeyClicked);

  auto* idItem = m_stringsTable->itemAt(pos);
  if (idItem) {
    menu.addSeparator();

    QAction* deleteAction = menu.addAction(tr("Delete Key"));
    connect(deleteAction, &QAction::triggered, this, &NMLocalizationPanel::onDeleteKeyClicked);

    QAction* copyKeyAction = menu.addAction(tr("Copy Key"));
    connect(copyKeyAction, &QAction::triggered, [this, idItem]() {
      int row = idItem->row();
      auto* keyItem = m_stringsTable->item(row, 0);
      if (keyItem) {
        QApplication::clipboard()->setText(keyItem->text());
      }
    });
  }

  menu.exec(m_stringsTable->viewport()->mapToGlobal(pos));
}

void NMLocalizationPanel::setDirty(bool dirty) {
  if (m_dirty != dirty) {
    m_dirty = dirty;
    if (m_saveBtn) {
      m_saveBtn->setEnabled(dirty);
    }
    updateStatusBar();
    emit dirtyStateChanged(dirty);
  }
}

void NMLocalizationPanel::onSaveClicked() {
  saveChanges();
}

bool NMLocalizationPanel::saveChanges() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showError(this, tr("Save Failed"), tr("No project is open."));
    return false;
  }

  m_dataModel.syncToManager(m_defaultLocale, m_currentLocale);

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));

  if (!m_ioHelper.saveChanges(m_availableLocales, localizationRoot, this)) {
    return false;
  }

  m_dataModel.clearDeletedKeys();
  setDirty(false);

  // Mark entries as not modified
  auto& entries = m_dataModel.entries();
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    it->isModified = false;
    it->isNew = false;
  }

  return true;
}

void NMLocalizationPanel::onExportClicked() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject() || m_currentLocale.isEmpty()) {
    return;
  }

  m_dataModel.syncToManager(m_defaultLocale, m_currentLocale);

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  m_ioHelper.exportLocale(m_currentLocale, localizationRoot, this);
}

void NMLocalizationPanel::onImportClicked() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showInfo(this, tr("Import Localization"),
                              tr("Open a project before importing strings."));
    return;
  }

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));

  // Capture old values for undo support (for the target locale)
  const auto& entries = m_dataModel.entries();
  QHash<QString, QString> oldValues;
  for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
    const LocalizationEntry& entry = it.value();
    if (!entry.isDeleted && entry.translations.contains(m_currentLocale)) {
      oldValues[entry.key] = entry.translations.value(m_currentLocale);
    }
  }

  QString importedLocaleCode;
  if (!m_ioHelper.importLocale(localizationRoot, this, importedLocaleCode)) {
    return;
  }

  // Switch to the imported locale and sync entries
  m_currentLocale = importedLocaleCode;
  m_dataModel.syncFromManager(m_defaultLocale, m_currentLocale);

  // Create undo macro for bulk import
  NMUndoManager::instance().beginMacro(QString("Import Locale: %1").arg(importedLocaleCode));

  // Create commands for each changed value
  const auto& updatedEntries = m_dataModel.entries();
  for (auto it = updatedEntries.constBegin(); it != updatedEntries.constEnd(); ++it) {
    const LocalizationEntry& entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const QString& key = entry.key;
    const QString newValue = entry.translations.value(importedLocaleCode, "");
    const QString oldValue = oldValues.value(key, "");

    // Only create command if value changed
    if (newValue != oldValue) {
      auto* cmd = new ChangeTranslationCommand(this, key, importedLocaleCode, oldValue, newValue);
      NMUndoManager::instance().pushCommand(cmd);
    }
  }

  NMUndoManager::instance().endMacro();

  refreshLocales();
  setDirty(true);
}

void NMLocalizationPanel::onRefreshClicked() {
  if (m_dirty) {
    NMDialogButton result = NMMessageDialog::showQuestion(
        this, tr("Unsaved Changes"),
        tr("You have unsaved changes. Do you want to discard them and "
           "refresh?"),
        {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

    if (result != NMDialogButton::Yes) {
      return;
    }
  }

  setDirty(false);
  refreshLocales();
}

void NMLocalizationPanel::setupToolBar() {
  // Toolbar setup is handled in setupUI
}

void NMLocalizationPanel::setupFilterBar() {
  // Filter bar setup is handled in setupUI
}

void NMLocalizationPanel::setupTable() {
  // Table setup is handled in setupUI
}

void NMLocalizationPanel::onUpdate([[maybe_unused]] double deltaTime) {}

void NMLocalizationPanel::onItemDoubleClicked(QTableWidgetItem* item) {
  if (!item) {
    return;
  }

  int row = item->row();
  auto* idItem = m_stringsTable->item(row, 0);
  if (idItem) {
    emit keySelected(idItem->text());
  }
}

void NMLocalizationPanel::onFilterChanged(int index) {
  Q_UNUSED(index);
  applyFilters();
}

void NMLocalizationPanel::onExportMissingClicked() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject() || m_currentLocale.isEmpty()) {
    return;
  }

  m_dataModel.syncToManager(m_defaultLocale, m_currentLocale);

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  m_ioHelper.exportMissingStrings(m_currentLocale, localizationRoot, this);
}

void NMLocalizationPanel::onEditPluralFormsClicked() {
  if (!m_stringsTable) {
    return;
  }

  QList<QTableWidgetItem*> selectedItems = m_stringsTable->selectedItems();
  if (selectedItems.isEmpty()) {
    NMMessageDialog::showInfo(this, tr("Edit Plural Forms"),
                              tr("Please select a key to edit plural forms."));
    return;
  }

  int row = selectedItems.first()->row();
  auto* idItem = m_stringsTable->item(row, 0);
  if (!idItem) {
    return;
  }

  const QString key = idItem->text();
  showPluralFormsDialog(key);
}

bool NMLocalizationPanel::showPluralFormsDialog(const QString& key) {
  bool result = NMLocalizationDialogs::showPluralFormsDialog(this, m_dataModel, key);
  if (result) {
    setDirty(true);
    m_dataModel.syncFromManager(m_defaultLocale, m_currentLocale);
    rebuildTable();
  }
  return result;
}

void NMLocalizationPanel::onToggleRTLPreview(bool checked) {
  m_uiHelper.applyRTLLayout(m_stringsTable, m_previewOutput, checked);
}

void NMLocalizationPanel::onPreviewVariablesChanged() {
  if (!m_previewInput) {
    return;
  }

  // Parse variables from input (format: var1=value1,var2=value2)
  m_previewVariables.clear();
  const QString input = m_previewInput->text();
  const QStringList pairs = input.split(',', Qt::SkipEmptyParts);

  for (const QString& pair : pairs) {
    const QStringList parts = pair.split('=');
    if (parts.size() == 2) {
      m_previewVariables[parts[0].trimmed()] = parts[1].trimmed();
    }
  }

  updatePreview();
}

void NMLocalizationPanel::updatePreview() {
  m_uiHelper.updatePreview(m_previewOutput, m_stringsTable, m_previewVariables, m_currentLocale);
}

int NMLocalizationPanel::importDialogueEntries(const QList<QPair<QString, QString>>& entries) {
  int imported = m_dataModel.importDialogueEntries(entries, m_defaultLocale, m_currentLocale);

  if (imported > 0) {
    setDirty(true);
    rebuildTable();
  }

  return imported;
}

bool NMLocalizationPanel::hasTranslation(const QString& key) const {
  return m_dataModel.hasTranslation(key, m_currentLocale, m_defaultLocale);
}

QString NMLocalizationPanel::getTranslation(const QString& key) const {
  return m_dataModel.getTranslation(key, m_currentLocale, m_defaultLocale);
}

void NMLocalizationPanel::setTranslationValue(const QString& key, const QString& locale,
                                              const QString& value) {
  // Update the in-memory entry
  m_dataModel.setTranslationValue(key, locale, value);
  setDirty(true);

  // Update the table if the key is currently displayed
  if (m_stringsTable && locale == m_currentLocale) {
    m_uiHelper.updateTableCell(m_stringsTable, key, value, m_keyToRowMap);
  }
}

void NMLocalizationPanel::navigateToUsage(const QString& key, int usageIndex) {
  const auto& entries = m_dataModel.entries();
  if (!entries.contains(key)) {
    return;
  }

  const LocalizationEntry& entry = entries.value(key);
  if (usageIndex >= 0 && usageIndex < entry.usageLocations.size()) {
    emit navigateToFile(entry.usageLocations.at(usageIndex), 0);
  }
}

QStringList NMLocalizationPanel::findMissingTranslations(const QString& locale) const {
  return m_searchHelper.findMissingTranslations(locale);
}

QStringList NMLocalizationPanel::findUnusedKeys() const {
  return m_searchHelper.findUnusedKeys();
}

void NMLocalizationPanel::scanProjectForUsages() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  const QString projectPath = QString::fromStdString(pm.getProjectPath());
  m_searchHelper.scanProjectForUsages(projectPath);

  // Update the table to reflect new usage data
  rebuildTable();
  updateStatusBar();
}

bool NMLocalizationPanel::addKey(const QString& key, const QString& defaultValue) {
  bool result = m_dataModel.addKey(key, defaultValue, m_defaultLocale, m_currentLocale);
  if (result) {
    setDirty(true);
  }
  return result;
}

bool NMLocalizationPanel::deleteKey(const QString& key) {
  bool result = m_dataModel.deleteKey(key, m_defaultLocale, m_currentLocale);
  if (result) {
    setDirty(true);
  }
  return result;
}

} // namespace NovelMind::editor::qt
