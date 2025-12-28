#pragma once

/**
 * @file nm_hierarchy_panel.hpp
 * @brief Hierarchy panel for scene object tree view
 *
 * Displays the scene hierarchy as a tree:
 * - Scene layers
 * - Objects with parent-child relationships
 * - Selection synchronization
 * - Drag-and-drop (Phase 2+)
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QToolBar>
#include <QTreeWidget>

class QLineEdit;
class QComboBox;

namespace NovelMind::editor::qt {

/**
 * @brief Tree widget for scene hierarchy
 */
class NMHierarchyTree : public QTreeWidget {
  Q_OBJECT

public:
  explicit NMHierarchyTree(QWidget *parent = nullptr);

  void setScene(class NMSceneGraphicsScene *scene);
  [[nodiscard]] class NMSceneGraphicsScene *scene() const { return m_scene; }

  void setSceneViewPanel(class NMSceneViewPanel *panel) {
    m_sceneViewPanel = panel;
  }
  [[nodiscard]] class NMSceneViewPanel *sceneViewPanel() const {
    return m_sceneViewPanel;
  }

  /**
   * @brief Clear and rebuild the tree
   */
  void refresh();

  /**
   * @brief Set filter text for searching objects
   */
  void setFilterText(const QString &text);

  /**
   * @brief Set type filter
   */
  void
  setTypeFilter(int typeIndex); // -1 = all, or NMSceneObjectType enum value

  /**
   * @brief Set tag filter
   */
  void setTagFilter(const QString &tag);

signals:
  void itemSelected(const QString &objectId);
  void itemDoubleClicked(const QString &objectId);

protected:
  void selectionChanged(const QItemSelection &selected,
                        const QItemSelection &deselected) override;
  void dropEvent(QDropEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
  void onItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onItemChanged(QTreeWidgetItem *item, int column);

private:
  bool canDropOn(QTreeWidgetItem *dragItem, QTreeWidgetItem *dropItem) const;
  QString getObjectId(QTreeWidgetItem *item) const;
  bool isLayerItem(QTreeWidgetItem *item) const;
  bool passesFilters(class NMSceneObject *obj) const;

  class NMSceneGraphicsScene *m_scene = nullptr;
  class NMSceneViewPanel *m_sceneViewPanel = nullptr;
  QString m_filterText;
  int m_typeFilter = -1; // -1 = all types
  QString m_tagFilter;
};

/**
 * @brief Hierarchy panel for scene structure
 */
class NMHierarchyPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMHierarchyPanel(QWidget *parent = nullptr);
  ~NMHierarchyPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  [[nodiscard]] NMHierarchyTree *hierarchyTree() const { return m_tree; }

  /**
   * @brief Refresh the hierarchy display
   */
  void refresh();

  /**
   * @brief Select an item by object ID
   */
  void selectObject(const QString &objectId);

  void setScene(class NMSceneGraphicsScene *scene);
  void setSceneViewPanel(class NMSceneViewPanel *panel);

signals:
  void objectSelected(const QString &objectId);
  void objectDoubleClicked(const QString &objectId);

private slots:
  void onRefresh();
  void onExpandAll();
  void onCollapseAll();
  void onBringForward();
  void onSendBackward();
  void onBringToFront();
  void onSendToBack();

private slots:
  void onFilterTextChanged(const QString &text);
  void onTypeFilterChanged(int index);
  void onTagFilterChanged(const QString &tag);

private:
  void setupToolBar();
  void setupContent();
  void adjustSelectedZ(int mode);

  NMHierarchyTree *m_tree = nullptr;
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  class NMSceneViewPanel *m_sceneViewPanel = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QComboBox *m_typeFilterCombo = nullptr;
  QLineEdit *m_tagFilterEdit = nullptr;
};

} // namespace NovelMind::editor::qt
