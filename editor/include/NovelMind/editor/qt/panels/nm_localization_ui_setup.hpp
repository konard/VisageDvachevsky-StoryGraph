#pragma once

/**
 * @file nm_localization_ui_setup.hpp
 * @brief UI setup helper for localization panel
 *
 * Handles the initial setup of all UI widgets and layout for the localization panel.
 */

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace NovelMind::editor::qt {

class NMLocalizationPanel;

/**
 * @brief UI setup helper for localization panel
 *
 * This class encapsulates all the UI widget creation and layout setup
 * to keep the main panel class focused on logic and coordination.
 */
class NMLocalizationUISetup {
public:
  /**
   * @brief Setup all UI widgets and layout
   * @param panel The localization panel to setup UI for
   */
  static void setupUI(NMLocalizationPanel* panel);

private:
  NMLocalizationUISetup() = default;
};

} // namespace NovelMind::editor::qt
