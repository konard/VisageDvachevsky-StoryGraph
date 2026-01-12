#pragma once

#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

namespace NovelMind::editor::qt::detail {

bool loadGraphLayout(QHash<QString, NMStoryGraphPanel::LayoutNode>& nodes, QString& entryScene);
void saveGraphLayout(const QHash<QString, NMStoryGraphPanel::LayoutNode>& nodes,
                     const QString& entryScene);
QString resolveScriptPath(const NMGraphNodeItem* node);
bool updateSceneGraphBlock(const QString& sceneId, const QString& scriptPath,
                           const QStringList& targets);
bool updateSceneSayStatement(const QString& sceneId, const QString& scriptPath,
                             const QString& speaker, const QString& text);
QStringList splitChoiceLines(const QString& raw);
NMStoryGraphPanel::LayoutNode buildLayoutFromNode(const NMGraphNodeItem* node);

/// Validates if a speaker name is a valid NMScript identifier.
/// Valid identifiers must start with a Unicode letter or underscore,
/// followed by letters, digits, or underscores.
/// @param speaker The speaker name to validate
/// @return true if the speaker is a valid identifier
bool isValidSpeakerIdentifier(const QString& speaker);

/// Sanitizes a speaker name to be a valid NMScript identifier.
/// Replaces invalid characters with underscores and ensures the name
/// starts with a valid character.
/// @param speaker The speaker name to sanitize
/// @return A sanitized version of the speaker name, or "Narrator" if empty
QString sanitizeSpeakerIdentifier(const QString& speaker);

// ============================================================================
// NMScript Generator (Graph -> Script conversion) - Issue #127
// ============================================================================

/// Generates complete NMScript file content from graph nodes.
/// @param nodes List of graph node items to convert
/// @param entryScene ID of the entry scene
/// @return Generated NMScript code as QString
QString generateNMScriptFromNodes(const QList<NMGraphNodeItem*>& nodes, const QString& entryScene);

/// Generates a single scene block from a graph node.
/// @param node The graph node to convert
/// @return NMScript scene block as QString
QString generateSceneBlock(const NMGraphNodeItem* node);

/// Writes generated script to the scripts/generated/ directory.
/// @param scriptContent The script content to write
/// @param filename The output filename (without path)
/// @return true if write succeeded
bool writeGeneratedScript(const QString& scriptContent, const QString& filename);

// ============================================================================
// NMScript Parser (Script -> Graph conversion) - Issue #127
// ============================================================================

/// Parsed node data from NMScript file
struct ParsedNode {
  QString id;                   ///< Scene/label identifier
  QString type;                 ///< Node type: Scene, Dialogue, Choice, Condition
  QString speaker;              ///< Speaker name (for dialogue)
  QString text;                 ///< Dialogue text
  QStringList choices;          ///< Choice options (for choice nodes)
  QStringList targets;          ///< Goto targets
  QString conditionExpr;        ///< Condition expression
  QStringList conditionOutputs; ///< Condition output branches
  int sourceLineNumber = 0;     ///< Line number in source file
};

/// Parse result from NMScript file
struct ParseResult {
  bool success = false;
  QString errorMessage;
  int errorLine = 0;
  QString entryPoint;
  QList<ParsedNode> nodes;
  QList<QPair<QString, QString>> edges; ///< from -> to connections
};

/// Parses an NMScript file and extracts node/scene information.
/// @param scriptPath Path to the .nms file
/// @return ParseResult with extracted nodes and edges
ParseResult parseNMScriptFile(const QString& scriptPath);

/// Parses NMScript content string and extracts node/scene information.
/// @param content The script content to parse
/// @return ParseResult with extracted nodes and edges
ParseResult parseNMScriptContent(const QString& content);

} // namespace NovelMind::editor::qt::detail
