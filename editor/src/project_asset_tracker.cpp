#include "NovelMind/editor/project_asset_tracker.hpp"
#include "NovelMind/editor/project_integrity.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

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

ProjectAssetTracker::ProjectAssetTracker() = default;

void ProjectAssetTracker::setProjectPath(const std::string& projectPath) {
  m_projectPath = projectPath;
}

void ProjectAssetTracker::scanProjectAssets() {
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

void ProjectAssetTracker::collectAssetReferences() {
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

void ProjectAssetTracker::checkAssetReferences(std::vector<IntegrityIssue>& issues) {
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

void ProjectAssetTracker::findOrphanedAssets(std::vector<IntegrityIssue>& issues) {
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


} // namespace NovelMind::editor
