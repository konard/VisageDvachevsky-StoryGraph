#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "nm_dialogs_detail.hpp"

#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace NovelMind::editor::qt {

NMNewProjectDialog::NMNewProjectDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("New Project"));
  setModal(true);
  setObjectName("NMNewProjectDialog");
  setMinimumWidth(520);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  buildUi();
  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

QStringList NMNewProjectDialog::standardResolutions() {
  return {"1920x1080 (Full HD)",  "1280x720 (HD)",  "2560x1440 (QHD)",
          "3840x2160 (4K)",       "1600x900",       "1366x768",
          "800x600 (Classic VN)", "1024x768 (4:3)", "Custom..."};
}

QStringList NMNewProjectDialog::standardLocales() {
  return {"en (English)",
          "ja (Japanese)",
          "zh-CN (Chinese Simplified)",
          "zh-TW (Chinese Traditional)",
          "ko (Korean)",
          "ru (Russian)",
          "es (Spanish)",
          "fr (French)",
          "de (German)",
          "pt-BR (Portuguese)",
          "it (Italian)"};
}

QStringList NMNewProjectDialog::workflowModes() {
  return {QObject::tr("Code-Only (Script is source of truth)"),
          QObject::tr("Graph-Only (Visual graph is source of truth)"),
          QObject::tr("Mixed (Both with conflict rules)")};
}

void NMNewProjectDialog::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Basic Settings Group
  auto *basicGroup = new QGroupBox(tr("Project Information"), this);
  auto *basicLayout = new QFormLayout(basicGroup);
  basicLayout->setSpacing(8);

  m_nameEdit = new QLineEdit(basicGroup);
  m_nameEdit->setPlaceholderText(tr("Enter project name"));
  basicLayout->addRow(tr("Name:"), m_nameEdit);

  m_templateCombo = new QComboBox(basicGroup);
  m_templateCombo->setToolTip(
      tr("Select a starting template for your project"));
  basicLayout->addRow(tr("Template:"), m_templateCombo);

  auto *dirRow = new QWidget(basicGroup);
  auto *dirLayout = new QHBoxLayout(dirRow);
  dirLayout->setContentsMargins(0, 0, 0, 0);
  dirLayout->setSpacing(6);

  m_directoryEdit = new QLineEdit(dirRow);
  m_directoryEdit->setPlaceholderText(tr("Select project location"));

  m_browseButton = new QPushButton(tr("Browse..."), dirRow);
  m_browseButton->setObjectName("NMSecondaryButton");
  connect(m_browseButton, &QPushButton::clicked, this,
          &NMNewProjectDialog::browseDirectory);

  dirLayout->addWidget(m_directoryEdit, 1);
  dirLayout->addWidget(m_browseButton);

  basicLayout->addRow(tr("Location:"), dirRow);
  layout->addWidget(basicGroup);

  // Display Settings Group
  auto *displayGroup = new QGroupBox(tr("Display Settings"), this);
  auto *displayLayout = new QFormLayout(displayGroup);
  displayLayout->setSpacing(8);

  m_resolutionCombo = new QComboBox(displayGroup);
  m_resolutionCombo->addItems(standardResolutions());
  m_resolutionCombo->setCurrentIndex(0); // Default to 1920x1080
  m_resolutionCombo->setToolTip(
      tr("Base resolution for the visual novel.\n"
         "Common choices: 1920x1080 for modern displays,\n"
         "1280x720 for wider compatibility."));
  displayLayout->addRow(tr("Base Resolution:"), m_resolutionCombo);

  m_localeCombo = new QComboBox(displayGroup);
  m_localeCombo->addItems(standardLocales());
  m_localeCombo->setCurrentIndex(0); // Default to English
  m_localeCombo->setToolTip(tr("Default language for your visual novel.\n"
                               "Additional languages can be added later."));
  displayLayout->addRow(tr("Default Language:"), m_localeCombo);

  layout->addWidget(displayGroup);

  // Workflow Settings Group (issue #100)
  auto *workflowGroup = new QGroupBox(tr("Workflow Settings"), this);
  auto *workflowLayout = new QFormLayout(workflowGroup);
  workflowLayout->setSpacing(8);

  m_workflowCombo = new QComboBox(workflowGroup);
  m_workflowCombo->addItems(workflowModes());
  m_workflowCombo->setCurrentIndex(0); // Default to Code-Only (Script)
  m_workflowCombo->setToolTip(
      tr("Choose the primary development workflow for your project.\n"
         "This determines which data source (Script or Graph) is authoritative."));
  workflowLayout->addRow(tr("Workflow Mode:"), m_workflowCombo);

  m_workflowDescription = new QLabel(workflowGroup);
  m_workflowDescription->setWordWrap(true);
  m_workflowDescription->setStyleSheet(
      "color: #888; padding: 8px; background: #2a2a2a; border-radius: 4px;");
  workflowLayout->addRow(m_workflowDescription);

  connect(m_workflowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMNewProjectDialog::updateWorkflowDescription);
  updateWorkflowDescription();

  layout->addWidget(workflowGroup);

  // Path Preview
  m_pathPreview = new QLabel(this);
  m_pathPreview->setWordWrap(true);
  m_pathPreview->setStyleSheet("color: #888; font-style: italic;");
  layout->addWidget(m_pathPreview);

  layout->addStretch();

  // Buttons
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName("NMSecondaryButton");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  m_createButton = new QPushButton(tr("Create Project"), this);
  m_createButton->setObjectName("NMPrimaryButton");
  m_createButton->setDefault(true);
  connect(m_createButton, &QPushButton::clicked, this, &QDialog::accept);

  buttonLayout->addWidget(m_cancelButton);
  buttonLayout->addWidget(m_createButton);

  layout->addLayout(buttonLayout);

  // Connect signals
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &NMNewProjectDialog::updatePreview);
  connect(m_directoryEdit, &QLineEdit::textChanged, this,
          &NMNewProjectDialog::updatePreview);
  connect(m_templateCombo, &QComboBox::currentTextChanged, this,
          &NMNewProjectDialog::updatePreview);

  updatePreview();
  updateCreateEnabled();
}

void NMNewProjectDialog::setTemplateOptions(const QStringList &templates) {
  if (!m_templateCombo) {
    return;
  }
  m_templateCombo->clear();
  m_templateCombo->addItems(templates);
  updatePreview();
}

void NMNewProjectDialog::setTemplate(const QString &templateName) {
  if (!m_templateCombo) {
    return;
  }
  const int index = m_templateCombo->findText(templateName);
  if (index >= 0) {
    m_templateCombo->setCurrentIndex(index);
  } else if (!templateName.trimmed().isEmpty()) {
    m_templateCombo->addItem(templateName.trimmed());
    m_templateCombo->setCurrentIndex(m_templateCombo->count() - 1);
  }
  updatePreview();
}

void NMNewProjectDialog::setProjectName(const QString &name) {
  if (m_nameEdit) {
    m_nameEdit->setText(name);
  }
  updatePreview();
}

void NMNewProjectDialog::setBaseDirectory(const QString &directory) {
  if (m_directoryEdit) {
    m_directoryEdit->setText(directory);
  }
  updatePreview();
}

QString NMNewProjectDialog::projectName() const {
  return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QString NMNewProjectDialog::baseDirectory() const {
  return m_directoryEdit ? m_directoryEdit->text().trimmed() : QString();
}

QString NMNewProjectDialog::projectPath() const {
  const QString base = baseDirectory();
  const QString name = projectName();
  if (base.isEmpty() || name.isEmpty()) {
    return QString();
  }
  return QDir(base).filePath(name);
}

QString NMNewProjectDialog::templateName() const {
  return m_templateCombo ? m_templateCombo->currentText().trimmed() : QString();
}

void NMNewProjectDialog::setResolution(const QString &resolution) {
  if (!m_resolutionCombo) {
    return;
  }
  const int index =
      m_resolutionCombo->findText(resolution, Qt::MatchStartsWith);
  if (index >= 0) {
    m_resolutionCombo->setCurrentIndex(index);
  }
}

void NMNewProjectDialog::setLocale(const QString &locale) {
  if (!m_localeCombo) {
    return;
  }
  const int index = m_localeCombo->findText(locale, Qt::MatchStartsWith);
  if (index >= 0) {
    m_localeCombo->setCurrentIndex(index);
  }
}

QString NMNewProjectDialog::resolution() const {
  if (!m_resolutionCombo) {
    return "1920x1080";
  }
  QString text = m_resolutionCombo->currentText();
  // Extract just the resolution part (e.g., "1920x1080" from "1920x1080 (Full
  // HD)")
  const qsizetype spacePos = text.indexOf(' ');
  return spacePos > 0 ? text.left(spacePos) : text;
}

QString NMNewProjectDialog::locale() const {
  if (!m_localeCombo) {
    return "en";
  }
  QString text = m_localeCombo->currentText();
  // Extract just the locale code (e.g., "en" from "en (English)")
  const qsizetype spacePos = text.indexOf(' ');
  return spacePos > 0 ? text.left(spacePos) : text;
}

void NMNewProjectDialog::setWorkflowMode(int modeIndex) {
  if (!m_workflowCombo) {
    return;
  }
  if (modeIndex >= 0 && modeIndex < m_workflowCombo->count()) {
    m_workflowCombo->setCurrentIndex(modeIndex);
  }
}

int NMNewProjectDialog::workflowMode() const {
  // Returns: 0 = Script (Code-Only), 1 = Graph (Graph-Only), 2 = Mixed
  return m_workflowCombo ? m_workflowCombo->currentIndex() : 0;
}

void NMNewProjectDialog::updateWorkflowDescription() {
  if (!m_workflowDescription || !m_workflowCombo) {
    return;
  }

  int idx = m_workflowCombo->currentIndex();
  QString desc;
  switch (idx) {
  case 0: // Code-Only (Script)
    desc = tr("NMScript files are the authoritative source. The Story Graph "
              "will automatically sync from script files. Best for "
              "programmers and version control workflows.");
    break;
  case 1: // Graph-Only
    desc = tr("The Story Graph is the authoritative source. Script files "
              "are automatically generated from the graph. Best for "
              "narrative designers and visual prototyping.");
    break;
  case 2: // Mixed
    desc = tr("Both Script and Graph can be edited. Graph overrides script "
              "when conflicts occur. Per-scene workflow can be configured. "
              "Best for team collaboration.");
    break;
  default:
    desc = tr("Select a workflow mode.");
    break;
  }
  m_workflowDescription->setText(desc);
}

void NMNewProjectDialog::updatePreview() {
  const QString path = projectPath();
  if (m_pathPreview) {
    if (path.isEmpty()) {
      m_pathPreview->setText(tr("Project path will appear here"));
    } else {
      m_pathPreview->setText(tr("Project path: %1").arg(path));
    }
  }
  updateCreateEnabled();
}

void NMNewProjectDialog::updateCreateEnabled() {
  if (!m_createButton) {
    return;
  }
  const bool enabled = !projectName().isEmpty() && !baseDirectory().isEmpty();
  m_createButton->setEnabled(enabled);
}

void NMNewProjectDialog::browseDirectory() {
  const QString current =
      baseDirectory().isEmpty() ? QDir::homePath() : baseDirectory();
  const QString dir = NMFileDialog::getExistingDirectory(
      this, tr("Select Project Location"), current);
  if (!dir.isEmpty()) {
    setBaseDirectory(dir);
  }
}

} // namespace NovelMind::editor::qt
