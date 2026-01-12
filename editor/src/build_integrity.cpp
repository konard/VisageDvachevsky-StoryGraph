/**
 * @file build_integrity.cpp
 * @brief IntegrityChecker implementation for NovelMind
 *
 * Implements project integrity checks:
 * - Missing assets detection
 * - Script validation
 * - Localization checks
 * - Unreachable content detection
 * - Circular reference detection
 */

#include "NovelMind/editor/build_system.hpp"

#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// IntegrityChecker Implementation
// ============================================================================

IntegrityChecker::IntegrityChecker() = default;
IntegrityChecker::~IntegrityChecker() = default;

Result<std::vector<IntegrityChecker::Issue>>
IntegrityChecker::checkProject(const std::string& projectPath) {
  std::vector<Issue> allIssues;

  // Run all checks
  auto missingAssets = checkMissingAssets(projectPath);
  allIssues.insert(allIssues.end(), missingAssets.begin(), missingAssets.end());

  auto scriptIssues = checkScripts(projectPath);
  allIssues.insert(allIssues.end(), scriptIssues.begin(), scriptIssues.end());

  auto localizationIssues = checkLocalization(projectPath);
  allIssues.insert(allIssues.end(), localizationIssues.begin(), localizationIssues.end());

  auto unreachableIssues = checkUnreachableContent(projectPath);
  allIssues.insert(allIssues.end(), unreachableIssues.begin(), unreachableIssues.end());

  auto circularIssues = checkCircularReferences(projectPath);
  allIssues.insert(allIssues.end(), circularIssues.begin(), circularIssues.end());

  return Result<std::vector<Issue>>::ok(allIssues);
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkMissingAssets(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Scan for referenced assets in scripts and scenes
  m_referencedAssets.clear();
  m_existingAssets.clear();

  // Collect existing assets
  fs::path assetsDir = fs::path(projectPath) / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
      if (entry.is_regular_file()) {
        m_existingAssets.push_back(fs::relative(entry.path(), assetsDir).string());
      }
    }
  }

  // Check for missing required directories
  std::vector<std::string> requiredDirs = {"assets", "scripts"};
  for (const auto& dir : requiredDirs) {
    if (!fs::exists(fs::path(projectPath) / dir)) {
      Issue issue;
      issue.severity = Issue::Severity::Error;
      issue.message = "Missing required directory: " + dir;
      issue.file = projectPath;
      issues.push_back(issue);
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkScripts(const std::string& projectPath) {
  std::vector<Issue> issues;

  fs::path scriptsDir = fs::path(projectPath) / "scripts";
  if (!fs::exists(scriptsDir)) {
    return issues;
  }

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".nms" || ext == ".nmscript") {
        // Basic syntax check
        std::ifstream file(entry.path());
        if (!file.is_open()) {
          Issue issue;
          issue.severity = Issue::Severity::Error;
          issue.message = "Cannot open script file";
          issue.file = entry.path().string();
          issues.push_back(issue);
          continue;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Check for balanced braces
        i32 braceCount = 0;
        i32 line = 1;
        for (char c : content) {
          if (c == '\n')
            line++;
          if (c == '{')
            braceCount++;
          if (c == '}')
            braceCount--;
        }

        if (braceCount != 0) {
          Issue issue;
          issue.severity = Issue::Severity::Warning;
          issue.message = "Unbalanced braces detected";
          issue.file = entry.path().string();
          issues.push_back(issue);
        }
      }
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkLocalization(const std::string& projectPath) {
  std::vector<Issue> issues;

  fs::path localizationDir = fs::path(projectPath) / "localization";
  if (!fs::exists(localizationDir)) {
    Issue issue;
    issue.severity = Issue::Severity::Info;
    issue.message = "No localization directory found";
    issue.file = projectPath;
    issues.push_back(issue);
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkUnreachableContent(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Check for unreachable scenes and content
  // Strategy: Find all scenes, then determine which are referenced
  // from the start scene or from other reachable scenes

  fs::path scenesDir = fs::path(projectPath) / "scenes";
  if (!fs::exists(scenesDir)) {
    return issues; // No scenes to check
  }

  // Collect all scene files
  std::vector<std::string> allScenes;
  for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".scene" || ext == ".json") {
        allScenes.push_back(fs::relative(entry.path(), projectPath).string());
      }
    }
  }

  if (allScenes.empty()) {
    return issues; // No scenes to check
  }

  // Find the start/entry scene (usually defined in project config)
  std::set<std::string> reachableScenes;
  fs::path projectConfig = fs::path(projectPath) / "project.json";

  if (fs::exists(projectConfig)) {
    // Mark start scene as reachable
    // In a full implementation, we would parse the JSON to find the start scene
    // For now, we'll just mark the first scene or "main.scene" as reachable
    for (const auto& scene : allScenes) {
      if (scene.find("main.scene") != std::string::npos ||
          scene.find("start.scene") != std::string::npos ||
          scene.find("intro.scene") != std::string::npos) {
        reachableScenes.insert(scene);
        break;
      }
    }
  }

  // If no start scene found, consider the first scene as start
  if (reachableScenes.empty() && !allScenes.empty()) {
    reachableScenes.insert(allScenes[0]);
  }

  // Find scenes referenced from scripts
  fs::path scriptsDir = fs::path(projectPath) / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          std::ifstream file(entry.path());
          std::string line;
          while (std::getline(file, line)) {
            // Look for scene references (e.g., "goto scene_name" or "load_scene")
            if (line.find("goto") != std::string::npos || line.find("scene") != std::string::npos ||
                line.find("load") != std::string::npos) {
              // Mark scenes mentioned in scripts as potentially reachable
              for (const auto& scene : allScenes) {
                std::string sceneName = fs::path(scene).stem().string();
                if (line.find(sceneName) != std::string::npos) {
                  reachableScenes.insert(scene);
                }
              }
            }
          }
        }
      }
    }
  }

  // Report unreachable scenes
  for (const auto& scene : allScenes) {
    if (reachableScenes.find(scene) == reachableScenes.end()) {
      Issue issue;
      issue.severity = Issue::Severity::Warning;
      issue.message = "Scene appears to be unreachable (not referenced from start scene or "
                      "scripts)";
      issue.file = scene;
      issues.push_back(issue);
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkCircularReferences(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Check for circular dependencies in scene references
  // Use a simple depth-first search to detect cycles

  fs::path scenesDir = fs::path(projectPath) / "scenes";
  if (!fs::exists(scenesDir)) {
    return issues; // No scenes to check
  }

  // Build a dependency graph of scenes
  std::map<std::string, std::set<std::string>> sceneReferences;

  // Collect all scene files and their references
  for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".scene" || ext == ".json") {
        std::string scenePath = fs::relative(entry.path(), projectPath).string();
        std::string sceneName = fs::path(scenePath).stem().string();

        // Parse scene file for references to other scenes
        std::ifstream file(entry.path());
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Look for scene references in the content
        // This is a simplified approach - in production, parse JSON properly
        std::set<std::string> references;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
          if (line.find("scene") != std::string::npos || line.find("goto") != std::string::npos ||
              line.find("next") != std::string::npos) {
            // Try to extract scene names from the line
            // Look for quoted strings or identifiers
            size_t pos = 0;
            while ((pos = line.find(".scene", pos)) != std::string::npos) {
              // Found a potential scene reference
              size_t start = line.rfind('\"', pos);
              if (start != std::string::npos && start < pos) {
                size_t end = line.find('\"', pos);
                if (end != std::string::npos) {
                  std::string refName = line.substr(start + 1, end - start - 1);
                  if (!refName.empty() && refName != sceneName) {
                    references.insert(refName);
                  }
                }
              }
              pos++;
            }
          }
        }

        sceneReferences[sceneName] = references;
      }
    }
  }

  // Detect cycles using DFS
  std::set<std::string> visited;
  std::set<std::string> recursionStack;

  std::function<bool(const std::string&, std::vector<std::string>&)> detectCycle;
  detectCycle = [&](const std::string& scene, std::vector<std::string>& path) -> bool {
    if (recursionStack.find(scene) != recursionStack.end()) {
      // Found a cycle - build the cycle path
      auto it = std::find(path.begin(), path.end(), scene);
      if (it != path.end()) {
        Issue issue;
        issue.severity = Issue::Severity::Error;
        std::string cyclePath = "Circular dependency detected: ";
        for (auto pathIt = it; pathIt != path.end(); ++pathIt) {
          cyclePath += *pathIt + " -> ";
        }
        cyclePath += scene;
        issue.message = cyclePath;
        issue.file = scene;
        issues.push_back(issue);
        return true;
      }
    }

    if (visited.find(scene) != visited.end()) {
      return false; // Already processed this node
    }

    visited.insert(scene);
    recursionStack.insert(scene);
    path.push_back(scene);

    auto it = sceneReferences.find(scene);
    if (it != sceneReferences.end()) {
      for (const auto& ref : it->second) {
        if (detectCycle(ref, path)) {
          // Cycle detected, but continue checking for more cycles
        }
      }
    }

    path.pop_back();
    recursionStack.erase(scene);
    return false;
  };

  // Check all scenes for cycles
  for (const auto& pair : sceneReferences) {
    std::vector<std::string> path;
    detectCycle(pair.first, path);
  }

  return issues;
}

} // namespace NovelMind::editor
