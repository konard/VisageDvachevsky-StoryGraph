#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <queue>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace NovelMind::editor {

namespace {
bool readFileToString(const fs::path& path, std::string& out) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}
} // namespace

// ============================================================================
// IntegrityReport Implementation
// ============================================================================

std::vector<IntegrityIssue> IntegrityReport::getIssuesBySeverity(IssueSeverity severity) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [severity](const IntegrityIssue& issue) { return issue.severity == severity; });
  return result;
}

std::vector<IntegrityIssue> IntegrityReport::getIssuesByCategory(IssueCategory category) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [category](const IntegrityIssue& issue) { return issue.category == category; });
  return result;
}

std::vector<IntegrityIssue> IntegrityReport::getIssuesByFile(const std::string& filePath) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [&filePath](const IntegrityIssue& issue) { return issue.filePath == filePath; });
  return result;
}

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
// Check Functions
// ============================================================================

void ProjectIntegrityChecker::checkProjectConfiguration(std::vector<IntegrityIssue>& issues) {
  fs::path projectFile = fs::path(m_projectPath) / "project.json";

  if (!fs::exists(projectFile)) {
    IntegrityIssue issue;
    issue.severity = IssueSeverity::Critical;
    issue.category = IssueCategory::Configuration;
    issue.code = "C001";
    issue.message = "Project configuration file not found";
    issue.filePath = projectFile.string();
    issue.hasQuickFix = true;
    issue.quickFixDescription = "Create default project.json";
    issues.push_back(issue);
    return;
  }

  // Check version compatibility
  auto& pm = ProjectManager::instance();
  if (pm.hasOpenProject()) {
    const auto& metadata = pm.getMetadata();
    std::string projectVersion = metadata.engineVersion;
    std::string currentVersion = "0.2.0"; // Should be from a constant

    if (!projectVersion.empty() && projectVersion != currentVersion) {
      // Simple version comparison - could be more sophisticated
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Warning;
      issue.category = IssueCategory::Configuration;
      issue.code = "C004";
      issue.message = "Project was created with engine version " + projectVersion +
                      " (current: " + currentVersion + ")";
      issue.filePath = projectFile.string();
      issue.suggestions.push_back("Update project to current engine version");
      issue.suggestions.push_back("Some features may not work as expected");
      issues.push_back(issue);
    }
  }

  // Check for required folders
  std::vector<std::pair<std::string, std::string>> requiredFolders = {
      {"Assets", "Assets folder"}, {"Scripts", "Scripts folder"}, {"Scenes", "Scenes folder"}};

  for (const auto& [folder, name] : requiredFolders) {
    fs::path folderPath = fs::path(m_projectPath) / folder;
    if (!fs::exists(folderPath)) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Warning;
      issue.category = IssueCategory::Configuration;
      issue.code = "C002";
      issue.message = name + " is missing";
      issue.filePath = folderPath.string();
      issue.hasQuickFix = true;
      issue.quickFixDescription = "Create " + folder + " directory";
      issues.push_back(issue);
    }
  }

  // Check for start scene/entry point
  if (pm.hasOpenProject()) {
    std::string startScene = pm.getStartScene();
    if (startScene.empty()) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Error;
      issue.category = IssueCategory::Configuration;
      issue.code = "C003";
      issue.message = "No start scene defined";
      issue.suggestions.push_back("Set a start scene in Project Settings");
      issue.hasQuickFix = true;
      issue.quickFixDescription = "Set first scene as start scene";
      issues.push_back(issue);
    } else {
      // Check if start scene exists
      fs::path sceneFile = fs::path(m_projectPath) / "Scenes" / (startScene + ".nmscene");
      if (!fs::exists(sceneFile)) {
        IntegrityIssue issue;
        issue.severity = IssueSeverity::Error;
        issue.category = IssueCategory::Scene;
        issue.code = "S001";
        issue.message = "Start scene '" + startScene + "' not found";
        issue.filePath = sceneFile.string();
        issue.hasQuickFix = true;
        issue.quickFixDescription = "Create scene file";
        issues.push_back(issue);
      }
    }
  }
}

void ProjectIntegrityChecker::checkSceneReferences(std::vector<IntegrityIssue>& issues) {
  fs::path scenesDir = fs::path(m_projectPath) / "Scenes";
  if (!fs::exists(scenesDir)) {
    return;
  }

  // Collect all scene IDs and check for serialization errors
  std::unordered_set<std::string> sceneIds;
  for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
    if (entry.path().extension() == ".nmscene") {
      std::string sceneId = entry.path().stem().string();
      sceneIds.insert(sceneId);

      // Check if scene file can be read (basic serialization check)
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        IntegrityIssue issue;
        issue.severity = IssueSeverity::Error;
        issue.category = IssueCategory::Scene;
        issue.code = "S003";
        issue.message = "Failed to read scene file: " + sceneId;
        issue.filePath = entry.path().string();
        issue.suggestions.push_back("Check file permissions");
        issue.suggestions.push_back("File may be corrupted");
        issues.push_back(issue);
        continue;
      }

      // Basic JSON validation - check for balanced braces
      int braceCount = 0;
      for (char c : content) {
        if (c == '{')
          braceCount++;
        else if (c == '}')
          braceCount--;
      }
      if (braceCount != 0) {
        IntegrityIssue issue;
        issue.severity = IssueSeverity::Error;
        issue.category = IssueCategory::Scene;
        issue.code = "S004";
        issue.message = "Scene file has malformed JSON: " + sceneId;
        issue.filePath = entry.path().string();
        issue.context = "Unbalanced braces detected";
        issue.suggestions.push_back("Check JSON syntax");
        issue.suggestions.push_back("Restore from backup if corrupted");
        issues.push_back(issue);
      }
    }
  }

  // Check script files for scene references
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (fs::exists(scriptsDir)) {
    std::regex sceneRefPattern(R"(goto\s+(\w+)|scene\s+(\w+))");

    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.path().extension() == ".nms") {
        std::ifstream file(entry.path());
        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
          lineNum++;
          std::smatch match;
          std::string::const_iterator searchStart(line.cbegin());

          while (std::regex_search(searchStart, line.cend(), match, sceneRefPattern)) {
            std::string sceneRef = match[1].matched ? match[1].str() : match[2].str();

            if (!sceneRef.empty() && sceneIds.find(sceneRef) == sceneIds.end()) {
              IntegrityIssue issue;
              issue.severity = IssueSeverity::Error;
              issue.category = IssueCategory::Scene;
              issue.code = "S002";
              issue.message = "Reference to undefined scene: " + sceneRef;
              issue.filePath = entry.path().string();
              issue.lineNumber = lineNum;
              issue.context = line;
              issue.suggestions.push_back("Create scene '" + sceneRef + "'");
              issue.suggestions.push_back("Fix the scene reference");
              issue.hasQuickFix = true;
              issue.quickFixDescription = "Comment out reference to missing scene";
              issues.push_back(issue);
            }

            searchStart = match.suffix().first;
          }
        }
      }
    }
  }
}

void ProjectIntegrityChecker::scanProjectAssets() {
  m_projectAssets.clear();

  fs::path assetsDir = fs::path(m_projectPath) / "Assets";
  if (!fs::exists(assetsDir)) {
    return;
  }

  std::vector<std::string> assetExtensions = {".png",  ".jpg", ".jpeg",    ".gif", ".bmp",
                                              ".webp", ".ogg", ".wav",     ".mp3", ".flac",
                                              ".ttf",  ".otf", ".nmscript"};

  for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (std::find(assetExtensions.begin(), assetExtensions.end(), ext) != assetExtensions.end()) {
        m_projectAssets.insert(fs::relative(entry.path(), m_projectPath).string());
      }
    }
  }
}

void ProjectIntegrityChecker::collectAssetReferences() {
  m_referencedAssets.clear();

  // Scan scene files for asset references
  fs::path scenesDir = fs::path(m_projectPath) / "Scenes";
  if (fs::exists(scenesDir)) {
    std::regex assetRefPattern(R"(\"(?:textureId|imageId|audioId|fontId)\":\s*\"([^\"]+)\")");

    for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
      if (entry.path().extension() == ".nmscene" || entry.path().extension() == ".json") {
        std::string content;
        if (!readFileToString(entry.path(), content)) {
          continue;
        }

        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());

        while (std::regex_search(searchStart, content.cend(), match, assetRefPattern)) {
          m_referencedAssets.insert(match[1].str());
          searchStart = match.suffix().first;
        }
      }
    }
  }

  // Scan script files for asset references
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (fs::exists(scriptsDir)) {
    std::regex assetRefPattern(R"(show\s+(?:background|character)\s+\"([^\"]+)\")");

    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.path().extension() == ".nms") {
        std::string content;
        if (!readFileToString(entry.path(), content)) {
          continue;
        }

        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());

        while (std::regex_search(searchStart, content.cend(), match, assetRefPattern)) {
          m_referencedAssets.insert(match[1].str());
          searchStart = match.suffix().first;
        }
      }
    }
  }
}

void ProjectIntegrityChecker::checkAssetReferences(std::vector<IntegrityIssue>& issues) {
  for (const auto& ref : m_referencedAssets) {
    // Check if referenced asset exists in project
    bool found = false;
    for (const auto& asset : m_projectAssets) {
      if (asset.find(ref) != std::string::npos || fs::path(asset).filename().string() == ref) {
        found = true;
        break;
      }
    }

    if (!found) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Error;
      issue.category = IssueCategory::Asset;
      issue.code = "A002";
      issue.message = "Referenced asset not found: " + ref;
      issue.hasQuickFix = true;
      issue.quickFixDescription = "Create placeholder asset";
      issues.push_back(issue);
    }
  }
}

void ProjectIntegrityChecker::findOrphanedAssets(std::vector<IntegrityIssue>& issues) {
  for (const auto& asset : m_projectAssets) {
    std::string filename = fs::path(asset).filename().string();

    bool isReferenced = false;
    for (const auto& ref : m_referencedAssets) {
      if (asset.find(ref) != std::string::npos || filename == ref) {
        isReferenced = true;
        break;
      }
    }

    if (!isReferenced) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Info;
      issue.category = IssueCategory::Asset;
      issue.code = "A003";
      issue.message = "Asset is not referenced: " + asset;
      issue.filePath = (fs::path(m_projectPath) / asset).string();
      issue.suggestions.push_back("Remove unused asset to reduce build size");
      issue.hasQuickFix = true;
      issue.quickFixDescription = "Remove unused asset file";
      issues.push_back(issue);
    }
  }
}

void ProjectIntegrityChecker::checkVoiceLines(std::vector<IntegrityIssue>& issues) {
  fs::path voiceDir = fs::path(m_projectPath) / "Assets" / "Voice";
  if (!fs::exists(voiceDir)) {
    // Voice directory doesn't exist - that's okay if voice isn't used
    return;
  }

  // Collect voice files
  std::unordered_set<std::string> voiceFiles;
  for (const auto& entry : fs::recursive_directory_iterator(voiceDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
        voiceFiles.insert(entry.path().stem().string());
      }
    }
  }

  // Check script files for voice references
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (fs::exists(scriptsDir)) {
    std::regex voiceRefPattern(R"(voice\s+\"([^\"]+)\")");

    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.path().extension() == ".nms") {
        std::ifstream file(entry.path());
        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
          lineNum++;
          std::smatch match;

          if (std::regex_search(line, match, voiceRefPattern)) {
            std::string voiceRef = match[1].str();
            if (voiceFiles.find(voiceRef) == voiceFiles.end()) {
              IntegrityIssue issue;
              issue.severity = IssueSeverity::Warning;
              issue.category = IssueCategory::VoiceLine;
              issue.code = "V001";
              issue.message = "Voice file not found: " + voiceRef;
              issue.filePath = entry.path().string();
              issue.lineNumber = lineNum;
              issue.context = line;
              issues.push_back(issue);
            }
          }
        }
      }
    }
  }
}

void ProjectIntegrityChecker::scanLocalizationFiles() {
  m_localizationStrings.clear();

  fs::path locDir = fs::path(m_projectPath) / "Localization";
  if (!fs::exists(locDir)) {
    return;
  }

  std::regex keyPattern(R"(\"([^\"]+)\":\s*\"[^\"]*\")");

  for (const auto& entry : fs::directory_iterator(locDir)) {
    if (entry.path().extension() == ".json") {
      std::string locale = entry.path().stem().string();
      std::vector<std::string> keys;

      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch match;
      std::string::const_iterator searchStart(content.cbegin());

      while (std::regex_search(searchStart, content.cend(), match, keyPattern)) {
        keys.push_back(match[1].str());
        searchStart = match.suffix().first;
      }

      m_localizationStrings[locale] = std::move(keys);
    }
  }
}

void ProjectIntegrityChecker::checkLocalizationKeys(std::vector<IntegrityIssue>& issues) {
  if (m_localizationStrings.empty()) {
    return;
  }

  // Get all unique keys across all locales
  std::unordered_set<std::string> allKeys;
  for (const auto& [locale, keys] : m_localizationStrings) {
    for (const auto& key : keys) {
      allKeys.insert(key);
    }
  }

  // Check for duplicate keys within each locale
  for (const auto& [locale, keys] : m_localizationStrings) {
    std::unordered_set<std::string> seen;
    for (const auto& key : keys) {
      if (seen.count(key) > 0) {
        IntegrityIssue issue;
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Localization;
        issue.code = "L001";
        issue.message = "Duplicate localization key in " + locale + ": " + key;
        issue.filePath = (fs::path(m_projectPath) / "Localization" / (locale + ".json")).string();
        issues.push_back(issue);
      }
      seen.insert(key);
    }
  }
}

void ProjectIntegrityChecker::checkMissingTranslations(std::vector<IntegrityIssue>& issues) {
  if (m_localizationStrings.size() < 2) {
    return; // Need at least 2 locales to compare
  }

  // Get reference locale (usually the default)
  std::string refLocale = "en";
  if (m_localizationStrings.find(refLocale) == m_localizationStrings.end()) {
    refLocale = m_localizationStrings.begin()->first;
  }

  const auto& refKeys = m_localizationStrings[refLocale];
  std::unordered_set<std::string> refKeySet(refKeys.begin(), refKeys.end());

  for (const auto& [locale, keys] : m_localizationStrings) {
    if (locale == refLocale)
      continue;

    std::unordered_set<std::string> localeKeySet(keys.begin(), keys.end());

    // Check for missing translations
    for (const auto& key : refKeySet) {
      if (localeKeySet.find(key) == localeKeySet.end()) {
        IntegrityIssue issue;
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Localization;
        issue.code = "L002";
        issue.message = "Missing translation for '" + key + "' in " + locale;
        issue.filePath = (fs::path(m_projectPath) / "Localization" / (locale + ".json")).string();
        issue.hasQuickFix = true;
        issue.quickFixDescription = "Add key with empty value";
        issues.push_back(issue);
      }
    }
  }
}

void ProjectIntegrityChecker::checkUnusedStrings(std::vector<IntegrityIssue>& /*issues*/) {
  // Would scan scripts for string usage and compare to localization files
  // Complex implementation - placeholder for now
}

void ProjectIntegrityChecker::checkStoryGraphStructure(std::vector<IntegrityIssue>& issues) {
  // Check for entry nodes
  // Would parse story graph files and validate structure
  // Placeholder - would require loading IR/graph data

  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return;
  }

  bool hasEntryPoint = false;

  for (const auto& entry : fs::directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::ifstream file(entry.path());
      if (!file) {
        continue;
      }
      std::ostringstream buffer;
      buffer << file.rdbuf();
      std::string content = buffer.str();

      // Look for scene definitions
      if (content.find("scene main") != std::string::npos ||
          content.find("scene start") != std::string::npos) {
        hasEntryPoint = true;
        break;
      }
    }
  }

  if (!hasEntryPoint) {
    IntegrityIssue issue;
    issue.severity = IssueSeverity::Error;
    issue.category = IssueCategory::StoryGraph;
    issue.code = "G001";
    issue.message = "No entry point scene found (main or start)";
    issue.suggestions.push_back("Create a 'main' scene as entry point");
    issue.hasQuickFix = true;
    issue.quickFixDescription = "Create main scene";
    issues.push_back(issue);
  }
}

void ProjectIntegrityChecker::analyzeReachability(std::vector<IntegrityIssue>& /*issues*/) {
  // Would perform graph traversal to find unreachable nodes
  // Complex implementation - placeholder for now
}

void ProjectIntegrityChecker::detectCycles(std::vector<IntegrityIssue>& /*issues*/) {
  // Would use DFS with coloring to detect cycles
  // Complex implementation - placeholder for now
}

void ProjectIntegrityChecker::checkDeadEnds(std::vector<IntegrityIssue>& issues) {
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return;
  }

  std::regex scenePattern(R"(scene\s+(\w+)\s*\{)");
  std::regex endPattern(R"(\b(end|goto|choice)\b)");

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch sceneMatch;
      std::string::const_iterator searchStart(content.cbegin());

      while (std::regex_search(searchStart, content.cend(), sceneMatch, scenePattern)) {
        std::string sceneName = sceneMatch[1].str();

        // Find the scene content (simplified - real implementation would parse
        // braces)
        size_t braceStart =
            content.find('{', static_cast<std::string::size_type>(sceneMatch.position()));
        if (braceStart == std::string::npos) {
          searchStart = sceneMatch.suffix().first;
          continue;
        }

        // Find matching closing brace (simplified)
        size_t braceEnd = content.find('}', braceStart);
        if (braceEnd == std::string::npos) {
          searchStart = sceneMatch.suffix().first;
          continue;
        }

        std::string sceneContent = content.substr(braceStart, braceEnd - braceStart);

        // Check if scene has an exit (end, goto, or choice)
        if (!std::regex_search(sceneContent, endPattern)) {
          IntegrityIssue issue;
          issue.severity = IssueSeverity::Warning;
          issue.category = IssueCategory::StoryGraph;
          issue.code = "G002";
          issue.message = "Scene '" + sceneName + "' may be a dead end (no goto, choice, or end)";
          issue.filePath = entry.path().string();
          issue.suggestions.push_back("Add 'end' to end the story");
          issue.suggestions.push_back("Add 'goto' to continue to another scene");
          issues.push_back(issue);
        }

        searchStart = sceneMatch.suffix().first;
      }
    }
  }
}

void ProjectIntegrityChecker::checkScriptSyntax(std::vector<IntegrityIssue>& /*issues*/) {
  // Would use the lexer/parser to validate syntax
  // Complex implementation - placeholder for now
}

void ProjectIntegrityChecker::checkResourceConflicts(std::vector<IntegrityIssue>& issues) {
  // Check for duplicate asset IDs
  std::unordered_map<std::string, std::vector<std::string>> assetsByName;

  fs::path assetsDir = fs::path(m_projectPath) / "Assets";
  if (!fs::exists(assetsDir)) {
    return;
  }

  for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
    if (entry.is_regular_file()) {
      std::string filename = entry.path().filename().string();
      assetsByName[filename].push_back(entry.path().string());
    }
  }

  for (const auto& [name, paths] : assetsByName) {
    if (paths.size() > 1) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Warning;
      issue.category = IssueCategory::Resource;
      issue.code = "R001";
      issue.message = "Duplicate asset name: " + name;
      issue.context = "Found in " + std::to_string(paths.size()) + " locations";
      for (const auto& path : paths) {
        issue.suggestions.push_back("  - " + path);
      }
      issues.push_back(issue);
    }
  }
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

Result<void> ProjectIntegrityChecker::applyQuickFix(const IntegrityIssue& issue) {
  if (!issue.hasQuickFix) {
    return Result<void>::error("No quick fix available for this issue");
  }

  // Dispatch to appropriate quick fix based on issue code
  if (issue.code == "C001") {
    // Create project.json configuration file
    fs::path projectDir(m_projectPath);
    std::string projectName = projectDir.filename().string();
    return QuickFixes::createDefaultProjectConfig(m_projectPath, projectName);
  }

  if (issue.code == "C002") {
    // Create missing directory
    if (!issue.filePath.empty()) {
      std::error_code ec;
      fs::create_directories(issue.filePath, ec);
      if (ec) {
        return Result<void>::error("Failed to create directory: " + ec.message());
      }
      return Result<void>::ok();
    }
    return Result<void>::error("No file path specified for directory creation");
  }

  if (issue.code == "C003") {
    // No start scene defined - set first available scene as start
    return QuickFixes::setFirstSceneAsStart(m_projectPath);
  }

  if (issue.code == "C004") {
    // Version mismatch - manual intervention required (no automatic fix)
    return Result<void>::error("Version mismatch requires manual project migration");
  }

  if (issue.code == "S001") {
    // Start scene file not found - create an empty scene file
    // Extract scene name from message or use the file path
    std::string sceneId;
    if (!issue.filePath.empty()) {
      fs::path scenePath(issue.filePath);
      sceneId = scenePath.stem().string();
    }
    if (sceneId.empty()) {
      return Result<void>::error("Could not determine scene ID from issue");
    }
    return QuickFixes::createEmptyScene(m_projectPath, sceneId);
  }

  if (issue.code == "S002") {
    // Reference to undefined scene - remove the reference
    // Extract scene ID from message: "Reference to undefined scene: <sceneId>"
    std::string sceneId;
    const std::string prefix = "Reference to undefined scene: ";
    size_t pos = issue.message.find(prefix);
    if (pos != std::string::npos) {
      sceneId = issue.message.substr(pos + prefix.length());
    }
    if (!sceneId.empty()) {
      return QuickFixes::removeMissingSceneReference(m_projectPath, sceneId);
    }
    return Result<void>::error("Could not extract scene ID from issue message");
  }

  if (issue.code == "S003" || issue.code == "S004") {
    // Scene file read error or malformed JSON - requires manual fix
    return Result<void>::error("Scene file corruption requires manual restoration from backup");
  }

  if (issue.code == "A001") {
    // File does not exist - create placeholder
    if (!issue.filePath.empty()) {
      return QuickFixes::createPlaceholderAsset(m_projectPath, issue.filePath);
    }
    return Result<void>::error("No file path specified for asset creation");
  }

  if (issue.code == "A002") {
    // Referenced asset not found - create placeholder asset
    // Extract asset name from message: "Referenced asset not found: <assetName>"
    std::string assetName;
    const std::string prefix = "Referenced asset not found: ";
    size_t pos = issue.message.find(prefix);
    if (pos != std::string::npos) {
      assetName = issue.message.substr(pos + prefix.length());
    }
    if (!assetName.empty()) {
      // Create in Assets directory with the referenced name
      std::string assetPath = "Assets/" + assetName;
      return QuickFixes::createPlaceholderAsset(m_projectPath, assetPath);
    }
    return Result<void>::error("Could not extract asset name from issue");
  }

  if (issue.code == "A003") {
    // Asset is not referenced (orphaned) - remove the asset file
    if (!issue.filePath.empty()) {
      return QuickFixes::removeOrphanedAsset(m_projectPath, issue.filePath);
    }
    return Result<void>::error("No file path specified for asset removal");
  }

  if (issue.code == "V001") {
    // Voice file not found - no automatic fix (requires audio file)
    return Result<void>::error("Missing voice files must be recorded or imported manually");
  }

  if (issue.code == "L001") {
    // Duplicate localization key - requires manual resolution
    return Result<void>::error("Duplicate localization keys require manual resolution");
  }

  if (issue.code == "L002") {
    // Missing translation - add missing localization key
    std::string key;
    std::string locale = "en";

    // Extract key from message: "Missing translation for '<key>' in <locale>"
    size_t startQuote = issue.message.find('\'');
    if (startQuote != std::string::npos) {
      size_t endQuote = issue.message.find('\'', startQuote + 1);
      if (endQuote != std::string::npos) {
        key = issue.message.substr(startQuote + 1, endQuote - startQuote - 1);
      }
    }

    // Extract locale from message
    size_t inPos = issue.message.find(" in ");
    if (inPos != std::string::npos) {
      locale = issue.message.substr(inPos + 4);
    }

    if (!key.empty()) {
      return QuickFixes::addMissingLocalizationKey(m_projectPath, key, locale);
    }
    return Result<void>::error("Could not extract localization key from issue message");
  }

  if (issue.code == "G001") {
    // No entry point scene found - create main scene
    return QuickFixes::createMainEntryScene(m_projectPath);
  }

  if (issue.code == "G002") {
    // Dead end scene - requires manual story flow editing
    return Result<void>::error("Dead end scenes require manual addition of goto, choice, or end");
  }

  if (issue.code == "R001") {
    // Duplicate asset name - requires manual resolution
    return Result<void>::error("Duplicate asset names require manual renaming or removal");
  }

  return Result<void>::error("Quick fix not implemented for issue: " + issue.code);
}

// ============================================================================
// QuickFixes Implementation
// ============================================================================

namespace QuickFixes {

Result<void> removeMissingSceneReference(const std::string& projectPath,
                                         const std::string& sceneId) {
  // Scan script files and remove/comment out references to the missing scene
  fs::path scriptsDir = fs::path(projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return Result<void>::ok(); // No scripts to fix
  }

  std::regex sceneRefPattern("(goto\\s+" + sceneId + "|scene\\s+" + sceneId + ")");
  bool anyChanges = false;

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::string modifiedContent =
          std::regex_replace(content, sceneRefPattern, "// [REMOVED: $1] - scene not found");

      if (modifiedContent != content) {
        std::ofstream outFile(entry.path());
        if (outFile.is_open()) {
          outFile << modifiedContent;
          outFile.close();
          anyChanges = true;
        }
      }
    }
  }

  if (anyChanges) {
    return Result<void>::ok();
  }
  return Result<void>::error("No references found to remove for scene: " + sceneId);
}

Result<void> createPlaceholderAsset(const std::string& projectPath, const std::string& assetPath) {
  fs::path fullPath;

  // Check if assetPath is already absolute or relative to project
  if (fs::path(assetPath).is_absolute()) {
    fullPath = assetPath;
  } else {
    fullPath = fs::path(projectPath) / assetPath;
  }

  // Create parent directories
  std::error_code ec;
  fs::create_directories(fullPath.parent_path(), ec);
  if (ec) {
    return Result<void>::error("Failed to create directory: " + ec.message());
  }

  // Determine file type and create appropriate placeholder
  std::string ext = fullPath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  std::ofstream file(fullPath, std::ios::binary);
  if (!file.is_open()) {
    return Result<void>::error("Failed to create placeholder file: " + fullPath.string());
  }

  if (ext == ".png") {
    // Minimal valid PNG (1x1 transparent pixel)
    const unsigned char pngData[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
        0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
        0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
    file.write(reinterpret_cast<const char*>(pngData), sizeof(pngData));
  } else if (ext == ".json") {
    file << "{\n}\n";
  } else if (ext == ".nms") {
    file << "// Placeholder script\nscene placeholder {\n  // Add content "
            "here\n}\n";
  } else {
    // Generic placeholder
    file << "PLACEHOLDER";
  }
  file.close();

  return Result<void>::ok();
}

Result<void> addMissingLocalizationKey(const std::string& projectPath, const std::string& key,
                                       const std::string& locale) {
  fs::path locFile = fs::path(projectPath) / "Localization" / (locale + ".json");

  // Create Localization directory if it doesn't exist
  std::error_code ec;
  fs::create_directories(locFile.parent_path(), ec);

  if (!fs::exists(locFile)) {
    // Create the file with the new key
    std::ofstream outFile(locFile);
    if (!outFile.is_open()) {
      return Result<void>::error("Failed to create localization file: " + locFile.string());
    }
    outFile << "{\n  \"" << key << "\": \"\"\n}\n";
    outFile.close();
    return Result<void>::ok();
  }

  // Read existing content
  std::string content;
  if (!readFileToString(locFile, content)) {
    return Result<void>::error("Failed to read localization file");
  }

  // Find last key-value pair and add new one
  size_t lastBrace = content.rfind('}');
  if (lastBrace != std::string::npos) {
    // Check if there are existing entries
    size_t lastQuote = content.rfind('"', lastBrace);
    std::string separator = lastQuote != std::string::npos ? ",\n" : "\n";

    std::string newEntry = separator + "  \"" + key + "\": \"\"";
    content.insert(lastBrace, newEntry);

    std::ofstream outFile(locFile);
    if (!outFile.is_open()) {
      return Result<void>::error("Failed to write localization file");
    }
    outFile << content;
    outFile.close();
  }

  return Result<void>::ok();
}

Result<void> removeOrphanedReferences(const std::string& /*projectPath*/,
                                      const std::vector<std::string>& /*assetPaths*/) {
  // Would scan files and remove references
  return Result<void>::ok();
}

Result<void> connectUnreachableNode(const std::string& /*projectPath*/,
                                    scripting::NodeId /*nodeId*/) {
  // Would modify graph structure
  return Result<void>::ok();
}

Result<void> resolveDuplicateId(const std::string& /*projectPath*/,
                                const std::string& /*duplicateId*/) {
  // Would rename duplicate IDs
  return Result<void>::ok();
}

Result<void> createEmptyScene(const std::string& projectPath, const std::string& sceneId) {
  // Create Scenes directory if it doesn't exist
  fs::path scenesDir = fs::path(projectPath) / "Scenes";
  std::error_code ec;
  fs::create_directories(scenesDir, ec);
  if (ec) {
    return Result<void>::error("Failed to create Scenes directory: " + ec.message());
  }

  fs::path sceneFile = scenesDir / (sceneId + ".nmscene");

  // Check if file already exists
  if (fs::exists(sceneFile)) {
    return Result<void>::error("Scene file already exists: " + sceneFile.string());
  }

  // Create empty scene JSON
  std::ofstream file(sceneFile);
  if (!file.is_open()) {
    return Result<void>::error("Failed to create scene file: " + sceneFile.string());
  }

  // Write minimal valid scene document
  file << "{\n";
  file << "  \"sceneId\": \"" << sceneId << "\",\n";
  file << "  \"objects\": []\n";
  file << "}\n";
  file.close();

  return Result<void>::ok();
}

Result<void> setFirstSceneAsStart(const std::string& projectPath) {
  // Find first available scene
  fs::path scenesDir = fs::path(projectPath) / "Scenes";
  if (!fs::exists(scenesDir)) {
    return Result<void>::error("Scenes directory not found");
  }

  std::string firstSceneId;
  for (const auto& entry : fs::directory_iterator(scenesDir)) {
    if (entry.path().extension() == ".nmscene") {
      firstSceneId = entry.path().stem().string();
      break;
    }
  }

  if (firstSceneId.empty()) {
    return Result<void>::error("No scenes found in project");
  }

  // Update project.json with the start scene
  fs::path projectFile = fs::path(projectPath) / "project.json";
  if (!fs::exists(projectFile)) {
    return Result<void>::error("project.json not found");
  }

  std::string content;
  if (!readFileToString(projectFile, content)) {
    return Result<void>::error("Failed to read project.json");
  }

  // Find and update startScene field, or add it
  std::regex startScenePattern(R"("startScene"\s*:\s*"[^"]*")");
  std::string replacement = "\"startScene\": \"" + firstSceneId + "\"";

  if (std::regex_search(content, startScenePattern)) {
    content = std::regex_replace(content, startScenePattern, replacement);
  } else {
    // Add startScene before the last closing brace
    size_t lastBrace = content.rfind('}');
    if (lastBrace != std::string::npos) {
      size_t lastQuote = content.rfind('"', lastBrace);
      std::string separator = lastQuote != std::string::npos ? ",\n" : "\n";
      content.insert(lastBrace, separator + "  " + replacement);
    }
  }

  std::ofstream outFile(projectFile);
  if (!outFile.is_open()) {
    return Result<void>::error("Failed to write project.json");
  }
  outFile << content;
  outFile.close();

  return Result<void>::ok();
}

Result<void> createMainEntryScene(const std::string& projectPath) {
  // First, create the main scene file
  auto result = createEmptyScene(projectPath, "main");
  if (!result.isOk()) {
    return result;
  }

  // Also create a corresponding main.nms script file
  fs::path scriptsDir = fs::path(projectPath) / "Scripts";
  std::error_code ec;
  fs::create_directories(scriptsDir, ec);

  fs::path scriptFile = scriptsDir / "main.nms";
  if (!fs::exists(scriptFile)) {
    std::ofstream file(scriptFile);
    if (file.is_open()) {
      file << "// Main entry point script\n";
      file << "scene main {\n";
      file << "  // Add your story content here\n";
      file << "  say \"Welcome to the story!\"\n";
      file << "  end\n";
      file << "}\n";
      file.close();
    }
  }

  // Try to set main as the start scene
  fs::path projectFile = fs::path(projectPath) / "project.json";
  if (fs::exists(projectFile)) {
    std::string content;
    if (readFileToString(projectFile, content)) {
      std::regex startScenePattern(R"("startScene"\s*:\s*"[^"]*")");
      std::string replacement = "\"startScene\": \"main\"";

      if (std::regex_search(content, startScenePattern)) {
        content = std::regex_replace(content, startScenePattern, replacement);
      } else {
        size_t lastBrace = content.rfind('}');
        if (lastBrace != std::string::npos) {
          size_t lastQuote = content.rfind('"', lastBrace);
          std::string separator = lastQuote != std::string::npos ? ",\n" : "\n";
          content.insert(lastBrace, separator + "  " + replacement);
        }
      }

      std::ofstream outFile(projectFile);
      if (outFile.is_open()) {
        outFile << content;
        outFile.close();
      }
    }
  }

  return Result<void>::ok();
}

Result<void> removeOrphanedAsset(const std::string& projectPath, const std::string& assetPath) {
  fs::path fullPath;

  // Handle both absolute and relative paths
  if (fs::path(assetPath).is_absolute()) {
    fullPath = assetPath;
  } else {
    fullPath = fs::path(projectPath) / assetPath;
  }

  if (!fs::exists(fullPath)) {
    return Result<void>::error("Asset file not found: " + fullPath.string());
  }

  std::error_code ec;
  fs::remove(fullPath, ec);
  if (ec) {
    return Result<void>::error("Failed to remove asset: " + ec.message());
  }

  return Result<void>::ok();
}

Result<void> createDefaultProjectConfig(const std::string& projectPath,
                                        const std::string& projectName) {
  fs::path projectFile = fs::path(projectPath) / "project.json";

  // Don't overwrite existing config
  if (fs::exists(projectFile)) {
    return Result<void>::error("project.json already exists");
  }

  std::ofstream file(projectFile);
  if (!file.is_open()) {
    return Result<void>::error("Failed to create project.json");
  }

  // Get current timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  file << "{\n";
  file << "  \"name\": \"" << projectName << "\",\n";
  file << "  \"version\": \"1.0.0\",\n";
  file << "  \"author\": \"\",\n";
  file << "  \"description\": \"\",\n";
  file << "  \"engineVersion\": \"0.2.0\",\n";
  file << "  \"startScene\": \"\",\n";
  file << "  \"createdAt\": " << timestamp << ",\n";
  file << "  \"modifiedAt\": " << timestamp << ",\n";
  file << "  \"defaultLocale\": \"en\",\n";
  file << "  \"targetResolution\": \"1920x1080\",\n";
  file << "  \"fullscreenDefault\": false,\n";
  file << "  \"buildPreset\": \"release\",\n";
  file << "  \"targetPlatforms\": [\"windows\", \"linux\", \"macos\"],\n";
  file << "  \"playbackSourceMode\": \"Script\"\n";
  file << "}\n";
  file.close();

  return Result<void>::ok();
}

} // namespace QuickFixes

} // namespace NovelMind::editor
