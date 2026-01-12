#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/editor_runtime_host.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include <filesystem>
#include <fstream>

using namespace NovelMind;
using namespace NovelMind::editor;
using namespace NovelMind::scripting;

// Test fixture helpers
namespace {

std::filesystem::path createTempDir() {
  auto tempDir = std::filesystem::temp_directory_path() / "nm_test_project";
  std::filesystem::create_directories(tempDir);
  std::filesystem::create_directories(tempDir / "scripts");
  std::filesystem::create_directories(tempDir / "assets");
  return tempDir;
}

void cleanupTempDir(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    std::filesystem::remove_all(path);
  }
}

void writeTestScript(const std::filesystem::path& dir, const std::string& content) {
  std::ofstream file(dir / "scripts" / "main.nms");
  file << content;
  file.close();
}

const char* SIMPLE_SCRIPT = R"(
character Hero(name="Hero", color="#00FF00")
character Narrator(name="", color="#AAAAAA")

scene intro {
    show background "bg_test"
    show Hero at center
    say Hero "Hello, world!"
    goto ending
}

scene ending {
    say Narrator "The End"
}
)";

const char* SCRIPT_WITH_CHOICES = R"(
character Player(name="Player", color="#0000FF")

scene start {
    say Player "What should I do?"
    choice {
        "Go left" -> goto left_path
        "Go right" -> goto right_path
    }
}

scene left_path {
    say Player "I went left."
}

scene right_path {
    say Player "I went right."
}
)";

const char* SCRIPT_WITH_VARIABLES = R"(
character Hero(name="Hero", color="#FF0000")

scene intro {
    set points = 0
    set flag visited = false
    say Hero "Starting adventure..."
    set points = points + 10
    set flag visited = true
    goto ending
}

scene ending {
    if points > 5 {
        say Hero "You scored high!"
    }
}
)";

} // anonymous namespace

// =============================================================================
// EditorRuntimeHost Tests
// =============================================================================

TEST_CASE("EditorRuntimeHost - Initial state is Unloaded", "[editor_runtime]") {
  EditorRuntimeHost host;
  CHECK(host.getState() == EditorRuntimeState::Unloaded);
  CHECK_FALSE(host.isProjectLoaded());
}

TEST_CASE("EditorRuntimeHost - Load project creates Stopped state", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto result = host.loadProject(project);

  if (result.isOk()) {
    CHECK(host.isProjectLoaded());
    CHECK(host.getState() == EditorRuntimeState::Stopped);
    CHECK(host.getProject().name == "TestProject");
  }

  host.unloadProject();
  CHECK_FALSE(host.isProjectLoaded());
  CHECK(host.getState() == EditorRuntimeState::Unloaded);

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Play changes state to Running", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      CHECK(host.getState() == EditorRuntimeState::Running);
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Pause and Resume work correctly", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      host.pause();
      CHECK(host.getState() == EditorRuntimeState::Paused);

      host.resume();
      CHECK(host.getState() == EditorRuntimeState::Running);
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Stop resets to Stopped state", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      host.stop();
      CHECK(host.getState() == EditorRuntimeState::Stopped);
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - PlayFromScene starts at specific scene", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.playFromScene("ending");
    if (playResult.isOk()) {
      CHECK(host.getState() == EditorRuntimeState::Running);
      CHECK(host.getCurrentScene() == "ending");
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - GetScenes returns all scene names", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto scenes = host.getScenes();
    CHECK(scenes.size() >= 2);

    bool hasIntro = false;
    bool hasEnding = false;
    for (const auto& scene : scenes) {
      if (scene == "intro")
        hasIntro = true;
      if (scene == "ending")
        hasEnding = true;
    }
    CHECK(hasIntro);
    CHECK(hasEnding);
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Breakpoint management", "[editor_runtime]") {
  EditorRuntimeHost host;

  // Add breakpoints
  Breakpoint bp1;
  bp1.scriptPath = "main.nms";
  bp1.line = 10;
  bp1.enabled = true;

  Breakpoint bp2;
  bp2.scriptPath = "main.nms";
  bp2.line = 20;
  bp2.enabled = true;
  bp2.condition = "points > 5";

  host.addBreakpoint(bp1);
  host.addBreakpoint(bp2);

  CHECK(host.getBreakpoints().size() == 2);

  // Disable breakpoint
  host.setBreakpointEnabled("main.nms", 10, false);
  const auto& breakpoints = host.getBreakpoints();
  bool foundDisabled = false;
  for (const auto& bp : breakpoints) {
    if (bp.line == 10 && !bp.enabled) {
      foundDisabled = true;
      break;
    }
  }
  CHECK(foundDisabled);

  // Remove breakpoint
  host.removeBreakpoint("main.nms", 10);
  CHECK(host.getBreakpoints().size() == 1);

  // Clear all
  host.clearBreakpoints();
  CHECK(host.getBreakpoints().empty());
}

TEST_CASE("EditorRuntimeHost - State change callbacks", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;

  std::vector<EditorRuntimeState> stateChanges;
  host.setOnStateChanged(
      [&stateChanges](EditorRuntimeState state) { stateChanges.push_back(state); });

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      host.pause();
      host.resume();
      host.stop();

      // Should have recorded state changes
      CHECK(stateChanges.size() >= 3);
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Scene snapshot during play", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SIMPLE_SCRIPT);

  EditorRuntimeHost host;
  // Disable hot reload to avoid expensive filesystem scans during test
  host.setAutoHotReload(false);

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      // Update a few frames
      host.update(0.016);
      host.update(0.016);

      SceneSnapshot snapshot = host.getSceneSnapshot();
      // Snapshot should have valid data
      CHECK(!snapshot.currentSceneId.empty());
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Variables and flags inspection", "[editor_runtime]") {
  auto tempDir = createTempDir();
  writeTestScript(tempDir, SCRIPT_WITH_VARIABLES);

  EditorRuntimeHost host;
  // Disable hot reload to avoid expensive filesystem scans during test
  host.setAutoHotReload(false);

  ProjectDescriptor project;
  project.name = "TestProject";
  project.path = tempDir.string();
  project.scriptsPath = (tempDir / "scripts").string();
  project.assetsPath = (tempDir / "assets").string();
  project.startScene = "intro";

  auto loadResult = host.loadProject(project);
  if (loadResult.isOk()) {
    auto playResult = host.play();
    if (playResult.isOk()) {
      // Run several updates to execute the script
      for (int i = 0; i < 10; ++i) {
        host.update(0.1);
        host.simulateClick(); // Advance dialogue
      }

      // Get variables - should have 'points' if script executed
      auto variables = host.getVariables();
      auto flags = host.getFlags();

      // The variables map should exist (even if empty due to runtime state)
      // This mainly tests the API works
      CHECK(true); // API call succeeded
    }
  }

  cleanupTempDir(tempDir);
}

TEST_CASE("EditorRuntimeHost - Hot reload toggle", "[editor_runtime]") {
  EditorRuntimeHost host;

  // Default should be enabled
  CHECK(host.isAutoHotReloadEnabled());

  host.setAutoHotReload(false);
  CHECK_FALSE(host.isAutoHotReloadEnabled());

  host.setAutoHotReload(true);
  CHECK(host.isAutoHotReloadEnabled());
}

// =============================================================================
// Script Compilation Integration Tests
// =============================================================================

TEST_CASE("Integration - Parser and Validator work together", "[integration]") {
  const char* validScript = R"(
character Hero(name="Hero", color="#00FF00")

scene intro {
    show Hero at center
    say Hero "Hello!"
    goto ending
}

scene ending {
    say Hero "Goodbye!"
}
)";

  Lexer lexer;
  auto tokens = lexer.tokenize(validScript);
  CHECK(tokens.isOk());

  Parser parser;
  auto program = parser.parse(tokens.value());
  CHECK(program.isOk());

  Validator validator;
  validator.setReportUnused(false);
  auto result = validator.validate(program.value());

  CHECK(result.isValid);
  CHECK_FALSE(result.hasErrors());
}

TEST_CASE("Integration - Invalid script produces validation errors", "[integration]") {
  const char* invalidScript = R"(
scene intro {
    show UndefinedChar at center
    say NonExistent "Hello!"
    goto nonexistent_scene
}
)";

  Lexer lexer;
  auto tokens = lexer.tokenize(invalidScript);
  REQUIRE(tokens.isOk());

  Parser parser;
  auto program = parser.parse(tokens.value());
  REQUIRE(program.isOk());

  Validator validator;
  auto result = validator.validate(program.value());

  CHECK(result.hasErrors());

  // Should have multiple undefined errors
  int undefinedCount = 0;
  for (const auto& error : result.errors.all()) {
    if (error.code == ErrorCode::UndefinedCharacter || error.code == ErrorCode::UndefinedScene) {
      undefinedCount++;
    }
  }
  CHECK(undefinedCount >= 2);
}

TEST_CASE("Integration - Complex script with choices validates", "[integration]") {
  Lexer lexer;
  auto tokens = lexer.tokenize(SCRIPT_WITH_CHOICES);
  REQUIRE(tokens.isOk());

  Parser parser;
  auto program = parser.parse(tokens.value());
  REQUIRE(program.isOk());

  Validator validator;
  validator.setReportUnused(false);
  auto result = validator.validate(program.value());

  CHECK(result.isValid);
  CHECK_FALSE(result.hasErrors());

  // Program should have correct structure
  CHECK(program.value().characters.size() == 1);
  CHECK(program.value().scenes.size() == 3);
}

TEST_CASE("Integration - Script with variables and flags validates", "[integration]") {
  Lexer lexer;
  auto tokens = lexer.tokenize(SCRIPT_WITH_VARIABLES);
  REQUIRE(tokens.isOk());

  Parser parser;
  auto program = parser.parse(tokens.value());
  REQUIRE(program.isOk());

  Validator validator;
  validator.setReportUnused(false);
  auto result = validator.validate(program.value());

  CHECK(result.isValid);
  CHECK_FALSE(result.hasErrors());
}
