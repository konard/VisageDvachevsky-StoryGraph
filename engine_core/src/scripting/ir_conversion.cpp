/**
 * @file ir_conversion.cpp
 * @brief IR <-> AST conversion utilities
 */

#include "NovelMind/scripting/ir.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace NovelMind::scripting {

// ============================================================================
// ASTToIRConverter Implementation
// ============================================================================

ASTToIRConverter::ASTToIRConverter() = default;
ASTToIRConverter::~ASTToIRConverter() = default;

Result<std::unique_ptr<IRGraph>>
ASTToIRConverter::convert(const Program &program) {
  m_graph = std::make_unique<IRGraph>();
  m_currentY = 0.0f;

  // Convert character declarations
  for (const auto &decl : program.characters) {
    convertCharacterDecl(decl);
  }

  // Convert scenes
  for (const auto &scene : program.scenes) {
    convertScene(scene);
    m_currentY += 200.0f;
  }

  return Result<std::unique_ptr<IRGraph>>::ok(std::move(m_graph));
}

void ASTToIRConverter::convertCharacterDecl(const CharacterDecl &decl) {
  m_graph->addCharacter(decl.id, decl.displayName, decl.color);
}

NodeId ASTToIRConverter::convertScene(const SceneDecl &scene) {
  NodeId startId = m_graph->createNode(IRNodeType::SceneStart);
  auto *startNode = m_graph->getNode(startId);
  startNode->setProperty("sceneName", scene.name);
  startNode->setPosition(100.0f, m_currentY);

  m_graph->addScene(scene.name, startId);

  NodeId prevNode = startId;
  f32 y = m_currentY + m_nodeSpacing;

  for (const auto &stmt : scene.body) {
    prevNode = convertStatement(*stmt, prevNode);
    y += m_nodeSpacing;
  }

  NodeId endId = m_graph->createNode(IRNodeType::SceneEnd);
  auto *endNode = m_graph->getNode(endId);
  endNode->setPosition(100.0f, y);

  PortId outPort{prevNode, "exec_out", true};
  PortId inPort{endId, "exec_in", false};
  m_graph->connect(outPort, inPort);

  return startId;
}

NodeId ASTToIRConverter::convertStatement(const Statement &stmt,
                                          NodeId prevNode) {
  // Use std::visit to handle the variant
  return std::visit(
      [this, prevNode, &stmt](const auto &stmtData) -> NodeId {
        using T = std::decay_t<decltype(stmtData)>;

        if constexpr (std::is_same_v<T, ShowStmt>) {
          IRNodeType nodeType =
              (stmtData.target == ShowStmt::Target::Background)
                  ? IRNodeType::ShowBackground
                  : IRNodeType::ShowCharacter;

          NodeId nodeId = createNodeAndConnect(nodeType, prevNode);
          auto *node = m_graph->getNode(nodeId);

          if (stmtData.target == ShowStmt::Target::Background) {
            node->setProperty("background", stmtData.identifier);
          } else {
            node->setProperty("character", stmtData.identifier);
          }
          node->setSourceLocation(stmt.location);

          return nodeId;
        } else if constexpr (std::is_same_v<T, HideStmt>) {
          NodeId nodeId =
              createNodeAndConnect(IRNodeType::HideCharacter, prevNode);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("character", stmtData.identifier);
          node->setSourceLocation(stmt.location);
          return nodeId;
        } else if constexpr (std::is_same_v<T, SayStmt>) {
          NodeId nodeId = createNodeAndConnect(IRNodeType::Dialogue, prevNode);
          auto *node = m_graph->getNode(nodeId);
          if (stmtData.speaker) {
            node->setProperty("character", *stmtData.speaker);
          }
          node->setProperty("text", stmtData.text);
          node->setSourceLocation(stmt.location);
          return nodeId;
        } else if constexpr (std::is_same_v<T, ChoiceStmt>) {
          NodeId choiceId = createNodeAndConnect(IRNodeType::Choice, prevNode);
          auto *choiceNode = m_graph->getNode(choiceId);
          choiceNode->setSourceLocation(stmt.location);

          std::vector<std::string> optionTexts;
          for (const auto &opt : stmtData.options) {
            optionTexts.push_back(opt.text);
          }
          choiceNode->setProperty("options", optionTexts);

          return choiceId;
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          NodeId branchId = createNodeAndConnect(IRNodeType::Branch, prevNode);
          auto *branchNode = m_graph->getNode(branchId);
          branchNode->setSourceLocation(stmt.location);
          return branchId;
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          NodeId gotoId = createNodeAndConnect(IRNodeType::Goto, prevNode);
          auto *gotoNode = m_graph->getNode(gotoId);
          gotoNode->setProperty("target", stmtData.target);
          gotoNode->setSourceLocation(stmt.location);
          return gotoId;
        } else if constexpr (std::is_same_v<T, PlayStmt>) {
          IRNodeType nodeType = (stmtData.type == PlayStmt::MediaType::Music)
                                    ? IRNodeType::PlayMusic
                                    : IRNodeType::PlaySound;
          NodeId nodeId = createNodeAndConnect(nodeType, prevNode);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("track", stmtData.resource);
          if (stmtData.loop && *stmtData.loop) {
            node->setProperty("loop", true);
          }
          node->setSourceLocation(stmt.location);
          return nodeId;
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          NodeId nodeId = createNodeAndConnect(IRNodeType::StopMusic, prevNode);
          m_graph->getNode(nodeId)->setSourceLocation(stmt.location);
          return nodeId;
        } else if constexpr (std::is_same_v<T, WaitStmt>) {
          NodeId nodeId = createNodeAndConnect(IRNodeType::Wait, prevNode);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("duration", static_cast<f64>(stmtData.duration));
          node->setSourceLocation(stmt.location);
          return nodeId;
        } else if constexpr (std::is_same_v<T, TransitionStmt>) {
          NodeId nodeId =
              createNodeAndConnect(IRNodeType::Transition, prevNode);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("type", stmtData.type);
          node->setProperty("duration", static_cast<f64>(stmtData.duration));
          node->setSourceLocation(stmt.location);
          return nodeId;
        } else {
          // For other statement types, return prevNode unchanged
          return prevNode;
        }
      },
      stmt.data);
}

NodeId ASTToIRConverter::convertExpression(const Expression &expr) {
  return std::visit(
      [this, &expr](const auto &exprData) -> NodeId {
        using T = std::decay_t<decltype(exprData)>;

        if constexpr (std::is_same_v<T, LiteralExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "literal");

          // Store the literal value based on its type
          std::visit(
              [node](const auto &val) {
                using V = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<V, std::monostate>) {
                  node->setProperty("valueType", "null");
                } else if constexpr (std::is_same_v<V, i32>) {
                  node->setProperty("valueType", "int");
                  node->setProperty("value", static_cast<i64>(val));
                } else if constexpr (std::is_same_v<V, f32>) {
                  node->setProperty("valueType", "float");
                  node->setProperty("value", static_cast<f64>(val));
                } else if constexpr (std::is_same_v<V, bool>) {
                  node->setProperty("valueType", "bool");
                  node->setProperty("value", val);
                } else if constexpr (std::is_same_v<V, std::string>) {
                  node->setProperty("valueType", "string");
                  node->setProperty("value", val);
                }
              },
              exprData.value);

          node->setSourceLocation(expr.location);
          return nodeId;

        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "identifier");
          node->setProperty("name", exprData.name);
          node->setSourceLocation(expr.location);
          return nodeId;

        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "binary");

          // Convert operator TokenType to string
          std::string opStr;
          switch (exprData.op) {
            case TokenType::Plus: opStr = "+"; break;
            case TokenType::Minus: opStr = "-"; break;
            case TokenType::Star: opStr = "*"; break;
            case TokenType::Slash: opStr = "/"; break;
            case TokenType::Percent: opStr = "%"; break;
            case TokenType::Equal: opStr = "=="; break;
            case TokenType::NotEqual: opStr = "!="; break;
            case TokenType::Less: opStr = "<"; break;
            case TokenType::LessEqual: opStr = "<="; break;
            case TokenType::Greater: opStr = ">"; break;
            case TokenType::GreaterEqual: opStr = ">="; break;
            case TokenType::And: opStr = "and"; break;
            case TokenType::Or: opStr = "or"; break;
            default: opStr = "unknown"; break;
          }
          node->setProperty("operator", opStr);

          // Recursively convert operands
          if (exprData.left) {
            NodeId leftId = convertExpression(*exprData.left);
            node->setProperty("leftOperand", static_cast<i64>(leftId));
          }
          if (exprData.right) {
            NodeId rightId = convertExpression(*exprData.right);
            node->setProperty("rightOperand", static_cast<i64>(rightId));
          }

          node->setSourceLocation(expr.location);
          return nodeId;

        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "unary");

          // Convert operator TokenType to string
          std::string opStr;
          switch (exprData.op) {
            case TokenType::Minus: opStr = "-"; break;
            case TokenType::Not: opStr = "not"; break;
            default: opStr = "unknown"; break;
          }
          node->setProperty("operator", opStr);

          // Recursively convert operand
          if (exprData.operand) {
            NodeId operandId = convertExpression(*exprData.operand);
            node->setProperty("operand", static_cast<i64>(operandId));
          }

          node->setSourceLocation(expr.location);
          return nodeId;

        } else if constexpr (std::is_same_v<T, CallExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "call");
          node->setProperty("callee", exprData.callee);

          // Convert arguments
          std::vector<std::string> argIds;
          for (const auto &arg : exprData.arguments) {
            if (arg) {
              NodeId argId = convertExpression(*arg);
              argIds.push_back(std::to_string(argId));
            }
          }
          node->setProperty("arguments", argIds);

          node->setSourceLocation(expr.location);
          return nodeId;

        } else if constexpr (std::is_same_v<T, PropertyExpr>) {
          NodeId nodeId = m_graph->createNode(IRNodeType::Expression);
          auto *node = m_graph->getNode(nodeId);
          node->setProperty("exprType", "property");
          node->setProperty("property", exprData.property);

          // Recursively convert object
          if (exprData.object) {
            NodeId objectId = convertExpression(*exprData.object);
            node->setProperty("object", static_cast<i64>(objectId));
          }

          node->setSourceLocation(expr.location);
          return nodeId;
        }

        // Fallback - should never reach here
        return 0;
      },
      expr.data);
}

NodeId ASTToIRConverter::createNodeAndConnect(IRNodeType type,
                                              NodeId prevNode) {
  NodeId newId = m_graph->createNode(type);
  auto *newNode = m_graph->getNode(newId);

  auto *prev = m_graph->getNode(prevNode);
  if (prev) {
    newNode->setPosition(prev->getX(), prev->getY() + m_nodeSpacing);
  }

  PortId outPort{prevNode, "exec_out", true};
  PortId inPort{newId, "exec_in", false};
  m_graph->connect(outPort, inPort);

  return newId;
}

// ============================================================================
// IRToASTConverter Implementation
// ============================================================================

IRToASTConverter::IRToASTConverter() = default;
IRToASTConverter::~IRToASTConverter() = default;

Result<Program> IRToASTConverter::convert(const IRGraph &graph) {
  Program program;
  m_visited.clear();

  for (const auto &sceneName : graph.getSceneNames()) {
    NodeId startId = graph.getSceneStartNode(sceneName);
    if (startId == 0) {
      continue;
    }

    SceneDecl scene;
    scene.name = sceneName;

    auto execOrder = graph.getExecutionOrder();
    for (NodeId id : execOrder) {
      const auto *node = graph.getNode(id);
      if (!node || m_visited.count(id)) {
        continue;
      }

      auto stmt = convertNode(node, graph);
      if (stmt) {
        scene.body.push_back(std::move(stmt));
      }
    }

    program.scenes.push_back(std::move(scene));
  }

  return Result<Program>::ok(std::move(program));
}

std::unique_ptr<Statement>
IRToASTConverter::convertNode(const IRNode *node, const IRGraph & /*graph*/) {
  m_visited.insert(node->getId());

  switch (node->getType()) {
  case IRNodeType::ShowCharacter: {
    ShowStmt show;
    show.target = ShowStmt::Target::Character;
    show.identifier = node->getStringProperty("character");
    return std::make_unique<Statement>(std::move(show),
                                       node->getSourceLocation());
  }

  case IRNodeType::ShowBackground: {
    ShowStmt show;
    show.target = ShowStmt::Target::Background;
    show.identifier = node->getStringProperty("background");
    return std::make_unique<Statement>(std::move(show),
                                       node->getSourceLocation());
  }

  case IRNodeType::HideCharacter: {
    HideStmt hide;
    hide.identifier = node->getStringProperty("character");
    return std::make_unique<Statement>(std::move(hide),
                                       node->getSourceLocation());
  }

  case IRNodeType::Dialogue: {
    SayStmt say;
    std::string character = node->getStringProperty("character");
    if (!character.empty()) {
      say.speaker = character;
    }
    say.text = node->getStringProperty("text");
    return std::make_unique<Statement>(std::move(say),
                                       node->getSourceLocation());
  }

  case IRNodeType::PlayMusic: {
    PlayStmt play;
    play.type = PlayStmt::MediaType::Music;
    play.resource = node->getStringProperty("track");
    play.loop = node->getBoolProperty("loop", false);
    return std::make_unique<Statement>(std::move(play),
                                       node->getSourceLocation());
  }

  case IRNodeType::PlaySound: {
    PlayStmt play;
    play.type = PlayStmt::MediaType::Sound;
    play.resource = node->getStringProperty("track");
    return std::make_unique<Statement>(std::move(play),
                                       node->getSourceLocation());
  }

  case IRNodeType::Wait: {
    WaitStmt wait;
    wait.duration = static_cast<f32>(node->getFloatProperty("duration", 1.0));
    return std::make_unique<Statement>(std::move(wait),
                                       node->getSourceLocation());
  }

  case IRNodeType::Goto: {
    GotoStmt gotoStmt;
    gotoStmt.target = node->getStringProperty("target");
    return std::make_unique<Statement>(std::move(gotoStmt),
                                       node->getSourceLocation());
  }

  default:
    return nullptr;
  }
}

std::unique_ptr<Expression>
IRToASTConverter::convertToExpression(const IRNode *node,
                                      const IRGraph &graph) {
  if (!node || node->getType() != IRNodeType::Expression) {
    return nullptr;
  }

  std::string exprType = node->getStringProperty("exprType");

  if (exprType == "literal") {
    LiteralExpr literal;
    std::string valueType = node->getStringProperty("valueType");

    if (valueType == "null") {
      literal.value = std::monostate{};
    } else if (valueType == "int") {
      literal.value = static_cast<i32>(node->getIntProperty("value"));
    } else if (valueType == "float") {
      literal.value = static_cast<f32>(node->getFloatProperty("value"));
    } else if (valueType == "bool") {
      literal.value = node->getBoolProperty("value");
    } else if (valueType == "string") {
      literal.value = node->getStringProperty("value");
    }

    return std::make_unique<Expression>(std::move(literal),
                                        node->getSourceLocation());

  } else if (exprType == "identifier") {
    IdentifierExpr identifier;
    identifier.name = node->getStringProperty("name");
    return std::make_unique<Expression>(std::move(identifier),
                                        node->getSourceLocation());

  } else if (exprType == "binary") {
    BinaryExpr binary;

    // Convert operator string back to TokenType
    std::string opStr = node->getStringProperty("operator");
    if (opStr == "+") binary.op = TokenType::Plus;
    else if (opStr == "-") binary.op = TokenType::Minus;
    else if (opStr == "*") binary.op = TokenType::Star;
    else if (opStr == "/") binary.op = TokenType::Slash;
    else if (opStr == "%") binary.op = TokenType::Percent;
    else if (opStr == "==") binary.op = TokenType::Equal;
    else if (opStr == "!=") binary.op = TokenType::NotEqual;
    else if (opStr == "<") binary.op = TokenType::Less;
    else if (opStr == "<=") binary.op = TokenType::LessEqual;
    else if (opStr == ">") binary.op = TokenType::Greater;
    else if (opStr == ">=") binary.op = TokenType::GreaterEqual;
    else if (opStr == "and") binary.op = TokenType::And;
    else if (opStr == "or") binary.op = TokenType::Or;
    else binary.op = TokenType::Error; // Fallback

    // Recursively convert operands
    i64 leftId = node->getIntProperty("leftOperand", 0);
    if (leftId != 0) {
      const IRNode *leftNode = graph.getNode(static_cast<NodeId>(leftId));
      binary.left = convertToExpression(leftNode, graph);
    }

    i64 rightId = node->getIntProperty("rightOperand", 0);
    if (rightId != 0) {
      const IRNode *rightNode = graph.getNode(static_cast<NodeId>(rightId));
      binary.right = convertToExpression(rightNode, graph);
    }

    return std::make_unique<Expression>(std::move(binary),
                                        node->getSourceLocation());

  } else if (exprType == "unary") {
    UnaryExpr unary;

    // Convert operator string back to TokenType
    std::string opStr = node->getStringProperty("operator");
    if (opStr == "-") unary.op = TokenType::Minus;
    else if (opStr == "not") unary.op = TokenType::Not;
    else unary.op = TokenType::Error; // Fallback

    // Recursively convert operand
    i64 operandId = node->getIntProperty("operand", 0);
    if (operandId != 0) {
      const IRNode *operandNode = graph.getNode(static_cast<NodeId>(operandId));
      unary.operand = convertToExpression(operandNode, graph);
    }

    return std::make_unique<Expression>(std::move(unary),
                                        node->getSourceLocation());

  } else if (exprType == "call") {
    CallExpr call;
    call.callee = node->getStringProperty("callee");

    // Convert arguments
    auto prop = node->getProperty("arguments");
    if (prop && std::holds_alternative<std::vector<std::string>>(*prop)) {
      const auto &argIds = std::get<std::vector<std::string>>(*prop);
      for (const auto &argIdStr : argIds) {
        try {
          NodeId argId = std::stoull(argIdStr);
          const IRNode *argNode = graph.getNode(argId);
          auto argExpr = convertToExpression(argNode, graph);
          if (argExpr) {
            call.arguments.push_back(std::move(argExpr));
          }
        } catch (...) {
          // Skip invalid argument IDs
        }
      }
    }

    return std::make_unique<Expression>(std::move(call),
                                        node->getSourceLocation());

  } else if (exprType == "property") {
    PropertyExpr property;
    property.property = node->getStringProperty("property");

    // Recursively convert object
    i64 objectId = node->getIntProperty("object", 0);
    if (objectId != 0) {
      const IRNode *objectNode = graph.getNode(static_cast<NodeId>(objectId));
      property.object = convertToExpression(objectNode, graph);
    }

    return std::make_unique<Expression>(std::move(property),
                                        node->getSourceLocation());
  }

  // Unknown expression type
  return nullptr;
}

// ============================================================================
// ASTToTextGenerator Implementation
// ============================================================================

ASTToTextGenerator::ASTToTextGenerator() = default;
ASTToTextGenerator::~ASTToTextGenerator() = default;

std::string ASTToTextGenerator::generate(const Program &program) {
  m_output.clear();
  m_indentLevel = 0;

  for (const auto &decl : program.characters) {
    generateCharacter(decl);
    newline();
  }

  if (!program.characters.empty()) {
    newline();
  }

  for (const auto &scene : program.scenes) {
    generateScene(scene);
    newline();
  }

  return m_output;
}

void ASTToTextGenerator::generateCharacter(const CharacterDecl &decl) {
  write("character ");
  write(decl.id);
  write("(name=\"");
  write(decl.displayName);
  write("\"");
  if (!decl.color.empty()) {
    write(", color=\"");
    write(decl.color);
    write("\"");
  }
  write(")");
}

void ASTToTextGenerator::generateScene(const SceneDecl &scene) {
  write("scene ");
  write(scene.name);
  write(" {");
  newline();

  m_indentLevel++;
  for (const auto &stmt : scene.body) {
    generateStatement(*stmt, m_indentLevel);
  }
  m_indentLevel--;

  indent();
  write("}");
}

void ASTToTextGenerator::generateStatement(const Statement &stmt,
                                           int /*indentLvl*/) {
  indent();

  std::visit(
      [this](const auto &stmtData) {
        using T = std::decay_t<decltype(stmtData)>;

        if constexpr (std::is_same_v<T, ShowStmt>) {
          if (stmtData.target == ShowStmt::Target::Background) {
            write("show background \"");
            if (stmtData.resource) {
              write(*stmtData.resource);
            } else {
              write(stmtData.identifier);
            }
            write("\"");
          } else {
            write("show ");
            write(stmtData.identifier);
          }
        } else if constexpr (std::is_same_v<T, HideStmt>) {
          write("hide ");
          write(stmtData.identifier);
        } else if constexpr (std::is_same_v<T, SayStmt>) {
          if (stmtData.speaker) {
            write("say ");
            write(*stmtData.speaker);
            write(" \"");
          } else {
            write("say \"");
          }
          write(stmtData.text);
          write("\"");
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          write("goto ");
          write(stmtData.target);
        } else if constexpr (std::is_same_v<T, PlayStmt>) {
          if (stmtData.type == PlayStmt::MediaType::Music) {
            write("play music \"");
          } else {
            write("play sound \"");
          }
          write(stmtData.resource);
          write("\"");
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          write("stop music");
        } else if constexpr (std::is_same_v<T, WaitStmt>) {
          write("wait ");
          write(std::to_string(stmtData.duration));
        } else if constexpr (std::is_same_v<T, TransitionStmt>) {
          write("transition ");
          write(stmtData.type);
          write(" ");
          write(std::to_string(stmtData.duration));
        }
      },
      stmt.data);

  newline();
}

void ASTToTextGenerator::generateExpression(const Expression &expr) {
  std::visit(
      [this](const auto &exprData) {
        using T = std::decay_t<decltype(exprData)>;

        if constexpr (std::is_same_v<T, LiteralExpr>) {
          std::visit(
              [this](const auto &val) {
                using V = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<V, std::string>) {
                  write("\"");
                  write(val);
                  write("\"");
                } else if constexpr (std::is_same_v<V, i32>) {
                  write(std::to_string(val));
                } else if constexpr (std::is_same_v<V, f32>) {
                  write(std::to_string(val));
                } else if constexpr (std::is_same_v<V, bool>) {
                  write(val ? "true" : "false");
                }
              },
              exprData.value);
        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          write(exprData.name);
        }
      },
      expr.data);
}

void ASTToTextGenerator::indent() {
  for (int i = 0; i < m_indentLevel; ++i) {
    m_output += "    ";
  }
}

void ASTToTextGenerator::newline() { m_output += "\n"; }

void ASTToTextGenerator::write(const std::string &text) { m_output += text; }

} // namespace NovelMind::scripting
