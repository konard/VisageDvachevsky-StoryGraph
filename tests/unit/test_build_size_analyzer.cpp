#include "NovelMind/editor/build_size_analyzer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace NovelMind;
using namespace NovelMind::editor;
namespace fs = std::filesystem;

// Test fixture helpers
static std::string createTempDir() {
  std::string tempPath =
      fs::temp_directory_path().string() + "/nm_analyzer_test_" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  fs::create_directories(tempPath);
  return tempPath;
}

static void cleanupTempDir(const std::string &path) {
  if (fs::exists(path)) {
    fs::remove_all(path);
  }
}

static void createTestFile(const std::string &path, const std::string &content) {
  fs::create_directories(fs::path(path).parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(content.data(), content.size());
  file.close();
}

// =============================================================================
// Hash Collision Detection Tests
// =============================================================================

TEST_CASE("BuildSizeAnalyzer detects true duplicates with SHA-256",
          "[build_size_analyzer][hash][security]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // Create two identical files
  std::string content = "This is identical content for testing duplicates.";
  createTestFile(assetsDir + "/file1.txt", content);
  createTestFile(assetsDir + "/file2.txt", content);

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // Should detect duplicates
  REQUIRE(analysis.duplicates.size() == 1);
  REQUIRE(analysis.duplicates[0].paths.size() == 2);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer rejects files with same size but different "
          "content",
          "[build_size_analyzer][hash][security]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // Create two files with same size but different content
  // Pad them to exactly the same length
  std::string content1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"; // 32 A's
  std::string content2 = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"; // 32 B's

  REQUIRE(content1.size() == content2.size());
  REQUIRE(content1 != content2);

  createTestFile(assetsDir + "/file1.txt", content1);
  createTestFile(assetsDir + "/file2.txt", content2);

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // Should NOT detect as duplicates (different SHA-256)
  REQUIRE(analysis.duplicates.size() == 0);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer handles files with weak hash collision patterns",
          "[build_size_analyzer][hash][security]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // Files designed to potentially cause weak hash collisions
  // These would have the same first 1KB and last 1KB in the old implementation
  std::string commonPrefix(1024, 'X');
  std::string commonSuffix(1024, 'Y');

  std::string content1 = commonPrefix + "DIFFERENT_MIDDLE_1" + commonSuffix;
  std::string content2 = commonPrefix + "DIFFERENT_MIDDLE_2" + commonSuffix;

  createTestFile(assetsDir + "/collision1.bin", content1);
  createTestFile(assetsDir + "/collision2.bin", content2);

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // SHA-256 should correctly identify these as different files
  REQUIRE(analysis.duplicates.size() == 0);

  // Both files should be in the analysis
  REQUIRE(analysis.assets.size() == 2);
  REQUIRE_FALSE(analysis.assets[0].isDuplicate);
  REQUIRE_FALSE(analysis.assets[1].isDuplicate);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer detects multiple duplicate groups",
          "[build_size_analyzer][hash]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // Create two groups of duplicates
  std::string contentA = "Content Group A";
  std::string contentB = "Content Group B - Different";

  createTestFile(assetsDir + "/groupA_1.txt", contentA);
  createTestFile(assetsDir + "/groupA_2.txt", contentA);
  createTestFile(assetsDir + "/groupA_3.txt", contentA);

  createTestFile(assetsDir + "/groupB_1.txt", contentB);
  createTestFile(assetsDir + "/groupB_2.txt", contentB);

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // Should detect two duplicate groups
  REQUIRE(analysis.duplicates.size() == 2);

  // Find the groups
  DuplicateGroup *groupA = nullptr;
  DuplicateGroup *groupB = nullptr;

  for (auto &group : analysis.duplicates) {
    if (group.paths.size() == 3) {
      groupA = &group;
    } else if (group.paths.size() == 2) {
      groupB = &group;
    }
  }

  REQUIRE(groupA != nullptr);
  REQUIRE(groupB != nullptr);
  REQUIRE(groupA->paths.size() == 3);
  REQUIRE(groupB->paths.size() == 2);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer hash is consistent across multiple runs",
          "[build_size_analyzer][hash]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  std::string content = "Test content for hash consistency";
  createTestFile(assetsDir + "/test.txt", content);

  BuildSizeAnalyzer analyzer1;
  analyzer1.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer1.setConfig(config);

  auto result1 = analyzer1.analyze();
  REQUIRE(result1.isOk());

  // Run again with a new analyzer instance
  BuildSizeAnalyzer analyzer2;
  analyzer2.setProjectPath(tempDir);
  analyzer2.setConfig(config);

  auto result2 = analyzer2.analyze();
  REQUIRE(result2.isOk());

  // The analysis should be identical
  auto analysis1 = result1.value();
  auto analysis2 = result2.value();

  REQUIRE(analysis1.totalFileCount == analysis2.totalFileCount);
  REQUIRE(analysis1.totalOriginalSize == analysis2.totalOriginalSize);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer handles empty files correctly",
          "[build_size_analyzer][hash]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // Create two empty files
  createTestFile(assetsDir + "/empty1.txt", "");
  createTestFile(assetsDir + "/empty2.txt", "");

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // Empty files should be detected as duplicates
  REQUIRE(analysis.duplicates.size() == 1);
  REQUIRE(analysis.duplicates[0].paths.size() == 2);
  REQUIRE(analysis.duplicates[0].singleFileSize == 0);

  cleanupTempDir(tempDir);
}

TEST_CASE("BuildSizeAnalyzer size mismatch prevents false duplicate detection",
          "[build_size_analyzer][hash][security]") {
  std::string tempDir = createTempDir();
  std::string assetsDir = tempDir + "/assets";
  fs::create_directories(assetsDir);

  // In case of a hypothetical hash collision (which SHA-256 should prevent),
  // the size check should still catch it
  std::string content1 = "Short";
  std::string content2 = "Much longer content";

  createTestFile(assetsDir + "/file1.txt", content1);
  createTestFile(assetsDir + "/file2.txt", content2);

  BuildSizeAnalyzer analyzer;
  analyzer.setProjectPath(tempDir);

  BuildSizeAnalysisConfig config;
  config.detectDuplicates = true;
  config.analyzeOther = true;
  analyzer.setConfig(config);

  auto result = analyzer.analyze();
  REQUIRE(result.isOk());

  auto analysis = result.value();

  // Different sizes mean no duplicates
  REQUIRE(analysis.duplicates.size() == 0);

  cleanupTempDir(tempDir);
}
