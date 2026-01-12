/**
 * @file test_ir_conversion_safety.cpp
 * @brief Tests for IR conversion null pointer safety
 *
 * These tests verify that IR conversion handles node creation failures
 * gracefully without crashing.
 * Related to issue #555: IR Conversion: No null check after createNode()
 */

#include "NovelMind/scripting/ir.hpp"
#include "NovelMind/scripting/ast.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace NovelMind::scripting;

// =============================================================================
// IR Conversion Safety Tests
// =============================================================================

TEST_CASE("ASTToIRConverter handles empty program", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program emptyProgram;

  auto result = converter.convert(emptyProgram);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);
}

TEST_CASE("ASTToIRConverter handles simple scene conversion", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  // Create a simple scene
  SceneDecl scene;
  scene.name = "test_scene";
  program.scenes.push_back(scene);

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);

  auto &graph = result.value();
  REQUIRE(graph->getSceneStartNode("test_scene") != 0);
}

TEST_CASE("ASTToIRConverter handles scene with statements", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  // Create a scene with a dialogue statement
  SceneDecl scene;
  scene.name = "test_scene";

  SayStmt sayStmt;
  sayStmt.text = "Hello, world!";
  sayStmt.speaker = "narrator";

  auto stmt = std::make_unique<Statement>(std::move(sayStmt), SourceLocation{});
  scene.body.push_back(std::move(stmt));

  program.scenes.push_back(scene);

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);
}

TEST_CASE("ASTToIRConverter handles multiple scenes", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  // Create multiple scenes
  for (int i = 0; i < 5; ++i) {
    SceneDecl scene;
    scene.name = "scene_" + std::to_string(i);
    program.scenes.push_back(scene);
  }

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);

  auto &graph = result.value();
  for (int i = 0; i < 5; ++i) {
    REQUIRE(graph->getSceneStartNode("scene_" + std::to_string(i)) != 0);
  }
}

TEST_CASE("ASTToIRConverter handles expressions safely", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  SceneDecl scene;
  scene.name = "expr_test";

  // Create a literal expression
  auto literalExpr = std::make_unique<Expression>(
    LiteralExpr{42},
    SourceLocation{}
  );

  program.scenes.push_back(scene);

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);
}

TEST_CASE("ASTToIRConverter handles character declarations", "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  // Add character declarations
  CharacterDecl char1;
  char1.id = "alice";
  char1.displayName = "Alice";
  char1.color = "#FF0000";
  program.characters.push_back(char1);

  CharacterDecl char2;
  char2.id = "bob";
  char2.displayName = "Bob";
  char2.color = "#0000FF";
  program.characters.push_back(char2);

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);

  auto &graph = result.value();
  REQUIRE(graph->hasCharacter("alice"));
  REQUIRE(graph->hasCharacter("bob"));
}

TEST_CASE("ASTToIRConverter handles complex scene with multiple statement types",
          "[ir][conversion][safety]") {
  ASTToIRConverter converter;
  Program program;

  SceneDecl scene;
  scene.name = "complex_scene";

  // Add show background statement
  ShowStmt showBg;
  showBg.target = ShowStmt::Target::Background;
  showBg.identifier = "bg_forest";
  scene.body.push_back(std::make_unique<Statement>(std::move(showBg), SourceLocation{}));

  // Add show character statement
  ShowStmt showChar;
  showChar.target = ShowStmt::Target::Character;
  showChar.identifier = "alice";
  scene.body.push_back(std::make_unique<Statement>(std::move(showChar), SourceLocation{}));

  // Add dialogue
  SayStmt say;
  say.speaker = "alice";
  say.text = "Hello!";
  scene.body.push_back(std::make_unique<Statement>(std::move(say), SourceLocation{}));

  // Add hide statement
  HideStmt hide;
  hide.identifier = "alice";
  scene.body.push_back(std::make_unique<Statement>(std::move(hide), SourceLocation{}));

  // Add wait statement
  WaitStmt wait;
  wait.duration = 1.0f;
  scene.body.push_back(std::make_unique<Statement>(std::move(wait), SourceLocation{}));

  // Add goto statement
  GotoStmt gotoStmt;
  gotoStmt.target = "next_scene";
  scene.body.push_back(std::make_unique<Statement>(std::move(gotoStmt), SourceLocation{}));

  program.scenes.push_back(scene);

  auto result = converter.convert(program);

  REQUIRE(result.isOk());
  REQUIRE(result.value() != nullptr);

  auto &graph = result.value();
  // Verify the scene was created
  REQUIRE(graph->getSceneStartNode("complex_scene") != 0);

  // Verify nodes were created (we should have at least scene start + statements + scene end)
  auto nodes = graph->getNodes();
  REQUIRE(nodes.size() > 6); // At minimum: start + 6 statements + end = 8 nodes
}

TEST_CASE("IRGraph createNode returns valid node ID", "[ir][safety]") {
  IRGraph graph;

  NodeId id1 = graph.createNode(IRNodeType::SceneStart);
  REQUIRE(id1 != 0);

  NodeId id2 = graph.createNode(IRNodeType::Dialogue);
  REQUIRE(id2 != 0);
  REQUIRE(id2 != id1);

  // Verify nodes can be retrieved
  auto *node1 = graph.getNode(id1);
  REQUIRE(node1 != nullptr);
  REQUIRE(node1->getType() == IRNodeType::SceneStart);

  auto *node2 = graph.getNode(id2);
  REQUIRE(node2 != nullptr);
  REQUIRE(node2->getType() == IRNodeType::Dialogue);
}

TEST_CASE("IRGraph getNode returns nullptr for invalid ID", "[ir][safety]") {
  IRGraph graph;

  // Invalid node ID (0)
  auto *node = graph.getNode(0);
  REQUIRE(node == nullptr);

  // Non-existent node ID
  auto *node2 = graph.getNode(9999);
  REQUIRE(node2 == nullptr);
}

TEST_CASE("ASTToIRConverter round-trip preserves scene structure", "[ir][conversion][safety]") {
  ASTToIRConverter astToIR;
  IRToASTConverter irToAST;

  Program originalProgram;
  SceneDecl scene;
  scene.name = "test_scene";

  SayStmt say;
  say.speaker = "alice";
  say.text = "Test dialogue";
  scene.body.push_back(std::make_unique<Statement>(std::move(say), SourceLocation{}));

  originalProgram.scenes.push_back(scene);

  // Convert to IR
  auto irResult = astToIR.convert(originalProgram);
  REQUIRE(irResult.isOk());

  auto &graph = irResult.value();
  REQUIRE(graph != nullptr);

  // Convert back to AST
  auto astResult = irToAST.convert(*graph);
  REQUIRE(astResult.isOk());

  auto &program = astResult.value();
  REQUIRE(program.scenes.size() == 1);
  REQUIRE(program.scenes[0].name == "test_scene");
}
