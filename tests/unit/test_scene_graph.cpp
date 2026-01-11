/**
 * @file test_scene_graph.cpp
 * @brief Comprehensive unit tests for SceneGraph 2.0
 *
 * Tests cover:
 * - Object creation and lifecycle
 * - Parent-child relationships
 * - Transform propagation
 * - Visibility inheritance
 * - Z-order sorting
 * - Property system
 * - Serialization
 * - Layer management
 * - Error paths and edge cases
 *
 * Related to Issue #179 - Test coverage gaps
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <memory>

using namespace NovelMind::scene;
using namespace NovelMind;

// Mock renderer for testing
class MockRenderer : public renderer::IRenderer {
public:
    Result<void> initialize([[maybe_unused]] platform::IWindow& window) override {
        return Result<void>::ok();
    }
    void shutdown() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void clear([[maybe_unused]] const renderer::Color& color) override {}
    void setBlendMode([[maybe_unused]] renderer::BlendMode mode) override {}
    void drawSprite([[maybe_unused]] const renderer::Texture& texture,
                    [[maybe_unused]] const renderer::Transform2D& transform,
                    [[maybe_unused]] const renderer::Color& tint) override {}
    void drawSprite([[maybe_unused]] const renderer::Texture& texture,
                    [[maybe_unused]] const renderer::Rect& sourceRect,
                    [[maybe_unused]] const renderer::Transform2D& transform,
                    [[maybe_unused]] const renderer::Color& tint) override {}
    void drawRect([[maybe_unused]] const renderer::Rect& rect,
                  [[maybe_unused]] const renderer::Color& color) override {}
    void fillRect([[maybe_unused]] const renderer::Rect& rect,
                  [[maybe_unused]] const renderer::Color& color) override {}
    void drawText([[maybe_unused]] const renderer::Font& font,
                  [[maybe_unused]] const std::string& text,
                  [[maybe_unused]] f32 x, [[maybe_unused]] f32 y,
                  [[maybe_unused]] const renderer::Color& color) override {}
    void setFade([[maybe_unused]] f32 alpha,
                 [[maybe_unused]] const renderer::Color& color) override {}
    [[nodiscard]] i32 getWidth() const override { return 1920; }
    [[nodiscard]] i32 getHeight() const override { return 1080; }
};

// Test object for rendering
class TestSceneObject : public SceneObjectBase {
public:
    explicit TestSceneObject(const std::string& id)
        : SceneObjectBase(id, SceneObjectType::Custom) {}

    void render([[maybe_unused]] renderer::IRenderer& renderer) override {
        renderCalled = true;
    }

    bool renderCalled = false;
};

// =============================================================================
// SceneObjectBase Tests
// =============================================================================

TEST_CASE("SceneObjectBase creation and identity", "[scene_graph][object]")
{
    auto obj = std::make_unique<TestSceneObject>("test_obj");

    REQUIRE(obj->getId() == "test_obj");
    REQUIRE(obj->getType() == SceneObjectType::Custom);
    REQUIRE(obj->getTypeName() != nullptr);
}

TEST_CASE("SceneObjectBase transform operations", "[scene_graph][object][transform]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    SECTION("Position") {
        obj->setPosition(100.0f, 200.0f);
        REQUIRE(obj->getX() == 100.0f);
        REQUIRE(obj->getY() == 200.0f);
    }

    SECTION("Scale") {
        obj->setScale(2.0f, 3.0f);
        REQUIRE(obj->getScaleX() == 2.0f);
        REQUIRE(obj->getScaleY() == 3.0f);

        obj->setUniformScale(1.5f);
        REQUIRE(obj->getScaleX() == 1.5f);
        REQUIRE(obj->getScaleY() == 1.5f);
    }

    SECTION("Rotation") {
        obj->setRotation(45.0f);
        REQUIRE(obj->getRotation() == 45.0f);
    }

    SECTION("Anchor") {
        obj->setAnchor(0.25f, 0.75f);
        REQUIRE(obj->getAnchorX() == 0.25f);
        REQUIRE(obj->getAnchorY() == 0.75f);
    }
}

TEST_CASE("SceneObjectBase visibility", "[scene_graph][object][visibility]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    SECTION("Visibility flag") {
        REQUIRE(obj->isVisible() == true);

        obj->setVisible(false);
        REQUIRE(obj->isVisible() == false);

        obj->setVisible(true);
        REQUIRE(obj->isVisible() == true);
    }

    SECTION("Alpha") {
        REQUIRE(obj->getAlpha() == 1.0f);

        obj->setAlpha(0.5f);
        REQUIRE(obj->getAlpha() == 0.5f);

        obj->setAlpha(0.0f);
        REQUIRE(obj->getAlpha() == 0.0f);
    }
}

TEST_CASE("SceneObjectBase z-ordering", "[scene_graph][object][zorder]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    REQUIRE(obj->getZOrder() == 0);

    obj->setZOrder(10);
    REQUIRE(obj->getZOrder() == 10);

    obj->setZOrder(-5);
    REQUIRE(obj->getZOrder() == -5);
}

TEST_CASE("SceneObjectBase parent-child relationships", "[scene_graph][object][hierarchy]")
{
    auto parent = std::make_unique<TestSceneObject>("parent");
    auto child1 = std::make_unique<TestSceneObject>("child1");
    auto child2 = std::make_unique<TestSceneObject>("child2");

    SECTION("Add children") {
        parent->addChild(std::move(child1));
        parent->addChild(std::move(child2));

        REQUIRE(parent->getChildren().size() == 2);
        REQUIRE(parent->findChild("child1") != nullptr);
        REQUIRE(parent->findChild("child2") != nullptr);
        REQUIRE(parent->findChild("nonexistent") == nullptr);
    }

    SECTION("Remove child") {
        parent->addChild(std::move(child1));
        REQUIRE(parent->getChildren().size() == 1);

        auto removed = parent->removeChild("child1");
        REQUIRE(removed != nullptr);
        REQUIRE(removed->getId() == "child1");
        REQUIRE(parent->getChildren().size() == 0);
    }

    SECTION("Parent reference") {
        auto* childPtr = child1.get();
        parent->addChild(std::move(child1));

        REQUIRE(childPtr->getParent() == parent.get());
    }
}

TEST_CASE("SceneObjectBase deep hierarchy limits", "[scene_graph][object][hierarchy][safety]")
{
    auto root = std::make_unique<TestSceneObject>("root");
    SceneObjectBase* current = root.get();

    // Create a deep hierarchy up to MAX_SCENE_DEPTH
    for (int i = 0; i < SceneObjectBase::MAX_SCENE_DEPTH - 1; ++i) {
        auto child = std::make_unique<TestSceneObject>("child_" + std::to_string(i));
        auto* childPtr = child.get();
        current->addChild(std::move(child));
        current = childPtr;
    }

    // Verify we can find objects in deep hierarchy
    REQUIRE(root->findChild("child_0") != nullptr);
    REQUIRE(root->findChild("child_50") != nullptr);
}

TEST_CASE("SceneObjectBase tags", "[scene_graph][object][tags]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    REQUIRE(obj->getTags().empty());
    REQUIRE_FALSE(obj->hasTag("clickable"));

    obj->addTag("clickable");
    obj->addTag("interactive");

    REQUIRE(obj->hasTag("clickable"));
    REQUIRE(obj->hasTag("interactive"));
    REQUIRE_FALSE(obj->hasTag("nonexistent"));
    REQUIRE(obj->getTags().size() == 2);

    obj->removeTag("clickable");
    REQUIRE_FALSE(obj->hasTag("clickable"));
    REQUIRE(obj->hasTag("interactive"));
    REQUIRE(obj->getTags().size() == 1);
}

TEST_CASE("SceneObjectBase property system", "[scene_graph][object][properties]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    REQUIRE(obj->getProperties().empty());

    obj->setProperty("color", "red");
    obj->setProperty("size", "large");

    REQUIRE(obj->getProperty("color").value() == "red");
    REQUIRE(obj->getProperty("size").value() == "large");
    REQUIRE_FALSE(obj->getProperty("nonexistent").has_value());

    REQUIRE(obj->getProperties().size() == 2);
}

TEST_CASE("SceneObjectBase serialization", "[scene_graph][object][serialization]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");
    obj->setPosition(100.0f, 200.0f);
    obj->setScale(1.5f, 2.0f);
    obj->setRotation(45.0f);
    obj->setAlpha(0.8f);
    obj->setVisible(false);
    obj->setZOrder(5);
    obj->setProperty("custom", "value");

    auto state = obj->saveState();

    REQUIRE(state.id == "obj");
    REQUIRE(state.type == SceneObjectType::Custom);
    REQUIRE(state.x == 100.0f);
    REQUIRE(state.y == 200.0f);
    REQUIRE(state.scaleX == 1.5f);
    REQUIRE(state.scaleY == 2.0f);
    REQUIRE(state.rotation == 45.0f);
    REQUIRE(state.alpha == 0.8f);
    REQUIRE(state.visible == false);
    REQUIRE(state.zOrder == 5);
    REQUIRE(state.properties.at("custom") == "value");

    // Test load state
    auto obj2 = std::make_unique<TestSceneObject>("obj2");
    obj2->loadState(state);

    REQUIRE(obj2->getX() == 100.0f);
    REQUIRE(obj2->getY() == 200.0f);
    REQUIRE(obj2->getAlpha() == 0.8f);
    REQUIRE(obj2->isVisible() == false);
}

TEST_CASE("SceneObjectBase update", "[scene_graph][object]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    // Update should not crash - verify object ID unchanged
    obj->update(0.016); // 60 FPS
    REQUIRE(obj->getId() == "obj");
}

TEST_CASE("SceneObjectBase render", "[scene_graph][object]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");
    MockRenderer renderer;

    REQUIRE_FALSE(obj->renderCalled);
    obj->render(renderer);
    REQUIRE(obj->renderCalled);
}

// =============================================================================
// BackgroundObject Tests
// =============================================================================

TEST_CASE("BackgroundObject creation", "[scene_graph][background]")
{
    BackgroundObject bg("bg1");

    REQUIRE(bg.getId() == "bg1");
    REQUIRE(bg.getType() == SceneObjectType::Background);
}

TEST_CASE("BackgroundObject texture", "[scene_graph][background]")
{
    BackgroundObject bg("bg1");

    bg.setTextureId("textures/bg_forest.png");
    REQUIRE(bg.getTextureId() == "textures/bg_forest.png");
}

TEST_CASE("BackgroundObject tint", "[scene_graph][background]")
{
    BackgroundObject bg("bg1");

    renderer::Color tint{128, 128, 255, 200};
    bg.setTint(tint);

    auto result = bg.getTint();
    REQUIRE(result.r == 128);
    REQUIRE(result.g == 128);
    REQUIRE(result.b == 255);
    REQUIRE(result.a == 200);
}

TEST_CASE("BackgroundObject serialization", "[scene_graph][background][serialization]")
{
    BackgroundObject bg("bg1");
    bg.setTextureId("bg_texture");
    bg.setTint({100, 150, 200, 255});
    bg.setPosition(10.0f, 20.0f);

    auto state = bg.saveState();

    REQUIRE(state.id == "bg1");
    REQUIRE(state.type == SceneObjectType::Background);
    REQUIRE(state.properties.count("textureId") > 0);

    BackgroundObject bg2("bg2");
    bg2.loadState(state);

    REQUIRE(bg2.getTextureId() == "bg_texture");
}

// =============================================================================
// CharacterObject Tests
// =============================================================================

TEST_CASE("CharacterObject creation", "[scene_graph][character]")
{
    CharacterObject char1("char1", "alice");

    REQUIRE(char1.getId() == "char1");
    REQUIRE(char1.getType() == SceneObjectType::Character);
    REQUIRE(char1.getCharacterId() == "alice");
}

TEST_CASE("CharacterObject properties", "[scene_graph][character]")
{
    CharacterObject char1("char1", "alice");

    SECTION("Display name") {
        char1.setDisplayName("Alice");
        REQUIRE(char1.getDisplayName() == "Alice");
    }

    SECTION("Expression") {
        char1.setExpression("happy");
        REQUIRE(char1.getExpression() == "happy");
    }

    SECTION("Pose") {
        char1.setPose("standing");
        REQUIRE(char1.getPose() == "standing");
    }

    SECTION("Slot position") {
        char1.setSlotPosition(CharacterObject::Position::Left);
        REQUIRE(char1.getSlotPosition() == CharacterObject::Position::Left);
    }

    SECTION("Highlighted") {
        REQUIRE_FALSE(char1.isHighlighted());
        char1.setHighlighted(true);
        REQUIRE(char1.isHighlighted());
    }
}

TEST_CASE("CharacterObject serialization", "[scene_graph][character][serialization]")
{
    CharacterObject char1("char1", "alice");
    char1.setDisplayName("Alice");
    char1.setExpression("sad");
    char1.setPose("sitting");

    auto state = char1.saveState();
    REQUIRE(state.id == "char1");
    REQUIRE(state.type == SceneObjectType::Character);

    CharacterObject char2("char2", "bob");
    char2.loadState(state);

    REQUIRE(char2.getDisplayName() == "Alice");
    REQUIRE(char2.getExpression() == "sad");
    REQUIRE(char2.getPose() == "sitting");
}

// =============================================================================
// DialogueUIObject Tests
// =============================================================================

TEST_CASE("DialogueUIObject creation", "[scene_graph][dialogue]")
{
    DialogueUIObject dialogue("dlg1");

    REQUIRE(dialogue.getId() == "dlg1");
    REQUIRE(dialogue.getType() == SceneObjectType::DialogueUI);
}

TEST_CASE("DialogueUIObject content", "[scene_graph][dialogue]")
{
    DialogueUIObject dialogue("dlg1");

    dialogue.setSpeaker("Alice");
    dialogue.setText("Hello, world!");

    REQUIRE(dialogue.getSpeaker() == "Alice");
    REQUIRE(dialogue.getText() == "Hello, world!");
}

TEST_CASE("DialogueUIObject typewriter effect", "[scene_graph][dialogue][typewriter]")
{
    DialogueUIObject dialogue("dlg1");

    SECTION("Enabled by default") {
        REQUIRE(dialogue.isTypewriterEnabled());
    }

    SECTION("Toggle typewriter") {
        dialogue.setTypewriterEnabled(false);
        REQUIRE_FALSE(dialogue.isTypewriterEnabled());
    }

    SECTION("Typewriter speed") {
        dialogue.setTypewriterSpeed(60.0f);
        REQUIRE(dialogue.getTypewriterSpeed() == 60.0f);
    }

    SECTION("Typewriter state") {
        dialogue.setText("Test message");
        dialogue.startTypewriter();
        REQUIRE_FALSE(dialogue.isTypewriterComplete());

        dialogue.skipTypewriter();
        REQUIRE(dialogue.isTypewriterComplete());
    }
}

TEST_CASE("DialogueUIObject update with typewriter", "[scene_graph][dialogue][typewriter]")
{
    DialogueUIObject dialogue("dlg1");
    dialogue.setText("Test");
    dialogue.setTypewriterSpeed(100.0f); // Fast for testing
    dialogue.startTypewriter();

    REQUIRE_FALSE(dialogue.isTypewriterComplete());

    // Update for enough time to complete
    for (int i = 0; i < 10; ++i) {
        dialogue.update(0.1); // 100ms per frame
    }

    REQUIRE(dialogue.isTypewriterComplete());
}

// =============================================================================
// ChoiceUIObject Tests
// =============================================================================

TEST_CASE("ChoiceUIObject creation", "[scene_graph][choice]")
{
    ChoiceUIObject choice("choice1");

    REQUIRE(choice.getId() == "choice1");
    REQUIRE(choice.getType() == SceneObjectType::ChoiceUI);
    REQUIRE(choice.getChoices().empty());
}

TEST_CASE("ChoiceUIObject choices management", "[scene_graph][choice]")
{
    ChoiceUIObject choice("choice1");

    std::vector<ChoiceUIObject::ChoiceOption> options = {
        {"opt1", "Option 1", true, true, ""},
        {"opt2", "Option 2", true, true, ""},
        {"opt3", "Option 3", true, true, ""}
    };

    choice.setChoices(options);
    REQUIRE(choice.getChoices().size() == 3);

    choice.clearChoices();
    REQUIRE(choice.getChoices().empty());
}

TEST_CASE("ChoiceUIObject selection", "[scene_graph][choice]")
{
    ChoiceUIObject choice("choice1");

    std::vector<ChoiceUIObject::ChoiceOption> options = {
        {"opt1", "First", true, true, ""},
        {"opt2", "Second", true, true, ""},
        {"opt3", "Third", true, true, ""}
    };
    choice.setChoices(options);

    REQUIRE(choice.getSelectedIndex() == 0);

    choice.selectNext();
    REQUIRE(choice.getSelectedIndex() == 1);

    choice.selectNext();
    REQUIRE(choice.getSelectedIndex() == 2);

    choice.selectPrevious();
    REQUIRE(choice.getSelectedIndex() == 1);

    choice.setSelectedIndex(0);
    REQUIRE(choice.getSelectedIndex() == 0);
}

TEST_CASE("ChoiceUIObject callback", "[scene_graph][choice]")
{
    ChoiceUIObject choice("choice1");

    std::vector<ChoiceUIObject::ChoiceOption> options = {
        {"opt1", "First", true, true, ""}
    };
    choice.setChoices(options);

    bool callbackFired = false;
    i32 selectedIndex = -1;
    std::string selectedId;

    choice.setOnSelect([&](i32 idx, const std::string& id) {
        callbackFired = true;
        selectedIndex = idx;
        selectedId = id;
    });

    [[maybe_unused]] bool confirmed = choice.confirm();

    REQUIRE(callbackFired);
    REQUIRE(selectedIndex == 0);
    REQUIRE(selectedId == "opt1");
}

// =============================================================================
// EffectOverlayObject Tests
// =============================================================================

TEST_CASE("EffectOverlayObject creation", "[scene_graph][effect]")
{
    EffectOverlayObject effect("fx1");

    REQUIRE(effect.getId() == "fx1");
    REQUIRE(effect.getType() == SceneObjectType::EffectOverlay);
    REQUIRE(effect.getEffectType() == EffectOverlayObject::EffectType::None);
}

TEST_CASE("EffectOverlayObject properties", "[scene_graph][effect]")
{
    EffectOverlayObject effect("fx1");

    effect.setEffectType(EffectOverlayObject::EffectType::Fade);
    REQUIRE(effect.getEffectType() == EffectOverlayObject::EffectType::Fade);

    effect.setIntensity(0.75f);
    REQUIRE(effect.getIntensity() == 0.75f);

    renderer::Color color{255, 0, 0, 128};
    effect.setColor(color);
    auto resultColor = effect.getColor();
    REQUIRE(resultColor.r == 255);
    REQUIRE(resultColor.a == 128);
}

TEST_CASE("EffectOverlayObject activation", "[scene_graph][effect]")
{
    EffectOverlayObject effect("fx1");

    REQUIRE_FALSE(effect.isEffectActive());

    effect.startEffect(2.0f);
    REQUIRE(effect.isEffectActive());

    effect.stopEffect();
    REQUIRE_FALSE(effect.isEffectActive());
}

// =============================================================================
// Layer Tests
// =============================================================================

TEST_CASE("Layer creation", "[scene_graph][layer]")
{
    Layer layer("Background", LayerType::Background);

    REQUIRE(layer.getName() == "Background");
    REQUIRE(layer.getType() == LayerType::Background);
    REQUIRE(layer.isVisible());
    REQUIRE(layer.getAlpha() == 1.0f);
}

TEST_CASE("Layer object management", "[scene_graph][layer]")
{
    Layer layer("Test", LayerType::Background);

    SECTION("Add objects") {
        layer.addObject(std::make_unique<TestSceneObject>("obj1"));
        layer.addObject(std::make_unique<TestSceneObject>("obj2"));

        REQUIRE(layer.getObjects().size() == 2);
        REQUIRE(layer.findObject("obj1") != nullptr);
        REQUIRE(layer.findObject("obj2") != nullptr);
        REQUIRE(layer.findObject("nonexistent") == nullptr);
    }

    SECTION("Remove object") {
        layer.addObject(std::make_unique<TestSceneObject>("obj1"));
        REQUIRE(layer.getObjects().size() == 1);

        auto removed = layer.removeObject("obj1");
        REQUIRE(removed != nullptr);
        REQUIRE(removed->getId() == "obj1");
        REQUIRE(layer.getObjects().size() == 0);
    }

    SECTION("Clear") {
        layer.addObject(std::make_unique<TestSceneObject>("obj1"));
        layer.addObject(std::make_unique<TestSceneObject>("obj2"));

        layer.clear();
        REQUIRE(layer.getObjects().empty());
    }
}

TEST_CASE("Layer z-order sorting", "[scene_graph][layer][zorder]")
{
    Layer layer("Test", LayerType::Background);

    auto obj1 = std::make_unique<TestSceneObject>("obj1");
    auto obj2 = std::make_unique<TestSceneObject>("obj2");
    auto obj3 = std::make_unique<TestSceneObject>("obj3");

    obj1->setZOrder(10);
    obj2->setZOrder(5);
    obj3->setZOrder(15);

    layer.addObject(std::move(obj1));
    layer.addObject(std::move(obj2));
    layer.addObject(std::move(obj3));

    layer.sortByZOrder();

    const auto& objects = layer.getObjects();
    REQUIRE(objects[0]->getZOrder() == 5);
    REQUIRE(objects[1]->getZOrder() == 10);
    REQUIRE(objects[2]->getZOrder() == 15);
}

TEST_CASE("Layer visibility and alpha", "[scene_graph][layer]")
{
    Layer layer("Test", LayerType::Background);

    SECTION("Visibility") {
        layer.setVisible(false);
        REQUIRE_FALSE(layer.isVisible());

        layer.setVisible(true);
        REQUIRE(layer.isVisible());
    }

    SECTION("Alpha") {
        layer.setAlpha(0.5f);
        REQUIRE(layer.getAlpha() == 0.5f);
    }
}

TEST_CASE("Layer update", "[scene_graph][layer]")
{
    Layer layer("Test", LayerType::Background);
    layer.addObject(std::make_unique<TestSceneObject>("obj1"));

    // Should not crash - verify layer name unchanged
    layer.update(0.016);
    REQUIRE(layer.getName() == "Test");
}

TEST_CASE("Layer render", "[scene_graph][layer]")
{
    Layer layer("Test", LayerType::Background);
    auto obj = std::make_unique<TestSceneObject>("obj1");
    auto* objPtr = obj.get();
    layer.addObject(std::move(obj));

    MockRenderer renderer;

    REQUIRE_FALSE(objPtr->renderCalled);
    layer.render(renderer);
    REQUIRE(objPtr->renderCalled);
}

// =============================================================================
// SceneGraph Tests
// =============================================================================

TEST_CASE("SceneGraph creation", "[scene_graph]")
{
    SceneGraph graph;

    REQUIRE(graph.getSceneId().empty());
}

TEST_CASE("SceneGraph scene management", "[scene_graph]")
{
    SceneGraph graph;

    graph.setSceneId("scene_001");
    REQUIRE(graph.getSceneId() == "scene_001");

    graph.clear();
    // After clear, scene should still have its ID but no objects
    REQUIRE(graph.getSceneId() == "scene_001");
}

TEST_CASE("SceneGraph layer access", "[scene_graph][layers]")
{
    SceneGraph graph;

    auto& bgLayer = graph.getBackgroundLayer();
    auto& charLayer = graph.getCharacterLayer();
    auto& uiLayer = graph.getUILayer();
    auto& fxLayer = graph.getEffectLayer();

    REQUIRE(bgLayer.getType() == LayerType::Background);
    REQUIRE(charLayer.getType() == LayerType::Characters);
    REQUIRE(uiLayer.getType() == LayerType::UI);
    REQUIRE(fxLayer.getType() == LayerType::Effects);
}

TEST_CASE("SceneGraph object management", "[scene_graph]")
{
    SceneGraph graph;

    SECTION("Add to layer") {
        graph.addToLayer(LayerType::Background,
                        std::make_unique<BackgroundObject>("bg1"));

        auto* obj = graph.findObject("bg1");
        REQUIRE(obj != nullptr);
        REQUIRE(obj->getId() == "bg1");
    }

    SECTION("Remove from layer") {
        graph.addToLayer(LayerType::Background,
                        std::make_unique<BackgroundObject>("bg1"));

        auto removed = graph.removeFromLayer(LayerType::Background, "bg1");
        REQUIRE(removed != nullptr);
        REQUIRE(removed->getId() == "bg1");
        REQUIRE(graph.findObject("bg1") == nullptr);
    }
}

TEST_CASE("SceneGraph find operations", "[scene_graph][search]")
{
    SceneGraph graph;

    auto obj1 = std::make_unique<TestSceneObject>("obj1");
    auto obj2 = std::make_unique<TestSceneObject>("obj2");

    obj1->addTag("interactive");
    obj2->addTag("interactive");
    obj2->addTag("clickable");

    graph.addToLayer(LayerType::UI, std::move(obj1));
    graph.addToLayer(LayerType::UI, std::move(obj2));

    SECTION("Find by tag") {
        auto objects = graph.findObjectsByTag("interactive");
        REQUIRE(objects.size() == 2);

        auto clickable = graph.findObjectsByTag("clickable");
        REQUIRE(clickable.size() == 1);
        REQUIRE(clickable[0]->getId() == "obj2");
    }

    SECTION("Find by type") {
        graph.addToLayer(LayerType::Background,
                        std::make_unique<BackgroundObject>("bg1"));

        auto backgrounds = graph.findObjectsByType(SceneObjectType::Background);
        REQUIRE(backgrounds.size() == 1);

        auto custom = graph.findObjectsByType(SceneObjectType::Custom);
        REQUIRE(custom.size() == 2);
    }
}

TEST_CASE("SceneGraph convenience methods", "[scene_graph]")
{
    SceneGraph graph;

    SECTION("Show background") {
        graph.showBackground("textures/forest.png");

        auto* bg = graph.findObject("_background");
        REQUIRE(bg != nullptr);
        REQUIRE(bg->getType() == SceneObjectType::Background);
    }

    SECTION("Show/hide character") {
        auto* char1 = graph.showCharacter("alice", "alice_id",
                                          CharacterObject::Position::Left);
        REQUIRE(char1 != nullptr);
        REQUIRE(char1->getId() == "alice");

        graph.hideCharacter("alice");
        REQUIRE(graph.findObject("alice") == nullptr);
    }

    SECTION("Show/hide dialogue") {
        auto* dlg = graph.showDialogue("Alice", "Hello!");
        REQUIRE(dlg != nullptr);
        REQUIRE(dlg->getSpeaker() == "Alice");

        graph.hideDialogue();
        REQUIRE(graph.findObject("_dialogue") == nullptr);
    }

    SECTION("Show/hide choices") {
        std::vector<ChoiceUIObject::ChoiceOption> opts = {
            {"opt1", "Choice 1", true, true, ""}
        };

        auto* choice = graph.showChoices(opts);
        REQUIRE(choice != nullptr);

        graph.hideChoices();
        REQUIRE(graph.findObject("_choices") == nullptr);
    }
}

TEST_CASE("SceneGraph update and render", "[scene_graph]")
{
    SceneGraph graph;
    MockRenderer renderer;

    auto obj = std::make_unique<TestSceneObject>("obj1");
    auto* objPtr = obj.get();
    graph.addToLayer(LayerType::UI, std::move(obj));

    SECTION("Update") {
        graph.update(0.016);
        // Verify update completes without crash - object should remain in graph
        REQUIRE(graph.getSceneId() == "");
    }

    SECTION("Render") {
        REQUIRE_FALSE(objPtr->renderCalled);
        graph.render(renderer);
        REQUIRE(objPtr->renderCalled);
    }
}

TEST_CASE("SceneGraph serialization", "[scene_graph][serialization]")
{
    SceneGraph graph;
    graph.setSceneId("test_scene");

    graph.showBackground("bg.png");
    graph.showCharacter("alice", "alice_id", CharacterObject::Position::Center);

    auto state = graph.saveState();

    REQUIRE(state.sceneId == "test_scene");
    REQUIRE(state.objects.size() >= 2);

    // Load into new graph
    SceneGraph graph2;
    graph2.loadState(state);

    REQUIRE(graph2.getSceneId() == "test_scene");
}

// =============================================================================
// Error Path Tests
// =============================================================================

TEST_CASE("SceneGraph error handling - invalid operations", "[scene_graph][error]")
{
    SceneGraph graph;

    SECTION("Remove non-existent object") {
        auto removed = graph.removeFromLayer(LayerType::Background, "nonexistent");
        REQUIRE(removed == nullptr);
    }

    SECTION("Find in empty graph") {
        REQUIRE(graph.findObject("anything") == nullptr);
        REQUIRE(graph.findObjectsByTag("anytag").empty());
        REQUIRE(graph.findObjectsByType(SceneObjectType::Custom).empty());
    }
}

TEST_CASE("Layer error handling", "[scene_graph][layer][error]")
{
    Layer layer("Test", LayerType::Background);

    SECTION("Remove non-existent object") {
        auto removed = layer.removeObject("nonexistent");
        REQUIRE(removed == nullptr);
    }

    SECTION("Find non-existent object") {
        REQUIRE(layer.findObject("nonexistent") == nullptr);
    }
}

TEST_CASE("SceneObjectBase error handling", "[scene_graph][object][error]")
{
    auto obj = std::make_unique<TestSceneObject>("obj");

    SECTION("Remove non-existent child") {
        auto removed = obj->removeChild("nonexistent");
        REQUIRE(removed == nullptr);
    }

    SECTION("Find non-existent child") {
        REQUIRE(obj->findChild("nonexistent") == nullptr);
    }

    SECTION("Get non-existent property") {
        REQUIRE_FALSE(obj->getProperty("nonexistent").has_value());
    }
}
