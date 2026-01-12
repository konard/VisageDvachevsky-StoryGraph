#pragma once

/**
 * @file script_reference_map.hpp
 * @brief Script Reference Map for NovelMind
 *
 * Analyzes and visualizes script references:
 * - Call graphs (who calls who)
 * - Variable modification tracking
 * - Story flow diagram generation
 * - Branch analysis
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ast.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Type of reference between script elements
 */
enum class ReferenceType : u8 {
  SceneCall,          // Scene calls another scene
  CharacterReference, // Script references a character
  VariableRead,       // Script reads a variable
  VariableWrite,      // Script writes a variable
  FlagRead,           // Script reads a flag
  FlagWrite,          // Script sets a flag
  AssetReference,     // Script references an asset
  LabelJump,          // Goto/jump to a label
  FunctionCall        // Script calls a function
};

/**
 * @brief A single reference in the script
 */
struct ScriptReference {
  ReferenceType type;
  std::string sourceId;   // ID of the source element
  std::string sourceName; // Human-readable source name
  std::string targetId;   // ID of the target element
  std::string targetName; // Human-readable target name
  std::string filePath;   // Source file
  i32 lineNumber = -1;    // Line number
  std::string context;    // Surrounding code snippet
};

/**
 * @brief Node in the call graph
 */
struct CallGraphNode {
  std::string id;
  std::string name;
  std::string type; // "scene", "label", "function"
  std::string filePath;

  std::vector<std::string> callers; // IDs of nodes that call this
  std::vector<std::string> callees; // IDs of nodes this calls

  // Statistics
  i32 inDegree = 0;  // Number of incoming edges
  i32 outDegree = 0; // Number of outgoing edges
  i32 lineCount = 0; // Lines of script

  // Analysis results
  bool isEntryPoint = false;
  bool isEndpoint = false;
  bool isUnreachable = false;
  bool isInCycle = false;

  // Layout (for visualization)
  f32 x = 0.0f;
  f32 y = 0.0f;
};

/**
 * @brief Variable usage information
 */
struct VariableUsage {
  std::string variableName;
  std::string initialValue;

  struct Usage {
    std::string location; // Scene/function ID
    std::string filePath;
    i32 lineNumber;
    bool isWrite;
    std::string expression; // The expression used
  };

  std::vector<Usage> reads;
  std::vector<Usage> writes;

  // Analysis
  bool isUnused = false;
  bool isWriteOnly = false;
  bool isReadOnly = false;
  bool hasConflictingWrites = false;
};

/**
 * @brief Branch information for story analysis
 */
struct BranchInfo {
  std::string choiceId;
  std::string choiceText;
  std::string condition;
  std::string targetSceneId;
  std::string sourceSceneId;
  i32 lineNumber;

  // Statistics
  i32 pathDepth = 0;               // How deep this branch goes
  i32 uniqueEndings = 0;           // Unique endings reachable from here
  f32 estimatedPlaythrough = 0.0f; // Estimated % of players taking this path
};

/**
 * @brief Story path analysis
 */
struct StoryPath {
  std::string id;
  std::string name;
  std::vector<std::string> nodeSequence; // Ordered list of scene/node IDs
  std::vector<std::string> choicesMade;  // Choices required to reach this path
  std::string ending;
  i32 estimatedDuration = 0; // Minutes

  // Requirements
  std::vector<std::string> requiredFlags;
  std::vector<std::string> requiredVariables;
};

/**
 * @brief Complete reference map for a project
 */
struct ReferenceMap {
  // All references
  std::vector<ScriptReference> references;

  // Call graph
  std::unordered_map<std::string, CallGraphNode> callGraph;

  // Variable usage
  std::unordered_map<std::string, VariableUsage> variableUsage;

  // Branch analysis
  std::vector<BranchInfo> branches;
  std::vector<StoryPath> storyPaths;

  // Statistics
  i32 totalScenes = 0;
  i32 totalCharacters = 0;
  i32 totalVariables = 0;
  i32 totalFlags = 0;
  i32 totalChoices = 0;
  i32 totalEndings = 0;
  i32 reachableScenes = 0;
  i32 unreachableScenes = 0;
  i32 cyclicPaths = 0;

  // Entry and exit points
  std::vector<std::string> entryPoints;
  std::vector<std::string> endpoints;

  // Generation info
  u64 generatedTimestamp = 0;
  f64 generationTimeMs = 0.0;
};

/**
 * @brief Configuration for reference map generation
 */
struct ReferenceMapConfig {
  bool analyzeCallGraph = true;
  bool analyzeVariables = true;
  bool analyzeFlags = true;
  bool analyzeBranches = true;
  bool analyzeStoryPaths = true;

  bool findUnreachable = true;
  bool findCycles = true;
  bool findUnused = true;

  // Path analysis limits
  i32 maxPathDepth = 100;
  i32 maxPaths = 1000;

  // Files to exclude
  std::vector<std::string> excludePatterns;
};

/**
 * @brief Listener for reference map generation progress
 */
class IReferenceMapListener {
public:
  virtual ~IReferenceMapListener() = default;

  virtual void onAnalysisStarted() = 0;
  virtual void onAnalysisProgress(const std::string& currentTask, f32 progress) = 0;
  virtual void onAnalysisCompleted(const ReferenceMap& map) = 0;
};

/**
 * @brief Script Reference Map Analyzer
 *
 * Analyzes script files to build a comprehensive reference map:
 * - Who calls who (call graph)
 * - Variable read/write tracking
 * - Story flow analysis
 * - Reachability analysis
 */
class ScriptReferenceAnalyzer {
public:
  ScriptReferenceAnalyzer();
  ~ScriptReferenceAnalyzer() = default;

  /**
   * @brief Set project path
   */
  void setProjectPath(const std::string& projectPath);

  /**
   * @brief Set configuration
   */
  void setConfig(const ReferenceMapConfig& config);

  /**
   * @brief Analyze the project and generate reference map
   */
  Result<ReferenceMap> analyze();

  /**
   * @brief Get cached reference map
   */
  [[nodiscard]] const ReferenceMap& getReferenceMap() const { return m_referenceMap; }

  /**
   * @brief Check if cache is valid
   */
  [[nodiscard]] bool isCacheValid() const;

  /**
   * @brief Invalidate cache
   */
  void invalidateCache();

  /**
   * @brief Add listener
   */
  void addListener(IReferenceMapListener* listener);

  /**
   * @brief Remove listener
   */
  void removeListener(IReferenceMapListener* listener);

  // Query methods

  /**
   * @brief Get all references to a scene
   */
  [[nodiscard]] std::vector<ScriptReference> getReferencesToScene(const std::string& sceneId) const;

  /**
   * @brief Get all references from a scene
   */
  [[nodiscard]] std::vector<ScriptReference>
  getReferencesFromScene(const std::string& sceneId) const;

  /**
   * @brief Get all uses of a variable
   */
  [[nodiscard]] const VariableUsage* getVariableUsage(const std::string& variableName) const;

  /**
   * @brief Get all scenes that can reach a target scene
   */
  [[nodiscard]] std::vector<std::string> getScenesReaching(const std::string& targetSceneId) const;

  /**
   * @brief Get all scenes reachable from a source scene
   */
  [[nodiscard]] std::vector<std::string>
  getScenesReachableFrom(const std::string& sourceSceneId) const;

  /**
   * @brief Get shortest path between two scenes
   */
  [[nodiscard]] std::vector<std::string> getShortestPath(const std::string& from,
                                                         const std::string& to) const;

  /**
   * @brief Get all paths to endings
   */
  [[nodiscard]] std::vector<StoryPath> getPathsToEnding(const std::string& endingId) const;

  /**
   * @brief Get call depth of a scene
   */
  [[nodiscard]] i32 getCallDepth(const std::string& sceneId) const;

private:
  void parseScriptFiles();
  void buildCallGraph();
  void analyzeVariableUsage();
  void analyzeBranches();
  void analyzeStoryPaths();
  void detectCycles();
  void detectUnreachable();
  void layoutCallGraph();

  void reportProgress(const std::string& task, f32 progress);

  std::string m_projectPath;
  ReferenceMapConfig m_config;
  ReferenceMap m_referenceMap;

  bool m_cacheValid = false;
  u64 m_lastAnalysisTime = 0;

  std::vector<IReferenceMapListener*> m_listeners;
};

/**
 * @brief Reference Map Visualizer
 *
 * Generates visual representations of the reference map:
 * - Call graph diagram
 * - Story flow diagram
 * - Variable dependency graph
 */
class ReferenceMapVisualizer {
public:
  ReferenceMapVisualizer();
  ~ReferenceMapVisualizer() = default;

  /**
   * @brief Set reference map to visualize
   */
  void setReferenceMap(const ReferenceMap* map);

  /**
   * @brief Export call graph as DOT (Graphviz) format
   */
  Result<std::string> exportCallGraphDOT() const;

  /**
   * @brief Export call graph as SVG
   */
  Result<void> exportCallGraphSVG(const std::string& outputPath) const;

  /**
   * @brief Export story flow as Mermaid diagram
   */
  Result<std::string> exportStoryFlowMermaid() const;

  /**
   * @brief Export variable usage as HTML table
   */
  Result<std::string> exportVariableUsageHTML() const;

  /**
   * @brief Export complete analysis report as HTML
   */
  Result<void> exportAnalysisReport(const std::string& outputPath) const;

  // Rendering for editor integration

  /**
   * @brief Render call graph overlay for StoryGraph panel
   */
  void renderCallGraphOverlay(renderer::IRenderer* renderer, f32 viewX, f32 viewY, f32 zoom);

  /**
   * @brief Render variable usage highlighting
   */
  void renderVariableHighlights(renderer::IRenderer* renderer, const std::string& variableName);

  // Filtering

  /**
   * @brief Set filter for visualization
   */
  void setFilter(const std::string& filter);

  /**
   * @brief Show only paths to a specific node
   */
  void showPathsTo(const std::string& nodeId);

  /**
   * @brief Show only paths from a specific node
   */
  void showPathsFrom(const std::string& nodeId);

  /**
   * @brief Reset filters
   */
  void resetFilters();

private:
  const ReferenceMap* m_map = nullptr;

  std::string m_currentFilter;
  std::string m_pathsToNode;
  std::string m_pathsFromNode;

  // Cached visualization data
  std::vector<std::pair<CallGraphNode, CallGraphNode>> m_visibleEdges;
  std::unordered_set<std::string> m_highlightedNodes;
};

/**
 * @brief Script Reference Map Panel
 */
class ScriptReferenceMapPanel {
public:
  ScriptReferenceMapPanel();
  ~ScriptReferenceMapPanel();

  void update(f64 deltaTime);
  void render();
  void onResize(i32 width, i32 height);

  void setAnalyzer(ScriptReferenceAnalyzer* analyzer);

  // Actions
  void refreshAnalysis();
  void exportReport(const std::string& path);

  // View modes
  enum class ViewMode : u8 { CallGraph, VariableUsage, StoryPaths, Statistics };
  void setViewMode(ViewMode mode) { m_viewMode = mode; }
  [[nodiscard]] ViewMode getViewMode() const { return m_viewMode; }

  // Selection
  void selectNode(const std::string& nodeId);
  void selectVariable(const std::string& variableName);

  // Callbacks
  void setOnNodeSelected(std::function<void(const std::string&)> callback);
  void setOnNavigateToSource(std::function<void(const std::string&, i32)> callback);

private:
  void renderCallGraphView();
  void renderVariableUsageView();
  void renderStoryPathsView();
  void renderStatisticsView();
  void renderToolbar();
  void renderDetailsPanel();

  ScriptReferenceAnalyzer* m_analyzer = nullptr;
  ReferenceMapVisualizer m_visualizer;

  ViewMode m_viewMode = ViewMode::CallGraph;

  std::string m_selectedNodeId;
  std::string m_selectedVariableName;
  std::string m_searchFilter;

  // View state
  f32 m_viewX = 0.0f;
  f32 m_viewY = 0.0f;
  f32 m_viewZoom = 1.0f;

  // Callbacks
  std::function<void(const std::string&)> m_onNodeSelected;
  std::function<void(const std::string&, i32)> m_onNavigateToSource;
};

} // namespace NovelMind::editor
