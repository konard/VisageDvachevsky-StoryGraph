/**
 * @file test_voice_integration.cpp
 * @brief Tests for voice-over integration in dialogue nodes
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/ir.hpp"
#include <string>

using namespace NovelMind::scripting;

TEST_CASE("VoiceClipData - Default Construction", "[voice][integration]") {
  VoiceClipData voiceData;

  REQUIRE(voiceData.voiceFilePath.empty());
  REQUIRE(voiceData.localizationKey.empty());
  REQUIRE(voiceData.bindingStatus == VoiceBindingStatus::Unbound);
  REQUIRE(voiceData.voiceDuration == 0.0f);
  REQUIRE(voiceData.autoDetected == false);
}

TEST_CASE("VoiceClipData - Initialization with values", "[voice][integration]") {
  VoiceClipData voiceData;
  voiceData.voiceFilePath = "voice/hero/line_001.ogg";
  voiceData.localizationKey = "hero_intro_001";
  voiceData.bindingStatus = VoiceBindingStatus::Bound;
  voiceData.voiceDuration = 2.5f;
  voiceData.autoDetected = false;

  REQUIRE(voiceData.voiceFilePath == "voice/hero/line_001.ogg");
  REQUIRE(voiceData.localizationKey == "hero_intro_001");
  REQUIRE(voiceData.bindingStatus == VoiceBindingStatus::Bound);
  REQUIRE(voiceData.voiceDuration == 2.5f);
  REQUIRE(voiceData.autoDetected == false);
}

TEST_CASE("VoiceClipData - Auto-detected voice", "[voice][integration]") {
  VoiceClipData voiceData;
  voiceData.voiceFilePath = "voice/alice/scene01_001.ogg";
  voiceData.localizationKey = "scene01_alice_001";
  voiceData.bindingStatus = VoiceBindingStatus::AutoMapped;
  voiceData.autoDetected = true;

  REQUIRE(voiceData.autoDetected == true);
  REQUIRE(voiceData.bindingStatus == VoiceBindingStatus::AutoMapped);
}

TEST_CASE("VoiceBindingStatus - Status values", "[voice][integration]") {
  REQUIRE(static_cast<int>(VoiceBindingStatus::Unbound) == 0);
  REQUIRE(static_cast<int>(VoiceBindingStatus::Bound) == 1);
  REQUIRE(static_cast<int>(VoiceBindingStatus::MissingFile) == 2);
  REQUIRE(static_cast<int>(VoiceBindingStatus::AutoMapped) == 3);
  REQUIRE(static_cast<int>(VoiceBindingStatus::Pending) == 4);
}

TEST_CASE("IRNode - Dialogue node with voice properties", "[voice][integration][ir]") {
  IRGraph graph;

  // Create a dialogue node
  NodeId dialogueId = graph.createNode(IRNodeType::Dialogue);
  IRNode *dialogueNode = graph.getNode(dialogueId);

  REQUIRE(dialogueNode != nullptr);
  REQUIRE(dialogueNode->getType() == IRNodeType::Dialogue);

  // Set dialogue text
  dialogueNode->setProperty("text", std::string("Hello, world!"));
  dialogueNode->setProperty("speaker", std::string("Hero"));

  // Set voice properties (stored as separate properties)
  dialogueNode->setProperty("voice_file", std::string("voice/hero/hello.ogg"));
  dialogueNode->setProperty("voice_localization_key", std::string("hero_hello_001"));
  dialogueNode->setProperty("voice_duration", 1.5);

  // Verify properties
  REQUIRE(dialogueNode->getStringProperty("text") == "Hello, world!");
  REQUIRE(dialogueNode->getStringProperty("speaker") == "Hero");
  REQUIRE(dialogueNode->getStringProperty("voice_file") == "voice/hero/hello.ogg");
  REQUIRE(dialogueNode->getStringProperty("voice_localization_key") == "hero_hello_001");
  REQUIRE(dialogueNode->getFloatProperty("voice_duration") == 1.5);
}

TEST_CASE("IRGraph - Multiple dialogue nodes with voice", "[voice][integration][ir]") {
  IRGraph graph;
  graph.setName("TestScene");

  // Create scene start
  NodeId startId = graph.createNode(IRNodeType::SceneStart);

  // Create first dialogue with voice
  NodeId dialogue1 = graph.createNode(IRNodeType::Dialogue);
  IRNode *node1 = graph.getNode(dialogue1);
  node1->setProperty("text", std::string("Welcome!"));
  node1->setProperty("speaker", std::string("Alice"));
  node1->setProperty("voice_file", std::string("voice/alice/welcome.ogg"));

  // Create second dialogue with voice
  NodeId dialogue2 = graph.createNode(IRNodeType::Dialogue);
  IRNode *node2 = graph.getNode(dialogue2);
  node2->setProperty("text", std::string("Thank you!"));
  node2->setProperty("speaker", std::string("Bob"));
  node2->setProperty("voice_file", std::string("voice/bob/thanks.ogg"));

  // Connect nodes
  PortId startOutput{startId, "out", true};
  PortId dialogue1Input{dialogue1, "in", false};
  PortId dialogue1Output{dialogue1, "out", true};
  PortId dialogue2Input{dialogue2, "in", false};

  graph.connect(startOutput, dialogue1Input);
  graph.connect(dialogue1Output, dialogue2Input);

  // Verify connections
  auto connections = graph.getConnections();
  REQUIRE(connections.size() == 2);

  // Verify dialogue nodes have voice files
  REQUIRE(node1->getStringProperty("voice_file") == "voice/alice/welcome.ogg");
  REQUIRE(node2->getStringProperty("voice_file") == "voice/bob/thanks.ogg");
}
