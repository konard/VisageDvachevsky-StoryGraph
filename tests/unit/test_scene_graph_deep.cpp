#include <catch2/catch_test_macros.hpp>

#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_object_handle.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/platform/window.hpp"
#include <memory>

using namespace NovelMind;
using namespace NovelMind::scene;

// Mock window for renderer initialization
class MockWindow : public platform::IWindow {
public:
  Result<void> create(const std::string &, i32, i32, u32) override { return Result<void>(); }
  void destroy() override {}
  void pollEvents() override {}
  bool shouldClose() const override { return false; }
  void swapBuffers() override {}
  void *getNativeHandle() override { return nullptr; }
  i32 getWidth() const override { return 800; }
  i32 getHeight() const override { return 600; }
};

// Mock renderer for testing
class MockRenderer : public renderer::IRenderer {
public:
  Result<void> initialize(platform::IWindow &) override { return Result<void>(); }
  void shutdown() override {}
  void beginFrame() override {}
  void endFrame() override {}
  void clear(const renderer::Color &color) override {
    (void)color;
  }
  void setBlendMode(renderer::BlendMode) override {}
  void drawSprite(const renderer::Texture &, const renderer::Transform2D &,
                  const renderer::Color &) override {}
  void drawSprite(const renderer::Texture &, const renderer::Rect &,
                  const renderer::Transform2D &, const renderer::Color &) override {}
  void drawRect(const renderer::Rect &, const renderer::Color &) override {}
  void fillRect(const renderer::Rect &, const renderer::Color &) override {}
  void drawText(const renderer::Font &, const std::string &, f32, f32,
                const renderer::Color &) override {}
  void setFade(f32, const renderer::Color &) override {}
  i32 getWidth() const override { return 800; }
  i32 getHeight() const override { return 600; }
};

// ============================================================================
// Scene Graph Basic Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("SceneGraph - Initialization", "[scene_graph][basic]") {
  SceneGraph graph;

  SECTION("Scene ID can be set and retrieved") {
    graph.setSceneId("test_scene");
    CHECK(graph.getSceneId() == "test_scene");
  }

  SECTION("Clear scene") {
    graph.setSceneId("test");
    graph.clear();
    // Verify clear doesn't crash
  }
}

TEST_CASE("SceneGraph - Layer access", "[scene_graph][layer]") {
  SceneGraph graph;

  SECTION("Access background layer") {
    Layer &bgLayer = graph.getBackgroundLayer();
    CHECK(bgLayer.getType() == LayerType::Background);
  }

  SECTION("Access character layer") {
    Layer &charLayer = graph.getCharacterLayer();
    CHECK(charLayer.getType() == LayerType::Characters);
  }

  SECTION("Access UI layer") {
    Layer &uiLayer = graph.getUILayer();
    CHECK(uiLayer.getType() == LayerType::UI);
  }

  SECTION("Access effect layer") {
    Layer &effectLayer = graph.getEffectLayer();
    CHECK(effectLayer.getType() == LayerType::Effects);
  }

  SECTION("Access layer by type") {
    Layer &layer1 = graph.getLayer(LayerType::Background);
    Layer &layer2 = graph.getLayer(LayerType::Characters);
    Layer &layer3 = graph.getLayer(LayerType::UI);
    Layer &layer4 = graph.getLayer(LayerType::Effects);

    CHECK(layer1.getType() == LayerType::Background);
    CHECK(layer2.getType() == LayerType::Characters);
    CHECK(layer3.getType() == LayerType::UI);
    CHECK(layer4.getType() == LayerType::Effects);
  }
}

TEST_CASE("SceneGraph - Object management", "[scene_graph][objects]") {
  SceneGraph graph;

  SECTION("Add background object to layer") {
    auto bg = std::make_unique<BackgroundObject>("bg1");
    bg->setTextureId("test_bg.png");

    graph.addToLayer(LayerType::Background, std::move(bg));

    auto *found = graph.findObject("bg1");
    CHECK(found != nullptr);
    if (found) {
      auto *bgObj = dynamic_cast<BackgroundObject *>(found);
      CHECK(bgObj != nullptr);
      if (bgObj) {
        CHECK(bgObj->getTextureId() == "test_bg.png");
      }
    }
  }

  SECTION("Remove object from layer") {
    auto bg = std::make_unique<BackgroundObject>("bg1");
    graph.addToLayer(LayerType::Background, std::move(bg));

    auto removed = graph.removeFromLayer(LayerType::Background, "bg1");
    CHECK(removed != nullptr);

    auto *found = graph.findObject("bg1");
    CHECK(found == nullptr);
  }

  SECTION("Find non-existent object returns nullptr") {
    auto *found = graph.findObject("nonexistent");
    CHECK(found == nullptr);
  }
}

TEST_CASE("SceneGraph - Convenience methods", "[scene_graph][convenience]") {
  SceneGraph graph;

  SECTION("Show background") {
    graph.showBackground("test_background.png");
    // Verify no crash
  }

  SECTION("Show character") {
    auto *character = graph.showCharacter("hero", "hero_sprite",
                                          CharacterObject::Position::Center);
    // May return null if not fully initialized
    (void)character;
  }

  SECTION("Show dialogue") {
    auto *dialogue = graph.showDialogue("Hero", "Hello, world!");
    // May return null if not fully initialized
    (void)dialogue;
  }

  SECTION("Hide dialogue") {
    graph.hideDialogue();
    // Verify no crash
  }
}

TEST_CASE("SceneGraph - Update and render", "[scene_graph][update]") {
  SceneGraph graph;
  MockRenderer renderer;

  SECTION("Update scene") {
    graph.update(0.016); // 60 FPS
    graph.update(0.033); // 30 FPS
    // Verify no crash
  }

  SECTION("Render scene") {
    graph.render(renderer);
    // Verify no crash
  }

  SECTION("Update then render") {
    graph.update(0.016);
    graph.render(renderer);
    // Verify no crash
  }
}

TEST_CASE("SceneGraph - Serialization", "[scene_graph][serialization]") {
  SceneGraph graph;

  SECTION("Save empty scene state") {
    SceneState state = graph.saveState();
    // Should return empty state
  }

  SECTION("Load scene state") {
    SceneState state;
    state.sceneId = "test_scene";

    graph.loadState(state);
    CHECK(graph.getSceneId() == "test_scene");
  }

  SECTION("Save and load round trip") {
    graph.setSceneId("original_scene");
    SceneState saved = graph.saveState();

    SceneGraph newGraph;
    newGraph.loadState(saved);
    CHECK(newGraph.getSceneId() == "original_scene");
  }
}

// ============================================================================
// Scene Object Base Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("SceneObjectBase - Transform operations", "[scene_graph][transform]") {
  BackgroundObject obj("test_obj");

  SECTION("Position") {
    obj.setPosition(100.0f, 200.0f);
    CHECK(obj.getX() == 100.0f);
    CHECK(obj.getY() == 200.0f);
  }

  SECTION("Scale") {
    obj.setScale(2.0f, 3.0f);
    CHECK(obj.getScaleX() == 2.0f);
    CHECK(obj.getScaleY() == 3.0f);
  }

  SECTION("Uniform scale") {
    obj.setUniformScale(1.5f);
    CHECK(obj.getScaleX() == 1.5f);
    CHECK(obj.getScaleY() == 1.5f);
  }

  SECTION("Rotation") {
    obj.setRotation(90.0f);
    CHECK(obj.getRotation() == 90.0f);
  }

  SECTION("Anchor") {
    obj.setAnchor(0.25f, 0.75f);
    CHECK(obj.getAnchorX() == 0.25f);
    CHECK(obj.getAnchorY() == 0.75f);
  }

  SECTION("Visibility") {
    obj.setVisible(false);
    CHECK(obj.isVisible() == false);

    obj.setVisible(true);
    CHECK(obj.isVisible() == true);
  }

  SECTION("Alpha") {
    obj.setAlpha(0.5f);
    CHECK(obj.getAlpha() == 0.5f);
  }

  SECTION("Z-order") {
    obj.setZOrder(10);
    CHECK(obj.getZOrder() == 10);
  }
}

TEST_CASE("SceneObjectBase - Property system", "[scene_graph][property]") {
  BackgroundObject obj("test_obj");

  SECTION("Set and get property") {
    obj.setProperty("name", "TestObject");
    auto value = obj.getProperty("name");

    REQUIRE(value.has_value());
    CHECK(value.value() == "TestObject");
  }

  SECTION("Get non-existent property returns nullopt") {
    auto value = obj.getProperty("nonexistent");
    CHECK_FALSE(value.has_value());
  }

  SECTION("Multiple properties") {
    obj.setProperty("prop1", "value1");
    obj.setProperty("prop2", "value2");
    obj.setProperty("prop3", "value3");

    const auto &props = obj.getProperties();
    CHECK(props.size() == 3);
    CHECK(props.at("prop1") == "value1");
    CHECK(props.at("prop2") == "value2");
    CHECK(props.at("prop3") == "value3");
  }
}

TEST_CASE("SceneObjectBase - Tag system", "[scene_graph][tag]") {
  BackgroundObject obj("test_obj");

  SECTION("Add and check tags") {
    obj.addTag("important");
    obj.addTag("npc");

    CHECK(obj.hasTag("important") == true);
    CHECK(obj.hasTag("npc") == true);
    CHECK(obj.hasTag("enemy") == false);
  }

  SECTION("Remove tag") {
    obj.addTag("temporary");
    CHECK(obj.hasTag("temporary") == true);

    obj.removeTag("temporary");
    CHECK(obj.hasTag("temporary") == false);
  }
}

TEST_CASE("SceneObjectBase - Type information", "[scene_graph][type]") {
  SECTION("Background object type") {
    BackgroundObject obj("bg1");
    CHECK(obj.getType() == SceneObjectType::Background);
    CHECK(obj.getId() == "bg1");
  }

  SECTION("Character object type") {
    CharacterObject obj("char1", "hero");
    CHECK(obj.getType() == SceneObjectType::Character);
    CHECK(obj.getId() == "char1");
  }

  SECTION("Dialogue UI object type") {
    DialogueUIObject obj("dialogue1");
    CHECK(obj.getType() == SceneObjectType::DialogueUI);
    CHECK(obj.getId() == "dialogue1");
  }

  SECTION("Choice UI object type") {
    ChoiceUIObject obj("choice1");
    CHECK(obj.getType() == SceneObjectType::ChoiceUI);
    CHECK(obj.getId() == "choice1");
  }
}

// ============================================================================
// Character Object Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("CharacterObject - Properties", "[scene_graph][character]") {
  CharacterObject character("hero", "hero_id");

  SECTION("Character ID") {
    CHECK(character.getCharacterId() == "hero_id");

    character.setCharacterId("villain_id");
    CHECK(character.getCharacterId() == "villain_id");
  }

  SECTION("Display name") {
    character.setDisplayName("The Hero");
    CHECK(character.getDisplayName() == "The Hero");
  }

  SECTION("Expression") {
    character.setExpression("happy");
    CHECK(character.getExpression() == "happy");
  }

  SECTION("Pose") {
    character.setPose("standing");
    CHECK(character.getPose() == "standing");
  }

  SECTION("Slot position") {
    character.setSlotPosition(CharacterObject::Position::Left);
    CHECK(character.getSlotPosition() == CharacterObject::Position::Left);
  }
}

// ============================================================================
// Background Object Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("BackgroundObject - Properties", "[scene_graph][background]") {
  BackgroundObject bg("bg1");

  SECTION("Texture ID") {
    bg.setTextureId("backgrounds/room.png");
    CHECK(bg.getTextureId() == "backgrounds/room.png");
  }

  SECTION("Tint color") {
    NovelMind::renderer::Color tint{255, 128, 64, 200};
    bg.setTint(tint);

    const auto &storedTint = bg.getTint();
    CHECK(storedTint.r == 255);
    CHECK(storedTint.g == 128);
    CHECK(storedTint.b == 64);
    CHECK(storedTint.a == 200);
  }
}

// ============================================================================
// Dialogue UI Object Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("DialogueUIObject - Text and speaker", "[scene_graph][dialogue]") {
  DialogueUIObject dialogue("dialogue1");

  SECTION("Speaker") {
    dialogue.setSpeaker("Hero");
    CHECK(dialogue.getSpeaker() == "Hero");
  }

  SECTION("Text") {
    dialogue.setText("Hello, world!");
    CHECK(dialogue.getText() == "Hello, world!");
  }

  SECTION("Typewriter effect") {
    dialogue.setTypewriterEnabled(true);
    CHECK(dialogue.isTypewriterEnabled() == true);

    dialogue.setTypewriterSpeed(50.0f);
    CHECK(dialogue.getTypewriterSpeed() == 50.0f);
  }
}

// ============================================================================
// Choice UI Object Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("ChoiceUIObject - Choice management", "[scene_graph][choice]") {
  ChoiceUIObject choice("choice1");

  SECTION("Set and get choices") {
    std::vector<ChoiceUIObject::ChoiceOption> choices = {
        {"opt1", "Option 1", true, true, ""},
        {"opt2", "Option 2", true, true, ""},
        {"opt3", "Option 3", true, true, ""}};

    choice.setChoices(choices);
    CHECK(choice.getChoices().size() == 3);
  }

  SECTION("Clear choices") {
    std::vector<ChoiceUIObject::ChoiceOption> choices = {
        {"opt1", "Option 1", true, true, ""}};
    choice.setChoices(choices);
    CHECK(choice.getChoices().size() == 1);

    choice.clearChoices();
    CHECK(choice.getChoices().empty());
  }

  SECTION("Selection") {
    std::vector<ChoiceUIObject::ChoiceOption> choices = {
        {"opt1", "Option 1", true, true, ""},
        {"opt2", "Option 2", true, true, ""}};
    choice.setChoices(choices);

    choice.setSelectedIndex(1);
    CHECK(choice.getSelectedIndex() == 1);
  }
}

// ============================================================================
// Scene Object Handle Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("SceneObjectHandle - Validity tracking", "[scene_graph][handle]") {
  SceneGraph graph;

  SECTION("Handle to existing object is valid") {
    auto bg = std::make_unique<BackgroundObject>("bg1");
    graph.addToLayer(LayerType::Background, std::move(bg));

    SceneObjectHandle handle(&graph, "bg1");
    CHECK(handle.isValid() == true);
    CHECK(handle.get() != nullptr);
  }

  SECTION("Handle to non-existent object is invalid") {
    SceneObjectHandle handle(&graph, "nonexistent");
    CHECK(handle.isValid() == false);
    CHECK(handle.get() == nullptr);
  }

  SECTION("Default handle is invalid") {
    SceneObjectHandle handle;
    CHECK(handle.isValid() == false);
  }

  SECTION("Reset makes handle invalid") {
    auto bg = std::make_unique<BackgroundObject>("bg1");
    graph.addToLayer(LayerType::Background, std::move(bg));

    SceneObjectHandle handle(&graph, "bg1");
    CHECK(handle.isValid() == true);

    handle.reset();
    CHECK(handle.isValid() == false);
  }
}
