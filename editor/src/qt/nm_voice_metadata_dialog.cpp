/**
 * @file nm_voice_metadata_dialog.cpp
 * @brief Implementation of the voice line metadata dialog
 *
 * Provides a comprehensive interface for editing voice line metadata
 * including tags, notes, speaker assignment, and scene information.
 */

#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "nm_dialogs_detail.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMVoiceMetadataDialog::NMVoiceMetadataDialog(
    QWidget *parent, const QString &lineId, const QStringList &currentTags,
    const QString &currentNotes, const QString &currentSpeaker,
    const QString &currentScene, const QStringList &availableSpeakers,
    const QStringList &availableScenes, const QStringList &suggestedTags)
    : QDialog(parent) {
  setWindowTitle(tr("Edit Voice Line Metadata"));
  setModal(true);
  setObjectName("NMVoiceMetadataDialog");
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setMinimumSize(450, 500);
  resize(500, 550);

  buildUi(lineId, currentTags, currentNotes, currentSpeaker, currentScene,
          availableSpeakers, availableScenes, suggestedTags);

  detail::applyDialogFrameStyle(this);
  detail::animateDialogIn(this);
}

void NMVoiceMetadataDialog::buildUi(
    const QString &lineId, const QStringList &currentTags,
    const QString &currentNotes, const QString &currentSpeaker,
    const QString &currentScene, const QStringList &availableSpeakers,
    const QStringList &availableScenes, const QStringList &suggestedTags) {

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->setSpacing(12);

  // Line ID header (read-only)
  auto *headerLabel = new QLabel(tr("Line ID: <b>%1</b>").arg(lineId), this);
  headerLabel->setObjectName("NMDialogHeader");
  mainLayout->addWidget(headerLabel);

  // Speaker and Scene section
  auto *identityGroup = new QGroupBox(tr("Identity"), this);
  auto *identityLayout = new QFormLayout(identityGroup);
  identityLayout->setSpacing(8);

  m_speakerCombo = new QComboBox(this);
  m_speakerCombo->setEditable(true);
  m_speakerCombo->setPlaceholderText(tr("Select or enter speaker..."));
  if (!availableSpeakers.isEmpty()) {
    m_speakerCombo->addItems(availableSpeakers);
  }
  if (!currentSpeaker.isEmpty()) {
    int idx = m_speakerCombo->findText(currentSpeaker);
    if (idx >= 0) {
      m_speakerCombo->setCurrentIndex(idx);
    } else {
      m_speakerCombo->setEditText(currentSpeaker);
    }
  }
  identityLayout->addRow(tr("Speaker:"), m_speakerCombo);

  m_sceneCombo = new QComboBox(this);
  m_sceneCombo->setEditable(true);
  m_sceneCombo->setPlaceholderText(tr("Select or enter scene..."));
  if (!availableScenes.isEmpty()) {
    m_sceneCombo->addItems(availableScenes);
  }
  if (!currentScene.isEmpty()) {
    int idx = m_sceneCombo->findText(currentScene);
    if (idx >= 0) {
      m_sceneCombo->setCurrentIndex(idx);
    } else {
      m_sceneCombo->setEditText(currentScene);
    }
  }
  identityLayout->addRow(tr("Scene:"), m_sceneCombo);

  mainLayout->addWidget(identityGroup);

  // Tags section
  auto *tagsGroup = new QGroupBox(tr("Tags"), this);
  auto *tagsLayout = new QVBoxLayout(tagsGroup);
  tagsLayout->setSpacing(8);

  // Tag input row
  auto *tagInputLayout = new QHBoxLayout();
  m_tagInput = new QLineEdit(this);
  m_tagInput->setPlaceholderText(tr("Enter a tag and press Add..."));
  tagInputLayout->addWidget(m_tagInput, 1);

  m_addTagBtn = new QPushButton(tr("Add"), this);
  m_addTagBtn->setObjectName("NMSecondaryButton");
  connect(m_addTagBtn, &QPushButton::clicked, this,
          &NMVoiceMetadataDialog::onAddTag);
  connect(m_tagInput, &QLineEdit::returnPressed, this,
          &NMVoiceMetadataDialog::onAddTag);
  tagInputLayout->addWidget(m_addTagBtn);

  m_removeTagBtn = new QPushButton(tr("Remove"), this);
  m_removeTagBtn->setObjectName("NMSecondaryButton");
  m_removeTagBtn->setEnabled(false);
  connect(m_removeTagBtn, &QPushButton::clicked, this,
          &NMVoiceMetadataDialog::onRemoveTag);
  tagInputLayout->addWidget(m_removeTagBtn);

  tagsLayout->addLayout(tagInputLayout);

  // Tag list
  m_tagList = new QListWidget(this);
  m_tagList->setMaximumHeight(100);
  m_tagList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tagList->addItems(currentTags);
  connect(m_tagList, &QListWidget::itemSelectionChanged, this, [this]() {
    m_removeTagBtn->setEnabled(!m_tagList->selectedItems().isEmpty());
  });
  tagsLayout->addWidget(m_tagList);

  // Tag suggestions (if any)
  if (!suggestedTags.isEmpty()) {
    auto *suggestionsLabel = new QLabel(tr("Suggestions:"), this);
    suggestionsLabel->setStyleSheet("color: #888;");
    tagsLayout->addWidget(suggestionsLabel);

    m_suggestionsWidget = new QWidget(this);
    auto *suggestionsLayout = new QHBoxLayout(m_suggestionsWidget);
    suggestionsLayout->setContentsMargins(0, 0, 0, 0);
    suggestionsLayout->setSpacing(4);

    for (const QString &tag : suggestedTags) {
      if (!currentTags.contains(tag)) {
        auto *suggBtn = new QPushButton(tag, m_suggestionsWidget);
        suggBtn->setObjectName("NMTagButton");
        suggBtn->setFlat(true);
        suggBtn->setStyleSheet(
            "QPushButton { background: #333; border: 1px solid #555; "
            "border-radius: 4px; padding: 2px 8px; color: #aaa; } "
            "QPushButton:hover { background: #444; color: #fff; }");
        connect(suggBtn, &QPushButton::clicked, this,
                [this, tag]() { onTagSuggestionClicked(tag); });
        suggestionsLayout->addWidget(suggBtn);
      }
    }
    suggestionsLayout->addStretch();
    tagsLayout->addWidget(m_suggestionsWidget);
  }

  mainLayout->addWidget(tagsGroup);

  // Notes section
  auto *notesGroup = new QGroupBox(tr("Notes"), this);
  auto *notesLayout = new QVBoxLayout(notesGroup);

  m_notesEdit = new QTextEdit(this);
  m_notesEdit->setPlaceholderText(
      tr("Enter notes for actors/directors..."));
  m_notesEdit->setAcceptRichText(false);
  m_notesEdit->setMinimumHeight(80);
  m_notesEdit->setPlainText(currentNotes);
  notesLayout->addWidget(m_notesEdit);

  mainLayout->addWidget(notesGroup);
  mainLayout->addStretch();

  // Button row
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setObjectName("NMSecondaryButton");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  buttonLayout->addWidget(m_cancelButton);

  m_okButton = new QPushButton(tr("Save"), this);
  m_okButton->setObjectName("NMPrimaryButton");
  m_okButton->setDefault(true);
  connect(m_okButton, &QPushButton::clicked, this, [this]() {
    updateResult();
    accept();
  });
  buttonLayout->addWidget(m_okButton);

  mainLayout->addLayout(buttonLayout);

  // Initialize result with current values
  m_result.tags = currentTags;
  m_result.notes = currentNotes;
  m_result.speaker = currentSpeaker;
  m_result.scene = currentScene;
}

void NMVoiceMetadataDialog::onAddTag() {
  QString tag = m_tagInput->text().trimmed();
  if (tag.isEmpty()) {
    return;
  }

  // Check if tag already exists
  QList<QListWidgetItem *> existing = m_tagList->findItems(tag, Qt::MatchExactly);
  if (!existing.isEmpty()) {
    m_tagInput->clear();
    return;
  }

  m_tagList->addItem(tag);
  m_tagInput->clear();
  m_tagInput->setFocus();
}

void NMVoiceMetadataDialog::onRemoveTag() {
  QList<QListWidgetItem *> selected = m_tagList->selectedItems();
  for (QListWidgetItem *item : selected) {
    delete m_tagList->takeItem(m_tagList->row(item));
  }
  m_removeTagBtn->setEnabled(false);
}

void NMVoiceMetadataDialog::onTagSuggestionClicked(const QString &tag) {
  // Check if tag already exists
  QList<QListWidgetItem *> existing = m_tagList->findItems(tag, Qt::MatchExactly);
  if (!existing.isEmpty()) {
    return;
  }

  m_tagList->addItem(tag);

  // Hide the clicked suggestion button
  if (m_suggestionsWidget) {
    QList<QPushButton *> buttons =
        m_suggestionsWidget->findChildren<QPushButton *>();
    for (QPushButton *btn : buttons) {
      if (btn->text() == tag) {
        btn->hide();
        break;
      }
    }
  }
}

void NMVoiceMetadataDialog::updateResult() {
  // Collect tags
  m_result.tags.clear();
  for (int i = 0; i < m_tagList->count(); ++i) {
    m_result.tags.append(m_tagList->item(i)->text());
  }

  m_result.notes = m_notesEdit->toPlainText();
  m_result.speaker = m_speakerCombo->currentText().trimmed();
  m_result.scene = m_sceneCombo->currentText().trimmed();
}

bool NMVoiceMetadataDialog::getMetadata(
    QWidget *parent, const QString &lineId, const QStringList &currentTags,
    const QString &currentNotes, const QString &currentSpeaker,
    const QString &currentScene, MetadataResult &outResult,
    const QStringList &availableSpeakers, const QStringList &availableScenes,
    const QStringList &suggestedTags) {

  NMVoiceMetadataDialog dialog(parent, lineId, currentTags, currentNotes,
                                currentSpeaker, currentScene, availableSpeakers,
                                availableScenes, suggestedTags);

  if (dialog.exec() == QDialog::Accepted) {
    outResult = dialog.result();
    return true;
  }
  return false;
}

} // namespace NovelMind::editor::qt
