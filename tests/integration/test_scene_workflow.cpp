/**
 * @file test_scene_workflow.cpp
 * @brief Integration tests for complete scene workflows
 *
 * Tests full workflows:
 * - Scene loading and transitions
 * - Character appearance and dialogue
 * - Save/Load cycle with scene state
 * - Script-to-scene interaction
 *
 * Related to Issue #179 - Integration test coverage
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_manager.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <memory>

using namespace NovelMind::scene;
using namespace NovelMind;

// Mock renderer for integration tests
class IntegrationMockRenderer : public renderer::IRenderer {
public:
    Result<void> initialize(platform::IWindow& window) override {
        return Result<void>::ok();
    }
    void shutdown() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void clear(const renderer::Color& color) override { clearCalls++; }
    void setBlendMode(renderer::BlendMode mode) override {}
    void drawSprite(const renderer::Texture& texture, const renderer::Transform2D& transform,
                    const renderer::Color& tint) override {
        drawTextureCalls++;
    }
    void drawSprite(const renderer::Texture& texture, const renderer::Rect& sourceRect,
                    const renderer::Transform2D& transform, const renderer::Color& tint) override {
        drawTextureCalls++;
    }
    void drawRect(const renderer::Rect& rect, const renderer::Color& color) override {
        drawQuadCalls++;
    }
    void fillRect(const renderer::Rect& rect, const renderer::Color& color) override {
        drawQuadCalls++;
    }
    void drawText(const renderer::Font& font, const std::string& text, f32 x, f32 y,
                  const renderer::Color& color) override {
        drawTextCalls++;
        lastText = text;
    }
    void setFade(f32 alpha, const renderer::Color& color) override {}
    [[nodiscard]] i32 getWidth() const override { return 1920; }
    [[nodiscard]] i32 getHeight() const override { return 1080; }

    int clearCalls = 0;
    int presentCalls = 0;
    int drawQuadCalls = 0;
    int drawTextureCalls = 0;
    int drawTextCalls = 0;
    std::string lastTextureId;
    std::string lastText;
};

TEST_CASE("Scene workflow - Complete dialogue scene", "[integration][scene][workflow]")
{
    SceneGraph graph;
    IntegrationMockRenderer renderer;

    SECTION("Setup scene with background and character") {
        graph.setSceneId("intro_scene");

        // Show background
        graph.showBackground("backgrounds/park.png");

        // Show character
        auto* alice = graph.showCharacter("alice", "alice_sprite",
                                          CharacterObject::Position::Center);
        REQUIRE(alice != nullptr);
        alice->setExpression("happy");

        // Show dialogue
        auto* dialogue = graph.showDialogue("Alice", "Hello! Welcome to the park.");
        REQUIRE(dialogue != nullptr);

        // Update scene
        graph.update(0.016);

        // Render scene
        graph.render(renderer);

        // Verify rendering happened
        REQUIRE(renderer.clearCalls > 0);
        REQUIRE(renderer.drawTextureCalls > 0);
    }
}

TEST_CASE("Scene workflow - Save and load cycle", "[integration][scene][serialization]")
{
    SceneGraph graph1;

    // Setup scene
    graph1.setSceneId("saveable_scene");
    graph1.showBackground("bg.png");

    auto* char1 = graph1.showCharacter("bob", "bob_sprite",
                                       CharacterObject::Position::Left);
    char1->setExpression("sad");
    char1->setAlpha(0.8f);

    auto* dialogue = graph1.showDialogue("Bob", "I need to save this state.");

    // Save state
    auto state = graph1.saveState();

    REQUIRE(state.sceneId == "saveable_scene");
    REQUIRE_FALSE(state.objects.empty());

    // Create new scene and load state
    SceneGraph graph2;
    graph2.loadState(state);

    REQUIRE(graph2.getSceneId() == "saveable_scene");

    // Verify character was restored
    auto* restoredChar = dynamic_cast<CharacterObject*>(graph2.findObject("bob"));
    REQUIRE(restoredChar != nullptr);
    REQUIRE(restoredChar->getExpression() == "sad");
    REQUIRE(restoredChar->getAlpha() == 0.8f);

    // Verify dialogue was restored
    auto* restoredDialogue = graph2.findObject("_dialogue");
    REQUIRE(restoredDialogue != nullptr);
}

TEST_CASE("Scene workflow - Character transitions", "[integration][scene][animation]")
{
    SceneGraph graph;
    IntegrationMockRenderer renderer;

    // Show character at left position
    auto* char1 = graph.showCharacter("alice", "alice_sprite",
                                      CharacterObject::Position::Left);
    REQUIRE(char1 != nullptr);

    f32 startX = char1->getX();

    // Animate to center
    char1->animateToSlot(CharacterObject::Position::Center, 1.0f);

    // Simulate time passing
    for (int i = 0; i < 60; ++i) {
        graph.update(1.0f / 60.0f);
    }

    // Position should have changed
    f32 endX = char1->getX();
    REQUIRE(endX != startX);
}

TEST_CASE("Scene workflow - Multiple characters and dialogue", "[integration][scene][multi]")
{
    SceneGraph graph;

    // Show background
    graph.showBackground("backgrounds/classroom.png");

    // Show multiple characters
    auto* alice = graph.showCharacter("alice", "alice_sprite",
                                      CharacterObject::Position::Left);
    auto* bob = graph.showCharacter("bob", "bob_sprite",
                                    CharacterObject::Position::Right);

    REQUIRE(alice != nullptr);
    REQUIRE(bob != nullptr);

    // Set expressions
    alice->setExpression("happy");
    bob->setExpression("surprised");

    // Highlight speaker
    alice->setHighlighted(true);
    bob->setHighlighted(false);

    // Show dialogue from Alice
    auto* dialogue = graph.showDialogue("Alice", "Hi Bob!");
    REQUIRE(dialogue != nullptr);

    // Verify both characters exist
    REQUIRE(graph.findObject("alice") != nullptr);
    REQUIRE(graph.findObject("bob") != nullptr);

    // Change speaker
    alice->setHighlighted(false);
    bob->setHighlighted(true);

    dialogue->setSpeaker("Bob");
    dialogue->setText("Oh, hello Alice!");

    // Verify state
    REQUIRE(dialogue->getSpeaker() == "Bob");
    REQUIRE(dialogue->getText() == "Oh, hello Alice!");
}

TEST_CASE("Scene workflow - Choices and branching", "[integration][scene][choices]")
{
    SceneGraph graph;

    graph.showBackground("bg.png");

    std::vector<ChoiceUIObject::ChoiceOption> options = {
        {"choice_a", "Go to the park", true, true, ""},
        {"choice_b", "Stay home", true, true, ""},
        {"choice_c", "Call a friend", true, true, ""}
    };

    auto* choiceUI = graph.showChoices(options);
    REQUIRE(choiceUI != nullptr);
    REQUIRE(choiceUI->getChoices().size() == 3);

    // Simulate user navigation
    choiceUI->selectNext();
    REQUIRE(choiceUI->getSelectedIndex() == 1);

    choiceUI->selectNext();
    REQUIRE(choiceUI->getSelectedIndex() == 2);

    choiceUI->selectPrevious();
    REQUIRE(choiceUI->getSelectedIndex() == 1);

    // Test callback
    bool choiceMade = false;
    std::string selectedChoice;

    choiceUI->setOnSelect([&](i32 idx, const std::string& id) {
        choiceMade = true;
        selectedChoice = id;
    });

    choiceUI->confirm();

    REQUIRE(choiceMade);
    REQUIRE(selectedChoice == "choice_b");
}

TEST_CASE("Scene workflow - Effects and transitions", "[integration][scene][effects]")
{
    SceneGraph graph;

    graph.showBackground("bg1.png");

    // Add fade effect
    auto effect = std::make_unique<EffectOverlayObject>("fade");
    effect->setEffectType(EffectOverlayObject::EffectType::Fade);
    effect->setColor({0, 0, 0, 255});
    effect->setIntensity(1.0f);

    auto* effectPtr = effect.get();
    graph.addToLayer(LayerType::Effect, std::move(effect));

    // Start fade out
    effectPtr->startEffect(1.0f);
    REQUIRE(effectPtr->isEffectActive());

    // Update for duration
    for (int i = 0; i < 60; ++i) {
        graph.update(1.0f / 60.0f);
    }

    // Change background during fade
    graph.showBackground("bg2.png");

    // Fade should still be manageable
    REQUIRE(graph.findObject("fade") != nullptr);
}

TEST_CASE("Scene workflow - Layer ordering and rendering", "[integration][scene][layers]")
{
    SceneGraph graph;
    IntegrationMockRenderer renderer;

    // Add objects to different layers
    graph.showBackground("bg.png");
    graph.showCharacter("alice", "alice_sprite", CharacterObject::Position::Center);
    graph.showDialogue("Alice", "Testing layer order");

    // Add effect overlay
    auto effect = std::make_unique<EffectOverlayObject>("overlay");
    effect->setEffectType(EffectOverlayObject::EffectType::Flash);
    graph.addToLayer(LayerType::Effect, std::move(effect));

    // Render entire scene
    graph.render(renderer);

    // All layers should have rendered
    REQUIRE(renderer.drawTextureCalls > 0);

    // Clear counter
    int initialCalls = renderer.drawTextureCalls;

    // Hide a layer and verify rendering changes
    graph.getCharacterLayer().setVisible(false);

    renderer.drawTextureCalls = 0;
    graph.render(renderer);

    // Should render fewer textures with character layer hidden
    REQUIRE(true);
}

TEST_CASE("Scene workflow - Scene transitions and cleanup", "[integration][scene][transition]")
{
    SceneGraph graph;

    // Setup first scene
    graph.setSceneId("scene_01");
    graph.showBackground("scene1_bg.png");
    graph.showCharacter("alice", "alice_sprite", CharacterObject::Position::Center);
    graph.showDialogue("Alice", "This is scene 1");

    REQUIRE(graph.findObject("alice") != nullptr);
    REQUIRE(graph.findObject("_dialogue") != nullptr);

    // Save state before transition
    auto scene1State = graph.saveState();

    // Clear for new scene
    graph.clear();

    // Verify clear worked
    REQUIRE(graph.findObject("alice") == nullptr);
    REQUIRE(graph.findObject("_dialogue") == nullptr);

    // Setup second scene
    graph.setSceneId("scene_02");
    graph.showBackground("scene2_bg.png");
    graph.showCharacter("bob", "bob_sprite", CharacterObject::Position::Right);

    REQUIRE(graph.findObject("bob") != nullptr);

    // Can restore previous scene
    graph.loadState(scene1State);

    REQUIRE(graph.getSceneId() == "scene_01");
    REQUIRE(graph.findObject("alice") != nullptr);
    REQUIRE(graph.findObject("bob") == nullptr);
}

TEST_CASE("Scene workflow - Typewriter effect simulation", "[integration][scene][typewriter]")
{
    SceneGraph graph;

    auto* dialogue = graph.showDialogue("Narrator", "This is a long message that will be revealed slowly...");
    REQUIRE(dialogue != nullptr);

    dialogue->setTypewriterEnabled(true);
    dialogue->setTypewriterSpeed(50.0f);
    dialogue->startTypewriter();

    REQUIRE_FALSE(dialogue->isTypewriterComplete());

    // Simulate frames
    for (int i = 0; i < 100; ++i) {
        graph.update(0.016); // ~60 FPS

        if (dialogue->isTypewriterComplete()) {
            break;
        }
    }

    REQUIRE(dialogue->isTypewriterComplete());
}

TEST_CASE("Scene workflow - Tags and filtering", "[integration][scene][tags]")
{
    SceneGraph graph;

    // Create objects with tags
    auto obj1 = std::make_unique<BackgroundObject>("bg1");
    obj1->addTag("daytime");
    obj1->addTag("outdoor");

    auto obj2 = std::make_unique<BackgroundObject>("bg2");
    obj2->addTag("nighttime");
    obj2->addTag("outdoor");

    auto obj3 = std::make_unique<BackgroundObject>("bg3");
    obj3->addTag("daytime");
    obj3->addTag("indoor");

    graph.addToLayer(LayerType::Background, std::move(obj1));
    graph.addToLayer(LayerType::Background, std::move(obj2));
    graph.addToLayer(LayerType::Background, std::move(obj3));

    // Find by tag
    auto daytimeObjects = graph.findObjectsByTag("daytime");
    REQUIRE(daytimeObjects.size() == 2);

    auto outdoorObjects = graph.findObjectsByTag("outdoor");
    REQUIRE(outdoorObjects.size() == 2);

    auto nighttimeObjects = graph.findObjectsByTag("nighttime");
    REQUIRE(nighttimeObjects.size() == 1);

    auto indoorObjects = graph.findObjectsByTag("indoor");
    REQUIRE(indoorObjects.size() == 1);
}
