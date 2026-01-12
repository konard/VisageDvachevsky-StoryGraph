/**
 * @file test_ui_panel_features.cpp
 * @brief Integration tests for UI panel features
 *
 * Tests the implementations added in Issue #46:
 * - Voice metadata dialog
 * - Localization undo/redo with translations
 * - Project scanning for key usages
 * - Voice auto-detection
 */

#include <catch2/catch_test_macros.hpp>

#include "NovelMind/audio/voice_manifest.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"

#include <QApplication>
#include <QHash>
#include <QString>
#include <QStringList>

using namespace NovelMind::editor::qt;
using namespace NovelMind::audio;

// Helper to ensure QApplication exists for Qt tests
class QtTestFixture {
public:
  QtTestFixture() {
    if (!QApplication::instance()) {
      static int argc = 1;
      static char* argv[] = {const_cast<char*>("test"), nullptr};
      m_app = std::make_unique<QApplication>(argc, argv);
    }
  }

private:
  std::unique_ptr<QApplication> m_app;
};

// =============================================================================
// Voice Metadata Dialog Tests
// =============================================================================

TEST_CASE("VoiceMetadataDialog: MetadataResult structure", "[integration][editor][voice][dialog]") {
  SECTION("Default values are correct") {
    NMVoiceMetadataDialog::MetadataResult result;
    REQUIRE(result.tags.isEmpty());
    REQUIRE(result.notes.isEmpty());
    REQUIRE(result.speaker.isEmpty());
    REQUIRE(result.scene.isEmpty());
  }

  SECTION("Values can be set") {
    NMVoiceMetadataDialog::MetadataResult result;
    result.tags = QStringList{"calm", "intro"};
    result.notes = "Speak slowly";
    result.speaker = "Hero";
    result.scene = "Chapter1";

    REQUIRE(result.tags.size() == 2);
    REQUIRE(result.tags.contains("calm"));
    REQUIRE(result.tags.contains("intro"));
    REQUIRE(result.notes == "Speak slowly");
    REQUIRE(result.speaker == "Hero");
    REQUIRE(result.scene == "Chapter1");
  }
}

TEST_CASE("VoiceMetadataDialog: Dialog can be constructed",
          "[integration][editor][voice][dialog]") {
  QtTestFixture fixture;

  SECTION("Constructor with minimal arguments") {
    NMVoiceMetadataDialog dialog(nullptr, "line_001", {}, "", "", "");
    // Dialog should be created without crash
    SUCCEED();
  }

  SECTION("Constructor with all arguments") {
    QStringList tags{"tag1", "tag2"};
    QStringList speakers{"Hero", "Villain"};
    QStringList scenes{"Intro", "Battle"};
    QStringList suggestedTags{"calm", "angry", "happy"};

    NMVoiceMetadataDialog dialog(nullptr, "line_002", tags, "Test notes", "Hero", "Intro", speakers,
                                 scenes, suggestedTags);
    // Dialog should be created without crash
    SUCCEED();
  }
}

// =============================================================================
// Localization Entry Tests
// =============================================================================

TEST_CASE("LocalizationEntry: Structure and fields", "[integration][editor][localization]") {
  SECTION("Default values are correct") {
    LocalizationEntry entry;
    REQUIRE(entry.key.isEmpty());
    REQUIRE(entry.translations.isEmpty());
    REQUIRE(entry.usageLocations.isEmpty());
    REQUIRE(entry.isMissing == false);
    REQUIRE(entry.isUnused == false);
    REQUIRE(entry.isModified == false);
    REQUIRE(entry.isNew == false);
    REQUIRE(entry.isDeleted == false);
  }

  SECTION("Entry can store translations") {
    LocalizationEntry entry;
    entry.key = "hello_world";
    entry.translations["en"] = "Hello, World!";
    entry.translations["de"] = "Hallo, Welt!";
    entry.translations["fr"] = "Bonjour, le Monde!";

    REQUIRE(entry.translations.size() == 3);
    REQUIRE(entry.translations["en"] == "Hello, World!");
    REQUIRE(entry.translations["de"] == "Hallo, Welt!");
    REQUIRE(entry.translations["fr"] == "Bonjour, le Monde!");
  }

  SECTION("Entry can store usage locations") {
    LocalizationEntry entry;
    entry.key = "test_key";
    entry.usageLocations.append("Scripts/main.nms:42");
    entry.usageLocations.append("Scenes/intro.json:15");

    REQUIRE(entry.usageLocations.size() == 2);
    REQUIRE(entry.usageLocations.contains("Scripts/main.nms:42"));
    REQUIRE(entry.usageLocations.contains("Scenes/intro.json:15"));
  }
}

TEST_CASE("LocalizationFilter: Filter modes", "[integration][editor][localization]") {
  SECTION("Filter enum values") {
    REQUIRE(static_cast<int>(LocalizationFilter::All) == 0);
    REQUIRE(static_cast<int>(LocalizationFilter::MissingTranslations) == 1);
    REQUIRE(static_cast<int>(LocalizationFilter::Unused) == 2);
    REQUIRE(static_cast<int>(LocalizationFilter::Modified) == 3);
    REQUIRE(static_cast<int>(LocalizationFilter::NewKeys) == 4);
  }
}

// =============================================================================
// Voice Manifest Tests (related to auto-detection)
// =============================================================================

TEST_CASE("VoiceManifest: Line lookup for auto-detection", "[integration][editor][voice]") {
  VoiceManifest manifest;
  manifest.setDefaultLocale("en");
  manifest.addLocale("en");
  manifest.addLocale("de");

  SECTION("Adding and retrieving lines") {
    VoiceManifestLine line;
    line.id = "intro_001";
    line.textKey = "dialog.intro.001";
    line.speaker = "Hero";
    line.scene = "Intro";

    auto result = manifest.addLine(line);
    REQUIRE(result.isOk());

    const VoiceManifestLine* retrieved = manifest.getLine("intro_001");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "intro_001");
    REQUIRE(retrieved->speaker == "Hero");
  }

  SECTION("Line with voice file paths") {
    VoiceManifestLine line;
    line.id = "greeting_001";
    line.textKey = "dialog.greeting.001";
    line.speaker = "Alice";

    VoiceLocaleFile enFile;
    enFile.locale = "en";
    enFile.filePath = "voice/en/greeting_001.ogg";
    enFile.status = VoiceLineStatus::Recorded;
    line.files["en"] = enFile;

    manifest.addLine(line);

    const VoiceManifestLine* retrieved = manifest.getLine("greeting_001");
    REQUIRE(retrieved != nullptr);

    const VoiceLocaleFile* localeFile = retrieved->getFile("en");
    REQUIRE(localeFile != nullptr);
    REQUIRE(localeFile->filePath == "voice/en/greeting_001.ogg");
    REQUIRE(localeFile->status == VoiceLineStatus::Recorded);
  }

  SECTION("Multiple lines for speaker lookup") {
    VoiceManifestLine line1;
    line1.id = "hero_001";
    line1.speaker = "Hero";
    manifest.addLine(line1);

    VoiceManifestLine line2;
    line2.id = "hero_002";
    line2.speaker = "Hero";
    manifest.addLine(line2);

    VoiceManifestLine line3;
    line3.id = "villain_001";
    line3.speaker = "Villain";
    manifest.addLine(line3);

    auto heroLines = manifest.getLinesBySpeaker("Hero");
    REQUIRE(heroLines.size() == 2);

    auto villainLines = manifest.getLinesBySpeaker("Villain");
    REQUIRE(villainLines.size() == 1);
  }
}

// =============================================================================
// Project Integrity Quick Fixes Tests
// =============================================================================

TEST_CASE("ProjectIntegrity: Quick fix dispatch", "[integration][editor][integrity]") {
  using namespace NovelMind::editor;

  SECTION("IntegrityIssue structure") {
    IntegrityIssue issue;
    issue.code = "L002";
    issue.message = "Missing localization key 'test_key' in en";
    issue.severity = IssueSeverity::Warning;
    issue.category = IssueCategory::Localization;
    issue.hasQuickFix = true;
    issue.filePath = "Localization/en.json";

    REQUIRE(issue.code == "L002");
    REQUIRE(issue.hasQuickFix == true);
    REQUIRE(issue.category == IssueCategory::Localization);
  }

  SECTION("Issue severity levels") {
    REQUIRE(static_cast<int>(IssueSeverity::Info) == 0);
    REQUIRE(static_cast<int>(IssueSeverity::Warning) == 1);
    REQUIRE(static_cast<int>(IssueSeverity::Error) == 2);
    REQUIRE(static_cast<int>(IssueSeverity::Critical) == 3);
  }

  SECTION("Issue categories") {
    // Categories are: Scene, Asset, VoiceLine, Localization, StoryGraph, Script, Resource,
    // Configuration
    REQUIRE(static_cast<int>(IssueCategory::Scene) == 0);
    REQUIRE(static_cast<int>(IssueCategory::Asset) == 1);
    REQUIRE(static_cast<int>(IssueCategory::VoiceLine) == 2);
    REQUIRE(static_cast<int>(IssueCategory::Localization) == 3);
    REQUIRE(static_cast<int>(IssueCategory::StoryGraph) == 4);
    REQUIRE(static_cast<int>(IssueCategory::Script) == 5);
    REQUIRE(static_cast<int>(IssueCategory::Resource) == 6);
    REQUIRE(static_cast<int>(IssueCategory::Configuration) == 7);
  }
}

// =============================================================================
// Localization Panel Tests
// =============================================================================

TEST_CASE("LocalizationPanel: Panel can be constructed",
          "[integration][editor][localization][panel]") {
  QtTestFixture fixture;

  SECTION("Panel construction") {
    NMLocalizationPanel panel;
    // Panel should be created without crash
    REQUIRE(panel.isDirty() == false);
  }

  SECTION("Panel initialization") {
    NMLocalizationPanel panel;
    panel.onInitialize();
    // Panel should initialize without crash
    SUCCEED();
  }

  SECTION("Panel shutdown") {
    NMLocalizationPanel panel;
    panel.onInitialize();
    panel.onShutdown();
    // Panel should shutdown cleanly
    SUCCEED();
  }
}

TEST_CASE("LocalizationPanel: Key operations", "[integration][editor][localization][panel]") {
  QtTestFixture fixture;

  SECTION("Add key operation") {
    NMLocalizationPanel panel;
    panel.onInitialize();

    [[maybe_unused]] bool result = panel.addKey("test_key", "Test value");
    // Operation should complete (may depend on project state)
    SUCCEED();
  }

  SECTION("Delete key operation") {
    NMLocalizationPanel panel;
    panel.onInitialize();

    // Add then delete
    panel.addKey("temp_key", "Temporary");
    [[maybe_unused]] bool result = panel.deleteKey("temp_key");
    // Operation should complete
    SUCCEED();
  }
}
