#include "NovelMind/editor/project_validators.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
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

ProjectValidators::ProjectValidators() = default;

void ProjectValidators::setProjectPath(const std::string& projectPath) {
  m_projectPath = projectPath;
}

void ProjectValidators::setLocales(const std::vector<std::string>& locales) {
  m_locales = locales;
}

void ProjectValidators::checkProjectConfiguration(std::vector<IntegrityIssue>& issues) {
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

void ProjectValidators::checkSceneReferences(std::vector<IntegrityIssue>& issues) {
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

void ProjectValidators::checkVoiceLines(std::vector<IntegrityIssue>& issues) {
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

void ProjectValidators::scanLocalizationFiles() {
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

void ProjectValidators::checkLocalizationKeys(std::vector<IntegrityIssue>& issues) {
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

void ProjectValidators::checkMissingTranslations(std::vector<IntegrityIssue>& issues) {
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

void ProjectValidators::checkUnusedStrings(std::vector<IntegrityIssue>& issues) {
  // Scan scripts for string usage and compare to localization files
  if (m_localizationStrings.empty()) {
    return; // No localization strings to check
  }

  // Collect all localization keys from all locales
  std::unordered_set<std::string> allKeys;
  for (const auto& [locale, keys] : m_localizationStrings) {
    for (const auto& key : keys) {
      allKeys.insert(key);
    }
  }

  // Collect referenced keys from scripts
  std::unordered_set<std::string> referencedKeys;
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";

  if (fs::exists(scriptsDir)) {
    // Pattern to match loc("key") or loc('key')
    std::regex locPattern(R"(loc\s*\(\s*[\"']([^\"']+)[\"']\s*\))");

    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.path().extension() == ".nms") {
        std::string content;
        if (!readFileToString(entry.path(), content)) {
          continue;
        }

        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());

        while (std::regex_search(searchStart, content.cend(), match, locPattern)) {
          referencedKeys.insert(match[1].str());
          searchStart = match.suffix().first;
        }
      }
    }
  }

  // Also check scene files for localization references
  fs::path scenesDir = fs::path(m_projectPath) / "Scenes";
  if (fs::exists(scenesDir)) {
    std::regex locPattern(R"(loc\s*\(\s*[\"']([^\"']+)[\"']\s*\))");

    for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
      if (entry.path().extension() == ".nmscene" || entry.path().extension() == ".json") {
        std::string content;
        if (!readFileToString(entry.path(), content)) {
          continue;
        }

        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin());

        while (std::regex_search(searchStart, content.cend(), match, locPattern)) {
          referencedKeys.insert(match[1].str());
          searchStart = match.suffix().first;
        }
      }
    }
  }

  // Find unused keys
  for (const auto& key : allKeys) {
    if (referencedKeys.find(key) == referencedKeys.end()) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Info;
      issue.category = IssueCategory::Localization;
      issue.code = "L003";
      issue.message = "Unused localization key: " + key;
      issue.suggestions.push_back("Remove unused key to reduce clutter");
      issue.suggestions.push_back("Key may be for future use - verify before removing");
      issue.hasQuickFix = true;
      issue.quickFixDescription = "Remove unused localization key from all locale files";
      issues.push_back(issue);
    }
  }
}

void ProjectValidators::checkScriptSyntax(std::vector<IntegrityIssue>& /*issues*/) {
  // Would use the lexer/parser to validate syntax
  // Complex implementation - placeholder for now
}

void ProjectValidators::checkResourceConflicts(std::vector<IntegrityIssue>& issues) {
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


} // namespace NovelMind::editor
