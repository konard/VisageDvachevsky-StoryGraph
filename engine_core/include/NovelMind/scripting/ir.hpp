#pragma once

/**
 * @file ir.hpp
 * @brief Intermediate Representation (IR) for NM Script
 *
 * The IR serves as the bridge between:
 * - NM Script text (parsed AST)
 * - Visual Graph (node-based editor)
 * - Bytecode (runtime execution)
 *
 * This enables bidirectional conversion:
 * - AST -> IR -> Visual Graph
 * - Visual Graph -> IR -> AST -> NM Script text
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ast.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace NovelMind::scripting {

// Forward declarations
class IRNode;
class IRGraph;

/**
 * @brief Unique identifier for IR nodes
 */
using NodeId = u64;

/**
 * @brief Port identifier for node connections
 */
struct PortId {
  NodeId nodeId;
  std::string portName;
  bool isOutput;

  bool operator==(const PortId &other) const {
    return nodeId == other.nodeId && portName == other.portName &&
           isOutput == other.isOutput;
  }
};

/**
 * @brief Connection between two nodes
 */
struct IRConnection {
  PortId source;     // Output port
  PortId target;     // Input port
  std::string label; // Optional label for debugging
};

/**
 * @brief IR Node types
 */
enum class IRNodeType : u8 {
  // Structure
  SceneStart, // Entry point for a scene
  SceneEnd,   // Exit point
  Comment,    // Documentation node
  Scene,      // Complete scene node (Visual-First workflow)

  // Flow control
  Sequence, // Sequential execution
  Branch,   // Conditional branch (if/else)
  Switch,   // Multi-way branch
  Loop,     // Loop construct
  Goto,     // Jump to label/scene
  Label,    // Jump target

  // VN-specific
  ShowCharacter,  // Display character
  HideCharacter,  // Hide character
  ShowBackground, // Display background
  Dialogue,       // Say text
  Choice,         // Player choice
  ChoiceOption,   // Single choice option

  // Audio
  PlayMusic, // Play music track
  StopMusic, // Stop music
  PlaySound, // Play sound effect

  // Effects
  Transition,  // Scene transition effect
  Wait,        // Delay execution
  SetVariable, // Set variable value
  GetVariable, // Read variable value

  // Advanced
  Expression,   // Expression evaluation
  FunctionCall, // Call a function
  Custom        // User-defined node
};

/**
 * @brief Development workflow modes for scene creation
 */
enum class DevelopmentWorkflow : u8 {
  VisualFirst, // Use embedded dialogue graphs in scene nodes
  CodeFirst,   // Use NMScript files for dialogue logic
  Hybrid       // Allow both approaches per scene
};

/**
 * @brief Voice-over binding status for dialogue nodes
 */
enum class VoiceBindingStatus : u8 {
  Unbound,     // No voice file assigned
  Bound,       // Voice file assigned and verified
  MissingFile, // Voice file assigned but file not found
  AutoMapped,  // Automatically mapped by pattern matching
  Pending      // Waiting for import
};

/**
 * @brief Voice clip data for dialogue nodes
 */
struct VoiceClipData {
  std::string voiceFilePath;           // Path to voice audio file
  std::string localizationKey;         // Localization key for auto-detection
  VoiceBindingStatus bindingStatus;    // Status of voice binding
  f32 voiceDuration;                   // Cached duration in seconds
  bool autoDetected;                   // Whether this was auto-detected

  VoiceClipData()
      : bindingStatus(VoiceBindingStatus::Unbound),
        voiceDuration(0.0f),
        autoDetected(false) {}
};

/**
 * @brief Translation status for localized dialogue
 */
enum class TranslationStatus : u8 {
  NotLocalizable,   // Text is not meant to be localized (e.g., system text)
  Untranslated,     // No translation exists for the current locale
  Translated,       // Translation exists and is complete
  NeedsReview,      // Translation exists but may be outdated
  Missing           // Key exists but translation is empty
};

/**
 * @brief Localization data for dialogue nodes
 *
 * Stores localization key and translation status for dialogue text.
 * Keys follow the format: scene.{sceneId}.dialogue.{nodeId}
 */
struct DialogueLocalizationData {
  std::string localizationKey;       // Auto-generated or custom key
  std::string customKeyOverride;     // Optional manual key override
  TranslationStatus status;          // Current translation status
  bool useCustomKey;                 // Whether to use custom key instead of auto-generated

  DialogueLocalizationData()
      : status(TranslationStatus::Untranslated),
        useCustomKey(false) {}

  /**
   * @brief Get the effective localization key
   * @return Custom key if set, otherwise the auto-generated key
   */
  [[nodiscard]] const std::string& getEffectiveKey() const {
    return useCustomKey && !customKeyOverride.empty()
           ? customKeyOverride
           : localizationKey;
  }

  /**
   * @brief Generate a localization key for a dialogue node
   * @param sceneId The scene identifier
   * @param nodeId The node identifier
   * @return Generated key in format scene.{sceneId}.dialogue.{nodeId}
   */
  static std::string generateKey(const std::string& sceneId, NodeId nodeId) {
    return "scene." + sceneId + ".dialogue." + std::to_string(nodeId);
  }

  /**
   * @brief Generate a localization key for a choice option
   * @param sceneId The scene identifier
   * @param nodeId The choice node identifier
   * @param optionIndex The option index (0-based)
   * @return Generated key in format scene.{sceneId}.choice.{nodeId}.{optionIndex}
   */
  static std::string generateChoiceKey(const std::string& sceneId,
                                        NodeId nodeId,
                                        size_t optionIndex) {
    return "scene." + sceneId + ".choice." + std::to_string(nodeId) +
           "." + std::to_string(optionIndex);
  }
};

/**
 * @brief Data structure for Scene Node properties
 *
 * Represents a complete scene with visual layout and embedded dialogue.
 */
struct SceneNodeData {
  std::string sceneId;            // Unique identifier for the scene
  std::string displayName;        // User-friendly name
  std::string scriptPath;         // Path to .nms file (Code-First workflow)
  bool hasEmbeddedDialogue;       // Uses embedded dialogue graph (Visual-First)
  std::vector<NodeId> embeddedDialogueNodes; // Node IDs of embedded dialogue graph
  std::string thumbnailPath;      // Optional scene preview thumbnail
  u32 dialogueCount;              // Number of dialogue nodes in scene

  SceneNodeData()
      : hasEmbeddedDialogue(false), dialogueCount(0) {}
};

/**
 * @brief Property value types for IR nodes
 */
using IRPropertyValue = std::variant<std::nullptr_t, bool, i64, f64,
                                     std::string, std::vector<std::string>>;

/**
 * @brief Port definition for a node type
 */
struct PortDefinition {
  std::string name;
  std::string displayName;
  bool isExecution = false; // True for flow ports, false for data ports
  bool required = false;
  std::string defaultValue;
};

/**
 * @brief IR Node - base unit of the IR graph
 */
class IRNode {
public:
  IRNode(NodeId id, IRNodeType type);
  virtual ~IRNode() = default;

  // Identity
  [[nodiscard]] NodeId getId() const { return m_id; }
  [[nodiscard]] IRNodeType getType() const { return m_type; }
  [[nodiscard]] const char *getTypeName() const;

  // Properties
  void setProperty(const std::string &name, const IRPropertyValue &value);
  [[nodiscard]] std::optional<IRPropertyValue>
  getProperty(const std::string &name) const;
  [[nodiscard]] const std::unordered_map<std::string, IRPropertyValue> &
  getProperties() const {
    return m_properties;
  }

  // Convenience property accessors
  [[nodiscard]] std::string
  getStringProperty(const std::string &name,
                    const std::string &defaultValue = "") const;
  [[nodiscard]] i64 getIntProperty(const std::string &name,
                                   i64 defaultValue = 0) const;
  [[nodiscard]] f64 getFloatProperty(const std::string &name,
                                     f64 defaultValue = 0.0) const;
  [[nodiscard]] bool getBoolProperty(const std::string &name,
                                     bool defaultValue = false) const;

  // Source location (for error reporting and debugging)
  void setSourceLocation(const SourceLocation &loc);
  [[nodiscard]] const SourceLocation &getSourceLocation() const {
    return m_location;
  }

  // Visual position (for graph editor)
  void setPosition(f32 x, f32 y);
  [[nodiscard]] f32 getX() const { return m_x; }
  [[nodiscard]] f32 getY() const { return m_y; }

  // Port definitions (based on type)
  [[nodiscard]] std::vector<PortDefinition> getInputPorts() const;
  [[nodiscard]] std::vector<PortDefinition> getOutputPorts() const;

  // Serialization
  [[nodiscard]] std::string toJson() const;
  static std::unique_ptr<IRNode> fromJson(const std::string &json);

protected:
  NodeId m_id;
  IRNodeType m_type;
  std::unordered_map<std::string, IRPropertyValue> m_properties;
  SourceLocation m_location;
  f32 m_x = 0.0f;
  f32 m_y = 0.0f;
};

/**
 * @brief IR Graph - complete representation of a script
 */
class IRGraph {
public:
  IRGraph();
  ~IRGraph();

  // Graph identity
  void setName(const std::string &name);
  [[nodiscard]] const std::string &getName() const { return m_name; }

  // Node management
  NodeId createNode(IRNodeType type);
  void removeNode(NodeId id);
  [[nodiscard]] IRNode *getNode(NodeId id);
  [[nodiscard]] const IRNode *getNode(NodeId id) const;
  [[nodiscard]] std::vector<IRNode *> getNodes();
  [[nodiscard]] std::vector<const IRNode *> getNodes() const;
  [[nodiscard]] std::vector<IRNode *> getNodesByType(IRNodeType type);

  // Connection management
  Result<void> connect(const PortId &source, const PortId &target);
  void disconnect(const PortId &source, const PortId &target);
  void disconnectAll(NodeId nodeId);
  [[nodiscard]] std::vector<IRConnection> getConnections() const;
  [[nodiscard]] std::vector<IRConnection>
  getConnectionsFrom(NodeId nodeId) const;
  [[nodiscard]] std::vector<IRConnection> getConnectionsTo(NodeId nodeId) const;
  [[nodiscard]] bool isConnected(const PortId &source,
                                 const PortId &target) const;

  // Traversal
  [[nodiscard]] std::vector<NodeId> getTopologicalOrder() const;
  [[nodiscard]] std::vector<NodeId> getExecutionOrder() const;

  // Validation
  [[nodiscard]] std::vector<std::string> validate() const;
  [[nodiscard]] bool isValid() const;

  // Scene management (for multi-scene scripts)
  void addScene(const std::string &sceneName, NodeId startNode);
  [[nodiscard]] NodeId getSceneStartNode(const std::string &sceneName) const;
  [[nodiscard]] std::vector<std::string> getSceneNames() const;

  // Character declarations
  void addCharacter(const std::string &id, const std::string &name,
                    const std::string &color);
  [[nodiscard]] bool hasCharacter(const std::string &id) const;

  // Serialization
  [[nodiscard]] std::string toJson() const;
  static std::unique_ptr<IRGraph> fromJson(const std::string &json);

private:
  NodeId m_nextId = 1;
  std::string m_name;
  std::unordered_map<NodeId, std::unique_ptr<IRNode>> m_nodes;
  std::vector<IRConnection> m_connections;
  std::unordered_map<std::string, NodeId> m_sceneStartNodes;
  std::unordered_map<std::string, std::pair<std::string, std::string>>
      m_characters; // id -> (name, color)
};

/**
 * @brief Converts AST to IR
 */
class ASTToIRConverter {
public:
  ASTToIRConverter();
  ~ASTToIRConverter();

  /**
   * @brief Convert parsed AST to IR graph
   */
  Result<std::unique_ptr<IRGraph>> convert(const Program &program);

private:
  void convertCharacterDecl(const CharacterDecl &decl);
  NodeId convertScene(const SceneDecl &scene);
  NodeId convertStatement(const Statement &stmt, NodeId prevNode);
  NodeId convertExpression(const Expression &expr);

  NodeId createNodeAndConnect(IRNodeType type, NodeId prevNode);

  std::unique_ptr<IRGraph> m_graph;
  f32 m_currentY = 0.0f;
  f32 m_nodeSpacing = 100.0f;
};

/**
 * @brief Converts IR to AST (for code generation)
 */
class IRToASTConverter {
public:
  IRToASTConverter();
  ~IRToASTConverter();

  /**
   * @brief Convert IR graph back to AST
   */
  Result<Program> convert(const IRGraph &graph);

private:
  std::unique_ptr<Statement> convertNode(const IRNode *node,
                                         const IRGraph &graph);
  std::unique_ptr<Expression> convertToExpression(const IRNode *node,
                                                  const IRGraph &graph);

  std::unordered_set<NodeId> m_visited;
};

/**
 * @brief Generates NM Script text from AST
 */
class ASTToTextGenerator {
public:
  ASTToTextGenerator();
  ~ASTToTextGenerator();

  /**
   * @brief Generate NM Script text from AST
   */
  std::string generate(const Program &program);

private:
  void generateCharacter(const CharacterDecl &decl);
  void generateScene(const SceneDecl &scene);
  void generateStatement(const Statement &stmt, int indent);
  void generateExpression(const Expression &expr);

  std::string m_output;
  int m_indentLevel = 0;

  void indent();
  void newline();
  void write(const std::string &text);
};

/**
 * @brief Visual Graph node for editor representation
 */
struct VisualGraphNode {
  NodeId id;
  std::string type;
  std::string displayName;
  f32 x;
  f32 y;
  f32 width = 200.0f;
  f32 height = 100.0f;
  std::vector<std::pair<std::string, std::string>>
      inputPorts; // name, displayName
  std::vector<std::pair<std::string, std::string>>
      outputPorts; // name, displayName
  std::unordered_map<std::string, std::string> properties;
  bool selected = false;
  bool collapsed = false;
};

/**
 * @brief Visual Graph edge for editor representation
 */
struct VisualGraphEdge {
  NodeId sourceNode;
  std::string sourcePort;
  NodeId targetNode;
  std::string targetPort;
  bool selected = false;
};

/**
 * @brief Visual Graph - editor-friendly representation
 */
class VisualGraph {
public:
  VisualGraph();
  ~VisualGraph();

  // Conversion from/to IR
  void fromIR(const IRGraph &ir);
  std::unique_ptr<IRGraph> toIR() const;

  // Node access
  [[nodiscard]] const std::vector<VisualGraphNode> &getNodes() const {
    return m_nodes;
  }
  [[nodiscard]] const std::vector<VisualGraphEdge> &getEdges() const {
    return m_edges;
  }

  [[nodiscard]] VisualGraphNode *findNode(NodeId id);
  [[nodiscard]] const VisualGraphNode *findNode(NodeId id) const;

  // Node manipulation
  NodeId addNode(const std::string &type, f32 x, f32 y);
  void removeNode(NodeId id);
  void setNodePosition(NodeId id, f32 x, f32 y);
  void setNodeProperty(NodeId id, const std::string &name,
                       const std::string &value);

  // Edge manipulation
  void addEdge(NodeId sourceNode, const std::string &sourcePort,
               NodeId targetNode, const std::string &targetPort);
  void removeEdge(NodeId sourceNode, const std::string &sourcePort,
                  NodeId targetNode, const std::string &targetPort);

  // Selection
  void selectNode(NodeId id, bool addToSelection = false);
  void deselectNode(NodeId id);
  void selectEdge(NodeId sourceNode, const std::string &sourcePort,
                  NodeId targetNode, const std::string &targetPort);
  void clearSelection();

  // Layout
  void autoLayout();

  // Serialization
  [[nodiscard]] std::string toJson() const;
  static std::unique_ptr<VisualGraph> fromJson(const std::string &json);

private:
  NodeId m_nextId = 1;
  std::vector<VisualGraphNode> m_nodes;
  std::vector<VisualGraphEdge> m_edges;
};

/**
 * @brief Round-trip converter between all representations
 *
 * Provides bidirectional conversion:
 * - NM Script text <-> AST <-> IR <-> Visual Graph
 */
class RoundTripConverter {
public:
  RoundTripConverter();
  ~RoundTripConverter();

  // Text -> IR
  Result<std::unique_ptr<IRGraph>> textToIR(const std::string &nmScript);

  // IR -> Text
  Result<std::string> irToText(const IRGraph &ir);

  // IR <-> Visual Graph
  Result<std::unique_ptr<VisualGraph>> irToVisualGraph(const IRGraph &ir);
  Result<std::unique_ptr<IRGraph>> visualGraphToIR(const VisualGraph &graph);

  // Direct conversions
  Result<std::unique_ptr<VisualGraph>>
  textToVisualGraph(const std::string &nmScript);
  Result<std::string> visualGraphToText(const VisualGraph &graph);

  // Validation
  [[nodiscard]] std::vector<std::string>
  validateConversion(const std::string &original,
                     const std::string &roundTripped);

private:
  // Components
  std::unique_ptr<Lexer> m_lexer;
  std::unique_ptr<Parser> m_parser;
  std::unique_ptr<ASTToIRConverter> m_astToIR;
  std::unique_ptr<IRToASTConverter> m_irToAST;
  std::unique_ptr<ASTToTextGenerator> m_textGen;
};

/**
 * @brief Graph difference types for diffing API
 */
enum class GraphDiffType : u8 {
  NodeAdded,
  NodeRemoved,
  NodeModified,
  EdgeAdded,
  EdgeRemoved,
  PropertyChanged,
  PositionChanged
};

/**
 * @brief Single difference entry in a graph diff
 */
struct GraphDiffEntry {
  GraphDiffType type;
  NodeId nodeId = 0;        // For node changes
  std::string propertyName; // For property changes
  std::string oldValue;     // Previous value
  std::string newValue;     // New value
  VisualGraphEdge edge;     // For edge changes
};

/**
 * @brief Result of diffing two graphs
 */
struct GraphDiff {
  std::vector<GraphDiffEntry> entries;
  bool hasStructuralChanges = false; // Node/edge additions or removals
  bool hasPropertyChanges = false;   // Only property values changed
  bool hasPositionChanges = false;   // Only visual positions changed

  [[nodiscard]] bool isEmpty() const { return entries.empty(); }
  [[nodiscard]] size_t size() const { return entries.size(); }
};

/**
 * @brief Graph Differ - computes differences between two visual graphs
 *
 * Used for:
 * - Undo/Redo operations (compute minimal diff)
 * - Collaborative editing (merge changes)
 * - Change tracking (history display)
 */
class GraphDiffer {
public:
  GraphDiffer() = default;
  ~GraphDiffer() = default;

  /**
   * @brief Compute diff between two graphs
   * @param oldGraph The original graph (before changes)
   * @param newGraph The modified graph (after changes)
   * @return GraphDiff containing all differences
   */
  [[nodiscard]] GraphDiff diff(const VisualGraph &oldGraph,
                               const VisualGraph &newGraph) const;

  /**
   * @brief Apply a diff to a graph
   * @param graph The graph to modify
   * @param diff The diff to apply
   * @return Result indicating success or failure
   */
  Result<void> applyDiff(VisualGraph &graph, const GraphDiff &diff) const;

  /**
   * @brief Invert a diff (for undo operations)
   * @param diff The diff to invert
   * @return Inverted diff
   */
  [[nodiscard]] GraphDiff invertDiff(const GraphDiff &diff) const;

  /**
   * @brief Merge two diffs (for collaborative editing)
   * @param diff1 First diff
   * @param diff2 Second diff
   * @return Merged diff or error if conflicts exist
   */
  [[nodiscard]] Result<GraphDiff> mergeDiffs(const GraphDiff &diff1,
                                             const GraphDiff &diff2) const;

  /**
   * @brief Check if two diffs conflict
   */
  [[nodiscard]] bool hasConflicts(const GraphDiff &diff1,
                                  const GraphDiff &diff2) const;

private:
  void diffNodes(const VisualGraph &oldGraph, const VisualGraph &newGraph,
                 GraphDiff &result) const;
  void diffEdges(const VisualGraph &oldGraph, const VisualGraph &newGraph,
                 GraphDiff &result) const;
  void diffNodeProperties(const VisualGraphNode &oldNode,
                          const VisualGraphNode &newNode,
                          GraphDiff &result) const;
};

/**
 * @brief ID Normalizer - ensures consistent node IDs across serialization
 *
 * When graphs are saved and loaded, or when merging graphs,
 * node IDs may become fragmented or inconsistent. The normalizer:
 * - Reassigns node IDs to be contiguous starting from 1
 * - Maintains topological order when possible
 * - Updates all edge references
 * - Provides mapping from old to new IDs
 */
class IDNormalizer {
public:
  IDNormalizer() = default;
  ~IDNormalizer() = default;

  /**
   * @brief Normalize node IDs in a visual graph
   * @param graph The graph to normalize (modified in place)
   * @return Mapping from old IDs to new IDs
   */
  std::unordered_map<NodeId, NodeId> normalize(VisualGraph &graph) const;

  /**
   * @brief Normalize node IDs in an IR graph
   * @param graph The IR graph to normalize (modified in place)
   * @return Mapping from old IDs to new IDs
   */
  std::unordered_map<NodeId, NodeId> normalize(IRGraph &graph) const;

  /**
   * @brief Check if a graph needs normalization
   * @param graph The graph to check
   * @return True if IDs are not contiguous or start from 1
   */
  [[nodiscard]] bool needsNormalization(const VisualGraph &graph) const;

  /**
   * @brief Create normalized copy without modifying original
   * @param graph The source graph
   * @return Normalized copy and ID mapping
   */
  [[nodiscard]] std::pair<std::unique_ptr<VisualGraph>,
                          std::unordered_map<NodeId, NodeId>>
  createNormalizedCopy(const VisualGraph &graph) const;

private:
  std::vector<NodeId> getTopologicalOrder(const VisualGraph &graph) const;
};

/**
 * @brief Round-trip guarantee validator
 *
 * Validates that conversions between representations are lossless:
 * - Text -> IR -> Text produces equivalent output
 * - IR -> VisualGraph -> IR produces equivalent output
 * - Full chain: Text -> IR -> VisualGraph -> IR -> Text
 */
class RoundTripValidator {
public:
  RoundTripValidator();
  ~RoundTripValidator();

  /**
   * @brief Validation result with detailed information
   */
  struct ValidationResult {
    bool isValid = false;
    std::vector<std::string> differences;
    std::string originalText;
    std::string roundTrippedText;
    std::unique_ptr<GraphDiff> graphDiff;
  };

  /**
   * @brief Validate text round-trip through IR
   */
  [[nodiscard]] ValidationResult
  validateTextRoundTrip(const std::string &nmScript);

  /**
   * @brief Validate IR round-trip through VisualGraph
   */
  [[nodiscard]] ValidationResult validateIRRoundTrip(const IRGraph &ir);

  /**
   * @brief Validate full chain: Text -> IR -> VisualGraph -> IR -> Text
   */
  [[nodiscard]] ValidationResult
  validateFullRoundTrip(const std::string &nmScript);

  /**
   * @brief Compare two IR graphs for semantic equivalence
   */
  [[nodiscard]] bool areSemanticalllyEquivalent(const IRGraph &a,
                                                const IRGraph &b) const;

private:
  std::unique_ptr<RoundTripConverter> m_converter;
  std::unique_ptr<GraphDiffer> m_differ;
  std::unique_ptr<IDNormalizer> m_normalizer;
};

/**
 * @brief Dialogue localization entry for export/import
 */
struct DialogueLocalizationEntry {
  std::string key;           // Localization key
  std::string sourceText;    // Original dialogue text (default locale)
  std::string speaker;       // Speaker/character name
  NodeId nodeId;             // Node ID for reference
  std::string sceneId;       // Parent scene ID
  TranslationStatus status;  // Translation status

  DialogueLocalizationEntry()
      : nodeId(0), status(TranslationStatus::Untranslated) {}
};

/**
 * @brief Helper class for dialogue localization operations
 *
 * Provides functionality to:
 * - Extract all localizable dialogue from an IR graph
 * - Generate localization keys for dialogue nodes
 * - Check translation status for dialogue nodes
 * - Collect dialogue entries for export to localization files
 */
class DialogueLocalizationHelper {
public:
  DialogueLocalizationHelper() = default;
  ~DialogueLocalizationHelper() = default;

  /**
   * @brief Collect all dialogue entries from a scene's embedded graph
   * @param graph The IR graph containing dialogue nodes
   * @param sceneId The scene identifier for key generation
   * @return Vector of dialogue localization entries
   */
  [[nodiscard]] std::vector<DialogueLocalizationEntry>
  collectDialogueEntries(const IRGraph& graph, const std::string& sceneId) const;

  /**
   * @brief Collect all choice option entries from a scene
   * @param graph The IR graph containing choice nodes
   * @param sceneId The scene identifier for key generation
   * @return Vector of dialogue localization entries for choice options
   */
  [[nodiscard]] std::vector<DialogueLocalizationEntry>
  collectChoiceEntries(const IRGraph& graph, const std::string& sceneId) const;

  /**
   * @brief Generate localization keys for all dialogue nodes in a graph
   * @param graph The IR graph to process (modified in place)
   * @param sceneId The scene identifier for key generation
   * @return Number of keys generated
   */
  size_t generateLocalizationKeys(IRGraph& graph, const std::string& sceneId);

  /**
   * @brief Check if a dialogue node has a localization key
   * @param node The dialogue node to check
   * @return True if the node has a localization key property
   */
  [[nodiscard]] bool hasLocalizationKey(const IRNode& node) const;

  /**
   * @brief Get the localization key for a dialogue node
   * @param node The dialogue node
   * @return The localization key, or empty string if not set
   */
  [[nodiscard]] std::string getLocalizationKey(const IRNode& node) const;

  /**
   * @brief Set the localization key for a dialogue node
   * @param node The dialogue node
   * @param key The localization key to set
   */
  void setLocalizationKey(IRNode& node, const std::string& key);

  /**
   * @brief Get all nodes that need localization (dialogues and choices)
   * @param graph The IR graph to scan
   * @return Vector of node IDs that require localization
   */
  [[nodiscard]] std::vector<NodeId>
  getLocalizableNodes(const IRGraph& graph) const;

  /**
   * @brief Find dialogue nodes missing localization keys
   * @param graph The IR graph to scan
   * @return Vector of node IDs without localization keys
   */
  [[nodiscard]] std::vector<NodeId>
  findMissingKeys(const IRGraph& graph) const;

  // Property name constants for dialogue localization
  static constexpr const char* PROP_LOCALIZATION_KEY = "localization_key";
  static constexpr const char* PROP_LOCALIZATION_KEY_CUSTOM = "localization_key_custom";
  static constexpr const char* PROP_USE_CUSTOM_KEY = "use_custom_localization_key";
  static constexpr const char* PROP_TRANSLATION_STATUS = "translation_status";
};

} // namespace NovelMind::scripting
