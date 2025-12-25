#pragma once

/**
 * @file nm_scene_dialogue_graph_panel.hpp
 * @brief Embedded dialogue graph editor for Scene Nodes (Visual-First workflow)
 *
 * Provides:
 * - Dialogue graph editing within a scene context
 * - Filtered node palette (dialogue, choice, wait, effect only)
 * - Visual distinction from main Story Graph
 * - Breadcrumb navigation
 * - Sync with Scene Node's embeddedDialogueNodes
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <atomic>

namespace NovelMind::editor::qt {

/**
 * @brief Filtered node palette for scene dialogue graphs
 *
 * Only shows node types appropriate for scene-level dialogue:
 * - Dialogue
 * - Choice
 * - Wait
 * - Effect (SetVariable, PlaySound, etc.)
 */
class NMSceneDialogueNodePalette : public QWidget {
  Q_OBJECT

public:
  explicit NMSceneDialogueNodePalette(QWidget *parent = nullptr);

signals:
  void nodeTypeSelected(const QString &nodeType);

private:
  void createNodeButton(const QString &nodeType, const QString &icon);
};

/**
 * @brief Graphics scene for scene dialogue graphs
 *
 * Similar to NMStoryGraphScene but restricted to dialogue node types.
 * Scene nodes cannot be placed in embedded dialogue graphs.
 */
class NMSceneDialogueGraphScene : public NMStoryGraphScene {
  Q_OBJECT

public:
  explicit NMSceneDialogueGraphScene(QObject *parent = nullptr);

  /**
   * @brief Check if a node type is allowed in scene dialogue graphs
   * @param nodeType The type of node to check
   * @return true if the node type is allowed
   */
  static bool isAllowedNodeType(const QString &nodeType);

  /**
   * @brief Add a node to the dialogue graph (with type validation)
   */
  NMGraphNodeItem *addNode(const QString &title, const QString &nodeType,
                           const QPointF &pos, uint64_t nodeId = 0,
                           const QString &nodeIdString = QString());
};

/**
 * @brief Embedded dialogue graph editor for Scene Nodes
 *
 * This panel opens when "Edit Dialogue Flow" is clicked on a Scene Node.
 * It provides a scoped graph editor for dialogue nodes within the scene.
 */
class NMSceneDialogueGraphPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMSceneDialogueGraphPanel(QWidget *parent = nullptr);
  ~NMSceneDialogueGraphPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Load a scene's embedded dialogue graph
   * @param sceneId The scene ID to load
   */
  void loadSceneDialogue(const QString &sceneId);

  /**
   * @brief Clear the current dialogue graph
   */
  void clear();

  /**
   * @brief Get the currently loaded scene ID
   * @return Scene ID, or empty string if none loaded
   */
  [[nodiscard]] QString currentSceneId() const { return m_currentSceneId; }

  /**
   * @brief Check if the dialogue graph has unsaved changes
   * @return true if there are unsaved changes
   */
  [[nodiscard]] bool hasUnsavedChanges() const {
    return m_hasUnsavedChanges.load(std::memory_order_relaxed);
  }

  /**
   * @brief Get the dialogue graph scene
   */
  [[nodiscard]] NMSceneDialogueGraphScene *graphScene() const {
    return m_dialogueScene;
  }

  /**
   * @brief Get the graph view
   */
  [[nodiscard]] NMStoryGraphView *graphView() const { return m_view; }

signals:
  /**
   * @brief Emitted when dialogue count changes
   * @param sceneId The scene ID
   * @param count New dialogue node count
   */
  void dialogueCountChanged(const QString &sceneId, int count);

  /**
   * @brief Emitted when the user wants to return to the Story Graph
   */
  void returnToStoryGraphRequested();

  /**
   * @brief Emitted when node is selected
   */
  void nodeSelected(const QString &nodeIdString);

private slots:
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();
  void onFitToGraph();
  void onAutoLayout();
  void onReturnToStoryGraph();
  void onNodeTypeSelected(const QString &nodeType);
  void onNodeAdded(uint64_t nodeId, const QString &nodeIdString,
                   const QString &nodeType);
  void onNodeDeleted(uint64_t nodeId);
  void onConnectionAdded(uint64_t fromNodeId, uint64_t toNodeId);
  void onConnectionDeleted(uint64_t fromNodeId, uint64_t toNodeId);
  void onRequestConnection(uint64_t fromNodeId, uint64_t toNodeId);
  void onDeleteSelected();
  void onNodesMoved(const QVector<GraphNodeMove> &moves);

private:
  void setupToolBar();
  void setupContent();
  void setupBreadcrumb();
  void updateBreadcrumb();
  void createNode(const QString &nodeType);
  void saveDialogueGraphToScene();
  void loadDialogueGraphFromScene();
  void updateDialogueCount();
  void markAsModified();

  // UI Components
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  QWidget *m_breadcrumbWidget = nullptr;
  QLabel *m_breadcrumbLabel = nullptr;
  QPushButton *m_returnButton = nullptr;
  NMSceneDialogueNodePalette *m_nodePalette = nullptr;

  // Graph Components
  NMSceneDialogueGraphScene *m_dialogueScene = nullptr;
  NMStoryGraphView *m_view = nullptr;
  NMStoryGraphMinimap *m_minimap = nullptr;

  // State
  QString m_currentSceneId;
  std::atomic<bool> m_hasUnsavedChanges{false};
  QHash<uint64_t, QString> m_nodeIdToString;
};

} // namespace NovelMind::editor::qt
