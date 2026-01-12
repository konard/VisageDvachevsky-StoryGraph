#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_graph_analyzer.hpp"
#include "NovelMind/editor/project_asset_tracker.hpp"
#include "NovelMind/editor/project_validators.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// ProjectIntegrityChecker Implementation
// ============================================================================

ProjectIntegrityChecker::ProjectIntegrityChecker() = default;

void ProjectIntegrityChecker::setProjectPath(const std::string& projectPath) {
  m_projectPath = projectPath;
}

void ProjectIntegrityChecker::setConfig(const IntegrityCheckConfig& config) {
  m_config = config;
}

void ProjectIntegrityChecker::addListener(IIntegrityCheckListener* listener) {
  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void ProjectIntegrityChecker::removeListener(IIntegrityCheckListener* listener) {
  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

void ProjectIntegrityChecker::reportProgress(const std::string& task, f32 progress) {
  for (auto* listener : m_listeners) {
    listener->onCheckProgress(task, progress);
  }
}

void ProjectIntegrityChecker::reportIssue(const IntegrityIssue& issue) {
  m_currentIssues.push_back(issue);
  for (auto* listener : m_listeners) {
    listener->onIssueFound(issue);
  }
}

IntegrityReport ProjectIntegrityChecker::runFullCheck() {
  if (m_projectPath.empty()) {
    IntegrityReport report;
    report.passed = false;
    IntegrityIssue issue;
    issue.severity = IssueSeverity::Critical;
    issue.category = IssueCategory::Configuration;
    issue.code = "C001";
    issue.message = "No project path specified";
    report.issues.push_back(issue);
    return report;
  }

  m_checkInProgress = true;
  m_cancelRequested = false;
  m_currentIssues.clear();

  auto startTime = std::chrono::high_resolution_clock::now();

  for (auto* listener : m_listeners) {
    listener->onCheckStarted();
  }

  // Clear collected data
  m_projectAssets.clear();
  m_referencedAssets.clear();
  m_localizationStrings.clear();

  f32 progress = 0.0f;
  const f32 progressStep = 1.0f / 8.0f;

  // Run checks based on configuration
  if (m_config.checkConfiguration && !m_cancelRequested) {
    reportProgress("Checking project configuration...", progress);
    checkProjectConfiguration(m_currentIssues);
    progress += progressStep;
  }

  if (m_config.checkScenes && !m_cancelRequested) {
    reportProgress("Checking scene references...", progress);
    checkSceneReferences(m_currentIssues);
    progress += progressStep;
  }

  if (m_config.checkAssets && !m_cancelRequested) {
    reportProgress("Scanning project assets...", progress);
    scanProjectAssets();
    reportProgress("Collecting asset references...", progress + 0.05f);
    collectAssetReferences();
    checkAssetReferences(m_currentIssues);
    if (m_config.reportUnreferencedAssets) {
      findOrphanedAssets(m_currentIssues);
    }
    progress += progressStep;
  }

  if (m_config.checkVoiceLines && !m_cancelRequested) {
    reportProgress("Checking voice lines...", progress);
    checkVoiceLines(m_currentIssues);
    progress += progressStep;
  }

  if (m_config.checkLocalization && !m_cancelRequested) {
    reportProgress("Checking localization...", progress);
    scanLocalizationFiles();
    checkLocalizationKeys(m_currentIssues);
    if (m_config.reportMissingTranslations) {
      checkMissingTranslations(m_currentIssues);
    }
    progress += progressStep;
  }

  if (m_config.checkStoryGraph && !m_cancelRequested) {
    reportProgress("Analyzing story graph...", progress);
    checkStoryGraphStructure(m_currentIssues);
    if (m_config.reportUnreachableNodes) {
      analyzeReachability(m_currentIssues);
    }
    if (m_config.reportCycles) {
      detectCycles(m_currentIssues);
    }
    checkDeadEnds(m_currentIssues);
    progress += progressStep;
  }

  if (m_config.checkScripts && !m_cancelRequested) {
    reportProgress("Checking scripts...", progress);
    checkScriptSyntax(m_currentIssues);
    progress += progressStep;
  }

  if (m_config.checkResources && !m_cancelRequested) {
    reportProgress("Checking resource conflicts...", progress);
    checkResourceConflicts(m_currentIssues);
    progress += progressStep;
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

  // Build report
  m_lastReport = IntegrityReport();
  m_lastReport.issues = std::move(m_currentIssues);
  m_lastReport.summary = calculateSummary(m_lastReport.issues);
  m_lastReport.checkTimestamp =
      static_cast<u64>(std::chrono::system_clock::now().time_since_epoch().count());
  m_lastReport.checkDurationMs = static_cast<f64>(duration.count());
  m_lastReport.passed =
      m_lastReport.summary.criticalCount == 0 && m_lastReport.summary.errorCount == 0;

  m_checkInProgress = false;

  for (auto* listener : m_listeners) {
    listener->onCheckCompleted(m_lastReport);
  }

  return m_lastReport;
}

IntegrityReport ProjectIntegrityChecker::runQuickCheck() {
  // Quick check only runs critical checks
  IntegrityCheckConfig quickConfig;
  quickConfig.checkScenes = true;
  quickConfig.checkAssets = false;
  quickConfig.checkVoiceLines = false;
  quickConfig.checkLocalization = false;
  quickConfig.checkStoryGraph = true;
  quickConfig.checkScripts = false;
  quickConfig.checkResources = false;
  quickConfig.checkConfiguration = true;
  quickConfig.reportUnreferencedAssets = false;
  quickConfig.reportUnreachableNodes = false;
  quickConfig.reportCycles = true;
  quickConfig.reportMissingTranslations = false;

  auto originalConfig = m_config;
  m_config = quickConfig;
  auto report = runFullCheck();
  m_config = originalConfig;

  return report;
}

IntegrityReport ProjectIntegrityChecker::checkCategory(IssueCategory category) {
  std::vector<IntegrityIssue> issues;

  switch (category) {
  case IssueCategory::Scene:
    checkSceneReferences(issues);
    break;
  case IssueCategory::Asset:
    scanProjectAssets();
    collectAssetReferences();
    checkAssetReferences(issues);
    findOrphanedAssets(issues);
    break;
  case IssueCategory::VoiceLine:
    checkVoiceLines(issues);
    break;
  case IssueCategory::Localization:
    scanLocalizationFiles();
    checkLocalizationKeys(issues);
    checkMissingTranslations(issues);
    break;
  case IssueCategory::StoryGraph:
    checkStoryGraphStructure(issues);
    analyzeReachability(issues);
    detectCycles(issues);
    checkDeadEnds(issues);
    break;
  case IssueCategory::Script:
    checkScriptSyntax(issues);
    break;
  case IssueCategory::Resource:
    checkResourceConflicts(issues);
    break;
  case IssueCategory::Configuration:
    checkProjectConfiguration(issues);
    break;
  }

  IntegrityReport report;
  report.issues = std::move(issues);
  report.summary = calculateSummary(report.issues);
  report.passed = report.summary.criticalCount == 0 && report.summary.errorCount == 0;

  return report;
}

std::vector<IntegrityIssue> ProjectIntegrityChecker::checkFile(const std::string& filePath) {
  std::vector<IntegrityIssue> issues;

  if (!fs::exists(filePath)) {
    IntegrityIssue issue;
    issue.severity = IssueSeverity::Error;
    issue.category = IssueCategory::Asset;
    issue.code = "A001";
    issue.message = "File does not exist";
    issue.filePath = filePath;
    issues.push_back(issue);
    return issues;
  }

  fs::path path(filePath);
  std::string ext = path.extension().string();

  // Check based on file type
  if (ext == ".nms") {
    // Script file - check syntax
    // Would use lexer/parser to validate
  } else if (ext == ".nmscene" || ext == ".json") {
    // Scene file - check references
  }

  return issues;
}

void ProjectIntegrityChecker::cancelCheck() {
  m_cancelRequested = true;
}


// ============================================================================
// Delegating Methods to Specialized Modules
// ============================================================================

void ProjectIntegrityChecker::checkProjectConfiguration(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.checkProjectConfiguration(issues);
}

void ProjectIntegrityChecker::checkSceneReferences(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.checkSceneReferences(issues);
}

void ProjectIntegrityChecker::scanProjectAssets() {
  ProjectAssetTracker tracker;
  tracker.setProjectPath(m_projectPath);
  tracker.scanProjectAssets();
  m_projectAssets = tracker.getProjectAssets();
}

void ProjectIntegrityChecker::collectAssetReferences() {
  ProjectAssetTracker tracker;
  tracker.setProjectPath(m_projectPath);
  tracker.collectAssetReferences();
  m_referencedAssets = tracker.getReferencedAssets();
}

void ProjectIntegrityChecker::checkAssetReferences(std::vector<IntegrityIssue>& issues) {
  ProjectAssetTracker tracker;
  tracker.setProjectPath(m_projectPath);
  tracker.scanProjectAssets();
  tracker.collectAssetReferences();
  tracker.checkAssetReferences(issues);
}

void ProjectIntegrityChecker::findOrphanedAssets(std::vector<IntegrityIssue>& issues) {
  ProjectAssetTracker tracker;
  tracker.setProjectPath(m_projectPath);
  tracker.scanProjectAssets();
  tracker.collectAssetReferences();
  tracker.findOrphanedAssets(issues);
}

void ProjectIntegrityChecker::checkVoiceLines(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.checkVoiceLines(issues);
}

void ProjectIntegrityChecker::scanLocalizationFiles() {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.setLocales(m_config.locales);
  validators.scanLocalizationFiles();
  m_localizationStrings = validators.getLocalizationStrings();
}

void ProjectIntegrityChecker::checkLocalizationKeys(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.scanLocalizationFiles();
  validators.checkLocalizationKeys(issues);
}

void ProjectIntegrityChecker::checkMissingTranslations(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.scanLocalizationFiles();
  validators.checkMissingTranslations(issues);
}

void ProjectIntegrityChecker::checkUnusedStrings(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.scanLocalizationFiles();
  validators.checkUnusedStrings(issues);
}

void ProjectIntegrityChecker::checkStoryGraphStructure(std::vector<IntegrityIssue>& issues) {
  ProjectGraphAnalyzer analyzer;
  analyzer.setProjectPath(m_projectPath);
  analyzer.checkStoryGraphStructure(issues);
}

void ProjectIntegrityChecker::analyzeReachability(std::vector<IntegrityIssue>& issues) {
  ProjectGraphAnalyzer analyzer;
  analyzer.setProjectPath(m_projectPath);
  analyzer.analyzeReachability(issues);
}

void ProjectIntegrityChecker::detectCycles(std::vector<IntegrityIssue>& issues) {
  ProjectGraphAnalyzer analyzer;
  analyzer.setProjectPath(m_projectPath);
  analyzer.detectCycles(issues);
}

void ProjectIntegrityChecker::checkDeadEnds(std::vector<IntegrityIssue>& issues) {
  ProjectGraphAnalyzer analyzer;
  analyzer.setProjectPath(m_projectPath);
  analyzer.checkDeadEnds(issues);
}

void ProjectIntegrityChecker::checkScriptSyntax(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.checkScriptSyntax(issues);
}

void ProjectIntegrityChecker::checkResourceConflicts(std::vector<IntegrityIssue>& issues) {
  ProjectValidators validators;
  validators.setProjectPath(m_projectPath);
  validators.checkResourceConflicts(issues);
}

IntegritySummary
ProjectIntegrityChecker::calculateSummary(const std::vector<IntegrityIssue>& issues) {
  IntegritySummary summary;
  summary.totalIssues = static_cast<i32>(issues.size());

  for (const auto& issue : issues) {
    // Count by severity
    switch (issue.severity) {
    case IssueSeverity::Info:
      summary.infoCount++;
      break;
    case IssueSeverity::Warning:
      summary.warningCount++;
      break;
    case IssueSeverity::Error:
      summary.errorCount++;
      break;
    case IssueSeverity::Critical:
      summary.criticalCount++;
      break;
    }

    // Count by category
    switch (issue.category) {
    case IssueCategory::Scene:
      summary.sceneIssues++;
      break;
    case IssueCategory::Asset:
      summary.assetIssues++;
      break;
    case IssueCategory::VoiceLine:
      summary.voiceIssues++;
      break;
    case IssueCategory::Localization:
      summary.localizationIssues++;
      break;
    case IssueCategory::StoryGraph:
      summary.graphIssues++;
      break;
    case IssueCategory::Script:
      summary.scriptIssues++;
      break;
    case IssueCategory::Resource:
      summary.resourceIssues++;
      break;
    case IssueCategory::Configuration:
      summary.configIssues++;
      break;
    }
  }

  // Set asset statistics
  summary.totalAssets = static_cast<i32>(m_projectAssets.size());
  summary.referencedAssets = static_cast<i32>(m_referencedAssets.size());
  summary.unreferencedAssets = summary.totalAssets - summary.referencedAssets;

  return summary;
}

bool ProjectIntegrityChecker::shouldExclude(const std::string& path) const {
  for (const auto& pattern : m_config.excludePatterns) {
    if (path.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}


} // namespace NovelMind::editor
