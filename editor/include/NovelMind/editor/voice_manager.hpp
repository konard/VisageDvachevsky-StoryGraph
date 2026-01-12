#pragma once

/**
 * @file voice_manager.hpp
 * @brief Voice Manager Panel - Voice-over management for visual novels
 *
 * Provides comprehensive voice-over management:
 * - Automatic voice file mapping (pattern-based filename recognition)
 * - Manual voice line binding
 * - Voice preview playback
 * - Timeline synchronization with dialogue
 * - Voice coverage statistics
 * - Export/import voice assignment tables
 */

#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Voice line binding status
 */
enum class VoiceBindingStatus : u8 {
  Unbound,     // No voice file assigned
  Bound,       // Voice file assigned and verified
  MissingFile, // Voice file assigned but file not found
  AutoMapped,  // Automatically mapped by pattern matching
  Pending      // Waiting for import
};

/**
 * @brief Represents a single dialogue line that can have voice
 */
struct DialogueLine {
  std::string id;          // Unique line ID (e.g., "scene_intro_001")
  std::string sceneId;     // Scene containing this line
  std::string characterId; // Speaking character
  std::string text;        // Dialogue text
  u32 lineNumber;          // Source line number
  std::string voiceFile;   // Bound voice file path (empty if unbound)
  VoiceBindingStatus status = VoiceBindingStatus::Unbound;
  f32 voiceDuration = 0.0f; // Cached duration in seconds
};

/**
 * @brief Voice file entry in the asset database
 */
struct VoiceFileEntry {
  std::string path;          // Full path to voice file
  std::string relativePath;  // Relative path from assets
  std::string filename;      // Just the filename
  f32 duration = 0.0f;       // Duration in seconds
  u64 fileSize = 0;          // File size in bytes
  u64 modifiedTimestamp = 0; // Last modification time
  bool bound = false;        // Whether this file is bound to a line
  std::string boundLineId;   // ID of line this is bound to
};

/**
 * @brief Pattern for automatic voice file mapping
 */
struct VoiceMappingPattern {
  std::string name;           // Pattern name (e.g., "Character_LineID")
  std::string pattern;        // Regex pattern for filename matching
  std::string characterGroup; // Regex group for character extraction
  std::string lineIdGroup;    // Regex group for line ID extraction
  bool enabled = true;
  i32 priority = 0; // Higher priority patterns are tried first
};

/**
 * @brief Voice coverage statistics
 */
struct VoiceCoverageStats {
  u32 totalLines = 0;         // Total dialogue lines
  u32 boundLines = 0;         // Lines with voice bound
  u32 unboundLines = 0;       // Lines without voice
  u32 missingFiles = 0;       // Lines with missing voice files
  f32 coveragePercent = 0.0f; // Percentage covered

  // Per-character stats
  std::unordered_map<std::string, u32> linesByCharacter;
  std::unordered_map<std::string, u32> boundByCharacter;
};

/**
 * @brief Filter options for the voice line list
 */
struct VoiceLineFilter {
  std::string characterFilter; // Filter by character (empty = all)
  std::string sceneFilter;     // Filter by scene (empty = all)
  VoiceBindingStatus statusFilter = VoiceBindingStatus::Unbound; // Status filter
  bool showAllStatuses = true;                                   // If true, ignore statusFilter
  std::string searchText;                                        // Text search in dialogue
};

/**
 * @brief Import/export format for voice assignments
 */
enum class VoiceTableFormat : u8 {
  CSV,  // Comma-separated values
  JSON, // JSON format
  TSV   // Tab-separated values
};

/**
 * @brief Callback types for voice manager events
 */
using OnVoiceBindingChanged =
    std::function<void(const std::string& lineId, const std::string& voiceFile)>;
using OnAutoMappingComplete = std::function<void(u32 mapped, u32 failed)>;
using OnVoicePreviewStart = std::function<void(const std::string& lineId)>;
using OnVoicePreviewStop = std::function<void()>;

/**
 * @brief Voice Manager - Comprehensive voice-over management
 *
 * The Voice Manager provides a complete workflow for managing voice-over
 * in visual novels. It supports:
 *
 * 1. Automatic mapping based on filename patterns:
 *    - hero_01_hello.ogg -> Character "hero", Line ID "01_hello"
 *    - narrator_12_explain.ogg -> Character "narrator", Line ID "12_explain"
 *
 * 2. Manual assignment through drag-and-drop or browser
 *
 * 3. Preview playback directly in the editor
 *
 * 4. Export/import for external workflow (voice studio, translators)
 *
 * Example usage:
 * @code
 * VoiceManager manager(audioManager);
 * manager.loadProject("/path/to/project");
 *
 * // Auto-map voice files
 * auto result = manager.autoMapVoiceFiles();
 *
 * // Get unbound lines
 * auto unbound = manager.getUnboundLines();
 *
 * // Manually bind a voice file
 * manager.bindVoice("line_001", "voice/hero/hello.ogg");
 *
 * // Preview a voice line
 * manager.previewVoice("line_001");
 * @endcode
 */
class VoiceManager {
public:
  explicit VoiceManager(audio::AudioManager* audioManager);
  ~VoiceManager();

  // Non-copyable
  VoiceManager(const VoiceManager&) = delete;
  VoiceManager& operator=(const VoiceManager&) = delete;

  // =========================================================================
  // Project Loading
  // =========================================================================

  /**
   * @brief Load voice data from a project
   * @param projectPath Path to the project directory
   * @return Success or error
   */
  Result<void> loadProject(const std::string& projectPath);

  /**
   * @brief Refresh voice files from disk
   */
  Result<void> refreshVoiceFiles();

  /**
   * @brief Refresh dialogue lines from scripts
   */
  Result<void> refreshDialogueLines();

  /**
   * @brief Check if project is loaded
   */
  [[nodiscard]] bool isProjectLoaded() const { return m_projectLoaded; }

  // =========================================================================
  // Dialogue Lines
  // =========================================================================

  /**
   * @brief Get all dialogue lines
   */
  [[nodiscard]] const std::vector<DialogueLine>& getAllLines() const { return m_dialogueLines; }

  /**
   * @brief Get dialogue lines with filter
   */
  [[nodiscard]] std::vector<const DialogueLine*>
  getFilteredLines(const VoiceLineFilter& filter) const;

  /**
   * @brief Get unbound dialogue lines
   */
  [[nodiscard]] std::vector<const DialogueLine*> getUnboundLines() const;

  /**
   * @brief Get dialogue line by ID
   */
  [[nodiscard]] const DialogueLine* getLine(const std::string& lineId) const;

  /**
   * @brief Get lines for a specific character
   */
  [[nodiscard]] std::vector<const DialogueLine*>
  getLinesForCharacter(const std::string& characterId) const;

  /**
   * @brief Get lines for a specific scene
   */
  [[nodiscard]] std::vector<const DialogueLine*> getLinesForScene(const std::string& sceneId) const;

  // =========================================================================
  // Voice Files
  // =========================================================================

  /**
   * @brief Get all voice files
   */
  [[nodiscard]] const std::vector<VoiceFileEntry>& getVoiceFiles() const { return m_voiceFiles; }

  /**
   * @brief Get unbound voice files
   */
  [[nodiscard]] std::vector<const VoiceFileEntry*> getUnboundVoiceFiles() const;

  /**
   * @brief Get voice file by path
   */
  [[nodiscard]] const VoiceFileEntry* getVoiceFile(const std::string& path) const;

  // =========================================================================
  // Voice Binding
  // =========================================================================

  /**
   * @brief Bind a voice file to a dialogue line
   * @param lineId Dialogue line ID
   * @param voicePath Path to voice file
   * @return Success or error
   */
  Result<void> bindVoice(const std::string& lineId, const std::string& voicePath);

  /**
   * @brief Unbind voice from a dialogue line
   * @param lineId Dialogue line ID
   */
  void unbindVoice(const std::string& lineId);

  /**
   * @brief Clear all voice bindings
   */
  void clearAllBindings();

  // =========================================================================
  // Auto-Mapping
  // =========================================================================

  /**
   * @brief Get configured mapping patterns
   */
  [[nodiscard]] const std::vector<VoiceMappingPattern>& getMappingPatterns() const {
    return m_mappingPatterns;
  }

  /**
   * @brief Add a mapping pattern
   */
  void addMappingPattern(const VoiceMappingPattern& pattern);

  /**
   * @brief Remove a mapping pattern
   */
  void removeMappingPattern(const std::string& name);

  /**
   * @brief Set default mapping patterns
   */
  void setDefaultPatterns();

  /**
   * @brief Automatically map voice files to dialogue lines
   * @return Number of successfully mapped files
   */
  Result<u32> autoMapVoiceFiles();

  /**
   * @brief Preview auto-mapping without applying
   * @return Map of line ID to suggested voice file
   */
  [[nodiscard]] std::unordered_map<std::string, std::string> previewAutoMapping() const;

  // =========================================================================
  // Voice Preview
  // =========================================================================

  /**
   * @brief Preview a voice line
   * @param lineId Dialogue line ID to preview
   */
  void previewVoice(const std::string& lineId);

  /**
   * @brief Preview a voice file directly
   * @param voicePath Path to voice file
   */
  void previewVoiceFile(const std::string& voicePath);

  /**
   * @brief Stop voice preview
   */
  void stopPreview();

  /**
   * @brief Check if preview is playing
   */
  [[nodiscard]] bool isPreviewPlaying() const { return m_previewPlaying; }

  /**
   * @brief Get currently previewing line ID
   */
  [[nodiscard]] const std::string& getPreviewingLineId() const { return m_previewingLineId; }

  // =========================================================================
  // Statistics
  // =========================================================================

  /**
   * @brief Get voice coverage statistics
   */
  [[nodiscard]] VoiceCoverageStats getCoverageStats() const;

  /**
   * @brief Get list of characters in the project
   */
  [[nodiscard]] std::vector<std::string> getCharacters() const;

  /**
   * @brief Get list of scenes in the project
   */
  [[nodiscard]] std::vector<std::string> getScenes() const;

  // =========================================================================
  // Import/Export
  // =========================================================================

  /**
   * @brief Export voice assignments to file
   * @param outputPath Path to output file
   * @param format Export format
   * @return Success or error
   */
  Result<void> exportVoiceTable(const std::string& outputPath, VoiceTableFormat format) const;

  /**
   * @brief Import voice assignments from file
   * @param inputPath Path to input file
   * @param format Import format
   * @return Success or error
   */
  Result<void> importVoiceTable(const std::string& inputPath, VoiceTableFormat format);

  /**
   * @brief Export unbound lines for recording
   * @param outputPath Path to output file
   * @param format Export format
   * @return Success or error
   */
  Result<void> exportUnboundLines(const std::string& outputPath, VoiceTableFormat format) const;

  // =========================================================================
  // Save/Load
  // =========================================================================

  /**
   * @brief Save voice bindings to project
   */
  Result<void> saveBindings();

  /**
   * @brief Load voice bindings from project
   */
  Result<void> loadBindings();

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnVoiceBindingChanged(OnVoiceBindingChanged callback);
  void setOnAutoMappingComplete(OnAutoMappingComplete callback);
  void setOnVoicePreviewStart(OnVoicePreviewStart callback);
  void setOnVoicePreviewStop(OnVoicePreviewStop callback);

private:
  // Internal helpers
  void scanVoiceDirectory(const std::string& path);
  void parseDialogueFromScript(const std::string& scriptPath);
  bool matchPattern(const std::string& filename, const VoiceMappingPattern& pattern,
                    std::string& outCharacter, std::string& outLineId) const;
  f32 getAudioDuration(const std::string& path) const;
  void fireBindingChanged(const std::string& lineId, const std::string& voiceFile);

  // Export helpers
  Result<void> exportCSV(const std::string& path,
                         const std::vector<const DialogueLine*>& lines) const;
  Result<void> exportJSON(const std::string& path,
                          const std::vector<const DialogueLine*>& lines) const;
  Result<void> exportTSV(const std::string& path,
                         const std::vector<const DialogueLine*>& lines) const;

  // Import helpers
  Result<void> importCSV(const std::string& path);
  Result<void> importJSON(const std::string& path);
  Result<void> importTSV(const std::string& path);

  // Audio manager (not owned)
  audio::AudioManager* m_audioManager = nullptr;

  // Project state
  std::string m_projectPath;
  std::string m_voiceAssetsPath;
  bool m_projectLoaded = false;

  // Data
  std::vector<DialogueLine> m_dialogueLines;
  std::vector<VoiceFileEntry> m_voiceFiles;
  std::vector<VoiceMappingPattern> m_mappingPatterns;

  // Lookup maps for fast access
  std::unordered_map<std::string, size_t> m_lineIdToIndex;
  std::unordered_map<std::string, size_t> m_voicePathToIndex;

  // Preview state
  bool m_previewPlaying = false;
  std::string m_previewingLineId;
  audio::AudioHandle m_previewHandle;

  // Callbacks
  OnVoiceBindingChanged m_onBindingChanged;
  OnAutoMappingComplete m_onAutoMappingComplete;
  OnVoicePreviewStart m_onPreviewStart;
  OnVoicePreviewStop m_onPreviewStop;
};

} // namespace NovelMind::editor
