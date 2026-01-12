#pragma once

/**
 * @file nm_scene_id_picker.hpp
 * @brief Scene ID Picker widget for Inspector panel
 *
 * Provides a dedicated widget for selecting Scene IDs with:
 * - Dropdown list of available scenes from SceneRegistry
 * - Scene thumbnail preview
 * - Validation state indicator
 * - Quick action buttons (Create New, Edit Scene, Locate)
 *
 * Addresses issue #212: Add Scene ID Picker to Inspector Panel
 */

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace NovelMind::editor {
class SceneRegistry;
}

namespace NovelMind::editor::qt {

/**
 * @brief Widget for selecting and managing Scene IDs in the Inspector
 *
 * This widget integrates with SceneRegistry to provide:
 * - Dropdown showing all registered scenes
 * - Thumbnail preview of selected scene
 * - Validation indicator for invalid scene references
 * - Quick actions for scene management
 */
class NMSceneIdPicker : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a Scene ID picker
   * @param registry Pointer to the scene registry (required)
   * @param parent Parent widget
   */
  explicit NMSceneIdPicker(SceneRegistry* registry, QWidget* parent = nullptr);

  /**
   * @brief Set the current scene ID
   * @param sceneId Scene ID to select
   */
  void setSceneId(const QString& sceneId);

  /**
   * @brief Get the currently selected scene ID
   * @return Selected scene ID (empty if none)
   */
  [[nodiscard]] QString sceneId() const;

  /**
   * @brief Refresh the scene list from registry
   */
  void refreshSceneList();

  /**
   * @brief Set whether the picker is read-only
   * @param readOnly true to disable editing
   */
  void setReadOnly(bool readOnly);

signals:
  /**
   * @brief Emitted when the scene ID changes
   * @param sceneId New scene ID
   */
  void sceneIdChanged(const QString& sceneId);

  /**
   * @brief Emitted when "Create New Scene" is clicked
   */
  void createNewSceneRequested();

  /**
   * @brief Emitted when "Edit Scene" is clicked
   * @param sceneId Scene ID to edit
   */
  void editSceneRequested(const QString& sceneId);

  /**
   * @brief Emitted when "Show in Story Graph" is clicked
   * @param sceneId Scene ID to locate
   */
  void locateSceneRequested(const QString& sceneId);

private slots:
  void onSceneSelectionChanged(int index);
  void onCreateNewClicked();
  void onEditSceneClicked();
  void onLocateClicked();
  void onSceneRegistryChanged();

private:
  void setupUI();
  void updateThumbnail();
  void updateValidationState();
  QString getDisplayName(const QString& sceneId) const;

  SceneRegistry* m_registry;
  QString m_currentSceneId;

  // UI Components
  QComboBox* m_sceneCombo;
  QLabel* m_thumbnailLabel;
  QLabel* m_sceneInfoLabel;
  QLabel* m_validationIcon;
  QPushButton* m_createButton;
  QPushButton* m_editButton;
  QPushButton* m_locateButton;

  // Layout
  QVBoxLayout* m_mainLayout;
  QHBoxLayout* m_comboLayout;
  QHBoxLayout* m_previewLayout;
  QHBoxLayout* m_actionsLayout;

  // State
  bool m_readOnly = false;
  bool m_updating = false;
};

} // namespace NovelMind::editor::qt
