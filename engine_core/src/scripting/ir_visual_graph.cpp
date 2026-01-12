/**
 * @file ir_visual_graph.cpp
 * @brief Visual graph representation for IR
 */

#include "NovelMind/scripting/ir.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace NovelMind::scripting {

// ============================================================================
// VisualGraph Implementation
// ============================================================================

VisualGraph::VisualGraph() = default;
VisualGraph::~VisualGraph() = default;

void VisualGraph::fromIR(const IRGraph& ir) {
  m_nodes.clear();
  m_edges.clear();

  for (const auto* node : ir.getNodes()) {
    VisualGraphNode vnode;
    vnode.id = node->getId();
    vnode.type = node->getTypeName();
    vnode.displayName = node->getTypeName();
    vnode.x = node->getX();
    vnode.y = node->getY();

    for (const auto& port : node->getInputPorts()) {
      vnode.inputPorts.push_back({port.name, port.displayName});
    }
    for (const auto& port : node->getOutputPorts()) {
      vnode.outputPorts.push_back({port.name, port.displayName});
    }

    for (const auto& [name, value] : node->getProperties()) {
      std::string strValue;
      std::visit(
          [&strValue](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
              strValue = "";
            } else if constexpr (std::is_same_v<T, bool>) {
              strValue = v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, i64>) {
              strValue = std::to_string(v);
            } else if constexpr (std::is_same_v<T, f64>) {
              strValue = std::to_string(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
              strValue = v;
            }
          },
          value);
      vnode.properties[name] = strValue;
    }

    m_nodes.push_back(vnode);
    m_nextId = std::max(m_nextId, vnode.id + 1);
  }

  for (const auto& conn : ir.getConnections()) {
    VisualGraphEdge edge;
    edge.sourceNode = conn.source.nodeId;
    edge.sourcePort = conn.source.portName;
    edge.targetNode = conn.target.nodeId;
    edge.targetPort = conn.target.portName;
    m_edges.push_back(edge);
  }
}

std::unique_ptr<IRGraph> VisualGraph::toIR() const {
  auto ir = std::make_unique<IRGraph>();

  static std::unordered_map<std::string, IRNodeType> typeMap = {
      {"SceneStart", IRNodeType::SceneStart},
      {"SceneEnd", IRNodeType::SceneEnd},
      {"Comment", IRNodeType::Comment},
      {"Scene", IRNodeType::Scene}, // Scene node type for
                                    // Visual-First/Code-First workflows
      {"Sequence", IRNodeType::Sequence},
      {"Branch", IRNodeType::Branch},
      {"Switch", IRNodeType::Switch},
      {"Loop", IRNodeType::Loop},
      {"ShowCharacter", IRNodeType::ShowCharacter},
      {"HideCharacter", IRNodeType::HideCharacter},
      {"ShowBackground", IRNodeType::ShowBackground},
      {"Dialogue", IRNodeType::Dialogue},
      {"Choice", IRNodeType::Choice},
      {"ChoiceOption", IRNodeType::ChoiceOption},
      {"PlayMusic", IRNodeType::PlayMusic},
      {"StopMusic", IRNodeType::StopMusic},
      {"PlaySound", IRNodeType::PlaySound},
      {"Transition", IRNodeType::Transition},
      {"Wait", IRNodeType::Wait},
      {"SetVariable", IRNodeType::SetVariable},
      {"GetVariable", IRNodeType::GetVariable},
      {"Goto", IRNodeType::Goto},
      {"Label", IRNodeType::Label},
      {"Expression", IRNodeType::Expression},
      {"FunctionCall", IRNodeType::FunctionCall},
      {"Custom", IRNodeType::Custom}};

  std::unordered_map<NodeId, NodeId> idMap;
  for (const auto& vnode : m_nodes) {
    auto typeIt = typeMap.find(vnode.type);
    IRNodeType type = (typeIt != typeMap.end()) ? typeIt->second : IRNodeType::Custom;

    NodeId newId = ir->createNode(type);
    idMap[vnode.id] = newId;

    auto* node = ir->getNode(newId);
    node->setPosition(vnode.x, vnode.y);

    for (const auto& [name, value] : vnode.properties) {
      if (value == "true") {
        node->setProperty(name, true);
      } else if (value == "false") {
        node->setProperty(name, false);
      } else {
        try {
          size_t pos;
          i64 intVal = std::stoll(value, &pos);
          if (pos == value.length()) {
            node->setProperty(name, intVal);
            continue;
          }
        } catch (...) {
        }

        try {
          size_t pos;
          f64 floatVal = std::stod(value, &pos);
          if (pos == value.length()) {
            node->setProperty(name, floatVal);
            continue;
          }
        } catch (...) {
        }

        node->setProperty(name, value);
      }
    }
  }

  for (const auto& edge : m_edges) {
    auto sourceIt = idMap.find(edge.sourceNode);
    auto targetIt = idMap.find(edge.targetNode);

    if (sourceIt != idMap.end() && targetIt != idMap.end()) {
      PortId source{sourceIt->second, edge.sourcePort, true};
      PortId target{targetIt->second, edge.targetPort, false};
      ir->connect(source, target);
    }
  }

  return ir;
}

VisualGraphNode* VisualGraph::findNode(NodeId id) {
  for (auto& node : m_nodes) {
    if (node.id == id) {
      return &node;
    }
  }
  return nullptr;
}

const VisualGraphNode* VisualGraph::findNode(NodeId id) const {
  for (const auto& node : m_nodes) {
    if (node.id == id) {
      return &node;
    }
  }
  return nullptr;
}

NodeId VisualGraph::addNode(const std::string& type, f32 x, f32 y) {
  VisualGraphNode node;
  node.id = m_nextId++;
  node.type = type;
  node.displayName = type;
  node.x = x;
  node.y = y;
  m_nodes.push_back(node);
  return node.id;
}

void VisualGraph::removeNode(NodeId id) {
  m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(),
                               [id](const VisualGraphEdge& e) {
                                 return e.sourceNode == id || e.targetNode == id;
                               }),
                m_edges.end());

  m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                               [id](const VisualGraphNode& n) { return n.id == id; }),
                m_nodes.end());
}

void VisualGraph::setNodePosition(NodeId id, f32 x, f32 y) {
  auto* node = findNode(id);
  if (node) {
    node->x = x;
    node->y = y;
  }
}

void VisualGraph::setNodeProperty(NodeId id, const std::string& name, const std::string& value) {
  auto* node = findNode(id);
  if (node) {
    node->properties[name] = value;
  }
}

void VisualGraph::addEdge(NodeId sourceNode, const std::string& sourcePort, NodeId targetNode,
                          const std::string& targetPort) {
  VisualGraphEdge edge;
  edge.sourceNode = sourceNode;
  edge.sourcePort = sourcePort;
  edge.targetNode = targetNode;
  edge.targetPort = targetPort;
  m_edges.push_back(edge);
}

void VisualGraph::removeEdge(NodeId sourceNode, const std::string& sourcePort, NodeId targetNode,
                             const std::string& targetPort) {
  m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(),
                               [&](const VisualGraphEdge& e) {
                                 return e.sourceNode == sourceNode && e.sourcePort == sourcePort &&
                                        e.targetNode == targetNode && e.targetPort == targetPort;
                               }),
                m_edges.end());
}

void VisualGraph::selectNode(NodeId id, bool addToSelection) {
  if (!addToSelection) {
    for (auto& node : m_nodes) {
      node.selected = false;
    }
  }

  auto* node = findNode(id);
  if (node) {
    node->selected = true;
  }
}

void VisualGraph::deselectNode(NodeId id) {
  auto* node = findNode(id);
  if (node) {
    node->selected = false;
  }
}

void VisualGraph::selectEdge(NodeId /*sourceNode*/, const std::string& /*sourcePort*/,
                             NodeId /*targetNode*/, const std::string& /*targetPort*/) {
  // Find and select the edge
}

void VisualGraph::clearSelection() {
  for (auto& node : m_nodes) {
    node.selected = false;
  }
  for (auto& edge : m_edges) {
    edge.selected = false;
  }
}

void VisualGraph::autoLayout() {
  f32 y = 100.0f;
  f32 spacing = 150.0f;

  for (auto& node : m_nodes) {
    node.x = 200.0f;
    node.y = y;
    y += spacing;
  }
}

std::string VisualGraph::toJson() const {
  std::stringstream ss;
  ss << "{\"nodes\":[";

  bool first = true;
  for (const auto& node : m_nodes) {
    if (!first)
      ss << ",";
    first = false;
    ss << "{\"id\":" << node.id;
    ss << ",\"type\":\"" << node.type << "\"";
    ss << ",\"x\":" << node.x;
    ss << ",\"y\":" << node.y << "}";
  }

  ss << "],\"edges\":[";

  first = true;
  for (const auto& edge : m_edges) {
    if (!first)
      ss << ",";
    first = false;
    ss << "{\"src\":" << edge.sourceNode;
    ss << ",\"srcPort\":\"" << edge.sourcePort << "\"";
    ss << ",\"tgt\":" << edge.targetNode;
    ss << ",\"tgtPort\":\"" << edge.targetPort << "\"}";
  }

  ss << "]}";
  return ss.str();
}

} // namespace NovelMind::scripting
