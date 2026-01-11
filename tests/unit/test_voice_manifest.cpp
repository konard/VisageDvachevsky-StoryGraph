/**
 * @file test_voice_manifest.cpp
 * @brief Unit tests for VoiceManifest class
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/audio/voice_manifest.hpp"
#include <filesystem>
#include <fstream>

using namespace NovelMind::audio;

namespace {
// Helper to create a test manifest
VoiceManifest createTestManifest() {
  VoiceManifest manifest;
  manifest.setProjectName("test_project");
  manifest.setDefaultLocale("en");
  manifest.addLocale("en");
  manifest.addLocale("ru");
  return manifest;
}

// Helper to create a test voice line
VoiceManifestLine createTestLine(const std::string &id = "test.line.001") {
  VoiceManifestLine line;
  line.id = id;
  line.textKey = "dialog." + id;
  line.speaker = "narrator";
  line.scene = "intro";
  line.tags = {"calm", "intro"};
  line.notes = "Speak softly";
  return line;
}
} // namespace

// ============================================================================
// Project Configuration Tests
// ============================================================================

TEST_CASE("VoiceManifest project configuration", "[voice_manifest]") {
  VoiceManifest manifest;

  SECTION("default values") {
    REQUIRE(manifest.getProjectName().empty());
    REQUIRE(manifest.getDefaultLocale() == "en");
    REQUIRE(manifest.getBasePath() == "assets/audio/voice");
  }

  SECTION("set project name") {
    manifest.setProjectName("my_novel");
    REQUIRE(manifest.getProjectName() == "my_novel");
  }

  SECTION("set default locale adds to locale list") {
    manifest.setDefaultLocale("ja");
    REQUIRE(manifest.getDefaultLocale() == "ja");
    REQUIRE(manifest.hasLocale("ja"));
  }

  SECTION("add and remove locales") {
    manifest.addLocale("en");
    manifest.addLocale("ru");
    manifest.addLocale("ja");

    REQUIRE(manifest.hasLocale("en"));
    REQUIRE(manifest.hasLocale("ru"));
    REQUIRE(manifest.hasLocale("ja"));
    REQUIRE_FALSE(manifest.hasLocale("fr"));

    manifest.removeLocale("ru");
    REQUIRE_FALSE(manifest.hasLocale("ru"));
    REQUIRE(manifest.hasLocale("en"));
  }

  SECTION("duplicate locales are ignored") {
    manifest.addLocale("en");
    manifest.addLocale("en");
    manifest.addLocale("en");

    auto locales = manifest.getLocales();
    int count = 0;
    for (const auto &loc : locales) {
      if (loc == "en")
        ++count;
    }
    REQUIRE(count == 1);
  }
}

// ============================================================================
// Voice Line Tests
// ============================================================================

TEST_CASE("VoiceManifest voice line operations", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();

  SECTION("add voice line") {
    VoiceManifestLine line = createTestLine();

    auto result = manifest.addLine(line);
    REQUIRE(result.isOk());
    REQUIRE(manifest.hasLine("test.line.001"));
    REQUIRE(manifest.getLineCount() == 1);
  }

  SECTION("add line with empty ID fails") {
    VoiceManifestLine line;
    line.textKey = "some.key";

    auto result = manifest.addLine(line);
    REQUIRE(result.isError());
  }

  SECTION("add duplicate line fails") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    auto result = manifest.addLine(line);
    REQUIRE(result.isError());
  }

  SECTION("get line by ID") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    const auto *retrieved = manifest.getLine("test.line.001");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "test.line.001");
    REQUIRE(retrieved->speaker == "narrator");
  }

  SECTION("get non-existent line returns nullptr") {
    const auto *retrieved = manifest.getLine("non.existent");
    REQUIRE(retrieved == nullptr);
  }

  SECTION("update existing line") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    line.speaker = "alex";
    auto result = manifest.updateLine(line);

    REQUIRE(result.isOk());
    const auto *updated = manifest.getLine("test.line.001");
    REQUIRE(updated->speaker == "alex");
  }

  SECTION("update non-existent line fails") {
    VoiceManifestLine line = createTestLine();

    auto result = manifest.updateLine(line);
    REQUIRE(result.isError());
  }

  SECTION("remove line") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    REQUIRE(manifest.hasLine("test.line.001"));
    manifest.removeLine("test.line.001");
    REQUIRE_FALSE(manifest.hasLine("test.line.001"));
  }

  SECTION("clear all lines") {
    manifest.addLine(createTestLine("line.001"));
    manifest.addLine(createTestLine("line.002"));
    manifest.addLine(createTestLine("line.003"));

    REQUIRE(manifest.getLineCount() == 3);
    manifest.clearLines();
    REQUIRE(manifest.getLineCount() == 0);
  }
}

// ============================================================================
// Filtering Tests
// ============================================================================

TEST_CASE("VoiceManifest line filtering", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();

  // Add various lines
  VoiceManifestLine line1 = createTestLine("intro.alex.001");
  line1.speaker = "alex";
  line1.scene = "intro";
  line1.tags = {"calm"};
  manifest.addLine(line1);

  VoiceManifestLine line2 = createTestLine("intro.beth.001");
  line2.speaker = "beth";
  line2.scene = "intro";
  line2.tags = {"excited"};
  manifest.addLine(line2);

  VoiceManifestLine line3 = createTestLine("chapter1.alex.001");
  line3.speaker = "alex";
  line3.scene = "chapter1";
  line3.tags = {"calm", "serious"};
  manifest.addLine(line3);

  SECTION("filter by speaker") {
    auto alexLines = manifest.getLinesBySpeaker("alex");
    REQUIRE(alexLines.size() == 2);

    auto bethLines = manifest.getLinesBySpeaker("beth");
    REQUIRE(bethLines.size() == 1);
  }

  SECTION("filter by scene") {
    auto introLines = manifest.getLinesByScene("intro");
    REQUIRE(introLines.size() == 2);

    auto chapter1Lines = manifest.getLinesByScene("chapter1");
    REQUIRE(chapter1Lines.size() == 1);
  }

  SECTION("filter by tag") {
    auto calmLines = manifest.getLinesByTag("calm");
    REQUIRE(calmLines.size() == 2);

    auto excitedLines = manifest.getLinesByTag("excited");
    REQUIRE(excitedLines.size() == 1);
  }

  SECTION("get unique speakers") {
    auto speakers = manifest.getSpeakers();
    REQUIRE(speakers.size() == 2);
  }

  SECTION("get unique scenes") {
    auto scenes = manifest.getScenes();
    REQUIRE(scenes.size() == 2);
  }

  SECTION("get unique tags") {
    auto tags = manifest.getTags();
    REQUIRE(tags.size() == 3); // calm, excited, serious
  }
}

// ============================================================================
// Status Management Tests
// ============================================================================

TEST_CASE("VoiceManifest status management", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();
  VoiceManifestLine line = createTestLine();
  manifest.addLine(line);

  SECTION("initial status is missing") {
    const auto *retrieved = manifest.getLine("test.line.001");
    REQUIRE(retrieved->getOverallStatus() == VoiceLineStatus::Missing);
  }

  SECTION("mark as recorded") {
    auto result = manifest.markAsRecorded("test.line.001", "en", "en/test.line.001.ogg");

    REQUIRE(result.isOk());
    const auto *retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file != nullptr);
    REQUIRE(file->status == VoiceLineStatus::Recorded);
    REQUIRE(file->filePath == "en/test.line.001.ogg");
  }

  SECTION("mark as imported") {
    auto result = manifest.markAsImported("test.line.001", "en", "imported/voice.ogg");

    REQUIRE(result.isOk());
    const auto *retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file->status == VoiceLineStatus::Imported);
  }

  SECTION("set status directly") {
    manifest.markAsRecorded("test.line.001", "en", "voice.ogg");

    auto result = manifest.setStatus("test.line.001", "en", VoiceLineStatus::NeedsReview);
    REQUIRE(result.isOk());

    const auto *retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file->status == VoiceLineStatus::NeedsReview);
  }

  SECTION("filter by status") {
    manifest.addLine(createTestLine("line.002"));
    manifest.addLine(createTestLine("line.003"));

    manifest.markAsRecorded("test.line.001", "en", "voice1.ogg");
    manifest.markAsImported("line.002", "en", "voice2.ogg");
    // line.003 remains missing

    auto missingLines = manifest.getLinesByStatus(VoiceLineStatus::Missing, "en");
    REQUIRE(missingLines.size() == 1);

    auto recordedLines = manifest.getLinesByStatus(VoiceLineStatus::Recorded, "en");
    REQUIRE(recordedLines.size() == 1);
  }
}

// ============================================================================
// Take Management Tests
// ============================================================================

TEST_CASE("VoiceManifest take management", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();
  VoiceManifestLine line = createTestLine();
  manifest.addLine(line);

  SECTION("add take") {
    VoiceTake take;
    take.takeNumber = 1;
    take.filePath = "en/test.line.001_take1.ogg";
    take.duration = 3.5f;

    auto result = manifest.addTake("test.line.001", "en", take);
    REQUIRE(result.isOk());

    auto takes = manifest.getTakes("test.line.001", "en");
    REQUIRE(takes.size() == 1);
    REQUIRE(takes[0].takeNumber == 1);
    REQUIRE(takes[0].isActive == true); // First take is auto-active
  }

  SECTION("multiple takes") {
    VoiceTake take1;
    take1.takeNumber = 1;
    take1.filePath = "take1.ogg";
    take1.duration = 3.0f;
    manifest.addTake("test.line.001", "en", take1);

    VoiceTake take2;
    take2.takeNumber = 2;
    take2.filePath = "take2.ogg";
    take2.duration = 3.5f;
    manifest.addTake("test.line.001", "en", take2);

    auto takes = manifest.getTakes("test.line.001", "en");
    REQUIRE(takes.size() == 2);
  }

  SECTION("set active take") {
    VoiceTake take1, take2;
    take1.takeNumber = 1;
    take1.filePath = "take1.ogg";
    take2.takeNumber = 2;
    take2.filePath = "take2.ogg";

    manifest.addTake("test.line.001", "en", take1);
    manifest.addTake("test.line.001", "en", take2);

    auto result = manifest.setActiveTake("test.line.001", "en", 1);
    REQUIRE(result.isOk());

    const auto *retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file->activeTakeIndex == 1);
    REQUIRE(file->filePath == "take2.ogg");
  }

  SECTION("invalid take index fails") {
    VoiceTake take;
    take.takeNumber = 1;
    manifest.addTake("test.line.001", "en", take);

    auto result = manifest.setActiveTake("test.line.001", "en", 5);
    REQUIRE(result.isError());
  }
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_CASE("VoiceManifest validation", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();

  SECTION("empty manifest is valid") {
    auto errors = manifest.validate();
    REQUIRE(errors.empty());
  }

  SECTION("valid manifest") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    auto errors = manifest.validate();
    REQUIRE(errors.empty());
  }

  SECTION("missing required field") {
    VoiceManifestLine line;
    line.id = "test.line";
    // textKey is missing
    manifest.addLine(line);

    auto errors = manifest.validate();
    REQUIRE_FALSE(errors.empty());

    bool foundMissingField = false;
    for (const auto &error : errors) {
      if (error.type == ManifestValidationError::Type::MissingRequiredField) {
        foundMissingField = true;
        break;
      }
    }
    REQUIRE(foundMissingField);
  }

  SECTION("invalid locale in files") {
    VoiceManifestLine line = createTestLine();
    VoiceLocaleFile file;
    file.locale = "fr"; // Not in manifest locales
    file.filePath = "fr/voice.ogg";
    line.files["fr"] = file;
    manifest.addLine(line);

    auto errors = manifest.validate();
    REQUIRE_FALSE(errors.empty());

    bool foundInvalidLocale = false;
    for (const auto &error : errors) {
      if (error.type == ManifestValidationError::Type::InvalidLocale) {
        foundInvalidLocale = true;
        break;
      }
    }
    REQUIRE(foundInvalidLocale);
  }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("VoiceManifest coverage statistics", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();

  manifest.addLine(createTestLine("line.001"));
  manifest.addLine(createTestLine("line.002"));
  manifest.addLine(createTestLine("line.003"));
  manifest.addLine(createTestLine("line.004"));

  manifest.markAsRecorded("line.001", "en", "voice1.ogg");
  manifest.markAsImported("line.002", "en", "voice2.ogg");
  manifest.setStatus("line.003", "en", VoiceLineStatus::Approved);
  // line.004 remains missing

  SECTION("overall stats") {
    auto stats = manifest.getCoverageStats("en");

    REQUIRE(stats.totalLines == 4);
    REQUIRE(stats.recordedLines == 1);
    REQUIRE(stats.importedLines == 1);
    REQUIRE(stats.approvedLines == 1);
    REQUIRE(stats.missingLines == 1);
    REQUIRE(stats.coveragePercent == 75.0f);
  }
}

// ============================================================================
// Naming Convention Tests
// ============================================================================

TEST_CASE("NamingConvention path generation", "[voice_manifest]") {
  SECTION("locale/id based") {
    NamingConvention conv = NamingConvention::localeIdBased();
    std::string path = conv.generatePath("en", "intro.alex.001", "intro", "alex", 1);
    REQUIRE(path == "en/intro.alex.001.ogg");
  }

  SECTION("scene/speaker based") {
    NamingConvention conv = NamingConvention::sceneSpeakerBased();
    std::string path = conv.generatePath("en", "intro.alex.001", "intro", "alex", 2);
    REQUIRE(path == "intro/alex/intro.alex.001_take2.ogg");
  }

  SECTION("flat by ID") {
    NamingConvention conv = NamingConvention::flatById();
    std::string path = conv.generatePath("ru", "intro.alex.001", "", "", 1);
    REQUIRE(path == "voice/intro.alex.001_ru.ogg");
  }
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_CASE("VoiceManifest JSON serialization", "[voice_manifest]") {
  VoiceManifest manifest = createTestManifest();

  VoiceManifestLine line = createTestLine();
  VoiceLocaleFile enFile;
  enFile.locale = "en";
  enFile.filePath = "en/test.line.001.ogg";
  enFile.status = VoiceLineStatus::Recorded;
  line.files["en"] = enFile;
  manifest.addLine(line);

  SECTION("to JSON string") {
    auto result = manifest.toJsonString();
    REQUIRE(result.isOk());

    std::string json = result.value();
    REQUIRE(json.find("\"project\": \"test_project\"") != std::string::npos);
    REQUIRE(json.find("\"test.line.001\"") != std::string::npos);
    REQUIRE(json.find("\"en\"") != std::string::npos);
  }

  SECTION("round-trip serialization") {
    auto jsonResult = manifest.toJsonString();
    REQUIRE(jsonResult.isOk());

    VoiceManifest loadedManifest;
    auto loadResult = loadedManifest.loadFromString(jsonResult.value());
    REQUIRE(loadResult.isOk());

    REQUIRE(loadedManifest.getProjectName() == manifest.getProjectName());
    REQUIRE(loadedManifest.getDefaultLocale() == manifest.getDefaultLocale());
    REQUIRE(loadedManifest.getLineCount() == manifest.getLineCount());

    const auto *loadedLine = loadedManifest.getLine("test.line.001");
    REQUIRE(loadedLine != nullptr);
    REQUIRE(loadedLine->speaker == "narrator");
  }
}

// ============================================================================
// Status String Conversion Tests
// ============================================================================

TEST_CASE("VoiceLineStatus string conversion", "[voice_manifest]") {
  SECTION("to string") {
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::Missing)) == "missing");
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::Recorded)) == "recorded");
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::Imported)) == "imported");
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::NeedsReview)) ==
            "needs_review");
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::Approved)) == "approved");
  }

  SECTION("from string") {
    REQUIRE(voiceLineStatusFromString("missing") == VoiceLineStatus::Missing);
    REQUIRE(voiceLineStatusFromString("recorded") == VoiceLineStatus::Recorded);
    REQUIRE(voiceLineStatusFromString("imported") == VoiceLineStatus::Imported);
    REQUIRE(voiceLineStatusFromString("needs_review") == VoiceLineStatus::NeedsReview);
    REQUIRE(voiceLineStatusFromString("approved") == VoiceLineStatus::Approved);
    REQUIRE(voiceLineStatusFromString("unknown") == VoiceLineStatus::Missing);
  }
}

// ============================================================================
// Malformed Input Error Handling Tests
// ============================================================================

TEST_CASE("VoiceManifest JSON malformed input handling", "[voice_manifest][error_handling]") {
  VoiceManifest manifest;

  SECTION("empty JSON content") {
    auto result = manifest.loadFromString("");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("empty") != std::string::npos);
  }

  SECTION("mismatched braces") {
    std::string malformedJson = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"]
    )"; // Missing closing brace

    auto result = manifest.loadFromString(malformedJson);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("mismatched braces") != std::string::npos);
  }

  SECTION("mismatched brackets") {
    std::string malformedJson = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"
    })"; // Missing closing bracket

    auto result = manifest.loadFromString(malformedJson);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("mismatched brackets") != std::string::npos);
  }

  SECTION("lines array without closing bracket") {
    std::string malformedJson = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {"id": "test.001", "text_key": "key.001"}
    })"; // Missing array closing bracket

    auto result = manifest.loadFromString(malformedJson);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("not properly closed") != std::string::npos);
  }

  SECTION("missing default locale falls back to 'en'") {
    std::string jsonWithoutLocale = R"({
      "project": "test",
      "locales": ["ru"]
    })";

    auto result = manifest.loadFromString(jsonWithoutLocale);
    REQUIRE(result.isOk());
    REQUIRE(manifest.getDefaultLocale() == "en");
  }

  SECTION("missing locales array defaults to default locale") {
    std::string jsonWithoutLocales = R"({
      "project": "test",
      "default_locale": "ru"
    })";

    auto result = manifest.loadFromString(jsonWithoutLocales);
    REQUIRE(result.isOk());
    REQUIRE(manifest.hasLocale("ru"));
  }

  SECTION("voice line without ID is skipped") {
    std::string jsonMissingId = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {"text_key": "key.001", "speaker": "alex"},
        {"id": "valid.001", "text_key": "key.002"}
      ]
    })";

    auto result = manifest.loadFromString(jsonMissingId);
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);
    REQUIRE(manifest.hasLine("valid.001"));
  }

  SECTION("voice line without text_key defaults to id") {
    std::string jsonMissingTextKey = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {"id": "test.001", "speaker": "alex"}
      ]
    })";

    auto result = manifest.loadFromString(jsonMissingTextKey);
    REQUIRE(result.isOk());
    const auto *line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->textKey == "test.001");
  }

  SECTION("duplicate line IDs are handled gracefully") {
    std::string jsonDuplicateIds = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {"id": "test.001", "text_key": "key.001"},
        {"id": "test.001", "text_key": "key.002"}
      ]
    })";

    auto result = manifest.loadFromString(jsonDuplicateIds);
    REQUIRE(result.isOk());
    // Second duplicate should be skipped with warning
    REQUIRE(manifest.getLineCount() == 1);
  }

  SECTION("empty lines array is valid") {
    std::string jsonEmptyLines = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": []
    })";

    auto result = manifest.loadFromString(jsonEmptyLines);
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 0);
  }

  SECTION("missing lines array is valid") {
    std::string jsonNoLines = R"({
      "project": "test",
      "default_locale": "en",
      "locales": ["en"]
    })";

    auto result = manifest.loadFromString(jsonNoLines);
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 0);
  }
}

TEST_CASE("VoiceManifest CSV malformed input handling", "[voice_manifest][error_handling]") {
  VoiceManifest manifest = createTestManifest();

  SECTION("non-existent CSV file") {
    auto result = manifest.importFromCsv("/nonexistent/file.csv", "en");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Failed to open") != std::string::npos);
  }

  SECTION("empty locale parameter") {
    // Create a temporary CSV file
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_empty_locale.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,alex,key.001,voice.ogg,intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Locale cannot be empty") != std::string::npos);

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with missing ID column") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_missing_id.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "speaker,text_key,voice_file\n";
    tempFile << "alex,key.001,voice.ogg\n";
    tempFile.close();

    // Should still process but with warning
    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk()); // Doesn't fail completely

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with quoted fields containing commas") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_quoted_commas.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,\"Smith, John\",key.001,\"path/with,comma.ogg\",intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    const auto *line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->speaker == "Smith, John");

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with Windows line endings") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_windows_endings.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\r\n";
    tempFile << "test.001,alex,key.001,voice.ogg,intro\r\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with empty lines") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_empty_lines.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "\n";
    tempFile << "test.001,alex,key.001,voice.ogg,intro\n";
    tempFile << "\n";
    tempFile << "test.002,beth,key.002,voice2.ogg,intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 2);

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with missing required fields") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_missing_fields.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << ",alex,key.001,voice.ogg,intro\n"; // Missing ID
    tempFile << "test.002,beth,key.002,voice2.ogg,intro\n"; // Valid
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    // Only the valid line should be imported
    REQUIRE(manifest.getLineCount() == 1);
    REQUIRE(manifest.hasLine("test.002"));

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with only header") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_only_header.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 0);

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with escaped quotes") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_escaped_quotes.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,\"Alex \"\"The Great\"\"\",key.001,voice.ogg,intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    const auto *line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->speaker == "Alex \"The Great\"");

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV import defaults text_key to id when missing") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_default_textkey.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,alex,,voice.ogg,intro\n"; // Empty text_key
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());

    const auto *line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->textKey == "test.001"); // Defaulted to ID

    std::filesystem::remove(tempPath);
  }
}

TEST_CASE("VoiceManifest file loading error handling", "[voice_manifest][error_handling]") {
  VoiceManifest manifest;

  SECTION("non-existent file") {
    auto result = manifest.loadFromFile("/nonexistent/path/file.json");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Failed to open") != std::string::npos);
  }

  SECTION("empty file") {
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "test_empty.json";
    std::ofstream tempFile(tempPath);
    tempFile.close();

    auto result = manifest.loadFromFile(tempPath.string());
    REQUIRE(result.isError());
    REQUIRE(result.error().find("empty") != std::string::npos);

    std::filesystem::remove(tempPath);
  }
}
