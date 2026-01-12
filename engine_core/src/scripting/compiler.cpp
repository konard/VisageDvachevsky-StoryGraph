#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/vm_debugger.hpp"
#include "NovelMind/core/endian.hpp"
#include <bit>

namespace NovelMind::scripting {

// Float serialization assumptions:
// - f32 (float) is exactly 4 bytes (32 bits), matching u32
// - IEEE 754 single-precision format is used (ubiquitous on modern platforms)
// - std::bit_cast provides well-defined, portable conversion without UB
// - All floats are serialized in little-endian byte order for cross-platform compatibility
static_assert(sizeof(f32) == sizeof(u32), "Float size must match u32 for bit_cast serialization");
static_assert(std::numeric_limits<f32>::is_iec559, "IEEE 754 (IEC 559) float format required");

Compiler::Compiler() = default;
Compiler::~Compiler() = default;

Result<CompiledScript> Compiler::compile(const Program &program, const std::string &sourceFilePath) {
  reset();
  m_sourceFilePath = sourceFilePath;

  try {
    compileProgram(program);
  } catch (...) {
    if (m_errors.empty() && m_scriptErrors.isEmpty()) {
      error(ErrorCode::CompilationFailed,
            "An internal compiler error occurred. This is likely a bug in the compiler. "
            "Please report this issue with the script that caused the error.");
    }
  }

  // Resolve pending jumps
  for (const auto &pending : m_pendingJumps) {
    // Bounds check to prevent buffer overflow (security vulnerability)
    if (pending.instructionIndex >= m_output.instructions.size()) {
      error(ErrorCode::CompilationFailed,
            "Internal compiler error: Invalid jump instruction index " +
            std::to_string(pending.instructionIndex) + " (program has " +
            std::to_string(m_output.instructions.size()) + " instructions). " +
            "This indicates a compiler bug. Please report this issue.");
      continue;
    }

    auto it = m_labels.find(pending.targetLabel);
    if (it != m_labels.end()) {
      m_output.instructions[pending.instructionIndex].operand = it->second;
    } else {
      ScriptError err(ErrorCode::InvalidGotoTarget, Severity::Error,
                      "Label '" + pending.targetLabel + "' is not defined in this script",
                      SourceLocation{});
      err.message += "\n\nWhat went wrong: The script tries to jump to a label that doesn't exist.";
      err.message += "\n\nHow to fix: Define the label using 'label " + pending.targetLabel +
                     "' or check for typos in the label name.";

      // Try to find similar labels
      std::vector<std::string> labelNames;
      for (const auto &[name, _] : m_labels) {
        labelNames.push_back(name);
      }
      auto similar = findSimilarStrings(pending.targetLabel, labelNames);
      if (!similar.empty()) {
        for (const auto &suggestion : similar) {
          err.suggestions.push_back(suggestion);
        }
      }

      addError(std::move(err));
    }
  }

  if (!m_errors.empty()) {
    return Result<CompiledScript>::error(m_errors[0].message);
  }

  return Result<CompiledScript>::ok(std::move(m_output));
}

const std::vector<CompileError> &Compiler::getErrors() const {
  return m_errors;
}

const ScriptErrorList &Compiler::getScriptErrors() const {
  return m_scriptErrors;
}

void Compiler::setSource(const std::string &source) {
  m_source = source;
}

void Compiler::reset() {
  m_output = CompiledScript{};
  m_errors.clear();
  m_scriptErrors.clear();
  m_pendingJumps.clear();
  m_labels.clear();
  m_currentScene.clear();
  m_sourceFilePath.clear();
  m_source.clear();
}

void Compiler::emitOp(OpCode op, u32 operand) {
  m_output.instructions.emplace_back(op, operand);
}

void Compiler::emitOp(OpCode op, u32 operand, const SourceLocation &loc) {
  u32 ip = static_cast<u32>(m_output.instructions.size());
  m_output.instructions.emplace_back(op, operand);
  recordSourceMapping(ip, loc);
}

u32 Compiler::emitJump(OpCode op) {
  u32 index = static_cast<u32>(m_output.instructions.size());
  emitOp(op, 0); // Placeholder, will be patched
  return index;
}

u32 Compiler::emitJump(OpCode op, const SourceLocation &loc) {
  u32 index = static_cast<u32>(m_output.instructions.size());
  emitOp(op, 0, loc); // Placeholder, will be patched
  return index;
}

void Compiler::recordSourceMapping(u32 ip, const SourceLocation &loc) {
  if (loc.line > 0) { // Only record valid source locations
    DebugSourceLocation debugLoc;
    debugLoc.filePath = m_sourceFilePath;
    debugLoc.line = loc.line;
    debugLoc.column = loc.column;
    debugLoc.sceneName = m_currentScene;
    m_output.sourceMappings[ip] = debugLoc;
  }
}

bool Compiler::patchJump(u32 jumpIndex) {
  // Bounds check to prevent buffer overflow (security vulnerability)
  if (jumpIndex >= m_output.instructions.size()) {
    error(ErrorCode::CompilationFailed,
          "Internal compiler error: Invalid jump instruction index " +
          std::to_string(jumpIndex) + " (program has " +
          std::to_string(m_output.instructions.size()) + " instructions). " +
          "This indicates a compiler bug. Please report this issue.");
    return false;
  }

  u32 target = static_cast<u32>(m_output.instructions.size());
  m_output.instructions[jumpIndex].operand = target;
  return true;
}

u32 Compiler::addString(const std::string &str) {
  // Check if string already exists
  for (u32 i = 0; i < m_output.stringTable.size(); ++i) {
    if (m_output.stringTable[i] == str) {
      return i;
    }
  }

  u32 index = static_cast<u32>(m_output.stringTable.size());
  m_output.stringTable.push_back(str);
  return index;
}

void Compiler::error(const std::string &message, SourceLocation loc) {
  m_errors.emplace_back(message, loc);
}

void Compiler::error(ErrorCode code, const std::string &message, SourceLocation loc) {
  // Add to legacy error list for backward compatibility
  m_errors.emplace_back(message, loc);

  // Create rich ScriptError
  ScriptError err(code, Severity::Error, message, loc);
  if (!m_sourceFilePath.empty()) {
    err.filePath = m_sourceFilePath;
  }
  if (!m_source.empty()) {
    err.source = m_source;
  }
  m_scriptErrors.add(std::move(err));
}

void Compiler::addError(ScriptError err) {
  // Add to legacy error list for backward compatibility
  m_errors.emplace_back(err.message, err.span.start);

  // Set file path and source if not already set
  if (!err.filePath.has_value() && !m_sourceFilePath.empty()) {
    err.filePath = m_sourceFilePath;
  }
  if (!err.source.has_value() && !m_source.empty()) {
    err.source = m_source;
  }
  m_scriptErrors.add(std::move(err));
}

void Compiler::compileProgram(const Program &program) {
  // First pass: register all characters
  for (const auto &character : program.characters) {
    compileCharacter(character);
  }

  // Second pass: compile all scenes
  for (const auto &scene : program.scenes) {
    compileScene(scene);
  }

  // Global statements (if any)
  for (const auto &stmt : program.globalStatements) {
    if (stmt) {
      compileStatement(*stmt);
    }
  }

  // Add HALT at end
  emitOp(OpCode::HALT);
}

void Compiler::compileCharacter(const CharacterDecl &decl) {
  m_output.characters[decl.id] = decl;
}

void Compiler::compileScene(const SceneDecl &decl) {
  // Record entry point
  u32 entryPoint = static_cast<u32>(m_output.instructions.size());
  m_output.sceneEntryPoints[decl.name] = entryPoint;
  m_labels[decl.name] = entryPoint;

  m_currentScene = decl.name;

  // Compile scene body
  for (const auto &stmt : decl.body) {
    if (stmt) {
      compileStatement(*stmt);
    }
  }

  m_currentScene.clear();
}

void Compiler::compileStatement(const Statement &stmt) {
  std::visit(
      [this, &stmt](const auto &s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, ShowStmt>) {
          compileShowStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, HideStmt>) {
          compileHideStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, SayStmt>) {
          compileSayStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, ChoiceStmt>) {
          compileChoiceStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          compileIfStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
          compileGotoStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, WaitStmt>) {
          compileWaitStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, PlayStmt>) {
          compilePlayStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, StopStmt>) {
          compileStopStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, SetStmt>) {
          compileSetStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, TransitionStmt>) {
          compileTransitionStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, MoveStmt>) {
          compileMoveStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          compileBlockStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, ExpressionStmt>) {
          compileExpressionStmt(s, stmt.location);
        } else if constexpr (std::is_same_v<T, CharacterDecl>) {
          compileCharacter(s);
        } else if constexpr (std::is_same_v<T, SceneDecl>) {
          compileScene(s);
        }
      },
      stmt.data);
}

void Compiler::compileExpression(const Expression &expr) {
  std::visit(
      [this](const auto &e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, LiteralExpr>) {
          compileLiteral(e);
        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          compileIdentifier(e);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          compileBinary(e);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          compileUnary(e);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          compileCall(e);
        } else if constexpr (std::is_same_v<T, PropertyExpr>) {
          compileProperty(e);
        }
      },
      expr.data);
}

// Statement compilers

void Compiler::compileShowStmt(const ShowStmt &stmt, const SourceLocation &loc) {
  switch (stmt.target) {
  case ShowStmt::Target::Background: {
    u32 resIndex = addString(stmt.resource.value_or(""));
    emitOp(OpCode::SHOW_BACKGROUND, resIndex, loc);
    break;
  }

  case ShowStmt::Target::Character:
  case ShowStmt::Target::Sprite: {
    u32 idIndex = addString(stmt.identifier);
    emitOp(OpCode::PUSH_STRING, idIndex);

    // Push position (encoded as int: 0=left, 1=center, 2=right, 3=custom)
    i32 posCode = 1; // Default center
    if (stmt.position.has_value()) {
      switch (stmt.position.value()) {
      case Position::Left:
        posCode = 0;
        break;
      case Position::Center:
        posCode = 1;
        break;
      case Position::Right:
        posCode = 2;
        break;
      case Position::Custom:
        posCode = 3;
        break;
      }
    }
    emitOp(OpCode::PUSH_INT, static_cast<u32>(posCode));

    emitOp(OpCode::SHOW_CHARACTER, idIndex, loc);
    break;
  }
  }

  // Handle transition if specified
  if (stmt.transition.has_value()) {
    u32 transIndex = addString(stmt.transition.value());
    // Push duration (convert float to portable little-endian representation)
    // Uses bit_cast (C++20) for well-defined float->u32 conversion, then converts to little-endian
    u32 durInt = 0;
    if (stmt.duration.has_value()) {
      f32 dur = stmt.duration.value();
      durInt = serializeFloat(dur);
    }
    emitOp(OpCode::PUSH_INT, durInt);
    emitOp(OpCode::TRANSITION, transIndex);
  }
}

void Compiler::compileHideStmt(const HideStmt &stmt, const SourceLocation &loc) {
  u32 idIndex = addString(stmt.identifier);
  emitOp(OpCode::HIDE_CHARACTER, idIndex, loc);

  // Handle transition if specified
  if (stmt.transition.has_value()) {
    u32 transIndex = addString(stmt.transition.value());
    u32 durInt = 0;
    if (stmt.duration.has_value()) {
      f32 dur = stmt.duration.value();
      durInt = serializeFloat(dur);
    }
    emitOp(OpCode::PUSH_INT, durInt);
    emitOp(OpCode::TRANSITION, transIndex);
  }
}

void Compiler::compileSayStmt(const SayStmt &stmt, const SourceLocation &loc) {
  u32 textIndex = addString(stmt.text);

  if (stmt.speaker.has_value()) {
    u32 speakerIndex = addString(stmt.speaker.value());
    emitOp(OpCode::PUSH_STRING, speakerIndex);
  } else {
    emitOp(OpCode::PUSH_NULL);
  }

  emitOp(OpCode::SAY, textIndex, loc);
}

void Compiler::compileChoiceStmt(const ChoiceStmt &stmt, const SourceLocation &loc) {
  // Validate choice count limit
  if (stmt.options.size() > MAX_CHOICE_COUNT) {
    error("Too many choices in choice statement: " +
          std::to_string(stmt.options.size()) + " (maximum allowed: " +
          std::to_string(MAX_CHOICE_COUNT) + ")", loc);
    return;
  }

  // Emit choice count
  emitOp(OpCode::PUSH_INT, static_cast<u32>(stmt.options.size()));

  // Emit each choice text
  for (const auto &option : stmt.options) {
    u32 textIndex = addString(option.text);
    emitOp(OpCode::PUSH_STRING, textIndex);
  }

  // Emit CHOICE opcode (will wait for player selection)
  emitOp(OpCode::CHOICE, static_cast<u32>(stmt.options.size()), loc);

  // After choice returns, the result is on stack
  // Generate jump table
  std::vector<u32> endJumps;

  for (size_t i = 0; i < stmt.options.size(); ++i) {
    const auto &option = stmt.options[i];

    // Compare choice result with index
    emitOp(OpCode::DUP);
    emitOp(OpCode::PUSH_INT, static_cast<u32>(i));
    emitOp(OpCode::EQ);

    // Jump if not this choice
    u32 skipJump = emitJump(OpCode::JUMP_IF_NOT);

    // Pop the choice result (we're done with it)
    emitOp(OpCode::POP);

    // Handle condition if present
    if (option.condition && option.condition.value()) {
      compileExpression(*(option.condition.value()));
      u32 condJump = emitJump(OpCode::JUMP_IF_NOT);

      // Execute choice body or goto
      if (option.gotoTarget.has_value()) {
        // Forward reference to scene/label
        u32 jumpIndex = static_cast<u32>(m_output.instructions.size());
        emitOp(OpCode::JUMP, 0);
        m_pendingJumps.push_back({jumpIndex, option.gotoTarget.value(), loc});
      } else {
        for (const auto &bodyStmt : option.body) {
          if (bodyStmt) {
            compileStatement(*bodyStmt);
          }
        }
      }

      patchJump(condJump);
    } else {
      // No condition
      if (option.gotoTarget.has_value()) {
        u32 jumpIndex = static_cast<u32>(m_output.instructions.size());
        emitOp(OpCode::JUMP, 0);
        m_pendingJumps.push_back({jumpIndex, option.gotoTarget.value(), loc});
      } else {
        for (const auto &bodyStmt : option.body) {
          if (bodyStmt) {
            compileStatement(*bodyStmt);
          }
        }
      }
    }

    // Jump to end of choice block
    endJumps.push_back(emitJump(OpCode::JUMP));

    // Patch the skip jump
    patchJump(skipJump);
  }

  // Pop unused choice result
  emitOp(OpCode::POP);

  // Patch all end jumps
  for (u32 jumpIndex : endJumps) {
    patchJump(jumpIndex);
  }
}

void Compiler::compileIfStmt(const IfStmt &stmt, [[maybe_unused]] const SourceLocation &loc) {
  // Compile condition
  compileExpression(*stmt.condition);

  // Jump to else if false
  u32 elseJump = emitJump(OpCode::JUMP_IF_NOT);

  // Then branch
  for (const auto &thenStmt : stmt.thenBranch) {
    if (thenStmt) {
      compileStatement(*thenStmt);
    }
  }

  // Jump over else
  u32 endJump = emitJump(OpCode::JUMP);

  // Else branch
  patchJump(elseJump);

  for (const auto &elseStmt : stmt.elseBranch) {
    if (elseStmt) {
      compileStatement(*elseStmt);
    }
  }

  patchJump(endJump);
}

void Compiler::compileGotoStmt(const GotoStmt &stmt, const SourceLocation &loc) {
  u32 jumpIndex = static_cast<u32>(m_output.instructions.size());
  emitOp(OpCode::GOTO_SCENE, 0, loc);
  m_pendingJumps.push_back({jumpIndex, stmt.target, loc});
}

void Compiler::compileWaitStmt(const WaitStmt &stmt, const SourceLocation &loc) {
  // Convert float duration to portable little-endian representation
  u32 durInt = serializeFloat(stmt.duration);
  emitOp(OpCode::WAIT, durInt, loc);
}

void Compiler::compilePlayStmt(const PlayStmt &stmt, const SourceLocation &loc) {
  u32 resIndex = addString(stmt.resource);

  if (stmt.type == PlayStmt::MediaType::Sound) {
    emitOp(OpCode::PLAY_SOUND, resIndex, loc);
  } else {
    emitOp(OpCode::PLAY_MUSIC, resIndex, loc);
  }
}

void Compiler::compileStopStmt(const StopStmt &stmt, const SourceLocation &loc) {
  if (stmt.fadeOut.has_value()) {
    f32 dur = stmt.fadeOut.value();
    u32 durInt = serializeFloat(dur);
    emitOp(OpCode::PUSH_INT, durInt);
  }

  emitOp(OpCode::STOP_MUSIC, 0, loc);
}

void Compiler::compileSetStmt(const SetStmt &stmt, const SourceLocation &loc) {
  // Compile value expression
  compileExpression(*stmt.value);

  // Store to variable
  u32 varIndex = addString(stmt.variable);

  // Use SET_FLAG for flags (boolean variables), STORE_GLOBAL for general
  // variables
  if (stmt.isFlag) {
    emitOp(OpCode::SET_FLAG, varIndex, loc);
  } else {
    emitOp(OpCode::STORE_GLOBAL, varIndex, loc);
  }
}

void Compiler::compileTransitionStmt(const TransitionStmt &stmt, [[maybe_unused]] const SourceLocation &loc) {
  u32 typeIndex = addString(stmt.type);
  u32 durInt = serializeFloat(stmt.duration);
  emitOp(OpCode::PUSH_INT, durInt);
  emitOp(OpCode::TRANSITION, typeIndex);
}

void Compiler::compileMoveStmt(const MoveStmt &stmt, [[maybe_unused]] const SourceLocation &loc) {
  // Push character ID string
  u32 charIndex = addString(stmt.characterId);
  emitOp(OpCode::PUSH_STRING, charIndex);

  // Push position (encoded as int: 0=left, 1=center, 2=right, 3=custom)
  i32 posCode = 1; // Default center
  switch (stmt.position) {
  case Position::Left:
    posCode = 0;
    break;
  case Position::Center:
    posCode = 1;
    break;
  case Position::Right:
    posCode = 2;
    break;
  case Position::Custom:
    posCode = 3;
    break;
  }
  emitOp(OpCode::PUSH_INT, static_cast<u32>(posCode));

  // Push custom coordinates if position is custom
  if (stmt.position == Position::Custom) {
    f32 x = stmt.customX.value_or(0.5f);
    f32 y = stmt.customY.value_or(0.5f);
    u32 xInt = serializeFloat(x);
    u32 yInt = serializeFloat(y);
    emitOp(OpCode::PUSH_FLOAT, xInt);
    emitOp(OpCode::PUSH_FLOAT, yInt);
  }

  // Push duration (convert float to portable little-endian representation)
  u32 durInt = serializeFloat(stmt.duration);
  emitOp(OpCode::PUSH_INT, durInt);

  // Emit the MOVE_CHARACTER opcode with character string index
  emitOp(OpCode::MOVE_CHARACTER, charIndex);
}

void Compiler::compileBlockStmt(const BlockStmt &stmt, [[maybe_unused]] const SourceLocation &loc) {
  for (const auto &s : stmt.statements) {
    if (s) {
      compileStatement(*s);
    }
  }
}

void Compiler::compileExpressionStmt(const ExpressionStmt &stmt, [[maybe_unused]] const SourceLocation &loc) {
  if (stmt.expression) {
    compileExpression(*stmt.expression);
    emitOp(OpCode::POP); // Discard result
  }
}

// Expression compilers

void Compiler::compileLiteral(const LiteralExpr &expr) {
  std::visit(
      [this](const auto &val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          emitOp(OpCode::PUSH_NULL);
        } else if constexpr (std::is_same_v<T, i32>) {
          emitOp(OpCode::PUSH_INT, static_cast<u32>(val));
        } else if constexpr (std::is_same_v<T, f32>) {
          u32 intRep = serializeFloat(val);
          emitOp(OpCode::PUSH_FLOAT, intRep);
        } else if constexpr (std::is_same_v<T, bool>) {
          emitOp(OpCode::PUSH_BOOL, val ? 1 : 0);
        } else if constexpr (std::is_same_v<T, std::string>) {
          u32 index = addString(val);
          emitOp(OpCode::PUSH_STRING, index);
        }
      },
      expr.value);
}

void Compiler::compileIdentifier(const IdentifierExpr &expr) {
  u32 nameIndex = addString(expr.name);
  emitOp(OpCode::LOAD_GLOBAL, nameIndex);
}

void Compiler::compileBinary(const BinaryExpr &expr) {
  // Compile left operand
  if (expr.left) {
    compileExpression(*expr.left);
  }

  // Short-circuit for and/or
  if (expr.op == TokenType::And) {
    // Duplicate left value for short-circuit evaluation
    emitOp(OpCode::DUP);
    // If left is false, jump to end keeping the false value
    u32 endJump = emitJump(OpCode::JUMP_IF_NOT);
    // If left is true, pop it and evaluate right
    emitOp(OpCode::POP);

    if (expr.right) {
      compileExpression(*expr.right);
    }

    patchJump(endJump);
    return;
  }

  if (expr.op == TokenType::Or) {
    // Duplicate left value for short-circuit evaluation
    emitOp(OpCode::DUP);
    // If left is true, jump to end keeping the true value
    u32 endJump = emitJump(OpCode::JUMP_IF);
    // If left is false, pop it and evaluate right
    emitOp(OpCode::POP);

    if (expr.right) {
      compileExpression(*expr.right);
    }

    patchJump(endJump);
    return;
  }

  // Compile right operand
  if (expr.right) {
    compileExpression(*expr.right);
  }

  // Emit operator
  switch (expr.op) {
  case TokenType::Plus:
    emitOp(OpCode::ADD);
    break;
  case TokenType::Minus:
    emitOp(OpCode::SUB);
    break;
  case TokenType::Star:
    emitOp(OpCode::MUL);
    break;
  case TokenType::Slash:
    emitOp(OpCode::DIV);
    break;
  case TokenType::Percent:
    emitOp(OpCode::MOD);
    break;
  case TokenType::Equal:
    emitOp(OpCode::EQ);
    break;
  case TokenType::NotEqual:
    emitOp(OpCode::NE);
    break;
  case TokenType::Less:
    emitOp(OpCode::LT);
    break;
  case TokenType::LessEqual:
    emitOp(OpCode::LE);
    break;
  case TokenType::Greater:
    emitOp(OpCode::GT);
    break;
  case TokenType::GreaterEqual:
    emitOp(OpCode::GE);
    break;
  default:
    error("Unknown binary operator");
    break;
  }
}

void Compiler::compileUnary(const UnaryExpr &expr) {
  if (expr.operand) {
    compileExpression(*expr.operand);
  }

  switch (expr.op) {
  case TokenType::Minus:
    emitOp(OpCode::NEG);
    break;
  case TokenType::Not:
    emitOp(OpCode::NOT);
    break;
  default:
    error("Unknown unary operator");
    break;
  }
}

void Compiler::compileCall(const CallExpr &expr) {
  // Compile arguments
  for (const auto &arg : expr.arguments) {
    if (arg) {
      compileExpression(*arg);
    }
  }

  // For now, function calls are handled as native callbacks
  // The VM will need to be extended to support this
  u32 funcIndex = addString(expr.callee);
  emitOp(OpCode::CALL, funcIndex);
}

void Compiler::compileProperty(const PropertyExpr &expr) {
  // Compile object
  if (expr.object) {
    compileExpression(*expr.object);
  }

  // Property access currently pushes property name as a string.
  // Object system integration will add a dedicated GET_PROPERTY opcode.
  u32 propIndex = addString(expr.property);
  emitOp(OpCode::PUSH_STRING, propIndex);
}

} // namespace NovelMind::scripting
