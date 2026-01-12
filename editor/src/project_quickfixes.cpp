#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"

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

Result<void> removeUnusedLocalizationKey(const std::string& projectPath, const std::string& key) {
  fs::path locDir = fs::path(projectPath) / "Localization";
  if (!fs::exists(locDir)) {
    return Result<void>::error("Localization directory not found");
  }

  bool anyChanges = false;

  // Remove key from all locale files
  for (const auto& entry : fs::directory_iterator(locDir)) {
    if (entry.path().extension() == ".json") {
      std::string content;
      if (!readFileToString(entry.path(), content)) {
        continue;
      }

      // Create regex to match the key and its value (including comma handling)
      std::regex keyPattern("\\s*,?\\s*\"" + key + "\"\\s*:\\s*\"[^\"]*\"\\s*,?\\s*");
      std::string modifiedContent = std::regex_replace(content, keyPattern, "");

      // Clean up any double commas or trailing commas
      modifiedContent = std::regex_replace(modifiedContent, std::regex(",\\s*,"), ",");
      modifiedContent = std::regex_replace(modifiedContent, std::regex(",\\s*}"), "\n}");

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
  return Result<void>::error("Key not found in any localization file: " + key);
}

Result<void> commentOutLine(const std::string& filePath, int lineNumber) {
  if (!fs::exists(filePath)) {
    return Result<void>::error("File not found: " + filePath);
  }

  std::ifstream inFile(filePath);
  if (!inFile.is_open()) {
    return Result<void>::error("Failed to open file: " + filePath);
  }

  std::vector<std::string> lines;
  std::string line;
  int currentLine = 0;

  while (std::getline(inFile, line)) {
    currentLine++;
    if (currentLine == lineNumber) {
      // Comment out this line
      lines.push_back("    // [DISABLED] " + line);
    } else {
      lines.push_back(line);
    }
  }
  inFile.close();

  // Write back
  std::ofstream outFile(filePath);
  if (!outFile.is_open()) {
    return Result<void>::error("Failed to write file: " + filePath);
  }

  for (const auto& l : lines) {
    outFile << l << "\n";
  }
  outFile.close();

  return Result<void>::ok();
}

} // namespace QuickFixes

} // namespace NovelMind::editor
