/**
 * @file nm_settings_dialog.cpp
 * @brief Settings Dialog implementation
 */

#include "NovelMind/editor/qt/nm_settings_dialog.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <set>

namespace NovelMind::editor {

// ============================================================================
// NMSettingWidget Base Class
// ============================================================================

NMSettingWidget::NMSettingWidget(const SettingDefinition &def, QWidget *parent)
    : QWidget(parent), m_definition(def) {}

// ============================================================================
// NMBoolSettingWidget
// ============================================================================

NMBoolSettingWidget::NMBoolSettingWidget(const SettingDefinition &def,
                                         QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_checkbox = new QCheckBox(QString::fromStdString(def.displayName), this);
  m_checkbox->setToolTip(QString::fromStdString(def.description));

  layout->addWidget(m_checkbox);
  layout->addStretch();

  connect(m_checkbox, &QCheckBox::toggled, this,
          &NMSettingWidget::valueChanged);
}

SettingValue NMBoolSettingWidget::getValue() const {
  return m_checkbox->isChecked();
}

void NMBoolSettingWidget::setValue(const SettingValue &value) {
  try {
    m_checkbox->setChecked(std::get<bool>(value));
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set bool value for ") +
                        m_definition.key);
  }
}

// ============================================================================
// NMIntSettingWidget
// ============================================================================

NMIntSettingWidget::NMIntSettingWidget(const SettingDefinition &def,
                                       QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *label = new QLabel(QString::fromStdString(def.displayName), this);
  label->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(label);

  if (def.type == SettingType::IntRange) {
    // Use slider for ranges
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMinimum(static_cast<int>(def.minValue));
    m_slider->setMaximum(static_cast<int>(def.maxValue));
    m_slider->setToolTip(QString::fromStdString(def.description));

    m_valueLabel = new QLabel(this);
    m_valueLabel->setMinimumWidth(60);
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(m_slider, 1);
    layout->addWidget(m_valueLabel);

    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
      m_valueLabel->setText(QString::number(value));
      emit valueChanged();
    });
  } else {
    // Use spinbox
    m_spinBox = new QSpinBox(this);
    m_spinBox->setMinimum(-2147483648);
    m_spinBox->setMaximum(2147483647);
    m_spinBox->setToolTip(QString::fromStdString(def.description));

    layout->addWidget(m_spinBox);

    connect(m_spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &NMSettingWidget::valueChanged);
  }

  layout->addStretch();
}

SettingValue NMIntSettingWidget::getValue() const {
  if (m_slider) {
    return m_slider->value();
  } else if (m_spinBox) {
    return m_spinBox->value();
  }
  return 0;
}

void NMIntSettingWidget::setValue(const SettingValue &value) {
  try {
    i32 intValue = std::get<i32>(value);
    if (m_slider) {
      m_slider->setValue(intValue);
      m_valueLabel->setText(QString::number(intValue));
    } else if (m_spinBox) {
      m_spinBox->setValue(intValue);
    }
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set int value for ") +
                        m_definition.key);
  }
}

// ============================================================================
// NMFloatSettingWidget
// ============================================================================

NMFloatSettingWidget::NMFloatSettingWidget(const SettingDefinition &def,
                                           QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *label = new QLabel(QString::fromStdString(def.displayName), this);
  label->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(label);

  if (def.type == SettingType::FloatRange) {
    // Use slider for ranges (scaled to int)
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMinimum(0);
    m_slider->setMaximum(1000); // 0-1000 for 0.001 precision
    m_slider->setToolTip(QString::fromStdString(def.description));

    m_valueLabel = new QLabel(this);
    m_valueLabel->setMinimumWidth(60);
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(m_slider, 1);
    layout->addWidget(m_valueLabel);

    connect(m_slider, &QSlider::valueChanged, this, [this, def](int value) {
      f32 realValue = def.minValue + (static_cast<f32>(value) / 1000.0f) *
                                         (def.maxValue - def.minValue);
      m_valueLabel->setText(QString::number(realValue, 'f', 2));
      emit valueChanged();
    });
  } else {
    // Use double spinbox
    m_spinBox = new QDoubleSpinBox(this);
    m_spinBox->setMinimum(-1000000.0);
    m_spinBox->setMaximum(1000000.0);
    m_spinBox->setDecimals(3);
    m_spinBox->setToolTip(QString::fromStdString(def.description));

    layout->addWidget(m_spinBox);

    connect(m_spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &NMSettingWidget::valueChanged);
  }

  layout->addStretch();
}

SettingValue NMFloatSettingWidget::getValue() const {
  if (m_slider) {
    f32 range = m_definition.maxValue - m_definition.minValue;
    return m_definition.minValue +
           (static_cast<f32>(m_slider->value()) / 1000.0f) * range;
  } else if (m_spinBox) {
    return static_cast<f32>(m_spinBox->value());
  }
  return 0.0f;
}

void NMFloatSettingWidget::setValue(const SettingValue &value) {
  try {
    f32 floatValue = std::get<f32>(value);
    if (m_slider) {
      f32 range = m_definition.maxValue - m_definition.minValue;
      int sliderValue = static_cast<int>((floatValue - m_definition.minValue) /
                                         range * 1000.0f);
      m_slider->setValue(sliderValue);
      m_valueLabel->setText(QString::number(floatValue, 'f', 2));
    } else if (m_spinBox) {
      m_spinBox->setValue(floatValue);
    }
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set float value for ") +
                        m_definition.key);
  }
}

// ============================================================================
// NMStringSettingWidget
// ============================================================================

NMStringSettingWidget::NMStringSettingWidget(const SettingDefinition &def,
                                             QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *label = new QLabel(QString::fromStdString(def.displayName), this);
  label->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(label);

  m_lineEdit = new QLineEdit(this);
  m_lineEdit->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(m_lineEdit, 1);

  connect(m_lineEdit, &QLineEdit::textChanged, this,
          &NMSettingWidget::valueChanged);
}

SettingValue NMStringSettingWidget::getValue() const {
  return m_lineEdit->text().toStdString();
}

void NMStringSettingWidget::setValue(const SettingValue &value) {
  try {
    m_lineEdit->setText(QString::fromStdString(std::get<std::string>(value)));
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set string value for ") +
                        m_definition.key);
  }
}

// ============================================================================
// NMEnumSettingWidget
// ============================================================================

NMEnumSettingWidget::NMEnumSettingWidget(const SettingDefinition &def,
                                         QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *label = new QLabel(QString::fromStdString(def.displayName), this);
  label->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(label);

  m_comboBox = new QComboBox(this);
  m_comboBox->setToolTip(QString::fromStdString(def.description));

  // Add enum options
  for (const auto &option : def.enumOptions) {
    m_comboBox->addItem(QString::fromStdString(option));
  }

  layout->addWidget(m_comboBox, 1);
  layout->addStretch();

  connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMSettingWidget::valueChanged);
}

SettingValue NMEnumSettingWidget::getValue() const {
  return m_comboBox->currentText().toStdString();
}

void NMEnumSettingWidget::setValue(const SettingValue &value) {
  try {
    QString valueStr = QString::fromStdString(std::get<std::string>(value));
    int index = m_comboBox->findText(valueStr);
    if (index >= 0) {
      m_comboBox->setCurrentIndex(index);
    }
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set enum value for ") +
                        m_definition.key);
  }
}

// ============================================================================
// NMPathSettingWidget
// ============================================================================

NMPathSettingWidget::NMPathSettingWidget(const SettingDefinition &def,
                                         QWidget *parent)
    : NMSettingWidget(def, parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *label = new QLabel(QString::fromStdString(def.displayName), this);
  label->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(label);

  m_lineEdit = new QLineEdit(this);
  m_lineEdit->setToolTip(QString::fromStdString(def.description));
  layout->addWidget(m_lineEdit, 1);

  m_browseButton = new QPushButton("Browse...", this);
  m_browseButton->setIcon(qt::NMIconManager::instance().getIcon("folder-open", 16));
  layout->addWidget(m_browseButton);

  connect(m_lineEdit, &QLineEdit::textChanged, this,
          &NMSettingWidget::valueChanged);
  connect(m_browseButton, &QPushButton::clicked, this,
          &NMPathSettingWidget::onBrowseClicked);
}

SettingValue NMPathSettingWidget::getValue() const {
  return m_lineEdit->text().toStdString();
}

void NMPathSettingWidget::setValue(const SettingValue &value) {
  try {
    m_lineEdit->setText(QString::fromStdString(std::get<std::string>(value)));
  } catch (...) {
    NOVELMIND_LOG_ERROR(std::string("Failed to set path value for ") +
                        m_definition.key);
  }
}

void NMPathSettingWidget::onBrowseClicked() {
  QString path = qt::NMFileDialog::getExistingDirectory(
      this, QString::fromStdString(m_definition.displayName),
      m_lineEdit->text());
  if (!path.isEmpty()) {
    m_lineEdit->setText(path);
  }
}

// ============================================================================
// NMSettingsCategoryPage
// ============================================================================

NMSettingsCategoryPage::NMSettingsCategoryPage(const std::string &category,
                                               NMSettingsRegistry *registry,
                                               QWidget *parent)
    : QWidget(parent), m_category(category), m_registry(registry) {
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(16, 16, 16, 16);
  m_layout->setSpacing(12);

  rebuild();
}

void NMSettingsCategoryPage::rebuild() {
  // Clear existing widgets
  for (auto *widget : m_widgets) {
    m_layout->removeWidget(widget);
    widget->deleteLater();
  }
  m_widgets.clear();

  // Get settings for this category
  // INVARIANT: Copy settings to prevent iterator invalidation if registry
  // changes during widget creation/setValue() calls
  auto settingsSnapshot = m_registry->getByCategory(m_category);

  if (settingsSnapshot.empty()) {
    auto *emptyLabel = new QLabel("No settings found in this category.", this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(emptyLabel);
    m_layout->addStretch();
    return;
  }

  // Group settings by subcategory (if any)
  // For simplicity, we'll just list them all
  for (const auto &def : settingsSnapshot) {
    auto *widget = createWidgetForSetting(def);
    if (widget) {
      m_widgets.push_back(widget);
      m_layout->addWidget(widget);

      // Connect change signal
      connect(widget, &NMSettingWidget::valueChanged, this,
              &NMSettingsCategoryPage::settingChanged);

      // Set initial value
      auto value = m_registry->getValue(def.key);
      if (value) {
        widget->setValue(*value);
      }
    }
  }

  m_layout->addStretch();
}

void NMSettingsCategoryPage::applyValues() {
  for (auto *widget : m_widgets) {
    auto value = widget->getValue();
    std::string error = m_registry->setValue(widget->getKey(), value);
    if (!error.empty()) {
      NOVELMIND_LOG_ERROR(std::string("Failed to set ") + widget->getKey() +
                          ": " + error);
    }
  }
}

void NMSettingsCategoryPage::revertValues() {
  for (auto *widget : m_widgets) {
    auto value = m_registry->getValue(widget->getKey());
    if (value) {
      widget->setValue(*value);
    }
  }
}

void NMSettingsCategoryPage::resetToDefaults() {
  m_registry->resetCategoryToDefaults(m_category);

  // Update widgets
  for (auto *widget : m_widgets) {
    auto value = m_registry->getValue(widget->getKey());
    if (value) {
      widget->setValue(*value);
    }
  }
}

NMSettingWidget *
NMSettingsCategoryPage::createWidgetForSetting(const SettingDefinition &def) {
  switch (def.type) {
  case SettingType::Bool:
    return new NMBoolSettingWidget(def, this);
  case SettingType::Int:
  case SettingType::IntRange:
    return new NMIntSettingWidget(def, this);
  case SettingType::Float:
  case SettingType::FloatRange:
    return new NMFloatSettingWidget(def, this);
  case SettingType::String:
    return new NMStringSettingWidget(def, this);
  case SettingType::Enum:
    return new NMEnumSettingWidget(def, this);
  case SettingType::Path:
    return new NMPathSettingWidget(def, this);
  default:
    NOVELMIND_LOG_WARN(std::string("Unsupported setting type for ") + def.key);
    return nullptr;
  }
}

// ============================================================================
// NMSettingsDialog
// ============================================================================

NMSettingsDialog::NMSettingsDialog(NMSettingsRegistry *registry,
                                   QWidget *parent)
    : QDialog(parent), m_registry(registry) {
  setupUI();
  buildCategoryTree();
  buildCategoryPages();
  updateButtonStates();
}

NMSettingsDialog::~NMSettingsDialog() = default;

void NMSettingsDialog::setupUI() {
  setWindowTitle("Settings");
  resize(900, 600);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Search bar
  auto *searchWidget = new QWidget(this);
  auto *searchLayout = new QHBoxLayout(searchWidget);
  searchLayout->setContentsMargins(8, 8, 8, 8);

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText("Search settings...");
  searchLayout->addWidget(m_searchEdit);

  mainLayout->addWidget(searchWidget);

  // Splitter with tree and content
  auto *splitter = new QSplitter(Qt::Horizontal, this);

  // Category tree
  m_categoryTree = new QTreeWidget(this);
  m_categoryTree->setHeaderHidden(true);
  m_categoryTree->setMinimumWidth(200);
  m_categoryTree->setMaximumWidth(300);
  splitter->addWidget(m_categoryTree);

  // Content stack (with scroll area)
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  m_contentStack = new QStackedWidget(this);
  scrollArea->setWidget(m_contentStack);
  splitter->addWidget(scrollArea);

  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  mainLayout->addWidget(splitter, 1);

  // Button bar
  auto *buttonWidget = new QWidget(this);
  auto *buttonLayout = new QHBoxLayout(buttonWidget);
  buttonLayout->setContentsMargins(8, 8, 8, 8);

  auto& iconMgr = qt::NMIconManager::instance();

  m_resetButton = new QPushButton("Reset to Defaults", this);
  m_resetButton->setIcon(iconMgr.getIcon("refresh", 16));
  buttonLayout->addWidget(m_resetButton);

  buttonLayout->addStretch();

  m_applyButton = new QPushButton("Apply", this);
  m_applyButton->setIcon(iconMgr.getIcon("file-save", 16));
  m_revertButton = new QPushButton("Revert", this);
  m_revertButton->setIcon(iconMgr.getIcon("property-reset", 16));
  m_okButton = new QPushButton("OK", this);
  m_okButton->setIcon(iconMgr.getIcon("status-success", 16));
  m_cancelButton = new QPushButton("Cancel", this);
  m_cancelButton->setIcon(iconMgr.getIcon("file-close", 16));

  buttonLayout->addWidget(m_applyButton);
  buttonLayout->addWidget(m_revertButton);
  buttonLayout->addWidget(m_okButton);
  buttonLayout->addWidget(m_cancelButton);

  mainLayout->addWidget(buttonWidget);

  // Connect signals
  connect(
      m_searchEdit, &QLineEdit::textChanged, this,
      [this](const QString &text) { onSearchTextChanged(text.toStdString()); });

  connect(m_categoryTree, &QTreeWidget::itemClicked, this,
          &NMSettingsDialog::onCategorySelected);

  connect(m_applyButton, &QPushButton::clicked, this,
          &NMSettingsDialog::onApplyClicked);
  connect(m_revertButton, &QPushButton::clicked, this,
          &NMSettingsDialog::onRevertClicked);
  connect(m_resetButton, &QPushButton::clicked, this,
          &NMSettingsDialog::onResetClicked);
  connect(m_okButton, &QPushButton::clicked, this,
          &NMSettingsDialog::onOkClicked);
  connect(m_cancelButton, &QPushButton::clicked, this,
          &NMSettingsDialog::onCancelClicked);
}

void NMSettingsDialog::buildCategoryTree() {
  m_categoryTree->clear();
  m_treeItems.clear();

  // Build tree from all setting definitions
  const auto &definitions = m_registry->getAllDefinitions();

  // Collect unique categories
  std::set<std::string> categories;
  for (const auto &[key, def] : definitions) {
    categories.insert(def.category);
  }

  // Create tree items for each category
  for (const auto &category : categories) {
    findOrCreateCategoryItem(category);
  }

  // Expand all
  m_categoryTree->expandAll();

  // Select first item
  if (m_categoryTree->topLevelItemCount() > 0) {
    m_categoryTree->setCurrentItem(m_categoryTree->topLevelItem(0));
  }
}

void NMSettingsDialog::buildCategoryPages() {
  // Get all categories from the tree
  const auto &definitions = m_registry->getAllDefinitions();

  std::set<std::string> categories;
  for (const auto &[key, def] : definitions) {
    categories.insert(def.category);
  }

  // Create a page for each category
  for (const auto &category : categories) {
    auto *page = new NMSettingsCategoryPage(category, m_registry, this);
    m_pages[QString::fromStdString(category)] = page;
    m_contentStack->addWidget(page);

    // Connect change signal
    connect(page, &NMSettingsCategoryPage::settingChanged, this,
            &NMSettingsDialog::onSettingChanged);
  }
}

void NMSettingsDialog::updateButtonStates() {
  bool hasDirty = m_hasUnsavedChanges || m_registry->isDirty();
  m_applyButton->setEnabled(hasDirty);
  m_revertButton->setEnabled(hasDirty);
}

QTreeWidgetItem *
NMSettingsDialog::findOrCreateCategoryItem(const std::string &categoryPath) {
  QString qCategoryPath = QString::fromStdString(categoryPath);

  // Check if already exists
  if (m_treeItems.contains(qCategoryPath)) {
    return m_treeItems[qCategoryPath];
  }

  // Split category path (e.g. "Editor/General" -> ["Editor", "General"])
  QStringList parts = qCategoryPath.split('/');

  QTreeWidgetItem *parentItem = nullptr;
  QString currentPath;

  for (int i = 0; i < parts.size(); ++i) {
    if (i > 0)
      currentPath += "/";
    currentPath += parts[i];

    if (m_treeItems.contains(currentPath)) {
      parentItem = m_treeItems[currentPath];
    } else {
      QTreeWidgetItem *item;
      if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
      } else {
        item = new QTreeWidgetItem(m_categoryTree);
      }
      item->setText(0, parts[i]);
      item->setData(0, Qt::UserRole, currentPath);

      m_treeItems[currentPath] = item;
      parentItem = item;
    }
  }

  return parentItem;
}

void NMSettingsDialog::showCategory(const std::string &category) {
  QString qCategory = QString::fromStdString(category);
  if (m_pages.contains(qCategory)) {
    m_contentStack->setCurrentWidget(m_pages[qCategory]);
  }

  // Select in tree
  if (m_treeItems.contains(qCategory)) {
    m_categoryTree->setCurrentItem(m_treeItems[qCategory]);
  }
}

void NMSettingsDialog::showSetting(const std::string &key) {
  auto def = m_registry->getDefinition(key);
  if (def) {
    showCategory(def->category);
  }
}

void NMSettingsDialog::onSearchTextChanged(const std::string &text) {
  if (text.empty()) {
    // Show all categories
    buildCategoryTree();
    return;
  }

  // Search and filter
  auto results = m_registry->search(text);

  // Hide all items
  for (int i = 0; i < m_categoryTree->topLevelItemCount(); ++i) {
    m_categoryTree->topLevelItem(i)->setHidden(true);
  }

  // Show matching categories
  std::set<std::string> matchingCategories;
  for (const auto &def : results) {
    matchingCategories.insert(def.category);
  }

  for (const auto &category : matchingCategories) {
    QString qCategory = QString::fromStdString(category);
    if (m_treeItems.contains(qCategory)) {
      auto *item = m_treeItems[qCategory];
      item->setHidden(false);

      // Show all parents
      auto *parent = item->parent();
      while (parent) {
        parent->setHidden(false);
        parent = parent->parent();
      }
    }
  }
}

void NMSettingsDialog::onCategorySelected(QTreeWidgetItem *item, int column) {
  (void)column;

  if (!item)
    return;

  QString categoryPath = item->data(0, Qt::UserRole).toString();
  showCategory(categoryPath.toStdString());
}

void NMSettingsDialog::onApplyClicked() {
  // Apply all page values to registry
  for (auto *page : m_pages.values()) {
    page->applyValues();
  }

  m_registry->apply();
  m_hasUnsavedChanges = false;
  updateButtonStates();

  NOVELMIND_LOG_INFO("Settings applied");
}

void NMSettingsDialog::onRevertClicked() {
  // Revert all pages
  for (auto *page : m_pages.values()) {
    page->revertValues();
  }

  m_registry->revert();
  m_hasUnsavedChanges = false;
  updateButtonStates();

  NOVELMIND_LOG_INFO("Settings reverted");
}

void NMSettingsDialog::onResetClicked() {
  // Get current page
  auto *currentPage =
      qobject_cast<NMSettingsCategoryPage *>(m_contentStack->currentWidget());
  if (currentPage) {
    currentPage->resetToDefaults();
    m_hasUnsavedChanges = true;
    updateButtonStates();

    NOVELMIND_LOG_INFO(std::string("Category reset to defaults: ") +
                       currentPage->getCategory());
  }
}

void NMSettingsDialog::onOkClicked() {
  onApplyClicked();
  accept();
}

void NMSettingsDialog::onCancelClicked() {
  onRevertClicked();
  reject();
}

void NMSettingsDialog::onSettingChanged() {
  m_hasUnsavedChanges = true;
  updateButtonStates();
}

} // namespace NovelMind::editor
