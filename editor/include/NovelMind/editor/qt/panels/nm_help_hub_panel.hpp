#pragma once

/**
 * @file nm_help_hub_panel.hpp
 * @brief Help Hub Panel - browse and launch tutorials
 *
 * The Help Hub provides a central location for users to:
 * - Browse all available tutorials by category
 * - See their progress on each tutorial
 * - Manually start any tutorial
 * - Reset tutorial progress
 * - Disable specific tutorials
 *
 * This gives users full control over the guided learning experience
 * without feeling forced through tutorials.
 */

#include "NovelMind/editor/guided_learning/tutorial_types.hpp"
#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <memory>

namespace NovelMind::editor::qt {

/**
 * @brief Help Hub Panel
 *
 * Dockable panel that provides access to all tutorials and hints.
 */
class NMHelpHubPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMHelpHubPanel(QWidget* parent = nullptr);
  ~NMHelpHubPanel() override;

  // NMDockPanel overrides
  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

public slots:
  /**
   * @brief Refresh the tutorial list
   */
  void refreshTutorialList();

  /**
   * @brief Filter tutorials by search text
   */
  void filterTutorials(const QString& searchText);

  /**
   * @brief Filter tutorials by level
   */
  void filterByLevel(int levelIndex);

private slots:
  void onTutorialItemClicked(QTreeWidgetItem* item, int column);
  void onTutorialItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onStartButtonClicked();
  void onResetProgressButtonClicked();
  void onDisableButtonClicked();
  void onResetAllProgressClicked();
  void onSettingsButtonClicked();

private:
  void setupUi();
  void createConnections();
  void updateTutorialDetails();
  void populateTutorialTree();

  QString getLevelDisplayName(guided_learning::TutorialLevel level) const;
  QString getStateDisplayName(guided_learning::TutorialState state) const;
  QColor getStateColor(guided_learning::TutorialState state) const;

  // UI Components
  QWidget* m_contentWidget = nullptr;

  // Search and filter
  QLineEdit* m_searchEdit = nullptr;
  QComboBox* m_levelFilter = nullptr;

  // Tutorial tree (categories -> tutorials)
  QTreeWidget* m_tutorialTree = nullptr;

  // Details panel
  QWidget* m_detailsPanel = nullptr;
  QLabel* m_titleLabel = nullptr;
  QLabel* m_descriptionLabel = nullptr;
  QLabel* m_levelLabel = nullptr;
  QLabel* m_durationLabel = nullptr;
  QLabel* m_progressLabel = nullptr;

  // Action buttons
  QPushButton* m_startButton = nullptr;
  QPushButton* m_resetProgressButton = nullptr;
  QPushButton* m_disableButton = nullptr;

  // Footer
  QPushButton* m_resetAllButton = nullptr;
  QPushButton* m_settingsButton = nullptr;

  // State
  QString m_selectedTutorialId;
};

} // namespace NovelMind::editor::qt
