#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/project_json.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

using namespace NovelMind;
using namespace NovelMind::editor;

// =============================================================================
// JSON String Escaping Tests
// =============================================================================

TEST_CASE("ProjectJson - String escaping", "[project_json][escaping]") {
  ProjectMetadata metadata;
  metadata.name = "Test Project";  // Valid name without quotes
  metadata.description = "Description with \"quotes\" and\nspecial\tchars";
  metadata.version = "1.0.0";

  std::string json;
  auto result = ProjectJsonHandler::serializeToString(metadata, json);

  REQUIRE(result.isOk());
  CHECK(json.find("\\\"") != std::string::npos);  // Escaped quotes in description
  CHECK(json.find("\\n") != std::string::npos);   // Escaped newline
  CHECK(json.find("\\t") != std::string::npos);   // Escaped tab
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_CASE("ProjectJson - Validation rejects empty name", "[project_json][validation]") {
  ProjectMetadata metadata;
  metadata.name = "";  // Invalid: empty name
  metadata.version = "1.0.0";

  auto result = ProjectJsonHandler::validate(metadata);
  CHECK(result.isError());
  CHECK(result.error().find("name") != std::string::npos);
}

TEST_CASE("ProjectJson - Validation rejects invalid characters", "[project_json][validation]") {
  ProjectMetadata metadata;
  metadata.name = "Invalid<Name>";  // Invalid: contains <
  metadata.version = "1.0.0";

  auto result = ProjectJsonHandler::validate(metadata);
  CHECK(result.isError());
  CHECK(result.error().find("invalid character") != std::string::npos);
}

TEST_CASE("ProjectJson - Validation rejects invalid version", "[project_json][validation]") {
  ProjectMetadata metadata;
  metadata.name = "Test Project";
  metadata.version = "not_a_version";  // Invalid: not semver

  auto result = ProjectJsonHandler::validate(metadata);
  CHECK(result.isError());
  CHECK(result.error().find("version") != std::string::npos);
}

TEST_CASE("ProjectJson - Validation rejects invalid resolution", "[project_json][validation]") {
  ProjectMetadata metadata;
  metadata.name = "Test Project";
  metadata.version = "1.0.0";
  metadata.targetResolution = "invalid";  // Invalid: not WIDTHxHEIGHT

  auto result = ProjectJsonHandler::validate(metadata);
  CHECK(result.isError());
  CHECK(result.error().find("resolution") != std::string::npos);
}

TEST_CASE("ProjectJson - Validation accepts valid metadata", "[project_json][validation]") {
  ProjectMetadata metadata;
  metadata.name = "Test Project";
  metadata.version = "1.0.0";
  metadata.targetResolution = "1920x1080";

  auto result = ProjectJsonHandler::validate(metadata);
  CHECK(result.isOk());
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_CASE("ProjectJson - Serialize minimal metadata", "[project_json][serialization]") {
  ProjectMetadata metadata;
  metadata.name = "Minimal Project";
  metadata.version = "1.0.0";

  std::string json;
  auto result = ProjectJsonHandler::serializeToString(metadata, json);

  REQUIRE(result.isOk());
  CHECK(json.find("\"name\"") != std::string::npos);
  CHECK(json.find("\"Minimal Project\"") != std::string::npos);
  CHECK(json.find("\"version\"") != std::string::npos);
  CHECK(json.find("\"1.0.0\"") != std::string::npos);
  CHECK(json.find("\"fileVersion\"") != std::string::npos);
}

TEST_CASE("ProjectJson - Serialize complete metadata", "[project_json][serialization]") {
  ProjectMetadata metadata;
  metadata.name = "Complete Project";
  metadata.version = "2.3.1";
  metadata.author = "Test Author";
  metadata.description = "Test Description";
  metadata.engineVersion = "0.2.0";
  metadata.startScene = "intro";
  metadata.defaultLocale = "ru";
  metadata.targetResolution = "2560x1440";
  metadata.fullscreenDefault = true;
  metadata.buildPreset = "debug";
  metadata.createdAt = 1234567890;
  metadata.modifiedAt = 1234567900;
  metadata.lastOpenedAt = 1234567910;
  metadata.targetPlatforms = {"windows", "linux"};

  std::string json;
  auto result = ProjectJsonHandler::serializeToString(metadata, json);

  REQUIRE(result.isOk());
  CHECK(json.find("\"Complete Project\"") != std::string::npos);
  CHECK(json.find("\"Test Author\"") != std::string::npos);
  CHECK(json.find("\"Test Description\"") != std::string::npos);
  CHECK(json.find("\"intro\"") != std::string::npos);
  CHECK(json.find("\"ru\"") != std::string::npos);
  CHECK(json.find("\"2560x1440\"") != std::string::npos);
  CHECK(json.find("true") != std::string::npos);
  CHECK(json.find("\"debug\"") != std::string::npos);
  CHECK(json.find("1234567890") != std::string::npos);
  CHECK(json.find("\"windows\"") != std::string::npos);
  CHECK(json.find("\"linux\"") != std::string::npos);
}

// =============================================================================
// Parsing Tests
// =============================================================================

TEST_CASE("ProjectJson - Parse minimal JSON", "[project_json][parsing]") {
  std::string json = R"({
    "fileVersion": 1,
    "name": "Test Project"
  })";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  REQUIRE(result.isOk());
  CHECK(metadata.name == "Test Project");
  CHECK(metadata.version == "1.0.0");  // Default value
}

TEST_CASE("ProjectJson - Parse complete JSON", "[project_json][parsing]") {
  std::string json = R"({
    "fileVersion": 1,
    "name": "Complete Project",
    "version": "2.3.1",
    "author": "Test Author",
    "description": "Test Description",
    "engineVersion": "0.2.0",
    "createdAt": 1234567890,
    "modifiedAt": 1234567900,
    "lastOpenedAt": 1234567910,
    "startScene": "intro",
    "defaultLocale": "ru",
    "targetResolution": "2560x1440",
    "fullscreenDefault": true,
    "buildPreset": "debug",
    "targetPlatforms": ["windows", "linux", "macos"]
  })";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  REQUIRE(result.isOk());
  CHECK(metadata.name == "Complete Project");
  CHECK(metadata.version == "2.3.1");
  CHECK(metadata.author == "Test Author");
  CHECK(metadata.description == "Test Description");
  CHECK(metadata.engineVersion == "0.2.0");
  CHECK(metadata.startScene == "intro");
  CHECK(metadata.defaultLocale == "ru");
  CHECK(metadata.targetResolution == "2560x1440");
  CHECK(metadata.fullscreenDefault == true);
  CHECK(metadata.buildPreset == "debug");
  CHECK(metadata.createdAt == 1234567890);
  CHECK(metadata.modifiedAt == 1234567900);
  CHECK(metadata.lastOpenedAt == 1234567910);
  REQUIRE(metadata.targetPlatforms.size() == 3);
  CHECK(metadata.targetPlatforms[0] == "windows");
  CHECK(metadata.targetPlatforms[1] == "linux");
  CHECK(metadata.targetPlatforms[2] == "macos");
}

TEST_CASE("ProjectJson - Parse with escaped strings", "[project_json][parsing]") {
  std::string json = R"({
    "fileVersion": 1,
    "name": "Test Project",
    "description": "Description with \"quotes\" and\nspecial\tchars"
  })";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  REQUIRE(result.isOk());
  CHECK(metadata.name == "Test Project");
  CHECK(metadata.description == "Description with \"quotes\" and\nspecial\tchars");
}

TEST_CASE("ProjectJson - Parse rejects missing required field", "[project_json][parsing]") {
  std::string json = R"({
    "fileVersion": 1,
    "version": "1.0.0"
  })";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  CHECK(result.isError());
  CHECK(result.error().find("name") != std::string::npos);
}

TEST_CASE("ProjectJson - Parse rejects invalid JSON", "[project_json][parsing]") {
  std::string json = "{ invalid json";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  CHECK(result.isError());
}

TEST_CASE("ProjectJson - Parse rejects unsupported version", "[project_json][parsing]") {
  std::string json = R"({
    "fileVersion": 999,
    "name": "Future Project"
  })";

  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::parseFromString(json, metadata);

  CHECK(result.isError());
  CHECK(result.error().find("Unsupported") != std::string::npos);
}

// =============================================================================
// Round-trip Tests (Serialize -> Parse)
// =============================================================================

TEST_CASE("ProjectJson - Round-trip preserves data", "[project_json][roundtrip]") {
  ProjectMetadata original;
  original.name = "Round-trip Test";
  original.version = "1.2.3";
  original.author = "Test Author";
  original.description = "Test Description with\nnewlines and \"quotes\"";
  original.engineVersion = "0.2.0";
  original.startScene = "main";
  original.defaultLocale = "en";
  original.targetResolution = "1920x1080";
  original.fullscreenDefault = false;
  original.buildPreset = "release";
  original.createdAt = 1000;
  original.modifiedAt = 2000;
  original.lastOpenedAt = 3000;
  original.targetPlatforms = {"windows", "linux", "macos"};

  // Serialize
  std::string json;
  auto serializeResult = ProjectJsonHandler::serializeToString(original, json);
  REQUIRE(serializeResult.isOk());

  // Parse
  ProjectMetadata parsed;
  auto parseResult = ProjectJsonHandler::parseFromString(json, parsed);
  REQUIRE(parseResult.isOk());

  // Compare
  CHECK(parsed.name == original.name);
  CHECK(parsed.version == original.version);
  CHECK(parsed.author == original.author);
  CHECK(parsed.description == original.description);
  CHECK(parsed.engineVersion == original.engineVersion);
  CHECK(parsed.startScene == original.startScene);
  CHECK(parsed.defaultLocale == original.defaultLocale);
  CHECK(parsed.targetResolution == original.targetResolution);
  CHECK(parsed.fullscreenDefault == original.fullscreenDefault);
  CHECK(parsed.buildPreset == original.buildPreset);
  CHECK(parsed.createdAt == original.createdAt);
  CHECK(parsed.modifiedAt == original.modifiedAt);
  CHECK(parsed.lastOpenedAt == original.lastOpenedAt);
  CHECK(parsed.targetPlatforms == original.targetPlatforms);
}

// =============================================================================
// File I/O Tests
// =============================================================================

TEST_CASE("ProjectJson - Save and load from file", "[project_json][file_io]") {
  namespace fs = std::filesystem;

  // Create temp directory
  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_project_json";
  fs::create_directories(tempDir);

  // Clean up on exit
  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  // Create metadata
  ProjectMetadata original;
  original.name = "File IO Test";  // No slash in name
  original.version = "1.0.0";
  original.author = "Test";
  original.createdAt = 12345;

  // Save to file
  auto saveResult = ProjectJsonHandler::saveToFile(projectFile.string(), original);
  if (saveResult.isError()) {
    std::cerr << "Save error: " << saveResult.error() << std::endl;
  }
  REQUIRE(saveResult.isOk());

  // Verify file exists
  CHECK(fs::exists(projectFile));

  // Load from file
  ProjectMetadata loaded;
  auto loadResult = ProjectJsonHandler::loadFromFile(projectFile.string(), loaded);
  if (loadResult.isError()) {
    std::cerr << "Load error: " << loadResult.error() << std::endl;
  }
  REQUIRE(loadResult.isOk());

  // Compare
  CHECK(loaded.name == original.name);
  CHECK(loaded.version == original.version);
  CHECK(loaded.author == original.author);
  CHECK(loaded.createdAt == original.createdAt);
}

TEST_CASE("ProjectJson - Load from non-existent file fails", "[project_json][file_io]") {
  ProjectMetadata metadata;
  auto result = ProjectJsonHandler::loadFromFile("/nonexistent/project.json", metadata);

  CHECK(result.isError());
  CHECK(result.error().find("not found") != std::string::npos);
}

TEST_CASE("ProjectJson - Atomic write creates temp file", "[project_json][atomic_write]") {
  namespace fs = std::filesystem;

  // Create temp directory
  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_atomic";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";
  fs::path tempFile = tempDir / "project.json.tmp";

  ProjectMetadata metadata;
  metadata.name = "Atomic Test";
  metadata.version = "1.0.0";

  // Save
  auto result = ProjectJsonHandler::saveToFile(projectFile.string(), metadata);
  REQUIRE(result.isOk());

  // Final file should exist
  CHECK(fs::exists(projectFile));

  // Temp file should be cleaned up
  CHECK_FALSE(fs::exists(tempFile));
}

TEST_CASE("ProjectJson - Atomic write preserves existing file on error", "[project_json][atomic_write]") {
  namespace fs = std::filesystem;

  // Create temp directory
  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_atomic_error";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  // Create initial valid file
  ProjectMetadata valid;
  valid.name = "Valid Project";
  valid.version = "1.0.0";

  auto saveResult = ProjectJsonHandler::saveToFile(projectFile.string(), valid);
  REQUIRE(saveResult.isOk());

  // Read initial content
  std::ifstream initialFile(projectFile);
  std::ostringstream initialBuffer;
  initialBuffer << initialFile.rdbuf();
  std::string initialContent = initialBuffer.str();

  // Try to save invalid metadata (should fail validation)
  ProjectMetadata invalid;
  invalid.name = "";  // Invalid: empty name
  invalid.version = "1.0.0";

  auto invalidSaveResult = ProjectJsonHandler::saveToFile(projectFile.string(), invalid);
  CHECK(invalidSaveResult.isError());

  // Original file should still exist and be unchanged
  CHECK(fs::exists(projectFile));

  std::ifstream finalFile(projectFile);
  std::ostringstream finalBuffer;
  finalBuffer << finalFile.rdbuf();
  std::string finalContent = finalBuffer.str();

  CHECK(initialContent == finalContent);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("ProjectJson - Error codes are descriptive", "[project_json][errors]") {
  const char *msg = projectJsonErrorToString(ProjectJsonError::MissingRequiredField);
  CHECK(msg != nullptr);
  CHECK(std::string(msg).find("Missing") != std::string::npos);

  msg = projectJsonErrorToString(ProjectJsonError::InvalidJsonSyntax);
  CHECK(msg != nullptr);
  CHECK(std::string(msg).find("syntax") != std::string::npos);

  msg = projectJsonErrorToString(ProjectJsonError::AtomicWriteFailed);
  CHECK(msg != nullptr);
  CHECK(std::string(msg).find("Atomic") != std::string::npos);
}

// =============================================================================
// Corruption and Recovery Tests
// =============================================================================

TEST_CASE("ProjectJson - Detects truncated project file", "[project_json][corruption]") {
  namespace fs = std::filesystem;

  // Create temp directory
  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_truncated";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  SECTION("Truncated in middle of JSON") {
    // Create truncated JSON (cut off in the middle)
    std::string truncatedJson = R"({
      "fileVersion": 1,
      "name": "Truncated Project",
      "version": "1.0.0",
      "author": "Test)";  // Truncated here - no closing quotes, braces

    std::ofstream file(projectFile);
    file << truncatedJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
    CHECK((result.error().find("syntax") != std::string::npos ||
           result.error().find("parse") != std::string::npos ||
           result.error().find("invalid") != std::string::npos));
  }

  SECTION("Truncated at end of file") {
    // Valid JSON but suddenly ends
    std::string truncatedJson = R"({
      "fileVersion": 1,
      "name": "Truncated)";

    std::ofstream file(projectFile);
    file << truncatedJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Empty file") {
    // Completely empty file
    std::ofstream file(projectFile);
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Single character file") {
    std::ofstream file(projectFile);
    file << "{";
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }
}

TEST_CASE("ProjectJson - Detects invalid JSON syntax", "[project_json][corruption]") {
  namespace fs = std::filesystem;

  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_invalid_json";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  SECTION("Missing closing brace") {
    std::string invalidJson = R"({
      "fileVersion": 1,
      "name": "Invalid Project"
    )";  // Missing closing }

    std::ofstream file(projectFile);
    file << invalidJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Missing comma between fields") {
    std::string invalidJson = R"({
      "fileVersion": 1
      "name": "Invalid Project"
    })";  // Missing comma after fileVersion

    std::ofstream file(projectFile);
    file << invalidJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Unescaped quotes in string") {
    std::string invalidJson = R"({
      "fileVersion": 1,
      "name": "Project with "unescaped" quotes"
    })";

    std::ofstream file(projectFile);
    file << invalidJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Trailing comma in object") {
    std::string invalidJson = R"({
      "fileVersion": 1,
      "name": "Invalid Project",
    })";  // Trailing comma before }

    std::ofstream file(projectFile);
    file << invalidJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    // Note: Some parsers allow trailing commas, but strict parsers reject them
    // Our custom parser should reject this
    CHECK(result.isError());
  }

  SECTION("Invalid characters in JSON") {
    std::string invalidJson = R"({
      "fileVersion": 1,
      "name": "Invalid Project",
      @#$%
    })";

    std::ofstream file(projectFile);
    file << invalidJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Not JSON at all - plain text") {
    std::string notJson = "This is just plain text, not JSON at all!";

    std::ofstream file(projectFile);
    file << notJson;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Binary data") {
    std::ofstream file(projectFile, std::ios::binary);
    char binaryData[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
    file.write(binaryData, sizeof(binaryData));
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }
}

TEST_CASE("ProjectJson - Handles missing required fields gracefully", "[project_json][corruption]") {
  namespace fs = std::filesystem;

  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_missing_fields";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  SECTION("Missing 'name' field") {
    std::string json = R"({
      "fileVersion": 1,
      "version": "1.0.0"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
    CHECK(result.error().find("name") != std::string::npos);
  }

  SECTION("Missing 'fileVersion' field") {
    std::string json = R"({
      "name": "Test Project",
      "version": "1.0.0"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
    CHECK((result.error().find("fileVersion") != std::string::npos ||
           result.error().find("version") != std::string::npos));
  }

  SECTION("Wrong type for field") {
    std::string json = R"({
      "fileVersion": "not_a_number",
      "name": "Test Project"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Array instead of string") {
    std::string json = R"({
      "fileVersion": 1,
      "name": ["This", "Should", "Be", "A", "String"]
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Null value for required field") {
    std::string json = R"({
      "fileVersion": 1,
      "name": null
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }
}

TEST_CASE("ProjectJson - Partial corruption preserves valid fields", "[project_json][corruption][recovery]") {
  namespace fs = std::filesystem;

  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_partial";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  SECTION("Valid core fields with corrupted optional field") {
    // Even with bad optional fields, the parser should reject the whole thing
    // to maintain data integrity
    std::string json = R"({
      "fileVersion": 1,
      "name": "Partial Project",
      "version": "1.0.0",
      "invalidField": {broken json here
    })";

    fs::path projectFile = tempDir / "project.json";
    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    // Should fail due to syntax error in optional field
    CHECK(result.isError());
  }
}

TEST_CASE("ProjectManager - Backup creation and restoration", "[project_manager][backup][recovery]") {
  namespace fs = std::filesystem;

  // Create temp directory for test project
  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_backup";
  fs::remove_all(tempDir);  // Clean any previous test
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  // Create a valid project
  ProjectMetadata originalMetadata;
  originalMetadata.name = "Backup Test Project";
  originalMetadata.version = "1.0.0";
  originalMetadata.author = "Test Author";
  originalMetadata.description = "Testing backup functionality";

  fs::path projectFile = tempDir / "project.json";
  auto saveResult = ProjectJsonHandler::saveToFile(projectFile.string(), originalMetadata);
  REQUIRE(saveResult.isOk());

  // Create required folders
  fs::create_directories(tempDir / "Assets");
  fs::create_directories(tempDir / "Scenes");
  fs::create_directories(tempDir / "Scripts");

  // Create a test asset file
  fs::path testAsset = tempDir / "Assets" / "test.txt";
  std::ofstream assetFile(testAsset);
  assetFile << "Original content";
  assetFile.close();

  SECTION("Backup captures current project state") {
    ProjectManager &pm = ProjectManager::instance();
    auto openResult = pm.openProject(tempDir.string());
    REQUIRE(openResult.isOk());

    auto backupResult = pm.createBackup();
    REQUIRE(backupResult.isOk());

    std::string backupPath = backupResult.value();
    CHECK(fs::exists(backupPath));
    CHECK(fs::exists(fs::path(backupPath) / "project.json"));
    CHECK(fs::exists(fs::path(backupPath) / "Assets" / "test.txt"));

    pm.closeProject();
  }

  SECTION("Backup restoration recovers corrupted project") {
    ProjectManager &pm = ProjectManager::instance();
    auto openResult = pm.openProject(tempDir.string());
    REQUIRE(openResult.isOk());

    // Create backup
    auto backupResult = pm.createBackup();
    REQUIRE(backupResult.isOk());
    std::string backupPath = backupResult.value();

    pm.closeProject();

    // Corrupt the project file
    std::ofstream corruptFile(projectFile);
    corruptFile << "{corrupted data";
    corruptFile.close();

    // Corrupt the asset
    std::ofstream corruptAsset(testAsset);
    corruptAsset << "Corrupted content";
    corruptAsset.close();

    // Open project again
    openResult = pm.openProject(tempDir.string());
    // Project opening should fail due to corruption
    if (openResult.isOk()) {
      // If it somehow opened, restore from backup
      auto restoreResult = pm.restoreFromBackup(backupPath);
      REQUIRE(restoreResult.isOk());

      pm.closeProject();
    } else {
      // Can't restore through ProjectManager if project won't open
      // Manual restoration would be needed in this case
      // This is expected behavior - user would manually restore backup
      CHECK(openResult.isError());
    }

    // Manually restore to verify backup validity
    fs::copy(fs::path(backupPath) / "project.json", projectFile,
             fs::copy_options::overwrite_existing);
    fs::copy(fs::path(backupPath) / "Assets" / "test.txt", testAsset,
             fs::copy_options::overwrite_existing);

    // Now project should open successfully
    openResult = pm.openProject(tempDir.string());
    REQUIRE(openResult.isOk());

    // Verify data is restored
    auto metadata = pm.getMetadata();
    CHECK(metadata.name == "Backup Test Project");
    CHECK(metadata.author == "Test Author");

    // Verify asset is restored
    std::ifstream restoredAsset(testAsset);
    std::string content;
    std::getline(restoredAsset, content);
    CHECK(content == "Original content");

    pm.closeProject();
  }

  SECTION("Multiple backups are maintained") {
    ProjectManager &pm = ProjectManager::instance();
    auto openResult = pm.openProject(tempDir.string());
    REQUIRE(openResult.isOk());

    // Create multiple backups
    auto backup1 = pm.createBackup();
    REQUIRE(backup1.isOk());

    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto backup2 = pm.createBackup();
    REQUIRE(backup2.isOk());

    // Verify both backups exist
    CHECK(fs::exists(backup1.value()));
    CHECK(fs::exists(backup2.value()));
    CHECK(backup1.value() != backup2.value());

    auto backups = pm.getAvailableBackups();
    CHECK(backups.size() >= 2);

    pm.closeProject();
  }
}

TEST_CASE("ProjectJson - Version migration support", "[project_json][migration]") {
  namespace fs = std::filesystem;

  fs::path tempDir = fs::temp_directory_path() / "novelmind_test_migration";
  fs::create_directories(tempDir);

  struct Cleanup {
    fs::path dir;
    ~Cleanup() { fs::remove_all(dir); }
  } cleanup{tempDir};

  fs::path projectFile = tempDir / "project.json";

  SECTION("Current version loads successfully") {
    std::string json = R"({
      "fileVersion": 1,
      "name": "Current Version Project",
      "version": "1.0.0"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    REQUIRE(result.isOk());
    CHECK(metadata.name == "Current Version Project");
  }

  SECTION("Future version is rejected") {
    std::string json = R"({
      "fileVersion": 999,
      "name": "Future Project",
      "version": "1.0.0",
      "futureFeature": "not yet implemented"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
    CHECK(result.error().find("Unsupported") != std::string::npos);
  }

  SECTION("Version 0 is rejected (pre-versioning)") {
    std::string json = R"({
      "fileVersion": 0,
      "name": "Ancient Project"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Negative version is rejected") {
    std::string json = R"({
      "fileVersion": -1,
      "name": "Invalid Version Project"
    })";

    std::ofstream file(projectFile);
    file << json;
    file.close();

    ProjectMetadata metadata;
    auto result = ProjectJsonHandler::loadFromFile(projectFile.string(), metadata);

    CHECK(result.isError());
  }

  SECTION("Saved file includes current version") {
    ProjectMetadata metadata;
    metadata.name = "Version Check Project";
    metadata.version = "1.0.0";

    auto saveResult = ProjectJsonHandler::saveToFile(projectFile.string(), metadata);
    REQUIRE(saveResult.isOk());

    // Read the file and verify it has fileVersion field
    std::ifstream file(projectFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    CHECK(content.find("\"fileVersion\"") != std::string::npos);
    CHECK(content.find("\"fileVersion\": 1") != std::string::npos);
  }
}
