#pragma once

/**
 * @file validator.hpp
 * @brief AST Validator for semantic analysis of NM Script
 *
 * This module performs semantic analysis on the AST to detect:
 * - Undefined character/scene/variable references
 * - Unused characters, scenes, and variables
 * - Dead branches and unreachable code
 * - Duplicate definitions
 * - Type mismatches
 * - Invalid goto targets
 * - Missing asset references (backgrounds, audio files, sprites)
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ast.hpp"
#include "NovelMind/scripting/script_error.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::scripting {

/**
 * @brief Project context interface for asset validation
 *
 * Provides methods to check for asset existence in the project.
 * This interface allows the validator to check if asset files
 * referenced in scripts actually exist in the project.
 */
class IProjectContext {
public:
  virtual ~IProjectContext() = default;

  /**
   * @brief Check if a background asset exists
   * @param assetId Background asset identifier (e.g., "bg_city", "cafeteria")
   * @return true if the background asset exists
   */
  [[nodiscard]] virtual bool backgroundExists(const std::string& assetId) const = 0;

  /**
   * @brief Check if an audio asset exists
   * @param assetPath Audio file path (e.g., "music_theme.wav", "sfx_click.wav")
   * @param mediaType Type of media (sound, music, voice)
   * @return true if the audio asset exists
   */
  [[nodiscard]] virtual bool audioExists(const std::string& assetPath,
                                         const std::string& mediaType) const = 0;

  /**
   * @brief Check if a character sprite exists
   * @param characterId Character identifier
   * @return true if the character has sprite assets
   */
  [[nodiscard]] virtual bool characterSpriteExists(const std::string& characterId) const = 0;
};

/**
 * @brief Symbol information for tracking definitions and usages
 */
struct SymbolInfo {
  std::string name;
  SourceLocation definitionLocation;
  std::vector<SourceLocation> usageLocations;
  bool isDefined = false;
  bool isUsed = false;
};

/**
 * @brief Result of validation analysis
 */
struct ValidationResult {
  ErrorList errors;
  bool isValid;

  [[nodiscard]] bool hasErrors() const { return errors.hasErrors(); }

  [[nodiscard]] bool hasWarnings() const { return errors.hasWarnings(); }
};

/**
 * @brief Callback for checking if a scene file exists
 * @param sceneId The scene identifier
 * @return true if the .nmscene file exists
 */
using SceneFileExistsCallback = std::function<bool(const std::string& sceneId)>;

/**
 * @brief Callback for checking if an object exists in a scene
 * @param sceneId The scene identifier
 * @param objectId The object identifier (e.g., character name)
 * @return true if the object exists in the scene file
 */
using SceneObjectExistsCallback =
    std::function<bool(const std::string& sceneId, const std::string& objectId)>;

/**
 * @brief Callback for checking if an asset file exists
 * @param assetPath The asset path
 * @return true if the asset file exists
 */
using AssetFileExistsCallback = std::function<bool(const std::string& assetPath)>;

/**
 * @brief AST Validator for semantic analysis
 *
 * Performs comprehensive validation of NM Script AST including:
 * - Symbol resolution (characters, scenes, variables)
 * - Usage tracking for unused symbol detection
 * - Control flow analysis for dead code detection
 * - Type checking for expressions
 * - Optional resource validation (scene files, objects, assets)
 *
 * Example usage:
 * @code
 * Validator validator;
 * ValidationResult result = validator.validate(program);
 * if (result.hasErrors()) {
 *     for (const auto& error : result.errors.all()) {
 *         std::cerr << error.format() << std::endl;
 *     }
 * }
 * @endcode
 */
class Validator {
public:
  Validator();
  ~Validator();

  /**
   * @brief Validate a parsed AST program
   * @param program The program to validate
   * @return ValidationResult containing all errors and warnings
   */
  [[nodiscard]] ValidationResult validate(const Program& program);

  /**
   * @brief Configure whether to report unused symbols as warnings
   */
  void setReportUnused(bool report);

  /**
   * @brief Configure whether to report dead code as warnings
   */
  void setReportDeadCode(bool report);

  /**
   * @brief Set project context for asset validation
   * @param context Project context providing asset existence checks
   */
  void setProjectContext(IProjectContext* context);

  /**
   * @brief Configure whether to validate asset references
   */
  void setValidateAssets(bool validate);

  /**
   * @brief Set the source code for context in error messages
   * @param source The full source code string
   */
  void setSource(const std::string& source);

  /**
   * @brief Set the file path for error messages
   * @param path The file path
   */
  void setFilePath(const std::string& path);

  /**
   * @brief Set callback for checking scene file existence
   * @param callback Function that returns true if .nmscene file exists
   */
  void setSceneFileExistsCallback(SceneFileExistsCallback callback);

  /**
   * @brief Set callback for checking scene object existence
   * @param callback Function that returns true if object exists in scene
   */
  void setSceneObjectExistsCallback(SceneObjectExistsCallback callback);

  /**
   * @brief Set callback for checking asset file existence
   * @param callback Function that returns true if asset file exists
   */
  void setAssetFileExistsCallback(AssetFileExistsCallback callback);

private:
  // Reset state for new validation
  void reset();

  // First pass: collect all definitions
  void collectDefinitions(const Program& program);
  void collectCharacterDefinition(const CharacterDecl& decl);
  void collectSceneDefinition(const SceneDecl& decl);

  // Second pass: validate references and usage
  void validateProgram(const Program& program);
  void validateScene(const SceneDecl& decl);
  void validateStatement(const Statement& stmt, bool& reachable);
  void validateExpression(const Expression& expr);

  // Statement validators
  void validateShowStmt(const ShowStmt& stmt);
  void validateHideStmt(const HideStmt& stmt);
  void validateSayStmt(const SayStmt& stmt);
  void validateChoiceStmt(const ChoiceStmt& stmt, bool& reachable);
  void validateIfStmt(const IfStmt& stmt, bool& reachable);
  void validateGotoStmt(const GotoStmt& stmt, bool& reachable);
  void validateWaitStmt(const WaitStmt& stmt);
  void validatePlayStmt(const PlayStmt& stmt);
  void validateStopStmt(const StopStmt& stmt);
  void validateSetStmt(const SetStmt& stmt);
  void validateTransitionStmt(const TransitionStmt& stmt);
  void validateBlockStmt(const BlockStmt& stmt, bool& reachable);

  // Expression validators
  void validateLiteral(const LiteralExpr& expr);
  void validateIdentifier(const IdentifierExpr& expr, SourceLocation loc);
  void validateBinary(const BinaryExpr& expr);
  void validateUnary(const UnaryExpr& expr);
  void validateCall(const CallExpr& expr, SourceLocation loc);
  void validateProperty(const PropertyExpr& expr);

  // Control flow analysis
  void analyzeControlFlow(const Program& program);
  void findReachableScenes(const std::string& startScene, std::unordered_set<std::string>& visited);

  // Unused symbol detection
  void reportUnusedSymbols();

  // Helper methods
  void markCharacterUsed(const std::string& name, SourceLocation loc);
  void markSceneUsed(const std::string& name, SourceLocation loc);
  void markVariableUsed(const std::string& name, SourceLocation loc);
  void markVariableDefined(const std::string& name, SourceLocation loc);

  bool isCharacterDefined(const std::string& name) const;
  bool isSceneDefined(const std::string& name) const;
  bool isVariableDefined(const std::string& name) const;

  // Error reporting
  void error(ErrorCode code, const std::string& message, SourceLocation loc);
  void warning(ErrorCode code, const std::string& message, SourceLocation loc);
  void info(ErrorCode code, const std::string& message, SourceLocation loc);

  // Enhanced error with suggestions
  void errorWithSuggestions(ErrorCode code, const std::string& message, SourceLocation loc,
                            const std::vector<std::string>& suggestions);

  // Helper to get all symbol names of a type
  [[nodiscard]] std::vector<std::string> getAllCharacterNames() const;
  [[nodiscard]] std::vector<std::string> getAllSceneNames() const;
  [[nodiscard]] std::vector<std::string> getAllVariableNames() const;

  // Symbol tables
  std::unordered_map<std::string, SymbolInfo> m_characters;
  std::unordered_map<std::string, SymbolInfo> m_scenes;
  std::unordered_map<std::string, SymbolInfo> m_variables;

  // Scene control flow graph (scene -> scenes it can goto)
  std::unordered_map<std::string, std::unordered_set<std::string>> m_sceneGraph;

  // Current context
  std::string m_currentScene;
  SourceLocation m_currentLocation;

  // Configuration
  bool m_reportUnused = true;
  bool m_reportDeadCode = true;
  bool m_validateAssets = false;

  // Project context for asset validation
  IProjectContext* m_projectContext = nullptr;

  // Source context for error messages
  std::string m_source;
  std::string m_filePath;
  // Resource validation callbacks (optional)
  SceneFileExistsCallback m_sceneFileExistsCallback;
  SceneObjectExistsCallback m_sceneObjectExistsCallback;
  AssetFileExistsCallback m_assetFileExistsCallback;

  // Results
  ErrorList m_errors;
};

} // namespace NovelMind::scripting
