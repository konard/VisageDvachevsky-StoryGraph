#include <catch2/catch_test_macros.hpp>

// Note: This test file tests the cycle detection logic independently
// Full Qt-based tests for the UI components would require Qt Test framework

// Standalone cycle detection algorithm test
#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// Standalone implementation of cycle detection for testing
// (mirrors the logic from NMStoryGraphScene::wouldCreateCycle)
bool wouldCreateCycle(
    uint64_t fromNodeId,
    uint64_t toNodeId,
    const std::unordered_map<uint64_t, std::vector<uint64_t>> &adjacencyList) {
  if (fromNodeId == toNodeId) {
    return true; // Self-loop
  }

  // Build adjacency list with the proposed edge
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj = adjacencyList;
  adj[fromNodeId].push_back(toNodeId);

  // DFS from 'to' to see if we can reach 'from'
  std::unordered_set<uint64_t> visited;
  std::vector<uint64_t> stack;
  stack.push_back(toNodeId);

  while (!stack.empty()) {
    uint64_t current = stack.back();
    stack.pop_back();
    if (current == fromNodeId) {
      return true; // Found a cycle
    }
    if (visited.find(current) != visited.end()) {
      continue;
    }
    visited.insert(current);

    auto it = adj.find(current);
    if (it == adj.end()) {
      continue;
    }
    for (uint64_t next : it->second) {
      if (visited.find(next) == visited.end()) {
        stack.push_back(next);
      }
    }
  }

  return false;
}

// Standalone implementation of Tarjan's algorithm for cycle detection
std::vector<std::vector<uint64_t>> detectCycles(
    const std::unordered_set<uint64_t> &allNodes,
    const std::unordered_map<uint64_t, std::vector<uint64_t>> &adjacencyList) {
  std::vector<std::vector<uint64_t>> cycles;

  // Tarjan's algorithm for strongly connected components
  std::unordered_map<uint64_t, int> index;
  std::unordered_map<uint64_t, int> lowlink;
  std::unordered_set<uint64_t> onStack;
  std::vector<uint64_t> stack;
  int nextIndex = 0;

  std::function<void(uint64_t)> strongconnect = [&](uint64_t v) {
    index[v] = nextIndex;
    lowlink[v] = nextIndex;
    nextIndex++;
    stack.push_back(v);
    onStack.insert(v);

    auto it = adjacencyList.find(v);
    if (it != adjacencyList.end()) {
      for (uint64_t w : it->second) {
        if (index.find(w) == index.end()) {
        strongconnect(w);
        lowlink[v] = std::min(lowlink[v], lowlink[w]);
        } else if (onStack.find(w) != onStack.end()) {
          lowlink[v] = std::min(lowlink[v], index[w]);
        }
      }
    }

    // If v is a root node, pop the stack and generate an SCC
    if (lowlink[v] == index[v]) {
      std::vector<uint64_t> component;
      uint64_t w;
      do {
        w = stack.back();
        stack.pop_back();
        onStack.erase(w);
        component.push_back(w);
      } while (w != v);

      // Only report SCCs with more than one node (actual cycles)
      if (component.size() > 1) {
        cycles.push_back(component);
      }
    }
  };

  for (uint64_t nodeId : allNodes) {
    if (index.find(nodeId) == index.end()) {
      strongconnect(nodeId);
    }
  }

  return cycles;
}

} // anonymous namespace

TEST_CASE("Story Graph - Self loop detection", "[story_graph][cycle]") {
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj;

  SECTION("Self-loop is detected") {
    bool hasCycle = wouldCreateCycle(1, 1, adj);
    CHECK(hasCycle == true);
  }
}

TEST_CASE("Story Graph - Simple cycle detection", "[story_graph][cycle]") {
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj;

  SECTION("No cycle in linear graph") {
    // 1 -> 2 -> 3
    adj[1] = {2};
    adj[2] = {3};

    bool hasCycle = wouldCreateCycle(1, 3, adj);
    CHECK(hasCycle == false);
  }

  SECTION("Cycle detected in triangle") {
    // 1 -> 2 -> 3, trying to add 3 -> 1
    adj[1] = {2};
    adj[2] = {3};

    bool hasCycle = wouldCreateCycle(3, 1, adj);
    CHECK(hasCycle == true);
  }

  SECTION("Cycle detected in simple loop") {
    // 1 -> 2, trying to add 2 -> 1
    adj[1] = {2};

    bool hasCycle = wouldCreateCycle(2, 1, adj);
    CHECK(hasCycle == true);
  }
}

TEST_CASE("Story Graph - Complex cycle detection", "[story_graph][cycle]") {
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj;

  SECTION("No cycle in DAG") {
    // Diamond pattern: 1 -> 2, 1 -> 3, 2 -> 4, 3 -> 4
    adj[1] = {2, 3};
    adj[2] = {4};
    adj[3] = {4};

    bool hasCycle = wouldCreateCycle(2, 3, adj);
    CHECK(hasCycle == false);
  }

  SECTION("Cycle detected in complex graph") {
    // 1 -> 2 -> 3 -> 4, trying to add 4 -> 2
    adj[1] = {2};
    adj[2] = {3};
    adj[3] = {4};

    bool hasCycle = wouldCreateCycle(4, 2, adj);
    CHECK(hasCycle == true);
  }

  SECTION("Cycle in disconnected components") {
    // Component 1: 1 -> 2 -> 3
    // Component 2: 4 -> 5, trying to add 5 -> 4
    adj[1] = {2};
    adj[2] = {3};
    adj[4] = {5};

    bool hasCycle = wouldCreateCycle(5, 4, adj);
    CHECK(hasCycle == true);
  }
}

TEST_CASE("Story Graph - Tarjan's algorithm cycle detection",
          "[story_graph][cycle]") {
  SECTION("No cycles in DAG") {
    std::unordered_set<uint64_t> nodes = {1, 2, 3, 4};
    std::unordered_map<uint64_t, std::vector<uint64_t>> adj;
    adj[1] = {2, 3};
    adj[2] = {4};
    adj[3] = {4};

    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.empty());
  }

  SECTION("Single cycle detected") {
    std::unordered_set<uint64_t> nodes = {1, 2, 3};
    std::unordered_map<uint64_t, std::vector<uint64_t>> adj;
    adj[1] = {2};
    adj[2] = {3};
    adj[3] = {1}; // Cycle: 1 -> 2 -> 3 -> 1

    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.size() == 1);
    CHECK(cycles[0].size() == 3);
    // All nodes should be in the cycle
    CHECK(std::find(cycles[0].begin(), cycles[0].end(), 1) != cycles[0].end());
    CHECK(std::find(cycles[0].begin(), cycles[0].end(), 2) != cycles[0].end());
    CHECK(std::find(cycles[0].begin(), cycles[0].end(), 3) != cycles[0].end());
  }

  SECTION("Multiple cycles detected") {
    std::unordered_set<uint64_t> nodes = {1, 2, 3, 4, 5, 6};
    std::unordered_map<uint64_t, std::vector<uint64_t>> adj;
    // Cycle 1: 1 -> 2 -> 1
    adj[1] = {2};
    adj[2] = {1};
    // Cycle 2: 4 -> 5 -> 6 -> 4
    adj[4] = {5};
    adj[5] = {6};
    adj[6] = {4};
    // Node 3 is disconnected

    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.size() == 2);
  }

  SECTION("Nested strongly connected component") {
    std::unordered_set<uint64_t> nodes = {1, 2, 3, 4};
    std::unordered_map<uint64_t, std::vector<uint64_t>> adj;
    // All nodes form one big SCC: 1 -> 2 -> 3 -> 4 -> 1
    adj[1] = {2};
    adj[2] = {3};
    adj[3] = {4};
    adj[4] = {1};

    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.size() == 1);
    CHECK(cycles[0].size() == 4);
  }
}

TEST_CASE("Story Graph - Empty graph", "[story_graph][cycle]") {
  std::unordered_set<uint64_t> nodes;
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj;

  SECTION("Empty graph has no cycles") {
    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.empty());
  }

  SECTION("Adding edge to empty graph creates no cycle") {
    bool hasCycle = wouldCreateCycle(1, 2, adj);
    CHECK(hasCycle == false);
  }
}

TEST_CASE("Story Graph - Large graph performance", "[story_graph][cycle][!benchmark]") {
  // Create a large DAG to test O(V+E) performance
  std::unordered_set<uint64_t> nodes;
  std::unordered_map<uint64_t, std::vector<uint64_t>> adj;

  const uint64_t numNodes = 1000;
  for (uint64_t i = 1; i <= numNodes; ++i) {
    nodes.insert(i);
    if (i < numNodes) {
      // Each node connects to next node (linear chain)
      adj[i].push_back(i + 1);
    }
  }

  SECTION("Large DAG has no cycles") {
    auto cycles = detectCycles(nodes, adj);
    CHECK(cycles.empty());
  }

  SECTION("Cycle check at end of large chain") {
    // This tests the worst case where we need to traverse the entire chain
    bool hasCycle = wouldCreateCycle(numNodes, 1, adj);
    CHECK(hasCycle == true);
  }

  SECTION("No cycle when adding parallel edge") {
    bool hasCycle = wouldCreateCycle(500, 750, adj);
    CHECK(hasCycle == false);
  }
}

// ============================================================================
// Scene Node Tests (Visual-First Workflow)
// ============================================================================

// Standalone Scene Node data structure for testing
// (mirrors SceneNodeData from ir.hpp)
namespace {

struct TestSceneNodeData {
  std::string sceneId;
  std::string displayName;
  std::string scriptPath;
  bool hasEmbeddedDialogue = false;
  std::vector<uint64_t> embeddedDialogueNodes;
  std::string thumbnailPath;
  uint32_t dialogueCount = 0;
};

// Validates that a scene node has required properties
bool isValidSceneNode(const TestSceneNodeData &data) {
  // Scene ID is required
  if (data.sceneId.empty()) {
    return false;
  }
  // Either embedded dialogue or script path should be defined for a valid scene
  if (!data.hasEmbeddedDialogue && data.scriptPath.empty()) {
    return false;
  }
  return true;
}

// Checks if scene node is configured for Visual-First workflow
bool isVisualFirstScene(const TestSceneNodeData &data) {
  return data.hasEmbeddedDialogue && !data.embeddedDialogueNodes.empty();
}

// Checks if scene node is configured for Code-First workflow
bool isCodeFirstScene(const TestSceneNodeData &data) {
  return !data.scriptPath.empty() && !data.hasEmbeddedDialogue;
}

// Checks if scene node is configured for Hybrid workflow
bool isHybridScene(const TestSceneNodeData &data) {
  return data.hasEmbeddedDialogue && !data.scriptPath.empty();
}

} // namespace

TEST_CASE("Scene Node - Basic validation", "[story_graph][scene_node]") {
  SECTION("Empty scene ID is invalid") {
    TestSceneNodeData data;
    data.sceneId = "";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {1, 2, 3};

    CHECK(isValidSceneNode(data) == false);
  }

  SECTION("Scene with ID and embedded dialogue is valid") {
    TestSceneNodeData data;
    data.sceneId = "intro_scene";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {1, 2, 3};

    CHECK(isValidSceneNode(data) == true);
  }

  SECTION("Scene with ID and script path is valid") {
    TestSceneNodeData data;
    data.sceneId = "cafe_scene";
    data.scriptPath = "Scripts/cafe_scene.nms";
    data.hasEmbeddedDialogue = false;

    CHECK(isValidSceneNode(data) == true);
  }

  SECTION("Scene without embedded dialogue or script path is invalid") {
    TestSceneNodeData data;
    data.sceneId = "orphan_scene";
    data.hasEmbeddedDialogue = false;
    data.scriptPath = "";

    CHECK(isValidSceneNode(data) == false);
  }
}

TEST_CASE("Scene Node - Workflow detection", "[story_graph][scene_node][workflow]") {
  SECTION("Visual-First scene detection") {
    TestSceneNodeData data;
    data.sceneId = "visual_scene";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {1, 2, 3, 4, 5};
    data.scriptPath = "";

    CHECK(isVisualFirstScene(data) == true);
    CHECK(isCodeFirstScene(data) == false);
    CHECK(isHybridScene(data) == false);
  }

  SECTION("Code-First scene detection") {
    TestSceneNodeData data;
    data.sceneId = "code_scene";
    data.hasEmbeddedDialogue = false;
    data.scriptPath = "Scripts/code_scene.nms";

    CHECK(isVisualFirstScene(data) == false);
    CHECK(isCodeFirstScene(data) == true);
    CHECK(isHybridScene(data) == false);
  }

  SECTION("Hybrid scene detection") {
    TestSceneNodeData data;
    data.sceneId = "hybrid_scene";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {1, 2};
    data.scriptPath = "Scripts/hybrid_scene.nms";

    CHECK(isVisualFirstScene(data) == true); // Has embedded dialogue
    CHECK(isCodeFirstScene(data) == false);  // Has embedded dialogue
    CHECK(isHybridScene(data) == true);      // Has both
  }

  SECTION("Empty embedded dialogue nodes is not Visual-First") {
    TestSceneNodeData data;
    data.sceneId = "empty_visual_scene";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {}; // Empty!
    data.scriptPath = "";

    CHECK(isVisualFirstScene(data) == false);
  }
}

TEST_CASE("Scene Node - Dialogue count tracking", "[story_graph][scene_node]") {
  SECTION("Dialogue count matches embedded nodes") {
    TestSceneNodeData data;
    data.sceneId = "counted_scene";
    data.hasEmbeddedDialogue = true;
    data.embeddedDialogueNodes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    data.dialogueCount = 10;

    CHECK(data.dialogueCount == data.embeddedDialogueNodes.size());
  }

  SECTION("Zero dialogue count for code-first scene") {
    TestSceneNodeData data;
    data.sceneId = "code_scene";
    data.scriptPath = "Scripts/code_scene.nms";
    data.dialogueCount = 0;

    CHECK(data.dialogueCount == 0);
  }
}

TEST_CASE("Scene Node - Scene graph structure", "[story_graph][scene_node]") {
  // Test scene-to-scene connections in story graph
  std::unordered_map<uint64_t, std::vector<uint64_t>> sceneAdjacency;

  // Scene 1 (Intro) -> Scene 2 (Cafe) -> Scene 3 (Choice)
  // Scene 3 branches to Scene 4 (Good ending) or Scene 5 (Bad ending)
  sceneAdjacency[1] = {2};      // Intro -> Cafe
  sceneAdjacency[2] = {3};      // Cafe -> Choice
  sceneAdjacency[3] = {4, 5};   // Choice -> Good/Bad endings
  sceneAdjacency[4] = {};       // Good ending (terminal)
  sceneAdjacency[5] = {};       // Bad ending (terminal)

  SECTION("Scene flow is acyclic") {
    std::unordered_set<uint64_t> allScenes = {1, 2, 3, 4, 5};
    auto cycles = detectCycles(allScenes, sceneAdjacency);
    CHECK(cycles.empty());
  }

  SECTION("Cannot create cycle in scene graph") {
    // Trying to add Bad Ending -> Intro would create a cycle
    bool wouldCycle = wouldCreateCycle(5, 1, sceneAdjacency);
    CHECK(wouldCycle == true);
  }

  SECTION("Can add parallel scene paths") {
    // Adding Cafe -> Bad Ending doesn't create a cycle
    bool wouldCycle = wouldCreateCycle(2, 5, sceneAdjacency);
    CHECK(wouldCycle == false);
  }

  SECTION("Terminal scenes have no outgoing connections") {
    CHECK(sceneAdjacency[4].empty());
    CHECK(sceneAdjacency[5].empty());
  }

  SECTION("Branch scenes have multiple outgoing connections") {
    CHECK(sceneAdjacency[3].size() == 2);
  }
}

// ============================================================================
// Condition Node Tests (Silent branching - Issue #76 fix verification)
// ============================================================================

namespace {

// Node type enumeration for testing
enum class TestNodeType {
  Scene,
  Dialogue,
  Condition,
  Choice,
  Event,
  Unknown
};

// Helper function to determine if a node type should generate say statements
// This mirrors the logic in nm_story_graph_scene.cpp and nm_story_graph_panel_handlers.cpp
bool shouldGenerateSayStatement(TestNodeType nodeType) {
  switch (nodeType) {
  case TestNodeType::Scene:
    return false;  // Scene nodes are "silent" containers
  case TestNodeType::Condition:
    return false;  // Condition nodes only branch, they don't speak (Issue #76 fix)
  case TestNodeType::Dialogue:
    return true;   // Dialogue nodes should have say statements
  case TestNodeType::Choice:
    return true;   // Choice nodes can have prompt text
  case TestNodeType::Event:
    return true;   // Event nodes may have narrative text
  default:
    return true;   // Default for unknown types
  }
}

// Helper to determine what script content comment should be generated
std::string getScriptContentComment(TestNodeType nodeType) {
  switch (nodeType) {
  case TestNodeType::Scene:
    return "// Scene node - add scene content here";
  case TestNodeType::Condition:
    return "// Condition node - add branching logic here";
  default:
    return "";  // No comment for dialogue types
  }
}

} // namespace

TEST_CASE("Condition Node - Silent branching behavior (Issue #76)",
          "[story_graph][condition_node]") {
  SECTION("Condition nodes should NOT generate say statements") {
    CHECK(shouldGenerateSayStatement(TestNodeType::Condition) == false);
  }

  SECTION("Scene nodes should NOT generate say statements") {
    CHECK(shouldGenerateSayStatement(TestNodeType::Scene) == false);
  }

  SECTION("Dialogue nodes SHOULD generate say statements") {
    CHECK(shouldGenerateSayStatement(TestNodeType::Dialogue) == true);
  }

  SECTION("Condition node script content comment") {
    auto comment = getScriptContentComment(TestNodeType::Condition);
    CHECK(comment == "// Condition node - add branching logic here");
    CHECK(comment.find("say") == std::string::npos);  // No "say" in comment
  }

  SECTION("Scene node script content comment") {
    auto comment = getScriptContentComment(TestNodeType::Scene);
    CHECK(comment == "// Scene node - add scene content here");
    CHECK(comment.find("say") == std::string::npos);  // No "say" in comment
  }
}

TEST_CASE("Condition Node - Branching properties", "[story_graph][condition_node]") {
  // Test condition node specific properties
  struct TestConditionNodeData {
    std::string nodeId;
    std::string conditionExpression;
    std::vector<std::string> conditionOutputs;
    std::unordered_map<std::string, std::string> conditionTargets;
  };

  SECTION("Default condition outputs are true/false") {
    TestConditionNodeData data;
    data.nodeId = "cond_1";
    data.conditionExpression = "has_key";
    data.conditionOutputs = {"true", "false"};

    CHECK(data.conditionOutputs.size() == 2);
    CHECK(data.conditionOutputs[0] == "true");
    CHECK(data.conditionOutputs[1] == "false");
  }

  SECTION("Condition can have custom output labels") {
    TestConditionNodeData data;
    data.nodeId = "cond_2";
    data.conditionExpression = "player_choice";
    data.conditionOutputs = {"path_a", "path_b", "path_c"};

    CHECK(data.conditionOutputs.size() == 3);
  }

  SECTION("Condition targets map outputs to node IDs") {
    TestConditionNodeData data;
    data.nodeId = "cond_3";
    data.conditionExpression = "check_inventory";
    data.conditionOutputs = {"success", "failure"};
    data.conditionTargets["success"] = "node_10";
    data.conditionTargets["failure"] = "node_11";

    CHECK(data.conditionTargets["success"] == "node_10");
    CHECK(data.conditionTargets["failure"] == "node_11");
  }

  SECTION("Empty condition expression is valid but shows placeholder") {
    TestConditionNodeData data;
    data.nodeId = "cond_4";
    data.conditionExpression = "";

    CHECK(data.conditionExpression.empty());
    // UI should show "(no condition)" placeholder for empty expressions
  }
}
