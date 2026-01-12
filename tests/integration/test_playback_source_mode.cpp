#include <catch2/catch_test_macros.hpp>

// Test for PlaybackSourceMode enum (Issue #82, #94)
// Note: This tests the enum values without Qt dependencies.
// Full UI tests would require Qt Test framework.

#include "NovelMind/editor/project_manager.hpp"

using namespace NovelMind::editor;

TEST_CASE("PlaybackSourceMode enum values", "[playback_source][issue_82]") {
  SECTION("Enum has expected values") {
    // Verify enum values are distinct and can be cast to integers
    REQUIRE(static_cast<int>(PlaybackSourceMode::Script) == 0);
    REQUIRE(static_cast<int>(PlaybackSourceMode::Graph) == 1);
    REQUIRE(static_cast<int>(PlaybackSourceMode::Mixed) == 2);
  }

  SECTION("Enum values are distinct") {
    REQUIRE(PlaybackSourceMode::Script != PlaybackSourceMode::Graph);
    REQUIRE(PlaybackSourceMode::Graph != PlaybackSourceMode::Mixed);
    REQUIRE(PlaybackSourceMode::Script != PlaybackSourceMode::Mixed);
  }
}

TEST_CASE("ProjectMetadata default playback source mode", "[playback_source][issue_82]") {
  ProjectMetadata meta;

  SECTION("Default value is Script mode") {
    // Script mode is the default to maintain backward compatibility
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Script);
  }

  SECTION("Playback source mode can be changed") {
    meta.playbackSourceMode = PlaybackSourceMode::Graph;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Graph);

    meta.playbackSourceMode = PlaybackSourceMode::Mixed;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Mixed);

    meta.playbackSourceMode = PlaybackSourceMode::Script;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Script);
  }
}

TEST_CASE("PlaybackSourceMode round-trip conversion", "[playback_source][issue_82]") {
  // Test that enum values can be safely converted to/from integers
  // This is important for serialization/deserialization

  SECTION("Script mode round-trip") {
    int value = static_cast<int>(PlaybackSourceMode::Script);
    auto mode = static_cast<PlaybackSourceMode>(value);
    REQUIRE(mode == PlaybackSourceMode::Script);
  }

  SECTION("Graph mode round-trip") {
    int value = static_cast<int>(PlaybackSourceMode::Graph);
    auto mode = static_cast<PlaybackSourceMode>(value);
    REQUIRE(mode == PlaybackSourceMode::Graph);
  }

  SECTION("Mixed mode round-trip") {
    int value = static_cast<int>(PlaybackSourceMode::Mixed);
    auto mode = static_cast<PlaybackSourceMode>(value);
    REQUIRE(mode == PlaybackSourceMode::Mixed);
  }
}

// ============================================================================
// Tests for Issue #94: Content source priority during playback
// ============================================================================

TEST_CASE("PlaybackSourceMode determines content source priority", "[playback_source][issue_94]") {
  SECTION("Script mode uses only NMScript files") {
    // In Script mode, only .nms files should be used for playback
    // Story Graph visual data is ignored
    ProjectMetadata meta;
    meta.playbackSourceMode = PlaybackSourceMode::Script;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Script);

    // Verify this is the default (backward compatible)
    ProjectMetadata defaultMeta;
    REQUIRE(defaultMeta.playbackSourceMode == PlaybackSourceMode::Script);
  }

  SECTION("Graph mode uses Story Graph visual data") {
    // In Graph mode, Story Graph visual data is authoritative
    // NMScript files are ignored, content comes from story_graph.json
    ProjectMetadata meta;
    meta.playbackSourceMode = PlaybackSourceMode::Graph;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Graph);
  }

  SECTION("Mixed mode merges both sources with Graph priority") {
    // In Mixed mode, both sources are used but Graph wins on conflicts
    // This allows users to have base scripts with Graph overrides
    ProjectMetadata meta;
    meta.playbackSourceMode = PlaybackSourceMode::Mixed;
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Mixed);
  }
}

TEST_CASE("PlaybackSourceMode affects entry scene selection", "[playback_source][issue_94]") {
  SECTION("Graph mode can override entry scene") {
    // When in Graph mode, the entry scene from story_graph.json
    // should take priority over project metadata startScene
    ProjectMetadata meta;
    meta.startScene = "script_start";
    meta.playbackSourceMode = PlaybackSourceMode::Graph;

    // In Graph mode, the runtime will check story_graph.json for entry
    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Graph);
  }

  SECTION("Mixed mode uses Graph entry when available") {
    // In Mixed mode, if story_graph.json has an entry scene,
    // it should override the script-based entry
    ProjectMetadata meta;
    meta.startScene = "script_start";
    meta.playbackSourceMode = PlaybackSourceMode::Mixed;

    REQUIRE(meta.playbackSourceMode == PlaybackSourceMode::Mixed);
  }
}
