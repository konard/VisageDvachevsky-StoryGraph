#include "NovelMind/editor/qt/nm_welcome_dialog.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSequentialAnimationGroup>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMWelcomeDialog::NMWelcomeDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Welcome to NovelMind Editor");
  setMinimumSize(1200, 700);
  resize(1400, 800);

  setupUI();
  loadRecentProjects();
  loadTemplates();
  styleDialog();
}

NMWelcomeDialog::~NMWelcomeDialog() = default;

void NMWelcomeDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header
  QWidget *header = new QWidget(this);
  header->setObjectName("WelcomeHeader");
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(24, 16, 24, 16);

  QLabel *titleLabel = new QLabel("NovelMind Editor", header);
  titleLabel->setObjectName("WelcomeTitle");
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(18);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);

  QLabel *versionLabel = new QLabel("v0.3.0", header);
  versionLabel->setObjectName("WelcomeVersion");

  // Search box
  m_searchBox = new QLineEdit(header);
  m_searchBox->setPlaceholderText("Search projects and templates...");
  m_searchBox->setMinimumWidth(300);
  connect(m_searchBox, &QLineEdit::textChanged, this,
          &NMWelcomeDialog::onSearchTextChanged);

  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(versionLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(m_searchBox);

  mainLayout->addWidget(header);

  // Content area - three columns
  QWidget *content = new QWidget(this);
  QHBoxLayout *contentLayout = new QHBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  setupLeftPanel();
  setupCenterPanel();
  setupRightPanel();

  contentLayout->addWidget(m_leftPanel, 1);
  contentLayout->addWidget(m_centerPanel, 2);
  contentLayout->addWidget(m_rightPanel, 1);

  mainLayout->addWidget(content, 1);

  // Footer
  m_footer = new QWidget(this);
  m_footer->setObjectName("WelcomeFooter");
  QHBoxLayout *footerLayout = new QHBoxLayout(m_footer);
  footerLayout->setContentsMargins(24, 12, 24, 12);

  QCheckBox *skipCheckbox = new QCheckBox("Don't show this again", m_footer);
  connect(skipCheckbox, &QCheckBox::toggled,
          [this](bool checked) { m_skipInFuture = checked; });

  m_btnClose = new QPushButton("Close", m_footer);
  m_btnClose->setIcon(NMIconManager::instance().getIcon("file-close", 16));
  m_btnClose->setMinimumWidth(100);
  connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);

  footerLayout->addWidget(skipCheckbox);
  footerLayout->addStretch();
  footerLayout->addWidget(m_btnClose);

  mainLayout->addWidget(m_footer);
}

void NMWelcomeDialog::setupLeftPanel() {
  m_leftPanel = new QWidget(this);
  m_leftPanel->setObjectName("WelcomeLeftPanel");
  m_leftLayout = new QVBoxLayout(m_leftPanel);
  m_leftLayout->setContentsMargins(24, 24, 12, 24);
  m_leftLayout->setSpacing(12);

  // Section: Quick Actions
  QLabel *quickActionsLabel = new QLabel("Quick Actions", m_leftPanel);
  quickActionsLabel->setObjectName("SectionTitle");
  QFont sectionFont = quickActionsLabel->font();
  sectionFont.setPointSize(12);
  sectionFont.setBold(true);
  quickActionsLabel->setFont(sectionFont);
  m_leftLayout->addWidget(quickActionsLabel);

  auto& iconMgr = NMIconManager::instance();

  // New Project button
  m_btnNewProject = new QPushButton("New Project", m_leftPanel);
  m_btnNewProject->setIcon(iconMgr.getIcon("welcome-new", 16));
  m_btnNewProject->setObjectName("PrimaryActionButton");
  m_btnNewProject->setMinimumHeight(48);
  connect(m_btnNewProject, &QPushButton::clicked, this,
          &NMWelcomeDialog::onNewProjectClicked);
  m_leftLayout->addWidget(m_btnNewProject);

  // Open Project button
  m_btnOpenProject = new QPushButton("Open Project", m_leftPanel);
  m_btnOpenProject->setIcon(iconMgr.getIcon("welcome-open", 16));
  m_btnOpenProject->setObjectName("SecondaryActionButton");
  m_btnOpenProject->setMinimumHeight(48);
  connect(m_btnOpenProject, &QPushButton::clicked, this,
          &NMWelcomeDialog::onOpenProjectClicked);
  m_leftLayout->addWidget(m_btnOpenProject);

  // Browse Examples button
  m_btnBrowseExamples = new QPushButton("Browse Examples", m_leftPanel);
  m_btnBrowseExamples->setIcon(iconMgr.getIcon("welcome-examples", 16));
  m_btnBrowseExamples->setObjectName("SecondaryActionButton");
  m_btnBrowseExamples->setMinimumHeight(48);
  connect(m_btnBrowseExamples, &QPushButton::clicked, this,
          &NMWelcomeDialog::onBrowseExamplesClicked);
  m_leftLayout->addWidget(m_btnBrowseExamples);

  m_leftLayout->addSpacing(24);

  // Section: Recent Projects
  QLabel *recentLabel = new QLabel("Recent Projects", m_leftPanel);
  recentLabel->setObjectName("SectionTitle");
  recentLabel->setFont(sectionFont);
  m_leftLayout->addWidget(recentLabel);

  m_recentProjectsList = new QListWidget(m_leftPanel);
  m_recentProjectsList->setObjectName("RecentProjectsList");
  connect(m_recentProjectsList, &QListWidget::itemClicked, this,
          &NMWelcomeDialog::onRecentProjectClicked);
  m_leftLayout->addWidget(m_recentProjectsList, 1);
}

void NMWelcomeDialog::setupCenterPanel() {
  m_centerPanel = new QWidget(this);
  m_centerPanel->setObjectName("WelcomeCenterPanel");
  QVBoxLayout *centerLayout = new QVBoxLayout(m_centerPanel);
  centerLayout->setContentsMargins(12, 24, 12, 24);
  centerLayout->setSpacing(16);

  // Section title
  QLabel *templatesLabel = new QLabel("Project Templates", m_centerPanel);
  templatesLabel->setObjectName("SectionTitle");
  QFont sectionFont = templatesLabel->font();
  sectionFont.setPointSize(12);
  sectionFont.setBold(true);
  templatesLabel->setFont(sectionFont);
  centerLayout->addWidget(templatesLabel);

  // Templates scroll area
  m_templatesScrollArea = new QScrollArea(m_centerPanel);
  m_templatesScrollArea->setObjectName("TemplatesScrollArea");
  m_templatesScrollArea->setWidgetResizable(true);
  m_templatesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_templatesContainer = new QWidget();
  m_templatesLayout = new QGridLayout(m_templatesContainer);
  m_templatesLayout->setSpacing(16);
  m_templatesLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  m_templatesScrollArea->setWidget(m_templatesContainer);
  centerLayout->addWidget(m_templatesScrollArea, 1);
}

void NMWelcomeDialog::setupRightPanel() {
  m_rightPanel = new QWidget(this);
  m_rightPanel->setObjectName("WelcomeRightPanel");
  QVBoxLayout *rightLayout = new QVBoxLayout(m_rightPanel);
  rightLayout->setContentsMargins(12, 24, 24, 24);
  rightLayout->setSpacing(16);

  // Section title
  QLabel *resourcesLabel = new QLabel("Learning Resources", m_rightPanel);
  resourcesLabel->setObjectName("SectionTitle");
  QFont sectionFont = resourcesLabel->font();
  sectionFont.setPointSize(12);
  sectionFont.setBold(true);
  resourcesLabel->setFont(sectionFont);
  rightLayout->addWidget(resourcesLabel);

  // Resources scroll area
  m_resourcesScrollArea = new QScrollArea(m_rightPanel);
  m_resourcesScrollArea->setObjectName("ResourcesScrollArea");
  m_resourcesScrollArea->setWidgetResizable(true);
  m_resourcesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_resourcesContainer = new QWidget();
  QVBoxLayout *resourcesLayout = new QVBoxLayout(m_resourcesContainer);
  resourcesLayout->setSpacing(12);
  resourcesLayout->setAlignment(Qt::AlignTop);

  // Add some learning resources
  struct Resource {
    QString title;
    QString description;
    QString url;
  };

  QVector<Resource> resources = {
      {"Getting Started Guide", "Learn the basics of NovelMind Editor",
       "https://github.com/VisageDvachevsky/NovelMind"},
      {"Tutorial Videos", "Video tutorials for common tasks",
       "https://github.com/VisageDvachevsky/NovelMind/tree/main/examples/"
       "sample_vn"},
      {"API Documentation", "Complete API reference",
       "https://github.com/VisageDvachevsky/NovelMind/tree/main/docs"},
      {"Community Forum", "Ask questions and share projects",
       "https://github.com/VisageDvachevsky/NovelMind/discussions"},
      {"Report Issues", "Found a bug? Let us know!",
       "https://github.com/VisageDvachevsky/NovelMind/issues"}};

  for (const auto &resource : resources) {
    QFrame *resourceCard = new QFrame(m_resourcesContainer);
    resourceCard->setObjectName("ResourceCard");
    resourceCard->setFrameShape(QFrame::StyledPanel);
    resourceCard->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *cardLayout = new QVBoxLayout(resourceCard);
    cardLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *titleLabel = new QLabel(resource.title, resourceCard);
    titleLabel->setObjectName("ResourceTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QLabel *descLabel = new QLabel(resource.description, resourceCard);
    descLabel->setObjectName("ResourceDescription");
    descLabel->setWordWrap(true);

    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(descLabel);

    resourcesLayout->addWidget(resourceCard);

    // Make the card clickable
    resourceCard->installEventFilter(this);
    resourceCard->setProperty("url", resource.url);
  }

  resourcesLayout->addStretch();

  m_resourcesScrollArea->setWidget(m_resourcesContainer);
  rightLayout->addWidget(m_resourcesScrollArea, 1);
}

void NMWelcomeDialog::loadRecentProjects() {
  QSettings settings("NovelMind", "Editor");
  int count = settings.beginReadArray("RecentProjects");

  for (int i = 0; i < count && i < MAX_RECENT_PROJECTS; ++i) {
    settings.setArrayIndex(i);
    RecentProject project;
    project.name = settings.value("name").toString();
    project.path = settings.value("path").toString();
    project.lastOpened = settings.value("lastOpened").toString();
    project.thumbnail = settings.value("thumbnail").toString();

    // Verify the project file still exists
    if (QFileInfo::exists(project.path)) {
      m_recentProjects.append(project);

      // Add to list widget
      QString displayText =
          QString("%1\n%2").arg(project.name, project.lastOpened);
      QListWidgetItem *item =
          new QListWidgetItem(displayText, m_recentProjectsList);
      item->setData(Qt::UserRole, project.path);
    }
  }

  settings.endArray();
}

void NMWelcomeDialog::loadTemplates() {
  // Define available templates
  m_templates = {
      {"Blank Project", "Start with an empty project", "", "Blank"},
      {"Visual Novel", "Traditional visual novel with dialogue and choices", "",
       "Visual Novel"},
      {"Dating Sim", "Dating simulation with relationship mechanics", "",
       "Dating Sim"},
      {"Mystery/Detective", "Investigation-focused story with clues", "",
       "Mystery"},
      {"RPG Story", "Story with stat tracking and combat", "", "RPG"},
      {"Horror", "Atmospheric horror visual novel", "", "Horror"}};

  // Create template cards in a grid
  int col = 0;
  int row = 0;
  const int COLS = 2;

  for (int i = 0; i < m_templates.size(); ++i) {
    QWidget *card = createTemplateCard(m_templates[i], i);
    m_templatesLayout->addWidget(card, row, col);

    col++;
    if (col >= COLS) {
      col = 0;
      row++;
    }
  }
}

QWidget *NMWelcomeDialog::createTemplateCard(const ProjectTemplate &tmpl,
                                             int index) {
  QFrame *card = new QFrame(m_templatesContainer);
  card->setObjectName("TemplateCard");
  card->setFrameShape(QFrame::StyledPanel);
  card->setMinimumSize(CARD_WIDTH, CARD_HEIGHT);
  card->setMaximumSize(CARD_WIDTH, CARD_HEIGHT);
  card->setCursor(Qt::PointingHandCursor);

  QVBoxLayout *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(16, 16, 16, 16);
  cardLayout->setSpacing(8);

  // Icon
  QLabel *iconLabel = new QLabel(card);
  iconLabel->setObjectName("TemplateIcon");
  iconLabel->setMinimumSize(48, 48);
  iconLabel->setMaximumSize(48, 48);
  iconLabel->setAlignment(Qt::AlignCenter);
  auto &iconMgr = NMIconManager::instance();
  iconLabel->setPixmap(iconMgr.getPixmap("file-new", 32));
  QFont iconFont = iconLabel->font();
  iconFont.setPointSize(24);
  iconLabel->setFont(iconFont);

  // Title
  QLabel *titleLabel = new QLabel(tmpl.name, card);
  titleLabel->setObjectName("TemplateTitle");
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(11);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);

  // Description
  QLabel *descLabel = new QLabel(tmpl.description, card);
  descLabel->setObjectName("TemplateDescription");
  descLabel->setWordWrap(true);

  cardLayout->addWidget(iconLabel);
  cardLayout->addWidget(titleLabel);
  cardLayout->addWidget(descLabel);
  cardLayout->addStretch();

  // Store template index for click handling
  card->setProperty("templateIndex", index);
  card->installEventFilter(this);

  return card;
}

void NMWelcomeDialog::refreshRecentProjects() {
  m_recentProjects.clear();
  m_recentProjectsList->clear();
  loadRecentProjects();
}

void NMWelcomeDialog::onNewProjectClicked() {
  // Default to blank template
  m_selectedTemplate = "Blank Project";
  m_createNewProject = true;
  accept();
}

void NMWelcomeDialog::onOpenProjectClicked() {
  QString projectPath = NMFileDialog::getExistingDirectory(
      this, tr("Open NovelMind Project"), QDir::homePath());

  if (!projectPath.isEmpty()) {
    m_selectedProjectPath = projectPath;
    m_createNewProject = false;
    accept();
  }
}

void NMWelcomeDialog::onRecentProjectClicked(QListWidgetItem *item) {
  if (item) {
    m_selectedProjectPath = item->data(Qt::UserRole).toString();
    m_createNewProject = false;
    accept();
  }
}

void NMWelcomeDialog::onTemplateClicked(int templateIndex) {
  if (templateIndex >= 0 && templateIndex < m_templates.size()) {
    m_selectedTemplate = m_templates[templateIndex].name;
    m_createNewProject = true;
    accept();
  }
}

void NMWelcomeDialog::onBrowseExamplesClicked() {
  QDesktopServices::openUrl(
      QUrl("https://github.com/VisageDvachevsky/NovelMind/tree/main/examples"));
}

void NMWelcomeDialog::onSearchTextChanged(const QString &text) {
  // Filter templates and recent projects based on search text
  const QString query = text.trimmed().toLower();

  // Filter templates
  if (m_templatesContainer) {
    const auto cards = m_templatesContainer->findChildren<QWidget *>();
    for (QWidget *card : cards) {
      bool match = query.isEmpty();
      const QVariant indexValue = card->property("templateIndex");
      if (indexValue.isValid()) {
        const int index = indexValue.toInt();
        if (index >= 0 && index < m_templates.size()) {
          const QString name = m_templates[index].name.toLower();
          const QString description = m_templates[index].description.toLower();
          match = query.isEmpty() || name.contains(query) ||
                  description.contains(query);
        }
        card->setVisible(match);
      }
    }
  }

  // Filter recent projects list
  if (m_recentProjectsList) {
    for (int i = 0; i < m_recentProjectsList->count(); ++i) {
      auto *item = m_recentProjectsList->item(i);
      if (!item) {
        continue;
      }
      const QString textValue = item->text().toLower();
      const QString pathValue = item->data(Qt::UserRole).toString().toLower();
      const bool match = query.isEmpty() || textValue.contains(query) ||
                         pathValue.contains(query);
      item->setHidden(!match);
    }
  }
}

void NMWelcomeDialog::styleDialog() {
  // Apply premium dark theme stylesheet for welcome dialog
  // Using NovelMind design system colors for consistency
  setStyleSheet(R"(
        /* ================================================================== */
        /* Welcome Dialog - Premium Dark Theme                                 */
        /* ================================================================== */

        QDialog {
            background-color: #0d1014;
        }

        /* ------------------------------------------------------------------ */
        /* Header Section                                                      */
        /* ------------------------------------------------------------------ */

        #WelcomeHeader {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1c2129, stop:1 #14181e);
            border-bottom: 1px solid #2a323e;
            padding: 16px 24px;
        }

        #WelcomeTitle {
            color: #e8edf3;
            font-size: 20px;
            font-weight: 700;
            letter-spacing: 0.5px;
        }

        #WelcomeVersion {
            color: #6c7684;
            font-size: 11px;
            background-color: #1c2129;
            border: 1px solid #2a323e;
            border-radius: 10px;
            padding: 2px 8px;
            margin-left: 8px;
        }

        /* ------------------------------------------------------------------ */
        /* Panel Sections                                                      */
        /* ------------------------------------------------------------------ */

        #WelcomeLeftPanel {
            background-color: #14181e;
            border-right: 1px solid #2a323e;
        }

        #WelcomeCenterPanel {
            background-color: #0d1014;
        }

        #WelcomeRightPanel {
            background-color: #14181e;
            border-left: 1px solid #2a323e;
        }

        #WelcomeFooter {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #14181e, stop:1 #1c2129);
            border-top: 1px solid #2a323e;
            padding: 12px 24px;
        }

        /* ------------------------------------------------------------------ */
        /* Section Titles                                                      */
        /* ------------------------------------------------------------------ */

        #SectionTitle {
            color: #9aa7b8;
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 12px;
            padding-bottom: 8px;
            border-bottom: 1px solid #2a323e;
        }

        /* ------------------------------------------------------------------ */
        /* Action Buttons                                                      */
        /* ------------------------------------------------------------------ */

        #PrimaryActionButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4aabff, stop:1 #3b9eff);
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 14px 20px;
            font-weight: 600;
            font-size: 12px;
        }

        #PrimaryActionButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #5cb8ff, stop:1 #4aabff);
        }

        #PrimaryActionButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2882e0, stop:1 #3b9eff);
        }

        #SecondaryActionButton {
            background-color: #1c2129;
            color: #e8edf3;
            border: 1px solid #2a323e;
            border-radius: 6px;
            padding: 14px 20px;
            font-size: 11px;
        }

        #SecondaryActionButton:hover {
            background-color: #262d38;
            border-color: #3b9eff;
        }

        #SecondaryActionButton:pressed {
            background-color: #14181e;
        }

        /* ------------------------------------------------------------------ */
        /* Recent Projects List                                                */
        /* ------------------------------------------------------------------ */

        #RecentProjectsList {
            background-color: #1c2129;
            border: 1px solid #2a323e;
            border-radius: 8px;
            color: #e8edf3;
            padding: 4px;
        }

        #RecentProjectsList::item {
            padding: 12px;
            border-radius: 6px;
            margin: 2px;
        }

        #RecentProjectsList::item:hover {
            background-color: #262d38;
        }

        #RecentProjectsList::item:selected {
            background-color: #1a3a5c;
            border-left: 3px solid #3b9eff;
        }

        /* ------------------------------------------------------------------ */
        /* Template & Resource Cards                                           */
        /* ------------------------------------------------------------------ */

        #TemplateCard {
            background-color: #1c2129;
            border: 1px solid #2a323e;
            border-radius: 10px;
        }

        #TemplateCard:hover {
            background-color: #262d38;
            border-color: #3b9eff;
            border-width: 2px;
        }

        #ResourceCard {
            background-color: #1c2129;
            border: 1px solid #2a323e;
            border-radius: 8px;
        }

        #ResourceCard:hover {
            background-color: #262d38;
            border-color: #3b9eff;
        }

        #TemplateIcon {
            background-color: #0d1014;
            border-radius: 8px;
            padding: 8px;
        }

        #TemplateTitle {
            color: #e8edf3;
            font-size: 13px;
            font-weight: 600;
        }

        #TemplateDescription {
            color: #6c7684;
            font-size: 11px;
            line-height: 1.4;
        }

        #ResourceTitle {
            color: #e8edf3;
            font-size: 12px;
            font-weight: 600;
        }

        #ResourceDescription {
            color: #6c7684;
            font-size: 10px;
        }

        /* ------------------------------------------------------------------ */
        /* Search Box                                                          */
        /* ------------------------------------------------------------------ */

        QLineEdit {
            background-color: #1c2129;
            border: 1px solid #2a323e;
            border-radius: 20px;
            padding: 10px 16px;
            color: #e8edf3;
            font-size: 11px;
        }

        QLineEdit:focus {
            border-color: #3b9eff;
            background-color: #262d38;
        }

        QLineEdit::placeholder {
            color: #4a525e;
        }

        /* ------------------------------------------------------------------ */
        /* Scroll Areas                                                        */
        /* ------------------------------------------------------------------ */

        QScrollArea {
            border: none;
            background-color: transparent;
        }

        QScrollBar:vertical {
            background-color: #14181e;
            width: 8px;
            border-radius: 4px;
        }

        QScrollBar::handle:vertical {
            background-color: #3a4452;
            border-radius: 4px;
            min-height: 40px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #4a5666;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }

        /* ------------------------------------------------------------------ */
        /* Footer Elements                                                     */
        /* ------------------------------------------------------------------ */

        QCheckBox {
            color: #9aa7b8;
            font-size: 11px;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #2a323e;
            border-radius: 4px;
            background-color: #1c2129;
        }

        QCheckBox::indicator:checked {
            background-color: #3b9eff;
            border-color: #3b9eff;
        }

        QCheckBox::indicator:hover {
            border-color: #3b9eff;
        }

        QPushButton {
            background-color: #1c2129;
            color: #e8edf3;
            border: 1px solid #2a323e;
            border-radius: 6px;
            padding: 8px 20px;
            font-size: 11px;
        }

        QPushButton:hover {
            background-color: #262d38;
            border-color: #3b9eff;
        }

        QPushButton:pressed {
            background-color: #14181e;
        }

        /* ------------------------------------------------------------------ */
        /* Empty State                                                         */
        /* ------------------------------------------------------------------ */

        #EmptyStateWidget {
            background-color: transparent;
        }

        #EmptyStateIcon {
            color: #4a525e;
        }

        #EmptyStateText {
            color: #6c7684;
            font-size: 12px;
        }

        #EmptyStateHint {
            color: #3b9eff;
            font-size: 11px;
        }
    )");
}

void NMWelcomeDialog::setupAnimations() {
  // Animation setup - panels start with full opacity and animate using
  // QGraphicsOpacityEffect which doesn't affect layout visibility
}

void NMWelcomeDialog::startEntranceAnimations() {
  if (m_animationsPlayed) {
    return;
  }
  m_animationsPlayed = true;

  // Create opacity effects for smooth fade-in without affecting visibility
  // Using QGraphicsOpacityEffect which properly handles the rendering pipeline

  auto createFadeIn = [this](QWidget *widget, int delayMs) {
    if (!widget)
      return;

    // Create and apply opacity effect
    auto *effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    // Create fade-in animation
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(350);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    // Start after delay
    // The context object 'this' ensures the timer is cancelled if dialog is
    // destroyed
    QTimer::singleShot(delayMs, this, [anim, widget]() {
      if (!widget || widget->isHidden()) {
        delete anim;
        return;
      }
      anim->start(QPropertyAnimation::DeleteWhenStopped);
      // Clear effect after animation to restore normal rendering
      QObject::connect(anim, &QPropertyAnimation::finished, widget, [widget]() {
        if (widget) {
          widget->setGraphicsEffect(nullptr);
        }
      });
    });
  };

  // Stagger the panel animations for a nice cascading effect
  createFadeIn(m_leftPanel, 0);     // Left panel fades in first
  createFadeIn(m_centerPanel, 100); // Center panel slightly delayed
  createFadeIn(m_rightPanel, 200);  // Right panel last
}

void NMWelcomeDialog::animateButtonHover(QWidget *button, bool entering) {
  if (!button)
    return;

  // Create smooth scale animation for button hover
  QPropertyAnimation *scaleAnim =
      new QPropertyAnimation(button, "geometry", this);
  scaleAnim->setDuration(150);
  scaleAnim->setEasingCurve(entering ? QEasingCurve::OutBack
                                     : QEasingCurve::InOutQuad);

  QRect currentGeom = button->geometry();
  QRect targetGeom = currentGeom;

  if (entering) {
    // Slight scale up on hover
    targetGeom.adjust(-2, -2, 2, 2);
  }

  scaleAnim->setStartValue(button->geometry());
  scaleAnim->setEndValue(targetGeom);
  scaleAnim->start(QPropertyAnimation::DeleteWhenStopped);
}

void NMWelcomeDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);

  if (!m_animationsPlayed) {
    setupAnimations();
    // Start animations after a short delay to ensure widgets are fully laid out
    QTimer::singleShot(50, this, &NMWelcomeDialog::startEntranceAnimations);
  }
}

bool NMWelcomeDialog::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    // Handle template card clicks
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget && widget->objectName() == "TemplateCard") {
      int templateIndex = widget->property("templateIndex").toInt();
      onTemplateClicked(templateIndex);
      return true;
    }

    // Handle resource card clicks
    if (widget && widget->objectName() == "ResourceCard") {
      QString url = widget->property("url").toString();
      if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
      }
      return true;
    }
  } else if (event->type() == QEvent::Enter) {
    // Handle hover enter on buttons only (avoid geometry shifts on cards)
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget && widget->objectName().contains("Button")) {
      animateButtonHover(widget, true);
    }
  } else if (event->type() == QEvent::Leave) {
    // Handle hover leave on buttons only (avoid geometry shifts on cards)
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget && widget->objectName().contains("Button")) {
      animateButtonHover(widget, false);
    }
  }

  return QDialog::eventFilter(watched, event);
}

} // namespace NovelMind::editor::qt
