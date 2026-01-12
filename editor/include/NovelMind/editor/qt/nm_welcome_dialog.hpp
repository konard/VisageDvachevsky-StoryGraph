#pragma once

/**
 * @file nm_welcome_dialog.hpp
 * @brief Welcome screen for the NovelMind Editor
 *
 * Provides a modern, Unreal-Engine-like welcome screen with:
 * - Recent projects (with thumbnails)
 * - Quick actions (New Project, Open Project, Browse Examples)
 * - Project templates
 * - Learning resources
 * - News/updates
 */

#include <QDialog>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QString>
#include <QVector>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QScrollArea;
class QLineEdit;
class QParallelAnimationGroup;

namespace NovelMind::editor::qt {

/**
 * @brief Recent project information
 */
struct RecentProject {
  QString name;
  QString path;
  QString lastOpened;
  QString thumbnail; // Path to project thumbnail
};

/**
 * @brief Project template information
 */
struct ProjectTemplate {
  QString name;
  QString description;
  QString icon;
  QString category; // "Blank", "Visual Novel", "Dating Sim", etc.
};

/**
 * @brief Welcome dialog shown on startup
 *
 * This dialog provides a central hub for:
 * - Quickly accessing recent projects
 * - Creating new projects from templates
 * - Opening existing projects
 * - Accessing learning resources
 */
class NMWelcomeDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct the welcome dialog
   * @param parent Parent widget
   */
  explicit NMWelcomeDialog(QWidget* parent = nullptr);

  /**
   * @brief Destructor
   */
  ~NMWelcomeDialog() override;

  /**
   * @brief Get the path of the project to open
   * @return Empty string if no project selected, otherwise project path
   */
  [[nodiscard]] QString selectedProjectPath() const { return m_selectedProjectPath; }

  /**
   * @brief Get the template for new project creation
   * @return Empty string if no template, otherwise template name
   */
  [[nodiscard]] QString selectedTemplate() const { return m_selectedTemplate; }

  /**
   * @brief Check if user wants to create a new project
   */
  [[nodiscard]] bool shouldCreateNewProject() const { return m_createNewProject; }

  /**
   * @brief Check if user wants to skip the welcome screen in future
   */
  [[nodiscard]] bool shouldSkipInFuture() const { return m_skipInFuture; }

signals:
  /**
   * @brief Emitted when user requests to create a new project
   * @param templateName Name of the template to use
   */
  void newProjectRequested(const QString& templateName);

  /**
   * @brief Emitted when user requests to open an existing project
   * @param projectPath Path to the project file
   */
  void openProjectRequested(const QString& projectPath);

  /**
   * @brief Emitted when user clicks on a learning resource
   * @param url URL to open
   */
  void learningResourceClicked(const QString& url);

public slots:
  /**
   * @brief Refresh the recent projects list
   */
  void refreshRecentProjects();

private slots:
  void onNewProjectClicked();
  void onOpenProjectClicked();
  void onRecentProjectClicked(QListWidgetItem* item);
  void onTemplateClicked(int templateIndex);
  void onBrowseExamplesClicked();
  void onSearchTextChanged(const QString& text);

protected:
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void setupUI();
  void setupLeftPanel();
  void setupCenterPanel();
  void setupRightPanel();
  void loadRecentProjects();
  void loadTemplates();
  void styleDialog();
  void setupAnimations();
  void startEntranceAnimations();
  void animateButtonHover(QWidget* button, bool entering);

  QWidget* createProjectCard(const RecentProject& project);
  QWidget* createTemplateCard(const ProjectTemplate& tmpl, int index);
  QWidget* createQuickActionButton(const QString& icon, const QString& text,
                                   const QString& description);

  // UI Components
  QLineEdit* m_searchBox = nullptr;
  QWidget* m_leftPanel = nullptr;
  QWidget* m_centerPanel = nullptr;
  QWidget* m_rightPanel = nullptr;

  // Left panel - Quick actions and recent projects
  QVBoxLayout* m_leftLayout = nullptr;
  QPushButton* m_btnNewProject = nullptr;
  QPushButton* m_btnOpenProject = nullptr;
  QPushButton* m_btnBrowseExamples = nullptr;
  QListWidget* m_recentProjectsList = nullptr;

  // Center panel - Templates
  QScrollArea* m_templatesScrollArea = nullptr;
  QWidget* m_templatesContainer = nullptr;
  QGridLayout* m_templatesLayout = nullptr;

  // Right panel - Learning resources and news
  QScrollArea* m_resourcesScrollArea = nullptr;
  QWidget* m_resourcesContainer = nullptr;

  // Footer
  QWidget* m_footer = nullptr;
  QPushButton* m_btnSkipInFuture = nullptr;
  QPushButton* m_btnClose = nullptr;

  // State
  QString m_selectedProjectPath;
  QString m_selectedTemplate;
  bool m_createNewProject = false;
  bool m_skipInFuture = false;

  QVector<RecentProject> m_recentProjects;
  QVector<ProjectTemplate> m_templates;

  // Animations
  QParallelAnimationGroup* m_entranceAnimGroup = nullptr;
  bool m_animationsPlayed = false;

  // Constants
  static constexpr int CARD_WIDTH = 280;
  static constexpr int CARD_HEIGHT = 180;
  static constexpr int MAX_RECENT_PROJECTS = 10;
};

} // namespace NovelMind::editor::qt
