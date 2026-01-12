#include "NovelMind/editor/qt/panels/nm_localization_ui_setup.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

void NMLocalizationUISetup::setupUI(NMLocalizationPanel* panel) {
  if (!panel) {
    return;
  }

  auto& style = NMStyleManager::instance();
  const auto& palette = style.palette();
  const auto& spacing = style.spacing();
  auto& iconMgr = NMIconManager::instance();

  QVBoxLayout* layout = new QVBoxLayout(panel->contentWidget());
  layout->setContentsMargins(spacing.xs, spacing.xs, spacing.xs, spacing.xs);
  layout->setSpacing(spacing.sm);

  // Top toolbar: Language selector and file operations
  QFrame* topToolbar = new QFrame(panel->contentWidget());
  topToolbar->setFrameStyle(QFrame::NoFrame);
  QHBoxLayout* topLayout = new QHBoxLayout(topToolbar);
  topLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm, spacing.sm);
  topLayout->setSpacing(spacing.md);

  // Language selection group
  QLabel* langLabel = new QLabel(QObject::tr("Language:"), topToolbar);
  langLabel->setStyleSheet(QString("color: %1; font-weight: 600;")
                               .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  topLayout->addWidget(langLabel);

  panel->m_languageSelector = new QComboBox(topToolbar);
  panel->m_languageSelector->setMinimumWidth(120);
  QObject::connect(panel->m_languageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
                   panel, &NMLocalizationPanel::onLocaleChanged);
  topLayout->addWidget(panel->m_languageSelector);

  topLayout->addStretch();

  // File operations group (Import/Export/Save)
  panel->m_importButton = new QPushButton(QObject::tr("Import..."), topToolbar);
  panel->m_importButton->setIcon(iconMgr.getIcon("import", 16));
  panel->m_importButton->setToolTip(QObject::tr("Import localization file"));
  QObject::connect(panel->m_importButton, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onImportClicked);
  topLayout->addWidget(panel->m_importButton);

  panel->m_exportButton = new QPushButton(QObject::tr("Export..."), topToolbar);
  panel->m_exportButton->setIcon(iconMgr.getIcon("export", 16));
  panel->m_exportButton->setToolTip(QObject::tr("Export current locale to file"));
  QObject::connect(panel->m_exportButton, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onExportClicked);
  topLayout->addWidget(panel->m_exportButton);

  panel->m_exportMissingBtn = new QPushButton(QObject::tr("Export Missing..."), topToolbar);
  panel->m_exportMissingBtn->setIcon(iconMgr.getIcon("export", 16));
  panel->m_exportMissingBtn->setToolTip(
      QObject::tr("Export missing translations for current locale"));
  QObject::connect(panel->m_exportMissingBtn, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onExportMissingClicked);
  topLayout->addWidget(panel->m_exportMissingBtn);

  panel->m_saveBtn = new QPushButton(QObject::tr("Save"), topToolbar);
  panel->m_saveBtn->setIcon(iconMgr.getIcon("file-save", 16));
  panel->m_saveBtn->setEnabled(false);
  panel->m_saveBtn->setToolTip(QObject::tr("Save all changes"));
  panel->m_saveBtn->setStyleSheet(
      QString("QPushButton { background-color: %1; color: %2; font-weight: 600; }"
              "QPushButton:hover { background-color: %3; }"
              "QPushButton:disabled { background-color: %4; color: %5; }")
          .arg(NMStyleManager::colorToStyleString(style.panelAccents().localization))
          .arg(NMStyleManager::colorToStyleString(palette.textInverse))
          .arg(NMStyleManager::colorToRgbaString(style.panelAccents().localization, 220))
          .arg(NMStyleManager::colorToStyleString(palette.bgLight))
          .arg(NMStyleManager::colorToStyleString(palette.textDisabled)));
  QObject::connect(panel->m_saveBtn, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onSaveClicked);
  topLayout->addWidget(panel->m_saveBtn);

  layout->addWidget(topToolbar);

  // Filter and action bar
  QFrame* filterToolbar = new QFrame(panel->contentWidget());
  filterToolbar->setFrameStyle(QFrame::NoFrame);
  QHBoxLayout* filterLayout = new QHBoxLayout(filterToolbar);
  filterLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm, spacing.sm);
  filterLayout->setSpacing(spacing.md);

  // Search box
  panel->m_searchEdit = new QLineEdit(filterToolbar);
  panel->m_searchEdit->setPlaceholderText(
      QObject::tr("Search keys, source, or translations..."));
  panel->m_searchEdit->setClearButtonEnabled(true);
  QObject::connect(panel->m_searchEdit, &QLineEdit::textChanged, panel,
                   &NMLocalizationPanel::onSearchTextChanged);
  filterLayout->addWidget(panel->m_searchEdit, 1);

  // Filter checkbox
  panel->m_showMissingOnly = new QCheckBox(QObject::tr("Show Missing Only"), filterToolbar);
  panel->m_showMissingOnly->setStyleSheet(
      QString("QCheckBox { color: %1; }").arg(NMStyleManager::colorToStyleString(palette.warning)));
  QObject::connect(panel->m_showMissingOnly, &QCheckBox::toggled, panel,
                   &NMLocalizationPanel::onShowOnlyMissingToggled);
  filterLayout->addWidget(panel->m_showMissingOnly);

  // Add separator
  QFrame* separator = new QFrame(filterToolbar);
  separator->setFrameShape(QFrame::VLine);
  separator->setFrameShadow(QFrame::Sunken);
  separator->setStyleSheet(
      QString("color: %1;").arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  filterLayout->addWidget(separator);

  // Edit operations group (Add/Delete)
  panel->m_addKeyBtn = new QPushButton(QObject::tr("+ Add Key"), filterToolbar);
  panel->m_addKeyBtn->setIcon(iconMgr.getIcon("add", 16));
  panel->m_addKeyBtn->setToolTip(QObject::tr("Add new localization key"));
  panel->m_addKeyBtn->setStyleSheet(
      QString("QPushButton { background-color: %1; color: %2; font-weight: 600; }"
              "QPushButton:hover { background-color: %3; }")
          .arg(NMStyleManager::colorToStyleString(palette.successSubtle))
          .arg(NMStyleManager::colorToStyleString(palette.success))
          .arg(NMStyleManager::colorToStyleString(palette.success)));
  QObject::connect(panel->m_addKeyBtn, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onAddKeyClicked);
  filterLayout->addWidget(panel->m_addKeyBtn);

  panel->m_deleteKeyBtn = new QPushButton(QObject::tr("Delete Key"), filterToolbar);
  panel->m_deleteKeyBtn->setIcon(iconMgr.getIcon("delete", 16));
  panel->m_deleteKeyBtn->setToolTip(QObject::tr("Delete selected key(s)"));
  panel->m_deleteKeyBtn->setStyleSheet(
      QString("QPushButton { background-color: %1; color: %2; }"
              "QPushButton:hover { background-color: %3; color: %4; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgMedium))
          .arg(NMStyleManager::colorToStyleString(palette.error))
          .arg(NMStyleManager::colorToStyleString(palette.errorSubtle))
          .arg(NMStyleManager::colorToStyleString(palette.error)));
  QObject::connect(panel->m_deleteKeyBtn, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onDeleteKeyClicked);
  filterLayout->addWidget(panel->m_deleteKeyBtn);

  // Add separator
  QFrame* separator2 = new QFrame(filterToolbar);
  separator2->setFrameShape(QFrame::VLine);
  separator2->setFrameShadow(QFrame::Sunken);
  separator2->setStyleSheet(
      QString("color: %1;").arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  filterLayout->addWidget(separator2);

  // Plural Forms button
  panel->m_pluralFormsBtn = new QPushButton(QObject::tr("Plural Forms..."), filterToolbar);
  panel->m_pluralFormsBtn->setIcon(iconMgr.getIcon("settings", 16));
  panel->m_pluralFormsBtn->setToolTip(QObject::tr("Edit plural forms for selected key"));
  QObject::connect(panel->m_pluralFormsBtn, &QPushButton::clicked, panel,
                   &NMLocalizationPanel::onEditPluralFormsClicked);
  filterLayout->addWidget(panel->m_pluralFormsBtn);

  // RTL Preview checkbox
  panel->m_rtlPreviewCheckbox = new QCheckBox(QObject::tr("RTL Preview"), filterToolbar);
  panel->m_rtlPreviewCheckbox->setToolTip(QObject::tr("Toggle right-to-left text preview"));
  QObject::connect(panel->m_rtlPreviewCheckbox, &QCheckBox::toggled, panel,
                   &NMLocalizationPanel::onToggleRTLPreview);
  filterLayout->addWidget(panel->m_rtlPreviewCheckbox);

  layout->addWidget(filterToolbar);

  // Table with improved styling
  panel->m_stringsTable = new QTableWidget(panel->contentWidget());
  panel->m_stringsTable->setObjectName("LocalizationTable");
  panel->m_stringsTable->setColumnCount(3);
  panel->m_stringsTable->setHorizontalHeaderLabels(
      {QObject::tr("Key"), QObject::tr("Source Text"), QObject::tr("Translation")});
  panel->m_stringsTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                         QAbstractItemView::EditKeyPressed);
  panel->m_stringsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  panel->m_stringsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
  panel->m_stringsTable->setContextMenuPolicy(Qt::CustomContextMenu);
  panel->m_stringsTable->setAlternatingRowColors(true);
  panel->m_stringsTable->setShowGrid(true);
  panel->m_stringsTable->verticalHeader()->setVisible(false);
  panel->m_stringsTable->verticalHeader()->setDefaultSectionSize(28);
  panel->m_stringsTable->horizontalHeader()->setStretchLastSection(true);
  panel->m_stringsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

  // Set column widths
  panel->m_stringsTable->setColumnWidth(0, 200); // Key column
  panel->m_stringsTable->setColumnWidth(1, 300); // Source column

  // Apply custom table styling
  panel->m_stringsTable->setStyleSheet(
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
          .arg(NMStyleManager::colorToStyleString(style.panelAccents().localization)));

  QObject::connect(panel->m_stringsTable, &QTableWidget::cellChanged, panel,
                   &NMLocalizationPanel::onCellChanged);
  QObject::connect(panel->m_stringsTable, &QTableWidget::customContextMenuRequested, panel,
                   &NMLocalizationPanel::onContextMenu);
  QObject::connect(panel->m_stringsTable, &QTableWidget::itemSelectionChanged, panel,
                   &NMLocalizationPanel::updatePreview);
  layout->addWidget(panel->m_stringsTable, 1);

  // Preview Panel with variable interpolation
  panel->m_previewPanel = new QWidget(panel->contentWidget());
  QHBoxLayout* previewLayout = new QHBoxLayout(panel->m_previewPanel);
  previewLayout->setContentsMargins(spacing.sm, spacing.sm, spacing.sm, spacing.sm);
  previewLayout->setSpacing(spacing.md);

  QLabel* previewLabel = new QLabel(QObject::tr("Preview with variables:"), panel->m_previewPanel);
  previewLabel->setStyleSheet(QString("color: %1; font-weight: 600;")
                                  .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));
  previewLayout->addWidget(previewLabel);

  panel->m_previewInput = new QLineEdit(panel->m_previewPanel);
  panel->m_previewInput->setPlaceholderText(QObject::tr("e.g., name=John,count=5"));
  panel->m_previewInput->setToolTip(
      QObject::tr("Enter variable values in format: var1=value1,var2=value2"));
  QObject::connect(panel->m_previewInput, &QLineEdit::textChanged, panel,
                   &NMLocalizationPanel::onPreviewVariablesChanged);
  previewLayout->addWidget(panel->m_previewInput, 1);

  panel->m_previewOutput = new QLabel(panel->m_previewPanel);
  panel->m_previewOutput->setStyleSheet(
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
  panel->m_previewOutput->setMinimumWidth(200);
  previewLayout->addWidget(panel->m_previewOutput, 1);

  layout->addWidget(panel->m_previewPanel);

  // Status bar with improved styling
  panel->m_statusLabel = new QLabel(panel->contentWidget());
  panel->m_statusLabel->setStyleSheet(QString("QLabel {"
                                              "  color: %1;"
                                              "  padding: 4px 8px;"
                                              "  background-color: %2;"
                                              "  border-top: 1px solid %3;"
                                              "}")
                                          .arg(NMStyleManager::colorToStyleString(palette.textSecondary))
                                          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
                                          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));
  layout->addWidget(panel->m_statusLabel);

  // Connect signal for undo/redo to trigger UI refresh
  QObject::connect(panel, &NMLocalizationPanel::localizationDataChanged, panel,
                   &NMLocalizationPanel::rebuildTable);

  // Note: refreshLocales() will be called by the panel after setupUI returns
}

} // namespace NovelMind::editor::qt
