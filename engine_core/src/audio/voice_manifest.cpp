/**
 * @file voice_manifest.cpp
 * @brief Voice Manifest implementation
 */

#include "NovelMind/audio/voice_manifest.hpp"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace NovelMind::audio {

namespace fs = std::filesystem;

// ============================================================================
// NamingConvention Implementation
// ============================================================================

std::string NamingConvention::generatePath(const std::string &locale,
                                           const std::string &id,
                                           const std::string &scene,
                                           const std::string &speaker,
                                           u32 take) const {
  std::string result = pattern;

  // Replace placeholders
  auto replacePlaceholder = [&result](const std::string &placeholder,
                                      const std::string &value) {
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  };

  replacePlaceholder("{locale}", locale);
  replacePlaceholder("{id}", id);
  replacePlaceholder("{scene}", scene.empty() ? "default" : scene);
  replacePlaceholder("{speaker}", speaker.empty() ? "narrator" : speaker);
  replacePlaceholder("{take}", std::to_string(take));

  return result;
}

// ============================================================================
// VoiceManifest Implementation
// ============================================================================

VoiceManifest::VoiceManifest() {
  m_namingConvention = NamingConvention::localeIdBased();
}

VoiceManifest::~VoiceManifest() = default;

VoiceManifest::VoiceManifest(VoiceManifest &&) noexcept = default;
VoiceManifest &VoiceManifest::operator=(VoiceManifest &&) noexcept = default;

// ============================================================================
// Project Configuration
// ============================================================================

void VoiceManifest::setProjectName(const std::string &name) {
  m_projectName = name;
}

void VoiceManifest::setDefaultLocale(const std::string &locale) {
  m_defaultLocale = locale;
  // Ensure default locale is in the list
  if (!hasLocale(locale)) {
    addLocale(locale);
  }
}

void VoiceManifest::addLocale(const std::string &locale) {
  if (!hasLocale(locale)) {
    m_locales.push_back(locale);
  }
}

void VoiceManifest::removeLocale(const std::string &locale) {
  m_locales.erase(std::remove(m_locales.begin(), m_locales.end(), locale),
                  m_locales.end());
}

bool VoiceManifest::hasLocale(const std::string &locale) const {
  return std::find(m_locales.begin(), m_locales.end(), locale) !=
         m_locales.end();
}

void VoiceManifest::setNamingConvention(const NamingConvention &convention) {
  m_namingConvention = convention;
}

void VoiceManifest::setBasePath(const std::string &path) { m_basePath = path; }

// ============================================================================
// Voice Lines
// ============================================================================

Result<void> VoiceManifest::addLine(const VoiceManifestLine &line) {
  if (line.id.empty()) {
    return Result<void>::error("Voice line ID cannot be empty");
  }

  if (hasLine(line.id)) {
    return Result<void>::error("Voice line with ID '" + line.id +
                               "' already exists");
  }

  m_lines.push_back(line);
  m_lineIdToIndex[line.id] = m_lines.size() - 1;

  fireLineChanged(line.id);
  return {};
}

Result<void> VoiceManifest::updateLine(const VoiceManifestLine &line) {
  auto it = m_lineIdToIndex.find(line.id);
  if (it == m_lineIdToIndex.end()) {
    return Result<void>::error("Voice line with ID '" + line.id +
                               "' not found");
  }

  m_lines[it->second] = line;
  fireLineChanged(line.id);
  return {};
}

void VoiceManifest::removeLine(const std::string &lineId) {
  auto it = m_lineIdToIndex.find(lineId);
  if (it == m_lineIdToIndex.end()) {
    return;
  }

  size_t index = it->second;
  m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(index));
  rebuildIndex();
  fireLineChanged(lineId);
}

const VoiceManifestLine *
VoiceManifest::getLine(const std::string &lineId) const {
  auto it = m_lineIdToIndex.find(lineId);
  if (it != m_lineIdToIndex.end()) {
    return &m_lines[it->second];
  }
  return nullptr;
}

VoiceManifestLine *VoiceManifest::getLineMutable(const std::string &lineId) {
  auto it = m_lineIdToIndex.find(lineId);
  if (it != m_lineIdToIndex.end()) {
    return &m_lines[it->second];
  }
  return nullptr;
}

std::vector<const VoiceManifestLine *>
VoiceManifest::getLinesBySpeaker(const std::string &speaker) const {
  std::vector<const VoiceManifestLine *> result;
  for (const auto &line : m_lines) {
    if (line.speaker == speaker) {
      result.push_back(&line);
    }
  }
  return result;
}

std::vector<const VoiceManifestLine *>
VoiceManifest::getLinesByScene(const std::string &scene) const {
  std::vector<const VoiceManifestLine *> result;
  for (const auto &line : m_lines) {
    if (line.scene == scene) {
      result.push_back(&line);
    }
  }
  return result;
}

std::vector<const VoiceManifestLine *>
VoiceManifest::getLinesByStatus(VoiceLineStatus status,
                                const std::string &locale) const {
  std::vector<const VoiceManifestLine *> result;
  for (const auto &line : m_lines) {
    if (locale.empty()) {
      if (line.getOverallStatus() == status) {
        result.push_back(&line);
      }
    } else {
      auto file = line.getFile(locale);
      if (file && file->status == status) {
        result.push_back(&line);
      } else if (!file && status == VoiceLineStatus::Missing) {
        result.push_back(&line);
      }
    }
  }
  return result;
}

std::vector<const VoiceManifestLine *>
VoiceManifest::getLinesByTag(const std::string &tag) const {
  std::vector<const VoiceManifestLine *> result;
  for (const auto &line : m_lines) {
    if (std::find(line.tags.begin(), line.tags.end(), tag) != line.tags.end()) {
      result.push_back(&line);
    }
  }
  return result;
}

std::vector<std::string> VoiceManifest::getSpeakers() const {
  std::unordered_set<std::string> speakers;
  for (const auto &line : m_lines) {
    if (!line.speaker.empty()) {
      speakers.insert(line.speaker);
    }
  }
  return {speakers.begin(), speakers.end()};
}

std::vector<std::string> VoiceManifest::getScenes() const {
  std::unordered_set<std::string> scenes;
  for (const auto &line : m_lines) {
    if (!line.scene.empty()) {
      scenes.insert(line.scene);
    }
  }
  return {scenes.begin(), scenes.end()};
}

std::vector<std::string> VoiceManifest::getTags() const {
  std::unordered_set<std::string> tags;
  for (const auto &line : m_lines) {
    for (const auto &tag : line.tags) {
      tags.insert(tag);
    }
  }
  return {tags.begin(), tags.end()};
}

bool VoiceManifest::hasLine(const std::string &lineId) const {
  return m_lineIdToIndex.find(lineId) != m_lineIdToIndex.end();
}

void VoiceManifest::clearLines() {
  m_lines.clear();
  m_lineIdToIndex.clear();
}

// ============================================================================
// Take Management
// ============================================================================

Result<void> VoiceManifest::addTake(const std::string &lineId,
                                    const std::string &locale,
                                    const VoiceTake &take) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  // Security: Validate path before storing it
  if (!take.filePath.empty() && !isValidRelativePath(take.filePath)) {
    return Result<void>::error(
        "Invalid file path in take: contains path traversal sequences, "
        "absolute path, or special characters");
  }

  auto &file = line->getOrCreateFile(locale);
  file.takes.push_back(take);

  // If this is the first take, make it active
  if (file.takes.size() == 1) {
    file.takes[0].isActive = true;
    file.activeTakeIndex = 0;
    file.filePath = take.filePath;
    file.duration = take.duration;
    file.status = VoiceLineStatus::Recorded;
  }

  fireLineChanged(lineId);
  return {};
}

Result<void> VoiceManifest::setActiveTake(const std::string &lineId,
                                          const std::string &locale,
                                          u32 takeIndex) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  auto it = line->files.find(locale);
  if (it == line->files.end()) {
    return Result<void>::error("Locale not found for line: " + locale);
  }

  auto &file = it->second;
  if (takeIndex >= file.takes.size()) {
    return Result<void>::error("Take index out of range");
  }

  // Deactivate previous take
  for (auto &take : file.takes) {
    take.isActive = false;
  }

  // Activate new take
  file.takes[takeIndex].isActive = true;
  file.activeTakeIndex = takeIndex;
  file.filePath = file.takes[takeIndex].filePath;
  file.duration = file.takes[takeIndex].duration;

  fireLineChanged(lineId);
  return {};
}

std::vector<VoiceTake>
VoiceManifest::getTakes(const std::string &lineId,
                        const std::string &locale) const {
  const auto *line = getLine(lineId);
  if (!line) {
    return {};
  }

  auto file = line->getFile(locale);
  if (!file) {
    return {};
  }

  return file->takes;
}

Result<void> VoiceManifest::removeTake(const std::string &lineId,
                                       const std::string &locale,
                                       u32 takeNumber) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  auto it = line->files.find(locale);
  if (it == line->files.end()) {
    return Result<void>::error("Locale not found for line: " + locale);
  }

  auto &file = it->second;

  // Find and remove the take with matching takeNumber
  auto takeIt = std::find_if(file.takes.begin(), file.takes.end(),
                             [takeNumber](const VoiceTake &take) {
                               return take.takeNumber == takeNumber;
                             });

  if (takeIt == file.takes.end()) {
    return Result<void>::error("Take not found: " + std::to_string(takeNumber));
  }

  bool wasActive = takeIt->isActive;
  file.takes.erase(takeIt);

  // If we removed the active take, clear the active index or activate another
  if (wasActive) {
    if (!file.takes.empty()) {
      // Activate the first take
      file.takes[0].isActive = true;
      file.activeTakeIndex = 0;
      file.filePath = file.takes[0].filePath;
      file.duration = file.takes[0].duration;
    } else {
      file.activeTakeIndex = 0;
      file.filePath.clear();
      file.duration = 0.0f;
      file.status = VoiceLineStatus::Missing;
    }
  } else if (file.activeTakeIndex > 0) {
    // Adjust active index if needed
    for (size_t i = 0; i < file.takes.size(); ++i) {
      if (file.takes[i].isActive) {
        file.activeTakeIndex = static_cast<u32>(i);
        break;
      }
    }
  }

  fireLineChanged(lineId);
  return {};
}

// ============================================================================
// Status Management
// ============================================================================

Result<void> VoiceManifest::setStatus(const std::string &lineId,
                                      const std::string &locale,
                                      VoiceLineStatus status) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  auto &file = line->getOrCreateFile(locale);
  file.status = status;

  fireStatusChanged(lineId, locale, status);
  return {};
}

Result<void> VoiceManifest::markAsRecorded(const std::string &lineId,
                                           const std::string &locale,
                                           const std::string &filePath) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  // Security: Validate path before storing it
  if (!isValidRelativePath(filePath)) {
    return Result<void>::error(
        "Invalid file path: contains path traversal sequences, absolute path, "
        "or special characters");
  }

  auto &file = line->getOrCreateFile(locale);
  file.filePath = filePath;
  file.status = VoiceLineStatus::Recorded;

  fireStatusChanged(lineId, locale, VoiceLineStatus::Recorded);
  return {};
}

Result<void> VoiceManifest::markAsImported(const std::string &lineId,
                                           const std::string &locale,
                                           const std::string &filePath) {
  auto *line = getLineMutable(lineId);
  if (!line) {
    return Result<void>::error("Voice line not found: " + lineId);
  }

  // Security: Validate path before storing it
  if (!isValidRelativePath(filePath)) {
    return Result<void>::error(
        "Invalid file path: contains path traversal sequences, absolute path, "
        "or special characters");
  }

  auto &file = line->getOrCreateFile(locale);
  file.filePath = filePath;
  file.status = VoiceLineStatus::Imported;

  fireStatusChanged(lineId, locale, VoiceLineStatus::Imported);
  return {};
}

// ============================================================================
// Validation
// ============================================================================

std::vector<ManifestValidationError>
VoiceManifest::validate(bool checkFiles) const {
  std::vector<ManifestValidationError> errors;
  std::unordered_set<std::string> seenIds;
  std::unordered_set<std::string> seenPaths;

  for (const auto &line : m_lines) {
    // Check for duplicate IDs
    if (seenIds.count(line.id) > 0) {
      errors.push_back({ManifestValidationError::Type::DuplicateId, line.id,
                        "id", "Duplicate voice line ID: " + line.id});
    }
    seenIds.insert(line.id);

    // Check required fields
    if (line.id.empty()) {
      errors.push_back({ManifestValidationError::Type::MissingRequiredField,
                        line.id, "id", "Voice line ID is required"});
    }

    if (line.textKey.empty()) {
      errors.push_back({ManifestValidationError::Type::MissingRequiredField,
                        line.id, "textKey",
                        "Text key is required for line: " + line.id});
    }

    // Check file paths
    for (const auto &[locale, file] : line.files) {
      // Check if locale is in manifest's locale list
      if (!hasLocale(locale)) {
        errors.push_back(
            {ManifestValidationError::Type::InvalidLocale, line.id,
             "files." + locale,
             "Locale '" + locale + "' not in manifest locale list"});
      }

      // Check for path conflicts
      if (!file.filePath.empty()) {
        // Security: Validate path before using it
        if (!isValidRelativePath(file.filePath)) {
          errors.push_back({ManifestValidationError::Type::InvalidFilePath,
                            line.id, "files." + locale,
                            "Invalid file path: contains path traversal "
                            "sequences or special characters"});
          continue; // Skip further checks for this invalid path
        }

        if (seenPaths.count(file.filePath) > 0) {
          errors.push_back(
              {ManifestValidationError::Type::PathConflict, line.id,
               "files." + locale,
               "Path conflict: " + file.filePath + " used by multiple lines"});
        }
        seenPaths.insert(file.filePath);

        // Check if file exists
        if (checkFiles && file.status != VoiceLineStatus::Missing) {
          // Security: Use sanitized path resolution
          auto resolvedPathResult = sanitizeAndResolvePath(file.filePath);
          if (resolvedPathResult.isError()) {
            errors.push_back({ManifestValidationError::Type::InvalidFilePath,
                              line.id, "files." + locale,
                              resolvedPathResult.error()});
          } else {
            std::string fullPath = resolvedPathResult.value();
            if (!fs::exists(fullPath)) {
              errors.push_back({ManifestValidationError::Type::FileNotFound,
                                line.id, "files." + locale,
                                "File not found: " + fullPath});
            }
          }
        }
      }
    }
  }

  return errors;
}

// ============================================================================
// Statistics
// ============================================================================

VoiceManifest::CoverageStats
VoiceManifest::getCoverageStats(const std::string &locale) const {
  CoverageStats stats;
  stats.totalLines = static_cast<u32>(m_lines.size());

  for (const auto &line : m_lines) {
    VoiceLineStatus status;
    f32 duration = 0.0f;

    if (locale.empty()) {
      status = line.getOverallStatus();
      // Sum durations across all locales
      for (const auto &[loc, file] : line.files) {
        duration = std::max(duration, file.duration);
      }
    } else {
      auto file = line.getFile(locale);
      status = file ? file->status : VoiceLineStatus::Missing;
      duration = file ? file->duration : 0.0f;
    }

    switch (status) {
    case VoiceLineStatus::Missing:
      ++stats.missingLines;
      break;
    case VoiceLineStatus::Recorded:
      ++stats.recordedLines;
      stats.totalDuration += duration;
      break;
    case VoiceLineStatus::Imported:
      ++stats.importedLines;
      stats.totalDuration += duration;
      break;
    case VoiceLineStatus::NeedsReview:
      ++stats.needsReviewLines;
      stats.totalDuration += duration;
      break;
    case VoiceLineStatus::Approved:
      ++stats.approvedLines;
      stats.totalDuration += duration;
      break;
    }
  }

  if (stats.totalLines > 0) {
    u32 completed =
        stats.recordedLines + stats.importedLines + stats.approvedLines;
    stats.coveragePercent =
        (static_cast<f32>(completed) / static_cast<f32>(stats.totalLines)) *
        100.0f;
  }

  return stats;
}

// ============================================================================
// File I/O - JSON Helpers
// ============================================================================

namespace {

// Simple JSON writer for basic types
class JsonWriter {
public:
  JsonWriter() { m_ss << "{\n"; }

  void addString(const std::string &key, const std::string &value,
                 bool last = false) {
    indent();
    m_ss << "\"" << escapeString(key) << "\": \"" << escapeString(value)
         << "\"";
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void addNumber(const std::string &key, f32 value, bool last = false) {
    indent();
    m_ss << "\"" << escapeString(key) << "\": " << value;
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void addBool(const std::string &key, bool value, bool last = false) {
    indent();
    m_ss << "\"" << escapeString(key) << "\": " << (value ? "true" : "false");
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void addInt(const std::string &key, u32 value, bool last = false) {
    indent();
    m_ss << "\"" << escapeString(key) << "\": " << value;
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void startArray(const std::string &key) {
    indent();
    m_ss << "\"" << escapeString(key) << "\": [\n";
    ++m_indentLevel;
  }

  void endArray(bool last = false) {
    --m_indentLevel;
    indent();
    m_ss << "]";
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void startObject(const std::string &key = "") {
    indent();
    if (!key.empty()) {
      m_ss << "\"" << escapeString(key) << "\": ";
    }
    m_ss << "{\n";
    ++m_indentLevel;
  }

  void endObject(bool last = false) {
    --m_indentLevel;
    indent();
    m_ss << "}";
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  void addArrayString(const std::string &value, bool last = false) {
    indent();
    m_ss << "\"" << escapeString(value) << "\"";
    if (!last)
      m_ss << ",";
    m_ss << "\n";
  }

  std::string finish() {
    m_ss << "}\n";
    return m_ss.str();
  }

private:
  std::stringstream m_ss;
  int m_indentLevel = 1;

  void indent() {
    for (int i = 0; i < m_indentLevel; ++i) {
      m_ss << "  ";
    }
  }

  static std::string escapeString(const std::string &s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
      switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += c;
      }
    }
    return result;
  }
};

// Extract JSON string value
std::string extractJsonString(const std::string &json, const std::string &key) {
  std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"";
  std::regex re(pattern);
  std::smatch match;
  if (std::regex_search(json, match, re)) {
    return match[1].str();
  }
  return "";
}

// Extract JSON number value
f32 extractJsonNumber(const std::string &json, const std::string &key) {
  std::string pattern = "\"" + key + "\"\\s*:\\s*([0-9.]+)";
  std::regex re(pattern);
  std::smatch match;
  if (std::regex_search(json, match, re)) {
    try {
      return std::stof(match[1].str());
    } catch (...) {
    }
  }
  return 0.0f;
}

// Extract JSON array of strings
std::vector<std::string> extractJsonStringArray(const std::string &json,
                                                const std::string &key) {
  std::vector<std::string> result;
  std::string pattern = "\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]";
  std::regex re(pattern);
  std::smatch match;
  if (std::regex_search(json, match, re)) {
    std::string arrayContent = match[1].str();
    std::regex itemRe("\"([^\"]*)\"");
    auto begin =
        std::sregex_iterator(arrayContent.begin(), arrayContent.end(), itemRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
      result.push_back((*it)[1].str());
    }
  }
  return result;
}

} // namespace

// ============================================================================
// File I/O
// ============================================================================

Result<void> VoiceManifest::loadFromFile(const std::string &filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file: " + filePath);
  }

  file.seekg(0, std::ios::end);
  std::streampos size = file.tellg();
  if (size < 0) {
    return Result<void>::error("Failed to get file size: " + filePath);
  }

  std::string content;
  content.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(content.data(), size);

  return loadFromString(content);
}

Result<void> VoiceManifest::loadFromString(const std::string &jsonContent) {
  // Clear existing data
  clearLines();
  m_locales.clear();

  // Extract project metadata
  m_projectName = extractJsonString(jsonContent, "project");
  m_defaultLocale = extractJsonString(jsonContent, "default_locale");
  m_locales = extractJsonStringArray(jsonContent, "locales");
  m_basePath = extractJsonString(jsonContent, "base_path");
  if (m_basePath.empty()) {
    m_basePath = "assets/audio/voice";
  }

  // Extract naming convention
  std::string convention = extractJsonString(jsonContent, "naming_convention");
  if (!convention.empty()) {
    m_namingConvention.pattern = convention;
  }

  // Extract voice lines
  std::string pattern = "\\{[^\\{\\}]*\"id\"\\s*:\\s*\"([^\"]*)\"[^\\{\\}]*\\}";
  std::regex lineRe("\"id\"\\s*:\\s*\"([^\"]*)\"");

  // Find "lines" array
  size_t linesStart = jsonContent.find("\"lines\"");
  if (linesStart != std::string::npos) {
    size_t arrayStart = jsonContent.find('[', linesStart);
    size_t arrayEnd = jsonContent.rfind(']');

    if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
      std::string linesContent =
          jsonContent.substr(arrayStart, arrayEnd - arrayStart + 1);

      // Parse each line object
      std::regex objRe("\\{([^\\{\\}]|\\{[^\\{\\}]*\\})*\\}");
      auto begin =
          std::sregex_iterator(linesContent.begin(), linesContent.end(), objRe);
      auto end = std::sregex_iterator();

      for (auto it = begin; it != end; ++it) {
        std::string lineJson = it->str();

        VoiceManifestLine line;
        line.id = extractJsonString(lineJson, "id");
        line.textKey = extractJsonString(lineJson, "text_key");
        line.speaker = extractJsonString(lineJson, "speaker");
        line.scene = extractJsonString(lineJson, "scene");
        line.notes = extractJsonString(lineJson, "notes");
        line.tags = extractJsonStringArray(lineJson, "tags");
        line.sourceScript = extractJsonString(lineJson, "source_script");
        line.sourceLine =
            static_cast<u32>(extractJsonNumber(lineJson, "source_line"));
        line.durationOverride =
            extractJsonNumber(lineJson, "duration_override");

        // Parse files
        size_t filesStart = lineJson.find("\"files\"");
        if (filesStart != std::string::npos) {
          size_t filesObjStart = lineJson.find('{', filesStart);
          size_t filesObjEnd = lineJson.find('}', filesObjStart);
          if (filesObjStart != std::string::npos &&
              filesObjEnd != std::string::npos) {
            std::string filesJson =
                lineJson.substr(filesObjStart, filesObjEnd - filesObjStart + 1);

            // For each locale in manifest
            for (const auto &locale : m_locales) {
              std::string locPattern =
                  "\"" + locale + "\"\\s*:\\s*\"([^\"]*)\"";
              std::regex locRe(locPattern);
              std::smatch locMatch;
              if (std::regex_search(filesJson, locMatch, locRe)) {
                std::string filePath = locMatch[1].str();

                // Security: Validate path before storing it
                if (!filePath.empty() && !isValidRelativePath(filePath)) {
                  // Skip invalid paths - don't add them to the manifest
                  // This prevents path traversal attacks from malicious manifests
                  continue;
                }

                VoiceLocaleFile locFile;
                locFile.locale = locale;
                locFile.filePath = filePath;
                locFile.status = locFile.filePath.empty()
                                     ? VoiceLineStatus::Missing
                                     : VoiceLineStatus::Imported;
                line.files[locale] = locFile;
              }
            }
          }
        }

        if (!line.id.empty()) {
          addLine(line);
        }
      }
    }
  }

  return {};
}

Result<void> VoiceManifest::saveToFile(const std::string &filePath) const {
  auto jsonResult = toJsonString();
  if (jsonResult.isError()) {
    return Result<void>::error(jsonResult.error());
  }

  std::ofstream file(filePath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + filePath);
  }

  file << jsonResult.value();
  return {};
}

Result<std::string> VoiceManifest::toJsonString() const {
  JsonWriter writer;

  writer.addString("project", m_projectName);
  writer.addString("default_locale", m_defaultLocale);

  // Locales array
  writer.startArray("locales");
  for (size_t i = 0; i < m_locales.size(); ++i) {
    writer.addArrayString(m_locales[i], i == m_locales.size() - 1);
  }
  writer.endArray();

  writer.addString("naming_convention", m_namingConvention.pattern);
  writer.addString("base_path", m_basePath);

  // Lines array
  writer.startArray("lines");
  for (size_t i = 0; i < m_lines.size(); ++i) {
    const auto &line = m_lines[i];
    bool isLastLine = (i == m_lines.size() - 1);

    writer.startObject();
    writer.addString("id", line.id);
    writer.addString("text_key", line.textKey);
    writer.addString("speaker", line.speaker);
    writer.addString("scene", line.scene);

    if (!line.notes.empty()) {
      writer.addString("notes", line.notes);
    }

    if (line.durationOverride > 0.0f) {
      writer.addNumber("duration_override", line.durationOverride);
    }

    if (!line.sourceScript.empty()) {
      writer.addString("source_script", line.sourceScript);
      writer.addInt("source_line", line.sourceLine);
    }

    // Tags
    if (!line.tags.empty()) {
      writer.startArray("tags");
      for (size_t j = 0; j < line.tags.size(); ++j) {
        writer.addArrayString(line.tags[j], j == line.tags.size() - 1);
      }
      writer.endArray();
    }

    // Files
    if (!line.files.empty()) {
      writer.startObject("files");
      size_t fileIndex = 0;
      for (const auto &[locale, file] : line.files) {
        bool isLastFile = (fileIndex == line.files.size() - 1);
        writer.addString(locale, file.filePath, isLastFile);
        ++fileIndex;
      }
      writer.endObject(true); // files is always last in line object
    }

    writer.endObject(isLastLine);
  }
  writer.endArray(true);

  return Result<std::string>::ok(writer.finish());
}

// ============================================================================
// CSV Import/Export
// ============================================================================

Result<void> VoiceManifest::importFromCsv(const std::string &csvPath,
                                          const std::string &locale) {
  std::ifstream file(csvPath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open CSV file: " + csvPath);
  }

  // Ensure locale is in the list
  if (!hasLocale(locale)) {
    addLocale(locale);
  }

  std::string line;
  bool headerSkipped = false;

  while (std::getline(file, line)) {
    if (!headerSkipped) {
      headerSkipped = true;
      continue;
    }

    if (line.empty())
      continue;

    // Simple CSV parsing (assumes: id,speaker,text_key,voice_file,scene)
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;

    while (std::getline(ss, field, ',')) {
      // Remove quotes
      if (!field.empty() && field.front() == '"' && field.back() == '"') {
        field = field.substr(1, field.size() - 2);
      }
      fields.push_back(field);
    }

    if (fields.size() >= 2) {
      std::string id = fields[0];

      // Check if line already exists
      auto *existingLine = getLineMutable(id);
      if (existingLine) {
        // Update existing line with file path
        if (fields.size() >= 4 && !fields[3].empty()) {
          // Security: Validate path before storing it
          if (isValidRelativePath(fields[3])) {
            auto &locFile = existingLine->getOrCreateFile(locale);
            locFile.filePath = fields[3];
            locFile.status = VoiceLineStatus::Imported;
          }
          // Silently skip invalid paths to prevent injection
        }
      } else {
        // Create new line
        VoiceManifestLine newLine;
        newLine.id = id;
        newLine.speaker = fields.size() > 1 ? fields[1] : "";
        newLine.textKey = fields.size() > 2 ? fields[2] : id;

        if (fields.size() >= 4 && !fields[3].empty()) {
          // Security: Validate path before storing it
          if (isValidRelativePath(fields[3])) {
            VoiceLocaleFile locFile;
            locFile.locale = locale;
            locFile.filePath = fields[3];
            locFile.status = VoiceLineStatus::Imported;
            newLine.files[locale] = locFile;
          }
          // Silently skip invalid paths to prevent injection
        }

        newLine.scene = fields.size() > 4 ? fields[4] : "";
        addLine(newLine);
      }
    }
  }

  return {};
}

Result<void> VoiceManifest::exportToCsv(const std::string &csvPath,
                                        const std::string &locale) const {
  std::ofstream file(csvPath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open CSV file for writing: " +
                               csvPath);
  }

  // Header
  file << "id,speaker,text_key,voice_file,scene,status\n";

  for (const auto &line : m_lines) {
    std::string voiceFile;
    std::string status;

    if (locale.empty()) {
      // Export default locale
      auto locFile = line.getFile(m_defaultLocale);
      if (locFile) {
        voiceFile = locFile->filePath;
        status = voiceLineStatusToString(locFile->status);
      } else {
        status = "missing";
      }
    } else {
      auto locFile = line.getFile(locale);
      if (locFile) {
        voiceFile = locFile->filePath;
        status = voiceLineStatusToString(locFile->status);
      } else {
        status = "missing";
      }
    }

    file << "\"" << line.id << "\","
         << "\"" << line.speaker << "\","
         << "\"" << line.textKey << "\","
         << "\"" << voiceFile << "\","
         << "\"" << line.scene << "\","
         << "\"" << status << "\"\n";
  }

  return {};
}

Result<void> VoiceManifest::exportTemplate(const std::string &filePath) const {
  // Create a manifest without file paths for recording workflow
  VoiceManifest templateManifest;
  templateManifest.setProjectName(m_projectName);
  templateManifest.setDefaultLocale(m_defaultLocale);
  for (const auto &locale : m_locales) {
    templateManifest.addLocale(locale);
  }
  templateManifest.setNamingConvention(m_namingConvention);
  templateManifest.setBasePath(m_basePath);

  // Copy lines without file paths
  for (const auto &line : m_lines) {
    VoiceManifestLine templateLine;
    templateLine.id = line.id;
    templateLine.textKey = line.textKey;
    templateLine.speaker = line.speaker;
    templateLine.scene = line.scene;
    templateLine.tags = line.tags;
    templateLine.notes = line.notes;
    templateLine.sourceScript = line.sourceScript;
    templateLine.sourceLine = line.sourceLine;
    // No files - this is a template

    templateManifest.addLine(templateLine);
  }

  return templateManifest.saveToFile(filePath);
}

// ============================================================================
// Generation
// ============================================================================

u32 VoiceManifest::generateFilePaths(const std::string &locale,
                                     bool overwriteExisting) {
  u32 count = 0;

  for (auto &line : m_lines) {
    auto &file = line.getOrCreateFile(locale);

    if (!overwriteExisting && !file.filePath.empty()) {
      continue;
    }

    std::string path = m_namingConvention.generatePath(
        locale, line.id, line.scene, line.speaker, 1);
    file.filePath = path;
    ++count;
  }

  rebuildIndex();
  return count;
}

Result<void>
VoiceManifest::createOutputDirectories(const std::string &locale) const {
  std::unordered_set<std::string> directories;

  for (const auto &line : m_lines) {
    auto file = line.getFile(locale);
    if (file && !file->filePath.empty()) {
      fs::path filePath = file->filePath;
      if (filePath.has_parent_path()) {
        directories.insert(
            (fs::path(m_basePath) / filePath.parent_path()).string());
      }
    }
  }

  for (const auto &dir : directories) {
    try {
      fs::create_directories(dir);
    } catch (const fs::filesystem_error &e) {
      return Result<void>::error("Failed to create directory: " + dir + " - " +
                                 e.what());
    }
  }

  return {};
}

// ============================================================================
// Callbacks
// ============================================================================

void VoiceManifest::setOnLineChanged(OnLineChanged callback) {
  m_onLineChanged = std::move(callback);
}

void VoiceManifest::setOnStatusChanged(OnStatusChanged callback) {
  m_onStatusChanged = std::move(callback);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void VoiceManifest::rebuildIndex() {
  m_lineIdToIndex.clear();
  for (size_t i = 0; i < m_lines.size(); ++i) {
    m_lineIdToIndex[m_lines[i].id] = i;
  }
}

void VoiceManifest::fireLineChanged(const std::string &lineId) {
  if (m_onLineChanged) {
    m_onLineChanged(lineId);
  }
}

void VoiceManifest::fireStatusChanged(const std::string &lineId,
                                      const std::string &locale,
                                      VoiceLineStatus status) {
  if (m_onStatusChanged) {
    m_onStatusChanged(lineId, locale, status);
  }
}

// ============================================================================
// Security: Path Validation
// ============================================================================

bool VoiceManifest::isValidRelativePath(const std::string &path) const {
  if (path.empty()) {
    return false;
  }

  // Reject absolute paths (Unix and Windows)
  if (path[0] == '/' || path[0] == '\\') {
    return false;
  }

  // Reject Windows absolute paths (C:, D:, etc.)
  if (path.length() >= 2 && path[1] == ':') {
    return false;
  }

  // Reject null bytes (null byte injection attack)
  if (path.find('\0') != std::string::npos) {
    return false;
  }

  // Reject paths containing ".." (path traversal)
  // Check for "../", "..\\" and ".." at the end
  if (path.find("..") != std::string::npos) {
    return false;
  }

  // Reject special device names on Windows
  static const std::vector<std::string> windowsDevices = {
      "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3",
      "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1",
      "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8",
      "LPT9"};

  std::string upperPath = path;
  std::transform(upperPath.begin(), upperPath.end(), upperPath.begin(),
                 ::toupper);

  for (const auto &device : windowsDevices) {
    if (upperPath == device || upperPath.find(device + ".") == 0 ||
        upperPath.find("/" + device) != std::string::npos ||
        upperPath.find("\\" + device) != std::string::npos) {
      return false;
    }
  }

  return true;
}

Result<std::string>
VoiceManifest::sanitizeAndResolvePath(const std::string &relativePath) const {
  // First, check for obviously invalid paths
  if (!isValidRelativePath(relativePath)) {
    return Result<std::string>::error(
        "Invalid file path: contains path traversal sequences, absolute path, "
        "or special characters");
  }

  try {
    // Canonicalize the base path
    fs::path basePath = fs::weakly_canonical(fs::path(m_basePath));

    // Combine with the relative path
    fs::path combined = basePath / relativePath;

    // Canonicalize the combined path
    // Use weakly_canonical to handle non-existent paths
    fs::path canonical = fs::weakly_canonical(combined);

    // Check if the canonical path is within the base directory
    // We need to ensure the resolved path starts with the base path
    auto baseStr = basePath.lexically_normal().string();
    auto canonicalStr = canonical.lexically_normal().string();

    // Ensure both paths end consistently for comparison
    if (!baseStr.empty() && baseStr.back() != '/' && baseStr.back() != '\\') {
      baseStr += '/';
    }

    // Check if canonical path starts with base path
    if (canonicalStr.find(baseStr) != 0) {
      return Result<std::string>::error(
          "Path traversal detected: resolved path '" + canonicalStr +
          "' is outside allowed directory '" + baseStr + "'");
    }

    return Result<std::string>::ok(canonical.string());

  } catch (const fs::filesystem_error &e) {
    return Result<std::string>::error(std::string("Filesystem error: ") +
                                      e.what());
  }
}

} // namespace NovelMind::audio
