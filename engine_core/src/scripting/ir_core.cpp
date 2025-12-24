/**
 * @file ir_core.cpp
 * @brief Intermediate Representation core implementation
 */

#include "NovelMind/scripting/ir.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace NovelMind::scripting {

// ============================================================================
// IRNode Implementation
// ============================================================================

IRNode::IRNode(NodeId id, IRNodeType type) : m_id(id), m_type(type) {}

const char *IRNode::getTypeName() const {
  switch (m_type) {
  case IRNodeType::SceneStart:
    return "SceneStart";
  case IRNodeType::SceneEnd:
    return "SceneEnd";
  case IRNodeType::Comment:
    return "Comment";
  case IRNodeType::Scene:
    return "Scene";
  case IRNodeType::Sequence:
    return "Sequence";
  case IRNodeType::Branch:
    return "Branch";
  case IRNodeType::Switch:
    return "Switch";
  case IRNodeType::Loop:
    return "Loop";
  case IRNodeType::Goto:
    return "Goto";
  case IRNodeType::Label:
    return "Label";
  case IRNodeType::ShowCharacter:
    return "ShowCharacter";
  case IRNodeType::HideCharacter:
    return "HideCharacter";
  case IRNodeType::ShowBackground:
    return "ShowBackground";
  case IRNodeType::Dialogue:
    return "Dialogue";
  case IRNodeType::Choice:
    return "Choice";
  case IRNodeType::ChoiceOption:
    return "ChoiceOption";
  case IRNodeType::PlayMusic:
    return "PlayMusic";
  case IRNodeType::StopMusic:
    return "StopMusic";
  case IRNodeType::PlaySound:
    return "PlaySound";
  case IRNodeType::Transition:
    return "Transition";
  case IRNodeType::Wait:
    return "Wait";
  case IRNodeType::SetVariable:
    return "SetVariable";
  case IRNodeType::GetVariable:
    return "GetVariable";
  case IRNodeType::Expression:
    return "Expression";
  case IRNodeType::FunctionCall:
    return "FunctionCall";
  case IRNodeType::Custom:
    return "Custom";
  }
  return "Unknown";
}

void IRNode::setProperty(const std::string &name,
                         const IRPropertyValue &value) {
  m_properties[name] = value;
}

std::optional<IRPropertyValue>
IRNode::getProperty(const std::string &name) const {
  auto it = m_properties.find(name);
  if (it != m_properties.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string IRNode::getStringProperty(const std::string &name,
                                      const std::string &defaultValue) const {
  auto prop = getProperty(name);
  if (prop && std::holds_alternative<std::string>(*prop)) {
    return std::get<std::string>(*prop);
  }
  return defaultValue;
}

i64 IRNode::getIntProperty(const std::string &name, i64 defaultValue) const {
  auto prop = getProperty(name);
  if (prop && std::holds_alternative<i64>(*prop)) {
    return std::get<i64>(*prop);
  }
  return defaultValue;
}

f64 IRNode::getFloatProperty(const std::string &name, f64 defaultValue) const {
  auto prop = getProperty(name);
  if (prop && std::holds_alternative<f64>(*prop)) {
    return std::get<f64>(*prop);
  }
  return defaultValue;
}

bool IRNode::getBoolProperty(const std::string &name, bool defaultValue) const {
  auto prop = getProperty(name);
  if (prop && std::holds_alternative<bool>(*prop)) {
    return std::get<bool>(*prop);
  }
  return defaultValue;
}

void IRNode::setSourceLocation(const SourceLocation &loc) { m_location = loc; }

void IRNode::setPosition(f32 x, f32 y) {
  m_x = x;
  m_y = y;
}

std::vector<PortDefinition> IRNode::getInputPorts() const {
  std::vector<PortDefinition> ports;

  // All nodes have execution input except SceneStart
  if (m_type != IRNodeType::SceneStart) {
    ports.push_back({"exec_in", "In", true, false, ""});
  }

  // Type-specific input ports
  switch (m_type) {
  case IRNodeType::Branch:
    ports.push_back({"condition", "Condition", false, true, ""});
    break;
  case IRNodeType::ShowCharacter:
    ports.push_back({"character", "Character", false, true, ""});
    ports.push_back({"position", "Position", false, false, "center"});
    ports.push_back({"expression", "Expression", false, false, "default"});
    break;
  case IRNodeType::HideCharacter:
    ports.push_back({"character", "Character", false, true, ""});
    break;
  case IRNodeType::ShowBackground:
    ports.push_back({"background", "Background", false, true, ""});
    break;
  case IRNodeType::Dialogue:
    ports.push_back({"character", "Character", false, false, ""});
    ports.push_back({"text", "Text", false, true, ""});
    break;
  case IRNodeType::Choice:
    break;
  case IRNodeType::PlayMusic:
  case IRNodeType::PlaySound:
    ports.push_back({"track", "Track", false, true, ""});
    ports.push_back({"volume", "Volume", false, false, "1.0"});
    break;
  case IRNodeType::Wait:
    ports.push_back({"duration", "Duration", false, true, ""});
    break;
  case IRNodeType::SetVariable:
    ports.push_back({"name", "Name", false, true, ""});
    ports.push_back({"value", "Value", false, true, ""});
    break;
  default:
    break;
  }

  return ports;
}

std::vector<PortDefinition> IRNode::getOutputPorts() const {
  std::vector<PortDefinition> ports;

  switch (m_type) {
  case IRNodeType::SceneStart:
  case IRNodeType::Sequence:
  case IRNodeType::ShowCharacter:
  case IRNodeType::HideCharacter:
  case IRNodeType::ShowBackground:
  case IRNodeType::Dialogue:
  case IRNodeType::PlayMusic:
  case IRNodeType::StopMusic:
  case IRNodeType::PlaySound:
  case IRNodeType::Transition:
  case IRNodeType::Wait:
  case IRNodeType::SetVariable:
  case IRNodeType::Label:
    ports.push_back({"exec_out", "Out", true, false, ""});
    break;
  case IRNodeType::Branch:
    ports.push_back({"true", "True", true, false, ""});
    ports.push_back({"false", "False", true, false, ""});
    break;
  case IRNodeType::GetVariable:
    ports.push_back({"value", "Value", false, false, ""});
    break;
  case IRNodeType::Expression:
    ports.push_back({"result", "Result", false, false, ""});
    break;
  case IRNodeType::SceneEnd:
    break;
  default:
    break;
  }

  return ports;
}

std::string IRNode::toJson() const {
  std::stringstream ss;
  ss << "{";
  ss << "\"id\":" << m_id << ",";
  ss << "\"type\":\"" << getTypeName() << "\",";
  ss << "\"x\":" << m_x << ",";
  ss << "\"y\":" << m_y << ",";
  ss << "\"properties\":{";

  bool first = true;
  for (const auto &[name, value] : m_properties) {
    if (!first)
      ss << ",";
    first = false;
    ss << "\"" << name << "\":";

    std::visit(
        [&ss](const auto &v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, std::nullptr_t>) {
            ss << "null";
          } else if constexpr (std::is_same_v<T, bool>) {
            ss << (v ? "true" : "false");
          } else if constexpr (std::is_same_v<T, i64>) {
            ss << v;
          } else if constexpr (std::is_same_v<T, f64>) {
            ss << v;
          } else if constexpr (std::is_same_v<T, std::string>) {
            ss << "\"" << v << "\"";
          } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            ss << "[";
            for (size_t i = 0; i < v.size(); ++i) {
              if (i > 0)
                ss << ",";
              ss << "\"" << v[i] << "\"";
            }
            ss << "]";
          }
        },
        value);
  }

  ss << "}}";
  return ss.str();
}

// ============================================================================
// IRGraph Implementation
// ============================================================================

IRGraph::IRGraph() = default;
IRGraph::~IRGraph() = default;

void IRGraph::setName(const std::string &name) { m_name = name; }

NodeId IRGraph::createNode(IRNodeType type) {
  NodeId id = m_nextId++;
  m_nodes[id] = std::make_unique<IRNode>(id, type);
  return id;
}

void IRGraph::removeNode(NodeId id) {
  disconnectAll(id);
  m_nodes.erase(id);
}

IRNode *IRGraph::getNode(NodeId id) {
  auto it = m_nodes.find(id);
  return (it != m_nodes.end()) ? it->second.get() : nullptr;
}

const IRNode *IRGraph::getNode(NodeId id) const {
  auto it = m_nodes.find(id);
  return (it != m_nodes.end()) ? it->second.get() : nullptr;
}

std::vector<IRNode *> IRGraph::getNodes() {
  std::vector<IRNode *> result;
  for (auto &[id, node] : m_nodes) {
    result.push_back(node.get());
  }
  return result;
}

std::vector<const IRNode *> IRGraph::getNodes() const {
  std::vector<const IRNode *> result;
  for (const auto &[id, node] : m_nodes) {
    result.push_back(node.get());
  }
  return result;
}

std::vector<IRNode *> IRGraph::getNodesByType(IRNodeType type) {
  std::vector<IRNode *> result;
  for (auto &[id, node] : m_nodes) {
    if (node->getType() == type) {
      result.push_back(node.get());
    }
  }
  return result;
}

Result<void> IRGraph::connect(const PortId &source, const PortId &target) {
  auto *sourceNode = getNode(source.nodeId);
  auto *targetNode = getNode(target.nodeId);

  if (!sourceNode || !targetNode) {
    return Result<void>::error("Invalid node ID");
  }

  if (isConnected(source, target)) {
    return Result<void>::ok();
  }

  IRConnection conn;
  conn.source = source;
  conn.target = target;
  m_connections.push_back(conn);

  return Result<void>::ok();
}

void IRGraph::disconnect(const PortId &source, const PortId &target) {
  m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
                                     [&](const IRConnection &c) {
                                       return c.source == source &&
                                              c.target == target;
                                     }),
                      m_connections.end());
}

void IRGraph::disconnectAll(NodeId nodeId) {
  m_connections.erase(std::remove_if(m_connections.begin(), m_connections.end(),
                                     [nodeId](const IRConnection &c) {
                                       return c.source.nodeId == nodeId ||
                                              c.target.nodeId == nodeId;
                                     }),
                      m_connections.end());
}

std::vector<IRConnection> IRGraph::getConnections() const {
  return m_connections;
}

std::vector<IRConnection> IRGraph::getConnectionsFrom(NodeId nodeId) const {
  std::vector<IRConnection> result;
  for (const auto &conn : m_connections) {
    if (conn.source.nodeId == nodeId) {
      result.push_back(conn);
    }
  }
  return result;
}

std::vector<IRConnection> IRGraph::getConnectionsTo(NodeId nodeId) const {
  std::vector<IRConnection> result;
  for (const auto &conn : m_connections) {
    if (conn.target.nodeId == nodeId) {
      result.push_back(conn);
    }
  }
  return result;
}

bool IRGraph::isConnected(const PortId &source, const PortId &target) const {
  for (const auto &conn : m_connections) {
    if (conn.source == source && conn.target == target) {
      return true;
    }
  }
  return false;
}

std::vector<NodeId> IRGraph::getTopologicalOrder() const {
  std::vector<NodeId> result;
  std::unordered_map<NodeId, int> inDegree;
  std::queue<NodeId> queue;

  for (const auto &[id, node] : m_nodes) {
    inDegree[id] = 0;
  }

  for (const auto &conn : m_connections) {
    inDegree[conn.target.nodeId]++;
  }

  for (const auto &[id, degree] : inDegree) {
    if (degree == 0) {
      queue.push(id);
    }
  }

  while (!queue.empty()) {
    NodeId id = queue.front();
    queue.pop();
    result.push_back(id);

    for (const auto &conn : getConnectionsFrom(id)) {
      inDegree[conn.target.nodeId]--;
      if (inDegree[conn.target.nodeId] == 0) {
        queue.push(conn.target.nodeId);
      }
    }
  }

  return result;
}

std::vector<NodeId> IRGraph::getExecutionOrder() const {
  std::vector<NodeId> result;
  std::unordered_set<NodeId> visited;

  auto startNodes =
      const_cast<IRGraph *>(this)->getNodesByType(IRNodeType::SceneStart);
  for (auto *start : startNodes) {
    std::queue<NodeId> queue;
    queue.push(start->getId());

    while (!queue.empty()) {
      NodeId id = queue.front();
      queue.pop();

      if (visited.count(id)) {
        continue;
      }
      visited.insert(id);
      result.push_back(id);

      for (const auto &conn : getConnectionsFrom(id)) {
        if (conn.source.portName.find("exec") != std::string::npos ||
            conn.source.portName == "true" || conn.source.portName == "false") {
          queue.push(conn.target.nodeId);
        }
      }
    }
  }

  return result;
}

std::vector<std::string> IRGraph::validate() const {
  std::vector<std::string> errors;

  for (const auto &[id, node] : m_nodes) {
    if (node->getType() != IRNodeType::SceneStart &&
        node->getType() != IRNodeType::Comment) {
      auto incoming = getConnectionsTo(id);
      if (incoming.empty()) {
        errors.push_back("Node " + std::to_string(id) +
                         " has no incoming connections");
      }
    }
  }

  for (const auto &[id, node] : m_nodes) {
    for (const auto &port : node->getInputPorts()) {
      if (port.required && !port.isExecution) {
        bool found = false;
        for (const auto &conn : getConnectionsTo(id)) {
          if (conn.target.portName == port.name) {
            found = true;
            break;
          }
        }
        if (!found) {
          auto prop = node->getProperty(port.name);
          if (!prop || std::holds_alternative<std::nullptr_t>(*prop)) {
            errors.push_back("Node " + std::to_string(id) +
                             " missing required input: " + port.name);
          }
        }
      }
    }
  }

  return errors;
}

bool IRGraph::isValid() const { return validate().empty(); }

void IRGraph::addScene(const std::string &sceneName, NodeId startNode) {
  m_sceneStartNodes[sceneName] = startNode;
}

NodeId IRGraph::getSceneStartNode(const std::string &sceneName) const {
  auto it = m_sceneStartNodes.find(sceneName);
  return (it != m_sceneStartNodes.end()) ? it->second : 0;
}

std::vector<std::string> IRGraph::getSceneNames() const {
  std::vector<std::string> names;
  for (const auto &[name, id] : m_sceneStartNodes) {
    names.push_back(name);
  }
  return names;
}

void IRGraph::addCharacter(const std::string &id, const std::string &name,
                           const std::string &color) {
  m_characters[id] = {name, color};
}

bool IRGraph::hasCharacter(const std::string &id) const {
  return m_characters.count(id) > 0;
}

std::string IRGraph::toJson() const {
  std::stringstream ss;
  ss << "{";
  ss << "\"name\":\"" << m_name << "\",";

  ss << "\"nodes\":[";
  bool first = true;
  for (const auto &[id, node] : m_nodes) {
    if (!first)
      ss << ",";
    first = false;
    ss << node->toJson();
  }
  ss << "],";

  ss << "\"connections\":[";
  first = true;
  for (const auto &conn : m_connections) {
    if (!first)
      ss << ",";
    first = false;
    ss << "{";
    ss << "\"sourceNode\":" << conn.source.nodeId << ",";
    ss << "\"sourcePort\":\"" << conn.source.portName << "\",";
    ss << "\"targetNode\":" << conn.target.nodeId << ",";
    ss << "\"targetPort\":\"" << conn.target.portName << "\"";
    ss << "}";
  }
  ss << "],";

  ss << "\"scenes\":{";
  first = true;
  for (const auto &[name, id] : m_sceneStartNodes) {
    if (!first)
      ss << ",";
    first = false;
    ss << "\"" << name << "\":" << id;
  }
  ss << "},";

  ss << "\"characters\":{";
  first = true;
  for (const auto &[id, info] : m_characters) {
    if (!first)
      ss << ",";
    first = false;
    ss << "\"" << id << "\":{\"name\":\"" << info.first << "\",\"color\":\""
       << info.second << "\"}";
  }
  ss << "}";

  ss << "}";
  return ss.str();
}

} // namespace NovelMind::scripting
