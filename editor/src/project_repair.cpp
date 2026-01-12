#include "NovelMind/editor/project_integrity.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace NovelMind::editor {

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

  if (issue.code == "L003") {
    // Unused localization key - remove from all locale files
    std::string key;
    const std::string prefix = "Unused localization key: ";
    size_t pos = issue.message.find(prefix);
    if (pos != std::string::npos) {
      key = issue.message.substr(pos + prefix.length());
    }
    if (!key.empty()) {
      return QuickFixes::removeUnusedLocalizationKey(m_projectPath, key);
    }
    return Result<void>::error("Could not extract localization key from issue message");
  }

  if (issue.code == "G003") {
    // Invalid goto reference - comment out the line
    if (!issue.filePath.empty() && issue.lineNumber > 0) {
      return QuickFixes::commentOutLine(issue.filePath, issue.lineNumber);
    }
    return Result<void>::error("No file path or line number specified");
  }

  if (issue.code == "G004") {
    // Duplicate scene definition - requires manual resolution
    return Result<void>::error("Duplicate scene definitions require manual resolution");
  }

  if (issue.code == "G005") {
    // Unreachable scene - no automatic fix (requires manual graph connection)
    return Result<void>::error("Unreachable scenes require manual connection to the story graph");
  }

  if (issue.code == "G006") {
    // Cycle detected - no automatic fix (needs manual review)
    return Result<void>::error("Story graph cycles require manual review and resolution");
  }

  return Result<void>::error("Quick fix not implemented for issue: " + issue.code);
}


} // namespace NovelMind::editor
