#pragma once

/**
 * @file nm_story_graph_panel.hpp
 * @brief Story Graph panel for node-based visual scripting
 *
 * Displays the story graph with:
 * - Node representation
 * - Connection lines
 * - Mini-map
 * - Viewport controls
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QToolBar>

namespace NovelMind::editor::qt {

class NMStoryGraphMinimap;

struct GraphNodeMove {
  uint64_t nodeId = 0;
  QPointF oldPos;
  QPointF newPos;
};

/**
 * @brief Graphics item representing a story graph node
 */
class NMGraphNodeItem : public QGraphicsItem {
public:
  enum { Type = QGraphicsItem::UserType + 1 };

  explicit NMGraphNodeItem(const QString &title, const QString &nodeType);

  int type() const override { return Type; }

  void setTitle(const QString &title);
  [[nodiscard]] QString title() const { return m_title; }

  void setNodeType(const QString &type);
  [[nodiscard]] QString nodeType() const { return m_nodeType; }

  void setNodeId(uint64_t id) { m_nodeId = id; }
  [[nodiscard]] uint64_t nodeId() const { return m_nodeId; }

  void setNodeIdString(const QString &id) { m_nodeIdString = id; }
  [[nodiscard]] QString nodeIdString() const { return m_nodeIdString; }

  void setSelected(bool selected);
  void setBreakpoint(bool hasBreakpoint);
  void setCurrentlyExecuting(bool isExecuting);
  void setEntry(bool isEntry);

  void setScriptPath(const QString &path) { m_scriptPath = path; }
  [[nodiscard]] QString scriptPath() const { return m_scriptPath; }

  void setDialogueSpeaker(const QString &speaker) { m_dialogueSpeaker = speaker; }
  [[nodiscard]] QString dialogueSpeaker() const { return m_dialogueSpeaker; }

  void setDialogueText(const QString &text) { m_dialogueText = text; }
  [[nodiscard]] QString dialogueText() const { return m_dialogueText; }

  void setChoiceOptions(const QStringList &choices) { m_choiceOptions = choices; }
  [[nodiscard]] QStringList choiceOptions() const { return m_choiceOptions; }

  // Scene Node specific properties
  void setSceneId(const QString &id) { m_sceneId = id; }
  [[nodiscard]] QString sceneId() const { return m_sceneId; }

  void setHasEmbeddedDialogue(bool embedded) { m_hasEmbeddedDialogue = embedded; }
  [[nodiscard]] bool hasEmbeddedDialogue() const { return m_hasEmbeddedDialogue; }

  void setDialogueCount(int count) { m_dialogueCount = count; }
  [[nodiscard]] int dialogueCount() const { return m_dialogueCount; }

  void setThumbnailPath(const QString &path) { m_thumbnailPath = path; }
  [[nodiscard]] QString thumbnailPath() const { return m_thumbnailPath; }

  [[nodiscard]] bool isSceneNode() const {
    return m_nodeType.compare("Scene", Qt::CaseInsensitive) == 0;
  }

  [[nodiscard]] bool hasBreakpoint() const { return m_hasBreakpoint; }
  [[nodiscard]] bool isCurrentlyExecuting() const {
    return m_isCurrentlyExecuting;
  }
  [[nodiscard]] bool isEntry() const { return m_isEntry; }

  [[nodiscard]] QPointF inputPortPosition() const;
  [[nodiscard]] QPointF outputPortPosition() const;
  [[nodiscard]] bool hitTestInputPort(const QPointF &scenePos) const;
  [[nodiscard]] bool hitTestOutputPort(const QPointF &scenePos) const;

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

protected:
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant &value) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
  QString m_title;
  QString m_nodeType;
  uint64_t m_nodeId = 0;
  QString m_nodeIdString;
  QString m_scriptPath;
  QString m_dialogueSpeaker;
  QString m_dialogueText;
  QStringList m_choiceOptions;
  bool m_isSelected = false;
  bool m_hasBreakpoint = false;
  bool m_isCurrentlyExecuting = false;
  bool m_isEntry = false;

  // Scene Node specific properties
  QString m_sceneId;
  bool m_hasEmbeddedDialogue = false;
  int m_dialogueCount = 0;
  QString m_thumbnailPath;

  static constexpr qreal NODE_WIDTH = 200;
  static constexpr qreal NODE_HEIGHT = 80;
  static constexpr qreal SCENE_NODE_HEIGHT = 100; // Larger for scene nodes
  static constexpr qreal CORNER_RADIUS = 8;
  static constexpr qreal PORT_RADIUS = 6;
};

/**
 * @brief Graphics item representing a connection between nodes
 */
class NMGraphConnectionItem : public QGraphicsItem {
public:
  enum { Type = QGraphicsItem::UserType + 2 };

  NMGraphConnectionItem(NMGraphNodeItem *startNode, NMGraphNodeItem *endNode);

  int type() const override { return Type; }

  void updatePath();

  [[nodiscard]] NMGraphNodeItem *startNode() const { return m_startNode; }
  [[nodiscard]] NMGraphNodeItem *endNode() const { return m_endNode; }

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

private:
  NMGraphNodeItem *m_startNode;
  NMGraphNodeItem *m_endNode;
  QPainterPath m_path;
};

/**
 * @brief Graphics scene for the story graph
 */
class NMStoryGraphScene : public QGraphicsScene {
  Q_OBJECT

public:
  explicit NMStoryGraphScene(QObject *parent = nullptr);

  /**
   * @brief Add a node to the graph
   */
  NMGraphNodeItem *addNode(const QString &title, const QString &nodeType,
                           const QPointF &pos, uint64_t nodeId = 0,
                           const QString &nodeIdString = QString());

  /**
   * @brief Add a connection between nodes
   */
  NMGraphConnectionItem *addConnection(NMGraphNodeItem *from,
                                       NMGraphNodeItem *to);
  NMGraphConnectionItem *addConnection(uint64_t fromNodeId,
                                       uint64_t toNodeId);

  /**
   * @brief Remove a node and its connections
   */
  void removeNode(NMGraphNodeItem *node);

  /**
   * @brief Remove a connection
   */
  void removeConnection(NMGraphConnectionItem *connection);
  bool removeConnection(uint64_t fromNodeId, uint64_t toNodeId);

  /**
   * @brief Clear all nodes and connections
   */
  void clearGraph();

  /**
   * @brief Get all nodes
   */
  [[nodiscard]] const QList<NMGraphNodeItem *> &nodes() const {
    return m_nodes;
  }

  [[nodiscard]] NMGraphNodeItem *findNode(uint64_t nodeId) const;
  [[nodiscard]] bool hasConnection(uint64_t fromNodeId,
                                   uint64_t toNodeId) const;

  /**
   * @brief Get all connections
   */
  [[nodiscard]] const QList<NMGraphConnectionItem *> &connections() const {
    return m_connections;
  }

  /**
   * @brief Find connections attached to a node
   */
  QList<NMGraphConnectionItem *>
  findConnectionsForNode(NMGraphNodeItem *node) const;

  void requestEntryNode(const QString &nodeIdString);

  /**
   * @brief Check if adding a connection would create a cycle
   * @param fromNodeId Source node
   * @param toNodeId Target node
   * @return true if adding this connection would create a cycle
   */
  [[nodiscard]] bool wouldCreateCycle(uint64_t fromNodeId,
                                       uint64_t toNodeId) const;

  /**
   * @brief Detect all cycles in the graph
   * @return List of node ID lists, each representing a cycle
   */
  [[nodiscard]] QList<QList<uint64_t>> detectCycles() const;

  /**
   * @brief Find all nodes unreachable from entry nodes
   * @return List of unreachable node IDs
   */
  [[nodiscard]] QList<uint64_t> findUnreachableNodes() const;

  /**
   * @brief Validate the graph structure
   * @return List of validation error messages
   */
  [[nodiscard]] QStringList validateGraph() const;

signals:
  void nodeAdded(uint64_t nodeId, const QString &nodeIdString,
                 const QString &nodeType);
  void nodeDeleted(uint64_t nodeId);
  void connectionAdded(uint64_t fromNodeId, uint64_t toNodeId);
  void connectionDeleted(uint64_t fromNodeId, uint64_t toNodeId);
  void entryNodeRequested(const QString &nodeIdString);
  void deleteSelectionRequested();
  void nodesMoved(const QVector<GraphNodeMove> &moves);

protected:
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
  QList<NMGraphNodeItem *> m_nodes;
  QList<NMGraphConnectionItem *> m_connections;
  QHash<uint64_t, NMGraphNodeItem *> m_nodeLookup;
  uint64_t m_nextNodeId = 1;
  QHash<uint64_t, QPointF> m_dragStartPositions;
  bool m_isDraggingNodes = false;
};

/**
 * @brief Graphics view for story graph with pan/zoom
 */
class NMStoryGraphView : public QGraphicsView {
  Q_OBJECT

public:
  explicit NMStoryGraphView(QWidget *parent = nullptr);

  void setZoomLevel(qreal zoom);
  [[nodiscard]] qreal zoomLevel() const { return m_zoomLevel; }

  void centerOnGraph();

  void setConnectionModeEnabled(bool enabled);
  [[nodiscard]] bool isConnectionModeEnabled() const {
    return m_connectionModeEnabled;
  }

  void setConnectionDrawingMode(bool enabled);
  [[nodiscard]] bool isConnectionDrawingMode() const {
    return m_isDrawingConnection;
  }

  void emitNodeClicked(uint64_t nodeId) { emit nodeClicked(nodeId); }

signals:
  void zoomChanged(qreal newZoom);
  void nodeClicked(uint64_t nodeId);
  void nodeDoubleClicked(uint64_t nodeId);
  void requestConnection(uint64_t fromNodeId, uint64_t toNodeId);

protected:
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
  qreal m_zoomLevel = 1.0;
  bool m_isPanning = false;
  QPoint m_lastPanPoint;
  bool m_isDrawingConnection = false;
  bool m_connectionModeEnabled = false;
  NMGraphNodeItem *m_connectionStartNode = nullptr;
  QPointF m_connectionEndPoint;
};

/**
 * @brief Node creation palette for adding new nodes to the graph
 */
class NMNodePalette : public QWidget {
  Q_OBJECT

public:
  explicit NMNodePalette(QWidget *parent = nullptr);

signals:
  void nodeTypeSelected(const QString &nodeType);

private:
  void createNodeButton(const QString &nodeType, const QString &icon);
};

/**
 * @brief Story Graph panel for visual scripting
 */
class NMStoryGraphPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMStoryGraphPanel(QWidget *parent = nullptr);
  ~NMStoryGraphPanel() override;

  struct LayoutNode {
    QPointF position;
    QString type;
    QString scriptPath;
    QString title;
    QString speaker;
    QString dialogueText;
    QStringList choices;
    // Scene Node specific properties
    QString sceneId;
    bool hasEmbeddedDialogue = false;
    int dialogueCount = 0;
    QString thumbnailPath;
  };

  void onInitialize() override;
  void onUpdate(double deltaTime) override;
  Q_SLOT void rebuildFromProjectScripts();

  [[nodiscard]] NMStoryGraphScene *graphScene() const { return m_scene; }
  [[nodiscard]] NMStoryGraphView *graphView() const { return m_view; }
  [[nodiscard]] NMStoryGraphMinimap *minimap() const { return m_minimap; }
  [[nodiscard]] NMGraphNodeItem *findNodeById(uint64_t nodeId) const;

  /**
   * @brief Find node by string ID
   */
  NMGraphNodeItem *findNodeByIdString(const QString &id) const;

  void applyNodePropertyChange(const QString &nodeIdString,
                               const QString &propertyName,
                               const QString &newValue);

  /**
   * @brief Create a new node at the view center
   */
  void createNode(const QString &nodeType);

  /**
   * @brief Navigate to a node and highlight it
   * @param nodeIdString The node ID to navigate to
   * @return true if navigation succeeded, false if node not found
   */
  bool navigateToNode(const QString &nodeIdString);

signals:
  void nodeSelected(const QString &nodeIdString);
  void nodeActivated(const QString &nodeIdString);
  void scriptNodeRequested(const QString &scriptPath);
  // Scene Node specific signals
  void sceneNodeDoubleClicked(const QString &sceneId);
  void editSceneLayoutRequested(const QString &sceneId);
  void editDialogueFlowRequested(const QString &sceneId);
  void openSceneScriptRequested(const QString &sceneId, const QString &scriptPath);

private slots:
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();
  void onFitToGraph();
  void onAutoLayout();
  void onCurrentNodeChanged(const QString &nodeId);
  void onBreakpointsChanged();
  void onNodeTypeSelected(const QString &nodeType);
  void onNodeClicked(uint64_t nodeId);
  void onNodeDoubleClicked(uint64_t nodeId);
  void onNodeAdded(uint64_t nodeId, const QString &nodeIdString,
                   const QString &nodeType);
  void onNodeDeleted(uint64_t nodeId);
  void onConnectionAdded(uint64_t fromNodeId, uint64_t toNodeId);
  void onConnectionDeleted(uint64_t fromNodeId, uint64_t toNodeId);
  void onRequestConnection(uint64_t fromNodeId, uint64_t toNodeId);
  void onDeleteSelected();
  void onNodesMoved(const QVector<GraphNodeMove> &moves);
  void onEntryNodeRequested(const QString &nodeIdString);

private:
  void setupToolBar();
  void setupContent();
  void setupNodePalette();
  void updateNodeBreakpoints();
  void updateCurrentNode(const QString &nodeId);

  NMStoryGraphScene *m_scene = nullptr;
  NMStoryGraphView *m_view = nullptr;
  NMStoryGraphMinimap *m_minimap = nullptr;
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  NMNodePalette *m_nodePalette = nullptr;
  QString m_currentExecutingNode;

  QHash<QString, LayoutNode> m_layoutNodes;
  QHash<uint64_t, QString> m_nodeIdToString;
  QString m_layoutEntryScene;
  bool m_isRebuilding = false;
  bool m_markNextNodeAsEntry = false;
};

} // namespace NovelMind::editor::qt
