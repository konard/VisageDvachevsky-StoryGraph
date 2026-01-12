#include "NovelMind/editor/project_json.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace NovelMind::editor {

// ============================================================================
// Error code to string
// ============================================================================

const char* projectJsonErrorToString(ProjectJsonError error) {
  switch (error) {
  // Parse errors
  case ProjectJsonError::ParseError:
    return "Parse error";
  case ProjectJsonError::InvalidJsonSyntax:
    return "Invalid JSON syntax";
  case ProjectJsonError::UnexpectedToken:
    return "Unexpected token";
  case ProjectJsonError::MissingRequiredField:
    return "Missing required field";
  case ProjectJsonError::InvalidFieldType:
    return "Invalid field type";
  case ProjectJsonError::InvalidFieldValue:
    return "Invalid field value";

  // Validation errors
  case ProjectJsonError::ValidationError:
    return "Validation error";
  case ProjectJsonError::InvalidVersion:
    return "Invalid version";
  case ProjectJsonError::UnsupportedVersion:
    return "Unsupported version";
  case ProjectJsonError::InvalidName:
    return "Invalid name";
  case ProjectJsonError::InvalidPath:
    return "Invalid path";
  case ProjectJsonError::InvalidTimestamp:
    return "Invalid timestamp";

  // I/O errors
  case ProjectJsonError::IoError:
    return "I/O error";
  case ProjectJsonError::FileNotFound:
    return "File not found";
  case ProjectJsonError::FileOpenFailed:
    return "Failed to open file";
  case ProjectJsonError::FileWriteFailed:
    return "Failed to write file";
  case ProjectJsonError::FileReadFailed:
    return "Failed to read file";
  case ProjectJsonError::AtomicWriteFailed:
    return "Atomic write failed";

  // Migration errors
  case ProjectJsonError::MigrationError:
    return "Migration error";
  case ProjectJsonError::MigrationFailed:
    return "Migration failed";
  case ProjectJsonError::DowngradeFailed:
    return "Downgrade failed";

  default:
    return "Unknown error";
  }
}

// ============================================================================
// JSON String Escaping
// ============================================================================

std::string ProjectJsonHandler::escapeJsonString(const std::string& str) {
  std::string result;
  result.reserve(str.size() * 2); // Reserve extra space for escapes

  for (char c : str) {
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
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    default:
      if (static_cast<unsigned char>(c) < 32) {
        // Control characters - output as \uXXXX
        char buf[7];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        result += buf;
      } else {
        result += c;
      }
      break;
    }
  }

  return result;
}

std::string ProjectJsonHandler::unescapeJsonString(const std::string& str) {
  std::string result;
  result.reserve(str.size());

  bool escape = false;
  for (size_t i = 0; i < str.size(); ++i) {
    char c = str[i];

    if (escape) {
      switch (c) {
      case '"':
        result += '"';
        break;
      case '\\':
        result += '\\';
        break;
      case '/':
        result += '/';
        break;
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      case 'b':
        result += '\b';
        break;
      case 'f':
        result += '\f';
        break;
      case 'u':
        // Unicode escape \uXXXX - simplified handling
        if (i + 4 < str.size()) {
          std::string hex = str.substr(i + 1, 4);
          try {
            int codepoint = std::stoi(hex, nullptr, 16);
            if (codepoint < 128) {
              result += static_cast<char>(codepoint);
            } else {
              // For simplicity, keep the escape sequence
              result += "\\u" + hex;
            }
            i += 4;
          } catch (...) {
            // Invalid hex, keep as is
            result += "\\u";
          }
        } else {
          result += "\\u";
        }
        break;
      default:
        // Invalid escape, keep backslash
        result += '\\';
        result += c;
        break;
      }
      escape = false;
    } else {
      if (c == '\\') {
        escape = true;
      } else {
        result += c;
      }
    }
  }

  return result;
}

// ============================================================================
// JSON Parsing
// ============================================================================

Result<JsonObject> ProjectJsonHandler::parseJson(const std::string& json) {
  JsonObject obj;

  // Simple regex-based parser for flat JSON objects
  // Pattern: "key" : value
  // where value can be: "string", number, true, false, null, or ["array"]

  // Remove whitespace and comments for easier parsing
  std::string cleaned;
  cleaned.reserve(json.size());
  bool inString = false;
  bool escape = false;

  for (char c : json) {
    if (escape) {
      cleaned += c;
      escape = false;
      continue;
    }

    if (c == '\\' && inString) {
      escape = true;
      cleaned += c;
      continue;
    }

    if (c == '"') {
      inString = !inString;
      cleaned += c;
      continue;
    }

    if (!inString && (c == '\n' || c == '\r')) {
      continue;
    }

    cleaned += c;
  }

  // Parse key-value pairs
  // Pattern: "key"\s*:\s*value
  std::regex kvPattern("\"([^\"\\\\]*(\\\\.[^\"\\\\]*)*)\"" // Quoted key
                       "\\s*:\\s*"                          // Colon separator
                       "("                                  // Value alternatives:
                       "\"([^\"\\\\]*(\\\\.[^\"\\\\]*)*)\"" // Quoted string
                       "|(-?\\d+\\.?\\d*)"                  // Number
                       "|true|false|null"                   // Boolean/null
                       "|\\[([^\\]]*)\\]"                   // Array
                       ")");

  auto begin = std::sregex_iterator(cleaned.begin(), cleaned.end(), kvPattern);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it) {
    std::smatch match = *it;

    std::string key = unescapeJsonString(match[1].str());

    // Determine value type by checking if it's a quoted string (match[4])
    // We check if the match was captured, not if it's empty (empty strings are
    // valid!)
    if (match.size() > 4 && match[4].matched) {
      // String value (even if empty)
      obj[key] = unescapeJsonString(match[4].str());
    } else {
      // Not a quoted string, check other types (number, bool, null, array)
      std::string fullValue = match[0].str();
      size_t colonPos = fullValue.find(':');
      if (colonPos != std::string::npos) {
        fullValue = fullValue.substr(colonPos + 1);
        // Trim whitespace
        fullValue.erase(0, fullValue.find_first_not_of(" \t"));
        fullValue.erase(fullValue.find_last_not_of(" \t,}") + 1);
      }

      if (fullValue == "null") {
        obj[key] = nullptr;
      } else if (fullValue == "true") {
        obj[key] = true;
      } else if (fullValue == "false") {
        obj[key] = false;
      } else if (fullValue.front() == '[') {
        // Array of strings
        std::vector<std::string> arr;
        std::regex arrPattern("\"([^\"\\\\]*(\\\\.[^\"\\\\]*)*)\"");
        auto arrBegin = std::sregex_iterator(fullValue.begin(), fullValue.end(), arrPattern);
        auto arrEnd = std::sregex_iterator();
        for (auto arrIt = arrBegin; arrIt != arrEnd; ++arrIt) {
          arr.push_back(unescapeJsonString((*arrIt)[1].str()));
        }
        obj[key] = arr;
      } else {
        // Number
        try {
          if (fullValue.find('.') != std::string::npos) {
            obj[key] = std::stod(fullValue);
          } else {
            obj[key] = static_cast<i64>(std::stoll(fullValue));
          }
        } catch (...) {
          return Result<JsonObject>::error("Invalid number value for key '" + key +
                                           "': " + fullValue);
        }
      }
    }
  }

  return Result<JsonObject>::ok(std::move(obj));
}

// ============================================================================
// Field Extraction Helpers
// ============================================================================

template <>
Result<std::string> ProjectJsonHandler::getField(const JsonObject& obj, const std::string& key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<std::string>::error("Missing required field: " + key);
  }

  if (auto* val = std::get_if<std::string>(&it->second)) {
    return Result<std::string>::ok(*val);
  }

  return Result<std::string>::error("Field '" + key + "' has wrong type (expected string)");
}

template <>
Result<i64> ProjectJsonHandler::getField(const JsonObject& obj, const std::string& key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<i64>::error("Missing required field: " + key);
  }

  if (auto* val = std::get_if<i64>(&it->second)) {
    return Result<i64>::ok(*val);
  }

  return Result<i64>::error("Field '" + key + "' has wrong type (expected integer)");
}

template <>
Result<bool> ProjectJsonHandler::getField(const JsonObject& obj, const std::string& key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<bool>::error("Missing required field: " + key);
  }

  if (auto* val = std::get_if<bool>(&it->second)) {
    return Result<bool>::ok(*val);
  }

  return Result<bool>::error("Field '" + key + "' has wrong type (expected boolean)");
}

template <>
Result<std::vector<std::string>> ProjectJsonHandler::getField(const JsonObject& obj,
                                                              const std::string& key) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<std::vector<std::string>>::error("Missing required field: " + key);
  }

  if (auto* val = std::get_if<std::vector<std::string>>(&it->second)) {
    return Result<std::vector<std::string>>::ok(*val);
  }

  return Result<std::vector<std::string>>::error("Field '" + key +
                                                 "' has wrong type (expected array)");
}

template <>
Result<std::string> ProjectJsonHandler::getOptionalField(const JsonObject& obj,
                                                         const std::string& key,
                                                         const std::string& defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<std::string>::ok(defaultValue);
  }

  if (auto* val = std::get_if<std::string>(&it->second)) {
    return Result<std::string>::ok(*val);
  }

  return Result<std::string>::error("Field '" + key + "' has wrong type (expected string)");
}

template <>
Result<i64> ProjectJsonHandler::getOptionalField(const JsonObject& obj, const std::string& key,
                                                 const i64& defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<i64>::ok(defaultValue);
  }

  // Try i64 first
  if (auto* val = std::get_if<i64>(&it->second)) {
    return Result<i64>::ok(*val);
  }

  // Try f64 and convert if it's a whole number
  if (auto* val = std::get_if<f64>(&it->second)) {
    f64 doubleVal = *val;
    i64 intVal = static_cast<i64>(doubleVal);
    // Check if conversion is safe (no fractional part)
    if (doubleVal == static_cast<f64>(intVal)) {
      return Result<i64>::ok(intVal);
    }
    return Result<i64>::error("Field '" + key + "' is a floating-point number, expected integer");
  }

  return Result<i64>::error("Field '" + key + "' has wrong type (expected integer)");
}

template <>
Result<bool> ProjectJsonHandler::getOptionalField(const JsonObject& obj, const std::string& key,
                                                  const bool& defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<bool>::ok(defaultValue);
  }

  if (auto* val = std::get_if<bool>(&it->second)) {
    return Result<bool>::ok(*val);
  }

  return Result<bool>::error("Field '" + key + "' has wrong type (expected boolean)");
}

template <>
Result<std::vector<std::string>>
ProjectJsonHandler::getOptionalField(const JsonObject& obj, const std::string& key,
                                     const std::vector<std::string>& defaultValue) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    return Result<std::vector<std::string>>::ok(defaultValue);
  }

  if (auto* val = std::get_if<std::vector<std::string>>(&it->second)) {
    return Result<std::vector<std::string>>::ok(*val);
  }

  return Result<std::vector<std::string>>::error("Field '" + key +
                                                 "' has wrong type (expected array)");
}

// ============================================================================
// Parse from String
// ============================================================================

Result<void> ProjectJsonHandler::parseFromString(const std::string& json,
                                                 ProjectMetadata& outMetadata) {
  // Parse JSON
  auto parseResult = parseJson(json);
  if (parseResult.isError()) {
    return Result<void>::error("JSON parse error: " + parseResult.error());
  }

  JsonObject obj = parseResult.value();

  // Extract version and check compatibility
  auto versionResult = getOptionalField<i64>(obj, "fileVersion", 1);
  if (versionResult.isError()) {
    return Result<void>::error("Invalid fileVersion: " + versionResult.error());
  }

  u32 version = static_cast<u32>(versionResult.value());
  if (version > getCurrentVersion()) {
    return Result<void>::error("Unsupported project version " + std::to_string(version) +
                               " (current version: " + std::to_string(getCurrentVersion()) + ")");
  }

  if (version < getMinSupportedVersion()) {
    return Result<void>::error(
        "Project version " + std::to_string(version) +
        " is too old (minimum supported: " + std::to_string(getMinSupportedVersion()) + ")");
  }

  // Migrate if needed
  if (version < getCurrentVersion()) {
    auto migrateResult = migrateFromVersion(version, obj);
    if (migrateResult.isError()) {
      return migrateResult;
    }
  }

  // Extract required fields
  auto nameResult = getField<std::string>(obj, "name");
  if (nameResult.isError()) {
    return Result<void>::error(nameResult.error());
  }
  outMetadata.name = nameResult.value();

  // Extract optional fields with defaults
  auto versionField = getOptionalField<std::string>(obj, "version", "1.0.0");
  if (versionField.isError()) {
    return Result<void>::error(versionField.error());
  }
  outMetadata.version = versionField.value();

  auto authorField = getOptionalField<std::string>(obj, "author", "");
  if (authorField.isError()) {
    return Result<void>::error(authorField.error());
  }
  outMetadata.author = authorField.value();

  auto descriptionField = getOptionalField<std::string>(obj, "description", "");
  if (descriptionField.isError()) {
    return Result<void>::error(descriptionField.error());
  }
  outMetadata.description = descriptionField.value();

  auto engineVersionField = getOptionalField<std::string>(obj, "engineVersion", "0.2.0");
  if (engineVersionField.isError()) {
    return Result<void>::error(engineVersionField.error());
  }
  outMetadata.engineVersion = engineVersionField.value();

  auto startSceneField = getOptionalField<std::string>(obj, "startScene", "");
  if (startSceneField.isError()) {
    return Result<void>::error(startSceneField.error());
  }
  outMetadata.startScene = startSceneField.value();

  auto defaultLocaleField = getOptionalField<std::string>(obj, "defaultLocale", "en");
  if (defaultLocaleField.isError()) {
    return Result<void>::error(defaultLocaleField.error());
  }
  outMetadata.defaultLocale = defaultLocaleField.value();

  auto targetResolutionField = getOptionalField<std::string>(obj, "targetResolution", "1920x1080");
  if (targetResolutionField.isError()) {
    return Result<void>::error(targetResolutionField.error());
  }
  outMetadata.targetResolution = targetResolutionField.value();

  auto fullscreenDefaultField = getOptionalField<bool>(obj, "fullscreenDefault", false);
  if (fullscreenDefaultField.isError()) {
    return Result<void>::error(fullscreenDefaultField.error());
  }
  outMetadata.fullscreenDefault = fullscreenDefaultField.value();

  auto buildPresetField = getOptionalField<std::string>(obj, "buildPreset", "release");
  if (buildPresetField.isError()) {
    return Result<void>::error(buildPresetField.error());
  }
  outMetadata.buildPreset = buildPresetField.value();

  // Timestamps
  auto createdAtField = getOptionalField<i64>(obj, "createdAt", 0);
  if (createdAtField.isError()) {
    return Result<void>::error(createdAtField.error());
  }
  outMetadata.createdAt = static_cast<u64>(createdAtField.value());

  auto modifiedAtField = getOptionalField<i64>(obj, "modifiedAt", 0);
  if (modifiedAtField.isError()) {
    return Result<void>::error(modifiedAtField.error());
  }
  outMetadata.modifiedAt = static_cast<u64>(modifiedAtField.value());

  auto lastOpenedAtField = getOptionalField<i64>(obj, "lastOpenedAt", 0);
  if (lastOpenedAtField.isError()) {
    return Result<void>::error(lastOpenedAtField.error());
  }
  outMetadata.lastOpenedAt = static_cast<u64>(lastOpenedAtField.value());

  // Target platforms (array)
  auto platformsResult = getOptionalField<std::vector<std::string>>(
      obj, "targetPlatforms", std::vector<std::string>{"windows", "linux", "macos"});
  if (platformsResult.isError()) {
    return Result<void>::error(platformsResult.error());
  }
  outMetadata.targetPlatforms = platformsResult.value();

  // Playback source mode (issue #82, #100)
  auto playbackModeResult = getOptionalField<std::string>(obj, "playbackSourceMode", "Script");
  if (playbackModeResult.isError()) {
    return Result<void>::error(playbackModeResult.error());
  }
  std::string modeStr = playbackModeResult.value();
  if (modeStr == "Graph") {
    outMetadata.playbackSourceMode = PlaybackSourceMode::Graph;
  } else if (modeStr == "Mixed") {
    outMetadata.playbackSourceMode = PlaybackSourceMode::Mixed;
  } else {
    outMetadata.playbackSourceMode = PlaybackSourceMode::Script;
  }

  // Validate
  auto validateResult = validate(outMetadata);
  if (validateResult.isError()) {
    return validateResult;
  }

  return Result<void>::ok();
}

// ============================================================================
// Serialize to String
// ============================================================================

Result<void> ProjectJsonHandler::serializeToString(const ProjectMetadata& metadata,
                                                   std::string& outJson) {
  // Validate before serializing
  auto validateResult = validate(metadata);
  if (validateResult.isError()) {
    return validateResult;
  }

  std::ostringstream json;

  json << "{\n";
  json << "  \"fileVersion\": " << getCurrentVersion() << ",\n";
  json << "  \"name\": \"" << escapeJsonString(metadata.name) << "\",\n";
  json << "  \"version\": \"" << escapeJsonString(metadata.version) << "\",\n";
  json << "  \"author\": \"" << escapeJsonString(metadata.author) << "\",\n";
  json << "  \"description\": \"" << escapeJsonString(metadata.description) << "\",\n";
  json << "  \"engineVersion\": \"" << escapeJsonString(metadata.engineVersion) << "\",\n";
  json << "  \"createdAt\": " << metadata.createdAt << ",\n";
  json << "  \"modifiedAt\": " << metadata.modifiedAt << ",\n";
  json << "  \"lastOpenedAt\": " << metadata.lastOpenedAt << ",\n";
  json << "  \"startScene\": \"" << escapeJsonString(metadata.startScene) << "\",\n";
  json << "  \"defaultLocale\": \"" << escapeJsonString(metadata.defaultLocale) << "\",\n";
  json << "  \"targetResolution\": \"" << escapeJsonString(metadata.targetResolution) << "\",\n";
  json << "  \"fullscreenDefault\": " << (metadata.fullscreenDefault ? "true" : "false") << ",\n";
  json << "  \"buildPreset\": \"" << escapeJsonString(metadata.buildPreset) << "\",\n";
  json << "  \"targetPlatforms\": [";
  for (size_t i = 0; i < metadata.targetPlatforms.size(); ++i) {
    if (i > 0)
      json << ", ";
    json << "\"" << escapeJsonString(metadata.targetPlatforms[i]) << "\"";
  }
  json << "],\n";

  // Playback source mode (issue #82, #100)
  std::string modeStr;
  switch (metadata.playbackSourceMode) {
  case PlaybackSourceMode::Graph:
    modeStr = "Graph";
    break;
  case PlaybackSourceMode::Mixed:
    modeStr = "Mixed";
    break;
  case PlaybackSourceMode::Script:
  default:
    modeStr = "Script";
    break;
  }
  json << "  \"playbackSourceMode\": \"" << modeStr << "\"\n";
  json << "}\n";

  outJson = json.str();
  return Result<void>::ok();
}

// ============================================================================
// Load from File
// ============================================================================

Result<void> ProjectJsonHandler::loadFromFile(const std::string& path,
                                              ProjectMetadata& outMetadata) {
  namespace fs = std::filesystem;

  // Check file exists
  if (!fs::exists(path)) {
    return Result<void>::error("Project file not found: " + path);
  }

  // Open file
  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open project file: " + path);
  }

  // Read entire file
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  if (content.empty()) {
    return Result<void>::error("Project file is empty: " + path);
  }

  // Parse
  return parseFromString(content, outMetadata);
}

// ============================================================================
// Save to File (Atomic)
// ============================================================================

Result<void> ProjectJsonHandler::saveToFile(const std::string& path,
                                            const ProjectMetadata& metadata) {
  namespace fs = std::filesystem;

  // Serialize to string
  std::string json;
  auto serializeResult = serializeToString(metadata, json);
  if (serializeResult.isError()) {
    return serializeResult;
  }

  // Atomic write: write to temp file, then rename
  fs::path targetPath(path);
  fs::path tempPath = targetPath;
  tempPath += ".tmp";

  // Write to temporary file
  {
    std::ofstream tempFile(tempPath);
    if (!tempFile.is_open()) {
      return Result<void>::error("Failed to create temporary file: " + tempPath.string());
    }

    tempFile << json;
    tempFile.flush();

    if (tempFile.fail()) {
      return Result<void>::error("Failed to write to temporary file: " + tempPath.string());
    }
  }

  // Verify temp file was written
  if (!fs::exists(tempPath)) {
    return Result<void>::error("Temporary file was not created: " + tempPath.string());
  }

  // Rename temp file to target (atomic operation)
  std::error_code ec;
  fs::rename(tempPath, targetPath, ec);
  if (ec) {
    // Clean up temp file on failure
    fs::remove(tempPath, ec);
    return Result<void>::error("Atomic write failed: " + ec.message());
  }

  return Result<void>::ok();
}

// ============================================================================
// Validation
// ============================================================================

Result<void> ProjectJsonHandler::validate(const ProjectMetadata& metadata) {
  // Name must not be empty
  if (metadata.name.empty()) {
    return Result<void>::error("Project name cannot be empty");
  }

  // Name must not contain invalid characters
  const std::string invalidChars = "<>:\"/\\|?*";
  for (char c : metadata.name) {
    if (invalidChars.find(c) != std::string::npos) {
      return Result<void>::error("Project name contains invalid character: " + std::string(1, c));
    }
  }

  // Version should be valid semver-like (basic check)
  if (!metadata.version.empty()) {
    std::regex versionPattern(R"(\d+\.\d+(\.\d+)?)");
    if (!std::regex_match(metadata.version, versionPattern)) {
      return Result<void>::error("Invalid version format: " + metadata.version);
    }
  }

  // Target resolution should be in format WIDTHxHEIGHT
  if (!metadata.targetResolution.empty()) {
    std::regex resolutionPattern(R"(\d+x\d+)");
    if (!std::regex_match(metadata.targetResolution, resolutionPattern)) {
      return Result<void>::error("Invalid resolution format: " + metadata.targetResolution +
                                 " (expected WIDTHxHEIGHT)");
    }
  }

  return Result<void>::ok();
}

// ============================================================================
// Migration
// ============================================================================

Result<void> ProjectJsonHandler::migrateFromVersion(u32 version, JsonObject& obj) {
  // Currently only version 1 is supported, no migration needed
  // Future versions can implement migration logic here

  // Example for future:
  // if (version == 1) {
  //   // Migrate v1 -> v2
  //   obj["newFieldInV2"] = "default_value";
  // }

  (void)version; // Suppress unused parameter warning
  (void)obj;

  return Result<void>::ok();
}

} // namespace NovelMind::editor
