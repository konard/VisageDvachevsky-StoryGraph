#include "NovelMind/editor/project_graph_analyzer.hpp"
#include "NovelMind/editor/project_integrity.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <queue>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

ProjectGraphAnalyzer::ProjectGraphAnalyzer() = default;

void ProjectGraphAnalyzer::setProjectPath(const std::string& projectPath) {
  m_projectPath = projectPath;
}

void ProjectGraphAnalyzer::checkStoryGraphStructure(std::vector<IntegrityIssue>& issues) {
  // Parse story graph files and validate structure
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return;
  }

  bool hasEntryPoint = false;
  std::unordered_set<std::string> definedScenes;
  std::unordered_map<std::string, std::string> sceneFiles;

  // Collect all defined scenes
  std::regex scenePattern(R"(scene\s+(\w+)\s*\{)");

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch match;
      std::string::const_iterator searchStart(content.cbegin());

      while (std::regex_search(searchStart, content.cend(), match, scenePattern)) {
        std::string sceneName = match[1].str();
        definedScenes.insert(sceneName);
        sceneFiles[sceneName] = entry.path().string();

        // Check for entry point scenes
        if (sceneName == "main" || sceneName == "start" || sceneName == "intro") {
          hasEntryPoint = true;
        }

        searchStart = match.suffix().first;
      }
    }
  }

  // Check for entry point
  if (!hasEntryPoint) {
    IntegrityIssue issue;
    issue.severity = IssueSeverity::Error;
    issue.category = IssueCategory::StoryGraph;
    issue.code = "G001";
    issue.message = "No entry point scene found (main, start, or intro)";
    issue.suggestions.push_back("Create a 'main' scene as entry point");
    issue.hasQuickFix = true;
    issue.quickFixDescription = "Create main scene";
    issues.push_back(issue);
  }

  // Check for invalid goto references
  std::regex gotoPattern(R"(\bgoto\s+(\w+))");

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::string line;
      std::istringstream stream(content);
      int lineNum = 0;

      while (std::getline(stream, line)) {
        lineNum++;
        std::smatch match;

        if (std::regex_search(line, match, gotoPattern)) {
          std::string targetScene = match[1].str();

          // Validate that the target scene exists
          if (definedScenes.find(targetScene) == definedScenes.end()) {
            IntegrityIssue issue;
            issue.severity = IssueSeverity::Error;
            issue.category = IssueCategory::StoryGraph;
            issue.code = "G003";
            issue.message = "Goto references undefined scene: " + targetScene;
            issue.filePath = entry.path().string();
            issue.lineNumber = lineNum;
            issue.context = line;
            issue.suggestions.push_back("Define scene '" + targetScene + "'");
            issue.suggestions.push_back("Fix the goto reference");
            issue.hasQuickFix = true;
            issue.quickFixDescription = "Comment out invalid goto";
            issues.push_back(issue);
          }
        }
      }
    }
  }

  // Check for duplicate scene definitions
  std::unordered_map<std::string, std::vector<std::string>> sceneDuplicates;

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch match;
      std::string::const_iterator searchStart(content.cbegin());

      while (std::regex_search(searchStart, content.cend(), match, scenePattern)) {
        std::string sceneName = match[1].str();
        sceneDuplicates[sceneName].push_back(entry.path().string());
        searchStart = match.suffix().first;
      }
    }
  }

  for (const auto& [sceneName, files] : sceneDuplicates) {
    if (files.size() > 1) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Error;
      issue.category = IssueCategory::StoryGraph;
      issue.code = "G004";
      issue.message = "Duplicate scene definition: " + sceneName;
      issue.context = "Found in " + std::to_string(files.size()) + " files";
      for (const auto& file : files) {
        issue.suggestions.push_back("  - " + file);
      }
      issues.push_back(issue);
    }
  }
}

void ProjectGraphAnalyzer::analyzeReachability(std::vector<IntegrityIssue>& issues) {
  // Perform graph traversal to find unreachable scenes
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return;
  }

  // Build scene graph
  std::unordered_set<std::string> definedScenes;
  std::unordered_map<std::string, std::vector<std::string>> sceneTransitions; // scene -> [target scenes]
  std::unordered_map<std::string, std::string> sceneFiles;

  std::regex scenePattern(R"(scene\s+(\w+)\s*\{)");
  std::regex gotoPattern(R"(\bgoto\s+(\w+))");

  // First pass: collect all defined scenes
  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch match;
      std::string::const_iterator searchStart(content.cbegin());

      while (std::regex_search(searchStart, content.cend(), match, scenePattern)) {
        std::string sceneName = match[1].str();
        definedScenes.insert(sceneName);
        sceneFiles[sceneName] = entry.path().string();
        searchStart = match.suffix().first;
      }
    }
  }

  if (definedScenes.empty()) {
    return; // No scenes to analyze
  }

  // Second pass: build transition graph
  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      // Find which scene we're in
      std::smatch sceneMatch;
      std::string::const_iterator sceneSearchStart(content.cbegin());
      std::string currentScene;

      while (std::regex_search(sceneSearchStart, content.cend(), sceneMatch, scenePattern)) {
        currentScene = sceneMatch[1].str();
        size_t sceneStart = static_cast<size_t>(
            std::distance(content.cbegin(), sceneSearchStart) + sceneMatch.position(0));

        // Find scene end (matching closing brace)
        size_t braceStart = content.find('{', sceneStart);
        if (braceStart == std::string::npos) {
          sceneSearchStart = sceneMatch.suffix().first;
          continue;
        }

        // Find matching closing brace
        int braceCount = 1;
        size_t braceEnd = braceStart + 1;
        while (braceEnd < content.size() && braceCount > 0) {
          if (content[braceEnd] == '{')
            braceCount++;
          else if (content[braceEnd] == '}')
            braceCount--;
          braceEnd++;
        }

        if (braceCount != 0) {
          sceneSearchStart = sceneMatch.suffix().first;
          continue;
        }

        std::string sceneContent = content.substr(braceStart, braceEnd - braceStart);

        // Find goto statements in this scene
        std::smatch gotoMatch;
        std::string::const_iterator gotoSearchStart(sceneContent.cbegin());

        while (std::regex_search(gotoSearchStart, sceneContent.cend(), gotoMatch, gotoPattern)) {
          std::string targetScene = gotoMatch[1].str();
          if (definedScenes.find(targetScene) != definedScenes.end()) {
            sceneTransitions[currentScene].push_back(targetScene);
          }
          gotoSearchStart = gotoMatch.suffix().first;
        }

        sceneSearchStart = sceneMatch.suffix().first;
      }
    }
  }

  // Find entry point scenes
  std::unordered_set<std::string> entryPoints;
  for (const auto& scene : definedScenes) {
    if (scene == "main" || scene == "start" || scene == "intro") {
      entryPoints.insert(scene);
    }
  }

  if (entryPoints.empty() && !definedScenes.empty()) {
    // Use first defined scene as entry point
    entryPoints.insert(*definedScenes.begin());
  }

  // BFS to find reachable scenes
  std::unordered_set<std::string> reachable;
  std::queue<std::string> toVisit;

  for (const auto& entry : entryPoints) {
    toVisit.push(entry);
    reachable.insert(entry);
  }

  while (!toVisit.empty()) {
    std::string current = toVisit.front();
    toVisit.pop();

    if (sceneTransitions.find(current) != sceneTransitions.end()) {
      for (const auto& target : sceneTransitions[current]) {
        if (reachable.find(target) == reachable.end()) {
          reachable.insert(target);
          toVisit.push(target);
        }
      }
    }
  }

  // Report unreachable scenes
  for (const auto& scene : definedScenes) {
    if (reachable.find(scene) == reachable.end()) {
      IntegrityIssue issue;
      issue.severity = IssueSeverity::Warning;
      issue.category = IssueCategory::StoryGraph;
      issue.code = "G005";
      issue.message = "Scene '" + scene + "' is unreachable from entry points";
      issue.filePath = sceneFiles[scene];
      issue.suggestions.push_back("Add a goto to this scene from a reachable scene");
      issue.suggestions.push_back("Remove the scene if it's no longer needed");
      issue.hasQuickFix = false;
      issues.push_back(issue);
    }
  }
}

void ProjectGraphAnalyzer::checkDeadEnds(std::vector<IntegrityIssue>& issues) {
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



} // namespace NovelMind::editor
