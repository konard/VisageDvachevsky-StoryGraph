#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QAction>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>

namespace NovelMind::editor::qt {

// ============================================================================
// NMHierarchyTree
// ============================================================================

NMHierarchyTree::NMHierarchyTree(QWidget *parent) : QTreeWidget(parent) {
  setColumnCount(3);
  setHeaderLabels({tr("Name"), tr("V"), tr("L")});
  setHeaderHidden(false);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setDragEnabled(true); // Enable drag-drop for reparenting
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setAnimated(true);
  setIndentation(16);

  connect(this, &QTreeWidget::itemDoubleClicked, this,
          &NMHierarchyTree::onItemDoubleClicked);
  connect(this, &QTreeWidget::itemChanged, this,
          &NMHierarchyTree::onItemChanged);

  if (header()) {
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  }
}

void NMHierarchyTree::setScene(NMSceneGraphicsScene *scene) {
  m_scene = scene;
  refresh();
}

/**
 * @brief Rebuild the entire hierarchy tree
 *
 * This clears and rebuilds the tree from scratch. For incremental updates
 * during gizmo operations (position/scale changes), use updateObjectItem()
 * instead to maintain 60 FPS performance.
 */
void NMHierarchyTree::refresh() {
  QString previouslySelected;
  if (!selectedItems().isEmpty()) {
    previouslySelected =
        selectedItems().first()->data(0, Qt::UserRole).toString();
  }

  QSignalBlocker blocker(this);
  clear();

  // PERF-2: Clear the object-to-item map when rebuilding tree
  m_objectToItemMap.clear();

  if (!m_scene) {
    return;
  }

  auto *rootItem = new QTreeWidgetItem(this);
  rootItem->setText(0, "Scene Objects");
  rootItem->setExpanded(true);

  QTreeWidgetItem *bgLayer = nullptr;
  QTreeWidgetItem *charLayer = nullptr;
  QTreeWidgetItem *uiLayer = nullptr;
  QTreeWidgetItem *effectLayer = nullptr;

  const auto objects = m_scene->sceneObjects();
  QList<NMSceneObject *> sorted = objects;
  std::sort(sorted.begin(), sorted.end(),
            [](const NMSceneObject *a, const NMSceneObject *b) {
              return a->zValue() < b->zValue();
            });

  for (NMSceneObject *object : sorted) {
    if (!object) {
      continue;
    }

    // Apply filters
    if (!passesFilters(object)) {
      continue;
    }

    QTreeWidgetItem *parentItem = rootItem;
    switch (object->objectType()) {
    case NMSceneObjectType::Background:
      if (!bgLayer) {
        bgLayer = new QTreeWidgetItem(rootItem);
        bgLayer->setText(0, "Backgrounds");
        bgLayer->setExpanded(true);
      }
      parentItem = bgLayer;
      break;
    case NMSceneObjectType::Character:
      if (!charLayer) {
        charLayer = new QTreeWidgetItem(rootItem);
        charLayer->setText(0, "Characters");
        charLayer->setExpanded(true);
      }
      parentItem = charLayer;
      break;
    case NMSceneObjectType::UI:
      if (!uiLayer) {
        uiLayer = new QTreeWidgetItem(rootItem);
        uiLayer->setText(0, "UI");
        uiLayer->setExpanded(true);
      }
      parentItem = uiLayer;
      break;
    case NMSceneObjectType::Effect:
      if (!effectLayer) {
        effectLayer = new QTreeWidgetItem(rootItem);
        effectLayer->setText(0, "Effects");
        effectLayer->setExpanded(true);
      }
      parentItem = effectLayer;
      break;
    }

    auto *item = new QTreeWidgetItem(parentItem);
    item->setText(0, object->name().isEmpty() ? object->id() : object->name());
    item->setData(0, Qt::UserRole, object->id());
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(1, object->isVisible() ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(2, object->isLocked() ? Qt::Checked : Qt::Unchecked);

    // PERF-2: Add to object-to-item map for O(1) lookup
    m_objectToItemMap.insert(object->id(), item);

    const bool isRuntime = object->id().startsWith("runtime_");
    if (isRuntime) {
      QFont font = item->font(0);
      font.setItalic(true);
      item->setFont(0, font);
      item->setForeground(0,
                          NMStyleManager::instance().palette().accentPrimary);
      item->setToolTip(0, tr("Runtime preview object (read-only)"));
      item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    }
  }

  if (!previouslySelected.isEmpty()) {
    // PERF-2: Use O(1) lookup from map instead of O(n) iterator
    QTreeWidgetItem *prevItem = m_objectToItemMap.value(previouslySelected);
    if (prevItem) {
      setCurrentItem(prevItem);
      prevItem->setSelected(true);
      scrollToItem(prevItem);
    }
  }
}

/**
 * @brief Update a single object's tree item (PERF-2 optimization)
 *
 * This provides O(1) update for individual object property changes instead of
 * rebuilding the entire tree. Use this during gizmo operations to maintain
 * 60 FPS performance. Only updates name, visibility, and lock state - does
 * not handle reparenting or object type changes (use refresh() for those).
 *
 * @param objectId ID of the object to update
 */
void NMHierarchyTree::updateObjectItem(const QString &objectId) {
  if (!m_scene || objectId.isEmpty()) {
    return;
  }

  // PERF-2: O(1) lookup using hash map
  QTreeWidgetItem *item = m_objectToItemMap.value(objectId);
  if (!item) {
    return; // Object not in tree (filtered out or doesn't exist)
  }

  NMSceneObject *object = m_scene->findSceneObject(objectId);
  if (!object) {
    // Object was deleted - remove from map and tree
    m_objectToItemMap.remove(objectId);
    delete item;
    return;
  }

  // Block signals to prevent triggering onItemChanged during update
  QSignalBlocker blocker(this);

  // Update name if changed
  const QString displayName =
      object->name().isEmpty() ? object->id() : object->name();
  if (item->text(0) != displayName) {
    item->setText(0, displayName);
  }

  // Update visibility if changed
  const Qt::CheckState expectedVisible =
      object->isVisible() ? Qt::Checked : Qt::Unchecked;
  if (item->checkState(1) != expectedVisible) {
    item->setCheckState(1, expectedVisible);
  }

  // Update lock state if changed
  const Qt::CheckState expectedLocked =
      object->isLocked() ? Qt::Checked : Qt::Unchecked;
  if (item->checkState(2) != expectedLocked) {
    item->setCheckState(2, expectedLocked);
  }
}

/**
 * @brief Find tree item for a specific object (O(1) lookup)
 *
 * PERF-2 optimization: Uses hash map for constant-time lookup instead
 * of iterating through all tree items.
 *
 * @param objectId ID of the object
 * @return Tree item or nullptr if not found
 */
QTreeWidgetItem *
NMHierarchyTree::findTreeItemForObject(const QString &objectId) const {
  return m_objectToItemMap.value(objectId, nullptr);
}

void NMHierarchyTree::setFilterText(const QString &text) {
  m_filterText = text;
  refresh();
}

void NMHierarchyTree::setTypeFilter(int typeIndex) {
  m_typeFilter = typeIndex;
  refresh();
}

void NMHierarchyTree::setTagFilter(const QString &tag) {
  m_tagFilter = tag;
  refresh();
}

bool NMHierarchyTree::passesFilters(NMSceneObject *obj) const {
  if (!obj) {
    return false;
  }

  // Name/ID filter
  if (!m_filterText.isEmpty()) {
    const QString name = obj->name().isEmpty() ? obj->id() : obj->name();
    if (!name.contains(m_filterText, Qt::CaseInsensitive)) {
      return false;
    }
  }

  // Type filter
  if (m_typeFilter >= 0) {
    if (static_cast<int>(obj->objectType()) != m_typeFilter) {
      return false;
    }
  }

  // Tag filter
  if (!m_tagFilter.isEmpty()) {
    if (!obj->hasTag(m_tagFilter)) {
      return false;
    }
  }

  return true;
}

void NMHierarchyTree::selectionChanged(const QItemSelection &selected,
                                       const QItemSelection &deselected) {
  QTreeWidget::selectionChanged(selected, deselected);

  QList<QTreeWidgetItem *> selectedItems = this->selectedItems();
  if (!selectedItems.isEmpty()) {
    QString objectId = selectedItems.first()->data(0, Qt::UserRole).toString();
    if (!objectId.isEmpty()) {
      emit itemSelected(objectId);
    }
  }
}

void NMHierarchyTree::onItemDoubleClicked(QTreeWidgetItem *item,
                                          int /*column*/) {
  if (item) {
    QString objectId = item->data(0, Qt::UserRole).toString();
    if (!objectId.isEmpty()) {
      emit itemDoubleClicked(objectId);
    }
  }
}

void NMHierarchyTree::onItemChanged(QTreeWidgetItem *item, int column) {
  if (!m_scene || !item) {
    return;
  }

  const QString objectId = item->data(0, Qt::UserRole).toString();
  if (objectId.isEmpty()) {
    return;
  }

  // Skip runtime preview objects
  const bool isRuntime = objectId.startsWith("runtime_");
  if (isRuntime) {
    return;
  }

  if (column == 1) {
    const bool newVisible = (item->checkState(1) == Qt::Checked);
    auto *obj = m_scene->findSceneObject(objectId);
    if (!obj) {
      return;
    }
    const bool oldVisible = obj->isVisible();
    if (oldVisible == newVisible) {
      return;
    }

    // Use undo command if scene view panel is available
    if (m_sceneViewPanel) {
      auto *cmd = new ToggleObjectVisibilityCommand(m_sceneViewPanel, objectId,
                                                    oldVisible, newVisible);
      NMUndoManager::instance().pushCommand(cmd);
    } else {
      m_scene->setObjectVisible(objectId, newVisible);
    }
    return;
  }

  if (column == 2) {
    const bool newLocked = (item->checkState(2) == Qt::Checked);
    auto *obj = m_scene->findSceneObject(objectId);
    if (!obj) {
      return;
    }
    const bool oldLocked = obj->isLocked();
    if (oldLocked == newLocked) {
      return;
    }

    // Use undo command if scene view panel is available
    if (m_sceneViewPanel) {
      auto *cmd = new ToggleObjectLockedCommand(m_sceneViewPanel, objectId,
                                                oldLocked, newLocked);
      NMUndoManager::instance().pushCommand(cmd);
    } else {
      m_scene->setObjectLocked(objectId, newLocked);
    }
    return;
  }
}

void NMHierarchyTree::dragEnterEvent(QDragEnterEvent *event) {
  QTreeWidget::dragEnterEvent(event);
}

void NMHierarchyTree::dragMoveEvent(QDragMoveEvent *event) {
  QTreeWidgetItem *dropItem = itemAt(event->position().toPoint());
  QTreeWidgetItem *dragItem = currentItem();

  if (!dragItem || !dropItem || !canDropOn(dragItem, dropItem)) {
    event->ignore();
    return;
  }

  QTreeWidget::dragMoveEvent(event);
}

void NMHierarchyTree::dropEvent(QDropEvent *event) {
  QTreeWidgetItem *dragItem = currentItem();
  QTreeWidgetItem *dropItem = itemAt(event->position().toPoint());

  if (!dragItem || !dropItem || !m_scene || !m_sceneViewPanel) {
    event->ignore();
    return;
  }

  const QString dragObjectId = getObjectId(dragItem);
  const QString dropObjectId = getObjectId(dropItem);

  if (dragObjectId.isEmpty() || !canDropOn(dragItem, dropItem)) {
    event->ignore();
    return;
  }

  // Get old parent before reparenting
  auto *dragObj = m_scene->findSceneObject(dragObjectId);
  const QString oldParentId = dragObj ? dragObj->parentObjectId() : QString();

  // Determine new parent: if dropping on a layer, new parent is empty
  const QString newParentId = isLayerItem(dropItem) ? QString() : dropObjectId;

  // Create undo command for reparenting
  auto *cmd = new ReparentObjectCommand(m_sceneViewPanel, dragObjectId,
                                        oldParentId, newParentId);
  NMUndoManager::instance().pushCommand(cmd);

  // Prevent default drop behavior since we handle it via undo command
  event->setDropAction(Qt::IgnoreAction);
  event->accept();

  // Refresh the tree
  refresh();
}

QString NMHierarchyTree::getObjectId(QTreeWidgetItem *item) const {
  if (!item) {
    return QString();
  }
  return item->data(0, Qt::UserRole).toString();
}

bool NMHierarchyTree::isLayerItem(QTreeWidgetItem *item) const {
  if (!item) {
    return false;
  }
  const QString text = item->text(0);
  return text == "Scene Objects" || text == "Backgrounds" ||
         text == "Characters" || text == "UI" || text == "Effects";
}

bool NMHierarchyTree::canDropOn(QTreeWidgetItem *dragItem,
                                QTreeWidgetItem *dropItem) const {
  if (!dragItem || !dropItem || !m_scene) {
    return false;
  }

  const QString dragId = getObjectId(dragItem);
  const QString dropId = getObjectId(dropItem);

  if (dragId.isEmpty()) {
    return false;
  }

  // Can drop on layer items
  if (isLayerItem(dropItem)) {
    return true;
  }

  // Cannot drop on self
  if (dragId == dropId) {
    return false;
  }

  // Check for cycle: cannot drop on own descendant
  if (!dropId.isEmpty()) {
    QString checkId = dropId;
    while (!checkId.isEmpty()) {
      if (checkId == dragId) {
        return false; // Would create a cycle
      }
      auto *checkObj = m_scene->findSceneObject(checkId);
      if (!checkObj) {
        break;
      }
      checkId = checkObj->parentObjectId();
    }
  }

  return true;
}

void NMHierarchyTree::contextMenuEvent(QContextMenuEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());

  QMenu contextMenu(this);

  // Get selected object ID if any
  QString objectId;
  if (item) {
    objectId = getObjectId(item);
  }

  const bool hasValidSelection = !objectId.isEmpty() && m_scene;
  const bool isRuntime = objectId.startsWith("runtime_");
  const bool canEdit = hasValidSelection && !isRuntime;

  // Duplicate action
  // Note: Duplication requires selecting the object first and using internal
  // duplication logic. For now, we select the object and show a message that
  // the user can use Ctrl+D in the scene view to duplicate.
  QAction *duplicateAction = contextMenu.addAction(tr("Duplicate"));
  duplicateAction->setEnabled(canEdit && m_sceneViewPanel);
  duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  connect(duplicateAction, &QAction::triggered, this, [this, objectId]() {
    if (m_sceneViewPanel && m_scene) {
      // Select the object first so scene view's duplicate shortcut works
      m_sceneViewPanel->selectObjectById(objectId);
      // Note: Actual duplication is handled by scene view panel's internal
      // keyboard shortcut (Ctrl+D). User needs to press Ctrl+D in scene view.
      // TODO: Expose duplicateSelectedObject() as public API for full support.
    }
  });

  // Rename action
  QAction *renameAction = contextMenu.addAction(tr("Rename"));
  renameAction->setEnabled(canEdit);
  renameAction->setShortcut(QKeySequence(Qt::Key_F2));
  connect(renameAction, &QAction::triggered, this, [this, objectId]() {
    if (!m_scene) {
      return;
    }
    auto *obj = m_scene->findSceneObject(objectId);
    if (!obj) {
      return;
    }

    bool ok = false;
    QString newName =
        QInputDialog::getText(this, tr("Rename Object"), tr("New name:"),
                              QLineEdit::Normal, obj->name(), &ok);
    if (ok && !newName.isEmpty()) {
      obj->setName(newName);
      refresh();
    }
  });

  contextMenu.addSeparator();

  // Delete action
  QAction *deleteAction = contextMenu.addAction(tr("Delete"));
  deleteAction->setEnabled(canEdit && m_sceneViewPanel);
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this, [this, objectId]() {
    if (m_sceneViewPanel) {
      QMessageBox::StandardButton reply = QMessageBox::question(
          this, tr("Delete Object"), tr("Delete '%1'?").arg(objectId),
          QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        m_sceneViewPanel->deleteObject(objectId);
        refresh();
      }
    }
  });

  contextMenu.addSeparator();

  // Toggle visibility
  QAction *visibilityAction = contextMenu.addAction(tr("Toggle Visibility"));
  visibilityAction->setEnabled(canEdit);
  visibilityAction->setShortcut(QKeySequence(Qt::Key_H));
  connect(visibilityAction, &QAction::triggered, this, [this, objectId]() {
    if (!m_scene) {
      return;
    }
    auto *obj = m_scene->findSceneObject(objectId);
    if (obj) {
      m_scene->setObjectVisible(objectId, !obj->isVisible());
      refresh();
    }
  });

  // Toggle lock
  QAction *lockAction = contextMenu.addAction(tr("Toggle Lock"));
  lockAction->setEnabled(canEdit);
  connect(lockAction, &QAction::triggered, this, [this, objectId]() {
    if (!m_scene) {
      return;
    }
    auto *obj = m_scene->findSceneObject(objectId);
    if (obj) {
      m_scene->setObjectLocked(objectId, !obj->isLocked());
      refresh();
    }
  });

  contextMenu.addSeparator();

  // Isolate action (hide all others)
  QAction *isolateAction = contextMenu.addAction(tr("Isolate"));
  isolateAction->setEnabled(canEdit);
  connect(isolateAction, &QAction::triggered, this, [this, objectId]() {
    if (!m_scene) {
      return;
    }
    for (auto *obj : m_scene->sceneObjects()) {
      if (obj) {
        m_scene->setObjectVisible(obj->id(), obj->id() == objectId);
      }
    }
    refresh();
  });

  // Show all action
  QAction *showAllAction = contextMenu.addAction(tr("Show All"));
  showAllAction->setEnabled(m_scene != nullptr);
  connect(showAllAction, &QAction::triggered, this, [this]() {
    if (!m_scene) {
      return;
    }
    for (auto *obj : m_scene->sceneObjects()) {
      if (obj && !obj->id().startsWith("runtime_")) {
        m_scene->setObjectVisible(obj->id(), true);
      }
    }
    refresh();
  });

  contextMenu.addSeparator();

  // Frame selected (zoom to object)
  QAction *frameAction = contextMenu.addAction(tr("Frame Selected"));
  frameAction->setEnabled(canEdit && m_sceneViewPanel);
  frameAction->setShortcut(QKeySequence(Qt::Key_F));
  connect(frameAction, &QAction::triggered, this, [this, objectId]() {
    if (m_sceneViewPanel && m_scene) {
      // Select the object first
      m_sceneViewPanel->selectObjectById(objectId);
      // Center the view on the selected object
      auto *obj = m_scene->findSceneObject(objectId);
      if (obj && m_sceneViewPanel->graphicsView()) {
        m_sceneViewPanel->graphicsView()->centerOn(obj);
      }
    }
  });

  contextMenu.exec(event->globalPos());
}

// ============================================================================
// NMHierarchyPanel
// ============================================================================

NMHierarchyPanel::NMHierarchyPanel(QWidget *parent)
    : NMDockPanel(tr("Hierarchy"), parent) {
  setPanelId("Hierarchy");

  // Hierarchy needs width for tree item names and icons, height to show tree
  setMinimumPanelSize(220, 180);

  setupContent();
  setupToolBar();
}

NMHierarchyPanel::~NMHierarchyPanel() = default;

void NMHierarchyPanel::onInitialize() { refresh(); }

void NMHierarchyPanel::onUpdate(double /*deltaTime*/) {
  // No continuous update needed
}

void NMHierarchyPanel::refresh() {
  if (m_tree) {
    m_tree->refresh();
  }
}

/**
 * @brief Update a single object (PERF-2: incremental update)
 *
 * Use this instead of refresh() for property changes (visibility, lock,
 * name) during gizmo operations to maintain 60 FPS. This provides O(1)
 * update complexity vs O(n) for full refresh.
 *
 * @param objectId ID of the object to update
 */
void NMHierarchyPanel::updateObject(const QString &objectId) {
  if (m_tree) {
    m_tree->updateObjectItem(objectId);
  }
}

void NMHierarchyPanel::setScene(NMSceneGraphicsScene *scene) {
  if (m_tree) {
    m_tree->setScene(scene);
  }
}

void NMHierarchyPanel::setSceneViewPanel(NMSceneViewPanel *panel) {
  m_sceneViewPanel = panel;
  if (m_tree) {
    m_tree->setSceneViewPanel(panel);
  }
}

void NMHierarchyPanel::selectObject(const QString &objectId) {
  if (!m_tree)
    return;

  QSignalBlocker blocker(m_tree);

  if (objectId.isEmpty()) {
    m_tree->clearSelection();
    return;
  }

  const auto selected = m_tree->selectedItems();
  if (!selected.isEmpty() &&
      selected.first()->data(0, Qt::UserRole).toString() == objectId) {
    return;
  }

  // PERF-2: Use O(1) lookup instead of O(n) iterator
  QTreeWidgetItem *item = m_tree->findTreeItemForObject(objectId);
  if (item) {
    m_tree->clearSelection();
    item->setSelected(true);
    m_tree->scrollToItem(item);
  }
}

void NMHierarchyPanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("HierarchyToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  // Search filter
  m_toolBar->addWidget(new QLabel(tr("Search:"), m_toolBar));
  m_searchEdit = new QLineEdit(m_toolBar);
  m_searchEdit->setPlaceholderText(tr("Filter by name..."));
  m_searchEdit->setMaximumWidth(150);
  m_toolBar->addWidget(m_searchEdit);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &NMHierarchyPanel::onFilterTextChanged);

  m_toolBar->addSeparator();

  // Type filter
  m_toolBar->addWidget(new QLabel(tr("Type:"), m_toolBar));
  m_typeFilterCombo = new QComboBox(m_toolBar);
  m_typeFilterCombo->addItem(tr("All"));
  m_typeFilterCombo->addItem(tr("Background"));
  m_typeFilterCombo->addItem(tr("Character"));
  m_typeFilterCombo->addItem(tr("UI"));
  m_typeFilterCombo->addItem(tr("Effect"));
  m_typeFilterCombo->setMaximumWidth(120);
  m_toolBar->addWidget(m_typeFilterCombo);
  connect(m_typeFilterCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMHierarchyPanel::onTypeFilterChanged);

  m_toolBar->addSeparator();

  // Tag filter
  m_toolBar->addWidget(new QLabel(tr("Tag:"), m_toolBar));
  m_tagFilterEdit = new QLineEdit(m_toolBar);
  m_tagFilterEdit->setPlaceholderText(tr("Filter by tag..."));
  m_tagFilterEdit->setMaximumWidth(120);
  m_toolBar->addWidget(m_tagFilterEdit);
  connect(m_tagFilterEdit, &QLineEdit::textChanged, this,
          &NMHierarchyPanel::onTagFilterChanged);

  m_toolBar->addSeparator();

  QAction *actionRefresh = m_toolBar->addAction(tr("Refresh"));
  actionRefresh->setToolTip(tr("Refresh Hierarchy"));
  connect(actionRefresh, &QAction::triggered, this,
          &NMHierarchyPanel::onRefresh);

  m_toolBar->addSeparator();

  QAction *actionExpandAll = m_toolBar->addAction(tr("Expand All"));
  actionExpandAll->setToolTip(tr("Expand All Items"));
  connect(actionExpandAll, &QAction::triggered, this,
          &NMHierarchyPanel::onExpandAll);

  QAction *actionCollapseAll = m_toolBar->addAction(tr("Collapse All"));
  actionCollapseAll->setToolTip(tr("Collapse All Items"));
  connect(actionCollapseAll, &QAction::triggered, this,
          &NMHierarchyPanel::onCollapseAll);

  m_toolBar->addSeparator();

  QAction *actionBringForward = m_toolBar->addAction(tr("Up"));
  actionBringForward->setToolTip(tr("Move selected object forward"));
  connect(actionBringForward, &QAction::triggered, this,
          &NMHierarchyPanel::onBringForward);

  QAction *actionSendBackward = m_toolBar->addAction(tr("Down"));
  actionSendBackward->setToolTip(tr("Move selected object backward"));
  connect(actionSendBackward, &QAction::triggered, this,
          &NMHierarchyPanel::onSendBackward);

  QAction *actionBringToFront = m_toolBar->addAction(tr("Front"));
  actionBringToFront->setToolTip(tr("Bring selected object to front"));
  connect(actionBringToFront, &QAction::triggered, this,
          &NMHierarchyPanel::onBringToFront);

  QAction *actionSendToBack = m_toolBar->addAction(tr("Back"));
  actionSendToBack->setToolTip(tr("Send selected object to back"));
  connect(actionSendToBack, &QAction::triggered, this,
          &NMHierarchyPanel::onSendToBack);

  if (auto *layout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_toolBar);
  }
}

void NMHierarchyPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_tree = new NMHierarchyTree(m_contentWidget);

  connect(m_tree, &NMHierarchyTree::itemSelected, this,
          &NMHierarchyPanel::objectSelected);
  connect(m_tree, &NMHierarchyTree::itemDoubleClicked, this,
          &NMHierarchyPanel::objectDoubleClicked);

  layout->addWidget(m_tree);

  setContentWidget(m_contentWidget);
}

void NMHierarchyPanel::onRefresh() { refresh(); }

void NMHierarchyPanel::onExpandAll() {
  if (m_tree) {
    m_tree->expandAll();
  }
}

void NMHierarchyPanel::onCollapseAll() {
  if (m_tree) {
    m_tree->collapseAll();
  }
}

void NMHierarchyPanel::onBringForward() { adjustSelectedZ(1); }

void NMHierarchyPanel::onSendBackward() { adjustSelectedZ(-1); }

void NMHierarchyPanel::onBringToFront() { adjustSelectedZ(2); }

void NMHierarchyPanel::onSendToBack() { adjustSelectedZ(-2); }

void NMHierarchyPanel::adjustSelectedZ(int mode) {
  if (!m_tree) {
    return;
  }
  auto *scene = m_tree->scene();
  if (!scene) {
    return;
  }

  const auto items = m_tree->selectedItems();
  if (items.isEmpty()) {
    return;
  }

  const QString objectId = items.first()->data(0, Qt::UserRole).toString();
  if (objectId.isEmpty()) {
    return;
  }

  auto *obj = scene->findSceneObject(objectId);
  if (!obj) {
    return;
  }

  qreal oldZ = obj->zValue();
  qreal newZ = oldZ;
  qreal minZ = oldZ;
  qreal maxZ = oldZ;
  for (auto *other : scene->sceneObjects()) {
    if (!other) {
      continue;
    }
    minZ = std::min(minZ, other->zValue());
    maxZ = std::max(maxZ, other->zValue());
  }

  switch (mode) {
  case -2:
    newZ = minZ - 1.0;
    break;
  case -1:
    newZ = oldZ - 1.0;
    break;
  case 1:
    newZ = oldZ + 1.0;
    break;
  case 2:
    newZ = maxZ + 1.0;
    break;
  default:
    break;
  }

  if (qFuzzyCompare(oldZ + 1.0, newZ + 1.0)) {
    return;
  }

  if (m_sceneViewPanel) {
    m_sceneViewPanel->setObjectZOrder(objectId, newZ);
  } else {
    scene->setObjectZOrder(objectId, newZ);
  }
  refresh();
}

void NMHierarchyPanel::onFilterTextChanged(const QString &text) {
  if (m_tree) {
    m_tree->setFilterText(text);
  }
}

void NMHierarchyPanel::onTypeFilterChanged(int index) {
  if (m_tree) {
    // Index 0 = "All" (-1), other indices map to NMSceneObjectType enum
    m_tree->setTypeFilter(index - 1);
  }
}

void NMHierarchyPanel::onTagFilterChanged(const QString &tag) {
  if (m_tree) {
    m_tree->setTagFilter(tag);
  }
}

} // namespace NovelMind::editor::qt
