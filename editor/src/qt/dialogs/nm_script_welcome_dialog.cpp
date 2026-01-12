#include "NovelMind/editor/qt/dialogs/nm_script_welcome_dialog.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMScriptWelcomeDialog::NMScriptWelcomeDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Welcome to NM Script Editor"));
  setMinimumSize(DIALOG_WIDTH, DIALOG_HEIGHT);
  resize(DIALOG_WIDTH, DIALOG_HEIGHT);

  setupUI();
  styleDialog();
}

NMScriptWelcomeDialog::~NMScriptWelcomeDialog() = default;

void NMScriptWelcomeDialog::setupUI() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupHeader();
  setupMainContent();
  setupFooter();

  mainLayout->addWidget(m_headerWidget);
  mainLayout->addWidget(m_scrollArea, 1);
  mainLayout->addWidget(m_footerWidget);
}

void NMScriptWelcomeDialog::setupHeader() {
  m_headerWidget = new QWidget(this);
  m_headerWidget->setObjectName("ScriptWelcomeHeader");
  auto* headerLayout = new QVBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(32, 24, 32, 24);
  headerLayout->setSpacing(8);

  // Title
  auto* titleLabel = new QLabel(tr("Welcome to NM Script Editor!"), m_headerWidget);
  titleLabel->setObjectName("ScriptWelcomeTitle");
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(20);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);

  // Subtitle
  auto* subtitleLabel = new QLabel(
      tr("Discover powerful IDE features to write your visual novel scripts"), m_headerWidget);
  subtitleLabel->setObjectName("ScriptWelcomeSubtitle");
  subtitleLabel->setWordWrap(true);

  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(subtitleLabel);
}

void NMScriptWelcomeDialog::setupMainContent() {
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setObjectName("ScriptWelcomeScrollArea");
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_contentWidget = new QWidget();
  auto* contentLayout = new QVBoxLayout(m_contentWidget);
  contentLayout->setContentsMargins(32, 24, 32, 24);
  contentLayout->setSpacing(24);

  // Section 1: Quick Actions
  auto* actionsLabel = new QLabel(tr("Quick Actions"), m_contentWidget);
  actionsLabel->setObjectName("SectionTitle");
  QFont sectionFont = actionsLabel->font();
  sectionFont.setPointSize(14);
  sectionFont.setBold(true);
  actionsLabel->setFont(sectionFont);
  contentLayout->addWidget(actionsLabel);

  auto* actionsRow = new QHBoxLayout();
  actionsRow->setSpacing(16);

  auto& iconMgr = NMIconManager::instance();

  m_takeTourBtn = new QPushButton(tr("Take a 2-Minute Tour"), m_contentWidget);
  m_takeTourBtn->setIcon(iconMgr.getIcon("play", 16));
  m_takeTourBtn->setObjectName("PrimaryActionButton");
  m_takeTourBtn->setMinimumHeight(48);
  m_takeTourBtn->setToolTip(tr("Interactive tour of Script Editor features"));
  connect(m_takeTourBtn, &QPushButton::clicked, this, &NMScriptWelcomeDialog::onTakeTourClicked);

  m_quickStartBtn = new QPushButton(tr("View Quick Start Guide"), m_contentWidget);
  m_quickStartBtn->setIcon(iconMgr.getIcon("help", 16));
  m_quickStartBtn->setObjectName("SecondaryActionButton");
  m_quickStartBtn->setMinimumHeight(48);
  m_quickStartBtn->setToolTip(tr("Open documentation in browser"));
  connect(m_quickStartBtn, &QPushButton::clicked, this,
          &NMScriptWelcomeDialog::onQuickStartClicked);

  auto* shortcutsBtn = new QPushButton(tr("Keyboard Shortcuts"), m_contentWidget);
  shortcutsBtn->setIcon(iconMgr.getIcon("settings", 16));
  shortcutsBtn->setObjectName("SecondaryActionButton");
  shortcutsBtn->setMinimumHeight(48);
  shortcutsBtn->setToolTip(tr("View all keyboard shortcuts"));
  connect(shortcutsBtn, &QPushButton::clicked, this,
          &NMScriptWelcomeDialog::onKeyboardShortcutsClicked);

  actionsRow->addWidget(m_takeTourBtn, 1);
  actionsRow->addWidget(m_quickStartBtn, 1);
  actionsRow->addWidget(shortcutsBtn, 1);
  contentLayout->addLayout(actionsRow);

  // Section 2: Key Features
  auto* featuresLabel = new QLabel(tr("Key Features"), m_contentWidget);
  featuresLabel->setObjectName("SectionTitle");
  featuresLabel->setFont(sectionFont);
  contentLayout->addWidget(featuresLabel);

  auto* featuresGrid = new QGridLayout();
  featuresGrid->setSpacing(16);

  // Feature cards in a 2-column grid
  featuresGrid->addWidget(
      createFeatureCard("📍", tr("Code Minimap"),
                        tr("Navigate large scripts easily. Click minimap to jump to any section."),
                        tr("Toggle: Right-click minimap")),
      0, 0);

  featuresGrid->addWidget(
      createFeatureCard("🔍", tr("Find & Replace"),
                        tr("Search and replace text with regex support. Count matches instantly."),
                        tr("Ctrl+F / Ctrl+H")),
      0, 1);

  featuresGrid->addWidget(
      createFeatureCard("⚡", tr("Code Snippets"),
                        tr("Insert common patterns instantly with tabstop navigation."),
                        tr("Ctrl+J")),
      1, 0);

  featuresGrid->addWidget(
      createFeatureCard("🎯", tr("Go to Definition"),
                        tr("Jump to scene/character definitions. Ctrl+Click on any symbol."),
                        tr("F12 / Ctrl+Click")),
      1, 1);

  featuresGrid->addWidget(
      createFeatureCard("🔗", tr("Find References"),
                        tr("See all uses of a scene, character, or variable across all scripts."),
                        tr("Shift+F12")),
      2, 0);

  featuresGrid->addWidget(
      createFeatureCard("🎨", tr("Command Palette"),
                        tr("Quick access to all editor commands. Type to search."),
                        tr("Ctrl+Shift+P")),
      2, 1);

  featuresGrid->addWidget(
      createFeatureCard("📂", tr("Code Folding"),
                        tr("Collapse/expand code blocks. Click [-] or [+] in the margin."),
                        tr("Click fold icons")),
      3, 0);

  featuresGrid->addWidget(
      createFeatureCard("🔤", tr("Bracket Matching"),
                        tr("Matching brackets are auto-highlighted as you type."), tr("Automatic")),
      3, 1);

  contentLayout->addLayout(featuresGrid);

  // Section 3: Sample Scripts
  auto* samplesLabel = new QLabel(tr("Try Sample Scripts"), m_contentWidget);
  samplesLabel->setObjectName("SectionTitle");
  samplesLabel->setFont(sectionFont);
  contentLayout->addWidget(samplesLabel);

  auto* samplesRow = new QHBoxLayout();
  samplesRow->setSpacing(16);

  samplesRow->addWidget(createSampleCard(
      tr("Basic Scene"), tr("Simple dialogue with characters. Perfect for beginners."), "basic"));

  samplesRow->addWidget(createSampleCard(
      tr("Choice System"), tr("Branching dialogue with player choices and flags."), "choices"));

  samplesRow->addWidget(createSampleCard(
      tr("Advanced Features"), tr("Variables, conditionals, transitions, and more."), "advanced"));

  contentLayout->addLayout(samplesRow);

  contentLayout->addStretch();

  m_scrollArea->setWidget(m_contentWidget);
}

void NMScriptWelcomeDialog::setupFooter() {
  m_footerWidget = new QWidget(this);
  m_footerWidget->setObjectName("ScriptWelcomeFooter");
  auto* footerLayout = new QHBoxLayout(m_footerWidget);
  footerLayout->setContentsMargins(24, 12, 24, 12);

  m_skipCheckbox = new QCheckBox(tr("Don't show this again"), m_footerWidget);
  connect(m_skipCheckbox, &QCheckBox::toggled, [this](bool checked) { m_skipInFuture = checked; });

  m_closeBtn = new QPushButton(tr("Close"), m_footerWidget);
  m_closeBtn->setIcon(NMIconManager::instance().getIcon("file-close", 16));
  m_closeBtn->setMinimumWidth(100);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

  footerLayout->addWidget(m_skipCheckbox);
  footerLayout->addStretch();
  footerLayout->addWidget(m_closeBtn);
}

QWidget* NMScriptWelcomeDialog::createFeatureCard(const QString& icon, const QString& title,
                                                  const QString& description,
                                                  const QString& shortcut) {
  auto* card = new QFrame(m_contentWidget);
  card->setObjectName("FeatureCard");
  card->setFrameShape(QFrame::StyledPanel);

  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(16, 16, 16, 16);
  cardLayout->setSpacing(8);

  // Icon and title row
  auto* headerRow = new QHBoxLayout();
  headerRow->setSpacing(8);

  auto* iconLabel = new QLabel(icon, card);
  QFont iconFont = iconLabel->font();
  iconFont.setPointSize(18);
  iconLabel->setFont(iconFont);

  auto* titleLabel = new QLabel(title, card);
  titleLabel->setObjectName("FeatureTitle");
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  titleFont.setPointSize(11);
  titleLabel->setFont(titleFont);

  headerRow->addWidget(iconLabel);
  headerRow->addWidget(titleLabel);
  headerRow->addStretch();

  // Description
  auto* descLabel = new QLabel(description, card);
  descLabel->setObjectName("FeatureDescription");
  descLabel->setWordWrap(true);

  // Shortcut (if provided)
  QWidget* shortcutWidget = nullptr;
  if (!shortcut.isEmpty()) {
    shortcutWidget = new QWidget(card);
    auto* shortcutLayout = new QHBoxLayout(shortcutWidget);
    shortcutLayout->setContentsMargins(0, 0, 0, 0);
    shortcutLayout->setSpacing(4);

    auto* shortcutLabel = new QLabel(shortcut, shortcutWidget);
    shortcutLabel->setObjectName("ShortcutLabel");
    QFont monoFont("Courier New");
    monoFont.setPointSize(9);
    shortcutLabel->setFont(monoFont);

    shortcutLayout->addWidget(shortcutLabel);
    shortcutLayout->addStretch();
  }

  cardLayout->addLayout(headerRow);
  cardLayout->addWidget(descLabel);
  if (shortcutWidget) {
    cardLayout->addWidget(shortcutWidget);
  }

  return card;
}

QWidget* NMScriptWelcomeDialog::createSampleCard(const QString& title, const QString& description,
                                                 const QString& sampleId) {
  auto* card = new QFrame(m_contentWidget);
  card->setObjectName("SampleCard");
  card->setFrameShape(QFrame::StyledPanel);
  card->setCursor(Qt::PointingHandCursor);

  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(16, 16, 16, 16);
  cardLayout->setSpacing(8);

  // Icon
  auto* iconLabel = new QLabel("📄", card);
  QFont iconFont = iconLabel->font();
  iconFont.setPointSize(24);
  iconLabel->setFont(iconFont);

  // Title
  auto* titleLabel = new QLabel(title, card);
  titleLabel->setObjectName("SampleTitle");
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  titleFont.setPointSize(11);
  titleLabel->setFont(titleFont);

  // Description
  auto* descLabel = new QLabel(description, card);
  descLabel->setObjectName("SampleDescription");
  descLabel->setWordWrap(true);

  // Open button
  auto* openBtn = new QPushButton(tr("Open Sample"), card);
  openBtn->setIcon(NMIconManager::instance().getIcon("file-open", 16));
  openBtn->setObjectName("SampleOpenButton");
  connect(openBtn, &QPushButton::clicked, this, [this, sampleId]() { onSampleClicked(sampleId); });

  cardLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
  cardLayout->addWidget(titleLabel);
  cardLayout->addWidget(descLabel);
  cardLayout->addStretch();
  cardLayout->addWidget(openBtn);

  return card;
}

QWidget* NMScriptWelcomeDialog::createShortcutRow(const QString& action, const QString& shortcut) {
  auto* row = new QWidget(m_contentWidget);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(16);

  auto* actionLabel = new QLabel(action, row);
  auto* shortcutLabel = new QLabel(shortcut, row);
  shortcutLabel->setObjectName("ShortcutText");
  QFont monoFont("Courier New");
  monoFont.setBold(true);
  shortcutLabel->setFont(monoFont);

  layout->addWidget(actionLabel);
  layout->addStretch();
  layout->addWidget(shortcutLabel);

  return row;
}

void NMScriptWelcomeDialog::showKeyboardShortcuts() {
  // Create a modal dialog showing all keyboard shortcuts
  auto* dialog = new QDialog(this);
  dialog->setWindowTitle(tr("Keyboard Shortcuts"));
  dialog->setMinimumSize(500, 600);

  auto* layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(16);

  auto* titleLabel = new QLabel(tr("Script Editor Keyboard Shortcuts"), dialog);
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(14);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  layout->addWidget(titleLabel);

  auto* scrollArea = new QScrollArea(dialog);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* scrollContent = new QWidget();
  auto* scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setSpacing(2);

  // Add shortcut rows
  const QList<QPair<QString, QString>> shortcuts = {
      {tr("Save current script"), "Ctrl+S"},
      {tr("Find"), "Ctrl+F"},
      {tr("Find and Replace"), "Ctrl+H"},
      {tr("Go to Definition"), "F12"},
      {tr("Find References"), "Shift+F12"},
      {tr("Insert Snippet"), "Ctrl+J"},
      {tr("Command Palette"), "Ctrl+Shift+P"},
      {tr("Symbol Navigator"), "Ctrl+Shift+O"},
      {tr("Format Script"), "Ctrl+Shift+F"},
      {tr("Comment/Uncomment"), "Ctrl+/"},
      {tr("Duplicate Line"), "Ctrl+D"},
      {tr("Move Line Up"), "Alt+Up"},
      {tr("Move Line Down"), "Alt+Down"},
      {tr("Next Tabstop (Snippets)"), "Tab"},
      {tr("Previous Tabstop (Snippets)"), "Shift+Tab"},
      {tr("Toggle Line Comment"), "Ctrl+/"},
  };

  for (const auto& [action, shortcut] : shortcuts) {
    scrollLayout->addWidget(createShortcutRow(action, shortcut));
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollContent);
  layout->addWidget(scrollArea);

  auto* closeBtn = new QPushButton(tr("Close"), dialog);
  connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
  layout->addWidget(closeBtn);

  dialog->exec();
  delete dialog;
}

void NMScriptWelcomeDialog::onTakeTourClicked() {
  emit takeTourRequested();
  accept();
}

void NMScriptWelcomeDialog::onQuickStartClicked() {
  emit viewQuickStartRequested();
  // Open documentation URL
  QDesktopServices::openUrl(QUrl("https://github.com/VisageDvachevsky/StoryGraph/blob/main/docs/"
                                 "SCENE_STORY_GRAPH_PIPELINE.md"));
}

void NMScriptWelcomeDialog::onSampleClicked(const QString& sampleId) {
  m_selectedSample = sampleId;
  emit openSampleRequested(sampleId);
  accept();
}

void NMScriptWelcomeDialog::onKeyboardShortcutsClicked() {
  showKeyboardShortcuts();
}

void NMScriptWelcomeDialog::styleDialog() {
  const auto& palette = NMStyleManager::instance().palette();

  // Apply comprehensive stylesheet for the dialog
  setStyleSheet(QString(R"(
    /* ================================================================== */
    /* Script Welcome Dialog - Dark Theme                                 */
    /* ================================================================== */

    QDialog {
      background-color: %1;
    }

    /* ------------------------------------------------------------------ */
    /* Header Section                                                      */
    /* ------------------------------------------------------------------ */

    #ScriptWelcomeHeader {
      background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
          stop:0 %2, stop:1 %3);
      border-bottom: 1px solid %4;
    }

    #ScriptWelcomeTitle {
      color: %5;
    }

    #ScriptWelcomeSubtitle {
      color: %6;
      font-size: 11pt;
    }

    /* ------------------------------------------------------------------ */
    /* Content Section                                                     */
    /* ------------------------------------------------------------------ */

    #ScriptWelcomeScrollArea {
      background-color: %1;
      border: none;
    }

    #SectionTitle {
      color: %5;
      padding: 8px 0px;
    }

    /* Feature Cards */
    #FeatureCard {
      background-color: %7;
      border: 1px solid %8;
      border-radius: 8px;
      padding: 12px;
    }

    #FeatureCard:hover {
      background-color: %9;
      border-color: %10;
    }

    #FeatureTitle {
      color: %5;
    }

    #FeatureDescription {
      color: %6;
      font-size: 10pt;
    }

    #ShortcutLabel {
      color: %11;
      background-color: %12;
      padding: 2px 8px;
      border-radius: 4px;
      font-family: "Courier New", monospace;
    }

    /* Sample Cards */
    #SampleCard {
      background-color: %7;
      border: 1px solid %8;
      border-radius: 8px;
      padding: 16px;
      min-height: 200px;
    }

    #SampleCard:hover {
      background-color: %9;
      border-color: %10;
    }

    #SampleTitle {
      color: %5;
    }

    #SampleDescription {
      color: %6;
      font-size: 10pt;
    }

    #SampleOpenButton {
      background-color: %10;
      color: white;
      border: none;
      border-radius: 4px;
      padding: 8px 16px;
      font-weight: bold;
    }

    #SampleOpenButton:hover {
      background-color: %13;
    }

    #SampleOpenButton:pressed {
      background-color: %14;
    }

    /* ------------------------------------------------------------------ */
    /* Footer Section                                                      */
    /* ------------------------------------------------------------------ */

    #ScriptWelcomeFooter {
      background-color: %3;
      border-top: 1px solid %4;
    }

    QCheckBox {
      color: %6;
    }

    /* ------------------------------------------------------------------ */
    /* Buttons                                                             */
    /* ------------------------------------------------------------------ */

    #PrimaryActionButton {
      background-color: %10;
      color: white;
      border: none;
      border-radius: 4px;
      padding: 12px 24px;
      font-weight: bold;
      font-size: 11pt;
    }

    #PrimaryActionButton:hover {
      background-color: %13;
    }

    #PrimaryActionButton:pressed {
      background-color: %14;
    }

    #SecondaryActionButton {
      background-color: %7;
      color: %5;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 12px 24px;
      font-weight: bold;
      font-size: 11pt;
    }

    #SecondaryActionButton:hover {
      background-color: %9;
      border-color: %10;
    }

    #SecondaryActionButton:pressed {
      background-color: %12;
    }

    QPushButton {
      background-color: %7;
      color: %5;
      border: 1px solid %8;
      border-radius: 4px;
      padding: 8px 16px;
    }

    QPushButton:hover {
      background-color: %9;
    }

    QPushButton:pressed {
      background-color: %12;
    }

    /* ------------------------------------------------------------------ */
    /* Shortcut Dialog                                                     */
    /* ------------------------------------------------------------------ */

    #ShortcutText {
      color: %11;
      background-color: %12;
      padding: 4px 8px;
      border-radius: 4px;
      font-family: "Courier New", monospace;
      font-weight: bold;
    }
  )")
                    .arg(palette.bgDark.name())                     // %1 - background
                    .arg(palette.bgMedium.name())                   // %2 - header top
                    .arg(palette.bgLight.name())                    // %3 - header bottom
                    .arg(palette.borderLight.name())                // %4 - borders
                    .arg(palette.textPrimary.name())                // %5 - primary text
                    .arg(palette.textSecondary.name())              // %6 - secondary text
                    .arg(palette.bgMedium.name())                   // %7 - card background
                    .arg(palette.borderDefault.name())              // %8 - card border
                    .arg(palette.bgLight.name())                    // %9 - hover bg
                    .arg(palette.accentPrimary.name())              // %10 - accent (buttons)
                    .arg(palette.accentHover.name())                // %11 - shortcut text
                    .arg(palette.bgDark.name())                     // %12 - shortcut bg
                    .arg(palette.accentPrimary.lighter(110).name()) // %13 - button hover
                    .arg(palette.accentPrimary.darker(110).name())  // %14 - button pressed
  );
}

} // namespace NovelMind::editor::qt
