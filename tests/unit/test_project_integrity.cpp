#include "NovelMind/editor/project_integrity.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace NovelMind;
using namespace NovelMind::editor;
namespace fs = std::filesystem;

// =============================================================================
// Test fixture helpers
// =============================================================================

static std::string createTempDir() {
  std::string tempPath =
      fs::temp_directory_path().string() + "/nm_integrity_test_" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  fs::create_directories(tempPath);
  return tempPath;
}

static void cleanupTempDir(const std::string& path) {
  if (fs::exists(path)) {
    fs::remove_all(path);
  }
}

static void createFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path);
  file << content;
  file.close();
}

// =============================================================================
// QuickFixes::createEmptyScene Tests
// =============================================================================

TEST_CASE("QuickFixes::createEmptyScene creates a valid scene file",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Creates scene file with correct JSON structure") {
    auto result = QuickFixes::createEmptyScene(projectPath, "test_scene");
    REQUIRE(result.isOk());

    fs::path sceneFile = fs::path(projectPath) / "Scenes" / "test_scene.nmscene";
    REQUIRE(fs::exists(sceneFile));

    // Read and verify content
    std::ifstream file(sceneFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"sceneId\": \"test_scene\"") != std::string::npos);
    REQUIRE(content.find("\"objects\": []") != std::string::npos);
  }

  SECTION("Fails if scene already exists") {
    // Create the scene first
    QuickFixes::createEmptyScene(projectPath, "existing_scene");

    // Try to create it again
    auto result = QuickFixes::createEmptyScene(projectPath, "existing_scene");
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("already exists") != std::string::npos);
  }

  SECTION("Creates Scenes directory if not exists") {
    REQUIRE(!fs::exists(fs::path(projectPath) / "Scenes"));

    auto result = QuickFixes::createEmptyScene(projectPath, "new_scene");
    REQUIRE(result.isOk());
    REQUIRE(fs::exists(fs::path(projectPath) / "Scenes"));
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::createDefaultProjectConfig Tests
// =============================================================================

TEST_CASE("QuickFixes::createDefaultProjectConfig creates valid project.json",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Creates project.json with required fields") {
    auto result = QuickFixes::createDefaultProjectConfig(projectPath, "TestProject");
    REQUIRE(result.isOk());

    fs::path projectFile = fs::path(projectPath) / "project.json";
    REQUIRE(fs::exists(projectFile));

    std::ifstream file(projectFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"name\": \"TestProject\"") != std::string::npos);
    REQUIRE(content.find("\"version\": \"1.0.0\"") != std::string::npos);
    REQUIRE(content.find("\"engineVersion\": \"0.2.0\"") != std::string::npos);
    REQUIRE(content.find("\"startScene\"") != std::string::npos);
  }

  SECTION("Does not overwrite existing project.json") {
    createFile(fs::path(projectPath) / "project.json", "{\"name\": \"Old\"}");

    auto result = QuickFixes::createDefaultProjectConfig(projectPath, "NewProject");
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("already exists") != std::string::npos);
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::createPlaceholderAsset Tests
// =============================================================================

TEST_CASE("QuickFixes::createPlaceholderAsset creates placeholder files",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Creates PNG placeholder with valid PNG header") {
    auto result = QuickFixes::createPlaceholderAsset(projectPath, "Assets/test.png");
    REQUIRE(result.isOk());

    fs::path assetFile = fs::path(projectPath) / "Assets" / "test.png";
    REQUIRE(fs::exists(assetFile));

    // Check PNG signature (first 8 bytes)
    std::ifstream file(assetFile, std::ios::binary);
    char header[8];
    file.read(header, 8);
    REQUIRE(header[0] == '\x89');
    REQUIRE(header[1] == 'P');
    REQUIRE(header[2] == 'N');
    REQUIRE(header[3] == 'G');
  }

  SECTION("Creates JSON placeholder") {
    auto result = QuickFixes::createPlaceholderAsset(projectPath, "Assets/data.json");
    REQUIRE(result.isOk());

    fs::path assetFile = fs::path(projectPath) / "Assets" / "data.json";
    REQUIRE(fs::exists(assetFile));

    std::ifstream file(assetFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    REQUIRE(content.find("{") != std::string::npos);
  }

  SECTION("Creates parent directories if needed") {
    auto result =
        QuickFixes::createPlaceholderAsset(projectPath, "Assets/Deep/Nested/Path/image.png");
    REQUIRE(result.isOk());
    REQUIRE(fs::exists(fs::path(projectPath) / "Assets/Deep/Nested/Path/image.png"));
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::removeOrphanedAsset Tests
// =============================================================================

TEST_CASE("QuickFixes::removeOrphanedAsset removes asset files",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Removes existing asset file") {
    fs::path assetPath = fs::path(projectPath) / "Assets" / "unused.png";
    createFile(assetPath, "dummy content");
    REQUIRE(fs::exists(assetPath));

    auto result = QuickFixes::removeOrphanedAsset(projectPath, assetPath.string());
    REQUIRE(result.isOk());
    REQUIRE(!fs::exists(assetPath));
  }

  SECTION("Fails for non-existent file") {
    auto result = QuickFixes::removeOrphanedAsset(projectPath, "Assets/nonexistent.png");
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("not found") != std::string::npos);
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::setFirstSceneAsStart Tests
// =============================================================================

TEST_CASE("QuickFixes::setFirstSceneAsStart updates project.json",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Sets first scene as start scene") {
    // Create project.json with empty startScene
    createFile(fs::path(projectPath) / "project.json",
               "{\n  \"name\": \"Test\",\n  \"startScene\": \"\"\n}");

    // Create a scene
    QuickFixes::createEmptyScene(projectPath, "first_scene");

    auto result = QuickFixes::setFirstSceneAsStart(projectPath);
    REQUIRE(result.isOk());

    // Verify project.json was updated
    std::ifstream file(fs::path(projectPath) / "project.json");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"startScene\": \"first_scene\"") != std::string::npos);
  }

  SECTION("Fails if no scenes exist") {
    createFile(fs::path(projectPath) / "project.json", "{\n  \"startScene\": \"\"\n}");
    fs::create_directories(fs::path(projectPath) / "Scenes");

    auto result = QuickFixes::setFirstSceneAsStart(projectPath);
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("No scenes found") != std::string::npos);
  }

  SECTION("Fails if Scenes directory doesn't exist") {
    createFile(fs::path(projectPath) / "project.json", "{\n  \"startScene\": \"\"\n}");

    auto result = QuickFixes::setFirstSceneAsStart(projectPath);
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("directory not found") != std::string::npos);
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::createMainEntryScene Tests
// =============================================================================

TEST_CASE("QuickFixes::createMainEntryScene creates main scene and script",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  // Create project.json first
  createFile(fs::path(projectPath) / "project.json",
             "{\n  \"name\": \"Test\",\n  \"startScene\": \"\"\n}");

  SECTION("Creates both scene file and script") {
    auto result = QuickFixes::createMainEntryScene(projectPath);
    REQUIRE(result.isOk());

    // Check scene file
    REQUIRE(fs::exists(fs::path(projectPath) / "Scenes" / "main.nmscene"));

    // Check script file
    fs::path scriptFile = fs::path(projectPath) / "Scripts" / "main.nms";
    REQUIRE(fs::exists(scriptFile));

    std::ifstream file(scriptFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("scene main") != std::string::npos);
    REQUIRE(content.find("end") != std::string::npos);
  }

  SECTION("Updates startScene in project.json") {
    QuickFixes::createMainEntryScene(projectPath);

    std::ifstream file(fs::path(projectPath) / "project.json");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"startScene\": \"main\"") != std::string::npos);
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::addMissingLocalizationKey Tests
// =============================================================================

TEST_CASE("QuickFixes::addMissingLocalizationKey adds keys to locale files",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Creates locale file if it doesn't exist") {
    auto result = QuickFixes::addMissingLocalizationKey(projectPath, "hello_world", "en");
    REQUIRE(result.isOk());

    fs::path locFile = fs::path(projectPath) / "Localization" / "en.json";
    REQUIRE(fs::exists(locFile));

    std::ifstream file(locFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"hello_world\": \"\"") != std::string::npos);
  }

  SECTION("Adds key to existing locale file") {
    // Create existing locale file
    createFile(fs::path(projectPath) / "Localization" / "de.json",
               "{\n  \"existing_key\": \"Existing Value\"\n}");

    auto result = QuickFixes::addMissingLocalizationKey(projectPath, "new_key", "de");
    REQUIRE(result.isOk());

    std::ifstream file(fs::path(projectPath) / "Localization" / "de.json");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("\"existing_key\": \"Existing Value\"") != std::string::npos);
    REQUIRE(content.find("\"new_key\": \"\"") != std::string::npos);
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// QuickFixes::removeMissingSceneReference Tests
// =============================================================================

TEST_CASE("QuickFixes::removeMissingSceneReference comments out references",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();

  SECTION("Comments out goto references to missing scene") {
    // Create a script with a reference to a missing scene
    createFile(fs::path(projectPath) / "Scripts" / "test.nms",
               "scene intro {\n  say \"Hello\"\n  goto missing_scene\n}\n");

    auto result = QuickFixes::removeMissingSceneReference(projectPath, "missing_scene");
    REQUIRE(result.isOk());

    std::ifstream file(fs::path(projectPath) / "Scripts" / "test.nms");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    REQUIRE(content.find("// [REMOVED:") != std::string::npos);
    REQUIRE(content.find("scene not found") != std::string::npos);
  }

  SECTION("Returns ok if Scripts directory doesn't exist") {
    auto result = QuickFixes::removeMissingSceneReference(projectPath, "missing_scene");
    REQUIRE(result.isOk());
  }

  cleanupTempDir(projectPath);
}

// =============================================================================
// ProjectIntegrityChecker::applyQuickFix Tests
// =============================================================================

TEST_CASE("ProjectIntegrityChecker::applyQuickFix dispatches correctly",
          "[project_integrity][quickfixes]") {
  std::string projectPath = createTempDir();
  ProjectIntegrityChecker checker;
  checker.setProjectPath(projectPath);

  SECTION("Returns error for issue without quick fix") {
    IntegrityIssue issue;
    issue.code = "TEST";
    issue.hasQuickFix = false;

    auto result = checker.applyQuickFix(issue);
    REQUIRE(!result.isOk());
    REQUIRE(result.error().find("No quick fix available") != std::string::npos);
  }

  SECTION("Handles C002 (missing directory) correctly") {
    IntegrityIssue issue;
    issue.code = "C002";
    issue.hasQuickFix = true;
    issue.filePath = (fs::path(projectPath) / "Assets").string();

    auto result = checker.applyQuickFix(issue);
    REQUIRE(result.isOk());
    REQUIRE(fs::exists(fs::path(projectPath) / "Assets"));
  }

  SECTION("Handles G001 (no entry point) correctly") {
    // Create project.json first
    createFile(fs::path(projectPath) / "project.json", "{\n  \"startScene\": \"\"\n}");

    IntegrityIssue issue;
    issue.code = "G001";
    issue.hasQuickFix = true;

    auto result = checker.applyQuickFix(issue);
    REQUIRE(result.isOk());
    REQUIRE(fs::exists(fs::path(projectPath) / "Scenes" / "main.nmscene"));
  }

  cleanupTempDir(projectPath);
}
