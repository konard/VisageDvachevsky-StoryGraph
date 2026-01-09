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

NMLocalizationPanel::NMLocalizationPanel(QWidget *parent)
    : NMDockPanel("Localization Manager", parent) {}

NMLocalizationPanel::~NMLocalizationPanel() = default;

void NMLocalizationPanel::onInitialize() { setupUI(); }

void NMLocalizationPanel::onShutdown() {}

void NMLocalizationPanel::setupUI() {
  auto &style = NMStyleManager::instance();
  const auto &palette = style.palette();
  const auto &spacing = style.spacing();
  auto &iconMgr = NMIconManager::instance();

  QVBoxLayout *layout = new QVBoxLayout(contentWidget());
  layout->setContentsMargins(spacing.xs, spacing.xs, spacing.xs, spacing.xs);
  layout->setSpacing(spacing.sm);

  // Top toolbar: Language selector and file operations
  QFrame *topToolbar = new QFrame(contentWidget());
  topToolbar->setFrameStyle(QFrame::NoFrame);
  QHBoxLayout *topLayout = new QHBoxLayout(topToolbar);
  topLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm, spacing.sm);
  topLayout->setSpacing(spacing.md);

  // Language selection group
  QLabel *langLabel = new QLabel(tr("Language:"), topToolbar);
  langLabel->setStyleSheet(
      QString("color: %1; font-weight: 600;")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  topLayout->addWidget(langLabel);

  m_languageSelector = new QComboBox(topToolbar);
  m_languageSelector->setMinimumWidth(120);
  connect(m_languageSelector,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMLocalizationPanel::onLocaleChanged);
  topLayout->addWidget(m_languageSelector);

  topLayout->addStretch();

  // File operations group (Import/Export/Save)
  m_importButton = new QPushButton(tr("Import..."), topToolbar);
  m_importButton->setIcon(iconMgr.getIcon("import", 16));
  m_importButton->setToolTip(tr("Import localization file"));
  connect(m_importButton, &QPushButton::clicked, this,
          &NMLocalizationPanel::importLocale);
  topLayout->addWidget(m_importButton);

  m_exportButton = new QPushButton(tr("Export..."), topToolbar);
  m_exportButton->setIcon(iconMgr.getIcon("export", 16));
  m_exportButton->setToolTip(tr("Export current locale to file"));
  connect(m_exportButton, &QPushButton::clicked, this,
          &NMLocalizationPanel::exportLocale);
  topLayout->addWidget(m_exportButton);

  m_exportMissingBtn = new QPushButton(tr("Export Missing..."), topToolbar);
  m_exportMissingBtn->setIcon(iconMgr.getIcon("export", 16));
  m_exportMissingBtn->setToolTip(
      tr("Export missing translations for current locale"));
  connect(m_exportMissingBtn, &QPushButton::clicked, this,
          &NMLocalizationPanel::onExportMissingClicked);
  topLayout->addWidget(m_exportMissingBtn);

  m_saveBtn = new QPushButton(tr("Save"), topToolbar);
  m_saveBtn->setIcon(iconMgr.getIcon("file-save", 16));
  m_saveBtn->setEnabled(false);
  m_saveBtn->setToolTip(tr("Save all changes"));
  m_saveBtn->setStyleSheet(
      QString(
          "QPushButton { background-color: %1; color: %2; font-weight: 600; }"
          "QPushButton:hover { background-color: %3; }"
          "QPushButton:disabled { background-color: %4; color: %5; }")
          .arg(NMStyleManager::colorToStyleString(
              style.panelAccents().localization))
          .arg(NMStyleManager::colorToStyleString(palette.textInverse))
          .arg(NMStyleManager::colorToRgbaString(
              style.panelAccents().localization, 220))
          .arg(NMStyleManager::colorToStyleString(palette.bgLight))
          .arg(NMStyleManager::colorToStyleString(palette.textDisabled)));
  connect(m_saveBtn, &QPushButton::clicked, this,
          &NMLocalizationPanel::onSaveClicked);
  topLayout->addWidget(m_saveBtn);

  layout->addWidget(topToolbar);

  // Filter and action bar
  QFrame *filterToolbar = new QFrame(contentWidget());
  filterToolbar->setFrameStyle(QFrame::NoFrame);
  QHBoxLayout *filterLayout = new QHBoxLayout(filterToolbar);
  filterLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm,
                                   spacing.sm);
  filterLayout->setSpacing(spacing.md);

  // Search box
  m_searchEdit = new QLineEdit(filterToolbar);
  m_searchEdit->setPlaceholderText(
      tr("Search keys, source, or translations..."));
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &NMLocalizationPanel::onSearchTextChanged);
  filterLayout->addWidget(m_searchEdit, 1);

  // Filter checkbox
  m_showMissingOnly = new QCheckBox(tr("Show Missing Only"), filterToolbar);
  m_showMissingOnly->setStyleSheet(
      QString("QCheckBox { color: %1; }")
          .arg(NMStyleManager::colorToStyleString(palette.warning)));
  connect(m_showMissingOnly, &QCheckBox::toggled, this,
          &NMLocalizationPanel::onShowOnlyMissingToggled);
  filterLayout->addWidget(m_showMissingOnly);

  // Add separator
  QFrame *separator = new QFrame(filterToolbar);
  separator->setFrameShape(QFrame::VLine);
  separator->setFrameShadow(QFrame::Sunken);
  separator->setStyleSheet(
      QString("color: %1;")
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  filterLayout->addWidget(separator);

  // Edit operations group (Add/Delete)
  m_addKeyBtn = new QPushButton(tr("+ Add Key"), filterToolbar);
  m_addKeyBtn->setIcon(iconMgr.getIcon("add", 16));
  m_addKeyBtn->setToolTip(tr("Add new localization key"));
  m_addKeyBtn->setStyleSheet(
      QString(
          "QPushButton { background-color: %1; color: %2; font-weight: 600; }"
          "QPushButton:hover { background-color: %3; }")
          .arg(NMStyleManager::colorToStyleString(palette.successSubtle))
          .arg(NMStyleManager::colorToStyleString(palette.success))
          .arg(NMStyleManager::colorToStyleString(palette.success)));
  connect(m_addKeyBtn, &QPushButton::clicked, this,
          &NMLocalizationPanel::onAddKeyClicked);
  filterLayout->addWidget(m_addKeyBtn);

  m_deleteKeyBtn = new QPushButton(tr("Delete Key"), filterToolbar);
  m_deleteKeyBtn->setIcon(iconMgr.getIcon("delete", 16));
  m_deleteKeyBtn->setToolTip(tr("Delete selected key(s)"));
  m_deleteKeyBtn->setStyleSheet(
      QString("QPushButton { background-color: %1; color: %2; }"
              "QPushButton:hover { background-color: %3; color: %4; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgMedium))
          .arg(NMStyleManager::colorToStyleString(palette.error))
          .arg(NMStyleManager::colorToStyleString(palette.errorSubtle))
          .arg(NMStyleManager::colorToStyleString(palette.error)));
  connect(m_deleteKeyBtn, &QPushButton::clicked, this,
          &NMLocalizationPanel::onDeleteKeyClicked);
  filterLayout->addWidget(m_deleteKeyBtn);

  // Add separator
  QFrame *separator2 = new QFrame(filterToolbar);
  separator2->setFrameShape(QFrame::VLine);
  separator2->setFrameShadow(QFrame::Sunken);
  separator2->setStyleSheet(
      QString("color: %1;")
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  filterLayout->addWidget(separator2);

  // Plural Forms button
  m_pluralFormsBtn = new QPushButton(tr("Plural Forms..."), filterToolbar);
  m_pluralFormsBtn->setIcon(iconMgr.getIcon("settings", 16));
  m_pluralFormsBtn->setToolTip(tr("Edit plural forms for selected key"));
  connect(m_pluralFormsBtn, &QPushButton::clicked, this,
          &NMLocalizationPanel::onEditPluralFormsClicked);
  filterLayout->addWidget(m_pluralFormsBtn);

  // RTL Preview checkbox
  m_rtlPreviewCheckbox = new QCheckBox(tr("RTL Preview"), filterToolbar);
  m_rtlPreviewCheckbox->setToolTip(tr("Toggle right-to-left text preview"));
  connect(m_rtlPreviewCheckbox, &QCheckBox::toggled, this,
          &NMLocalizationPanel::onToggleRTLPreview);
  filterLayout->addWidget(m_rtlPreviewCheckbox);

  layout->addWidget(filterToolbar);

  // Table with improved styling
  m_stringsTable = new QTableWidget(contentWidget());
  m_stringsTable->setObjectName("LocalizationTable");
  m_stringsTable->setColumnCount(3);
  m_stringsTable->setHorizontalHeaderLabels(
      {tr("Key"), tr("Source Text"), tr("Translation")});
  m_stringsTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                  QAbstractItemView::EditKeyPressed);
  m_stringsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_stringsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_stringsTable->setContextMenuPolicy(Qt::CustomContextMenu);
  m_stringsTable->setAlternatingRowColors(true);
  m_stringsTable->setShowGrid(true);
  m_stringsTable->verticalHeader()->setVisible(false);
  m_stringsTable->verticalHeader()->setDefaultSectionSize(28);
  m_stringsTable->horizontalHeader()->setStretchLastSection(true);
  m_stringsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

  // Set column widths
  m_stringsTable->setColumnWidth(0, 200); // Key column
  m_stringsTable->setColumnWidth(1, 300); // Source column

  // Apply custom table styling
  m_stringsTable->setStyleSheet(
      QString("QTableWidget {"
              "  background-color: %1;"
              "  gridline-color: %2;"
              "  alternate-background-color: %3;"
              "  border: 1px solid %4;"
              "}"
              "QTableWidget::item {"
              "  padding: 6px 8px;"
              "  color: %5;"
              "  border: none;"
              "}"
              "QTableWidget::item:selected {"
              "  background-color: %6;"
              "  color: %7;"
              "}"
              "QTableWidget::item:hover {"
              "  background-color: %8;"
              "}"
              "QHeaderView::section {"
              "  background-color: %9;"
              "  color: %10;"
              "  padding: 6px 8px;"
              "  border: none;"
              "  border-right: 1px solid %2;"
              "  border-bottom: 2px solid %11;"
              "  font-weight: 600;"
              "}")
          .arg(NMStyleManager::colorToStyleString(palette.bgDark))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault))
          .arg(NMStyleManager::colorToStyleString(palette.bgMedium))
          .arg(NMStyleManager::colorToStyleString(palette.borderLight))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToRgbaString(palette.accentPrimary, 80))
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToRgbaString(palette.bgLight, 60))
          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(
              style.panelAccents().localization)));

  connect(m_stringsTable, &QTableWidget::cellChanged, this,
          &NMLocalizationPanel::onCellChanged);
  connect(m_stringsTable, &QTableWidget::customContextMenuRequested, this,
          &NMLocalizationPanel::onContextMenu);
  connect(m_stringsTable, &QTableWidget::itemSelectionChanged, this,
          &NMLocalizationPanel::updatePreview);
  layout->addWidget(m_stringsTable, 1);

  // Preview Panel with variable interpolation
  m_previewPanel = new QWidget(contentWidget());
  QHBoxLayout *previewLayout = new QHBoxLayout(m_previewPanel);
  previewLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm,
                                    spacing.sm);
  previewLayout->setSpacing(spacing.md);

  QLabel *previewLabel =
      new QLabel(tr("Preview with variables:"), m_previewPanel);
  previewLabel->setStyleSheet(
      QString("color: %1; font-weight: 600;")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  previewLayout->addWidget(previewLabel);

  m_previewInput = new QLineEdit(m_previewPanel);
  m_previewInput->setPlaceholderText(tr("e.g., name=John,count=5"));
  m_previewInput->setToolTip(
      tr("Enter variable values in format: var1=value1,var2=value2"));
  connect(m_previewInput, &QLineEdit::textChanged, this,
          &NMLocalizationPanel::onPreviewVariablesChanged);
  previewLayout->addWidget(m_previewInput, 1);

  m_previewOutput = new QLabel(m_previewPanel);
  m_previewOutput->setStyleSheet(
      QString("QLabel {"
              "  color: %1;"
              "  padding: 4px 8px;"
              "  background-color: %2;"
              "  border: 1px solid %3;"
              "  border-radius: 3px;"
              "}")
          .arg(NMStyleManager::colorToStyleString(palette.textPrimary))
          .arg(NMStyleManager::colorToStyleString(palette.bgMedium))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  m_previewOutput->setMinimumWidth(200);
  previewLayout->addWidget(m_previewOutput, 1);

  layout->addWidget(m_previewPanel);

  // Status bar with improved styling
  m_statusLabel = new QLabel(contentWidget());
  m_statusLabel->setStyleSheet(
      QString("QLabel {"
              "  color: %1;"
              "  padding: 4px 8px;"
              "  background-color: %2;"
              "  border-top: 1px solid %3;"
              "}")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  layout->addWidget(m_statusLabel);

  refreshLocales();
}

void NMLocalizationPanel::refreshLocales() {
  if (!m_languageSelector) {
    return;
  }

  m_languageSelector->blockSignals(true);
  m_languageSelector->clear();

  auto &pm = ProjectManager::instance();
  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));

  QStringList localeCodes;
  QDir dir(localizationRoot);
  if (dir.exists()) {
    const QStringList filters = {"*.csv", "*.json", "*.po", "*.xliff", "*.xlf"};
    const QFileInfoList files =
        dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &info : files) {
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

  for (const auto &code : localeCodes) {
    m_languageSelector->addItem(code.toUpper(), code);
  }

  m_languageSelector->blockSignals(false);

  if (!localeCodes.isEmpty()) {
    loadLocale(localeCodes.first());
  }
}

static NovelMind::localization::LocalizationFormat
formatForExtension(const QString &ext) {
  const QString lower = ext.toLower();
  if (lower == "csv")
    return NovelMind::localization::LocalizationFormat::CSV;
  if (lower == "json")
    return NovelMind::localization::LocalizationFormat::JSON;
  if (lower == "po")
    return NovelMind::localization::LocalizationFormat::PO;
  if (lower == "xliff" || lower == "xlf")
    return NovelMind::localization::LocalizationFormat::XLIFF;
  return NovelMind::localization::LocalizationFormat::CSV;
}

void NMLocalizationPanel::loadLocale(const QString &localeCode) {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  m_currentLocale = localeCode;

  NovelMind::localization::LocaleId locale;
  locale.language = localeCode.toStdString();

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  QDir dir(localizationRoot);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QStringList candidates = {
      dir.filePath(localeCode + ".csv"), dir.filePath(localeCode + ".json"),
      dir.filePath(localeCode + ".po"), dir.filePath(localeCode + ".xliff"),
      dir.filePath(localeCode + ".xlf")};
  for (const auto &path : candidates) {
    QFileInfo info(path);
    if (!info.exists()) {
      continue;
    }
    const auto format = formatForExtension(info.suffix());
    m_localization.loadStrings(locale, info.absoluteFilePath().toStdString(),
                               format);
    break;
  }

  syncEntriesFromManager();
  rebuildTable();
  highlightMissingTranslations();
  updateStatusBar();
}

void NMLocalizationPanel::syncEntriesFromManager() {
  m_entries.clear();
  m_deletedKeys.clear();

  NovelMind::localization::LocaleId defaultLocale;
  defaultLocale.language = m_defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLocale;
  currentLocale.language = m_currentLocale.toStdString();

  const auto *defaultTable = m_localization.getStringTable(defaultLocale);
  const auto *currentTable = m_localization.getStringTable(currentLocale);

  std::vector<std::string> ids;
  if (defaultTable) {
    ids = defaultTable->getStringIds();
  }
  if (currentTable) {
    auto currentIds = currentTable->getStringIds();
    ids.insert(ids.end(), currentIds.begin(), currentIds.end());
  }

  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  for (const auto &id : ids) {
    LocalizationEntry entry;
    entry.key = QString::fromStdString(id);

    if (defaultTable) {
      auto val = defaultTable->getString(id);
      if (val) {
        entry.translations[m_defaultLocale] = QString::fromStdString(*val);
      }
    }
    if (currentTable && m_currentLocale != m_defaultLocale) {
      auto val = currentTable->getString(id);
      if (val) {
        entry.translations[m_currentLocale] = QString::fromStdString(*val);
      }
    }

    // Check if missing translation for current locale
    entry.isMissing = !entry.translations.contains(m_currentLocale) ||
                      entry.translations.value(m_currentLocale).isEmpty();

    m_entries[entry.key] = entry;
  }
}

void NMLocalizationPanel::syncEntriesToManager() {
  NovelMind::localization::LocaleId defaultLocale;
  defaultLocale.language = m_defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLocale;
  currentLocale.language = m_currentLocale.toStdString();

  // Remove deleted keys from manager
  for (const QString &key : m_deletedKeys) {
    m_localization.removeString(defaultLocale, key.toStdString());
    m_localization.removeString(currentLocale, key.toStdString());
  }

  // Update/add entries
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const std::string id = entry.key.toStdString();

    if (entry.translations.contains(m_defaultLocale)) {
      m_localization.setString(
          defaultLocale, id,
          entry.translations.value(m_defaultLocale).toStdString());
    }
    if (entry.translations.contains(m_currentLocale)) {
      m_localization.setString(
          currentLocale, id,
          entry.translations.value(m_currentLocale).toStdString());
    }
  }
}

/**
 * @brief Rebuild the entire localization table
 *
 * PERF-4 optimization: Populates m_keyToRowMap for O(1) row lookups
 * when updating individual translations via setTranslationValue().
 */
void NMLocalizationPanel::rebuildTable() {
  if (!m_stringsTable) {
    return;
  }

  m_stringsTable->blockSignals(true);
  m_stringsTable->setRowCount(0);

  // PERF-4: Clear and rebuild key-to-row mapping
  m_keyToRowMap.clear();

  QList<QString> sortedKeys = m_entries.keys();
  std::sort(sortedKeys.begin(), sortedKeys.end());

  // PERF-4: Pre-reserve capacity for the row map
  m_keyToRowMap.reserve(sortedKeys.size());

  int row = 0;
  for (const QString &key : sortedKeys) {
    const LocalizationEntry &entry = m_entries.value(key);

    if (entry.isDeleted) {
      continue;
    }

    m_stringsTable->insertRow(row);

    auto *idItem = new QTableWidgetItem(entry.key);
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    m_stringsTable->setItem(row, 0, idItem);

    QString sourceText = entry.translations.value(m_defaultLocale);
    auto *sourceItem = new QTableWidgetItem(sourceText);
    sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
    m_stringsTable->setItem(row, 1, sourceItem);

    QString translationText = entry.translations.value(m_currentLocale);
    auto *translationItem = new QTableWidgetItem(translationText);
    m_stringsTable->setItem(row, 2, translationItem);

    // PERF-4: Store key-to-row mapping for O(1) lookup
    m_keyToRowMap.insert(key, row);

    row++;
  }

  m_stringsTable->blockSignals(false);
  applyFilters();
  highlightMissingTranslations();
}

void NMLocalizationPanel::applyFilters() {
  if (!m_stringsTable) {
    return;
  }

  const QString searchText =
      m_searchEdit ? m_searchEdit->text().toLower() : QString();
  const bool showMissingOnly =
      m_showMissingOnly && m_showMissingOnly->isChecked();

  int visibleCount = 0;
  int missingCount = 0;

  for (int row = 0; row < m_stringsTable->rowCount(); ++row) {
    auto *idItem = m_stringsTable->item(row, 0);
    auto *sourceItem = m_stringsTable->item(row, 1);
    auto *translationItem = m_stringsTable->item(row, 2);

    if (!idItem) {
      m_stringsTable->setRowHidden(row, true);
      continue;
    }

    const QString key = idItem->text();
    const QString source = sourceItem ? sourceItem->text() : QString();
    const QString translation =
        translationItem ? translationItem->text() : QString();

    bool isMissing = translation.isEmpty();
    if (isMissing) {
      missingCount++;
    }

    // Apply search filter (case-insensitive)
    bool matchesSearch = searchText.isEmpty() ||
                         key.toLower().contains(searchText) ||
                         source.toLower().contains(searchText) ||
                         translation.toLower().contains(searchText);

    // Apply missing filter
    bool matchesMissingFilter = !showMissingOnly || isMissing;

    bool visible = matchesSearch && matchesMissingFilter;
    m_stringsTable->setRowHidden(row, !visible);

    if (visible) {
      visibleCount++;
    }
  }

  // Update status
  updateStatusBar();
}

void NMLocalizationPanel::highlightMissingTranslations() {
  if (!m_stringsTable) {
    return;
  }

  auto &style = NMStyleManager::instance();
  const auto &palette = style.palette();

  // Use warning colors for missing translations (more subtle for dark theme)
  const QColor missingBg = palette.warningSubtle;
  const QColor missingText = palette.warning;
  const QColor normalText = palette.textPrimary;
  const QColor mutedText = palette.textMuted;

  for (int row = 0; row < m_stringsTable->rowCount(); ++row) {
    auto *translationItem = m_stringsTable->item(row, 2);
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
    auto *keyItem = m_stringsTable->item(row, 0);
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
    auto *sourceItem = m_stringsTable->item(row, 1);
    if (sourceItem) {
      sourceItem->setForeground(palette.textSecondary);
    }
  }
}

void NMLocalizationPanel::updateStatusBar() {
  if (!m_statusLabel) {
    return;
  }

  auto &style = NMStyleManager::instance();
  const auto &palette = style.palette();

  int totalCount = 0;
  int visibleCount = 0;
  int missingCount = 0;

  for (int row = 0; row < m_stringsTable->rowCount(); ++row) {
    totalCount++;
    if (!m_stringsTable->isRowHidden(row)) {
      visibleCount++;
    }
    auto *translationItem = m_stringsTable->item(row, 2);
    if (translationItem && translationItem->text().isEmpty()) {
      missingCount++;
    }
  }

  // Calculate completion percentage
  int completedCount = totalCount - missingCount;
  int completionPercent =
      totalCount > 0 ? (completedCount * 100) / totalCount : 100;

  // Build styled status message
  QString status;

  // Show visible/total keys
  status +=
      QString(
          "<span style='color: %1;'>Showing <b>%2</b> of <b>%3</b> keys</span>")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
          .arg(visibleCount)
          .arg(totalCount);

  // Show missing count with color coding
  if (missingCount > 0) {
    status +=
        QString(" <span style='color: %1;'>●</span> ")
            .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
    status += QString("<span style='color: %1;'>Missing: <b>%2</b></span>")
                  .arg(NMStyleManager::colorToStyleString(palette.warning))
                  .arg(missingCount);
  }

  // Show completion percentage
  QString completionColor =
      completionPercent == 100
          ? NMStyleManager::colorToStyleString(palette.success)
          : NMStyleManager::colorToStyleString(palette.info);

  status += QString(" <span style='color: %1;'>●</span> ")
                .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
  status += QString("<span style='color: %1;'>Complete: <b>%2%</b></span>")
                .arg(completionColor)
                .arg(completionPercent);

  // Show modified indicator
  if (m_dirty) {
    status +=
        QString(" <span style='color: %1;'>●</span> ")
            .arg(NMStyleManager::colorToStyleString(palette.borderDefault));
    status +=
        QString("<span style='color: %1; font-weight: 600;'>● MODIFIED</span>")
            .arg(NMStyleManager::colorToStyleString(
                style.panelAccents().localization));
  }

  m_statusLabel->setText(status);
}

void NMLocalizationPanel::onSearchTextChanged(const QString &text) {
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
  if (!localeCode.isEmpty()) {
    loadLocale(localeCode);
  }
}

void NMLocalizationPanel::onCellChanged(int row, int column) {
  if (!m_stringsTable || column != 2) {
    return;
  }

  auto *idItem = m_stringsTable->item(row, 0);
  auto *valueItem = m_stringsTable->item(row, 2);
  if (!idItem || !valueItem || m_currentLocale.isEmpty()) {
    return;
  }

  const QString key = idItem->text();
  const QString value = valueItem->text();

  // Capture old value for undo
  QString oldValue;
  if (m_entries.contains(key)) {
    oldValue = m_entries[key].translations.value(m_currentLocale, "");

    // Only create undo command if value changed
    if (oldValue != value) {
      auto *cmd = new ChangeTranslationCommand(this, key, m_currentLocale,
                                               oldValue, value);
      NMUndoManager::instance().pushCommand(cmd);
    }

    // Update in-memory entry
    m_entries[key].translations[m_currentLocale] = value;
    m_entries[key].isMissing = value.isEmpty();
    m_entries[key].isModified = true;
  }

  // Update manager
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.setString(locale, key.toStdString(), value.toStdString());

  setDirty(true);
  highlightMissingTranslations();
  updateStatusBar();

  emit translationChanged(key, m_currentLocale, value);
}

void NMLocalizationPanel::onAddKeyClicked() {
  QString key;
  QString defaultValue;

  if (!showAddKeyDialog(key, defaultValue)) {
    return;
  }

  if (addKey(key, defaultValue)) {
    // Create undo command for adding key
    auto *cmd = new AddLocalizationKeyCommand(this, key, defaultValue);
    NMUndoManager::instance().pushCommand(cmd);

    rebuildTable();

    // PERF-4: Use O(1) lookup to select the new row instead of O(n) search
    auto rowIt = m_keyToRowMap.find(key);
    if (rowIt != m_keyToRowMap.end()) {
      int row = rowIt.value();
      auto *item = m_stringsTable->item(row, 0);
      if (item) {
        m_stringsTable->selectRow(row);
        m_stringsTable->scrollToItem(item);
      }
    }
  }
}

bool NMLocalizationPanel::showAddKeyDialog(QString &outKey,
                                           QString &outDefaultValue) {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Add Localization Key"));
  dialog.setMinimumWidth(400);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  QFormLayout *formLayout = new QFormLayout();

  QLineEdit *keyEdit = new QLineEdit(&dialog);
  keyEdit->setPlaceholderText(tr("e.g., menu.button.start"));
  formLayout->addRow(tr("Key:"), keyEdit);

  QLineEdit *valueEdit = new QLineEdit(&dialog);
  valueEdit->setPlaceholderText(tr("Default value (optional)"));
  formLayout->addRow(tr("Default Value:"), valueEdit);

  QLabel *errorLabel = new QLabel(&dialog);
  errorLabel->setStyleSheet("color: red;");
  errorLabel->hide();

  layout->addLayout(formLayout);
  layout->addWidget(errorLabel);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
  okButton->setEnabled(false);

  layout->addWidget(buttonBox);

  // Validation on key change
  connect(keyEdit, &QLineEdit::textChanged,
          [this, okButton, errorLabel, keyEdit](const QString &text) {
            QString error;
            bool valid = true;

            if (text.isEmpty()) {
              valid = false;
              error = tr("Key cannot be empty");
            } else if (!isValidKeyName(text)) {
              valid = false;
              error = tr("Key must contain only letters, numbers, underscore, "
                         "dot, or dash");
            } else if (!isKeyUnique(text)) {
              valid = false;
              error = tr("Key already exists");
            }

            okButton->setEnabled(valid);
            if (!valid && !text.isEmpty()) {
              errorLabel->setText(error);
              errorLabel->show();
            } else {
              errorLabel->hide();
            }
          });

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  outKey = keyEdit->text().trimmed();
  outDefaultValue = valueEdit->text();
  return true;
}

bool NMLocalizationPanel::isValidKeyName(const QString &key) const {
  return s_keyValidationRegex.match(key).hasMatch();
}

bool NMLocalizationPanel::isKeyUnique(const QString &key) const {
  return !m_entries.contains(key) || m_entries.value(key).isDeleted;
}

bool NMLocalizationPanel::addKey(const QString &key,
                                 const QString &defaultValue) {
  if (key.isEmpty() || !isValidKeyName(key) || !isKeyUnique(key)) {
    return false;
  }

  LocalizationEntry entry;
  entry.key = key;
  entry.translations[m_defaultLocale] = defaultValue;
  entry.isNew = true;
  entry.isMissing = m_currentLocale != m_defaultLocale;

  m_entries[key] = entry;

  // Add to manager
  NovelMind::localization::LocaleId locale;
  locale.language = m_defaultLocale.toStdString();
  m_localization.setString(locale, key.toStdString(),
                           defaultValue.toStdString());

  setDirty(true);
  return true;
}

void NMLocalizationPanel::onDeleteKeyClicked() {
  if (!m_stringsTable) {
    return;
  }

  QList<QTableWidgetItem *> selectedItems = m_stringsTable->selectedItems();
  if (selectedItems.isEmpty()) {
    NMMessageDialog::showInfo(this, tr("Delete Key"),
                              tr("Please select a key to delete."));
    return;
  }

  // Get unique keys from selection
  QSet<QString> keysToDelete;
  for (auto *item : selectedItems) {
    int row = item->row();
    auto *idItem = m_stringsTable->item(row, 0);
    if (idItem) {
      keysToDelete.insert(idItem->text());
    }
  }

  QString message;
  if (keysToDelete.size() == 1) {
    message = tr("Are you sure you want to delete the key '%1'?")
                  .arg(*keysToDelete.begin());
  } else {
    message =
        tr("Are you sure you want to delete %1 keys?").arg(keysToDelete.size());
  }

  NMDialogButton result = NMMessageDialog::showQuestion(
      this, tr("Confirm Delete"), message,
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  // Create macro for deleting multiple keys
  if (keysToDelete.size() > 1) {
    NMUndoManager::instance().beginMacro("Delete Localization Keys");
  }

  for (const QString &key : keysToDelete) {
    // Capture translations before deleting
    QHash<QString, QString> translations;
    if (m_entries.contains(key)) {
      translations = m_entries[key].translations;
    }

    // Create and push delete command
    auto *cmd = new DeleteLocalizationKeyCommand(this, key, translations);
    NMUndoManager::instance().pushCommand(cmd);

    deleteKey(key);
  }

  if (keysToDelete.size() > 1) {
    NMUndoManager::instance().endMacro();
  }

  rebuildTable();
}

bool NMLocalizationPanel::deleteKey(const QString &key) {
  if (!m_entries.contains(key)) {
    return false;
  }

  m_entries[key].isDeleted = true;
  m_deletedKeys.insert(key);

  // Remove from manager
  NovelMind::localization::LocaleId defaultLocale;
  defaultLocale.language = m_defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLocale;
  currentLocale.language = m_currentLocale.toStdString();

  m_localization.removeString(defaultLocale, key.toStdString());
  m_localization.removeString(currentLocale, key.toStdString());

  setDirty(true);
  return true;
}

void NMLocalizationPanel::onContextMenu(const QPoint &pos) {
  QMenu menu(this);

  QAction *addAction = menu.addAction(tr("Add Key..."));
  connect(addAction, &QAction::triggered, this,
          &NMLocalizationPanel::onAddKeyClicked);

  auto *idItem = m_stringsTable->itemAt(pos);
  if (idItem) {
    menu.addSeparator();

    QAction *deleteAction = menu.addAction(tr("Delete Key"));
    connect(deleteAction, &QAction::triggered, this,
            &NMLocalizationPanel::onDeleteKeyClicked);

    QAction *copyKeyAction = menu.addAction(tr("Copy Key"));
    connect(copyKeyAction, &QAction::triggered, [this, idItem]() {
      int row = idItem->row();
      auto *keyItem = m_stringsTable->item(row, 0);
      if (keyItem) {
        QApplication::clipboard()->setText(keyItem->text());
      }
    });
  }

  menu.exec(m_stringsTable->viewport()->mapToGlobal(pos));
}

QStringList
NMLocalizationPanel::findMissingTranslations(const QString &locale) const {
  QStringList missing;

  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    if (!entry.translations.contains(locale) ||
        entry.translations.value(locale).isEmpty()) {
      missing.append(entry.key);
    }
  }

  return missing;
}

QStringList NMLocalizationPanel::findUnusedKeys() const {
  QStringList unused;

  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    if (entry.isUnused || entry.usageLocations.isEmpty()) {
      unused.append(entry.key);
    }
  }

  return unused;
}

void NMLocalizationPanel::scanProjectForUsages() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  const QString projectPath = QString::fromStdString(pm.getProjectPath());

  // Clear existing usage data
  for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
    it->usageLocations.clear();
    it->isUnused = true;
  }

  // Directories to scan for script files
  QStringList scanDirs = {projectPath + "/Scripts", projectPath + "/Scenes",
                          projectPath + "/Data"};

  // File extensions to scan
  QStringList extensions = {"*.nms", "*.json", "*.yaml", "*.yml", "*.xml"};

  // Regex patterns to find localization key references
  // Matches patterns like: localize("key"), tr("key"), @key, loc:key
  QRegularExpression keyRefPattern(
      R"((?:localize|tr|getText)\s*\(\s*["\']([^"\']+)["\']\)|@([A-Za-z0-9_.-]+)|loc:([A-Za-z0-9_.-]+))");

  for (const QString &scanDir : scanDirs) {
    QDir dir(scanDir);
    if (!dir.exists()) {
      continue;
    }

    QDirIterator it(scanDir, extensions, QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
      const QString filePath = it.next();
      QFile file(filePath);

      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        continue;
      }

      QTextStream in(&file);
      int lineNumber = 0;

      while (!in.atEnd()) {
        const QString line = in.readLine();
        lineNumber++;

        QRegularExpressionMatchIterator matchIt =
            keyRefPattern.globalMatch(line);

        while (matchIt.hasNext()) {
          QRegularExpressionMatch match = matchIt.next();
          QString foundKey;

          // Check which capture group matched
          if (!match.captured(1).isEmpty()) {
            foundKey = match.captured(1); // localize/tr/getText pattern
          } else if (!match.captured(2).isEmpty()) {
            foundKey = match.captured(2); // @key pattern
          } else if (!match.captured(3).isEmpty()) {
            foundKey = match.captured(3); // loc:key pattern
          }

          if (!foundKey.isEmpty() && m_entries.contains(foundKey)) {
            LocalizationEntry &entry = m_entries[foundKey];
            const QString usageLocation =
                QString("%1:%2").arg(filePath).arg(lineNumber);

            if (!entry.usageLocations.contains(usageLocation)) {
              entry.usageLocations.append(usageLocation);
            }
            entry.isUnused = false;
          }
        }
      }

      file.close();
    }
  }

  // Update the table to reflect new usage data
  rebuildTable();
  updateStatusBar();
}

void NMLocalizationPanel::navigateToUsage(const QString &key, int usageIndex) {
  if (!m_entries.contains(key)) {
    return;
  }

  const LocalizationEntry &entry = m_entries.value(key);
  if (usageIndex >= 0 && usageIndex < entry.usageLocations.size()) {
    emit navigateToFile(entry.usageLocations.at(usageIndex), 0);
  }
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

void NMLocalizationPanel::onSaveClicked() { saveChanges(); }

bool NMLocalizationPanel::saveChanges() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showError(this, tr("Save Failed"),
                               tr("No project is open."));
    return false;
  }

  syncEntriesToManager();

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  QDir dir(localizationRoot);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Save each locale to its file
  for (const QString &localeCode : m_availableLocales) {
    NovelMind::localization::LocaleId locale;
    locale.language = localeCode.toStdString();

    // Default to CSV format
    const QString filePath = dir.filePath(localeCode + ".csv");
    auto result = m_localization.exportStrings(
        locale, filePath.toStdString(),
        NovelMind::localization::LocalizationFormat::CSV);

    if (result.isError()) {
      NMMessageDialog::showError(
          this, tr("Save Failed"),
          tr("Failed to save %1: %2")
              .arg(localeCode)
              .arg(QString::fromStdString(result.error())));
      return false;
    }
  }

  m_deletedKeys.clear();
  setDirty(false);

  // Mark entries as not modified
  for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
    it->isModified = false;
    it->isNew = false;
  }

  return true;
}

void NMLocalizationPanel::exportLocale() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject() || m_currentLocale.isEmpty()) {
    return;
  }

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  const QString filter = tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString defaultName =
      QDir(localizationRoot).filePath(m_currentLocale + ".csv");
  const QString path = NMFileDialog::getSaveFileName(
      this, tr("Export Localization"), defaultName, filter);
  if (path.isEmpty()) {
    return;
  }

  syncEntriesToManager();

  const QFileInfo info(path);
  const auto format = formatForExtension(info.suffix());

  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  auto result =
      m_localization.exportStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(this, tr("Export Failed"),
                               QString::fromStdString(result.error()));
  }
}

void NMLocalizationPanel::importLocale() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showInfo(this, tr("Import Localization"),
                              tr("Open a project before importing strings."));
    return;
  }

  const QString filter = tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString path = NMFileDialog::getOpenFileName(
      this, tr("Import Localization"), QDir::homePath(), filter);
  if (path.isEmpty()) {
    return;
  }

  QFileInfo info(path);
  const QString localeCode = info.baseName();
  const auto format = formatForExtension(info.suffix());

  // Capture old values for undo support (for the target locale)
  QHash<QString, QString> oldValues;
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (!entry.isDeleted && entry.translations.contains(localeCode)) {
      oldValues[entry.key] = entry.translations.value(localeCode);
    }
  }

  NovelMind::localization::LocaleId locale;
  locale.language = localeCode.toStdString();
  auto result = m_localization.loadStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(this, tr("Import Failed"),
                               QString::fromStdString(result.error()));
    return;
  }

  // Switch to the imported locale and sync entries
  m_currentLocale = localeCode;
  syncEntriesFromManager();

  // Create undo macro for bulk import
  NMUndoManager::instance().beginMacro(
      QString("Import Locale: %1").arg(info.fileName()));

  // Create commands for each changed value
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const QString &key = entry.key;
    const QString newValue = entry.translations.value(localeCode, "");
    const QString oldValue = oldValues.value(key, "");

    // Only create command if value changed
    if (newValue != oldValue) {
      auto *cmd = new ChangeTranslationCommand(this, key, localeCode, oldValue,
                                               newValue);
      NMUndoManager::instance().pushCommand(cmd);
    }
  }

  NMUndoManager::instance().endMacro();

  refreshLocales();
  setDirty(true);
}

void NMLocalizationPanel::onUpdate([[maybe_unused]] double deltaTime) {}

void NMLocalizationPanel::onItemDoubleClicked(QTableWidgetItem *item) {
  if (!item) {
    return;
  }

  int row = item->row();
  auto *idItem = m_stringsTable->item(row, 0);
  if (idItem) {
    emit keySelected(idItem->text());
  }
}

void NMLocalizationPanel::onFilterChanged(int index) {
  Q_UNUSED(index);
  applyFilters();
}

void NMLocalizationPanel::onExportClicked() { exportLocale(); }

void NMLocalizationPanel::onImportClicked() { importLocale(); }

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

void NMLocalizationPanel::exportToCsv(const QString &filePath) {
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.exportStrings(
      locale, filePath.toStdString(),
      NovelMind::localization::LocalizationFormat::CSV);
}

void NMLocalizationPanel::exportToJson(const QString &filePath) {
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.exportStrings(
      locale, filePath.toStdString(),
      NovelMind::localization::LocalizationFormat::JSON);
}

void NMLocalizationPanel::importFromCsv(const QString &filePath) {
  // Capture old values for undo support
  QHash<QString, QString> oldValues;
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (!entry.isDeleted && entry.translations.contains(m_currentLocale)) {
      oldValues[entry.key] = entry.translations.value(m_currentLocale);
    }
  }

  // Perform the import
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.loadStrings(locale, filePath.toStdString(),
                             NovelMind::localization::LocalizationFormat::CSV);
  syncEntriesFromManager();

  // Create undo macro for bulk import
  QFileInfo fileInfo(filePath);
  NMUndoManager::instance().beginMacro(
      QString("Import CSV: %1").arg(fileInfo.fileName()));

  // Create commands for each changed value
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const QString &key = entry.key;
    const QString newValue = entry.translations.value(m_currentLocale, "");
    const QString oldValue = oldValues.value(key, "");

    // Only create command if value changed
    if (newValue != oldValue) {
      auto *cmd =
          new ChangeTranslationCommand(this, key, m_currentLocale, oldValue,
                                       newValue);
      NMUndoManager::instance().pushCommand(cmd);
    }
  }

  NMUndoManager::instance().endMacro();

  rebuildTable();
  setDirty(true);
}

void NMLocalizationPanel::importFromJson(const QString &filePath) {
  // Capture old values for undo support
  QHash<QString, QString> oldValues;
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (!entry.isDeleted && entry.translations.contains(m_currentLocale)) {
      oldValues[entry.key] = entry.translations.value(m_currentLocale);
    }
  }

  // Perform the import
  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  m_localization.loadStrings(locale, filePath.toStdString(),
                             NovelMind::localization::LocalizationFormat::JSON);
  syncEntriesFromManager();

  // Create undo macro for bulk import
  QFileInfo fileInfo(filePath);
  NMUndoManager::instance().beginMacro(
      QString("Import JSON: %1").arg(fileInfo.fileName()));

  // Create commands for each changed value
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry &entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const QString &key = entry.key;
    const QString newValue = entry.translations.value(m_currentLocale, "");
    const QString oldValue = oldValues.value(key, "");

    // Only create command if value changed
    if (newValue != oldValue) {
      auto *cmd =
          new ChangeTranslationCommand(this, key, m_currentLocale, oldValue,
                                       newValue);
      NMUndoManager::instance().pushCommand(cmd);
    }
  }

  NMUndoManager::instance().endMacro();

  rebuildTable();
  setDirty(true);
}

void NMLocalizationPanel::onExportMissingClicked() { exportMissingStrings(); }

void NMLocalizationPanel::exportMissingStrings() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject() || m_currentLocale.isEmpty()) {
    return;
  }

  const QString localizationRoot =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Localization));
  const QString filter = tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString defaultName =
      QDir(localizationRoot).filePath(m_currentLocale + "_missing.csv");
  const QString path = NMFileDialog::getSaveFileName(
      this, tr("Export Missing Translations"), defaultName, filter);
  if (path.isEmpty()) {
    return;
  }

  syncEntriesToManager();

  const QFileInfo info(path);
  const auto format = formatForExtension(info.suffix());

  NovelMind::localization::LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  auto result =
      m_localization.exportMissingStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(this, tr("Export Failed"),
                               QString::fromStdString(result.error()));
  } else {
    NMMessageDialog::showInfo(
        this, tr("Export Successful"),
        tr("Missing translations exported successfully to:\n%1").arg(path));
  }
}

void NMLocalizationPanel::onEditPluralFormsClicked() {
  if (!m_stringsTable) {
    return;
  }

  QList<QTableWidgetItem *> selectedItems = m_stringsTable->selectedItems();
  if (selectedItems.isEmpty()) {
    NMMessageDialog::showInfo(this, tr("Edit Plural Forms"),
                              tr("Please select a key to edit plural forms."));
    return;
  }

  int row = selectedItems.first()->row();
  auto *idItem = m_stringsTable->item(row, 0);
  if (!idItem) {
    return;
  }

  const QString key = idItem->text();
  showPluralFormsDialog(key);
}

bool NMLocalizationPanel::showPluralFormsDialog(const QString &key) {
  using namespace NovelMind::localization;

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Edit Plural Forms - %1").arg(key));
  dialog.setMinimumWidth(600);
  dialog.setMinimumHeight(400);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);

  // Instructions
  QLabel *infoLabel = new QLabel(
      tr("Define plural forms for different count values:\n"
         "Zero (0), One (1), Two (2), Few (3-10), Many (11+), Other (default)"),
      &dialog);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  // Get current localized string
  LocaleId locale;
  locale.language = m_currentLocale.toStdString();
  auto *table = m_localization.getStringTableMutable(locale);

  QHash<PluralCategory, QLineEdit *> formEdits;

  QFormLayout *formLayout = new QFormLayout();

  const QList<QPair<PluralCategory, QString>> categories = {
      {PluralCategory::Zero, tr("Zero (count = 0):")},
      {PluralCategory::One, tr("One (count = 1):")},
      {PluralCategory::Two, tr("count = 2):")},
      {PluralCategory::Few, tr("Few (3-10):")},
      {PluralCategory::Many, tr("Many (11+):")},
      {PluralCategory::Other, tr("Other (default):")}};

  for (const auto &[category, label] : categories) {
    QLineEdit *edit = new QLineEdit(&dialog);
    if (table) {
      const auto &strings = table->getStrings();
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

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
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
      setDirty(true);
      syncEntriesFromManager();
      rebuildTable();
    }
  }

  return true;
}

void NMLocalizationPanel::onToggleRTLPreview(bool checked) {
  applyRTLLayout(checked);
}

void NMLocalizationPanel::applyRTLLayout(bool rtl) {
  if (!m_stringsTable) {
    return;
  }

  Qt::LayoutDirection direction = rtl ? Qt::RightToLeft : Qt::LeftToRight;

  // Apply to table
  m_stringsTable->setLayoutDirection(direction);

  // Apply to preview
  if (m_previewOutput) {
    m_previewOutput->setLayoutDirection(direction);
  }

  // Update alignment for translation column
  for (int row = 0; row < m_stringsTable->rowCount(); ++row) {
    auto *translationItem = m_stringsTable->item(row, 2);
    if (translationItem) {
      if (rtl) {
        translationItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      } else {
        translationItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      }
    }
  }
}

void NMLocalizationPanel::onPreviewVariablesChanged() {
  if (!m_previewInput || !m_previewOutput) {
    return;
  }

  // Parse variables from input (format: var1=value1,var2=value2)
  m_previewVariables.clear();
  const QString input = m_previewInput->text();
  const QStringList pairs = input.split(',', Qt::SkipEmptyParts);

  for (const QString &pair : pairs) {
    const QStringList parts = pair.split('=');
    if (parts.size() == 2) {
      m_previewVariables[parts[0].trimmed()] = parts[1].trimmed();
    }
  }

  updatePreview();
}

void NMLocalizationPanel::updatePreview() {
  if (!m_previewOutput || !m_stringsTable) {
    return;
  }

  // Get currently selected key
  QList<QTableWidgetItem *> selectedItems = m_stringsTable->selectedItems();
  if (selectedItems.isEmpty()) {
    m_previewOutput->setText(tr("(select a key to preview)"));
    return;
  }

  int row = selectedItems.first()->row();
  auto *translationItem = m_stringsTable->item(row, 2);
  if (!translationItem) {
    m_previewOutput->setText("");
    return;
  }

  QString text = translationItem->text();

  // Apply variable interpolation
  std::unordered_map<std::string, std::string> variables;
  for (auto it = m_previewVariables.begin(); it != m_previewVariables.end();
       ++it) {
    variables[it.key().toStdString()] = it.value().toStdString();
  }

  std::string stdText = text.toStdString();
  std::string interpolated = m_localization.interpolate(stdText, variables);

  m_previewOutput->setText(QString::fromStdString(interpolated));
}

int NMLocalizationPanel::importDialogueEntries(
    const QList<QPair<QString, QString>> &entries) {
  if (entries.isEmpty()) {
    return 0;
  }

  int imported = 0;
  for (const auto &entry : entries) {
    const QString &key = entry.first;
    const QString &sourceText = entry.second;

    // Skip if key already exists and has a value
    if (m_entries.contains(key) && !m_entries[key].isDeleted) {
      // Update source text if different
      if (m_entries[key].translations.value(m_defaultLocale) != sourceText) {
        m_entries[key].translations[m_defaultLocale] = sourceText;
        m_entries[key].isModified = true;
        ++imported;
      }
      continue;
    }

    // Add new entry
    LocalizationEntry newEntry;
    newEntry.key = key;
    newEntry.translations[m_defaultLocale] = sourceText;
    newEntry.isNew = true;
    newEntry.isMissing = (m_currentLocale != m_defaultLocale);

    m_entries[key] = newEntry;

    // Add to localization manager
    NovelMind::localization::LocaleId locale;
    locale.language = m_defaultLocale.toStdString();
    m_localization.setString(locale, key.toStdString(),
                             sourceText.toStdString());

    ++imported;
  }

  if (imported > 0) {
    setDirty(true);
    rebuildTable();
  }

  return imported;
}

bool NMLocalizationPanel::hasTranslation(const QString &key) const {
  if (!m_entries.contains(key)) {
    return false;
  }

  const LocalizationEntry &entry = m_entries.value(key);
  if (entry.isDeleted) {
    return false;
  }

  // Check if translation exists for current locale
  if (m_currentLocale == m_defaultLocale) {
    return entry.translations.contains(m_defaultLocale) &&
           !entry.translations.value(m_defaultLocale).isEmpty();
  }

  return entry.translations.contains(m_currentLocale) &&
         !entry.translations.value(m_currentLocale).isEmpty();
}

QString NMLocalizationPanel::getTranslation(const QString &key) const {
  if (!m_entries.contains(key)) {
    return QString();
  }

  const LocalizationEntry &entry = m_entries.value(key);
  if (entry.isDeleted) {
    return QString();
  }

  // Return translation for current locale, fallback to default
  if (entry.translations.contains(m_currentLocale)) {
    return entry.translations.value(m_currentLocale);
  }

  return entry.translations.value(m_defaultLocale);
}

/**
 * @brief Set a translation value for a specific key and locale
 *
 * PERF-4 optimization: Uses m_keyToRowMap for O(1) row lookup instead
 * of O(n) linear search through all table rows.
 */
void NMLocalizationPanel::setTranslationValue(const QString &key,
                                              const QString &locale,
                                              const QString &value) {
  // Update the in-memory entry
  if (m_entries.contains(key)) {
    m_entries[key].translations[locale] = value;
    m_entries[key].isMissing = value.isEmpty();
    m_entries[key].isModified = true;
    setDirty(true);

    // Update the table if the key is currently displayed
    if (m_stringsTable && locale == m_currentLocale) {
      // PERF-4: Use O(1) hash lookup instead of O(n) linear search
      auto rowIt = m_keyToRowMap.find(key);
      if (rowIt != m_keyToRowMap.end()) {
        int row = rowIt.value();
        // Update the translation cell (column 2)
        auto *valueItem = m_stringsTable->item(row, 2);
        if (valueItem) {
          // Block signals to prevent creating a new undo command
          bool wasBlocked = m_stringsTable->blockSignals(true);
          valueItem->setText(value);
          m_stringsTable->blockSignals(wasBlocked);
        }
      }
    }
  }
}

} // namespace NovelMind::editor::qt
