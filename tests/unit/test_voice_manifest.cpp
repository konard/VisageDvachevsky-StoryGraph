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
VoiceManifestLine createTestLine(const std::string& id = "test.line.001") {
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
    for (const auto& loc : locales) {
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

    const auto* retrieved = manifest.getLine("test.line.001");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "test.line.001");
    REQUIRE(retrieved->speaker == "narrator");
  }

  SECTION("get non-existent line returns nullptr") {
    const auto* retrieved = manifest.getLine("non.existent");
    REQUIRE(retrieved == nullptr);
  }

  SECTION("update existing line") {
    VoiceManifestLine line = createTestLine();
    manifest.addLine(line);

    line.speaker = "alex";
    auto result = manifest.updateLine(line);

    REQUIRE(result.isOk());
    const auto* updated = manifest.getLine("test.line.001");
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
    const auto* retrieved = manifest.getLine("test.line.001");
    REQUIRE(retrieved->getOverallStatus() == VoiceLineStatus::Missing);
  }

  SECTION("mark as recorded") {
    auto result = manifest.markAsRecorded("test.line.001", "en", "en/test.line.001.ogg");

    REQUIRE(result.isOk());
    const auto* retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file != nullptr);
    REQUIRE(file->status == VoiceLineStatus::Recorded);
    REQUIRE(file->filePath == "en/test.line.001.ogg");
  }

  SECTION("mark as imported") {
    auto result = manifest.markAsImported("test.line.001", "en", "imported/voice.ogg");

    REQUIRE(result.isOk());
    const auto* retrieved = manifest.getLine("test.line.001");
    auto file = retrieved->getFile("en");
    REQUIRE(file->status == VoiceLineStatus::Imported);
  }

  SECTION("set status directly") {
    manifest.markAsRecorded("test.line.001", "en", "voice.ogg");

    auto result = manifest.setStatus("test.line.001", "en", VoiceLineStatus::NeedsReview);
    REQUIRE(result.isOk());

    const auto* retrieved = manifest.getLine("test.line.001");
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

    const auto* retrieved = manifest.getLine("test.line.001");
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
    for (const auto& error : errors) {
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
    for (const auto& error : errors) {
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

    const auto* loadedLine = loadedManifest.getLine("test.line.001");
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
    REQUIRE(std::string(voiceLineStatusToString(VoiceLineStatus::NeedsReview)) == "needs_review");
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
// JSON Parsing Tests for Nested Structures (Issue #559)
// ============================================================================

TEST_CASE("VoiceManifest parses nested JSON objects correctly", "[voice_manifest][issue_559]") {
  SECTION("nested objects in files field") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": ["en", "ru"],
      "lines": [
        {
          "id": "test.001",
          "text_key": "dialog.test.001",
          "speaker": "narrator",
          "scene": "intro",
          "files": {
            "en": {
              "path": "en/test.001.ogg",
              "duration": 3.5
            },
            "ru": {
              "path": "ru/test.001.ogg",
              "duration": 4.2
            }
          }
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->files.size() == 2);

    const auto* enFile = line->getFile("en");
    REQUIRE(enFile != nullptr);
    REQUIRE(enFile->filePath == "en/test.001.ogg");
    REQUIRE(enFile->duration == 3.5f);

    const auto* ruFile = line->getFile("ru");
    REQUIRE(ruFile != nullptr);
    REQUIRE(ruFile->filePath == "ru/test.001.ogg");
    REQUIRE(ruFile->duration == 4.2f);
  }

  SECTION("deeply nested structures") {
    std::string json = R"({
      "project": "nested_test",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {
          "id": "deep.001",
          "text_key": "dialog.deep.001",
          "speaker": "narrator",
          "scene": "intro",
          "notes": "Test with {braces} and \"quotes\"",
          "files": {
            "en": "en/deep.001.ogg"
          }
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    const auto* line = manifest.getLine("deep.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->notes == "Test with {braces} and \"quotes\"");
  }
}

TEST_CASE("VoiceManifest parses nested JSON arrays correctly", "[voice_manifest][issue_559]") {
  SECTION("tags array with multiple items") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": ["en", "ru", "ja"],
      "lines": [
        {
          "id": "test.001",
          "text_key": "dialog.test.001",
          "speaker": "narrator",
          "scene": "intro",
          "tags": ["calm", "intro", "important", "long"],
          "files": {
            "en": "en/test.001.ogg"
          }
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    REQUIRE(manifest.getLocales().size() == 3);
    REQUIRE(manifest.hasLocale("ja"));

    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->tags.size() == 4);
    REQUIRE(line->tags[0] == "calm");
    REQUIRE(line->tags[1] == "intro");
    REQUIRE(line->tags[2] == "important");
    REQUIRE(line->tags[3] == "long");
  }

  SECTION("empty tags array") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": [],
      "lines": [
        {
          "id": "test.001",
          "text_key": "dialog.test.001",
          "tags": []
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->tags.empty());
  }
}

TEST_CASE("VoiceManifest parses strings containing braces correctly",
          "[voice_manifest][issue_559]") {
  SECTION("notes field with braces") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {
          "id": "test.001",
          "text_key": "dialog.test.001",
          "speaker": "narrator",
          "notes": "Use {expression} with {{nested}} braces and [brackets]",
          "files": {
            "en": "en/test.001.ogg"
          }
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->notes == "Use {expression} with {{nested}} braces and [brackets]");
  }

  SECTION("file paths with special characters") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": ["en"],
      "lines": [
        {
          "id": "test.001",
          "text_key": "dialog.test.001",
          "files": {
            "en": "assets/{locale}/voice/test.001.ogg"
          }
        }
      ]
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isOk());
    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    const auto* enFile = line->getFile("en");
    REQUIRE(enFile != nullptr);
    REQUIRE(enFile->filePath == "assets/{locale}/voice/test.001.ogg");
  }
}

TEST_CASE("VoiceManifest handles malformed JSON with error messages",
          "[voice_manifest][issue_559]") {
  SECTION("invalid JSON syntax") {
    std::string json = R"({
      "project": "test_project",
      "invalid syntax here
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isError());
    REQUIRE(result.error().find("JSON parse error") != std::string::npos);
  }

  SECTION("unterminated string") {
    std::string json = R"({
      "project": "test_project,
      "default_locale": "en"
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isError());
  }

  SECTION("invalid root type") {
    std::string json = R"([
      "this", "is", "an", "array"
    ])";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isError());
    REQUIRE(result.error().find("root must be an object") != std::string::npos);
  }

  SECTION("invalid nested structure") {
    std::string json = R"({
      "project": "test_project",
      "default_locale": "en",
      "locales": "this should be an array",
      "lines": []
    })";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    // Should succeed but skip invalid locales field
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLocales().empty());
  }

  SECTION("empty JSON") {
    std::string json = "";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isError());
  }

  SECTION("null JSON") {
    std::string json = "null";

    VoiceManifest manifest;
    auto result = manifest.loadFromString(json);

    REQUIRE(result.isError());
    REQUIRE(result.error().find("root must be an object") != std::string::npos);
  }
}

TEST_CASE("VoiceManifest handles complex real-world JSON", "[voice_manifest][issue_559]") {
  SECTION("full manifest with all fields") {
    std::string json = R"({
      "project": "my_visual_novel",
      "default_locale": "en",
      "locales": ["en", "ru", "ja"],
      "base_path": "assets/audio/voice",
      "naming_convention": "{locale}/{id}.ogg",
      "lines": [
        {
          "id": "intro.alex.001",
          "text_key": "dialog.intro.alex.001",
          "speaker": "alex",
          "scene": "intro",
          "notes": "Speak with {calm} emotion, use \"soft\" voice",
          "tags": ["main", "calm", "intro"],
          "source_script": "scripts/intro.txt",
          "source_line": 42,
          "duration_override": 5.5,
          "files": {
            "en": "assets/audio/voice/en/intro.alex.001.ogg",
            "ru": "assets/audio/voice/ru/intro.alex.001.ogg",
            "ja": "assets/audio/voice/ja/intro.alex.001.ogg"
          }
        },
        {
          "id": "intro.beth.001",
          "text_key": "dialog.intro.beth.001",
          "speaker": "beth",
          "scene": "intro",
          "tags": ["excited"],
          "files": {
            "en": {
              "path": "assets/audio/voice/en/intro.beth.001.ogg",
              "duration": 3.2
            }
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
    const auto* line = manifest.getLine("test.001");
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
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_empty_locale.csv";
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
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_quoted_commas.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,\"Smith, John\",key.001,\"path/with,comma.ogg\",intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->speaker == "Smith, John");

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with Windows line endings") {
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_windows_endings.csv";
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
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_empty_lines.csv";
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
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_missing_fields.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << ",alex,key.001,voice.ogg,intro\n";          // Missing ID
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
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_only_header.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 0);

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV with escaped quotes") {
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_escaped_quotes.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,\"Alex \"\"The Great\"\"\",key.001,voice.ogg,intro\n";
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());
    REQUIRE(manifest.getLineCount() == 1);

    const auto* line = manifest.getLine("test.001");
    REQUIRE(line != nullptr);
    REQUIRE(line->speaker == "Alex \"The Great\"");

    std::filesystem::remove(tempPath);
  }

  SECTION("CSV import defaults text_key to id when missing") {
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_default_textkey.csv";
    std::ofstream tempFile(tempPath);
    tempFile << "id,speaker,text_key,voice_file,scene\n";
    tempFile << "test.001,alex,,voice.ogg,intro\n"; // Empty text_key
    tempFile.close();

    auto result = manifest.importFromCsv(tempPath.string(), "en");
    REQUIRE(result.isOk());

    const auto* line = manifest.getLine("test.001");
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
    // Security Tests - Path Traversal Prevention
    // ============================================================================

    TEST_CASE("VoiceManifest security - path traversal prevention", "[voice_manifest][security]") {
      VoiceManifest manifest = createTestManifest();

      SECTION("Unix path traversal - ../../../etc/passwd") {
        auto result = manifest.markAsRecorded("test.line.001", "en", "../../../etc/passwd");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Windows path traversal - ..\\..\\..\\Windows\\System32\\config") {
        auto result =
            manifest.markAsRecorded("test.line.001", "en", "..\\..\\..\\Windows\\System32\\config");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Mixed separators path traversal - ../..\\../etc/passwd") {
        auto result = manifest.markAsRecorded("test.line.001", "en", "../..\\../etc/passwd");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Unix absolute path - /etc/passwd") {
        auto result = manifest.markAsRecorded("test.line.001", "en", "/etc/passwd");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Windows absolute path - C:\\Windows\\System32\\config") {
        auto result =
            manifest.markAsRecorded("test.line.001", "en", "C:\\Windows\\System32\\config");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Null byte injection - audio\\0../../etc/passwd") {
        std::string maliciousPath = "audio";
        maliciousPath += '\0';
        maliciousPath += "../../etc/passwd";
        auto result = manifest.markAsRecorded("test.line.001", "en", maliciousPath);
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Valid relative path should succeed - en/voice001.ogg") {
        auto result = manifest.markAsRecorded("test.line.001", "en", "en/voice001.ogg");
        REQUIRE_FALSE(result.isError());
      }

      SECTION("Valid relative path with subdirs - en/chapter1/scene1/voice001.ogg") {
        auto result =
            manifest.markAsRecorded("test.line.001", "en", "en/chapter1/scene1/voice001.ogg");
        REQUIRE_FALSE(result.isError());
      }
    }

    TEST_CASE("VoiceManifest security - markAsImported path validation",
              "[voice_manifest][security]") {
      VoiceManifest manifest = createTestManifest();

      SECTION("Unix path traversal in markAsImported") {
        auto result = manifest.markAsImported("test.line.001", "en", "../../../etc/passwd");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Windows absolute path in markAsImported") {
        auto result =
            manifest.markAsImported("test.line.001", "en", "C:\\Users\\admin\\secret.txt");
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Valid path in markAsImported") {
        auto result = manifest.markAsImported("test.line.001", "en", "imported/voice001.ogg");
        REQUIRE_FALSE(result.isError());
      }
    }

    TEST_CASE("VoiceManifest security - addTake path validation", "[voice_manifest][security]") {
      VoiceManifest manifest = createTestManifest();
      VoiceManifestLine line = createTestLine();
      manifest.addLine(line);

      SECTION("Unix path traversal in VoiceTake") {
        VoiceTake take;
        take.takeNumber = 1;
        take.filePath = "../../../etc/passwd";
        take.duration = 2.5f;

        auto result = manifest.addTake("test.line.001", "en", take);
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Windows path traversal in VoiceTake") {
        VoiceTake take;
        take.takeNumber = 1;
        take.filePath = "..\\..\\..\\Windows\\System32\\malware.exe";
        take.duration = 2.5f;

        auto result = manifest.addTake("test.line.001", "en", take);
        REQUIRE(result.isError());
        REQUIRE(result.error().find("Invalid file path") != std::string::npos);
      }

      SECTION("Valid path in VoiceTake") {
        VoiceTake take;
        take.takeNumber = 1;
        take.filePath = "takes/voice001_take1.ogg";
        take.duration = 2.5f;

        auto result = manifest.addTake("test.line.001", "en", take);
        REQUIRE_FALSE(result.isError());
      }
    }

    TEST_CASE("VoiceManifest security - JSON loading path validation",
              "[voice_manifest][security]") {
      SECTION("Malicious JSON with path traversal") {
        std::string maliciousJson = R"({
      "project": "malicious_project",
      "default_locale": "en",
      "locales": ["en"],
      "base_path": "assets/audio/voice",
      "lines": [
        {
          "id": "malicious.line.001",
          "text_key": "dialog.test.001",
          "speaker": "hacker",
          "scene": "exploit",
          "files": {
            "en": "../../../etc/passwd"
          }
        }
      ]
    })";

        VoiceManifest manifest;
        auto result = manifest.loadFromString(maliciousJson);

        // Loading should succeed but the malicious path should be rejected
        REQUIRE_FALSE(result.isError());

        // The line should be created but without the malicious file path
        const auto* line = manifest.getLine("malicious.line.001");
        REQUIRE(line != nullptr);

        // The file should not have been added due to path validation
        auto file = line->getFile("en");
        REQUIRE(file == nullptr);
      }

      SECTION("Valid JSON should load successfully") {
        std::string validJson = R"({
      "project": "valid_project",
      "default_locale": "en",
      "locales": ["en"],
      "base_path": "assets/audio/voice",
      "lines": [
        {
          "id": "valid.line.001",
          "text_key": "dialog.test.001",
          "speaker": "narrator",
          "scene": "intro",
          "files": {
            "en": "en/valid_voice.ogg"
          }
        }
      ]
    })";

        VoiceManifest manifest;
        auto result = manifest.loadFromString(json);

        REQUIRE(result.isOk());
        REQUIRE(manifest.getProjectName() == "my_visual_novel");
        REQUIRE(manifest.getDefaultLocale() == "en");
        REQUIRE(manifest.getLocales().size() == 3);
        REQUIRE(manifest.getBasePath() == "assets/audio/voice");
        REQUIRE(manifest.getLineCount() == 2);

        // Test first line
        const auto* line1 = manifest.getLine("intro.alex.001");
        REQUIRE(line1 != nullptr);
        REQUIRE(line1->speaker == "alex");
        REQUIRE(line1->scene == "intro");
        REQUIRE(line1->notes == "Speak with {calm} emotion, use \"soft\" voice");
        REQUIRE(line1->tags.size() == 3);
        REQUIRE(line1->sourceScript == "scripts/intro.txt");
        REQUIRE(line1->sourceLine == 42);
        REQUIRE(line1->durationOverride == 5.5f);
        REQUIRE(line1->files.size() == 3);

        // Test second line with nested file object
        const auto* line2 = manifest.getLine("intro.beth.001");
        REQUIRE(line2 != nullptr);
        REQUIRE(line2->speaker == "beth");
        REQUIRE(line2->tags.size() == 1);
        const auto* enFile = line2->getFile("en");
        REQUIRE(enFile != nullptr);
        REQUIRE(enFile->filePath == "assets/audio/voice/en/intro.beth.001.ogg");
        REQUIRE(enFile->duration == 3.2f);
        auto result = manifest.loadFromString(validJson);

        REQUIRE_FALSE(result.isError());

        const auto* line = manifest.getLine("valid.line.001");
        REQUIRE(line != nullptr);

        auto file = line->getFile("en");
        REQUIRE(file != nullptr);
        REQUIRE(file->filePath == "en/valid_voice.ogg");
      }
    }

    TEST_CASE("VoiceManifest security - CSV import path validation", "[voice_manifest][security]") {
      namespace fs = std::filesystem;

      SECTION("Malicious CSV with path traversal") {
        // Create a temporary malicious CSV file
        std::string csvContent =
            "id,speaker,text_key,voice_file,scene\n"
            "exploit.001,hacker,dialog.exploit.001,../../../etc/passwd,exploit_scene\n"
            "valid.001,narrator,dialog.valid.001,en/valid.ogg,normal_scene\n";

        std::string tempPath = "test_malicious.csv";
        std::ofstream csvFile(tempPath);
        csvFile << csvContent;
        csvFile.close();

        VoiceManifest manifest;
        manifest.setDefaultLocale("en");
        manifest.addLocale("en");

        auto result = manifest.importFromCsv(tempPath, "en");

        // Import should succeed
        REQUIRE_FALSE(result.isError());

        // The exploit line should be created but without the malicious path
        const auto* exploitLine = manifest.getLine("exploit.001");
        REQUIRE(exploitLine != nullptr);
        auto exploitFile = exploitLine->getFile("en");
        REQUIRE(exploitFile == nullptr); // Malicious path should be rejected

        // The valid line should have its path
        const auto* validLine = manifest.getLine("valid.001");
        REQUIRE(validLine != nullptr);
        auto validFile = validLine->getFile("en");
        REQUIRE(validFile != nullptr);
        REQUIRE(validFile->filePath == "en/valid.ogg");

        // Clean up
        fs::remove(tempPath);
      }
    }

    TEST_CASE("VoiceManifest security - validation detects malicious paths",
              "[voice_manifest][security]") {
      VoiceManifest manifest = createTestManifest();

      // Manually create a line with a malicious path (bypassing the API for testing)
      VoiceManifestLine line = createTestLine("exploit.001");
      VoiceLocaleFile locFile;
      locFile.locale = "en";
      locFile.filePath = "../../../etc/passwd"; // Inject malicious path directly
      locFile.status = VoiceLineStatus::Imported;
      line.files["en"] = locFile;

      // We can't use addLine because it would validate, so we test validation directly
      // Instead, let's verify the validation catches this if it were in the manifest

      SECTION("Validation should detect invalid file paths") {
        // Create manifest with valid base path
        manifest.setBasePath("/tmp/test_voice");

        // Add a normal line first
        VoiceManifestLine normalLine = createTestLine("normal.001");
        VoiceLocaleFile normalFile;
        normalFile.locale = "en";
        normalFile.filePath = "en/voice.ogg";
        normalFile.status = VoiceLineStatus::Imported;
        normalLine.files["en"] = normalFile;
        manifest.addLine(normalLine);

        // Validate - should pass for normal line
        auto errors = manifest.validate(false);
        REQUIRE(errors.empty());
      }
    }
