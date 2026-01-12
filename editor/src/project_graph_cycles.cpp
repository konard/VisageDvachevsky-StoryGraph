#include "NovelMind/editor/project_graph_analyzer.hpp"
#include "NovelMind/editor/project_integrity.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
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

void ProjectGraphAnalyzer::detectCycles(std::vector<IntegrityIssue>& issues) {
  // Use DFS with coloring to detect cycles in the story graph
  fs::path scriptsDir = fs::path(m_projectPath) / "Scripts";
  if (!fs::exists(scriptsDir)) {
    return;
  }

  // Build scene graph
  std::unordered_set<std::string> definedScenes;
  std::unordered_map<std::string, std::vector<std::string>> sceneTransitions;
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
    return;
  }

  // Second pass: build transition graph
  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.path().extension() == ".nms") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      std::smatch sceneMatch;
      std::string::const_iterator sceneSearchStart(content.cbegin());

      while (std::regex_search(sceneSearchStart, content.cend(), sceneMatch, scenePattern)) {
        std::string currentScene = sceneMatch[1].str();
        size_t sceneStart = static_cast<size_t>(
            std::distance(content.cbegin(), sceneSearchStart) + sceneMatch.position(0));

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

        // Find goto statements
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

  // DFS to detect cycles using colors
  enum class Color { White, Gray, Black };
  std::unordered_map<std::string, Color> colors;
  std::vector<std::string> cycleStack;
  std::unordered_set<std::string> reportedCycles;

  // Initialize all nodes as white
  for (const auto& scene : definedScenes) {
    colors[scene] = Color::White;
  }

  // DFS function to detect cycles
  std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
    colors[node] = Color::Gray;
    cycleStack.push_back(node);

    if (sceneTransitions.find(node) != sceneTransitions.end()) {
      for (const auto& neighbor : sceneTransitions[node]) {
        if (colors[neighbor] == Color::Gray) {
          // Found a cycle - build cycle path
          std::vector<std::string> cycle;
          bool inCycle = false;
          for (const auto& stackNode : cycleStack) {
            if (stackNode == neighbor) {
              inCycle = true;
            }
            if (inCycle) {
              cycle.push_back(stackNode);
            }
          }
          cycle.push_back(neighbor); // Complete the cycle

          // Create a unique key for this cycle
          std::string cycleKey;
          for (const auto& n : cycle) {
            cycleKey += n + "->";
          }

          // Only report each unique cycle once
          if (reportedCycles.find(cycleKey) == reportedCycles.end()) {
            reportedCycles.insert(cycleKey);

            // Build cycle description
            std::string cyclePath;
            for (size_t i = 0; i < cycle.size(); ++i) {
              if (i > 0)
                cyclePath += " -> ";
              cyclePath += cycle[i];
            }

            IntegrityIssue issue;
            issue.severity = IssueSeverity::Warning;
            issue.category = IssueCategory::StoryGraph;
            issue.code = "G006";
            issue.message = "Cycle detected in story graph";
            issue.context = cyclePath;
            issue.filePath = sceneFiles[node];
            issue.suggestions.push_back("Verify if this cycle is intentional (e.g., gameplay loop)");
            issue.suggestions.push_back("Add an 'end' statement to break unintended loops");
            issue.suggestions.push_back("Ensure player has a way to exit the cycle");
            issue.hasQuickFix = false;
            issues.push_back(issue);
          }
        } else if (colors[neighbor] == Color::White) {
          if (dfs(neighbor)) {
            return true;
          }
        }
      }
    }

    cycleStack.pop_back();
    colors[node] = Color::Black;
    return false;
  };

  // Run DFS from all white nodes
  for (const auto& scene : definedScenes) {
    if (colors[scene] == Color::White) {
      dfs(scene);
    }
  }
}


} // namespace NovelMind::editor
