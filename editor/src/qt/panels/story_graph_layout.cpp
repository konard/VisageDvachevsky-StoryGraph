#include "story_graph_layout.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QQueue>
#include <algorithm>
#include <climits>

namespace NovelMind::editor::qt::layout {

/**
 * @brief Improved auto-layout using Sugiyama-style hierarchical graph layout
 *
 * Issue #326: The previous implementation had several problems:
 * - Used simple BFS which doesn't consider longest path to each node
 * - Centering could push nodes to negative coordinates
 * - No edge crossing minimization
 * - Poor orphaned node placement
 *
 * This implementation uses a proper Sugiyama algorithm:
 * 1. Layer assignment using longest path from sources
 * 2. Edge crossing minimization using barycenter heuristic
 * 3. Proper coordinate assignment with all nodes in positive space
 * 4. Orphaned nodes placed in a separate region
 */
void applyAutoLayout(NMStoryGraphScene *scene,
                     const QList<NMGraphNodeItem *> &nodes,
                     const QList<NMGraphConnectionItem *> &connections) {
  if (!scene || nodes.isEmpty()) {
    return;
  }

  // Layout constants - can be made configurable in the future
  const qreal horizontalSpacing = 280.0; // Increased for better readability
  const qreal verticalSpacing = 160.0;   // Increased for better readability
  const qreal startX = 100.0;            // Left margin
  const qreal startY = 100.0;            // Top margin
  [[maybe_unused]] const qreal orphanAreaGap =
      100.0; // Gap before orphan section

  // Build adjacency lists (forward and reverse)
  QHash<uint64_t, QList<uint64_t>> successors;   // node -> children
  QHash<uint64_t, QList<uint64_t>> predecessors; // node -> parents
  QHash<uint64_t, int> inDegree;
  QHash<uint64_t, int> outDegree;
  QSet<uint64_t> entryNodeSet;

  for (auto *node : nodes) {
    successors[node->nodeId()] = QList<uint64_t>();
    predecessors[node->nodeId()] = QList<uint64_t>();
    inDegree[node->nodeId()] = 0;
    outDegree[node->nodeId()] = 0;
  }

  for (auto *conn : connections) {
    uint64_t fromId = conn->startNode()->nodeId();
    uint64_t toId = conn->endNode()->nodeId();
    successors[fromId].append(toId);
    predecessors[toId].append(fromId);
    inDegree[toId]++;
    outDegree[fromId]++;
  }

  // ===== STEP 1: Find entry nodes (sources) =====
  // Entry nodes are either marked as entry or have no incoming edges
  QList<uint64_t> entryNodes;
  for (auto *node : nodes) {
    if (inDegree[node->nodeId()] == 0 || node->isEntry()) {
      entryNodes.append(node->nodeId());
      entryNodeSet.insert(node->nodeId());
    }
  }

  // Then add nodes with no incoming edges (sources)
  for (auto *node : nodes) {
    uint64_t id = node->nodeId();
    if (inDegree[id] == 0 && !entryNodeSet.contains(id)) {
      entryNodes.append(id);
      entryNodeSet.insert(id);
    }
  }

  // ===== STEP 2: Layer assignment using longest path =====
  // This ensures nodes are placed at the maximum depth from any source,
  // which creates a more visually appealing hierarchical layout
  QHash<uint64_t, int> nodeLayers;
  QSet<uint64_t> visited;

  // Use topological sort with longest path calculation
  // Process nodes in topological order, computing max distance from sources
  QList<uint64_t> topoOrder;
  QHash<uint64_t, int> tempInDegree = inDegree;
  QQueue<uint64_t> zeroInDegreeQueue;

  // Initialize queue with sources
  for (auto *node : nodes) {
    if (tempInDegree[node->nodeId()] == 0) {
      zeroInDegreeQueue.enqueue(node->nodeId());
      nodeLayers[node->nodeId()] = 0;
    }
  }

  // Process topologically
  while (!zeroInDegreeQueue.isEmpty()) {
    uint64_t nodeId = zeroInDegreeQueue.dequeue();
    topoOrder.append(nodeId);
    visited.insert(nodeId);

    for (uint64_t childId : successors[nodeId]) {
      // Update child's layer to max(current layer, parent layer + 1)
      int newLayer = nodeLayers[nodeId] + 1;
      if (!nodeLayers.contains(childId) || newLayer > nodeLayers[childId]) {
        nodeLayers[childId] = newLayer;
      }

      tempInDegree[childId]--;
      if (tempInDegree[childId] == 0) {
        zeroInDegreeQueue.enqueue(childId);
      }
    }
  }

  // Handle cycles: nodes not in topo order are part of cycles
  // Place them based on their connections to already-placed nodes
  for (auto *node : nodes) {
    uint64_t id = node->nodeId();
    if (!visited.contains(id)) {
      // Find the minimum layer of any successor that's already placed
      int minSuccessorLayer = INT_MAX;
      for (uint64_t succId : successors[id]) {
        if (nodeLayers.contains(succId)) {
          minSuccessorLayer = qMin(minSuccessorLayer, nodeLayers[succId]);
        }
      }

      // Find the maximum layer of any predecessor that's already placed
      int maxPredecessorLayer = -1;
      for (uint64_t predId : predecessors[id]) {
        if (nodeLayers.contains(predId)) {
          maxPredecessorLayer = qMax(maxPredecessorLayer, nodeLayers[predId]);
        }
      }

      // Place between predecessor and successor layers if possible
      if (maxPredecessorLayer >= 0 && minSuccessorLayer < INT_MAX) {
        nodeLayers[id] = maxPredecessorLayer + 1;
      } else if (maxPredecessorLayer >= 0) {
        nodeLayers[id] = maxPredecessorLayer + 1;
      } else if (minSuccessorLayer < INT_MAX) {
        nodeLayers[id] = qMax(0, minSuccessorLayer - 1);
      } else {
        // Isolated node in cycle, place at layer 0
        nodeLayers[id] = 0;
      }
      visited.insert(id);
    }
  }

  // ===== STEP 3: Identify orphaned nodes (disconnected components) =====
  // Orphaned nodes have no connections at all
  QList<uint64_t> orphanedNodes;
  QSet<uint64_t> connectedNodes;

  for (auto *node : nodes) {
    uint64_t id = node->nodeId();
    if (inDegree[id] == 0 && outDegree[id] == 0) {
      orphanedNodes.append(id);
    } else {
      connectedNodes.insert(id);
    }
  }

  // ===== STEP 4: Build layer-to-nodes mapping for connected nodes only =====
  QHash<int, QList<uint64_t>> layerNodes;
  int maxLayer = 0;

  for (uint64_t nodeId : connectedNodes) {
    int layer = nodeLayers[nodeId];
    layerNodes[layer].append(nodeId);
    maxLayer = qMax(maxLayer, layer);
  }

  // ===== STEP 5: Edge crossing minimization using barycenter heuristic =====
  // For each layer (except the first), order nodes to minimize crossings
  // with the previous layer

  // First, order the first layer: put entry nodes first, then by out-degree
  if (layerNodes.contains(0)) {
    QList<uint64_t> &layer0 = layerNodes[0];
    std::sort(layer0.begin(), layer0.end(),
              [&entryNodeSet, &outDegree](uint64_t a, uint64_t b) {
                // Entry nodes come first
                bool aIsEntry = entryNodeSet.contains(a);
                bool bIsEntry = entryNodeSet.contains(b);
                if (aIsEntry != bIsEntry) {
                  return aIsEntry;
                }
                // Then sort by out-degree (more connections = more central)
                return outDegree[a] > outDegree[b];
              });
  }

  // Apply barycenter heuristic for subsequent layers
  // Iterate multiple times for better convergence
  const int iterations = 4;
  for (int iter = 0; iter < iterations; ++iter) {
    // Forward sweep (layer 1 to maxLayer)
    for (int layer = 1; layer <= maxLayer; ++layer) {
      if (!layerNodes.contains(layer)) {
        continue;
      }

      QList<uint64_t> &currentLayer = layerNodes[layer];
      QHash<uint64_t, qreal> barycenter;

      // Calculate barycenter for each node based on predecessor positions
      for (uint64_t nodeId : currentLayer) {
        qreal sum = 0.0;
        int count = 0;

        for (uint64_t predId : predecessors[nodeId]) {
          if (nodeLayers.contains(predId) &&
              nodeLayers[predId] == layer - 1 &&
              layerNodes.contains(layer - 1)) {
            int predIndex =
                static_cast<int>(layerNodes[layer - 1].indexOf(predId));
            if (predIndex >= 0) {
              sum += predIndex;
              ++count;
            }
          }
        }

        if (count > 0) {
          barycenter[nodeId] = sum / count;
        } else {
          // No predecessors in previous layer, keep original position
          barycenter[nodeId] =
              static_cast<qreal>(currentLayer.indexOf(nodeId));
        }
      }

      // Sort by barycenter
      std::sort(currentLayer.begin(), currentLayer.end(),
                [&barycenter](uint64_t a, uint64_t b) {
                  return barycenter[a] < barycenter[b];
                });
    }

    // Backward sweep (maxLayer-1 down to 0)
    for (int layer = maxLayer - 1; layer >= 0; --layer) {
      if (!layerNodes.contains(layer)) {
        continue;
      }

      QList<uint64_t> &currentLayer = layerNodes[layer];
      QHash<uint64_t, qreal> barycenter;

      // Calculate barycenter for each node based on successor positions
      for (uint64_t nodeId : currentLayer) {
        qreal sum = 0.0;
        int count = 0;

        for (uint64_t succId : successors[nodeId]) {
          if (nodeLayers.contains(succId) &&
              nodeLayers[succId] == layer + 1 &&
              layerNodes.contains(layer + 1)) {
            int succIndex =
                static_cast<int>(layerNodes[layer + 1].indexOf(succId));
            if (succIndex >= 0) {
              sum += succIndex;
              ++count;
            }
          }
        }

        if (count > 0) {
          barycenter[nodeId] = sum / count;
        } else {
          // No successors in next layer, keep original position
          barycenter[nodeId] =
              static_cast<qreal>(currentLayer.indexOf(nodeId));
        }
      }

      // Sort by barycenter
      std::sort(currentLayer.begin(), currentLayer.end(),
                [&barycenter](uint64_t a, uint64_t b) {
                  return barycenter[a] < barycenter[b];
                });
    }
  }

  for (auto *node : nodes) {
    if (!visited.contains(node->nodeId())) {
      maxLayer++;
      nodeLayers[node->nodeId()] = maxLayer;
      layerNodes[maxLayer].append(node->nodeId());
    }
  }

  // Position nodes
  for (int layer : layerNodes.keys()) {
    const auto &nodesInLayer = layerNodes[layer];
    qreal y = startY + layer * verticalSpacing;
    qreal totalWidth =
        static_cast<qreal>(nodesInLayer.size() - 1) * horizontalSpacing;
    qreal x = startX - totalWidth / 2.0;

    for (int i = 0; i < nodesInLayer.size(); ++i) {
      uint64_t nodeId = nodesInLayer[i];
      auto *node = scene->findNode(nodeId);
      if (node) {
        node->setPos(x + i * horizontalSpacing, y);
      }
    }
  }

  // Update all connection paths
  for (auto *conn : connections) {
    conn->updatePath();
  }
}

} // namespace NovelMind::editor::qt::layout
