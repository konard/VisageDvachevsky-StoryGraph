#include "NovelMind/editor/project_manager.hpp"
#include <algorithm>
#include <filesystem>

namespace NovelMind::editor {

// ============================================================================
// Folder Structure
// ============================================================================

Result<void> ProjectManager::createFolderStructure() {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return Result<void>::error("No project path set");
  }

  std::error_code ec;
  fs::path base(m_projectPath);

  // Create all standard folders
  std::vector<fs::path> folders = {base / "Assets",
                                   base / "Assets" / "Images",
                                   base / "Assets" / "Audio",
                                   base / "Assets" / "Fonts",
                                   base / "scripts",
                                   base / "scripts" / "generated",
                                   base / "Scenes",
                                   base / "Localization",
                                   base / "Build",
                                   base / ".temp",
                                   base / ".backup"};

  for (const auto& folder : folders) {
    if (!fs::exists(folder)) {
      if (!fs::create_directories(folder, ec)) {
        return Result<void>::error("Failed to create folder: " + folder.string() + " - " +
                                   ec.message());
      }
    }
  }

  return Result<void>::ok();
}

bool ProjectManager::verifyFolderStructure() const {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return false;
  }

  fs::path base(m_projectPath);

  // Check required folders
  std::vector<fs::path> required = {base / "Assets", base / "scripts", base / "Scenes"};

  for (const auto& folder : required) {
    if (!fs::exists(folder) || !fs::is_directory(folder)) {
      return false;
    }
  }

  return true;
}

Result<void> ProjectManager::createFolder(const std::string& relativePath) {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return Result<void>::error("No project is open");
  }

  fs::path fullPath = fs::path(m_projectPath) / relativePath;

  std::error_code ec;
  if (!fs::create_directories(fullPath, ec)) {
    return Result<void>::error("Failed to create folder: " + ec.message());
  }

  return Result<void>::ok();
}

bool ProjectManager::isPathInProject(const std::string& path) const {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return false;
  }

  fs::path projectPath = fs::canonical(m_projectPath);
  fs::path targetPath = fs::canonical(path);

  auto it =
      std::search(targetPath.begin(), targetPath.end(), projectPath.begin(), projectPath.end());

  return it == targetPath.begin();
}

std::string ProjectManager::toRelativePath(const std::string& absolutePath) const {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return absolutePath;
  }

  return fs::relative(absolutePath, m_projectPath).string();
}

std::string ProjectManager::toAbsolutePath(const std::string& relativePath) const {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return relativePath;
  }

  return (fs::path(m_projectPath) / relativePath).string();
}

std::string ProjectManager::getFolderPath(ProjectFolder folder) const {
  namespace fs = std::filesystem;

  if (m_projectPath.empty()) {
    return "";
  }

  fs::path base(m_projectPath);

  switch (folder) {
  case ProjectFolder::Root:
    return m_projectPath;
  case ProjectFolder::Assets:
    return (base / "Assets").string();
  case ProjectFolder::Images:
    return (base / "Assets" / "Images").string();
  case ProjectFolder::Audio:
    return (base / "Assets" / "Audio").string();
  case ProjectFolder::Fonts:
    return (base / "Assets" / "Fonts").string();
  case ProjectFolder::Scripts:
    return (base / "scripts").string();
  case ProjectFolder::ScriptsGenerated:
    return (base / "scripts" / "generated").string();
  case ProjectFolder::Scenes:
    return (base / "Scenes").string();
  case ProjectFolder::Localization:
    return (base / "Localization").string();
  case ProjectFolder::Build:
    return (base / "Build").string();
  case ProjectFolder::Temp:
    return (base / ".temp").string();
  case ProjectFolder::Backup:
    return (base / ".backup").string();
  }

  return m_projectPath;
}

std::vector<std::string> ProjectManager::getProjectFiles(const std::string& extension) const {
  namespace fs = std::filesystem;

  std::vector<std::string> files;

  if (m_projectPath.empty()) {
    return files;
  }

  for (const auto& entry : fs::recursive_directory_iterator(m_projectPath)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      files.push_back(entry.path().string());
    }
  }

  return files;
}

} // namespace NovelMind::editor
