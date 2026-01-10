#include "NovelMind/editor/editor_runtime_host.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/scene_document.hpp"
#include "NovelMind/scripting/ir.hpp"
#include "editor_runtime_host_detail.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <variant>

// JSON parsing - use nlohmann/json if available, otherwise simple parsing
#ifdef NOVELMIND_HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace NovelMind::editor {

namespace fs = std::filesystem;

namespace {

/**
 * @brief Simple JSON value type for story graph parsing
 */
struct JsonValue {
  std::string type; // "null", "bool", "number", "string", "array", "object"
  std::string stringValue;
  double numberValue = 0.0;
  bool boolValue = false;
  std::vector<JsonValue> arrayValue;
  std::unordered_map<std::string, JsonValue> objectValue;
};

/**
 * @brief Minimal JSON parser for story_graph.json
 */
class SimpleJsonParser {
public:
  static Result<JsonValue> parse(const std::string &json) {
    SimpleJsonParser parser(json);
    return parser.parseValue();
  }

private:
  explicit SimpleJsonParser(const std::string &json) : m_json(json), m_pos(0) {}

  void skipWhitespace() {
    while (m_pos < m_json.size() && std::isspace(m_json[m_pos])) {
      ++m_pos;
    }
  }

  Result<JsonValue> parseValue() {
    skipWhitespace();
    if (m_pos >= m_json.size()) {
      return Result<JsonValue>::error("Unexpected end of JSON");
    }

    char c = m_json[m_pos];
    if (c == '{') {
      return parseObject();
    } else if (c == '[') {
      return parseArray();
    } else if (c == '"') {
      return parseString();
    } else if (c == 't' || c == 'f') {
      return parseBool();
    } else if (c == 'n') {
      return parseNull();
    } else if (std::isdigit(c) || c == '-') {
      return parseNumber();
    }
    return Result<JsonValue>::error("Unexpected character in JSON");
  }

  Result<JsonValue> parseObject() {
    if (m_pos >= m_json.size() || m_json[m_pos] != '{') {
      return Result<JsonValue>::error("Expected '{'");
    }
    ++m_pos;

    JsonValue result;
    result.type = "object";

    skipWhitespace();
    if (m_pos < m_json.size() && m_json[m_pos] == '}') {
      ++m_pos;
      return Result<JsonValue>::ok(std::move(result));
    }

    while (true) {
      skipWhitespace();
      auto keyResult = parseString();
      if (!keyResult.isOk()) {
        return keyResult;
      }
      std::string key = keyResult.value().stringValue;

      skipWhitespace();
      if (m_pos >= m_json.size() || m_json[m_pos] != ':') {
        return Result<JsonValue>::error("Expected ':'");
      }
      ++m_pos;

      auto valueResult = parseValue();
      if (!valueResult.isOk()) {
        return valueResult;
      }
      result.objectValue[key] = std::move(valueResult.value());

      skipWhitespace();
      if (m_pos >= m_json.size()) {
        return Result<JsonValue>::error("Unexpected end of JSON");
      }
      if (m_json[m_pos] == '}') {
        ++m_pos;
        break;
      }
      if (m_json[m_pos] != ',') {
        return Result<JsonValue>::error("Expected ',' or '}'");
      }
      ++m_pos;
    }

    return Result<JsonValue>::ok(std::move(result));
  }

  Result<JsonValue> parseArray() {
    if (m_pos >= m_json.size() || m_json[m_pos] != '[') {
      return Result<JsonValue>::error("Expected '['");
    }
    ++m_pos;

    JsonValue result;
    result.type = "array";

    skipWhitespace();
    if (m_pos < m_json.size() && m_json[m_pos] == ']') {
      ++m_pos;
      return Result<JsonValue>::ok(std::move(result));
    }

    while (true) {
      auto valueResult = parseValue();
      if (!valueResult.isOk()) {
        return valueResult;
      }
      result.arrayValue.push_back(std::move(valueResult.value()));

      skipWhitespace();
      if (m_pos >= m_json.size()) {
        return Result<JsonValue>::error("Unexpected end of JSON");
      }
      if (m_json[m_pos] == ']') {
        ++m_pos;
        break;
      }
      if (m_json[m_pos] != ',') {
        return Result<JsonValue>::error("Expected ',' or ']'");
      }
      ++m_pos;
    }

    return Result<JsonValue>::ok(std::move(result));
  }

  Result<JsonValue> parseString() {
    if (m_pos >= m_json.size() || m_json[m_pos] != '"') {
      return Result<JsonValue>::error("Expected '\"'");
    }
    ++m_pos;

    std::string str;
    while (m_pos < m_json.size() && m_json[m_pos] != '"') {
      if (m_json[m_pos] == '\\' && m_pos + 1 < m_json.size()) {
        ++m_pos;
        char escaped = m_json[m_pos];
        switch (escaped) {
        case 'n':
          str += '\n';
          break;
        case 't':
          str += '\t';
          break;
        case 'r':
          str += '\r';
          break;
        case '"':
          str += '"';
          break;
        case '\\':
          str += '\\';
          break;
        default:
          str += escaped;
          break;
        }
      } else {
        str += m_json[m_pos];
      }
      ++m_pos;
    }

    if (m_pos >= m_json.size()) {
      return Result<JsonValue>::error("Unterminated string");
    }
    ++m_pos; // Skip closing quote

    JsonValue result;
    result.type = "string";
    result.stringValue = str;
    return Result<JsonValue>::ok(std::move(result));
  }

  Result<JsonValue> parseNumber() {
    size_t start = m_pos;
    if (m_json[m_pos] == '-') {
      ++m_pos;
    }
    while (m_pos < m_json.size() &&
           (std::isdigit(m_json[m_pos]) || m_json[m_pos] == '.')) {
      ++m_pos;
    }

    JsonValue result;
    result.type = "number";
    result.numberValue = std::stod(m_json.substr(start, m_pos - start));
    return Result<JsonValue>::ok(std::move(result));
  }

  Result<JsonValue> parseBool() {
    JsonValue result;
    result.type = "bool";

    if (m_json.substr(m_pos, 4) == "true") {
      result.boolValue = true;
      m_pos += 4;
    } else if (m_json.substr(m_pos, 5) == "false") {
      result.boolValue = false;
      m_pos += 5;
    } else {
      return Result<JsonValue>::error("Expected boolean");
    }

    return Result<JsonValue>::ok(std::move(result));
  }

  Result<JsonValue> parseNull() {
    if (m_json.substr(m_pos, 4) == "null") {
      m_pos += 4;
      JsonValue result;
      result.type = "null";
      return Result<JsonValue>::ok(std::move(result));
    }
    return Result<JsonValue>::error("Expected null");
  }

  const std::string &m_json;
  size_t m_pos;
};

/**
 * @brief Helper to get string value from JsonValue object
 */
std::string jsonGetString(const JsonValue &obj, const std::string &key) {
  auto it = obj.objectValue.find(key);
  if (it != obj.objectValue.end() && it->second.type == "string") {
    return it->second.stringValue;
  }
  return "";
}

/**
 * @brief Escape special characters in dialogue text for NMScript
 */
std::string escapeDialogueText(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 10);
  for (char c : text) {
    switch (c) {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += c;
      break;
    }
  }
  return escaped;
}

/**
 * @brief Check if a node has a specific type (case-insensitive)
 */
bool nodeTypeEquals(const std::string &nodeType, const std::string &target) {
  if (nodeType.size() != target.size())
    return false;
  for (size_t i = 0; i < nodeType.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(nodeType[i])) !=
        std::tolower(static_cast<unsigned char>(target[i]))) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Convert story graph JSON to NMScript text
 *
 * This generates script content from the story graph visual representation.
 * Used when PlaybackSourceMode::Graph or Mixed is selected.
 *
 * Supports full parity with the Story Graph node palette:
 * - Scene nodes -> scene blocks with dialogue
 * - Dialogue nodes -> say statements
 * - Choice nodes -> choice blocks with branching
 * - Condition nodes -> if/else blocks
 * - Jump nodes -> goto statements
 * - Variable nodes -> set statements
 * - Random nodes -> randomized branching
 * - End nodes -> scene end markers
 * - Label nodes -> label declarations
 * - Event nodes -> event triggers
 * - Script nodes -> inline script blocks
 */
std::string generateScriptFromGraphJson(const JsonValue &graphJson) {
  std::ostringstream script;

  // Get entry scene
  std::string entryScene = jsonGetString(graphJson, "entry");

  // Process nodes array
  auto nodesIt = graphJson.objectValue.find("nodes");
  if (nodesIt == graphJson.objectValue.end() ||
      nodesIt->second.type != "array") {
    return "";
  }

  // Categorize nodes by type for proper ordering
  std::vector<const JsonValue *> sceneNodes;
  std::vector<const JsonValue *> dialogueNodes;
  std::vector<const JsonValue *> choiceNodes;
  std::vector<const JsonValue *> conditionNodes;
  std::vector<const JsonValue *> jumpNodes;
  std::vector<const JsonValue *> variableNodes;
  std::vector<const JsonValue *> randomNodes;
  std::vector<const JsonValue *> endNodes;
  std::vector<const JsonValue *> labelNodes;
  std::vector<const JsonValue *> eventNodes;
  std::vector<const JsonValue *> scriptNodes;

  for (const auto &node : nodesIt->second.arrayValue) {
    if (node.type != "object") {
      continue;
    }
    std::string nodeType = jsonGetString(node, "type");

    if (nodeTypeEquals(nodeType, "Scene")) {
      sceneNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Dialogue")) {
      dialogueNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Choice")) {
      choiceNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Condition")) {
      conditionNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Jump")) {
      jumpNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Variable")) {
      variableNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Random")) {
      randomNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "End")) {
      endNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Label")) {
      labelNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Event")) {
      eventNodes.push_back(&node);
    } else if (nodeTypeEquals(nodeType, "Script")) {
      scriptNodes.push_back(&node);
    }
  }

  // Add file header comment
  script << "// ========================================\n";
  script << "// Generated from Story Graph (Graph Mode)\n";
  script << "// Do not edit manually - changes may be overwritten\n";
  script << "// ========================================\n\n";

  // Generate script from scene nodes (primary containers)
  for (const auto *nodePtr : sceneNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string title = jsonGetString(node, "title");
    std::string dialogueText = jsonGetString(node, "dialogueText");
    if (dialogueText.empty()) {
      dialogueText = jsonGetString(node, "text");
    }
    std::string speaker = jsonGetString(node, "speaker");
    std::string conditionExpression = jsonGetString(node, "conditionExpression");

    // Use ID as scene name
    script << "scene " << nodeId << " {\n";

    // Add dialogue if present
    if (!dialogueText.empty() && dialogueText != "New scene") {
      std::string escapedText = escapeDialogueText(dialogueText);

      if (!speaker.empty()) {
        script << "    say " << speaker << " \"" << escapedText << "\"\n";
      } else {
        script << "    say \"" << escapedText << "\"\n";
      }
    }

    // Check for condition expression (for Condition-type nodes stored as Scene)
    if (!conditionExpression.empty()) {
      // Generate if/else block for condition
      auto conditionTargetsIt = node.objectValue.find("conditionTargets");
      if (conditionTargetsIt != node.objectValue.end() &&
          conditionTargetsIt->second.type == "object" &&
          !conditionTargetsIt->second.objectValue.empty()) {
        script << "    if " << conditionExpression << " {\n";
        // Find true branch
        auto trueIt = conditionTargetsIt->second.objectValue.find("true");
        if (trueIt != conditionTargetsIt->second.objectValue.end() &&
            trueIt->second.type == "string") {
          script << "        goto " << trueIt->second.stringValue << "\n";
        }
        script << "    }";
        // Find false branch
        auto falseIt = conditionTargetsIt->second.objectValue.find("false");
        if (falseIt != conditionTargetsIt->second.objectValue.end() &&
            falseIt->second.type == "string") {
          script << " else {\n";
          script << "        goto " << falseIt->second.stringValue << "\n";
          script << "    }";
        }
        script << "\n";
      }
    }

    // Check for choice targets (branching)
    auto choiceTargetsIt = node.objectValue.find("choiceTargets");
    if (choiceTargetsIt != node.objectValue.end() &&
        choiceTargetsIt->second.type == "object" &&
        !choiceTargetsIt->second.objectValue.empty()) {
      script << "    choice {\n";
      for (const auto &[optionText, targetValue] :
           choiceTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          std::string escapedOption = escapeDialogueText(optionText);
          script << "        \"" << escapedOption << "\" -> goto "
                 << targetValue.stringValue << "\n";
        }
      }
      script << "    }\n";
    }

    script << "}\n\n";
  }

  // Generate standalone Dialogue nodes (not embedded in scenes)
  // These are wrapped in auto-generated scenes
  for (const auto *nodePtr : dialogueNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string dialogueText = jsonGetString(node, "dialogueText");
    if (dialogueText.empty()) {
      dialogueText = jsonGetString(node, "text");
    }
    std::string speaker = jsonGetString(node, "speaker");

    if (!dialogueText.empty() && dialogueText != "New dialogue") {
      // Wrap dialogue in a scene block
      script << "scene " << nodeId << " {\n";
      std::string escapedText = escapeDialogueText(dialogueText);
      if (!speaker.empty()) {
        script << "    say " << speaker << " \"" << escapedText << "\"\n";
      } else {
        script << "    say Narrator \"" << escapedText << "\"\n";
      }

      // Check for outgoing connections (stored as choiceTargets for single target)
      auto choiceTargetsIt = node.objectValue.find("choiceTargets");
      if (choiceTargetsIt != node.objectValue.end() &&
          choiceTargetsIt->second.type == "object" &&
          !choiceTargetsIt->second.objectValue.empty()) {
        // If single target, use goto
        if (choiceTargetsIt->second.objectValue.size() == 1) {
          for (const auto &[key, targetValue] :
               choiceTargetsIt->second.objectValue) {
            if (targetValue.type == "string") {
              script << "    goto " << targetValue.stringValue << "\n";
            }
          }
        }
      }
      script << "}\n\n";
    }
  }

  // Generate Choice nodes
  for (const auto *nodePtr : choiceNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string dialogueText = jsonGetString(node, "dialogueText");
    if (dialogueText.empty()) {
      dialogueText = jsonGetString(node, "text");
    }
    std::string speaker = jsonGetString(node, "speaker");

    script << "scene " << nodeId << " {\n";

    // Add any dialogue before the choice
    if (!dialogueText.empty() && dialogueText != "New choice") {
      std::string escapedText = escapeDialogueText(dialogueText);
      if (!speaker.empty()) {
        script << "    say " << speaker << " \"" << escapedText << "\"\n";
      } else {
        script << "    say Narrator \"" << escapedText << "\"\n";
      }
    }

    // Generate choice block
    auto choiceTargetsIt = node.objectValue.find("choiceTargets");
    if (choiceTargetsIt != node.objectValue.end() &&
        choiceTargetsIt->second.type == "object" &&
        !choiceTargetsIt->second.objectValue.empty()) {
      script << "    choice {\n";
      for (const auto &[optionText, targetValue] :
           choiceTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          std::string escapedOption = escapeDialogueText(optionText);
          script << "        \"" << escapedOption << "\" -> goto "
                 << targetValue.stringValue << "\n";
        }
      }
      script << "    }\n";
    }
    script << "}\n\n";
  }

  // Generate Condition nodes
  for (const auto *nodePtr : conditionNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string conditionExpression = jsonGetString(node, "conditionExpression");

    script << "scene " << nodeId << " {\n";

    if (!conditionExpression.empty()) {
      auto conditionTargetsIt = node.objectValue.find("conditionTargets");
      if (conditionTargetsIt != node.objectValue.end() &&
          conditionTargetsIt->second.type == "object" &&
          !conditionTargetsIt->second.objectValue.empty()) {

        script << "    if " << conditionExpression << " {\n";

        // Find true branch
        auto trueIt = conditionTargetsIt->second.objectValue.find("true");
        if (trueIt != conditionTargetsIt->second.objectValue.end() &&
            trueIt->second.type == "string") {
          script << "        goto " << trueIt->second.stringValue << "\n";
        }
        script << "    }";

        // Find false branch
        auto falseIt = conditionTargetsIt->second.objectValue.find("false");
        if (falseIt != conditionTargetsIt->second.objectValue.end() &&
            falseIt->second.type == "string") {
          script << " else {\n";
          script << "        goto " << falseIt->second.stringValue << "\n";
          script << "    }";
        }
        script << "\n";
      }
    } else {
      script << "    // Condition node - add condition expression\n";
    }
    script << "}\n\n";
  }

  // Generate Variable nodes
  for (const auto *nodePtr : variableNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string variableName = jsonGetString(node, "variableName");
    std::string variableValue = jsonGetString(node, "variableValue");

    script << "scene " << nodeId << " {\n";
    if (!variableName.empty()) {
      if (variableValue.empty()) {
        variableValue = "0"; // Default to 0 if no value
      }
      script << "    set " << variableName << " = " << variableValue << "\n";
    }

    // Check for next node (goto)
    auto choiceTargetsIt = node.objectValue.find("choiceTargets");
    if (choiceTargetsIt != node.objectValue.end() &&
        choiceTargetsIt->second.type == "object" &&
        !choiceTargetsIt->second.objectValue.empty()) {
      for (const auto &[key, targetValue] :
           choiceTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          script << "    goto " << targetValue.stringValue << "\n";
          break; // Only first connection
        }
      }
    }
    script << "}\n\n";
  }

  // Generate Random nodes
  for (const auto *nodePtr : randomNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");

    script << "scene " << nodeId << " {\n";
    script << "    // Random branching\n";

    auto conditionTargetsIt = node.objectValue.find("conditionTargets");
    if (conditionTargetsIt != node.objectValue.end() &&
        conditionTargetsIt->second.type == "object" &&
        !conditionTargetsIt->second.objectValue.empty()) {
      // Generate random logic using choice with equal probability markers
      script << "    choice {\n";
      for (const auto &[label, targetValue] :
           conditionTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          script << "        \"" << label << "\" -> goto "
                 << targetValue.stringValue << "\n";
        }
      }
      script << "    }\n";
    }
    script << "}\n\n";
  }

  // Generate Jump nodes (simple goto)
  for (const auto *nodePtr : jumpNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string jumpTarget = jsonGetString(node, "jumpTarget");

    script << "scene " << nodeId << " {\n";
    if (!jumpTarget.empty()) {
      script << "    goto " << jumpTarget << "\n";
    } else {
      // Check choiceTargets for connection target
      auto choiceTargetsIt = node.objectValue.find("choiceTargets");
      if (choiceTargetsIt != node.objectValue.end() &&
          choiceTargetsIt->second.type == "object" &&
          !choiceTargetsIt->second.objectValue.empty()) {
        for (const auto &[key, targetValue] :
             choiceTargetsIt->second.objectValue) {
          if (targetValue.type == "string") {
            script << "    goto " << targetValue.stringValue << "\n";
            break;
          }
        }
      }
    }
    script << "}\n\n";
  }

  // Generate Event nodes
  for (const auto *nodePtr : eventNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string eventName = jsonGetString(node, "eventName");

    script << "scene " << nodeId << " {\n";
    if (!eventName.empty()) {
      script << "    // Event trigger: " << eventName << "\n";
      // Note: Event triggering is not standard NMScript, but we document it
    }

    // Check for next node
    auto choiceTargetsIt = node.objectValue.find("choiceTargets");
    if (choiceTargetsIt != node.objectValue.end() &&
        choiceTargetsIt->second.type == "object" &&
        !choiceTargetsIt->second.objectValue.empty()) {
      for (const auto &[key, targetValue] :
           choiceTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          script << "    goto " << targetValue.stringValue << "\n";
          break;
        }
      }
    }
    script << "}\n\n";
  }

  // Generate Script nodes (inline script)
  for (const auto *nodePtr : scriptNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string scriptContent = jsonGetString(node, "scriptContent");

    script << "scene " << nodeId << " {\n";
    if (!scriptContent.empty()) {
      // Include script content as-is (trusted from graph)
      script << "    " << scriptContent << "\n";
    }

    // Check for next node
    auto choiceTargetsIt = node.objectValue.find("choiceTargets");
    if (choiceTargetsIt != node.objectValue.end() &&
        choiceTargetsIt->second.type == "object" &&
        !choiceTargetsIt->second.objectValue.empty()) {
      for (const auto &[key, targetValue] :
           choiceTargetsIt->second.objectValue) {
        if (targetValue.type == "string") {
          script << "    goto " << targetValue.stringValue << "\n";
          break;
        }
      }
    }
    script << "}\n\n";
  }

  // Generate End nodes
  for (const auto *nodePtr : endNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");

    script << "scene " << nodeId << " {\n";
    script << "    // End of story path\n";
    script << "}\n\n";
  }

  // Generate Label nodes (they act as targets but don't generate content)
  // Labels are implicit scene names in NMScript, so they're already handled
  // by the scene generation above if they have content

  return script.str();
}

/**
 * @brief Load story graph from project's .novelmind/story_graph.json
 */
Result<std::string> loadStoryGraphScript(const std::string &projectPath) {
  fs::path graphPath = fs::path(projectPath) / ".novelmind" / "story_graph.json";
  qDebug() << "[loadStoryGraphScript] Looking for graph at:" << QString::fromStdString(graphPath.string());

  if (!fs::exists(graphPath)) {
    qWarning() << "[loadStoryGraphScript] Story graph file NOT FOUND!";
    qWarning() << "[loadStoryGraphScript] Expected location:" << QString::fromStdString(graphPath.string());
    qWarning() << "[loadStoryGraphScript] This file should be created when you modify nodes in Story Graph panel";
    return Result<std::string>::error("Story graph file not found: " +
                                      graphPath.string());
  }

  qDebug() << "[loadStoryGraphScript] File exists, opening...";
  std::ifstream file(graphPath);
  if (!file) {
    qCritical() << "[loadStoryGraphScript] Failed to open file (permissions?)";
    return Result<std::string>::error("Failed to open story graph file: " +
                                      graphPath.string());
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string jsonContent = buffer.str();
  file.close();

  qDebug() << "[loadStoryGraphScript] File loaded, size:" << jsonContent.size() << "bytes";

  // Parse JSON
  qDebug() << "[loadStoryGraphScript] Parsing JSON...";
  auto parseResult = SimpleJsonParser::parse(jsonContent);
  if (!parseResult.isOk()) {
    qCritical() << "[loadStoryGraphScript] JSON parse error:" << QString::fromStdString(parseResult.error());
    return Result<std::string>::error("Failed to parse story graph JSON: " +
                                      parseResult.error());
  }

  qDebug() << "[loadStoryGraphScript] JSON parsed successfully, converting to script...";
  // Convert to script
  std::string script = generateScriptFromGraphJson(parseResult.value());
  if (script.empty()) {
    qCritical() << "[loadStoryGraphScript] No scene nodes found in story graph!";
    qCritical() << "[loadStoryGraphScript] The JSON file exists but has no valid scene nodes";
    return Result<std::string>::error("No scene nodes found in story graph");
  }

  qDebug() << "[loadStoryGraphScript] Script generated successfully, length:" << script.size() << "characters";
  return Result<std::string>::ok(std::move(script));
}

/**
 * @brief Get the current playback source mode from project settings
 */
PlaybackSourceMode getPlaybackSourceMode() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return PlaybackSourceMode::Script; // Default to script mode
  }
  return pm.getMetadata().playbackSourceMode;
}

/**
 * @brief Collect entry scene from story graph if available
 */
std::string getGraphEntryScene(const std::string &projectPath) {
  fs::path graphPath = fs::path(projectPath) / ".novelmind" / "story_graph.json";

  if (!fs::exists(graphPath)) {
    return "";
  }

  std::ifstream file(graphPath);
  if (!file) {
    return "";
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string jsonContent = buffer.str();
  file.close();

  auto parseResult = SimpleJsonParser::parse(jsonContent);
  if (!parseResult.isOk()) {
    return "";
  }

  return jsonGetString(parseResult.value(), "entry");
}

} // anonymous namespace

// ============================================================================
// Private Helpers
// ============================================================================

Result<void> EditorRuntimeHost::compileProject() {
  try {
    // Determine the playback source mode from project settings
    PlaybackSourceMode sourceMode = getPlaybackSourceMode();
    qDebug() << "[EditorRuntimeHost] === COMPILING PROJECT ===";
    qDebug() << "[EditorRuntimeHost] Playback source mode:" << static_cast<int>(sourceMode)
             << "(0=Script, 1=Graph, 2=Mixed)";
    qDebug() << "[EditorRuntimeHost] Project path:" << QString::fromStdString(m_project.path);

    std::string allScripts;
    m_sceneNames.clear();

    // Handle different playback source modes (Issue #94)
    switch (sourceMode) {
    case PlaybackSourceMode::Graph: {
      qDebug() << "[EditorRuntimeHost] Graph mode: Loading story graph...";
      // Load story graph and convert to script
      auto graphScriptResult = loadStoryGraphScript(m_project.path);
      if (!graphScriptResult.isOk()) {
        qCritical() << "[EditorRuntimeHost] GRAPH LOAD FAILED:" << QString::fromStdString(graphScriptResult.error());
        qCritical() << "[EditorRuntimeHost] Expected file:" << QString::fromStdString(m_project.path) << "/.novelmind/story_graph.json";
        // Fall back to script mode if graph is not available
        return Result<void>::error(
            "Graph mode selected but story graph not available: " +
            graphScriptResult.error() +
            ". Switch to Script mode or create a Story Graph.");
      }
      qDebug() << "[EditorRuntimeHost] Story graph loaded successfully, converting to script...";
      allScripts = "// Generated from Story Graph (Graph Mode)\n";
      allScripts += graphScriptResult.value();
      qDebug() << "[EditorRuntimeHost] Generated script length:" << allScripts.size() << "characters";

      // Use graph entry scene if available
      std::string graphEntry = getGraphEntryScene(m_project.path);
      if (!graphEntry.empty() && m_project.startScene.empty()) {
        m_project.startScene = graphEntry;
      }
      break;
    }

    case PlaybackSourceMode::Mixed: {
      // First load all script files (base content)
      fs::path scriptsPath(m_project.scriptsPath);
      if (fs::exists(scriptsPath)) {
        for (const auto &entry : fs::recursive_directory_iterator(scriptsPath)) {
          if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".nms" || ext == ".nm") {
              std::ifstream file(entry.path());
              if (file) {
                std::string content;
                if (detail::readFileToString(file, content)) {
                  allScripts += "\n// File: " + entry.path().string() + "\n";
                  allScripts += content;

                  u64 modTime = static_cast<u64>(
                      std::chrono::duration_cast<std::chrono::seconds>(
                          entry.last_write_time().time_since_epoch())
                          .count());
                  m_fileTimestamps[entry.path().string()] = modTime;
                }
              }
            }
          }
        }
      }

      // Then append graph-generated content (graph overrides scripts on conflict)
      auto graphScriptResult = loadStoryGraphScript(m_project.path);
      if (graphScriptResult.isOk()) {
        allScripts += "\n// ========================================\n";
        allScripts += "// Story Graph Overrides (Mixed Mode)\n";
        allScripts += "// ========================================\n";
        allScripts += graphScriptResult.value();

        // In Mixed mode, graph entry scene takes priority
        std::string graphEntry = getGraphEntryScene(m_project.path);
        if (!graphEntry.empty()) {
          m_project.startScene = graphEntry;
        }
      }
      // If graph is not available in Mixed mode, just use scripts (no error)
      break;
    }

    case PlaybackSourceMode::Script:
    default: {
      // Default behavior: load script files only
      fs::path scriptsPath(m_project.scriptsPath);

      if (!fs::exists(scriptsPath)) {
        return Result<void>::error("Scripts path does not exist: " +
                                   m_project.scriptsPath);
      }

      for (const auto &entry : fs::recursive_directory_iterator(scriptsPath)) {
        if (entry.is_regular_file()) {
          std::string ext = entry.path().extension().string();
          if (ext == ".nms" || ext == ".nm") {
            std::ifstream file(entry.path());
            if (file) {
              std::string content;
              if (!detail::readFileToString(file, content)) {
                return Result<void>::error("Failed to read script file: " +
                                           entry.path().string());
              }
              allScripts += "\n// File: " + entry.path().string() + "\n";
              allScripts += content;

              // Track file timestamps for hot reload
              u64 modTime = static_cast<u64>(
                  std::chrono::duration_cast<std::chrono::seconds>(
                      entry.last_write_time().time_since_epoch())
                      .count());
              m_fileTimestamps[entry.path().string()] = modTime;
            }
          }
        }
      }
      break;
    }
    }

    if (allScripts.empty()) {
      qCritical() << "[EditorRuntimeHost] No content found for playback!";
      qCritical() << "[EditorRuntimeHost] Check your scripts or Story Graph";
      return Result<void>::error("No content found for playback. "
                                 "Check your scripts or Story Graph.");
    }

    qDebug() << "[EditorRuntimeHost] Total script content:" << allScripts.size() << "characters";
    qDebug() << "[EditorRuntimeHost] Starting compilation pipeline...";

    // Allow UI to process events before starting compilation
    QCoreApplication::processEvents();

    // Lexer
    qDebug() << "[EditorRuntimeHost] Step 1/4: Lexer tokenization...";
    scripting::Lexer lexer;
    auto tokensResult = lexer.tokenize(allScripts);

    if (!tokensResult.isOk()) {
      qCritical() << "[EditorRuntimeHost] Lexer error:" << QString::fromStdString(tokensResult.error());
      return Result<void>::error("Lexer error: " + tokensResult.error());
    }
    qDebug() << "[EditorRuntimeHost] Lexer: Generated" << tokensResult.value().size() << "tokens";

    // Allow UI to update after lexer
    QCoreApplication::processEvents();

    // Parser
    qDebug() << "[EditorRuntimeHost] Step 2/4: Parser...";
    scripting::Parser parser;
    auto parseResult = parser.parse(tokensResult.value());

    if (!parseResult.isOk()) {
      qCritical() << "[EditorRuntimeHost] Parse error:" << QString::fromStdString(parseResult.error());
      return Result<void>::error("Parse error: " + parseResult.error());
    }

    m_program =
        std::make_unique<scripting::Program>(std::move(parseResult.value()));

    // Extract scene names
    for (const auto &scene : m_program->scenes) {
      m_sceneNames.push_back(scene.name);
    }
    qDebug() << "[EditorRuntimeHost] Parser: Found" << m_sceneNames.size() << "scenes";

    // Allow UI to update after parser
    QCoreApplication::processEvents();

    // Validator
    qDebug() << "[EditorRuntimeHost] Step 3/4: Validator...";
    scripting::Validator validator;
    auto validationResult = validator.validate(*m_program);

    if (validationResult.hasErrors()) {
      std::string errorMsg = "Validation errors:\n";
      for (const auto &err : validationResult.errors.errors()) {
        errorMsg += "  " + err.format() + "\n";
        qCritical() << "[EditorRuntimeHost] Validation error:" << QString::fromStdString(err.format());
      }
      return Result<void>::error(errorMsg);
    }
    qDebug() << "[EditorRuntimeHost] Validator: No errors";

    // Allow UI to update after validator
    QCoreApplication::processEvents();

    // Compiler
    qDebug() << "[EditorRuntimeHost] Step 4/4: Compiler...";
    scripting::Compiler compiler;
    auto compileResult = compiler.compile(*m_program);

    if (!compileResult.isOk()) {
      qCritical() << "[EditorRuntimeHost] Compilation error:" << QString::fromStdString(compileResult.error());
      return Result<void>::error("Compilation error: " + compileResult.error());
    }

    m_compiledScript = std::make_unique<scripting::CompiledScript>(
        std::move(compileResult.value()));

    qDebug() << "[EditorRuntimeHost] === COMPILATION SUCCESSFUL ===";
    qDebug() << "[EditorRuntimeHost] Scenes available:" << m_sceneNames.size();
    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Exception during compilation: ") +
                               e.what());
  }
}

Result<void> EditorRuntimeHost::initializeRuntime() {
  // Create scene graph
  m_sceneGraph = std::make_unique<scene::SceneGraph>();

  // Create resource manager for project assets
  m_resourceManager = std::make_unique<resource::ResourceManager>();
  if (!m_project.assetsPath.empty()) {
    m_resourceManager->setBasePath(m_project.assetsPath);
  } else {
    m_resourceManager->setBasePath(m_project.path);
  }
  m_sceneGraph->setResourceManager(m_resourceManager.get());

  // Create animation manager
  m_animationManager = std::make_unique<scene::AnimationManager>();

  // Create audio manager (dev mode - unencrypted)
  m_audioManager = std::make_unique<audio::AudioManager>();
  m_audioManager->setDataProvider([this](const std::string &id) {
    if (!m_resourceManager) {
      return Result<std::vector<u8>>::error("Resource manager unavailable");
    }
    return m_resourceManager->readData(id);
  });
  m_audioManager->initialize();

  // Create save manager
  m_saveManager = std::make_unique<save::SaveManager>();
  fs::path savePath = fs::path(m_project.path) / "Saves";
  std::error_code ec;
  fs::create_directories(savePath, ec);
  m_saveManager->setSavePath(savePath.string());

  // Create script runtime
  m_scriptRuntime = std::make_unique<scripting::ScriptRuntime>();

  // Connect runtime to scene components
  // Note: In a full implementation, we would also connect:
  // - SceneManager
  // - DialogueBox
  // - ChoiceMenu
  // - AudioManager

  // Set up event callback
  m_scriptRuntime->setEventCallback(
      [this](const scripting::ScriptEvent &event) { onRuntimeEvent(event); });

  return Result<void>::ok();
}

void EditorRuntimeHost::resetRuntime() {
  if (m_sceneGraph) {
    m_sceneGraph->clear();
  }

  if (m_animationManager) {
    m_animationManager->stopAll();
  }

  m_singleStepping = false;
  m_targetInstructionPointer = 0;
}

bool EditorRuntimeHost::checkBreakpoint(
    const scripting::SourceLocation &location) {
  for (const auto &bp : m_breakpoints) {
    if (bp.enabled && bp.line == location.line) {
      // Check file match (simplified)
      if (bp.condition.empty()) {
        fireBreakpointHit(bp);
        return true;
      }
      // Would evaluate conditional breakpoint here
    }
  }
  return false;
}

void EditorRuntimeHost::fireStateChanged(EditorRuntimeState newState) {
  if (m_onStateChanged) {
    m_onStateChanged(newState);
  }
}

void EditorRuntimeHost::fireBreakpointHit(const Breakpoint &bp) {
  if (m_onBreakpointHit) {
    m_state = EditorRuntimeState::Paused;
    auto stack = getScriptCallStack();
    m_onBreakpointHit(bp, stack);
  }
}

void EditorRuntimeHost::onRuntimeEvent(const scripting::ScriptEvent &event) {
  switch (event.type) {
  case scripting::ScriptEventType::SceneChange:
    // Update scene graph with new scene ID
    if (m_sceneGraph) {
      m_sceneGraph->setSceneId(event.name);
    }
    applySceneDocument(event.name);
    if (m_onSceneChanged) {
      m_onSceneChanged(event.name);
    }
    break;

  case scripting::ScriptEventType::BackgroundChanged:
    if (m_sceneGraph) {
      m_sceneGraph->showBackground(event.name);
    }
    break;

  case scripting::ScriptEventType::CharacterShow: {
    if (m_sceneGraph) {
      // event.value may hold desired slot (int). Default to Center.
      scene::CharacterObject::Position pos =
          scene::CharacterObject::Position::Center;
      if (std::holds_alternative<i32>(event.value)) {
        i32 posCode = std::get<i32>(event.value);
        switch (posCode) {
        case 0:
          pos = scene::CharacterObject::Position::Left;
          break;
        case 1:
          pos = scene::CharacterObject::Position::Center;
          break;
        case 2:
          pos = scene::CharacterObject::Position::Right;
          break;
        default:
          pos = scene::CharacterObject::Position::Custom;
          break;
        }
      }
      m_sceneGraph->showCharacter(event.name, event.name, pos);
    }
    break;
  }

  case scripting::ScriptEventType::CharacterHide:
    if (m_sceneGraph) {
      m_sceneGraph->hideCharacter(event.name);
    }
    break;

  case scripting::ScriptEventType::DialogueStart: {
    const std::string speaker = event.name;
    const std::string text = scripting::asString(event.value);
    if (m_sceneGraph) {
      m_sceneGraph->showDialogue(speaker, text);
    }
    if (m_onDialogueChanged) {
      m_onDialogueChanged(speaker, text);
    }
    break;
  }

  case scripting::ScriptEventType::DialogueComplete:
    if (m_sceneGraph) {
      // Keep dialogue box visible but mark typewriter complete
      m_sceneGraph->hideDialogue();
    }
    if (m_onDialogueChanged) {
      m_onDialogueChanged("", "");
    }
    break;

  case scripting::ScriptEventType::ChoiceStart: {
    if (m_scriptRuntime && m_sceneGraph) {
      const auto &choices = m_scriptRuntime->getCurrentChoices();
      std::vector<scene::ChoiceUIObject::ChoiceOption> options;
      options.reserve(choices.size());
      for (size_t i = 0; i < choices.size(); ++i) {
        scene::ChoiceUIObject::ChoiceOption opt;
        opt.id = "choice_" + std::to_string(i);
        opt.text = choices[i];
        options.push_back(opt);
      }
      m_sceneGraph->showChoices(options);
      if (m_onChoicesChanged) {
        m_onChoicesChanged(choices);
      }
    }
    break;
  }

  case scripting::ScriptEventType::ChoiceSelected:
    if (m_sceneGraph) {
      m_sceneGraph->hideChoices();
    }
    if (m_onChoicesChanged) {
      m_onChoicesChanged({});
    }
    break;

  case scripting::ScriptEventType::VariableChanged:
    if (m_onVariableChanged) {
      m_onVariableChanged(event.name, event.value);
    }
    break;

  default:
    break;
  }
}

void EditorRuntimeHost::applySceneDocument(const std::string &sceneId) {
  if (!m_sceneGraph || sceneId.empty()) {
    return;
  }

  if (m_project.scenesPath.empty()) {
    return;
  }

  namespace fs = std::filesystem;
  const fs::path scenePath =
      fs::path(m_project.scenesPath) / (sceneId + ".nmscene");
  const auto result = loadSceneDocument(scenePath.string());
  if (!result.isOk()) {
    return;
  }

  m_sceneGraph->clear();
  m_sceneGraph->setSceneId(sceneId);

  const auto &doc = result.value();
  for (const auto &item : doc.objects) {
    scene::SceneObjectState state;
    state.id = item.id;
    state.x = item.x;
    state.y = item.y;
    state.rotation = item.rotation;
    state.scaleX = item.scaleX;
    state.scaleY = item.scaleY;
    state.alpha = item.alpha;
    state.visible = item.visible;
    state.zOrder = item.zOrder;
    state.properties = item.properties;

    const std::string &type = item.type;
    if (type == "Background") {
      state.type = scene::SceneObjectType::Background;
      auto obj = std::make_unique<scene::BackgroundObject>(state.id);
      obj->loadState(state);
      m_sceneGraph->addToLayer(scene::LayerType::Background, std::move(obj));
    } else if (type == "Character") {
      state.type = scene::SceneObjectType::Character;
      std::string characterId = state.id;
      auto it = state.properties.find("characterId");
      if (it != state.properties.end() && !it->second.empty()) {
        characterId = it->second;
      }
      auto obj =
          std::make_unique<scene::CharacterObject>(state.id, characterId);
      obj->loadState(state);
      m_sceneGraph->addToLayer(scene::LayerType::Characters, std::move(obj));
    } else if (type == "Effect") {
      state.type = scene::SceneObjectType::EffectOverlay;
      auto obj = std::make_unique<scene::EffectOverlayObject>(state.id);
      obj->loadState(state);
      m_sceneGraph->addToLayer(scene::LayerType::Effects, std::move(obj));
    } else {
      continue;
    }
  }
}

} // namespace NovelMind::editor
