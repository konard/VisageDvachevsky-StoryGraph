#pragma once

/**
 * @file ast.hpp
 * @brief Abstract Syntax Tree definitions for NM Script
 *
 * This module defines the AST node types used to represent
 * parsed NM Script programs.
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/token.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace NovelMind::scripting {

// Forward declarations
struct Expression;
struct Statement;

using ExprPtr = std::unique_ptr<Expression>;
using StmtPtr = std::unique_ptr<Statement>;

/**
 * @brief Position enum for character/sprite placement
 */
enum class Position { Left, Center, Right, Custom };

/**
 * @brief Expression types in NM Script
 */

struct LiteralExpr {
  std::variant<std::monostate, i32, f32, bool, std::string> value;
};

struct IdentifierExpr {
  std::string name;
};

struct BinaryExpr {
  ExprPtr left;
  TokenType op;
  ExprPtr right;
};

struct UnaryExpr {
  TokenType op;
  ExprPtr operand;
};

struct CallExpr {
  std::string callee;
  std::vector<ExprPtr> arguments;
};

struct PropertyExpr {
  ExprPtr object;
  std::string property;
};

/**
 * @brief Expression variant type
 */
struct Expression {
  std::variant<LiteralExpr, IdentifierExpr, BinaryExpr, UnaryExpr, CallExpr,
               PropertyExpr>
      data;
  SourceLocation location;

  template <typename T>
  explicit Expression(T &&expr, SourceLocation loc = {})
      : data(std::forward<T>(expr)), location(loc) {}
};

/**
 * @brief Statement types in NM Script
 */

/**
 * @brief Character declaration: character Hero(name="Alex", color="#FFCC00")
 */
struct CharacterDecl {
  std::string id;
  std::string displayName;
  std::string color;
  std::optional<std::string> defaultSprite;
};

/**
 * @brief Scene declaration: scene intro { ... }
 */
struct SceneDecl {
  std::string name;
  std::vector<StmtPtr> body;
};

/**
 * @brief Show command: show background "bg_city" or show Hero at center
 */
struct ShowStmt {
  enum class Target { Background, Character, Sprite };

  Target target;
  std::string identifier;
  std::optional<std::string> resource;
  std::optional<Position> position;
  std::optional<f32> customX;
  std::optional<f32> customY;
  std::optional<std::string> transition;
  std::optional<f32> duration;
};

/**
 * @brief Hide command: hide Hero
 */
struct HideStmt {
  std::string identifier;
  std::optional<std::string> transition;
  std::optional<f32> duration;
};

/**
 * @brief Say command: say Hero "Hello, world!"
 */
struct SayStmt {
  std::optional<std::string> speaker;
  std::string text;
};

/**
 * @brief Choice option within a choice block
 */
struct ChoiceOption {
  std::string text;
  std::optional<ExprPtr> condition;
  std::vector<StmtPtr> body;
  std::optional<std::string> gotoTarget;
};

/**
 * @brief Choice block: choice { "Option 1" -> ... }
 */
struct ChoiceStmt {
  std::vector<ChoiceOption> options;
};

/**
 * @brief If statement: if condition { ... } else { ... }
 */
struct IfStmt {
  ExprPtr condition;
  std::vector<StmtPtr> thenBranch;
  std::vector<StmtPtr> elseBranch;
};

/**
 * @brief Goto statement: goto scene_name
 */
struct GotoStmt {
  std::string target;
};

/**
 * @brief Wait statement: wait 2.0
 */
struct WaitStmt {
  f32 duration;
};

/**
 * @brief Play statement: play sound "click.ogg" or play music "bgm.ogg"
 */
struct PlayStmt {
  enum class MediaType { Sound, Music };

  MediaType type;
  std::string resource;
  std::optional<f32> volume;
  std::optional<bool> loop;
};

/**
 * @brief Stop statement: stop music
 */
struct StopStmt {
  PlayStmt::MediaType type;
  std::optional<f32> fadeOut;
};

/**
 * @brief Set statement: set variable = value
 *        or: set flag variable = value (for boolean flags)
 */
struct SetStmt {
  std::string variable;
  ExprPtr value;
  bool isFlag = false; ///< True if this is a flag (boolean) variable
};

/**
 * @brief Transition statement: transition fade 1.0
 */
struct TransitionStmt {
  std::string type;
  f32 duration;
  std::optional<std::string> color;
};

/**
 * @brief Move command: move Character to position duration=seconds
 */
struct MoveStmt {
  std::string characterId;
  Position position;
  std::optional<f32> customX;
  std::optional<f32> customY;
  f32 duration = 0.5f; ///< Default duration in seconds
};

/**
 * @brief Expression statement (for standalone expressions)
 */
struct ExpressionStmt {
  ExprPtr expression;
};

/**
 * @brief Block statement (group of statements)
 */
struct BlockStmt {
  std::vector<StmtPtr> statements;
};

/**
 * @brief Statement variant type
 */
struct Statement {
  std::variant<CharacterDecl, SceneDecl, ShowStmt, HideStmt, SayStmt,
               ChoiceStmt, IfStmt, GotoStmt, WaitStmt, PlayStmt, StopStmt,
               SetStmt, TransitionStmt, MoveStmt, ExpressionStmt, BlockStmt>
      data;
  SourceLocation location;

  template <typename T>
  explicit Statement(T &&stmt, SourceLocation loc = {})
      : data(std::forward<T>(stmt)), location(loc) {}
};

/**
 * @brief Root AST node representing a complete NM Script program
 */
struct Program {
  std::vector<CharacterDecl> characters;
  std::vector<SceneDecl> scenes;
  std::vector<StmtPtr> globalStatements;
};

/**
 * @brief Helper to create expressions
 */
template <typename T> ExprPtr makeExpr(T &&expr, SourceLocation loc = {}) {
  return std::make_unique<Expression>(std::forward<T>(expr), loc);
}

/**
 * @brief Helper to create statements
 */
template <typename T> StmtPtr makeStmt(T &&stmt, SourceLocation loc = {}) {
  return std::make_unique<Statement>(std::forward<T>(stmt), loc);
}

} // namespace NovelMind::scripting
