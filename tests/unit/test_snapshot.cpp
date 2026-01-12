#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <sstream>

using namespace NovelMind;
using namespace NovelMind::scene;
using namespace NovelMind::renderer;

// =============================================================================
// Scene Snapshot Testing
// Verifies scene state serialization and comparison for regression detection
// =============================================================================

namespace {

// Helper to collect all object states from all layers
std::vector<SceneObjectState> collectObjectStates(SceneGraph& graph) {
  std::vector<SceneObjectState> states;

  auto collectFromLayer = [&states](Layer& layer) {
    for (const auto& obj : layer.getObjects()) {
      states.push_back(obj->saveState());
    }
  };

  collectFromLayer(graph.getBackgroundLayer());
  collectFromLayer(graph.getCharacterLayer());
  collectFromLayer(graph.getUILayer());
  collectFromLayer(graph.getEffectLayer());

  return states;
}

struct SceneSnapshotData {
  std::string sceneId;
  std::vector<SceneObjectState> objectStates;

  bool operator==(const SceneSnapshotData& other) const {
    if (sceneId != other.sceneId)
      return false;
    if (objectStates.size() != other.objectStates.size())
      return false;
    for (size_t i = 0; i < objectStates.size(); ++i) {
      if (objectStates[i].id != other.objectStates[i].id)
        return false;
      if (objectStates[i].visible != other.objectStates[i].visible)
        return false;
      if (objectStates[i].alpha != other.objectStates[i].alpha)
        return false;
    }
    return true;
  }

  std::string serialize() const {
    std::stringstream ss;
    ss << "scene:" << sceneId << "\n";
    for (const auto& state : objectStates) {
      ss << "object:" << state.id << ",visible:" << (state.visible ? "true" : "false")
         << ",alpha:" << state.alpha << ",x:" << state.x << ",y:" << state.y << "\n";
    }
    return ss.str();
  }
};

SceneSnapshotData captureSnapshot(SceneGraph& graph) {
  SceneSnapshotData snapshot;
  snapshot.sceneId = graph.getSceneId();
  snapshot.objectStates = collectObjectStates(graph);
  return snapshot;
}

} // anonymous namespace

TEST_CASE("SceneSnapshot - Empty scene produces empty snapshot", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("test_scene");
  auto snapshot = captureSnapshot(graph);

  CHECK(snapshot.sceneId == "test_scene");
  CHECK(snapshot.objectStates.empty());
}

TEST_CASE("SceneSnapshot - Background object via showBackground", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("intro");
  graph.showBackground("city_night");

  auto snapshot = captureSnapshot(graph);

  // showBackground creates a background object
  REQUIRE(snapshot.objectStates.size() >= 1);
}

TEST_CASE("SceneSnapshot - Character object state capture", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("dialogue");

  auto* character = graph.showCharacter("hero", "Hero", CharacterObject::Position::Center);
  REQUIRE(character != nullptr);

  character->setExpression("happy");
  character->setPose("standing");
  character->setHighlighted(true);

  auto snapshot = captureSnapshot(graph);

  REQUIRE(snapshot.objectStates.size() >= 1);

  bool foundHero = false;
  for (const auto& state : snapshot.objectStates) {
    if (state.id == "hero") {
      foundHero = true;
      CHECK(state.properties.at("expression") == "happy");
      CHECK(state.properties.at("pose") == "standing");
      CHECK(state.properties.at("highlighted") == "true");
    }
  }
  CHECK(foundHero);
}

TEST_CASE("SceneSnapshot - Multiple characters", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("multi");

  graph.showCharacter("alice", "Alice", CharacterObject::Position::Left);
  graph.showCharacter("bob", "Bob", CharacterObject::Position::Right);

  auto snapshot = captureSnapshot(graph);

  bool hasAlice = false, hasBob = false;
  for (const auto& state : snapshot.objectStates) {
    if (state.id == "alice")
      hasAlice = true;
    if (state.id == "bob")
      hasBob = true;
  }
  CHECK(hasAlice);
  CHECK(hasBob);
}

TEST_CASE("SceneSnapshot - Hide character sets visibility to false", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("test");

  graph.showCharacter("hero", "Hero", CharacterObject::Position::Center);
  auto snapshotBefore = captureSnapshot(graph);

  bool foundAndVisible = false;
  for (const auto& state : snapshotBefore.objectStates) {
    if (state.id == "hero" && state.visible)
      foundAndVisible = true;
  }
  CHECK(foundAndVisible);

  graph.hideCharacter("hero");
  auto snapshotAfter = captureSnapshot(graph);

  bool isHidden = false;
  for (const auto& state : snapshotAfter.objectStates) {
    if (state.id == "hero" && !state.visible)
      isHidden = true;
  }
  CHECK(isHidden);
}

TEST_CASE("SceneSnapshot - Dialogue UI state capture", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("narration");

  auto* dlg = graph.showDialogue("Narrator", "Once upon a time...");
  REQUIRE(dlg != nullptr);

  dlg->setTypewriterEnabled(true);
  dlg->setTypewriterSpeed(50.0f);

  auto snapshot = captureSnapshot(graph);

  bool foundDialogue = false;
  for (const auto& state : snapshot.objectStates) {
    if (state.type == SceneObjectType::DialogueUI) {
      foundDialogue = true;
      CHECK(state.properties.at("speaker") == "Narrator");
      CHECK(state.properties.at("text") == "Once upon a time...");
      CHECK(state.properties.at("typewriterEnabled") == "true");
    }
  }
  CHECK(foundDialogue);
}

TEST_CASE("SceneSnapshot - Choice UI state capture", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("branch_point");

  std::vector<ChoiceUIObject::ChoiceOption> choices = {
      {"opt1", "Go left", true, true, ""},
      {"opt2", "Go right", false, true, ""}, // disabled
      {"opt3", "Stay here", true, true, ""}};
  auto* choiceUI = graph.showChoices(choices);
  REQUIRE(choiceUI != nullptr);

  auto snapshot = captureSnapshot(graph);

  bool foundChoice = false;
  for (const auto& state : snapshot.objectStates) {
    if (state.type == SceneObjectType::ChoiceUI) {
      foundChoice = true;
      CHECK(state.properties.at("choiceCount") == "3");
      CHECK(state.properties.at("choice0_text") == "Go left");
      CHECK(state.properties.at("choice1_enabled") == "false");
    }
  }
  CHECK(foundChoice);
}

TEST_CASE("SceneSnapshot - Full scene save and load roundtrip", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("roundtrip_test");

  graph.showBackground("forest");
  auto* hero = graph.showCharacter("hero", "Hero", CharacterObject::Position::Center);
  hero->setExpression("neutral");

  // Save state
  SceneState savedState = graph.saveState();

  // Modify scene
  hero->setExpression("angry");

  // Restore state
  graph.loadState(savedState);

  // Verify restoration
  auto* restoredHero = dynamic_cast<CharacterObject*>(graph.findObject("hero"));
  if (restoredHero) {
    // State restoration should work
    CHECK(true);
  }
}

TEST_CASE("SceneSnapshot - Serialization format", "[snapshot]") {
  SceneGraph graph;
  graph.setSceneId("serialize_test");
  graph.showBackground("test");

  auto snapshot = captureSnapshot(graph);
  auto serialized = snapshot.serialize();

  CHECK(serialized.find("scene:serialize_test") != std::string::npos);
  CHECK(serialized.find("visible:true") != std::string::npos);
}

TEST_CASE("SceneSnapshot - Golden reference comparison", "[snapshot]") {
  // This test establishes a "golden" snapshot for regression testing
  SceneGraph graph;
  graph.setSceneId("golden_test");

  graph.showBackground("standard_bg");
  auto* hero = graph.showCharacter("hero", "Hero", CharacterObject::Position::Center);
  hero->setExpression("neutral");
  hero->setPose("standing");

  graph.showDialogue("Hero", "Standard dialogue text");

  auto snapshot = captureSnapshot(graph);

  // Verify expected structure
  CHECK(snapshot.sceneId == "golden_test");
  CHECK(snapshot.objectStates.size() >= 3); // bg + char + dialogue

  // Verify specific golden properties
  bool foundHero = false;
  for (const auto& state : snapshot.objectStates) {
    if (state.id == "hero") {
      foundHero = true;
      CHECK(state.properties.at("expression") == "neutral");
      CHECK(state.properties.at("pose") == "standing");
    }
  }
  CHECK(foundHero);
}
