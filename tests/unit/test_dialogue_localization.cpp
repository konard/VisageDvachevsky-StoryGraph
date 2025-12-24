/**
 * @file test_dialogue_localization.cpp
 * @brief Unit tests for dialogue localization in Scene Nodes
 *
 * Tests the localization key generation, translation status tracking,
 * and dialogue entry collection for embedded dialogue in Scene Nodes.
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/ir.hpp"
#include <string>

using namespace NovelMind::scripting;

TEST_CASE("DialogueLocalizationData - Default Construction", "[localization][dialogue]") {
  DialogueLocalizationData locData;

  REQUIRE(locData.localizationKey.empty());
  REQUIRE(locData.customKeyOverride.empty());
  REQUIRE(locData.status == TranslationStatus::Untranslated);
  REQUIRE(locData.useCustomKey == false);
}

TEST_CASE("DialogueLocalizationData - Key Generation", "[localization][dialogue]") {
  SECTION("Generate dialogue key") {
    std::string key = DialogueLocalizationData::generateKey("intro_scene", 42);
    REQUIRE(key == "scene.intro_scene.dialogue.42");
  }

  SECTION("Generate choice key") {
    std::string key = DialogueLocalizationData::generateChoiceKey("main_menu", 10, 2);
    REQUIRE(key == "scene.main_menu.choice.10.2");
  }

  SECTION("Key format consistency") {
    std::string key1 = DialogueLocalizationData::generateKey("scene_a", 1);
    std::string key2 = DialogueLocalizationData::generateKey("scene_a", 2);

    // Keys should be different for different node IDs
    REQUIRE(key1 != key2);

    // Keys should follow the same pattern
    REQUIRE(key1.find("scene.scene_a.dialogue.") == 0);
    REQUIRE(key2.find("scene.scene_a.dialogue.") == 0);
  }
}

TEST_CASE("DialogueLocalizationData - Effective Key Selection", "[localization][dialogue]") {
  DialogueLocalizationData locData;
  locData.localizationKey = "auto.generated.key";

  SECTION("Returns auto-generated key by default") {
    REQUIRE(locData.getEffectiveKey() == "auto.generated.key");
  }

  SECTION("Returns custom key when set") {
    locData.customKeyOverride = "custom.override.key";
    locData.useCustomKey = true;

    REQUIRE(locData.getEffectiveKey() == "custom.override.key");
  }

  SECTION("Returns auto key when custom is empty") {
    locData.useCustomKey = true;
    locData.customKeyOverride = "";

    REQUIRE(locData.getEffectiveKey() == "auto.generated.key");
  }
}

TEST_CASE("TranslationStatus - Status Values", "[localization][dialogue]") {
  REQUIRE(static_cast<int>(TranslationStatus::NotLocalizable) == 0);
  REQUIRE(static_cast<int>(TranslationStatus::Untranslated) == 1);
  REQUIRE(static_cast<int>(TranslationStatus::Translated) == 2);
  REQUIRE(static_cast<int>(TranslationStatus::NeedsReview) == 3);
  REQUIRE(static_cast<int>(TranslationStatus::Missing) == 4);
}

TEST_CASE("DialogueLocalizationEntry - Default Construction", "[localization][dialogue]") {
  DialogueLocalizationEntry entry;

  REQUIRE(entry.key.empty());
  REQUIRE(entry.sourceText.empty());
  REQUIRE(entry.speaker.empty());
  REQUIRE(entry.nodeId == 0);
  REQUIRE(entry.sceneId.empty());
  REQUIRE(entry.status == TranslationStatus::Untranslated);
}

TEST_CASE("DialogueLocalizationHelper - Check Localization Key", "[localization][dialogue]") {
  DialogueLocalizationHelper helper;
  IRGraph graph;
  graph.setName("TestScene");

  NodeId dialogueId = graph.createNode(IRNodeType::Dialogue);
  IRNode* dialogueNode = graph.getNode(dialogueId);

  SECTION("Node without localization key") {
    REQUIRE_FALSE(helper.hasLocalizationKey(*dialogueNode));
    REQUIRE(helper.getLocalizationKey(*dialogueNode).empty());
  }

  SECTION("Node with localization key") {
    helper.setLocalizationKey(*dialogueNode, "scene.test.dialogue.1");

    REQUIRE(helper.hasLocalizationKey(*dialogueNode));
    REQUIRE(helper.getLocalizationKey(*dialogueNode) == "scene.test.dialogue.1");
  }

  SECTION("Node with custom key override") {
    dialogueNode->setProperty(DialogueLocalizationHelper::PROP_LOCALIZATION_KEY,
                               std::string("auto.key"));
    dialogueNode->setProperty(DialogueLocalizationHelper::PROP_LOCALIZATION_KEY_CUSTOM,
                               std::string("custom.key"));
    dialogueNode->setProperty(DialogueLocalizationHelper::PROP_USE_CUSTOM_KEY, true);

    REQUIRE(helper.hasLocalizationKey(*dialogueNode));
    REQUIRE(helper.getLocalizationKey(*dialogueNode) == "custom.key");
  }
}

TEST_CASE("DialogueLocalizationHelper - Generate Keys", "[localization][dialogue]") {
  DialogueLocalizationHelper helper;
  IRGraph graph;
  graph.setName("TestScene");

  // Create dialogue nodes without keys
  NodeId d1 = graph.createNode(IRNodeType::Dialogue);
  NodeId d2 = graph.createNode(IRNodeType::Dialogue);
  NodeId d3 = graph.createNode(IRNodeType::Dialogue);

  graph.getNode(d1)->setProperty("text", std::string("Hello!"));
  graph.getNode(d2)->setProperty("text", std::string("How are you?"));
  graph.getNode(d3)->setProperty("text", std::string("Goodbye!"));

  SECTION("Generate keys for all dialogue nodes") {
    size_t keysGenerated = helper.generateLocalizationKeys(graph, "test_scene");

    REQUIRE(keysGenerated == 3);
    REQUIRE(helper.hasLocalizationKey(*graph.getNode(d1)));
    REQUIRE(helper.hasLocalizationKey(*graph.getNode(d2)));
    REQUIRE(helper.hasLocalizationKey(*graph.getNode(d3)));
  }

  SECTION("Skip nodes that already have keys") {
    helper.setLocalizationKey(*graph.getNode(d1), "existing.key");

    size_t keysGenerated = helper.generateLocalizationKeys(graph, "test_scene");

    REQUIRE(keysGenerated == 2);
    REQUIRE(helper.getLocalizationKey(*graph.getNode(d1)) == "existing.key");
  }
}

TEST_CASE("DialogueLocalizationHelper - Collect Dialogue Entries", "[localization][dialogue]") {
  DialogueLocalizationHelper helper;
  IRGraph graph;
  graph.setName("TestScene");

  // Create dialogue nodes
  NodeId d1 = graph.createNode(IRNodeType::Dialogue);
  NodeId d2 = graph.createNode(IRNodeType::Dialogue);

  graph.getNode(d1)->setProperty("text", std::string("Welcome to the game!"));
  graph.getNode(d1)->setProperty("speaker", std::string("Narrator"));

  graph.getNode(d2)->setProperty("text", std::string("Press any key to continue."));
  graph.getNode(d2)->setProperty("speaker", std::string("System"));

  // Generate keys first
  helper.generateLocalizationKeys(graph, "intro");

  SECTION("Collect all dialogue entries") {
    auto entries = helper.collectDialogueEntries(graph, "intro");

    REQUIRE(entries.size() == 2);

    // Entries should be sorted by node ID
    REQUIRE(entries[0].nodeId == d1);
    REQUIRE(entries[0].sourceText == "Welcome to the game!");
    REQUIRE(entries[0].speaker == "Narrator");
    REQUIRE(entries[0].sceneId == "intro");
    REQUIRE(entries[0].key == DialogueLocalizationData::generateKey("intro", d1));

    REQUIRE(entries[1].nodeId == d2);
    REQUIRE(entries[1].sourceText == "Press any key to continue.");
    REQUIRE(entries[1].speaker == "System");
  }
}

TEST_CASE("DialogueLocalizationHelper - Get Localizable Nodes", "[localization][dialogue]") {
  DialogueLocalizationHelper helper;
  IRGraph graph;
  graph.setName("TestScene");

  // Create various node types
  graph.createNode(IRNodeType::SceneStart);
  NodeId d1 = graph.createNode(IRNodeType::Dialogue);
  NodeId c1 = graph.createNode(IRNodeType::Choice);
  graph.createNode(IRNodeType::ShowCharacter);
  NodeId d2 = graph.createNode(IRNodeType::Dialogue);
  graph.createNode(IRNodeType::SceneEnd);

  auto localizableNodes = helper.getLocalizableNodes(graph);

  // Only dialogue and choice nodes should be returned
  REQUIRE(localizableNodes.size() == 3);
  REQUIRE(std::find(localizableNodes.begin(), localizableNodes.end(), d1) != localizableNodes.end());
  REQUIRE(std::find(localizableNodes.begin(), localizableNodes.end(), d2) != localizableNodes.end());
  REQUIRE(std::find(localizableNodes.begin(), localizableNodes.end(), c1) != localizableNodes.end());
}

TEST_CASE("DialogueLocalizationHelper - Find Missing Keys", "[localization][dialogue]") {
  DialogueLocalizationHelper helper;
  IRGraph graph;
  graph.setName("TestScene");

  NodeId d1 = graph.createNode(IRNodeType::Dialogue);
  NodeId d2 = graph.createNode(IRNodeType::Dialogue);
  NodeId d3 = graph.createNode(IRNodeType::Dialogue);

  // Only set key for d1
  helper.setLocalizationKey(*graph.getNode(d1), "scene.test.dialogue.1");

  auto missingKeys = helper.findMissingKeys(graph);

  REQUIRE(missingKeys.size() == 2);
  REQUIRE(std::find(missingKeys.begin(), missingKeys.end(), d2) != missingKeys.end());
  REQUIRE(std::find(missingKeys.begin(), missingKeys.end(), d3) != missingKeys.end());
  REQUIRE(std::find(missingKeys.begin(), missingKeys.end(), d1) == missingKeys.end());
}

TEST_CASE("DialogueLocalizationHelper - Property Constants", "[localization][dialogue]") {
  // Verify property name constants are defined correctly
  REQUIRE(std::string(DialogueLocalizationHelper::PROP_LOCALIZATION_KEY) == "localization_key");
  REQUIRE(std::string(DialogueLocalizationHelper::PROP_LOCALIZATION_KEY_CUSTOM) == "localization_key_custom");
  REQUIRE(std::string(DialogueLocalizationHelper::PROP_USE_CUSTOM_KEY) == "use_custom_localization_key");
  REQUIRE(std::string(DialogueLocalizationHelper::PROP_TRANSLATION_STATUS) == "translation_status");
}

TEST_CASE("IRNode - Dialogue node with localization properties", "[localization][dialogue][ir]") {
  IRGraph graph;

  NodeId dialogueId = graph.createNode(IRNodeType::Dialogue);
  IRNode* dialogueNode = graph.getNode(dialogueId);

  REQUIRE(dialogueNode != nullptr);
  REQUIRE(dialogueNode->getType() == IRNodeType::Dialogue);

  // Set dialogue properties
  dialogueNode->setProperty("text", std::string("Hello, world!"));
  dialogueNode->setProperty("speaker", std::string("Hero"));

  // Set localization properties
  dialogueNode->setProperty("localization_key", std::string("scene.intro.dialogue.1"));
  dialogueNode->setProperty("translation_status", static_cast<NovelMind::i64>(TranslationStatus::Translated));

  // Verify properties
  REQUIRE(dialogueNode->getStringProperty("text") == "Hello, world!");
  REQUIRE(dialogueNode->getStringProperty("speaker") == "Hero");
  REQUIRE(dialogueNode->getStringProperty("localization_key") == "scene.intro.dialogue.1");
  REQUIRE(dialogueNode->getIntProperty("translation_status") == static_cast<NovelMind::i64>(TranslationStatus::Translated));
}

TEST_CASE("Scene Node with embedded dialogue localization", "[localization][dialogue][scene]") {
  IRGraph graph;
  graph.setName("Chapter1");

  // Create a scene with embedded dialogue
  NodeId sceneStart = graph.createNode(IRNodeType::SceneStart);

  NodeId d1 = graph.createNode(IRNodeType::Dialogue);
  NodeId d2 = graph.createNode(IRNodeType::Dialogue);
  NodeId choice = graph.createNode(IRNodeType::Choice);
  NodeId d3 = graph.createNode(IRNodeType::Dialogue);

  graph.getNode(sceneStart)->setProperty("scene_id", std::string("opening"));

  graph.getNode(d1)->setProperty("text", std::string("Welcome, adventurer!"));
  graph.getNode(d1)->setProperty("speaker", std::string("Innkeeper"));

  graph.getNode(d2)->setProperty("text", std::string("What brings you here?"));
  graph.getNode(d2)->setProperty("speaker", std::string("Innkeeper"));

  graph.getNode(choice)->setProperty("options",
      std::vector<std::string>{"I'm looking for work.", "Just passing through."});

  graph.getNode(d3)->setProperty("text", std::string("I see. Well, good luck!"));
  graph.getNode(d3)->setProperty("speaker", std::string("Innkeeper"));

  // Connect nodes
  PortId startOut{sceneStart, "out", true};
  PortId d1In{d1, "in", false};
  PortId d1Out{d1, "out", true};
  PortId d2In{d2, "in", false};

  graph.connect(startOut, d1In);
  graph.connect(d1Out, d2In);

  // Generate localization keys for the scene
  DialogueLocalizationHelper helper;
  size_t keysGenerated = helper.generateLocalizationKeys(graph, "opening");

  // Should generate keys for all dialogue nodes
  REQUIRE(keysGenerated >= 3);

  // Collect entries
  auto entries = helper.collectDialogueEntries(graph, "opening");
  REQUIRE(entries.size() == 3);

  // Verify entries have correct keys
  for (const auto& entry : entries) {
    REQUIRE(entry.key.find("scene.opening.dialogue.") == 0);
    REQUIRE(!entry.sourceText.empty());
  }
}
