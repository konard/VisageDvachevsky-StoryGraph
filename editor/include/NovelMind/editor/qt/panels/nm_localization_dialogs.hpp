#pragma once

/**
 * @file nm_localization_dialogs.hpp
 * @brief Dialog helper for localization panel
 *
 * Handles dialog UI for adding keys and editing plural forms.
 */

#include <QString>

namespace NovelMind::editor::qt {

class NMLocalizationPanel;
class NMLocalizationDataModel;
class NMLocalizationSearch;

/**
 * @brief Dialog helper for localization panel
 *
 * This class encapsulates dialog interactions to keep the main panel focused.
 */
class NMLocalizationDialogs {
public:
  /**
   * @brief Show dialog for adding a new localization key
   * @param panel The parent localization panel
   * @param searchHelper Reference to search helper for validation
   * @param outKey Output parameter for the entered key
   * @param outDefaultValue Output parameter for the entered default value
   * @return true if user clicked OK and entered valid data
   */
  static bool showAddKeyDialog(NMLocalizationPanel* panel, NMLocalizationSearch& searchHelper,
                                QString& outKey, QString& outDefaultValue);

  /**
   * @brief Show dialog for editing plural forms
   * @param panel The parent localization panel
   * @param dataModel Reference to data model
   * @param key The localization key to edit plural forms for
   * @return true if user clicked OK
   */
  static bool showPluralFormsDialog(NMLocalizationPanel* panel, NMLocalizationDataModel& dataModel,
                                     const QString& key);

private:
  NMLocalizationDialogs() = default;
};

} // namespace NovelMind::editor::qt
