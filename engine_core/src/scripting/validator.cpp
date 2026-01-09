#include "NovelMind/scripting/validator.hpp"

namespace NovelMind::scripting {

Validator::Validator() = default;
Validator::~Validator() = default;

ValidationResult Validator::validate(const Program &program) {
  reset();

  // First pass: collect all definitions
  collectDefinitions(program);

  // Second pass: validate references and usage
  validateProgram(program);

  // Third pass: control flow analysis
  analyzeControlFlow(program);

  // Report unused symbols if configured
  if (m_reportUnused) {
    reportUnusedSymbols();
  }

  ValidationResult result;
  result.errors = std::move(m_errors);
  result.isValid = !result.errors.hasErrors();
  return result;
}

void Validator::setReportUnused(bool report) { m_reportUnused = report; }

void Validator::setReportDeadCode(bool report) { m_reportDeadCode = report; }

void Validator::setProjectContext(IProjectContext *context) {
  m_projectContext = context;
}

void Validator::setValidateAssets(bool validate) {
  m_validateAssets = validate;
void Validator::setSource(const std::string &source) { m_source = source; }

void Validator::setFilePath(const std::string &path) { m_filePath = path; }
void Validator::setSceneFileExistsCallback(SceneFileExistsCallback callback) {
  m_sceneFileExistsCallback = std::move(callback);
}

void Validator::setSceneObjectExistsCallback(
    SceneObjectExistsCallback callback) {
  m_sceneObjectExistsCallback = std::move(callback);
}

void Validator::setAssetFileExistsCallback(AssetFileExistsCallback callback) {
  m_assetFileExistsCallback = std::move(callback);
}

void Validator::reset() {
  m_characters.clear();
  m_scenes.clear();
  m_variables.clear();
  m_sceneGraph.clear();
  m_currentScene.clear();
  m_currentLocation = {};
  m_errors.clear();
}

// First pass: collect definitions

void Validator::collectDefinitions(const Program &program) {
  // Collect character definitions
  for (const auto &character : program.characters) {
    collectCharacterDefinition(character);
  }

  // Collect scene definitions
  for (const auto &scene : program.scenes) {
    collectSceneDefinition(scene);
  }
}

void Validator::collectCharacterDefinition(const CharacterDecl &decl) {
  auto it = m_characters.find(decl.id);
  if (it != m_characters.end() && it->second.isDefined) {
    // Duplicate definition
    ScriptError err(ErrorCode::DuplicateCharacterDefinition, Severity::Error,
                    "Character '" + decl.id + "' is already defined",
                    m_currentLocation);
    err.withRelated(it->second.definitionLocation, "Previously defined here");
    m_errors.add(std::move(err));
    return;
  }

  SymbolInfo info;
  info.name = decl.id;
  info.definitionLocation = m_currentLocation;
  info.isDefined = true;
  m_characters[decl.id] = std::move(info);
}

void Validator::collectSceneDefinition(const SceneDecl &decl) {
  auto it = m_scenes.find(decl.name);
  if (it != m_scenes.end() && it->second.isDefined) {
    // Duplicate definition
    ScriptError err(ErrorCode::DuplicateSceneDefinition, Severity::Error,
                    "Scene '" + decl.name + "' is already defined",
                    m_currentLocation);
    err.withRelated(it->second.definitionLocation, "Previously defined here");
    m_errors.add(std::move(err));
    return;
  }

  SymbolInfo info;
  info.name = decl.name;
  info.definitionLocation = m_currentLocation;
  info.isDefined = true;
  m_scenes[decl.name] = std::move(info);

  // Initialize scene in the control flow graph
  m_sceneGraph[decl.name] = {};
}

// Second pass: validate references

void Validator::validateProgram(const Program &program) {
  // Validate scenes
  for (const auto &scene : program.scenes) {
    validateScene(scene);
  }

  // Validate global statements
  bool reachable = true;
  for (const auto &stmt : program.globalStatements) {
    if (stmt) {
      validateStatement(*stmt, reachable);
    }
  }
}

void Validator::validateScene(const SceneDecl &decl) {
  m_currentScene = decl.name;

  // Check if scene file exists (if callback is provided)
  if (m_sceneFileExistsCallback) {
    if (!m_sceneFileExistsCallback(decl.name)) {
      warning(ErrorCode::MissingSceneFile,
              "Scene file '" + decl.name +
                  ".nmscene' not found - scene objects cannot be validated",
              m_currentLocation);
    }
  }

  // Check for empty scene
  if (decl.body.empty()) {
    warning(ErrorCode::EmptyScene, "Scene '" + decl.name + "' is empty",
            m_currentLocation);
  }

  bool reachable = true;
  for (const auto &stmt : decl.body) {
    if (stmt) {
      if (!reachable && m_reportDeadCode) {
        warning(ErrorCode::UnreachableCode,
                "Unreachable code after unconditional goto", stmt->location);
      }
      validateStatement(*stmt, reachable);
    }
  }

  m_currentScene.clear();
}

void Validator::validateStatement(const Statement &stmt, bool &reachable) {
  m_currentLocation = stmt.location;

  std::visit(
      [this, &reachable](const auto &s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, ShowStmt>) {
          validateShowStmt(s);
        } else if constexpr (std::is_same_v<T, HideStmt>) {
          validateHideStmt(s);
        } else if constexpr (std::is_same_v<T, SayStmt>) {
          validateSayStmt(s);
        } else if constexpr (std::is_same_v<T, ChoiceStmt>) {
          validateChoiceStmt(s, reachable);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          validateIfStmt(s, reachable);
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          validateGotoStmt(s, reachable);
        } else if constexpr (std::is_same_v<T, WaitStmt>) {
          validateWaitStmt(s);
        } else if constexpr (std::is_same_v<T, PlayStmt>) {
          validatePlayStmt(s);
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          validateStopStmt(s);
        } else if constexpr (std::is_same_v<T, SetStmt>) {
          validateSetStmt(s);
        } else if constexpr (std::is_same_v<T, TransitionStmt>) {
          validateTransitionStmt(s);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          validateBlockStmt(s, reachable);
        } else if constexpr (std::is_same_v<T, ExpressionStmt>) {
          if (s.expression) {
            validateExpression(*s.expression);
          }
        }
      },
      stmt.data);
}

void Validator::validateExpression(const Expression &expr) {
  std::visit(
      [this, &expr](const auto &e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, LiteralExpr>) {
          validateLiteral(e);
        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          validateIdentifier(e, expr.location);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          validateBinary(e);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          validateUnary(e);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          validateCall(e, expr.location);
        } else if constexpr (std::is_same_v<T, PropertyExpr>) {
          validateProperty(e);
        }
      },
      expr.data);
}

// Statement validators

void Validator::validateShowStmt(const ShowStmt &stmt) {
  switch (stmt.target) {
  case ShowStmt::Target::Character:
  case ShowStmt::Target::Sprite: {
    if (!isCharacterDefined(stmt.identifier)) {
      // Find similar character names for "Did you mean?" suggestions
      auto allCharacters = getAllCharacterNames();
      auto similar = findSimilarStrings(stmt.identifier, allCharacters);

      std::vector<std::string> suggestions;
      if (!similar.empty()) {
        // Add "Did you mean?" suggestion for the first match
        suggestions.push_back("Did you mean '" + similar[0] + "'?");
      }
      // Add declaration suggestion
      suggestions.push_back("Add declaration: character " + stmt.identifier +
                            "(name=\"" + stmt.identifier + "\")");

      errorWithSuggestions(ErrorCode::UndefinedCharacter,
                           "Undefined character '" + stmt.identifier + "'",
                           m_currentLocation, suggestions);
    } else {
      markCharacterUsed(stmt.identifier, m_currentLocation);
    }

    // Validate character sprite asset exists
    if (m_validateAssets && m_projectContext) {
      if (!m_projectContext->characterSpriteExists(stmt.identifier)) {
        warning(ErrorCode::UndefinedResource,
                "Character sprite asset for '" + stmt.identifier +
                    "' not found in project",
    // Check if character object exists in current scene (if callback provided)
    if (m_sceneObjectExistsCallback && !m_currentScene.empty()) {
      if (!m_sceneObjectExistsCallback(m_currentScene, stmt.identifier)) {
        warning(ErrorCode::MissingSceneObject,
                "Object '" + stmt.identifier + "' not found in scene '" +
                    m_currentScene + "'",
                m_currentLocation);
      }
    }
    break;
  }

  case ShowStmt::Target::Background:
    // Validate background asset exists
    if (m_validateAssets && m_projectContext) {
      // The resource field contains the background identifier
      const std::string &bgId =
          stmt.resource.has_value() ? stmt.resource.value() : stmt.identifier;
      if (!bgId.empty() && !m_projectContext->backgroundExists(bgId)) {
        warning(ErrorCode::UndefinedResource,
                "Background asset '" + bgId + "' not found in project",
    // Validate background asset if resource is specified
    if (stmt.resource.has_value() && m_assetFileExistsCallback) {
      const std::string &assetPath = stmt.resource.value();
      if (!m_assetFileExistsCallback(assetPath)) {
        warning(ErrorCode::MissingAssetFile,
                "Asset '" + assetPath + "' not found in project",
                m_currentLocation);
      }
    }
    break;
  }

  // Validate transition if present
  if (stmt.transition.has_value()) {
    // Validate known transition types
    const std::string &trans = stmt.transition.value();
    static const std::unordered_set<std::string> validTransitions = {
        "fade", "slide", "dissolve", "none"};
    if (validTransitions.find(trans) == validTransitions.end()) {
      warning(ErrorCode::UndefinedResource,
              "Unknown transition type '" + trans + "'", m_currentLocation);
    }
  }
}

void Validator::validateHideStmt(const HideStmt &stmt) {
  if (!isCharacterDefined(stmt.identifier)) {
    // Find similar character names for "Did you mean?" suggestions
    auto allCharacters = getAllCharacterNames();
    auto similar = findSimilarStrings(stmt.identifier, allCharacters);

    std::vector<std::string> suggestions;
    if (!similar.empty()) {
      suggestions.push_back("Did you mean '" + similar[0] + "'?");
    }
    suggestions.push_back("Add declaration: character " + stmt.identifier +
                          "(name=\"" + stmt.identifier + "\")");

    errorWithSuggestions(ErrorCode::UndefinedCharacter,
                         "Undefined character '" + stmt.identifier + "'",
                         m_currentLocation, suggestions);
  } else {
    markCharacterUsed(stmt.identifier, m_currentLocation);
  }
}

void Validator::validateSayStmt(const SayStmt &stmt) {
  if (stmt.speaker.has_value()) {
    const std::string &speaker = stmt.speaker.value();
    if (!isCharacterDefined(speaker)) {
      // Find similar character names for "Did you mean?" suggestions
      auto allCharacters = getAllCharacterNames();
      auto similar = findSimilarStrings(speaker, allCharacters);

      std::vector<std::string> suggestions;
      if (!similar.empty()) {
        suggestions.push_back("Did you mean '" + similar[0] + "'?");
      }
      suggestions.push_back("Add declaration: character " + speaker +
                            "(name=\"" + speaker + "\")");

      errorWithSuggestions(ErrorCode::UndefinedCharacter,
                           "Undefined character '" + speaker + "'",
                           m_currentLocation, suggestions);
    } else {
      markCharacterUsed(speaker, m_currentLocation);
    }
  }

  // Check for empty dialogue
  if (stmt.text.empty()) {
    warning(ErrorCode::InvalidSyntax, "Empty dialogue text", m_currentLocation);
  }
}

void Validator::validateChoiceStmt(const ChoiceStmt &stmt,
                                   bool & /*reachable*/) {
  if (stmt.options.empty()) {
    error(ErrorCode::EmptyChoiceBlock, "Choice block has no options",
          m_currentLocation);
    return;
  }

  // Check for duplicate choice texts
  std::unordered_set<std::string> seenTexts;
  for (const auto &option : stmt.options) {
    if (seenTexts.find(option.text) != seenTexts.end()) {
      warning(ErrorCode::DuplicateChoiceText,
              "Duplicate choice text: '" + option.text + "'",
              m_currentLocation);
    }
    seenTexts.insert(option.text);

    // Validate condition if present
    if (option.condition && option.condition.value()) {
      validateExpression(*(option.condition.value()));
    }

    // Validate goto target if present
    if (option.gotoTarget.has_value()) {
      const std::string &target = option.gotoTarget.value();
      if (!isSceneDefined(target)) {
        // Find similar scene names for "Did you mean?" suggestions
        auto allScenes = getAllSceneNames();
        auto similar = findSimilarStrings(target, allScenes);

        std::vector<std::string> suggestions;
        if (!similar.empty()) {
          suggestions.push_back("Did you mean '" + similar[0] + "'?");
        }
        suggestions.push_back("Create scene: scene " + target + " { ... }");

        errorWithSuggestions(ErrorCode::UndefinedScene,
                             "Undefined scene '" + target + "' in choice goto",
                             m_currentLocation, suggestions);
      } else {
        markSceneUsed(target, m_currentLocation);
        // Add to control flow graph
        if (!m_currentScene.empty()) {
          m_sceneGraph[m_currentScene].insert(target);
        }
      }
    } else if (option.body.empty()) {
      warning(ErrorCode::ChoiceWithoutBranch,
              "Choice option has no body and no goto target",
              m_currentLocation);
    }

    // Validate choice body
    bool bodyReachable = true;
    for (const auto &bodyStmt : option.body) {
      if (bodyStmt) {
        validateStatement(*bodyStmt, bodyReachable);
      }
    }
  }

  // After a choice, code is still reachable (player will pick one)
}

void Validator::validateIfStmt(const IfStmt &stmt, bool &reachable) {
  // Validate condition
  if (stmt.condition) {
    validateExpression(*stmt.condition);
  } else {
    error(ErrorCode::ExpectedExpression, "If statement requires a condition",
          m_currentLocation);
  }

  // Validate then branch
  bool thenReachable = true;
  for (const auto &thenStmt : stmt.thenBranch) {
    if (thenStmt) {
      validateStatement(*thenStmt, thenReachable);
    }
  }

  // Validate else branch
  bool elseReachable = true;
  for (const auto &elseStmt : stmt.elseBranch) {
    if (elseStmt) {
      validateStatement(*elseStmt, elseReachable);
    }
  }

  // Code after if is reachable if either branch is reachable
  reachable = thenReachable || elseReachable;
}

void Validator::validateGotoStmt(const GotoStmt &stmt, bool &reachable) {
  if (!isSceneDefined(stmt.target)) {
    // Find similar scene names for "Did you mean?" suggestions
    auto allScenes = getAllSceneNames();
    auto similar = findSimilarStrings(stmt.target, allScenes);

    std::vector<std::string> suggestions;
    if (!similar.empty()) {
      suggestions.push_back("Did you mean '" + similar[0] + "'?");
    }
    suggestions.push_back("Create scene: scene " + stmt.target + " { ... }");

    errorWithSuggestions(ErrorCode::UndefinedScene,
                         "Undefined scene '" + stmt.target + "'",
                         m_currentLocation, suggestions);
  } else {
    markSceneUsed(stmt.target, m_currentLocation);

    // Add to control flow graph
    if (!m_currentScene.empty()) {
      m_sceneGraph[m_currentScene].insert(stmt.target);
    }
  }

  // After unconditional goto, code is unreachable
  reachable = false;
}

void Validator::validateWaitStmt(const WaitStmt &stmt) {
  if (stmt.duration < 0.0f) {
    warning(ErrorCode::InvalidSyntax, "Wait duration should be positive",
            m_currentLocation);
  }
}

void Validator::validatePlayStmt(const PlayStmt &stmt) {
  if (stmt.resource.empty()) {
    error(ErrorCode::InvalidResourcePath,
          "Play statement requires a resource path", m_currentLocation);
  } else {
    // Validate audio asset exists
    if (m_validateAssets && m_projectContext) {
      std::string mediaType;
      switch (stmt.type) {
      case PlayStmt::MediaType::Sound:
        mediaType = "sound";
        break;
      case PlayStmt::MediaType::Music:
        mediaType = "music";
        break;
      }

      if (!m_projectContext->audioExists(stmt.resource, mediaType)) {
        warning(ErrorCode::UndefinedResource,
                "Audio asset '" + stmt.resource + "' not found in project " +
                    mediaType + " directory",
                m_currentLocation);
      }
  } else if (m_assetFileExistsCallback) {
    // Check if audio asset exists
    if (!m_assetFileExistsCallback(stmt.resource)) {
      warning(ErrorCode::MissingAssetFile,
              "Asset '" + stmt.resource + "' not found in project",
              m_currentLocation);
    }
  }

  if (stmt.volume.has_value()) {
    f32 vol = stmt.volume.value();
    if (vol < 0.0f || vol > 1.0f) {
      warning(ErrorCode::InvalidSyntax, "Volume should be between 0.0 and 1.0",
              m_currentLocation);
    }
  }
}

void Validator::validateStopStmt(const StopStmt &stmt) {
  if (stmt.fadeOut.has_value() && stmt.fadeOut.value() < 0.0f) {
    warning(ErrorCode::InvalidSyntax, "Fade out duration should be positive",
            m_currentLocation);
  }
}

void Validator::validateSetStmt(const SetStmt &stmt) {
  // Mark variable as defined
  markVariableDefined(stmt.variable, m_currentLocation);

  // Validate value expression
  if (stmt.value) {
    validateExpression(*stmt.value);
  } else {
    error(ErrorCode::ExpectedExpression, "Set statement requires a value",
          m_currentLocation);
  }
}

void Validator::validateTransitionStmt(const TransitionStmt &stmt) {
  // Validate known transition types
  static const std::unordered_set<std::string> validTransitions = {
      "fade", "slide", "dissolve", "none", "fadethrough"};
  if (validTransitions.find(stmt.type) == validTransitions.end()) {
    warning(ErrorCode::UndefinedResource,
            "Unknown transition type '" + stmt.type + "'", m_currentLocation);
  }

  if (stmt.duration < 0.0f) {
    warning(ErrorCode::InvalidSyntax, "Transition duration should be positive",
            m_currentLocation);
  }
}

void Validator::validateBlockStmt(const BlockStmt &stmt, bool &reachable) {
  for (const auto &s : stmt.statements) {
    if (s) {
      validateStatement(*s, reachable);
    }
  }
}

// Expression validators

void Validator::validateLiteral(const LiteralExpr & /*expr*/) {
  // Literals are always valid
}

void Validator::validateIdentifier(const IdentifierExpr &expr,
                                   SourceLocation loc) {
  // Check if it's a known variable
  markVariableUsed(expr.name, loc);
}

void Validator::validateBinary(const BinaryExpr &expr) {
  if (expr.left) {
    validateExpression(*expr.left);
  }
  if (expr.right) {
    validateExpression(*expr.right);
  }
}

void Validator::validateUnary(const UnaryExpr &expr) {
  if (expr.operand) {
    validateExpression(*expr.operand);
  }
}

void Validator::validateCall(const CallExpr &expr, SourceLocation /*loc*/) {
  // Validate arguments
  for (const auto &arg : expr.arguments) {
    if (arg) {
      validateExpression(*arg);
    }
  }

  // Note: Function validation could be extended to check for known functions
}

void Validator::validateProperty(const PropertyExpr &expr) {
  if (expr.object) {
    validateExpression(*expr.object);
  }
}

// Control flow analysis

void Validator::analyzeControlFlow(const Program &program) {
  if (program.scenes.empty()) {
    return;
  }

  // Find the first scene (entry point)
  const std::string &startScene = program.scenes[0].name;

  // Find all reachable scenes
  std::unordered_set<std::string> reachable;
  findReachableScenes(startScene, reachable);

  // Report unreachable scenes
  if (m_reportDeadCode) {
    for (const auto &[sceneName, info] : m_scenes) {
      if (info.isDefined && reachable.find(sceneName) == reachable.end()) {
        // First scene is always reachable
        if (sceneName != startScene) {
          warning(ErrorCode::UnreachableScene,
                  "Scene '" + sceneName +
                      "' is unreachable from the starting scene",
                  info.definitionLocation);
        }
      }
    }
  }
}

void Validator::findReachableScenes(const std::string &startScene,
                                    std::unordered_set<std::string> &visited) {
  if (visited.find(startScene) != visited.end()) {
    return; // Already visited
  }

  visited.insert(startScene);

  auto it = m_sceneGraph.find(startScene);
  if (it != m_sceneGraph.end()) {
    for (const auto &targetScene : it->second) {
      findReachableScenes(targetScene, visited);
    }
  }
}

// Unused symbol detection

void Validator::reportUnusedSymbols() {
  // Report unused characters
  for (const auto &[name, info] : m_characters) {
    if (info.isDefined && !info.isUsed) {
      warning(ErrorCode::UnusedCharacter,
              "Character '" + name + "' is defined but never used",
              info.definitionLocation);
    }
  }

  // Report unused scenes (excluding the first scene)
  bool firstScene = true;
  for (const auto &[name, info] : m_scenes) {
    if (firstScene) {
      firstScene = false;
      continue;
    }
    if (info.isDefined && !info.isUsed) {
      warning(ErrorCode::UnusedScene,
              "Scene '" + name + "' is defined but never referenced by goto",
              info.definitionLocation);
    }
  }

  // Report unused variables
  for (const auto &[name, info] : m_variables) {
    if (info.isDefined && !info.isUsed) {
      warning(ErrorCode::UnusedVariable,
              "Variable '" + name + "' is set but never read",
              info.definitionLocation);
    }
  }
}

// Helper methods

void Validator::markCharacterUsed(const std::string &name, SourceLocation loc) {
  auto it = m_characters.find(name);
  if (it != m_characters.end()) {
    it->second.isUsed = true;
    it->second.usageLocations.push_back(loc);
  }
}

void Validator::markSceneUsed(const std::string &name, SourceLocation loc) {
  auto it = m_scenes.find(name);
  if (it != m_scenes.end()) {
    it->second.isUsed = true;
    it->second.usageLocations.push_back(loc);
  }
}

void Validator::markVariableUsed(const std::string &name, SourceLocation loc) {
  auto it = m_variables.find(name);
  if (it != m_variables.end()) {
    it->second.isUsed = true;
    it->second.usageLocations.push_back(loc);
  } else {
    // Variable used but not defined - could be an error in strict mode
    // For now, we allow global variables to be used before set
    SymbolInfo info;
    info.name = name;
    info.isUsed = true;
    info.usageLocations.push_back(loc);
    m_variables[name] = std::move(info);
  }
}

void Validator::markVariableDefined(const std::string &name,
                                    SourceLocation loc) {
  auto it = m_variables.find(name);
  if (it != m_variables.end()) {
    // Variable already exists - just update definition location
    if (!it->second.isDefined) {
      it->second.isDefined = true;
      it->second.definitionLocation = loc;
    }
  } else {
    SymbolInfo info;
    info.name = name;
    info.definitionLocation = loc;
    info.isDefined = true;
    m_variables[name] = std::move(info);
  }
}

bool Validator::isCharacterDefined(const std::string &name) const {
  auto it = m_characters.find(name);
  return it != m_characters.end() && it->second.isDefined;
}

bool Validator::isSceneDefined(const std::string &name) const {
  auto it = m_scenes.find(name);
  return it != m_scenes.end() && it->second.isDefined;
}

bool Validator::isVariableDefined(const std::string &name) const {
  auto it = m_variables.find(name);
  return it != m_variables.end() && it->second.isDefined;
}

// Error reporting

void Validator::error(ErrorCode code, const std::string &message,
                      SourceLocation loc) {
  ScriptError err(code, Severity::Error, message, loc);

  // Add source context if available
  if (!m_source.empty()) {
    err.withSource(m_source);
  }

  // Add file path if available
  if (!m_filePath.empty()) {
    err.withFilePath(m_filePath);
  }

  m_errors.add(std::move(err));
}

void Validator::warning(ErrorCode code, const std::string &message,
                        SourceLocation loc) {
  ScriptError err(code, Severity::Warning, message, loc);

  // Add source context if available
  if (!m_source.empty()) {
    err.withSource(m_source);
  }

  // Add file path if available
  if (!m_filePath.empty()) {
    err.withFilePath(m_filePath);
  }

  m_errors.add(std::move(err));
}

void Validator::info(ErrorCode code, const std::string &message,
                     SourceLocation loc) {
  ScriptError err(code, Severity::Info, message, loc);

  // Add source context if available
  if (!m_source.empty()) {
    err.withSource(m_source);
  }

  // Add file path if available
  if (!m_filePath.empty()) {
    err.withFilePath(m_filePath);
  }

  m_errors.add(std::move(err));
}

void Validator::errorWithSuggestions(ErrorCode code, const std::string &message,
                                     SourceLocation loc,
                                     const std::vector<std::string> &suggestions) {
  ScriptError err(code, Severity::Error, message, loc);

  // Add source context if available
  if (!m_source.empty()) {
    err.withSource(m_source);
  }

  // Add file path if available
  if (!m_filePath.empty()) {
    err.withFilePath(m_filePath);
  }

  // Add suggestions
  for (const auto &suggestion : suggestions) {
    err.withSuggestion(suggestion);
  }

  m_errors.add(std::move(err));
}

// Helper functions to get all symbol names

std::vector<std::string> Validator::getAllCharacterNames() const {
  std::vector<std::string> names;
  names.reserve(m_characters.size());
  for (const auto &[name, info] : m_characters) {
    if (info.isDefined) {
      names.push_back(name);
    }
  }
  return names;
}

std::vector<std::string> Validator::getAllSceneNames() const {
  std::vector<std::string> names;
  names.reserve(m_scenes.size());
  for (const auto &[name, info] : m_scenes) {
    if (info.isDefined) {
      names.push_back(name);
    }
  }
  return names;
}

std::vector<std::string> Validator::getAllVariableNames() const {
  std::vector<std::string> names;
  names.reserve(m_variables.size());
  for (const auto &[name, info] : m_variables) {
    if (info.isDefined) {
      names.push_back(name);
    }
  }
  return names;
}

} // namespace NovelMind::scripting
