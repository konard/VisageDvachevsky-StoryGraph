#pragma once

/**
 * @file project_json.hpp
 * @brief Robust JSON parser/serializer for project metadata
 *
 * This module provides:
 * - Strict JSON parsing with validation
 * - Atomic file writes (tmp + rename)
 * - Version migration support
 * - Explicit error codes
 * - No data loss guarantees
 *
 * Design philosophy:
 * - Use custom JSON handling to match existing codebase patterns
 * - Fail loudly on parsing errors (no silent fallbacks)
 * - Validate all data before accepting
 * - Support forward/backward compatibility via versioning
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Error codes for project JSON operations
 */
enum class ProjectJsonError : u32 {
  // Parse errors (1xx)
  ParseError = 100,
  InvalidJsonSyntax = 101,
  UnexpectedToken = 102,
  MissingRequiredField = 103,
  InvalidFieldType = 104,
  InvalidFieldValue = 105,

  // Validation errors (2xx)
  ValidationError = 200,
  InvalidVersion = 201,
  UnsupportedVersion = 202,
  InvalidName = 203,
  InvalidPath = 204,
  InvalidTimestamp = 205,

  // I/O errors (3xx)
  IoError = 300,
  FileNotFound = 301,
  FileOpenFailed = 302,
  FileWriteFailed = 303,
  FileReadFailed = 304,
  AtomicWriteFailed = 305,

  // Migration errors (4xx)
  MigrationError = 400,
  MigrationFailed = 401,
  DowngradeFailed = 402
};

/**
 * @brief Convert error code to string
 */
[[nodiscard]] const char* projectJsonErrorToString(ProjectJsonError error);

/**
 * @brief JSON value type for simple parsing
 */
using JsonValue =
    std::variant<std::nullptr_t, bool, i64, f64, std::string, std::vector<std::string>>;

/**
 * @brief Simple JSON object representation
 */
using JsonObject = std::unordered_map<std::string, JsonValue>;

/**
 * @brief Project JSON parser/serializer
 *
 * Provides strict parsing with validation and atomic writes.
 * Uses custom JSON parsing to match existing codebase patterns.
 */
class ProjectJsonHandler {
public:
  /**
   * @brief Parse project metadata from JSON string
   * @param json JSON content
   * @param outMetadata Parsed metadata
   * @return Success or specific error code
   */
  [[nodiscard]] static Result<void> parseFromString(const std::string& json,
                                                    ProjectMetadata& outMetadata);

  /**
   * @brief Serialize project metadata to JSON string
   * @param metadata Metadata to serialize
   * @param outJson Resulting JSON string
   * @return Success or specific error code
   */
  [[nodiscard]] static Result<void> serializeToString(const ProjectMetadata& metadata,
                                                      std::string& outJson);

  /**
   * @brief Load project metadata from file
   * @param path Path to project.json
   * @param outMetadata Parsed metadata
   * @return Success or specific error code
   */
  [[nodiscard]] static Result<void> loadFromFile(const std::string& path,
                                                 ProjectMetadata& outMetadata);

  /**
   * @brief Save project metadata to file atomically
   * @param path Path to project.json
   * @param metadata Metadata to save
   * @return Success or specific error code
   *
   * Atomic write process:
   * 1. Write to temporary file (.tmp)
   * 2. Verify write succeeded
   * 3. Rename to final location
   * This ensures no corruption on write failures
   */
  [[nodiscard]] static Result<void> saveToFile(const std::string& path,
                                               const ProjectMetadata& metadata);

  /**
   * @brief Validate project metadata
   * @param metadata Metadata to validate
   * @return Success or specific validation error
   */
  [[nodiscard]] static Result<void> validate(const ProjectMetadata& metadata);

  /**
   * @brief Get supported project file version
   */
  [[nodiscard]] static constexpr u32 getCurrentVersion() { return 1; }

  /**
   * @brief Get minimum supported version for backward compatibility
   */
  [[nodiscard]] static constexpr u32 getMinSupportedVersion() { return 1; }

private:
  // JSON parsing helpers
  [[nodiscard]] static Result<JsonObject> parseJson(const std::string& json);
  [[nodiscard]] static std::string escapeJsonString(const std::string& str);
  [[nodiscard]] static std::string unescapeJsonString(const std::string& str);

  // Field extraction helpers
  template <typename T>
  [[nodiscard]] static Result<T> getField(const JsonObject& obj, const std::string& key);

  template <typename T>
  [[nodiscard]] static Result<T> getOptionalField(const JsonObject& obj, const std::string& key,
                                                  const T& defaultValue);

  // Migration support
  [[nodiscard]] static Result<void> migrateFromVersion(u32 version, JsonObject& obj);
};

} // namespace NovelMind::editor
