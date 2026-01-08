#pragma once

/**
 * @file compiler.hpp
 * @brief Bytecode compiler for NM Script
 *
 * This module provides the Compiler class which transforms an AST
 * into bytecode that can be executed by the VM.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ast.hpp"
#include "NovelMind/scripting/opcode.hpp"
#include "NovelMind/scripting/value.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::scripting {

/**
 * @brief Compiled bytecode representation
 */
struct CompiledScript {
  std::vector<Instruction> instructions;
  std::vector<std::string> stringTable;

  // Scene entry points: scene name -> instruction index
  std::unordered_map<std::string, u32> sceneEntryPoints;

  // Character definitions
  std::unordered_map<std::string, CharacterDecl> characters;

  // Variable declarations (for type checking)
  std::unordered_map<std::string, ValueType> variables;
};

/**
 * @brief Compiler error information
 */
struct CompileError {
  std::string message;
  SourceLocation location;

  CompileError() = default;
  CompileError(std::string msg, SourceLocation loc)
      : message(std::move(msg)), location(loc) {}
};

/**
 * @brief Compiles NM Script AST into bytecode
 *
 * The Compiler traverses the AST and emits bytecode instructions
 * that can be executed by the VirtualMachine.
 *
 * Example usage:
 * @code
 * Compiler compiler;
 * auto result = compiler.compile(program);
 * if (result.isOk()) {
 *     CompiledScript script = result.value();
 *     vm.load(script.instructions, script.stringTable);
 * }
 * @endcode
 */
class Compiler {
public:
  Compiler();
  ~Compiler();

  /**
   * @brief Compile an AST program to bytecode
   * @param program The parsed program AST
   * @return Result containing compiled script or error
   */
  [[nodiscard]] Result<CompiledScript> compile(const Program &program);

  /**
   * @brief Get all errors encountered during compilation
   */
  [[nodiscard]] const std::vector<CompileError> &getErrors() const;

private:
  // Compilation helpers
  void reset();
  void emitOp(OpCode op, u32 operand = 0);
  u32 emitJump(OpCode op);
  /**
   * @brief Patch a jump instruction with the current program counter
   * @param jumpIndex Index of the jump instruction to patch
   * @return true if successful, false if jumpIndex is out of bounds
   * @note This function validates bounds to prevent buffer overflow
   */
  bool patchJump(u32 jumpIndex);
  u32 addString(const std::string &str);

  // Error handling
  void error(const std::string &message, SourceLocation loc = {});

  // Visitors
  void compileProgram(const Program &program);
  void compileCharacter(const CharacterDecl &decl);
  void compileScene(const SceneDecl &decl);
  void compileStatement(const Statement &stmt);
  void compileExpression(const Expression &expr);

  // Statement compilers
  void compileShowStmt(const ShowStmt &stmt);
  void compileHideStmt(const HideStmt &stmt);
  void compileSayStmt(const SayStmt &stmt);
  void compileChoiceStmt(const ChoiceStmt &stmt);
  void compileIfStmt(const IfStmt &stmt);
  void compileGotoStmt(const GotoStmt &stmt);
  void compileWaitStmt(const WaitStmt &stmt);
  void compilePlayStmt(const PlayStmt &stmt);
  void compileStopStmt(const StopStmt &stmt);
  void compileSetStmt(const SetStmt &stmt);
  void compileTransitionStmt(const TransitionStmt &stmt);
  void compileMoveStmt(const MoveStmt &stmt);
  void compileBlockStmt(const BlockStmt &stmt);
  void compileExpressionStmt(const ExpressionStmt &stmt);

  // Expression compilers
  void compileLiteral(const LiteralExpr &expr);
  void compileIdentifier(const IdentifierExpr &expr);
  void compileBinary(const BinaryExpr &expr);
  void compileUnary(const UnaryExpr &expr);
  void compileCall(const CallExpr &expr);
  void compileProperty(const PropertyExpr &expr);

  CompiledScript m_output;
  std::vector<CompileError> m_errors;

  // For resolving forward references
  struct PendingJump {
    u32 instructionIndex;
    std::string targetLabel;
  };
  std::vector<PendingJump> m_pendingJumps;
  std::unordered_map<std::string, u32> m_labels;

  // Current compilation context
  std::string m_currentScene;
};

} // namespace NovelMind::scripting
