/**
 * @file ir_round_trip.cpp
 * @brief IR round-trip utilities and validators
 */

#include "NovelMind/scripting/ir.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace NovelMind::scripting {

// ============================================================================
// RoundTripConverter Implementation
// ============================================================================

RoundTripConverter::RoundTripConverter()
    : m_lexer(std::make_unique<Lexer>()), m_parser(std::make_unique<Parser>()),
      m_astToIR(std::make_unique<ASTToIRConverter>()),
      m_irToAST(std::make_unique<IRToASTConverter>()),
      m_textGen(std::make_unique<ASTToTextGenerator>()) {}

RoundTripConverter::~RoundTripConverter() = default;

Result<std::unique_ptr<IRGraph>> RoundTripConverter::textToIR(const std::string& nmScript) {
  // Tokenize the script first
  auto tokenResult = m_lexer->tokenize(nmScript);
  if (!tokenResult.isOk()) {
    return Result<std::unique_ptr<IRGraph>>::error("Lexer error: " + tokenResult.error());
  }

  // Parse tokens to AST
  auto parseResult = m_parser->parse(tokenResult.value());
  if (!parseResult.isOk()) {
    return Result<std::unique_ptr<IRGraph>>::error("Parse error: " + parseResult.error());
  }

  // Convert AST to IR
  return m_astToIR->convert(parseResult.value());
}

Result<std::string> RoundTripConverter::irToText(const IRGraph& ir) {
  auto astResult = m_irToAST->convert(ir);
  if (!astResult.isOk()) {
    return Result<std::string>::error("IR to AST conversion failed");
  }

  return Result<std::string>::ok(m_textGen->generate(astResult.value()));
}

Result<std::unique_ptr<VisualGraph>> RoundTripConverter::irToVisualGraph(const IRGraph& ir) {
  auto graph = std::make_unique<VisualGraph>();
  graph->fromIR(ir);
  return Result<std::unique_ptr<VisualGraph>>::ok(std::move(graph));
}

Result<std::unique_ptr<IRGraph>> RoundTripConverter::visualGraphToIR(const VisualGraph& graph) {
  return Result<std::unique_ptr<IRGraph>>::ok(graph.toIR());
}

Result<std::unique_ptr<VisualGraph>>
RoundTripConverter::textToVisualGraph(const std::string& nmScript) {
  auto irResult = textToIR(nmScript);
  if (!irResult.isOk()) {
    return Result<std::unique_ptr<VisualGraph>>::error(irResult.error());
  }

  return irToVisualGraph(*irResult.value());
}

Result<std::string> RoundTripConverter::visualGraphToText(const VisualGraph& graph) {
  auto irResult = visualGraphToIR(graph);
  if (!irResult.isOk()) {
    return Result<std::string>::error(irResult.error());
  }

  return irToText(*irResult.value());
}

std::vector<std::string>
RoundTripConverter::validateConversion(const std::string& /*original*/,
                                       const std::string& /*roundTripped*/) {
  std::vector<std::string> differences;
  // Would compare original and round-tripped versions
  return differences;
}

// ============================================================================
// GraphDiffer Implementation
// ============================================================================

GraphDiff GraphDiffer::diff(const VisualGraph& oldGraph, const VisualGraph& newGraph) const {
  GraphDiff result;

  diffNodes(oldGraph, newGraph, result);
  diffEdges(oldGraph, newGraph, result);

  return result;
}

void GraphDiffer::diffNodes(const VisualGraph& oldGraph, const VisualGraph& newGraph,
                            GraphDiff& result) const {
  const auto& oldNodes = oldGraph.getNodes();
  const auto& newNodes = newGraph.getNodes();

  // Build ID sets
  std::unordered_set<NodeId> oldIds;
  std::unordered_set<NodeId> newIds;

  for (const auto& node : oldNodes) {
    oldIds.insert(node.id);
  }
  for (const auto& node : newNodes) {
    newIds.insert(node.id);
  }

  // Find removed nodes
  for (const auto& node : oldNodes) {
    if (newIds.find(node.id) == newIds.end()) {
      GraphDiffEntry entry;
      entry.type = GraphDiffType::NodeRemoved;
      entry.nodeId = node.id;
      entry.oldValue = node.type;
      result.entries.push_back(entry);
      result.hasStructuralChanges = true;
    }
  }

  // Find added and modified nodes
  for (const auto& newNode : newNodes) {
    if (oldIds.find(newNode.id) == oldIds.end()) {
      GraphDiffEntry entry;
      entry.type = GraphDiffType::NodeAdded;
      entry.nodeId = newNode.id;
      entry.newValue = newNode.type;
      result.entries.push_back(entry);
      result.hasStructuralChanges = true;
    } else {
      // Node exists in both - check for modifications
      const auto* oldNode = oldGraph.findNode(newNode.id);
      if (oldNode) {
        diffNodeProperties(*oldNode, newNode, result);

        // Check position changes
        if (oldNode->x != newNode.x || oldNode->y != newNode.y) {
          GraphDiffEntry entry;
          entry.type = GraphDiffType::PositionChanged;
          entry.nodeId = newNode.id;
          entry.oldValue = std::to_string(oldNode->x) + "," + std::to_string(oldNode->y);
          entry.newValue = std::to_string(newNode.x) + "," + std::to_string(newNode.y);
          result.entries.push_back(entry);
          result.hasPositionChanges = true;
        }
      }
    }
  }
}

void GraphDiffer::diffEdges(const VisualGraph& oldGraph, const VisualGraph& newGraph,
                            GraphDiff& result) const {
  const auto& oldEdges = oldGraph.getEdges();
  const auto& newEdges = newGraph.getEdges();

  // Helper to compare edges
  auto edgeEqual = [](const VisualGraphEdge& a, const VisualGraphEdge& b) {
    return a.sourceNode == b.sourceNode && a.sourcePort == b.sourcePort &&
           a.targetNode == b.targetNode && a.targetPort == b.targetPort;
  };

  // Find removed edges
  for (const auto& oldEdge : oldEdges) {
    bool found = false;
    for (const auto& newEdge : newEdges) {
      if (edgeEqual(oldEdge, newEdge)) {
        found = true;
        break;
      }
    }
    if (!found) {
      GraphDiffEntry entry;
      entry.type = GraphDiffType::EdgeRemoved;
      entry.edge = oldEdge;
      result.entries.push_back(entry);
      result.hasStructuralChanges = true;
    }
  }

  // Find added edges
  for (const auto& newEdge : newEdges) {
    bool found = false;
    for (const auto& oldEdge : oldEdges) {
      if (edgeEqual(oldEdge, newEdge)) {
        found = true;
        break;
      }
    }
    if (!found) {
      GraphDiffEntry entry;
      entry.type = GraphDiffType::EdgeAdded;
      entry.edge = newEdge;
      result.entries.push_back(entry);
      result.hasStructuralChanges = true;
    }
  }
}

void GraphDiffer::diffNodeProperties(const VisualGraphNode& oldNode, const VisualGraphNode& newNode,
                                     GraphDiff& result) const {
  // Check type change (rare but possible)
  if (oldNode.type != newNode.type) {
    GraphDiffEntry entry;
    entry.type = GraphDiffType::NodeModified;
    entry.nodeId = oldNode.id;
    entry.propertyName = "type";
    entry.oldValue = oldNode.type;
    entry.newValue = newNode.type;
    result.entries.push_back(entry);
    result.hasPropertyChanges = true;
  }

  // Check display name
  if (oldNode.displayName != newNode.displayName) {
    GraphDiffEntry entry;
    entry.type = GraphDiffType::PropertyChanged;
    entry.nodeId = oldNode.id;
    entry.propertyName = "displayName";
    entry.oldValue = oldNode.displayName;
    entry.newValue = newNode.displayName;
    result.entries.push_back(entry);
    result.hasPropertyChanges = true;
  }

  // Check properties
  std::unordered_set<std::string> allProps;
  for (const auto& [name, value] : oldNode.properties) {
    allProps.insert(name);
  }
  for (const auto& [name, value] : newNode.properties) {
    allProps.insert(name);
  }

  for (const auto& propName : allProps) {
    auto oldIt = oldNode.properties.find(propName);
    auto newIt = newNode.properties.find(propName);

    std::string oldVal = (oldIt != oldNode.properties.end()) ? oldIt->second : "";
    std::string newVal = (newIt != newNode.properties.end()) ? newIt->second : "";

    if (oldVal != newVal) {
      GraphDiffEntry entry;
      entry.type = GraphDiffType::PropertyChanged;
      entry.nodeId = oldNode.id;
      entry.propertyName = propName;
      entry.oldValue = oldVal;
      entry.newValue = newVal;
      result.entries.push_back(entry);
      result.hasPropertyChanges = true;
    }
  }
}

Result<void> GraphDiffer::applyDiff(VisualGraph& graph, const GraphDiff& diff) const {
  for (const auto& entry : diff.entries) {
    switch (entry.type) {
    case GraphDiffType::NodeAdded: {
      graph.addNode(entry.newValue, 0.0f, 0.0f);
      break;
    }
    case GraphDiffType::NodeRemoved: {
      graph.removeNode(entry.nodeId);
      break;
    }
    case GraphDiffType::PropertyChanged: {
      graph.setNodeProperty(entry.nodeId, entry.propertyName, entry.newValue);
      break;
    }
    case GraphDiffType::PositionChanged: {
      std::istringstream iss(entry.newValue);
      f32 x, y;
      char comma;
      if (iss >> x >> comma >> y) {
        graph.setNodePosition(entry.nodeId, x, y);
      }
      break;
    }
    case GraphDiffType::EdgeAdded: {
      graph.addEdge(entry.edge.sourceNode, entry.edge.sourcePort, entry.edge.targetNode,
                    entry.edge.targetPort);
      break;
    }
    case GraphDiffType::EdgeRemoved: {
      graph.removeEdge(entry.edge.sourceNode, entry.edge.sourcePort, entry.edge.targetNode,
                       entry.edge.targetPort);
      break;
    }
    default:
      break;
    }
  }

  return Result<void>::ok();
}

GraphDiff GraphDiffer::invertDiff(const GraphDiff& diff) const {
  GraphDiff inverted;
  inverted.hasStructuralChanges = diff.hasStructuralChanges;
  inverted.hasPropertyChanges = diff.hasPropertyChanges;
  inverted.hasPositionChanges = diff.hasPositionChanges;

  for (auto it = diff.entries.rbegin(); it != diff.entries.rend(); ++it) {
    GraphDiffEntry entry = *it;

    switch (entry.type) {
    case GraphDiffType::NodeAdded:
      entry.type = GraphDiffType::NodeRemoved;
      std::swap(entry.oldValue, entry.newValue);
      break;
    case GraphDiffType::NodeRemoved:
      entry.type = GraphDiffType::NodeAdded;
      std::swap(entry.oldValue, entry.newValue);
      break;
    case GraphDiffType::EdgeAdded:
      entry.type = GraphDiffType::EdgeRemoved;
      break;
    case GraphDiffType::EdgeRemoved:
      entry.type = GraphDiffType::EdgeAdded;
      break;
    case GraphDiffType::PropertyChanged:
    case GraphDiffType::PositionChanged:
    case GraphDiffType::NodeModified:
      std::swap(entry.oldValue, entry.newValue);
      break;
    }

    inverted.entries.push_back(entry);
  }

  return inverted;
}

Result<GraphDiff> GraphDiffer::mergeDiffs(const GraphDiff& diff1, const GraphDiff& diff2) const {
  if (hasConflicts(diff1, diff2)) {
    return Result<GraphDiff>::error("Diffs have conflicts and cannot be merged");
  }

  GraphDiff merged;
  merged.entries = diff1.entries;
  merged.entries.insert(merged.entries.end(), diff2.entries.begin(), diff2.entries.end());
  merged.hasStructuralChanges = diff1.hasStructuralChanges || diff2.hasStructuralChanges;
  merged.hasPropertyChanges = diff1.hasPropertyChanges || diff2.hasPropertyChanges;
  merged.hasPositionChanges = diff1.hasPositionChanges || diff2.hasPositionChanges;

  return Result<GraphDiff>::ok(std::move(merged));
}

bool GraphDiffer::hasConflicts(const GraphDiff& diff1, const GraphDiff& diff2) const {
  for (const auto& e1 : diff1.entries) {
    if (e1.type == GraphDiffType::PropertyChanged || e1.type == GraphDiffType::PositionChanged) {
      for (const auto& e2 : diff2.entries) {
        if ((e2.type == GraphDiffType::PropertyChanged ||
             e2.type == GraphDiffType::PositionChanged) &&
            e1.nodeId == e2.nodeId && e1.propertyName == e2.propertyName &&
            e1.newValue != e2.newValue) {
          return true;
        }
      }
    }
  }

  return false;
}

// ============================================================================
// IDNormalizer Implementation
// ============================================================================

std::unordered_map<NodeId, NodeId> IDNormalizer::normalize(VisualGraph& graph) const {
  std::unordered_map<NodeId, NodeId> mapping;

  auto order = getTopologicalOrder(graph);

  if (order.empty()) {
    const auto& nodes = graph.getNodes();
    for (const auto& node : nodes) {
      order.push_back(node.id);
    }
    std::sort(order.begin(), order.end());
  }

  NodeId newId = 1;
  for (NodeId oldId : order) {
    mapping[oldId] = newId++;
  }

  return mapping;
}

std::unordered_map<NodeId, NodeId> IDNormalizer::normalize(IRGraph& /*graph*/) const {
  std::unordered_map<NodeId, NodeId> mapping;
  return mapping;
}

bool IDNormalizer::needsNormalization(const VisualGraph& graph) const {
  const auto& nodes = graph.getNodes();
  if (nodes.empty()) {
    return false;
  }

  std::vector<NodeId> ids;
  for (const auto& node : nodes) {
    ids.push_back(node.id);
  }
  std::sort(ids.begin(), ids.end());

  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] != static_cast<NodeId>(i + 1)) {
      return true;
    }
  }

  return false;
}

std::pair<std::unique_ptr<VisualGraph>, std::unordered_map<NodeId, NodeId>>
IDNormalizer::createNormalizedCopy(const VisualGraph& graph) const {
  auto normalized = std::make_unique<VisualGraph>();
  std::unordered_map<NodeId, NodeId> mapping;

  auto order = getTopologicalOrder(graph);

  if (order.empty()) {
    const auto& nodes = graph.getNodes();
    for (const auto& node : nodes) {
      order.push_back(node.id);
    }
    std::sort(order.begin(), order.end());
  }

  for (NodeId oldId : order) {
    const auto* oldNode = graph.findNode(oldId);
    if (oldNode) {
      NodeId assignedId = normalized->addNode(oldNode->type, oldNode->x, oldNode->y);
      mapping[oldId] = assignedId;

      auto* newNode = normalized->findNode(assignedId);
      if (newNode) {
        newNode->displayName = oldNode->displayName;
        newNode->properties = oldNode->properties;
        newNode->width = oldNode->width;
        newNode->height = oldNode->height;
        newNode->inputPorts = oldNode->inputPorts;
        newNode->outputPorts = oldNode->outputPorts;
      }
    }
  }

  for (const auto& edge : graph.getEdges()) {
    auto srcIt = mapping.find(edge.sourceNode);
    auto tgtIt = mapping.find(edge.targetNode);

    if (srcIt != mapping.end() && tgtIt != mapping.end()) {
      normalized->addEdge(srcIt->second, edge.sourcePort, tgtIt->second, edge.targetPort);
    }
  }

  return {std::move(normalized), mapping};
}

std::vector<NodeId> IDNormalizer::getTopologicalOrder(const VisualGraph& graph) const {
  std::vector<NodeId> result;
  const auto& nodes = graph.getNodes();
  const auto& edges = graph.getEdges();

  std::unordered_map<NodeId, int> inDegree;
  std::unordered_map<NodeId, std::vector<NodeId>> adjacency;

  for (const auto& node : nodes) {
    inDegree[node.id] = 0;
    adjacency[node.id] = {};
  }

  for (const auto& edge : edges) {
    inDegree[edge.targetNode]++;
    adjacency[edge.sourceNode].push_back(edge.targetNode);
  }

  std::queue<NodeId> queue;
  for (const auto& [id, degree] : inDegree) {
    if (degree == 0) {
      queue.push(id);
    }
  }

  while (!queue.empty()) {
    NodeId id = queue.front();
    queue.pop();
    result.push_back(id);

    for (NodeId neighbor : adjacency[id]) {
      inDegree[neighbor]--;
      if (inDegree[neighbor] == 0) {
        queue.push(neighbor);
      }
    }
  }

  return result;
}

// ============================================================================
// RoundTripValidator Implementation
// ============================================================================

RoundTripValidator::RoundTripValidator()
    : m_converter(std::make_unique<RoundTripConverter>()),
      m_differ(std::make_unique<GraphDiffer>()), m_normalizer(std::make_unique<IDNormalizer>()) {}

RoundTripValidator::~RoundTripValidator() = default;

RoundTripValidator::ValidationResult
RoundTripValidator::validateTextRoundTrip(const std::string& nmScript) {
  ValidationResult result;
  result.originalText = nmScript;

  auto irResult = m_converter->textToIR(nmScript);
  if (!irResult.isOk()) {
    result.differences.push_back("Failed to convert text to IR: " + irResult.error());
    return result;
  }

  auto textResult = m_converter->irToText(*irResult.value());
  if (!textResult.isOk()) {
    result.differences.push_back("Failed to convert IR back to text: " + textResult.error());
    return result;
  }

  result.roundTrippedText = textResult.value();

  if (result.originalText != result.roundTrippedText) {
    result.differences.push_back("Text differs after round-trip");
  } else {
    result.isValid = true;
  }

  return result;
}

RoundTripValidator::ValidationResult RoundTripValidator::validateIRRoundTrip(const IRGraph& ir) {
  ValidationResult result;

  auto vgResult = m_converter->irToVisualGraph(ir);
  if (!vgResult.isOk()) {
    result.differences.push_back("Failed to convert IR to VisualGraph");
    return result;
  }

  auto ir2Result = m_converter->visualGraphToIR(*vgResult.value());
  if (!ir2Result.isOk()) {
    result.differences.push_back("Failed to convert VisualGraph back to IR");
    return result;
  }

  if (areSemanticalllyEquivalent(ir, *ir2Result.value())) {
    result.isValid = true;
  } else {
    result.differences.push_back("IR differs after round-trip through VisualGraph");
  }

  return result;
}

RoundTripValidator::ValidationResult
RoundTripValidator::validateFullRoundTrip(const std::string& nmScript) {
  ValidationResult result;
  result.originalText = nmScript;

  auto ir1Result = m_converter->textToIR(nmScript);
  if (!ir1Result.isOk()) {
    result.differences.push_back("Failed to convert text to IR: " + ir1Result.error());
    return result;
  }

  auto vgResult = m_converter->irToVisualGraph(*ir1Result.value());
  if (!vgResult.isOk()) {
    result.differences.push_back("Failed to convert IR to VisualGraph");
    return result;
  }

  auto ir2Result = m_converter->visualGraphToIR(*vgResult.value());
  if (!ir2Result.isOk()) {
    result.differences.push_back("Failed to convert VisualGraph to IR");
    return result;
  }

  auto textResult = m_converter->irToText(*ir2Result.value());
  if (!textResult.isOk()) {
    result.differences.push_back("Failed to convert IR to text");
    return result;
  }

  result.roundTrippedText = textResult.value();

  if (result.originalText == result.roundTrippedText) {
    result.isValid = true;
  } else {
    result.differences.push_back("Text differs after full round-trip");
  }

  return result;
}

bool RoundTripValidator::areSemanticalllyEquivalent(const IRGraph& a, const IRGraph& b) const {
  auto nodesA = a.getNodes();
  auto nodesB = b.getNodes();

  if (nodesA.size() != nodesB.size()) {
    return false;
  }

  if (a.getConnections().size() != b.getConnections().size()) {
    return false;
  }

  std::unordered_map<std::string, int> typeCountA;
  std::unordered_map<std::string, int> typeCountB;

  for (const auto* node : nodesA) {
    typeCountA[node->getTypeName()]++;
  }
  for (const auto* node : nodesB) {
    typeCountB[node->getTypeName()]++;
  }

  return typeCountA == typeCountB;
}

} // namespace NovelMind::scripting
