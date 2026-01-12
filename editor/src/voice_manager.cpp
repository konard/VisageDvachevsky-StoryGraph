/**
 * @file voice_manager.cpp
 * @brief Voice Manager implementation
 */

#include "NovelMind/editor/voice_manager.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace NovelMind::editor {

namespace fs = std::filesystem;
namespace {

bool readFileToString(std::ifstream& file, std::string& out) {
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(out.data(), static_cast<std::streamsize>(out.size()));
  return static_cast<bool>(file);
}

} // namespace

VoiceManager::VoiceManager(audio::AudioManager* audioManager) : m_audioManager(audioManager) {
  setDefaultPatterns();
}

VoiceManager::~VoiceManager() {
  if (m_previewPlaying) {
    stopPreview();
  }
}

// =========================================================================
// Project Loading
// =========================================================================

Result<void> VoiceManager::loadProject(const std::string& projectPath) {
  m_projectPath = projectPath;
  m_voiceAssetsPath = (fs::path(projectPath) / "Assets" / "Audio" / "Voice").string();

  // Clear existing data
  m_dialogueLines.clear();
  m_voiceFiles.clear();
  m_lineIdToIndex.clear();
  m_voicePathToIndex.clear();

  // Load voice bindings
  auto bindingsResult = loadBindings();
  if (bindingsResult.isError()) {
    // Bindings file may not exist yet, which is fine
  }

  // Scan voice files
  auto voiceResult = refreshVoiceFiles();
  if (voiceResult.isError()) {
    return voiceResult;
  }

  // Parse dialogue lines
  auto dialogueResult = refreshDialogueLines();
  if (dialogueResult.isError()) {
    return dialogueResult;
  }

  m_projectLoaded = true;
  return {};
}

Result<void> VoiceManager::refreshVoiceFiles() {
  m_voiceFiles.clear();
  m_voicePathToIndex.clear();

  if (!fs::exists(m_voiceAssetsPath)) {
    // Voice directory doesn't exist yet, which is fine
    return {};
  }

  scanVoiceDirectory(m_voiceAssetsPath);

  // Build lookup map
  for (size_t i = 0; i < m_voiceFiles.size(); ++i) {
    m_voicePathToIndex[m_voiceFiles[i].path] = i;
    m_voicePathToIndex[m_voiceFiles[i].relativePath] = i;
  }

  return {};
}

Result<void> VoiceManager::refreshDialogueLines() {
  m_dialogueLines.clear();
  m_lineIdToIndex.clear();

  std::string scriptsPath = (fs::path(m_projectPath) / "Scripts").string();
  if (!fs::exists(scriptsPath)) {
    return Result<void>::error("Scripts directory not found: " + scriptsPath);
  }

  // Recursively parse all script files
  for (const auto& entry : fs::recursive_directory_iterator(scriptsPath)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".nms" || ext == ".nm") {
        parseDialogueFromScript(entry.path().string());
      }
    }
  }

  // Build lookup map
  for (size_t i = 0; i < m_dialogueLines.size(); ++i) {
    m_lineIdToIndex[m_dialogueLines[i].id] = i;
  }

  return {};
}

void VoiceManager::scanVoiceDirectory(const std::string& path) {
  for (const auto& entry : fs::recursive_directory_iterator(path)) {
    if (!entry.is_regular_file())
      continue;

    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Support common audio formats
    if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac") {
      VoiceFileEntry voiceEntry;
      voiceEntry.path = entry.path().string();
      voiceEntry.relativePath = fs::relative(entry.path(), m_projectPath).string();
      voiceEntry.filename = entry.path().filename().string();
      voiceEntry.fileSize = fs::file_size(entry.path());
      voiceEntry.modifiedTimestamp =
          static_cast<u64>(fs::last_write_time(entry.path()).time_since_epoch().count());
      voiceEntry.duration = getAudioDuration(voiceEntry.path);
      voiceEntry.bound = false;

      m_voiceFiles.push_back(std::move(voiceEntry));
    }
  }
}

void VoiceManager::parseDialogueFromScript(const std::string& scriptPath) {
  std::ifstream file(scriptPath);
  if (!file.is_open())
    return;

  std::string sceneName = fs::path(scriptPath).stem().string();
  std::string currentCharacter;
  u32 lineNumber = 0;
  u32 dialogueIndex = 0;

  std::string line;
  while (std::getline(file, line)) {
    ++lineNumber;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#' || line.find("//") == 0)
      continue;

    // Look for character declarations: character Name(...)
    std::regex charRegex(R"(character\s+(\w+)\s*\()");
    std::smatch charMatch;
    if (std::regex_search(line, charMatch, charRegex)) {
      // Store character info if needed
      continue;
    }

    // Look for say commands: say CharacterName "text"
    std::regex sayRegex(R"(say\s+(\w+)\s+\"([^\"]*)\")");
    std::smatch sayMatch;
    if (std::regex_search(line, sayMatch, sayRegex)) {
      DialogueLine dialogueLine;
      dialogueLine.characterId = sayMatch[1].str();
      dialogueLine.text = sayMatch[2].str();
      dialogueLine.sceneId = sceneName;
      dialogueLine.lineNumber = lineNumber;

      // Generate unique ID
      std::stringstream ss;
      ss << sceneName << "_" << dialogueLine.characterId << "_" << std::setfill('0') << std::setw(3)
         << dialogueIndex++;
      dialogueLine.id = ss.str();

      dialogueLine.status = VoiceBindingStatus::Unbound;
      m_dialogueLines.push_back(std::move(dialogueLine));
      continue;
    }

    // Look for inline dialogue: Character "text"
    std::regex inlineRegex(R"(^\s*(\w+)\s+\"([^\"]*)\"\s*$)");
    std::smatch inlineMatch;
    if (std::regex_search(line, inlineMatch, inlineRegex)) {
      std::string potentialChar = inlineMatch[1].str();
      // Skip keywords
      if (potentialChar != "show" && potentialChar != "hide" && potentialChar != "play" &&
          potentialChar != "stop" && potentialChar != "goto" && potentialChar != "call" &&
          potentialChar != "if" && potentialChar != "else" && potentialChar != "scene" &&
          potentialChar != "background") {
        DialogueLine dialogueLine;
        dialogueLine.characterId = potentialChar;
        dialogueLine.text = inlineMatch[2].str();
        dialogueLine.sceneId = sceneName;
        dialogueLine.lineNumber = lineNumber;

        std::stringstream ss;
        ss << sceneName << "_" << dialogueLine.characterId << "_" << std::setfill('0')
           << std::setw(3) << dialogueIndex++;
        dialogueLine.id = ss.str();

        dialogueLine.status = VoiceBindingStatus::Unbound;
        m_dialogueLines.push_back(std::move(dialogueLine));
      }
    }
  }
}

// =========================================================================
// Dialogue Lines
// =========================================================================

std::vector<const DialogueLine*>
VoiceManager::getFilteredLines(const VoiceLineFilter& filter) const {
  std::vector<const DialogueLine*> result;
  result.reserve(m_dialogueLines.size());

  for (const auto& line : m_dialogueLines) {
    // Character filter
    if (!filter.characterFilter.empty() && line.characterId != filter.characterFilter)
      continue;

    // Scene filter
    if (!filter.sceneFilter.empty() && line.sceneId != filter.sceneFilter)
      continue;

    // Status filter
    if (!filter.showAllStatuses && line.status != filter.statusFilter)
      continue;

    // Text search
    if (!filter.searchText.empty()) {
      std::string lowerText = line.text;
      std::string lowerSearch = filter.searchText;
      std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
      std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
      if (lowerText.find(lowerSearch) == std::string::npos)
        continue;
    }

    result.push_back(&line);
  }

  return result;
}

std::vector<const DialogueLine*> VoiceManager::getUnboundLines() const {
  std::vector<const DialogueLine*> result;
  for (const auto& line : m_dialogueLines) {
    if (line.status == VoiceBindingStatus::Unbound) {
      result.push_back(&line);
    }
  }
  return result;
}

const DialogueLine* VoiceManager::getLine(const std::string& lineId) const {
  auto it = m_lineIdToIndex.find(lineId);
  if (it != m_lineIdToIndex.end()) {
    return &m_dialogueLines[it->second];
  }
  return nullptr;
}

std::vector<const DialogueLine*>
VoiceManager::getLinesForCharacter(const std::string& characterId) const {
  std::vector<const DialogueLine*> result;
  for (const auto& line : m_dialogueLines) {
    if (line.characterId == characterId) {
      result.push_back(&line);
    }
  }
  return result;
}

std::vector<const DialogueLine*> VoiceManager::getLinesForScene(const std::string& sceneId) const {
  std::vector<const DialogueLine*> result;
  for (const auto& line : m_dialogueLines) {
    if (line.sceneId == sceneId) {
      result.push_back(&line);
    }
  }
  return result;
}

// =========================================================================
// Voice Files
// =========================================================================

std::vector<const VoiceFileEntry*> VoiceManager::getUnboundVoiceFiles() const {
  std::vector<const VoiceFileEntry*> result;
  for (const auto& file : m_voiceFiles) {
    if (!file.bound) {
      result.push_back(&file);
    }
  }
  return result;
}

const VoiceFileEntry* VoiceManager::getVoiceFile(const std::string& path) const {
  auto it = m_voicePathToIndex.find(path);
  if (it != m_voicePathToIndex.end()) {
    return &m_voiceFiles[it->second];
  }
  return nullptr;
}

// =========================================================================
// Voice Binding
// =========================================================================

Result<void> VoiceManager::bindVoice(const std::string& lineId, const std::string& voicePath) {
  auto lineIt = m_lineIdToIndex.find(lineId);
  if (lineIt == m_lineIdToIndex.end()) {
    return Result<void>::error("Dialogue line not found: " + lineId);
  }

  DialogueLine& line = m_dialogueLines[lineIt->second];

  // Unbind previous voice file if any
  if (!line.voiceFile.empty()) {
    auto prevIt = m_voicePathToIndex.find(line.voiceFile);
    if (prevIt != m_voicePathToIndex.end()) {
      m_voiceFiles[prevIt->second].bound = false;
      m_voiceFiles[prevIt->second].boundLineId.clear();
    }
  }

  // Check if voice file exists
  std::string fullPath = voicePath;
  if (!fs::path(voicePath).is_absolute()) {
    fullPath = m_projectPath + "/" + voicePath;
  }

  if (!fs::exists(fullPath)) {
    line.voiceFile = voicePath;
    line.status = VoiceBindingStatus::MissingFile;
  } else {
    line.voiceFile = voicePath;
    line.status = VoiceBindingStatus::Bound;
    line.voiceDuration = getAudioDuration(fullPath);

    // Mark voice file as bound
    auto voiceIt = m_voicePathToIndex.find(voicePath);
    if (voiceIt != m_voicePathToIndex.end()) {
      m_voiceFiles[voiceIt->second].bound = true;
      m_voiceFiles[voiceIt->second].boundLineId = lineId;
    }
  }

  fireBindingChanged(lineId, voicePath);
  return {};
}

void VoiceManager::unbindVoice(const std::string& lineId) {
  auto lineIt = m_lineIdToIndex.find(lineId);
  if (lineIt == m_lineIdToIndex.end())
    return;

  DialogueLine& line = m_dialogueLines[lineIt->second];

  if (!line.voiceFile.empty()) {
    auto voiceIt = m_voicePathToIndex.find(line.voiceFile);
    if (voiceIt != m_voicePathToIndex.end()) {
      m_voiceFiles[voiceIt->second].bound = false;
      m_voiceFiles[voiceIt->second].boundLineId.clear();
    }
  }

  line.voiceFile.clear();
  line.status = VoiceBindingStatus::Unbound;
  line.voiceDuration = 0.0f;

  fireBindingChanged(lineId, "");
}

void VoiceManager::clearAllBindings() {
  for (auto& line : m_dialogueLines) {
    line.voiceFile.clear();
    line.status = VoiceBindingStatus::Unbound;
    line.voiceDuration = 0.0f;
  }

  for (auto& file : m_voiceFiles) {
    file.bound = false;
    file.boundLineId.clear();
  }
}

// =========================================================================
// Auto-Mapping
// =========================================================================

void VoiceManager::setDefaultPatterns() {
  m_mappingPatterns.clear();

  // Pattern 1: character_lineId.ext (e.g., hero_001_hello.ogg)
  VoiceMappingPattern p1;
  p1.name = "character_id_description";
  p1.pattern = R"(^(\w+)_(\d{3}_\w+)\.\w+$)";
  p1.characterGroup = "1";
  p1.lineIdGroup = "2";
  p1.priority = 100;
  m_mappingPatterns.push_back(p1);

  // Pattern 2: character_lineNumber.ext (e.g., narrator_042.ogg)
  VoiceMappingPattern p2;
  p2.name = "character_number";
  p2.pattern = R"(^(\w+)_(\d+)\.\w+$)";
  p2.characterGroup = "1";
  p2.lineIdGroup = "2";
  p2.priority = 90;
  m_mappingPatterns.push_back(p2);

  // Pattern 3: scene_character_line.ext (e.g., intro_hero_greeting.ogg)
  VoiceMappingPattern p3;
  p3.name = "scene_character_description";
  p3.pattern = R"(^(\w+)_(\w+)_(\w+)\.\w+$)";
  p3.characterGroup = "2";
  p3.lineIdGroup = "3";
  p3.priority = 80;
  m_mappingPatterns.push_back(p3);

  // Sort by priority
  std::sort(m_mappingPatterns.begin(), m_mappingPatterns.end(),
            [](const VoiceMappingPattern& a, const VoiceMappingPattern& b) {
              return a.priority > b.priority;
            });
}

void VoiceManager::addMappingPattern(const VoiceMappingPattern& pattern) {
  m_mappingPatterns.push_back(pattern);
  std::sort(m_mappingPatterns.begin(), m_mappingPatterns.end(),
            [](const VoiceMappingPattern& a, const VoiceMappingPattern& b) {
              return a.priority > b.priority;
            });
}

void VoiceManager::removeMappingPattern(const std::string& name) {
  m_mappingPatterns.erase(
      std::remove_if(m_mappingPatterns.begin(), m_mappingPatterns.end(),
                     [&name](const VoiceMappingPattern& p) { return p.name == name; }),
      m_mappingPatterns.end());
}

Result<u32> VoiceManager::autoMapVoiceFiles() {
  u32 mapped = 0;
  u32 failed = 0;

  for (const auto& voiceFile : m_voiceFiles) {
    if (voiceFile.bound)
      continue;

    std::string character, lineId;
    bool matched = false;

    for (const auto& pattern : m_mappingPatterns) {
      if (!pattern.enabled)
        continue;

      if (matchPattern(voiceFile.filename, pattern, character, lineId)) {
        matched = true;
        break;
      }
    }

    if (matched) {
      // Try to find a matching dialogue line
      for (auto& line : m_dialogueLines) {
        if (line.status != VoiceBindingStatus::Unbound)
          continue;

        // Case-insensitive comparison
        std::string lineChar = line.characterId;
        std::transform(lineChar.begin(), lineChar.end(), lineChar.begin(), ::tolower);
        std::transform(character.begin(), character.end(), character.begin(), ::tolower);

        if (lineChar == character) {
          // Check if line ID matches or if it's a sequential match
          auto bindResult = bindVoice(line.id, voiceFile.relativePath);
          if (bindResult.isOk()) {
            // Mark as auto-mapped
            auto it = m_lineIdToIndex.find(line.id);
            if (it != m_lineIdToIndex.end()) {
              m_dialogueLines[it->second].status = VoiceBindingStatus::AutoMapped;
            }
            ++mapped;
            break;
          } else {
            ++failed;
          }
        }
      }
    }
  }

  if (m_onAutoMappingComplete) {
    m_onAutoMappingComplete(mapped, failed);
  }

  return Result<u32>::ok(mapped);
}

std::unordered_map<std::string, std::string> VoiceManager::previewAutoMapping() const {
  std::unordered_map<std::string, std::string> result;

  for (const auto& voiceFile : m_voiceFiles) {
    if (voiceFile.bound)
      continue;

    std::string character, lineId;
    for (const auto& pattern : m_mappingPatterns) {
      if (!pattern.enabled)
        continue;

      if (matchPattern(voiceFile.filename, pattern, character, lineId)) {
        // Find potential matching line
        for (const auto& line : m_dialogueLines) {
          if (line.status != VoiceBindingStatus::Unbound)
            continue;

          std::string lineChar = line.characterId;
          std::transform(lineChar.begin(), lineChar.end(), lineChar.begin(), ::tolower);
          std::string testChar = character;
          std::transform(testChar.begin(), testChar.end(), testChar.begin(), ::tolower);

          if (lineChar == testChar) {
            result[line.id] = voiceFile.relativePath;
            break;
          }
        }
        break;
      }
    }
  }

  return result;
}

bool VoiceManager::matchPattern(const std::string& filename, const VoiceMappingPattern& pattern,
                                std::string& outCharacter, std::string& outLineId) const {
  try {
    std::regex regex(pattern.pattern);
    std::smatch match;
    if (std::regex_match(filename, match, regex)) {
      int charGroup = std::stoi(pattern.characterGroup);
      int lineGroup = std::stoi(pattern.lineIdGroup);

      if (charGroup < static_cast<int>(match.size())) {
        outCharacter = match[static_cast<size_t>(charGroup)].str();
      }
      if (lineGroup < static_cast<int>(match.size())) {
        outLineId = match[static_cast<size_t>(lineGroup)].str();
      }
      return true;
    }
  } catch (const std::regex_error&) {
    // Invalid regex pattern
  }
  return false;
}

// =========================================================================
// Voice Preview
// =========================================================================

void VoiceManager::previewVoice(const std::string& lineId) {
  const auto* line = getLine(lineId);
  if (!line || line->voiceFile.empty())
    return;

  previewVoiceFile(line->voiceFile);
  m_previewingLineId = lineId;
}

void VoiceManager::previewVoiceFile(const std::string& voicePath) {
  if (m_previewPlaying) {
    stopPreview();
  }

  std::string fullPath = voicePath;
  if (!fs::path(voicePath).is_absolute()) {
    fullPath = m_projectPath + "/" + voicePath;
  }

  if (m_audioManager) {
    audio::VoiceConfig config;
    config.duckMusic = false; // Don't duck in editor preview
    m_previewHandle = m_audioManager->playVoice(fullPath, config);
    m_previewPlaying = true;

    if (m_onPreviewStart) {
      m_onPreviewStart(m_previewingLineId);
    }
  }
}

void VoiceManager::stopPreview() {
  if (m_audioManager && m_previewPlaying) {
    m_audioManager->stopVoice(0.1f);
    m_previewPlaying = false;
    m_previewingLineId.clear();

    if (m_onPreviewStop) {
      m_onPreviewStop();
    }
  }
}

// =========================================================================
// Statistics
// =========================================================================

VoiceCoverageStats VoiceManager::getCoverageStats() const {
  VoiceCoverageStats stats;

  for (const auto& line : m_dialogueLines) {
    ++stats.totalLines;
    ++stats.linesByCharacter[line.characterId];

    switch (line.status) {
    case VoiceBindingStatus::Bound:
    case VoiceBindingStatus::AutoMapped:
      ++stats.boundLines;
      ++stats.boundByCharacter[line.characterId];
      break;
    case VoiceBindingStatus::MissingFile:
      ++stats.missingFiles;
      break;
    case VoiceBindingStatus::Unbound:
    case VoiceBindingStatus::Pending:
      ++stats.unboundLines;
      break;
    }
  }

  if (stats.totalLines > 0) {
    stats.coveragePercent =
        (static_cast<f32>(stats.boundLines) / static_cast<f32>(stats.totalLines)) * 100.0f;
  }

  return stats;
}

std::vector<std::string> VoiceManager::getCharacters() const {
  std::vector<std::string> result;
  std::unordered_map<std::string, bool> seen;

  for (const auto& line : m_dialogueLines) {
    if (!seen[line.characterId]) {
      result.push_back(line.characterId);
      seen[line.characterId] = true;
    }
  }

  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::string> VoiceManager::getScenes() const {
  std::vector<std::string> result;
  std::unordered_map<std::string, bool> seen;

  for (const auto& line : m_dialogueLines) {
    if (!seen[line.sceneId]) {
      result.push_back(line.sceneId);
      seen[line.sceneId] = true;
    }
  }

  std::sort(result.begin(), result.end());
  return result;
}

// =========================================================================
// Import/Export
// =========================================================================

Result<void> VoiceManager::exportVoiceTable(const std::string& outputPath,
                                            VoiceTableFormat format) const {
  std::vector<const DialogueLine*> allLines;
  allLines.reserve(m_dialogueLines.size());
  for (const auto& line : m_dialogueLines) {
    allLines.push_back(&line);
  }

  switch (format) {
  case VoiceTableFormat::CSV:
    return exportCSV(outputPath, allLines);
  case VoiceTableFormat::JSON:
    return exportJSON(outputPath, allLines);
  case VoiceTableFormat::TSV:
    return exportTSV(outputPath, allLines);
  default:
    return Result<void>::error("Unknown export format");
  }
}

Result<void> VoiceManager::importVoiceTable(const std::string& inputPath, VoiceTableFormat format) {
  switch (format) {
  case VoiceTableFormat::CSV:
    return importCSV(inputPath);
  case VoiceTableFormat::JSON:
    return importJSON(inputPath);
  case VoiceTableFormat::TSV:
    return importTSV(inputPath);
  default:
    return Result<void>::error("Unknown import format");
  }
}

Result<void> VoiceManager::exportUnboundLines(const std::string& outputPath,
                                              VoiceTableFormat format) const {
  auto unbound = getUnboundLines();

  switch (format) {
  case VoiceTableFormat::CSV:
    return exportCSV(outputPath, unbound);
  case VoiceTableFormat::JSON:
    return exportJSON(outputPath, unbound);
  case VoiceTableFormat::TSV:
    return exportTSV(outputPath, unbound);
  default:
    return Result<void>::error("Unknown export format");
  }
}

Result<void> VoiceManager::exportCSV(const std::string& path,
                                     const std::vector<const DialogueLine*>& lines) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  // Header
  file << "LineID,Scene,Character,Text,VoiceFile,Status\n";

  for (const auto* line : lines) {
    file << "\"" << line->id << "\","
         << "\"" << line->sceneId << "\","
         << "\"" << line->characterId << "\","
         << "\"" << line->text << "\","
         << "\"" << line->voiceFile << "\"," << static_cast<int>(line->status) << "\n";
  }

  return {};
}

Result<void> VoiceManager::exportJSON(const std::string& path,
                                      const std::vector<const DialogueLine*>& lines) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  file << "{\n  \"voiceAssignments\": [\n";

  for (size_t i = 0; i < lines.size(); ++i) {
    const auto* line = lines[i];
    file << "    {\n"
         << "      \"lineId\": \"" << line->id << "\",\n"
         << "      \"scene\": \"" << line->sceneId << "\",\n"
         << "      \"character\": \"" << line->characterId << "\",\n"
         << "      \"text\": \"" << line->text << "\",\n"
         << "      \"voiceFile\": \"" << line->voiceFile << "\"\n"
         << "    }";
    if (i < lines.size() - 1)
      file << ",";
    file << "\n";
  }

  file << "  ]\n}\n";

  return {};
}

Result<void> VoiceManager::exportTSV(const std::string& path,
                                     const std::vector<const DialogueLine*>& lines) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  // Header
  file << "LineID\tScene\tCharacter\tText\tVoiceFile\n";

  for (const auto* line : lines) {
    file << line->id << "\t" << line->sceneId << "\t" << line->characterId << "\t" << line->text
         << "\t" << line->voiceFile << "\n";
  }

  return {};
}

Result<void> VoiceManager::importCSV(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for reading: " + path);
  }

  std::string line;
  std::getline(file, line); // Skip header

  while (std::getline(file, line)) {
    // Simple CSV parsing (doesn't handle quoted commas)
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

    if (fields.size() >= 5 && !fields[0].empty() && !fields[4].empty()) {
      bindVoice(fields[0], fields[4]);
    }
  }

  return {};
}

Result<void> VoiceManager::importJSON(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for reading: " + path);
  }

  // Simple JSON parsing for voice assignments
  std::string content;
  if (!readFileToString(file, content)) {
    return Result<void>::error("Failed to read file: " + path);
  }

  // Find each assignment
  std::string lineIdPattern = "\"lineId\"\\s*:\\s*\"([^\"]*)\"";
  std::string voiceFilePattern = "\"voiceFile\"\\s*:\\s*\"([^\"]*)\"";
  std::regex lineIdRegex(lineIdPattern);
  std::regex voiceFileRegex(voiceFilePattern);

  auto lineIdBegin = std::sregex_iterator(content.begin(), content.end(), lineIdRegex);
  auto lineIdEnd = std::sregex_iterator();

  std::vector<std::string> lineIds;
  for (auto it = lineIdBegin; it != lineIdEnd; ++it) {
    lineIds.push_back((*it)[1].str());
  }

  auto voiceFileBegin = std::sregex_iterator(content.begin(), content.end(), voiceFileRegex);
  auto voiceFileEnd = std::sregex_iterator();
  std::vector<std::string> voiceFiles;
  for (auto it = voiceFileBegin; it != voiceFileEnd; ++it) {
    voiceFiles.push_back((*it)[1].str());
  }

  for (size_t i = 0; i < std::min(lineIds.size(), voiceFiles.size()); ++i) {
    if (!lineIds[i].empty() && !voiceFiles[i].empty()) {
      bindVoice(lineIds[i], voiceFiles[i]);
    }
  }

  return {};
}

Result<void> VoiceManager::importTSV(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for reading: " + path);
  }

  std::string line;
  std::getline(file, line); // Skip header

  while (std::getline(file, line)) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '\t')) {
      fields.push_back(field);
    }

    if (fields.size() >= 5 && !fields[0].empty() && !fields[4].empty()) {
      bindVoice(fields[0], fields[4]);
    }
  }

  return {};
}

// =========================================================================
// Save/Load
// =========================================================================

Result<void> VoiceManager::saveBindings() {
  std::string bindingsPath = m_projectPath + "/voice_bindings.json";

  std::vector<const DialogueLine*> boundLines;
  for (const auto& line : m_dialogueLines) {
    if (!line.voiceFile.empty()) {
      boundLines.push_back(&line);
    }
  }

  return exportJSON(bindingsPath, boundLines);
}

Result<void> VoiceManager::loadBindings() {
  std::string bindingsPath = m_projectPath + "/voice_bindings.json";

  if (!fs::exists(bindingsPath)) {
    // No bindings file yet, which is fine
    return {};
  }

  return importJSON(bindingsPath);
}

// =========================================================================
// Callbacks
// =========================================================================

void VoiceManager::setOnVoiceBindingChanged(OnVoiceBindingChanged callback) {
  m_onBindingChanged = std::move(callback);
}

void VoiceManager::setOnAutoMappingComplete(OnAutoMappingComplete callback) {
  m_onAutoMappingComplete = std::move(callback);
}

void VoiceManager::setOnVoicePreviewStart(OnVoicePreviewStart callback) {
  m_onPreviewStart = std::move(callback);
}

void VoiceManager::setOnVoicePreviewStop(OnVoicePreviewStop callback) {
  m_onPreviewStop = std::move(callback);
}

void VoiceManager::fireBindingChanged(const std::string& lineId, const std::string& voiceFile) {
  if (m_onBindingChanged) {
    m_onBindingChanged(lineId, voiceFile);
  }
}

f32 VoiceManager::getAudioDuration(const std::string& path) const {
  // Audio duration would be calculated using actual audio loading
  // For now, estimate based on file size (rough approximation)
  // Assumes ~128kbps average bitrate
  if (fs::exists(path)) {
    u64 fileSize = fs::file_size(path);
    return static_cast<f32>(fileSize) / (128000.0f / 8.0f);
  }
  return 0.0f;
}

} // namespace NovelMind::editor
