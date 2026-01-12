#pragma once

/**
 * @file nm_settings_dialog.hpp
 * @brief Unity-style Settings Dialog
 *
 * Provides a centralized settings UI with:
 * - Tree view of categories (left panel)
 * - Content panel for settings (right panel)
 * - Search bar with filtering
 * - Apply/Revert/Reset buttons
 * - Support for Editor Preferences and Project Settings
 */

#include "NovelMind/editor/settings_registry.hpp"
#include <QDialog>
#include <QHash>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

class QLineEdit;
class QStackedWidget;
class QPushButton;
class QLabel;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QSlider;
class QTreeWidgetItem;

namespace NovelMind::editor {

// Forward declarations
class NMSettingsRegistry;

// ============================================================================
// Setting Widget Base Class
// ============================================================================

/**
 * @brief Base class for setting edit widgets
 */
class NMSettingWidget : public QWidget {
  Q_OBJECT

public:
  explicit NMSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);
  virtual ~NMSettingWidget() = default;

  /**
   * @brief Get current value from the widget
   */
  virtual SettingValue getValue() const = 0;

  /**
   * @brief Set value in the widget
   */
  virtual void setValue(const SettingValue& value) = 0;

  /**
   * @brief Get the setting key
   */
  const std::string& getKey() const { return m_definition.key; }

  /**
   * @brief Get the setting definition
   */
  const SettingDefinition& getDefinition() const { return m_definition; }

signals:
  void valueChanged();

protected:
  SettingDefinition m_definition;
};

// ============================================================================
// Specific Setting Widgets
// ============================================================================

/**
 * @brief Boolean setting (checkbox)
 */
class NMBoolSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMBoolSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private:
  QCheckBox* m_checkbox = nullptr;
};

/**
 * @brief Integer setting (spinbox or slider)
 */
class NMIntSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMIntSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private:
  QSpinBox* m_spinBox = nullptr;
  QSlider* m_slider = nullptr; // For IntRange type
  QLabel* m_valueLabel = nullptr;
};

/**
 * @brief Float setting (spinbox or slider)
 */
class NMFloatSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMFloatSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private:
  QDoubleSpinBox* m_spinBox = nullptr;
  QSlider* m_slider = nullptr; // For FloatRange type
  QLabel* m_valueLabel = nullptr;
};

/**
 * @brief String setting (line edit)
 */
class NMStringSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMStringSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private:
  QLineEdit* m_lineEdit = nullptr;
};

/**
 * @brief Enum setting (combobox)
 */
class NMEnumSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMEnumSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private:
  QComboBox* m_comboBox = nullptr;
};

/**
 * @brief Path setting (line edit + browse button)
 */
class NMPathSettingWidget : public NMSettingWidget {
  Q_OBJECT

public:
  explicit NMPathSettingWidget(const SettingDefinition& def, QWidget* parent = nullptr);

  SettingValue getValue() const override;
  void setValue(const SettingValue& value) override;

private slots:
  void onBrowseClicked();

private:
  QLineEdit* m_lineEdit = nullptr;
  QPushButton* m_browseButton = nullptr;
};

// ============================================================================
// Settings Category Page
// ============================================================================

/**
 * @brief Page showing all settings in a category
 */
class NMSettingsCategoryPage : public QWidget {
  Q_OBJECT

public:
  explicit NMSettingsCategoryPage(const std::string& category, NMSettingsRegistry* registry,
                                  QWidget* parent = nullptr);

  /**
   * @brief Rebuild the page with current settings
   */
  void rebuild();

  /**
   * @brief Apply current values to registry
   */
  void applyValues();

  /**
   * @brief Revert to original values
   */
  void revertValues();

  /**
   * @brief Reset to defaults
   */
  void resetToDefaults();

  /**
   * @brief Get category name
   */
  const std::string& getCategory() const { return m_category; }

signals:
  void settingChanged();

private:
  NMSettingWidget* createWidgetForSetting(const SettingDefinition& def);

  std::string m_category;
  NMSettingsRegistry* m_registry = nullptr;
  QVBoxLayout* m_layout = nullptr;
  std::vector<NMSettingWidget*> m_widgets;
};

// ============================================================================
// Main Settings Dialog
// ============================================================================

/**
 * @brief Main settings dialog window
 */
class NMSettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit NMSettingsDialog(NMSettingsRegistry* registry, QWidget* parent = nullptr);
  ~NMSettingsDialog() override;

  /**
   * @brief Show specific category
   */
  void showCategory(const std::string& category);

  /**
   * @brief Show specific setting (by key)
   */
  void showSetting(const std::string& key);

private slots:
  void onSearchTextChanged(const std::string& text);
  void onCategorySelected(QTreeWidgetItem* item, int column);
  void onApplyClicked();
  void onRevertClicked();
  void onResetClicked();
  void onOkClicked();
  void onCancelClicked();
  void onSettingChanged();

private:
  void setupUI();
  void buildCategoryTree();
  void buildCategoryPages();
  void updateButtonStates();

  QTreeWidgetItem* findOrCreateCategoryItem(const std::string& categoryPath);

  NMSettingsRegistry* m_registry = nullptr;

  // UI Components
  QLineEdit* m_searchEdit = nullptr;
  QTreeWidget* m_categoryTree = nullptr;
  QStackedWidget* m_contentStack = nullptr;

  QPushButton* m_applyButton = nullptr;
  QPushButton* m_revertButton = nullptr;
  QPushButton* m_resetButton = nullptr;
  QPushButton* m_okButton = nullptr;
  QPushButton* m_cancelButton = nullptr;

  // Category pages (category name -> page widget)
  QHash<QString, NMSettingsCategoryPage*> m_pages;

  // Tree items (category path -> tree item)
  QHash<QString, QTreeWidgetItem*> m_treeItems;

  bool m_hasUnsavedChanges = false;
};

} // namespace NovelMind::editor
