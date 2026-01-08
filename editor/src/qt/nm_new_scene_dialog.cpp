#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/scene_template_manager.hpp"
#include "nm_dialogs_detail.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMNewSceneDialog::NMNewSceneDialog(QWidget *parent,
                                   SceneTemplateManager *templateManager)
    : QDialog(parent), m_templateManager(templateManager) {
  setWindowTitle(tr("New Scene"));
  setModal(true);
  setObjectName("NMNewSceneDialog");
  setMinimumSize(650, 500);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  buildUi();
  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

QString NMNewSceneDialog::sceneName() const {
  if (m_nameEdit) {
    return m_nameEdit->text().trimmed();
  }
  return QString();
}

QString NMNewSceneDialog::sceneId() const {
  QString name = sceneName();
  if (name.isEmpty()) {
    return QString();
  }

  // Sanitize name to create ID
  QString id = name.toLower();
  id.replace(' ', '_');

  // Remove characters that aren't alphanumeric or underscore
  QString sanitized;
  for (const QChar &ch : id) {
    if (ch.isLetterOrNumber() || ch == '_') {
      sanitized += ch;
    }
  }

  // Remove consecutive underscores
  static QRegularExpression multiUnderscore("_+");
  sanitized.replace(multiUnderscore, "_");

  // Remove leading/trailing underscores
  while (sanitized.startsWith('_')) {
    sanitized.remove(0, 1);
  }
  while (sanitized.endsWith('_')) {
    sanitized.chop(1);
  }

  return sanitized.isEmpty() ? "scene" : sanitized;
}

bool NMNewSceneDialog::getNewScene(QWidget *parent,
                                   SceneTemplateManager *templateManager,
                                   QString &outSceneId,
                                   QString &outTemplateId) {
  NMNewSceneDialog dialog(parent, templateManager);
  if (dialog.exec() == QDialog::Accepted) {
    outSceneId = dialog.sceneId();
    outTemplateId = dialog.selectedTemplateId();
    return true;
  }
  return false;
}

void NMNewSceneDialog::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // Scene Name Group
  auto *nameGroup = new QGroupBox(tr("Scene Information"), this);
  auto *nameLayout = new QFormLayout(nameGroup);
  nameLayout->setSpacing(8);

  m_nameEdit = new QLineEdit(nameGroup);
  m_nameEdit->setPlaceholderText(tr("Enter scene name (e.g., Forest Clearing)"));
  nameLayout->addRow(tr("Scene Name:"), m_nameEdit);

  m_sceneIdPreview = new QLabel(nameGroup);
  m_sceneIdPreview->setStyleSheet("color: #888; font-style: italic;");
  nameLayout->addRow(tr("Scene ID:"), m_sceneIdPreview);

  layout->addWidget(nameGroup);

  // Template Selection Group
  auto *templateGroup = new QGroupBox(tr("Template Selection"), this);
  auto *templateMainLayout = new QVBoxLayout(templateGroup);
  templateMainLayout->setSpacing(8);

  // Category filter
  auto *categoryRow = new QHBoxLayout();
  auto *categoryLabel = new QLabel(tr("Category:"), templateGroup);
  m_categoryCombo = new QComboBox(templateGroup);
  m_categoryCombo->addItem(tr("All Templates"));
  m_categoryCombo->setToolTip(tr("Filter templates by category"));
  categoryRow->addWidget(categoryLabel);
  categoryRow->addWidget(m_categoryCombo, 1);
  templateMainLayout->addLayout(categoryRow);

  // Template list and preview splitter
  auto *splitter = new QSplitter(Qt::Horizontal, templateGroup);
  splitter->setChildrenCollapsible(false);

  // Template list
  m_templateList = new QListWidget(splitter);
  m_templateList->setIconSize(QSize(64, 36));
  m_templateList->setViewMode(QListView::ListMode);
  m_templateList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_templateList->setSpacing(2);
  m_templateList->setToolTip(
      tr("Select a template to start with, or choose 'Start from Blank'"));

  // Preview panel
  auto *previewWidget = new QWidget(splitter);
  auto *previewLayout = new QVBoxLayout(previewWidget);
  previewLayout->setContentsMargins(8, 0, 0, 0);

  m_previewImage = new QLabel(previewWidget);
  m_previewImage->setFixedSize(256, 144);
  m_previewImage->setAlignment(Qt::AlignCenter);
  m_previewImage->setStyleSheet(
      "background: #2a2a2a; border: 1px solid #444; border-radius: 4px;");
  previewLayout->addWidget(m_previewImage, 0, Qt::AlignCenter);

  m_previewName = new QLabel(previewWidget);
  m_previewName->setAlignment(Qt::AlignCenter);
  m_previewName->setStyleSheet("font-weight: bold; font-size: 14px;");
  previewLayout->addWidget(m_previewName);

  m_previewDescription = new QLabel(previewWidget);
  m_previewDescription->setWordWrap(true);
  m_previewDescription->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_previewDescription->setStyleSheet("color: #aaa; padding: 8px;");
  m_previewDescription->setMinimumHeight(60);
  previewLayout->addWidget(m_previewDescription);

  previewLayout->addStretch();

  splitter->addWidget(m_templateList);
  splitter->addWidget(previewWidget);
  splitter->setSizes({250, 280});

  templateMainLayout->addWidget(splitter, 1);
  layout->addWidget(templateGroup, 1);

  // Buttons
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName("NMSecondaryButton");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  m_createButton = new QPushButton(tr("Create Scene"), this);
  m_createButton->setObjectName("NMPrimaryButton");
  m_createButton->setDefault(true);
  m_createButton->setEnabled(false);
  connect(m_createButton, &QPushButton::clicked, this, &QDialog::accept);

  buttonLayout->addWidget(m_cancelButton);
  buttonLayout->addWidget(m_createButton);
  layout->addLayout(buttonLayout);

  // Connect signals
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          &NMNewSceneDialog::updateCreateEnabled);
  connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() {
    m_sceneIdPreview->setText(sceneId().isEmpty() ? tr("(enter name)")
                                                  : sceneId());
  });
  connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMNewSceneDialog::onCategoryChanged);
  connect(m_templateList, &QListWidget::itemSelectionChanged, this,
          &NMNewSceneDialog::onTemplateSelected);
  connect(m_templateList, &QListWidget::itemDoubleClicked, this, [this]() {
    if (m_createButton->isEnabled()) {
      accept();
    }
  });

  // Populate templates
  populateTemplateList();
  m_sceneIdPreview->setText(tr("(enter name)"));
}

void NMNewSceneDialog::populateTemplateList() {
  if (!m_templateManager || !m_templateList || !m_categoryCombo) {
    return;
  }

  // Populate categories
  m_categoryCombo->clear();
  m_categoryCombo->addItem(tr("All Templates"));
  QStringList categories = m_templateManager->getCategories();
  for (const QString &category : categories) {
    m_categoryCombo->addItem(category);
  }

  // Populate template list
  m_templateList->clear();

  // Add "Start from Blank" option first
  auto *blankItem = new QListWidgetItem(tr("Start from Blank"));
  blankItem->setData(Qt::UserRole, QString()); // Empty template ID
  blankItem->setToolTip(
      tr("Create an empty scene with no pre-defined objects"));
  m_templateList->addItem(blankItem);

  // Get current category filter
  QString categoryFilter;
  if (m_categoryCombo->currentIndex() > 0) {
    categoryFilter = m_categoryCombo->currentText();
  }

  // Add templates
  auto templates = m_templateManager->getAvailableTemplates(categoryFilter);
  for (const auto &meta : templates) {
    auto *item = new QListWidgetItem(meta.name);
    item->setData(Qt::UserRole, meta.id);
    item->setToolTip(meta.description);

    // Set icon from preview if available
    QPixmap preview = m_templateManager->getTemplatePreview(meta.id);
    if (!preview.isNull()) {
      item->setIcon(QIcon(preview.scaled(64, 36, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation)));
    }

    m_templateList->addItem(item);
  }

  // Select first item by default
  if (m_templateList->count() > 0) {
    m_templateList->setCurrentRow(0);
  }
}

void NMNewSceneDialog::onTemplateSelected() {
  if (!m_templateList) {
    return;
  }

  QListWidgetItem *item = m_templateList->currentItem();
  if (!item) {
    m_selectedTemplateId.clear();
    updatePreview();
    return;
  }

  m_selectedTemplateId = item->data(Qt::UserRole).toString();
  updatePreview();
  updateCreateEnabled();
}

void NMNewSceneDialog::onCategoryChanged(int /*index*/) {
  populateTemplateList();
}

void NMNewSceneDialog::updatePreview() {
  if (!m_previewImage || !m_previewName || !m_previewDescription) {
    return;
  }

  if (m_selectedTemplateId.isEmpty()) {
    // "Start from Blank" selected
    m_previewName->setText(tr("Empty Scene"));
    m_previewDescription->setText(
        tr("Start with a completely blank canvas. "
           "No objects will be pre-created - add everything yourself."));

    // Show placeholder preview
    QPixmap placeholder(256, 144);
    placeholder.fill(QColor(40, 40, 45));
    QPainter painter(&placeholder);
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(0, 0, 255, 143);
    painter.setPen(QColor(150, 150, 150));
    painter.drawText(placeholder.rect(), Qt::AlignCenter, tr("Blank"));
    painter.end();
    m_previewImage->setPixmap(placeholder);
  } else if (m_templateManager) {
    // Show template preview
    auto meta = m_templateManager->getTemplateMetadata(m_selectedTemplateId);
    m_previewName->setText(meta.name);
    m_previewDescription->setText(meta.description);

    QPixmap preview = m_templateManager->getTemplatePreview(m_selectedTemplateId);
    if (!preview.isNull()) {
      m_previewImage->setPixmap(
          preview.scaled(256, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
  }
}

void NMNewSceneDialog::updateCreateEnabled() {
  bool hasName = !sceneName().isEmpty() && !sceneId().isEmpty();
  bool hasSelection = m_templateList && m_templateList->currentItem() != nullptr;

  if (m_createButton) {
    m_createButton->setEnabled(hasName && hasSelection);
  }
}

} // namespace NovelMind::editor::qt
