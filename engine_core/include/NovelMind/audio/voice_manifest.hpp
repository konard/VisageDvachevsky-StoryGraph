#pragma once

/**
 * @file voice_manifest.hpp
 * @brief Voice Manifest System - Structured voice line management
 *
 * Provides a comprehensive voice authoring format replacing unclear CSV:
 * - Explicit JSON/YAML manifest format with clear field definitions
 * - Multi-locale support for voice files
 * - Take management for recording workflow
 * - Status tracking (missing, recorded, imported, needs review)
 * - Validation and schema enforcement
 * - Legacy CSV import/export for backwards compatibility
 *
 * Example manifest format:
 * @code
 * {
 *   "project": "my_visual_novel",
 *   "default_locale": "en",
 *   "locales": ["en", "ru"],
 *   "naming_convention": "{locale}/{id}.ogg",
 *   "lines": [
 *     {
 *       "id": "intro.alex.001",
 *       "speaker": "alex",
 *       "text_key": "dialog.intro.alex.001",
 *       "scene": "intro",
 *       "take": 1,
 *       "files": {
 *         "en": "assets/audio/voice/en/intro.alex.001.ogg",
 *         "ru": "assets/audio/voice/ru/intro.alex.001.ogg"
 *       },
 *       "tags": ["main", "calm"]
 *     }
 *   ]
 * }
 * @endcode
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::audio {

/**
 * @brief Voice line recording/import status
 */
enum class VoiceLineStatus : u8 {
  Missing,     // File does not exist
  Recorded,    // Recorded directly in editor
  Imported,    // Imported from external source
  NeedsReview, // Flagged for review
  Approved     // Reviewed and approved
};

/**
 * @brief Convert status to string
 */
[[nodiscard]] inline const char *
voiceLineStatusToString(VoiceLineStatus status) {
  switch (status) {
  case VoiceLineStatus::Missing:
    return "missing";
  case VoiceLineStatus::Recorded:
    return "recorded";
  case VoiceLineStatus::Imported:
    return "imported";
  case VoiceLineStatus::NeedsReview:
    return "needs_review";
  case VoiceLineStatus::Approved:
    return "approved";
  }
  return "unknown";
}

/**
 * @brief Parse status from string
 */
[[nodiscard]] inline VoiceLineStatus
voiceLineStatusFromString(const std::string &str) {
  if (str == "missing")
    return VoiceLineStatus::Missing;
  if (str == "recorded")
    return VoiceLineStatus::Recorded;
  if (str == "imported")
    return VoiceLineStatus::Imported;
  if (str == "needs_review")
    return VoiceLineStatus::NeedsReview;
  if (str == "approved")
    return VoiceLineStatus::Approved;
  return VoiceLineStatus::Missing;
}

/**
 * @brief Recording take information
 */
struct VoiceTake {
  u32 takeNumber = 1;        // Take number (1, 2, 3...)
  std::string filePath;      // Path to take audio file
  u64 recordedTimestamp = 0; // Unix timestamp of recording
  f32 duration = 0.0f;       // Duration in seconds
  bool isActive = false;     // Is this the active/selected take
  std::string notes;         // Actor/director notes for this take
};

/**
 * @brief Voice line locale-specific file mapping
 */
struct VoiceLocaleFile {
  std::string locale;   // Locale ID (e.g., "en", "ru")
  std::string filePath; // Path to audio file
  VoiceLineStatus status = VoiceLineStatus::Missing;
  f32 duration = 0.0f;          // Cached duration in seconds
  u32 sampleRate = 0;           // Audio sample rate
  u8 channels = 0;              // Number of audio channels
  f32 loudnessLUFS = 0.0f;      // Loudness in LUFS (if available)
  std::vector<VoiceTake> takes; // All recording takes
  u32 activeTakeIndex = 0;      // Index of active take
};

/**
 * @brief Single voice line entry in the manifest
 */
struct VoiceManifestLine {
  // Required fields (MUST)
  std::string id;      // Unique voice line ID (e.g., "intro.alex.001")
  std::string textKey; // Localization key for the dialogue text

  // Recommended fields
  std::string speaker; // Speaker/character ID for filtering
  std::string scene;   // Scene identifier

  // Optional fields
  std::vector<std::string>
      tags;                    // Tags for organization (e.g., "calm", "angry")
  std::string notes;           // Notes for actors/directors
  f32 durationOverride = 0.0f; // Manual duration override (0 = use actual)

  // File mappings per locale
  std::unordered_map<std::string, VoiceLocaleFile> files;

  // Source reference
  std::string sourceScript; // Script file this line comes from
  u32 sourceLine = 0;       // Line number in source script

  /**
   * @brief Get file for a specific locale
   */
  [[nodiscard]] const VoiceLocaleFile *
  getFile(const std::string &locale) const {
    auto it = files.find(locale);
    return (it != files.end()) ? &it->second : nullptr;
  }

  /**
   * @brief Get mutable file for a specific locale, creating if needed
   */
  VoiceLocaleFile &getOrCreateFile(const std::string &locale) {
    auto it = files.find(locale);
    if (it != files.end()) {
      return it->second;
    }
    files[locale].locale = locale;
    return files[locale];
  }

  /**
   * @brief Check if file exists for locale
   */
  [[nodiscard]] bool hasFile(const std::string &locale) const {
    auto file = getFile(locale);
    return file && file->status != VoiceLineStatus::Missing;
  }

  /**
   * @brief Get overall status across all locales (worst status)
   */
  [[nodiscard]] VoiceLineStatus getOverallStatus() const {
    if (files.empty())
      return VoiceLineStatus::Missing;

    bool hasMissing = false;
    bool hasNeedsReview = false;
    bool allApproved = true;

    for (const auto &[locale, file] : files) {
      if (file.status == VoiceLineStatus::Missing)
        hasMissing = true;
      if (file.status == VoiceLineStatus::NeedsReview)
        hasNeedsReview = true;
      if (file.status != VoiceLineStatus::Approved)
        allApproved = false;
    }

    if (hasMissing)
      return VoiceLineStatus::Missing;
    if (hasNeedsReview)
      return VoiceLineStatus::NeedsReview;
    if (allApproved)
      return VoiceLineStatus::Approved;
    return VoiceLineStatus::Recorded;
  }
};

/**
 * @brief Naming convention templates
 */
struct NamingConvention {
  std::string pattern;     // Pattern template (e.g., "{locale}/{id}.ogg")
  std::string description; // Human-readable description

  /**
   * @brief Generate path from pattern and values
   */
  [[nodiscard]] std::string generatePath(const std::string &locale,
                                         const std::string &id,
                                         const std::string &scene = "",
                                         const std::string &speaker = "",
                                         u32 take = 1) const;

  /**
   * @brief Predefined naming conventions
   */
  static NamingConvention localeIdBased() {
    return {"{locale}/{id}.ogg", "Locale folder with ID filename"};
  }

  static NamingConvention sceneSpeakerBased() {
    return {"{scene}/{speaker}/{id}_take{take}.ogg",
            "Scene/Speaker folders with take"};
  }

  static NamingConvention flatById() {
    return {"voice/{id}_{locale}.ogg", "Flat folder with locale suffix"};
  }
};

/**
 * @brief Validation error for manifest
 */
struct ManifestValidationError {
  enum class Type : u8 {
    DuplicateId,          // Voice line ID is not unique
    MissingRequiredField, // Required field is missing
    InvalidLocale,        // Locale not in manifest's locale list
    FileNotFound,         // Referenced file does not exist
    InvalidFilePath,      // Path is malformed
    PathConflict          // Multiple lines point to same file
  };

  Type type;
  std::string lineId;
  std::string field;
  std::string message;
};

/**
 * @brief Voice Manifest - Central voice line database
 *
 * The Voice Manifest provides structured management of voice lines,
 * replacing the unclear CSV format with an explicit, validated format.
 *
 * Features:
 * - Clear field definitions with required/optional marking
 * - Multi-locale support
 * - Take management for recording workflow
 * - Status tracking for production pipeline
 * - Validation with detailed error reporting
 * - Legacy CSV import/export
 *
 * Example usage:
 * @code
 * VoiceManifest manifest;
 * manifest.setProjectName("my_visual_novel");
 * manifest.setDefaultLocale("en");
 * manifest.addLocale("en");
 * manifest.addLocale("ru");
 *
 * // Add a voice line
 * VoiceManifestLine line;
 * line.id = "intro.alex.001";
 * line.textKey = "dialog.intro.alex.001";
 * line.speaker = "alex";
 * line.scene = "intro";
 * manifest.addLine(line);
 *
 * // Save manifest
 * manifest.saveToFile("voice_manifest.json");
 * @endcode
 */
class VoiceManifest {
public:
  VoiceManifest();
  ~VoiceManifest();

  // Non-copyable
  VoiceManifest(const VoiceManifest &) = delete;
  VoiceManifest &operator=(const VoiceManifest &) = delete;

  // Move semantics
  VoiceManifest(VoiceManifest &&) noexcept;
  VoiceManifest &operator=(VoiceManifest &&) noexcept;

  // =========================================================================
  // Project Configuration
  // =========================================================================

  /**
   * @brief Set project name
   */
  void setProjectName(const std::string &name);

  /**
   * @brief Get project name
   */
  [[nodiscard]] const std::string &getProjectName() const {
    return m_projectName;
  }

  /**
   * @brief Set default locale
   */
  void setDefaultLocale(const std::string &locale);

  /**
   * @brief Get default locale
   */
  [[nodiscard]] const std::string &getDefaultLocale() const {
    return m_defaultLocale;
  }

  /**
   * @brief Add a supported locale
   */
  void addLocale(const std::string &locale);

  /**
   * @brief Remove a locale
   */
  void removeLocale(const std::string &locale);

  /**
   * @brief Get all supported locales
   */
  [[nodiscard]] const std::vector<std::string> &getLocales() const {
    return m_locales;
  }

  /**
   * @brief Check if locale is supported
   */
  [[nodiscard]] bool hasLocale(const std::string &locale) const;

  /**
   * @brief Set naming convention
   */
  void setNamingConvention(const NamingConvention &convention);

  /**
   * @brief Get naming convention
   */
  [[nodiscard]] const NamingConvention &getNamingConvention() const {
    return m_namingConvention;
  }

  /**
   * @brief Set base path for voice assets
   */
  void setBasePath(const std::string &path);

  /**
   * @brief Get base path for voice assets
   */
  [[nodiscard]] const std::string &getBasePath() const { return m_basePath; }

  // =========================================================================
  // Voice Lines
  // =========================================================================

  /**
   * @brief Add a voice line
   * @param line Voice line to add
   * @return Success or error if ID already exists
   */
  Result<void> addLine(const VoiceManifestLine &line);

  /**
   * @brief Update an existing voice line
   * @param line Voice line with updated data
   * @return Success or error if ID not found
   */
  Result<void> updateLine(const VoiceManifestLine &line);

  /**
   * @brief Remove a voice line
   * @param lineId Voice line ID to remove
   */
  void removeLine(const std::string &lineId);

  /**
   * @brief Get a voice line by ID
   * @param lineId Voice line ID
   * @return Pointer to line or nullptr if not found
   */
  [[nodiscard]] const VoiceManifestLine *
  getLine(const std::string &lineId) const;

  /**
   * @brief Get mutable voice line by ID
   */
  VoiceManifestLine *getLineMutable(const std::string &lineId);

  /**
   * @brief Get all voice lines
   */
  [[nodiscard]] const std::vector<VoiceManifestLine> &getLines() const {
    return m_lines;
  }

  /**
   * @brief Get lines filtered by speaker
   */
  [[nodiscard]] std::vector<const VoiceManifestLine *>
  getLinesBySpeaker(const std::string &speaker) const;

  /**
   * @brief Get lines filtered by scene
   */
  [[nodiscard]] std::vector<const VoiceManifestLine *>
  getLinesByScene(const std::string &scene) const;

  /**
   * @brief Get lines filtered by status
   */
  [[nodiscard]] std::vector<const VoiceManifestLine *>
  getLinesByStatus(VoiceLineStatus status,
                   const std::string &locale = "") const;

  /**
   * @brief Get lines filtered by tag
   */
  [[nodiscard]] std::vector<const VoiceManifestLine *>
  getLinesByTag(const std::string &tag) const;

  /**
   * @brief Get all unique speakers
   */
  [[nodiscard]] std::vector<std::string> getSpeakers() const;

  /**
   * @brief Get all unique scenes
   */
  [[nodiscard]] std::vector<std::string> getScenes() const;

  /**
   * @brief Get all unique tags
   */
  [[nodiscard]] std::vector<std::string> getTags() const;

  /**
   * @brief Get number of lines
   */
  [[nodiscard]] size_t getLineCount() const { return m_lines.size(); }

  /**
   * @brief Check if line exists
   */
  [[nodiscard]] bool hasLine(const std::string &lineId) const;

  /**
   * @brief Clear all lines
   */
  void clearLines();

  // =========================================================================
  // Take Management
  // =========================================================================

  /**
   * @brief Add a take to a voice line
   * @param lineId Voice line ID
   * @param locale Locale for the take
   * @param take Take information
   * @return Success or error
   */
  Result<void> addTake(const std::string &lineId, const std::string &locale,
                       const VoiceTake &take);

  /**
   * @brief Set active take for a voice line
   * @param lineId Voice line ID
   * @param locale Locale
   * @param takeIndex Index of the take to activate
   * @return Success or error
   */
  Result<void> setActiveTake(const std::string &lineId,
                             const std::string &locale, u32 takeIndex);

  /**
   * @brief Get all takes for a voice line
   * @param lineId Voice line ID
   * @param locale Locale
   * @return Vector of takes or empty if not found
   */
  [[nodiscard]] std::vector<VoiceTake>
  getTakes(const std::string &lineId, const std::string &locale) const;

  /**
   * @brief Remove a take from a voice line
   * @param lineId Voice line ID
   * @param locale Locale
   * @param takeNumber Take number to remove
   * @return Success or error
   */
  Result<void> removeTake(const std::string &lineId, const std::string &locale,
                          u32 takeNumber);

  // =========================================================================
  // Status Management
  // =========================================================================

  /**
   * @brief Set status for a voice line locale
   */
  Result<void> setStatus(const std::string &lineId, const std::string &locale,
                         VoiceLineStatus status);

  /**
   * @brief Mark file as recorded
   */
  Result<void> markAsRecorded(const std::string &lineId,
                              const std::string &locale,
                              const std::string &filePath);

  /**
   * @brief Mark file as imported
   */
  Result<void> markAsImported(const std::string &lineId,
                              const std::string &locale,
                              const std::string &filePath);

  // =========================================================================
  // Validation
  // =========================================================================

  /**
   * @brief Validate the manifest
   * @param checkFiles If true, verify that files exist on disk
   * @return Vector of validation errors (empty if valid)
   */
  [[nodiscard]] std::vector<ManifestValidationError>
  validate(bool checkFiles = false) const;

  /**
   * @brief Check if manifest is valid
   */
  [[nodiscard]] bool isValid(bool checkFiles = false) const {
    return validate(checkFiles).empty();
  }

  // =========================================================================
  // Statistics
  // =========================================================================

  /**
   * @brief Get coverage statistics for a locale
   */
  struct CoverageStats {
    u32 totalLines = 0;
    u32 recordedLines = 0;
    u32 importedLines = 0;
    u32 missingLines = 0;
    u32 needsReviewLines = 0;
    u32 approvedLines = 0;
    f32 coveragePercent = 0.0f;
    f32 totalDuration = 0.0f;
  };

  [[nodiscard]] CoverageStats
  getCoverageStats(const std::string &locale = "") const;

  // =========================================================================
  // File I/O
  // =========================================================================

  /**
   * @brief Load manifest from JSON file
   */
  Result<void> loadFromFile(const std::string &filePath);

  /**
   * @brief Load manifest from JSON string
   */
  Result<void> loadFromString(const std::string &jsonContent);

  /**
   * @brief Save manifest to JSON file
   */
  Result<void> saveToFile(const std::string &filePath) const;

  /**
   * @brief Export manifest as JSON string
   */
  [[nodiscard]] Result<std::string> toJsonString() const;

  /**
   * @brief Import from legacy CSV format
   * @param csvPath Path to CSV file
   * @param locale Locale for the imported data
   * @return Success or error with details
   */
  Result<void> importFromCsv(const std::string &csvPath,
                             const std::string &locale);

  /**
   * @brief Export to legacy CSV format
   * @param csvPath Output path
   * @param locale Locale to export (empty for all)
   * @return Success or error
   */
  Result<void> exportToCsv(const std::string &csvPath,
                           const std::string &locale = "") const;

  /**
   * @brief Export template manifest for recording
   * This creates a manifest with all lines but no file paths,
   * suitable for voice recording workflow
   */
  Result<void> exportTemplate(const std::string &filePath) const;

  // =========================================================================
  // Generation
  // =========================================================================

  /**
   * @brief Generate manifest from localization keys
   * @param keyPrefix Prefix filter for keys (e.g., "dialog.")
   * @param locManager Localization manager to read keys from
   * @return Number of lines generated
   */
  // Note: Implementation will integrate with LocalizationManager

  /**
   * @brief Generate file paths for all lines using naming convention
   * @param locale Locale to generate paths for
   * @param overwriteExisting If true, regenerate existing paths
   * @return Number of paths generated
   */
  u32 generateFilePaths(const std::string &locale,
                        bool overwriteExisting = false);

  /**
   * @brief Create output directories for the naming convention
   */
  Result<void> createOutputDirectories(const std::string &locale) const;

  // =========================================================================
  // Callbacks
  // =========================================================================

  using OnLineChanged = std::function<void(const std::string &lineId)>;
  using OnStatusChanged =
      std::function<void(const std::string &lineId, const std::string &locale,
                         VoiceLineStatus status)>;

  void setOnLineChanged(OnLineChanged callback);
  void setOnStatusChanged(OnStatusChanged callback);

private:
  // Project metadata
  std::string m_projectName;
  std::string m_defaultLocale = "en";
  std::vector<std::string> m_locales;
  NamingConvention m_namingConvention;
  std::string m_basePath = "assets/audio/voice";

  // Voice lines
  std::vector<VoiceManifestLine> m_lines;
  std::unordered_map<std::string, size_t> m_lineIdToIndex;

  // Callbacks
  OnLineChanged m_onLineChanged;
  OnStatusChanged m_onStatusChanged;

  // Internal helpers
  void rebuildIndex();
  void fireLineChanged(const std::string &lineId);
  void fireStatusChanged(const std::string &lineId, const std::string &locale,
                         VoiceLineStatus status);

  // Security: Path validation
  [[nodiscard]] bool isValidRelativePath(const std::string &path) const;
  [[nodiscard]] Result<std::string>
  sanitizeAndResolvePath(const std::string &relativePath) const;
};

} // namespace NovelMind::audio
