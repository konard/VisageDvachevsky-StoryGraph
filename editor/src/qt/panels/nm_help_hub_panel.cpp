/**
 * @file nm_help_hub_panel.cpp
 * @brief Implementation of the Help Hub Panel
 */

#include "NovelMind/editor/qt/panels/nm_help_hub_panel.hpp"
#include "NovelMind/editor/guided_learning/tutorial_manager.hpp"
#include "NovelMind/editor/guided_learning/tutorial_subsystem.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

using namespace guided_learning;

NMHelpHubPanel::NMHelpHubPanel(QWidget *parent)
    : NMDockPanel(tr("Help Hub"), parent) {
  setPanelId("help_hub");
  setupUi();
  createConnections();
}

NMHelpHubPanel::~NMHelpHubPanel() = default;

void NMHelpHubPanel::onInitialize() { refreshTutorialList(); }

void NMHelpHubPanel::onShutdown() {
  // Cleanup if needed
}

void NMHelpHubPanel::onUpdate(double deltaTime) {
  Q_UNUSED(deltaTime);
  // Could refresh progress indicators here
}

void NMHelpHubPanel::setupUi() {
  m_contentWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // Header: Search and Filter
  auto *headerLayout = new QHBoxLayout();

  m_searchEdit = new QLineEdit();
  m_searchEdit->setPlaceholderText(tr("Search tutorials..."));
  m_searchEdit->setClearButtonEnabled(true);
  headerLayout->addWidget(m_searchEdit, 1);

  m_levelFilter = new QComboBox();
  m_levelFilter->addItem(tr("All Levels"));
  m_levelFilter->addItem(tr("Beginner"));
  m_levelFilter->addItem(tr("Intermediate"));
  m_levelFilter->addItem(tr("Advanced"));
  headerLayout->addWidget(m_levelFilter);

  mainLayout->addLayout(headerLayout);

  // Splitter: Tree and Details
  auto *splitter = new QSplitter(Qt::Horizontal);

  // Tutorial Tree
  m_tutorialTree = new QTreeWidget();
  m_tutorialTree->setHeaderLabels({tr("Tutorial"), tr("Status")});
  m_tutorialTree->setColumnCount(2);
  m_tutorialTree->header()->setStretchLastSection(false);
  m_tutorialTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tutorialTree->header()->setSectionResizeMode(1,
                                                 QHeaderView::ResizeToContents);
  m_tutorialTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tutorialTree->setRootIsDecorated(true);
  splitter->addWidget(m_tutorialTree);

  // Details Panel
  m_detailsPanel = new QWidget();
  auto *detailsLayout = new QVBoxLayout(m_detailsPanel);
  detailsLayout->setContentsMargins(12, 12, 12, 12);
  detailsLayout->setSpacing(8);

  m_titleLabel = new QLabel();
  m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
  m_titleLabel->setWordWrap(true);
  detailsLayout->addWidget(m_titleLabel);

  m_descriptionLabel = new QLabel();
  m_descriptionLabel->setWordWrap(true);
  m_descriptionLabel->setStyleSheet("color: #888;");
  detailsLayout->addWidget(m_descriptionLabel);

  auto *infoLayout = new QHBoxLayout();

  m_levelLabel = new QLabel();
  infoLayout->addWidget(m_levelLabel);

  m_durationLabel = new QLabel();
  infoLayout->addWidget(m_durationLabel);

  infoLayout->addStretch();
  detailsLayout->addLayout(infoLayout);

  m_progressLabel = new QLabel();
  m_progressLabel->setStyleSheet("color: #0a84ff;");
  detailsLayout->addWidget(m_progressLabel);

  detailsLayout->addStretch();

  // Action buttons
  auto *buttonLayout = new QHBoxLayout();

  auto &iconMgr = NMIconManager::instance();

  m_startButton = new QPushButton(tr("Start Tutorial"));
  m_startButton->setIcon(iconMgr.getIcon("play", 16));
  m_startButton->setEnabled(false);
  m_startButton->setStyleSheet("QPushButton { background-color: #0a84ff; "
                               "color: white; padding: 6px 16px; }");
  buttonLayout->addWidget(m_startButton);

  m_resetProgressButton = new QPushButton(tr("Reset Progress"));
  m_resetProgressButton->setIcon(iconMgr.getIcon("property-reset", 16));
  m_resetProgressButton->setEnabled(false);
  buttonLayout->addWidget(m_resetProgressButton);

  m_disableButton = new QPushButton(tr("Disable"));
  m_disableButton->setIcon(iconMgr.getIcon("file-close", 16));
  m_disableButton->setEnabled(false);
  buttonLayout->addWidget(m_disableButton);

  buttonLayout->addStretch();
  detailsLayout->addLayout(buttonLayout);

  splitter->addWidget(m_detailsPanel);
  splitter->setSizes({300, 200});

  mainLayout->addWidget(splitter, 1);

  // Footer
  auto *footerLayout = new QHBoxLayout();

  m_resetAllButton = new QPushButton(tr("Reset All Progress"));
  m_resetAllButton->setIcon(iconMgr.getIcon("refresh", 16));
  footerLayout->addWidget(m_resetAllButton);

  footerLayout->addStretch();

  m_settingsButton = new QPushButton(tr("Settings"));
  m_settingsButton->setIcon(iconMgr.getIcon("settings", 16));
  footerLayout->addWidget(m_settingsButton);

  mainLayout->addLayout(footerLayout);

  setContentWidget(m_contentWidget);
}

void NMHelpHubPanel::createConnections() {
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &NMHelpHubPanel::filterTutorials);

  connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMHelpHubPanel::filterByLevel);

  connect(m_tutorialTree, &QTreeWidget::itemClicked, this,
          &NMHelpHubPanel::onTutorialItemClicked);

  connect(m_tutorialTree, &QTreeWidget::itemDoubleClicked, this,
          &NMHelpHubPanel::onTutorialItemDoubleClicked);

  connect(m_startButton, &QPushButton::clicked, this,
          &NMHelpHubPanel::onStartButtonClicked);

  connect(m_resetProgressButton, &QPushButton::clicked, this,
          &NMHelpHubPanel::onResetProgressButtonClicked);

  connect(m_disableButton, &QPushButton::clicked, this,
          &NMHelpHubPanel::onDisableButtonClicked);

  connect(m_resetAllButton, &QPushButton::clicked, this,
          &NMHelpHubPanel::onResetAllProgressClicked);

  connect(m_settingsButton, &QPushButton::clicked, this,
          &NMHelpHubPanel::onSettingsButtonClicked);

  // Connect to tutorial manager for updates
  if (NMTutorialSubsystem::hasInstance()) {
    connect(&NMTutorialManager::instance(), &NMTutorialManager::progressChanged,
            this, &NMHelpHubPanel::refreshTutorialList);
  }
}

void NMHelpHubPanel::refreshTutorialList() {
  populateTutorialTree();
  updateTutorialDetails();
}

void NMHelpHubPanel::filterTutorials(const QString &searchText) {
  populateTutorialTree();

  if (searchText.isEmpty())
    return;

  // Hide items that don't match search
  for (int i = 0; i < m_tutorialTree->topLevelItemCount(); ++i) {
    auto *categoryItem = m_tutorialTree->topLevelItem(i);
    bool anyChildVisible = false;

    for (int j = 0; j < categoryItem->childCount(); ++j) {
      auto *tutorialItem = categoryItem->child(j);
      bool matches =
          tutorialItem->text(0).contains(searchText, Qt::CaseInsensitive);
      tutorialItem->setHidden(!matches);
      if (matches)
        anyChildVisible = true;
    }

    categoryItem->setHidden(!anyChildVisible);
  }
}

void NMHelpHubPanel::filterByLevel(int levelIndex) {
  populateTutorialTree();

  if (levelIndex == 0)
    return; // "All Levels"

  TutorialLevel targetLevel = static_cast<TutorialLevel>(levelIndex - 1);

  // Hide items that don't match level
  for (int i = 0; i < m_tutorialTree->topLevelItemCount(); ++i) {
    auto *categoryItem = m_tutorialTree->topLevelItem(i);
    bool anyChildVisible = false;

    for (int j = 0; j < categoryItem->childCount(); ++j) {
      auto *tutorialItem = categoryItem->child(j);
      QString tutorialId = tutorialItem->data(0, Qt::UserRole).toString();

      auto tutorial =
          NMTutorialManager::instance().getTutorial(tutorialId.toStdString());
      if (tutorial) {
        bool matches = tutorial->level == targetLevel;
        tutorialItem->setHidden(!matches);
        if (matches)
          anyChildVisible = true;
      }
    }

    categoryItem->setHidden(!anyChildVisible);
  }
}

void NMHelpHubPanel::populateTutorialTree() {
  m_tutorialTree->clear();

  if (!NMTutorialSubsystem::hasInstance())
    return;

  auto tutorials = NMTutorialManager::instance().getAllTutorials();

  // Group by category
  std::map<std::string, std::vector<TutorialDefinition>> byCategory;
  for (const auto &tutorial : tutorials) {
    std::string category =
        tutorial.category.empty() ? "General" : tutorial.category;
    byCategory[category].push_back(tutorial);
  }

  for (const auto &[category, categoryTutorials] : byCategory) {
    auto *categoryItem = new QTreeWidgetItem(m_tutorialTree);
    categoryItem->setText(0, QString::fromStdString(category));
    categoryItem->setExpanded(true);

    for (const auto &tutorial : categoryTutorials) {
      auto *tutorialItem = new QTreeWidgetItem(categoryItem);
      tutorialItem->setText(0, QString::fromStdString(tutorial.title));
      tutorialItem->setData(0, Qt::UserRole,
                            QString::fromStdString(tutorial.id));

      // Get progress
      auto progress =
          NMTutorialManager::instance().getTutorialProgress(tutorial.id);
      QString statusText = getStateDisplayName(progress.state);
      QColor statusColor = getStateColor(progress.state);

      tutorialItem->setText(1, statusText);
      tutorialItem->setForeground(1, statusColor);

      // Set icon based on level
      QString levelStr = getLevelDisplayName(tutorial.level);
      tutorialItem->setToolTip(0, levelStr);
    }
  }
}

void NMHelpHubPanel::onTutorialItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);

  // Only handle tutorial items (not categories)
  if (!item->parent())
    return;

  m_selectedTutorialId = item->data(0, Qt::UserRole).toString();
  updateTutorialDetails();
}

void NMHelpHubPanel::onTutorialItemDoubleClicked(QTreeWidgetItem *item,
                                                 int column) {
  Q_UNUSED(column);

  // Only handle tutorial items (not categories)
  if (!item->parent())
    return;

  m_selectedTutorialId = item->data(0, Qt::UserRole).toString();
  onStartButtonClicked();
}

void NMHelpHubPanel::updateTutorialDetails() {
  bool hasSelection = !m_selectedTutorialId.isEmpty();

  m_startButton->setEnabled(hasSelection);
  m_resetProgressButton->setEnabled(hasSelection);
  m_disableButton->setEnabled(hasSelection);

  if (!hasSelection) {
    m_titleLabel->setText(tr("Select a tutorial"));
    m_descriptionLabel->setText(
        tr("Choose a tutorial from the list to see details."));
    m_levelLabel->clear();
    m_durationLabel->clear();
    m_progressLabel->clear();
    return;
  }

  if (!NMTutorialSubsystem::hasInstance())
    return;

  auto tutorial = NMTutorialManager::instance().getTutorial(
      m_selectedTutorialId.toStdString());
  if (!tutorial) {
    m_selectedTutorialId.clear();
    updateTutorialDetails();
    return;
  }

  m_titleLabel->setText(QString::fromStdString(tutorial->title));
  m_descriptionLabel->setText(QString::fromStdString(tutorial->description));

  QString levelStr = getLevelDisplayName(tutorial->level);
  m_levelLabel->setText(QString("Level: %1").arg(levelStr));

  m_durationLabel->setText(QString("~%1 min").arg(tutorial->estimatedMinutes));

  auto progress =
      NMTutorialManager::instance().getTutorialProgress(tutorial->id);

  QString progressText;
  switch (progress.state) {
  case TutorialState::NotStarted:
    progressText = tr("Not started");
    m_startButton->setText(tr("Start Tutorial"));
    m_disableButton->setText(tr("Disable"));
    break;
  case TutorialState::InProgress:
    progressText = QString(tr("In progress: Step %1 of %2"))
                       .arg(progress.currentStepIndex + 1)
                       .arg(tutorial->steps.size());
    m_startButton->setText(tr("Continue"));
    m_disableButton->setText(tr("Disable"));
    break;
  case TutorialState::Completed:
    progressText = tr("Completed");
    m_startButton->setText(tr("Restart"));
    m_disableButton->setText(tr("Disable"));
    break;
  case TutorialState::Disabled:
    progressText = tr("Disabled");
    m_startButton->setText(tr("Start Tutorial"));
    m_startButton->setEnabled(false);
    m_disableButton->setText(tr("Enable"));
    break;
  }
  m_progressLabel->setText(progressText);

  // Check prerequisites
  if (!NMTutorialManager::instance().arePrerequisitesMet(tutorial->id)) {
    m_startButton->setEnabled(false);
    m_progressLabel->setText(m_progressLabel->text() +
                             tr("\n(Prerequisites not met)"));
  }
}

void NMHelpHubPanel::onStartButtonClicked() {
  if (m_selectedTutorialId.isEmpty())
    return;

  if (!NMTutorialSubsystem::hasInstance())
    return;

  // Check if disabled, enable first
  auto progress = NMTutorialManager::instance().getTutorialProgress(
      m_selectedTutorialId.toStdString());
  if (progress.state == TutorialState::Disabled) {
    NMTutorialManager::instance().enableTutorial(
        m_selectedTutorialId.toStdString());
  }

  // If completed, reset first
  if (progress.state == TutorialState::Completed) {
    NMTutorialManager::instance().resetTutorialProgress(
        m_selectedTutorialId.toStdString());
  }

  // Start the tutorial
  bool started = NMTutorialSubsystem::instance().startTutorial(
      m_selectedTutorialId.toStdString());

  if (started) {
    // Hide this panel to show the tutorial
    hide();
  }
}

void NMHelpHubPanel::onResetProgressButtonClicked() {
  if (m_selectedTutorialId.isEmpty())
    return;

  auto reply = NMMessageDialog::showQuestion(
      this, tr("Reset Progress"),
      tr("Are you sure you want to reset progress for this tutorial?"),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (reply == NMDialogButton::Yes) {
    NMTutorialManager::instance().resetTutorialProgress(
        m_selectedTutorialId.toStdString());
    updateTutorialDetails();
  }
}

void NMHelpHubPanel::onDisableButtonClicked() {
  if (m_selectedTutorialId.isEmpty())
    return;

  auto progress = NMTutorialManager::instance().getTutorialProgress(
      m_selectedTutorialId.toStdString());

  if (progress.state == TutorialState::Disabled) {
    NMTutorialManager::instance().enableTutorial(
        m_selectedTutorialId.toStdString());
  } else {
    NMTutorialManager::instance().disableTutorial(
        m_selectedTutorialId.toStdString());
  }

  updateTutorialDetails();
}

void NMHelpHubPanel::onResetAllProgressClicked() {
  auto reply = NMMessageDialog::showQuestion(
      this, tr("Reset All Progress"),
      tr("Are you sure you want to reset progress for ALL tutorials?\n"
         "This cannot be undone."),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (reply == NMDialogButton::Yes) {
    NMTutorialManager::instance().resetAllProgress();
    refreshTutorialList();
  }
}

void NMHelpHubPanel::onSettingsButtonClicked() {
  // Could open settings dialog or navigate to Editor Preferences
  // For now, just show a simple toggle
  bool currentlyEnabled = NMTutorialManager::instance().isEnabled();

  auto reply = NMMessageDialog::showQuestion(
      this, tr("Guided Learning Settings"),
      currentlyEnabled ? tr("Guided learning is currently enabled.\nWould you "
                            "like to disable it?")
                       : tr("Guided learning is currently disabled.\nWould you "
                            "like to enable it?"),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (reply == NMDialogButton::Yes) {
    NMTutorialSubsystem::instance().setEnabled(!currentlyEnabled);
  }
}

QString NMHelpHubPanel::getLevelDisplayName(TutorialLevel level) const {
  switch (level) {
  case TutorialLevel::Beginner:
    return tr("Beginner");
  case TutorialLevel::Intermediate:
    return tr("Intermediate");
  case TutorialLevel::Advanced:
    return tr("Advanced");
  }
  return tr("Unknown");
}

QString NMHelpHubPanel::getStateDisplayName(TutorialState state) const {
  switch (state) {
  case TutorialState::NotStarted:
    return tr("Not Started");
  case TutorialState::InProgress:
    return tr("In Progress");
  case TutorialState::Completed:
    return tr("Completed");
  case TutorialState::Disabled:
    return tr("Disabled");
  }
  return tr("Unknown");
}

QColor NMHelpHubPanel::getStateColor(TutorialState state) const {
  switch (state) {
  case TutorialState::NotStarted:
    return QColor(150, 150, 150);
  case TutorialState::InProgress:
    return QColor(255, 165, 0); // Orange
  case TutorialState::Completed:
    return QColor(50, 205, 50); // Green
  case TutorialState::Disabled:
    return QColor(128, 128, 128);
  }
  return QColor(150, 150, 150);
}

} // namespace NovelMind::editor::qt
