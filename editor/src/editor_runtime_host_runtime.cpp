#include "NovelMind/editor/editor_runtime_host.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/scene_document.hpp"
#include "NovelMind/scripting/ir.hpp"
#include "editor_runtime_host_detail.hpp"

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
 * @brief Convert story graph JSON to NMScript text
 *
 * This generates script content from the story graph visual representation.
 * Used when PlaybackSourceMode::Graph or Mixed is selected.
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

  // First pass: collect all scene nodes
  std::vector<const JsonValue *> sceneNodes;
  for (const auto &node : nodesIt->second.arrayValue) {
    if (node.type != "object") {
      continue;
    }
    std::string nodeType = jsonGetString(node, "type");
    if (nodeType == "Scene") {
      sceneNodes.push_back(&node);
    }
  }

  // Generate script from scene nodes
  for (const auto *nodePtr : sceneNodes) {
    const JsonValue &node = *nodePtr;
    std::string nodeId = jsonGetString(node, "id");
    std::string title = jsonGetString(node, "title");
    std::string dialogueText = jsonGetString(node, "dialogueText");
    if (dialogueText.empty()) {
      dialogueText = jsonGetString(node, "text");
    }
    std::string speaker = jsonGetString(node, "speaker");

    // Use ID as scene name
    script << "scene " << nodeId << " {\n";

    // Add dialogue if present
    if (!dialogueText.empty()) {
      // Escape quotes in dialogue
      std::string escapedText = dialogueText;
      size_t pos = 0;
      while ((pos = escapedText.find('"', pos)) != std::string::npos) {
        escapedText.insert(pos, "\\");
        pos += 2;
      }

      if (!speaker.empty()) {
        script << "    say " << speaker << " \"" << escapedText << "\"\n";
      } else {
        script << "    say \"" << escapedText << "\"\n";
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
          script << "        \"" << optionText << "\" -> goto "
                 << targetValue.stringValue << "\n";
        }
      }
      script << "    }\n";
    }

    script << "}\n\n";
  }

  return script.str();
}

/**
 * @brief Load story graph from project's .novelmind/story_graph.json
 */
Result<std::string> loadStoryGraphScript(const std::string &projectPath) {
  fs::path graphPath = fs::path(projectPath) / ".novelmind" / "story_graph.json";

  if (!fs::exists(graphPath)) {
    return Result<std::string>::error("Story graph file not found: " +
                                      graphPath.string());
  }

  std::ifstream file(graphPath);
  if (!file) {
    return Result<std::string>::error("Failed to open story graph file: " +
                                      graphPath.string());
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string jsonContent = buffer.str();
  file.close();

  // Parse JSON
  auto parseResult = SimpleJsonParser::parse(jsonContent);
  if (!parseResult.isOk()) {
    return Result<std::string>::error("Failed to parse story graph JSON: " +
                                      parseResult.error());
  }

  // Convert to script
  std::string script = generateScriptFromGraphJson(parseResult.value());
  if (script.empty()) {
    return Result<std::string>::error("No scene nodes found in story graph");
  }

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

    std::string allScripts;
    m_sceneNames.clear();

    // Handle different playback source modes (Issue #94)
    switch (sourceMode) {
    case PlaybackSourceMode::Graph: {
      // Load story graph and convert to script
      auto graphScriptResult = loadStoryGraphScript(m_project.path);
      if (!graphScriptResult.isOk()) {
        // Fall back to script mode if graph is not available
        return Result<void>::error(
            "Graph mode selected but story graph not available: " +
            graphScriptResult.error() +
            ". Switch to Script mode or create a Story Graph.");
      }
      allScripts = "// Generated from Story Graph (Graph Mode)\n";
      allScripts += graphScriptResult.value();

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
      return Result<void>::error("No content found for playback. "
                                 "Check your scripts or Story Graph.");
    }

    // Lexer
    scripting::Lexer lexer;
    auto tokensResult = lexer.tokenize(allScripts);

    if (!tokensResult.isOk()) {
      return Result<void>::error("Lexer error: " + tokensResult.error());
    }

    // Parser
    scripting::Parser parser;
    auto parseResult = parser.parse(tokensResult.value());

    if (!parseResult.isOk()) {
      return Result<void>::error("Parse error: " + parseResult.error());
    }

    m_program =
        std::make_unique<scripting::Program>(std::move(parseResult.value()));

    // Extract scene names
    for (const auto &scene : m_program->scenes) {
      m_sceneNames.push_back(scene.name);
    }

    // Validator
    scripting::Validator validator;
    auto validationResult = validator.validate(*m_program);

    if (validationResult.hasErrors()) {
      std::string errorMsg = "Validation errors:\n";
      for (const auto &err : validationResult.errors.errors()) {
        errorMsg += "  " + err.format() + "\n";
      }
      return Result<void>::error(errorMsg);
    }

    // Compiler
    scripting::Compiler compiler;
    auto compileResult = compiler.compile(*m_program);

    if (!compileResult.isOk()) {
      return Result<void>::error("Compilation error: " + compileResult.error());
    }

    m_compiledScript = std::make_unique<scripting::CompiledScript>(
        std::move(compileResult.value()));

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
