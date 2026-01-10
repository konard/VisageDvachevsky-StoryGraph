#pragma once

/**
 * @file project_integrity.hpp
 * @brief Project Integrity Checker for NovelMind Editor
 *
 * Comprehensive validation system that checks:
 * - Missing scenes and assets
 * - Missing voice lines and localization keys
 * - Unreferenced assets
 * - StoryGraph cycles and unreachable nodes
 * - Duplicate IDs
 * - Resource conflicts in MultiPack
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class ProjectIntegrityChecker;

/**
 * @brief Severity level for integrity issues
 */
enum class IssueSeverity : u8 {
  Info,    // Informational, not a problem
  Warning, // Potential problem
  Error,   // Definite problem, may cause runtime issues
  Critical // Build-blocking problem
};

/**
 * @brief Category of integrity issue
 */
enum class IssueCategory : u8 {
  Scene,        // Missing or invalid scene references
  Asset,        // Missing or invalid asset references
  VoiceLine,    // Missing voice line files
  Localization, // Missing localization keys
  StoryGraph,   // Graph structure issues
  Script,       // Script compilation issues
  Resource,     // Resource conflicts or duplicates
  Configuration // Project configuration issues
};

/**
 * @brief Represents a single integrity issue
 */
struct IntegrityIssue {
  IssueSeverity severity;
  IssueCategory category;
  std::string code;                     // Unique issue code (e.g., "E001")
  std::string message;                  // Human-readable description
  std::string filePath;                 // Related file (if any)
  i32 lineNumber = -1;                  // Line number (if applicable)
  std::string context;                  // Additional context
  std::vector<std::string> suggestions; // Suggested fixes

  // Quick fix support
  bool hasQuickFix = false;
  std::string quickFixDescription;
};

/**
 * @brief Summary statistics for integrity check
 */
struct IntegritySummary {
  i32 totalIssues = 0;
  i32 criticalCount = 0;
  i32 errorCount = 0;
  i32 warningCount = 0;
  i32 infoCount = 0;

  i32 sceneIssues = 0;
  i32 assetIssues = 0;
  i32 voiceIssues = 0;
  i32 localizationIssues = 0;
  i32 graphIssues = 0;
  i32 scriptIssues = 0;
  i32 resourceIssues = 0;
  i32 configIssues = 0;

  // Asset statistics
  i32 totalAssets = 0;
  i32 referencedAssets = 0;
  i32 unreferencedAssets = 0;

  // StoryGraph statistics
  i32 totalNodes = 0;
  i32 reachableNodes = 0;
  i32 unreachableNodes = 0;
  bool hasCycles = false;

  // Localization statistics
  i32 totalStrings = 0;
  i32 translatedStrings = 0;
  i32 missingTranslations = 0;
};

/**
 * @brief Result of an integrity check
 */
struct IntegrityReport {
  IntegritySummary summary;
  std::vector<IntegrityIssue> issues;
  u64 checkTimestamp = 0;
  f64 checkDurationMs = 0.0;
  bool passed = false; // True if no critical or error issues

  // Filter issues by severity
  [[nodiscard]] std::vector<IntegrityIssue> getIssuesBySeverity(IssueSeverity severity) const;

  // Filter issues by category
  [[nodiscard]] std::vector<IntegrityIssue> getIssuesByCategory(IssueCategory category) const;

  // Filter issues by file
  [[nodiscard]] std::vector<IntegrityIssue> getIssuesByFile(const std::string& filePath) const;
};

/**
 * @brief Configuration for integrity checking
 */
struct IntegrityCheckConfig {
  bool checkScenes = true;
  bool checkAssets = true;
  bool checkVoiceLines = true;
  bool checkLocalization = true;
  bool checkStoryGraph = true;
  bool checkScripts = true;
  bool checkResources = true;
  bool checkConfiguration = true;

  bool reportUnreferencedAssets = true;
  bool reportUnreachableNodes = true;
  bool reportCycles = true;
  bool reportMissingTranslations = true;

  std::vector<std::string> excludePatterns; // File patterns to exclude
  std::vector<std::string> locales;         // Locales to check
};

/**
 * @brief Listener for integrity check progress
 */
class IIntegrityCheckListener {
public:
  virtual ~IIntegrityCheckListener() = default;

  virtual void onCheckStarted() = 0;
  virtual void onCheckProgress(const std::string& currentTask, f32 progress) = 0;
  virtual void onIssueFound(const IntegrityIssue& issue) = 0;
  virtual void onCheckCompleted(const IntegrityReport& report) = 0;
};

/**
 * @brief Project Integrity Checker
 *
 * Performs comprehensive validation of a NovelMind project to detect:
 * - Missing scenes referenced in StoryGraph
 * - Missing assets (textures, audio, fonts)
 * - Missing voice line audio files
 * - Missing localization keys or translations
 * - Unreferenced assets (orphaned files)
 * - Cycles in StoryGraph (potential infinite loops)
 * - Unreachable nodes in StoryGraph
 * - Duplicate IDs across scenes/objects
 * - Resource conflicts in multi-pack configurations
 */
class ProjectIntegrityChecker {
public:
  ProjectIntegrityChecker();
  ~ProjectIntegrityChecker() = default;

  /**
   * @brief Set the project path to check
   */
  void setProjectPath(const std::string& projectPath);

  /**
   * @brief Set configuration for the check
   */
  void setConfig(const IntegrityCheckConfig& config);

  /**
   * @brief Run full integrity check
   */
  IntegrityReport runFullCheck();

  /**
   * @brief Run a quick check (most critical issues only)
   */
  IntegrityReport runQuickCheck();

  /**
   * @brief Check specific category only
   */
  IntegrityReport checkCategory(IssueCategory category);

  /**
   * @brief Check a specific file
   */
  std::vector<IntegrityIssue> checkFile(const std::string& filePath);

  /**
   * @brief Cancel ongoing check
   */
  void cancelCheck();

  /**
   * @brief Check if a check is in progress
   */
  [[nodiscard]] bool isCheckInProgress() const { return m_checkInProgress; }

  /**
   * @brief Add a check listener
   */
  void addListener(IIntegrityCheckListener* listener);

  /**
   * @brief Remove a listener
   */
  void removeListener(IIntegrityCheckListener* listener);

  /**
   * @brief Apply a quick fix for an issue
   */
  Result<void> applyQuickFix(const IntegrityIssue& issue);

  /**
   * @brief Get the last check report
   */
  [[nodiscard]] const IntegrityReport& getLastReport() const { return m_lastReport; }

private:
  // Check functions for each category
  void checkSceneReferences(std::vector<IntegrityIssue>& issues);
  void checkAssetReferences(std::vector<IntegrityIssue>& issues);
  void checkVoiceLines(std::vector<IntegrityIssue>& issues);
  void checkLocalizationKeys(std::vector<IntegrityIssue>& issues);
  void checkStoryGraphStructure(std::vector<IntegrityIssue>& issues);
  void checkScriptSyntax(std::vector<IntegrityIssue>& issues);
  void checkResourceConflicts(std::vector<IntegrityIssue>& issues);
  void checkProjectConfiguration(std::vector<IntegrityIssue>& issues);

  // StoryGraph analysis
  void analyzeReachability(std::vector<IntegrityIssue>& issues);
  void detectCycles(std::vector<IntegrityIssue>& issues);
  void checkDeadEnds(std::vector<IntegrityIssue>& issues);

  // Asset analysis
  void scanProjectAssets();
  void collectAssetReferences();
  void findOrphanedAssets(std::vector<IntegrityIssue>& issues);

  // Localization analysis
  void scanLocalizationFiles();
  void checkMissingTranslations(std::vector<IntegrityIssue>& issues);
  void checkUnusedStrings(std::vector<IntegrityIssue>& issues);

  // Helper functions
  void reportProgress(const std::string& task, f32 progress);
  void reportIssue(const IntegrityIssue& issue);
  IntegritySummary calculateSummary(const std::vector<IntegrityIssue>& issues);

  bool shouldExclude(const std::string& path) const;

  std::string m_projectPath;
  IntegrityCheckConfig m_config;
  IntegrityReport m_lastReport;

  bool m_checkInProgress = false;
  bool m_cancelRequested = false;

  // Collected data during check
  std::unordered_set<std::string> m_projectAssets;
  std::unordered_set<std::string> m_referencedAssets;
  std::unordered_map<std::string, std::vector<std::string>> m_localizationStrings;

  std::vector<IIntegrityCheckListener*> m_listeners;
  std::vector<IntegrityIssue> m_currentIssues;
};

/**
 * @brief Quick fix implementations
 */
namespace QuickFixes {
/**
 * @brief Remove a reference to a missing scene
 */
Result<void> removeMissingSceneReference(const std::string& projectPath,
                                         const std::string& sceneId);

/**
 * @brief Create a stand-in for a missing asset
 */
Result<void> createPlaceholderAsset(const std::string& projectPath, const std::string& assetPath);

/**
 * @brief Add a missing localization key with default value
 */
Result<void> addMissingLocalizationKey(const std::string& projectPath, const std::string& key,
                                       const std::string& locale);

/**
 * @brief Remove orphaned asset references
 */
Result<void> removeOrphanedReferences(const std::string& projectPath,
                                      const std::vector<std::string>& assetPaths);

/**
 * @brief Connect an unreachable node to the graph
 */
Result<void> connectUnreachableNode(const std::string& projectPath, scripting::NodeId nodeId);

/**
 * @brief Resolve duplicate ID by renaming
 */
Result<void> resolveDuplicateId(const std::string& projectPath, const std::string& duplicateId);

/**
 * @brief Create an empty scene file
 * @param projectPath Path to the project root
 * @param sceneId Scene identifier (file will be created as sceneId.nmscene)
 */
Result<void> createEmptyScene(const std::string& projectPath, const std::string& sceneId);

/**
 * @brief Set the first available scene as the start scene
 * @param projectPath Path to the project root
 */
Result<void> setFirstSceneAsStart(const std::string& projectPath);

/**
 * @brief Create a main entry point scene
 * @param projectPath Path to the project root
 */
Result<void> createMainEntryScene(const std::string& projectPath);

/**
 * @brief Remove an orphaned/unreferenced asset file
 * @param projectPath Path to the project root
 * @param assetPath Relative path to the asset
 */
Result<void> removeOrphanedAsset(const std::string& projectPath, const std::string& assetPath);

/**
 * @brief Create a default project.json configuration file
 * @param projectPath Path to the project root
 * @param projectName Name of the project
 */
Result<void> createDefaultProjectConfig(const std::string& projectPath,
                                        const std::string& projectName);
} // namespace QuickFixes

} // namespace NovelMind::editor
