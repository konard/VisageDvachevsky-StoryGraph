#include "NovelMind/editor/qt/nm_hotkeys_dialog.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// Helper class to capture key sequences
class ShortcutRecorder : public QDialog {
  Q_OBJECT
public:
  explicit ShortcutRecorder(QWidget* parent = nullptr) : QDialog(parent) {
    setWindowTitle(tr("Record Shortcut"));
    setModal(true);
    setFixedSize(300, 120);

    auto* layout = new QVBoxLayout(this);
    m_label = new QLabel(tr("Press the key combination you want to use.\n"
                            "Press Escape to cancel."),
                         this);
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label);

    m_shortcutLabel = new QLabel(this);
    m_shortcutLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_shortcutLabel->font();
    font.setPointSize(14);
    font.setBold(true);
    m_shortcutLabel->setFont(font);
    layout->addWidget(m_shortcutLabel);

    auto* clearBtn = new QPushButton(tr("Clear"), this);
    clearBtn->setIcon(NMIconManager::instance().getIcon("delete", 16));
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
      m_sequence = QKeySequence();
      accept();
    });
    layout->addWidget(clearBtn);

    setFocusPolicy(Qt::StrongFocus);
  }

  QKeySequence sequence() const { return m_sequence; }

protected:
  void keyPressEvent(QKeyEvent* event) override {
    if (event->key() == Qt::Key_Escape) {
      reject();
      return;
    }

    // Build the key sequence
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    // Ignore modifier-only keys
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt ||
        key == Qt::Key_Meta) {
      return;
    }

    int combo = key;
    if (modifiers & Qt::ControlModifier)
      combo |= static_cast<int>(Qt::CTRL);
    if (modifiers & Qt::ShiftModifier)
      combo |= static_cast<int>(Qt::SHIFT);
    if (modifiers & Qt::AltModifier)
      combo |= static_cast<int>(Qt::ALT);
    if (modifiers & Qt::MetaModifier)
      combo |= static_cast<int>(Qt::META);

    m_sequence = QKeySequence(combo);
    m_shortcutLabel->setText(m_sequence.toString(QKeySequence::NativeText));

    // Auto-accept after a short delay
    QTimer::singleShot(300, this, &QDialog::accept);
  }

private:
  QLabel* m_label = nullptr;
  QLabel* m_shortcutLabel = nullptr;
  QKeySequence m_sequence;
};

NMHotkeysDialog::NMHotkeysDialog(const QList<NMHotkeyEntry>& entries, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("Hotkeys & Tips"));
  setModal(true);
  resize(680, 520);

  // Store entries for lookup
  for (const auto& entry : entries) {
    m_entries.insert(entry.id, entry);
  }

  buildUi(entries);
}

void NMHotkeysDialog::buildUi(const QList<NMHotkeyEntry>& entries) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto* title = new QLabel(tr("<b>Hotkeys & Tips</b><br><span style='color: "
                              "gray;'>Double-click to edit. Type to filter.</span>"),
                           this);
  title->setTextFormat(Qt::RichText);
  layout->addWidget(title);

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter actions, shortcuts, or tips..."));
  layout->addWidget(m_filterEdit);

  m_tree = new QTreeWidget(this);
  m_tree->setHeaderLabels({tr("Action"), tr("Shortcut"), tr("Notes")});
  m_tree->setRootIsDecorated(true);
  m_tree->setAllColumnsShowFocus(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setIndentation(18);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);

  const auto& palette = NMStyleManager::instance().palette();
  m_tree->setStyleSheet(QString("QTreeWidget {"
                                "  background-color: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "}"
                                "QTreeWidget::item:selected {"
                                "  background-color: %4;"
                                "}"
                                "QHeaderView::section {"
                                "  background-color: %5;"
                                "  color: %2;"
                                "  padding: 4px 6px;"
                                "  border: 1px solid %3;"
                                "}")
                            .arg(palette.bgMedium.name())
                            .arg(palette.textPrimary.name())
                            .arg(palette.borderDark.name())
                            .arg(palette.bgLight.name())
                            .arg(palette.bgDark.name()));

  QMap<QString, QTreeWidgetItem*> sectionItems;
  for (const auto& entry : entries) {
    QTreeWidgetItem* sectionItem = sectionItems.value(entry.section, nullptr);
    if (!sectionItem) {
      sectionItem = new QTreeWidgetItem(m_tree);
      sectionItem->setText(0, entry.section);
      sectionItem->setFirstColumnSpanned(true);
      QFont boldFont = sectionItem->font(0);
      boldFont.setBold(true);
      sectionItem->setFont(0, boldFont);
      sectionItem->setExpanded(true);
      sectionItems.insert(entry.section, sectionItem);
    }

    auto* item = new QTreeWidgetItem(sectionItem);
    item->setText(0, entry.action);
    item->setText(1, entry.shortcut);
    item->setText(2, entry.notes);
    item->setData(0, Qt::UserRole, entry.id); // Store ID for lookup

    // Mark modified entries
    if (entry.isModified) {
      QFont italicFont = item->font(1);
      italicFont.setItalic(true);
      item->setFont(1, italicFont);
    }

    // Store for quick lookup
    m_itemLookup.insert(entry.id, item);
  }

  layout->addWidget(m_tree, 1);

  // Conflict warning label
  m_conflictLabel = new QLabel(this);
  m_conflictLabel->setStyleSheet("color: #ff6b6b; font-weight: bold;");
  m_conflictLabel->setVisible(false);
  layout->addWidget(m_conflictLabel);

  // Action buttons
  auto* actionLayout = new QHBoxLayout();
  actionLayout->setSpacing(8);

  auto& iconMgr = NMIconManager::instance();

  m_recordBtn = new QPushButton(tr("Record Shortcut"), this);
  m_recordBtn->setIcon(iconMgr.getIcon("record", 16));
  m_recordBtn->setEnabled(false);
  actionLayout->addWidget(m_recordBtn);

  m_resetBtn = new QPushButton(tr("Reset to Default"), this);
  m_resetBtn->setIcon(iconMgr.getIcon("property-reset", 16));
  m_resetBtn->setEnabled(false);
  actionLayout->addWidget(m_resetBtn);

  m_resetAllBtn = new QPushButton(tr("Reset All"), this);
  m_resetAllBtn->setIcon(iconMgr.getIcon("refresh", 16));
  actionLayout->addWidget(m_resetAllBtn);

  actionLayout->addStretch();

  m_exportBtn = new QPushButton(tr("Export..."), this);
  m_exportBtn->setIcon(iconMgr.getIcon("export", 16));
  actionLayout->addWidget(m_exportBtn);

  m_importBtn = new QPushButton(tr("Import..."), this);
  m_importBtn->setIcon(iconMgr.getIcon("import", 16));
  actionLayout->addWidget(m_importBtn);

  layout->addLayout(actionLayout);

  // Dialog buttons
  auto* buttons = new QDialogButtonBox(this);
  m_applyBtn = buttons->addButton(QDialogButtonBox::Apply);
  buttons->addButton(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_applyBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onApplyClicked);
  layout->addWidget(buttons);

  // Connect signals
  connect(m_filterEdit, &QLineEdit::textChanged, this, &NMHotkeysDialog::applyFilter);
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &NMHotkeysDialog::onItemDoubleClicked);
  connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
    auto items = m_tree->selectedItems();
    bool hasSelection = !items.isEmpty() && items.first()->parent() != nullptr;
    m_recordBtn->setEnabled(hasSelection);
    m_resetBtn->setEnabled(hasSelection);
  });
  connect(m_recordBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onRecordShortcut);
  connect(m_resetBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onResetToDefault);
  connect(m_resetAllBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onResetAllToDefaults);
  connect(m_exportBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onExportClicked);
  connect(m_importBtn, &QPushButton::clicked, this, &NMHotkeysDialog::onImportClicked);
}

void NMHotkeysDialog::applyFilter(const QString& text) {
  const QString filter = text.trimmed().toLower();

  for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
    auto* sectionItem = m_tree->topLevelItem(i);
    if (!sectionItem) {
      continue;
    }

    bool anyVisible = false;
    for (int j = 0; j < sectionItem->childCount(); ++j) {
      auto* child = sectionItem->child(j);
      if (!child) {
        continue;
      }
      const QString haystack =
          (child->text(0) + " " + child->text(1) + " " + child->text(2)).toLower();
      const bool match = filter.isEmpty() || haystack.contains(filter);
      child->setHidden(!match);
      anyVisible = anyVisible || match;
    }

    sectionItem->setHidden(!filter.isEmpty() && !anyVisible);
    sectionItem->setExpanded(true);
  }
}

void NMHotkeysDialog::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
  Q_UNUSED(column);

  // Only allow editing child items (not section headers)
  if (!item || !item->parent()) {
    return;
  }

  // Check if this entry is customizable
  QString actionId = item->data(0, Qt::UserRole).toString();
  if (m_entries.contains(actionId) && !m_entries[actionId].isCustomizable) {
    NMMessageDialog::showInfo(this, tr("Cannot Modify"), tr("This shortcut cannot be customized."));
    return;
  }

  // Record new shortcut
  ShortcutRecorder recorder(this);
  if (recorder.exec() == QDialog::Accepted) {
    QString newShortcut = recorder.sequence().toString(QKeySequence::NativeText);
    setShortcutForItem(item, newShortcut);
  }
}

void NMHotkeysDialog::onRecordShortcut() {
  auto items = m_tree->selectedItems();
  if (items.isEmpty() || !items.first()->parent()) {
    return;
  }

  auto* item = items.first();
  QString actionId = item->data(0, Qt::UserRole).toString();

  if (m_entries.contains(actionId) && !m_entries[actionId].isCustomizable) {
    NMMessageDialog::showInfo(this, tr("Cannot Modify"), tr("This shortcut cannot be customized."));
    return;
  }

  ShortcutRecorder recorder(this);
  if (recorder.exec() == QDialog::Accepted) {
    QString newShortcut = recorder.sequence().toString(QKeySequence::NativeText);
    setShortcutForItem(item, newShortcut);
  }
}

void NMHotkeysDialog::onResetToDefault() {
  auto items = m_tree->selectedItems();
  if (items.isEmpty() || !items.first()->parent()) {
    return;
  }

  auto* item = items.first();
  QString actionId = item->data(0, Qt::UserRole).toString();

  if (m_entries.contains(actionId)) {
    QString defaultShortcut = m_entries[actionId].defaultShortcut;
    setShortcutForItem(item, defaultShortcut);

    // Mark as not modified
    m_entries[actionId].isModified = false;
    QFont normalFont = item->font(1);
    normalFont.setItalic(false);
    item->setFont(1, normalFont);
  }
}

void NMHotkeysDialog::onResetAllToDefaults() {
  auto result = NMMessageDialog::showQuestion(
      this, tr("Reset All Shortcuts"),
      tr("Are you sure you want to reset all shortcuts to their defaults?\n"
         "This cannot be undone."),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
    it->shortcut = it->defaultShortcut;
    it->isModified = false;

    if (auto* item = m_itemLookup.value(it->id)) {
      item->setText(1, it->defaultShortcut);
      QFont normalFont = item->font(1);
      normalFont.setItalic(false);
      item->setFont(1, normalFont);
    }
  }

  updateConflictWarnings();
}

void NMHotkeysDialog::onExportClicked() {
  QString filePath = QFileDialog::getSaveFileName(this, tr("Export Hotkeys"), QString(),
                                                  tr("JSON Files (*.json)"));

  if (filePath.isEmpty()) {
    return;
  }

  if (exportToFile(filePath)) {
    NMMessageDialog::showInfo(this, tr("Export Successful"),
                              tr("Hotkeys have been exported to:\n%1").arg(filePath));
  } else {
    NMMessageDialog::showError(this, tr("Export Failed"),
                               tr("Failed to export hotkeys to the selected file."));
  }
}

void NMHotkeysDialog::onImportClicked() {
  QString filePath = QFileDialog::getOpenFileName(this, tr("Import Hotkeys"), QString(),
                                                  tr("JSON Files (*.json)"));

  if (filePath.isEmpty()) {
    return;
  }

  if (importFromFile(filePath)) {
    NMMessageDialog::showInfo(this, tr("Import Successful"),
                              tr("Hotkeys have been imported successfully."));
  } else {
    NMMessageDialog::showError(this, tr("Import Failed"),
                               tr("Failed to import hotkeys from the selected file."));
  }
}

void NMHotkeysDialog::onApplyClicked() {
  // Emit changes for each modified entry
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    if (it->isModified) {
      emit hotkeyChanged(it->id, it->shortcut);
    }
  }

  NMMessageDialog::showInfo(this, tr("Changes Applied"),
                            tr("Shortcut changes have been applied.\n"
                               "Note: Some changes may require restarting the application."));
}

void NMHotkeysDialog::setShortcutForItem(QTreeWidgetItem* item, const QString& shortcut) {
  QString actionId = item->data(0, Qt::UserRole).toString();

  if (!m_entries.contains(actionId)) {
    return;
  }

  // Update the entry
  m_entries[actionId].shortcut = shortcut;
  m_entries[actionId].isModified = (shortcut != m_entries[actionId].defaultShortcut);

  // Update the tree item
  item->setText(1, shortcut);

  // Mark as modified with italic font
  QFont font = item->font(1);
  font.setItalic(m_entries[actionId].isModified);
  item->setFont(1, font);

  // Check for conflicts
  updateConflictWarnings();

  // Emit change signal
  emit hotkeyChanged(actionId, shortcut);
}

void NMHotkeysDialog::updateConflictWarnings() {
  auto conflicts = detectConflicts();

  if (conflicts.isEmpty()) {
    m_conflictLabel->setVisible(false);
    return;
  }

  QString warningText = tr("Conflicts detected: ");
  QStringList conflictStrings;
  for (const auto& conflict : conflicts) {
    conflictStrings.append(tr("'%1' and '%2' both use %3")
                               .arg(conflict.action1Name)
                               .arg(conflict.action2Name)
                               .arg(conflict.shortcut));
  }
  warningText += conflictStrings.join("; ");

  m_conflictLabel->setText(warningText);
  m_conflictLabel->setVisible(true);

  highlightConflicts();
}

void NMHotkeysDialog::highlightConflicts() {
  // Reset all highlights first
  for (auto* item : m_itemLookup) {
    item->setBackground(1, QBrush());
  }

  // Highlight conflicting items
  auto conflicts = detectConflicts();
  const auto& palette = NMStyleManager::instance().palette();
  QColor conflictColor = palette.bgMedium;
  conflictColor.setRed(qMin(255, conflictColor.red() + 60));

  for (const auto& conflict : conflicts) {
    if (auto* item1 = m_itemLookup.value(conflict.actionId1)) {
      item1->setBackground(1, QBrush(conflictColor));
    }
    if (auto* item2 = m_itemLookup.value(conflict.actionId2)) {
      item2->setBackground(1, QBrush(conflictColor));
    }

    emit conflictDetected(conflict);
  }
}

QList<NMHotkeyEntry> NMHotkeysDialog::getModifiedEntries() const {
  QList<NMHotkeyEntry> modified;
  for (const auto& entry : m_entries) {
    if (entry.isModified) {
      modified.append(entry);
    }
  }
  return modified;
}

QList<NMHotkeyConflict> NMHotkeysDialog::detectConflicts() const {
  QList<NMHotkeyConflict> conflicts;

  // Build shortcut -> action map
  QHash<QString, QList<QString>> shortcutToActions;
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    if (!it->shortcut.isEmpty()) {
      shortcutToActions[it->shortcut].append(it->id);
    }
  }

  // Find conflicts
  for (auto it = shortcutToActions.constBegin(); it != shortcutToActions.constEnd(); ++it) {
    if (it->size() > 1) {
      for (int i = 0; i < it->size() - 1; ++i) {
        for (int j = i + 1; j < it->size(); ++j) {
          NMHotkeyConflict conflict;
          conflict.shortcut = it.key();
          conflict.actionId1 = it.value()[i];
          conflict.actionId2 = it.value()[j];
          conflict.action1Name = m_entries[conflict.actionId1].action;
          conflict.action2Name = m_entries[conflict.actionId2].action;
          conflicts.append(conflict);
        }
      }
    }
  }

  return conflicts;
}

bool NMHotkeysDialog::exportToFile(const QString& filePath) const {
  QJsonObject root;
  QJsonArray entriesArray;

  for (const auto& entry : m_entries) {
    QJsonObject entryObj;
    entryObj["id"] = entry.id;
    entryObj["shortcut"] = entry.shortcut;
    entriesArray.append(entryObj);
  }

  root["version"] = 1;
  root["hotkeys"] = entriesArray;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  return true;
}

bool NMHotkeysDialog::importFromFile(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError) {
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray entriesArray = root["hotkeys"].toArray();

  for (const auto& entryVal : entriesArray) {
    QJsonObject entryObj = entryVal.toObject();
    QString id = entryObj["id"].toString();
    QString shortcut = entryObj["shortcut"].toString();

    if (m_entries.contains(id)) {
      m_entries[id].shortcut = shortcut;
      m_entries[id].isModified = (shortcut != m_entries[id].defaultShortcut);

      if (auto* item = m_itemLookup.value(id)) {
        item->setText(1, shortcut);
        QFont font = item->font(1);
        font.setItalic(m_entries[id].isModified);
        item->setFont(1, font);
      }
    }
  }

  updateConflictWarnings();
  return true;
}

QString NMHotkeysDialog::recordKeySequence() {
  ShortcutRecorder recorder(this);
  if (recorder.exec() == QDialog::Accepted) {
    return recorder.sequence().toString(QKeySequence::NativeText);
  }
  return QString();
}

} // namespace NovelMind::editor::qt

#include "nm_hotkeys_dialog.moc"
