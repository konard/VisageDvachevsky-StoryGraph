#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"

using namespace NovelMind::scripting;

// =============================================================================
// Helper Functions
// =============================================================================

namespace CompilerTestHelpers {

// Helper to compile a script and return the result
Result<CompiledScript> compileScript(const std::string &source) {
  Lexer lexer;
  auto tokens = lexer.tokenize(source);
  if (tokens.isError()) {
    return Result<CompiledScript>::error("Lexer error: " + tokens.errorMessage());
  }

  Parser parser;
  auto program = parser.parse(tokens.value());
  if (program.isError()) {
    return Result<CompiledScript>::error("Parser error: " + program.errorMessage());
  }

  Compiler compiler;
  return compiler.compile(program.value(), "test.nm");
}

// Helper to check if instruction exists in compiled script
bool hasInstruction(const CompiledScript &script, OpCode opcode) {
  for (const auto &instr : script.instructions) {
    if (instr.opcode == opcode) {
      return true;
    }
  }
  return false;
}

// Helper to count instructions of a specific opcode
int countInstructions(const CompiledScript &script, OpCode opcode) {
  int count = 0;
  for (const auto &instr : script.instructions) {
    if (instr.opcode == opcode) {
      count++;
    }
  }
  return count;
}

// Helper to get instruction at index
const Instruction* getInstructionAt(const CompiledScript &script, size_t index) {
  if (index >= script.instructions.size()) {
    return nullptr;
  }
  return &script.instructions[index];
}

} // namespace CompilerTestHelpers

using namespace CompilerTestHelpers;

// =============================================================================
// Opcode Generation Tests
// =============================================================================

TEST_CASE("Compiler generates PUSH_INT for integer literals", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PUSH_INT));

  // Find the PUSH_INT instruction
  bool foundCorrectValue = false;
  for (const auto &instr : script.instructions) {
    if (instr.opcode == OpCode::PUSH_INT && instr.operand == 42) {
      foundCorrectValue = true;
      break;
    }
  }
  REQUIRE(foundCorrectValue);
}

TEST_CASE("Compiler generates PUSH_FLOAT for float literals", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = 3.14
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PUSH_FLOAT));
}

TEST_CASE("Compiler generates PUSH_STRING for string literals", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set message = "Hello, World!"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PUSH_STRING));
  REQUIRE(script.stringTable.size() >= 2); // "message" and "Hello, World!"

  // Check that "Hello, World!" is in the string table
  bool foundString = false;
  for (const auto &str : script.stringTable) {
    if (str == "Hello, World!") {
      foundString = true;
      break;
    }
  }
  REQUIRE(foundString);
}

TEST_CASE("Compiler generates PUSH_BOOL for boolean literals", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set flag = true
      set flag2 = false
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(countInstructions(script, OpCode::PUSH_BOOL) >= 2);

  // Check for true (operand = 1) and false (operand = 0)
  bool foundTrue = false;
  bool foundFalse = false;
  for (const auto &instr : script.instructions) {
    if (instr.opcode == OpCode::PUSH_BOOL) {
      if (instr.operand == 1) foundTrue = true;
      if (instr.operand == 0) foundFalse = true;
    }
  }
  REQUIRE(foundTrue);
  REQUIRE(foundFalse);
}

TEST_CASE("Compiler generates PUSH_NULL for null literals", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = null
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PUSH_NULL));
}

TEST_CASE("Compiler generates ADD for addition", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set sum = 10 + 5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::ADD));
}

TEST_CASE("Compiler generates SUB for subtraction", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set diff = 10 - 5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::SUB));
}

TEST_CASE("Compiler generates MUL for multiplication", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set product = 10 * 5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::MUL));
}

TEST_CASE("Compiler generates DIV for division", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set quotient = 10 / 5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::DIV));
}

TEST_CASE("Compiler generates MOD for modulo", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set remainder = 10 % 3
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::MOD));
}

TEST_CASE("Compiler generates NEG for unary minus", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = -42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::NEG));
}

TEST_CASE("Compiler generates comparison operators", "[compiler][opcode]") {
  SECTION("EQ for equality") {
    auto result = compileScript(R"(
      scene test {
        set equal = 5 == 5
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::EQ));
  }

  SECTION("NE for inequality") {
    auto result = compileScript(R"(
      scene test {
        set notEqual = 5 != 3
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::NE));
  }

  SECTION("LT for less than") {
    auto result = compileScript(R"(
      scene test {
        set less = 3 < 5
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::LT));
  }

  SECTION("LE for less than or equal") {
    auto result = compileScript(R"(
      scene test {
        set lessOrEqual = 5 <= 5
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::LE));
  }

  SECTION("GT for greater than") {
    auto result = compileScript(R"(
      scene test {
        set greater = 5 > 3
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::GT));
  }

  SECTION("GE for greater than or equal") {
    auto result = compileScript(R"(
      scene test {
        set greaterOrEqual = 5 >= 5
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::GE));
  }
}

TEST_CASE("Compiler generates logical operators", "[compiler][opcode]") {
  SECTION("AND operator") {
    auto result = compileScript(R"(
      scene test {
        set result = true and false
      }
    )");
    REQUIRE(result.isOk());
    // AND uses short-circuit evaluation with JUMP_IF_NOT
    REQUIRE(hasInstruction(result.value(), OpCode::JUMP_IF_NOT));
  }

  SECTION("OR operator") {
    auto result = compileScript(R"(
      scene test {
        set result = true or false
      }
    )");
    REQUIRE(result.isOk());
    // OR uses short-circuit evaluation with JUMP_IF_NOT
    REQUIRE(hasInstruction(result.value(), OpCode::JUMP_IF_NOT));
  }

  SECTION("NOT operator") {
    auto result = compileScript(R"(
      scene test {
        set result = not true
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::NOT));
  }
}

TEST_CASE("Compiler generates LOAD_GLOBAL and STORE_GLOBAL", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = 10
      set y = x
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::STORE_GLOBAL));
  REQUIRE(hasInstruction(script, OpCode::LOAD_GLOBAL));
}

TEST_CASE("Compiler generates HALT at program end", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      set x = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // HALT should be the last instruction
  REQUIRE(script.instructions.size() > 0);
  REQUIRE(script.instructions.back().opcode == OpCode::HALT);
}

TEST_CASE("Compiler generates POP for expression statements", "[compiler][opcode]") {
  auto result = compileScript(R"(
    scene test {
      42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::POP));
}

// =============================================================================
// Visual Novel Opcode Tests
// =============================================================================

TEST_CASE("Compiler generates SHOW_BACKGROUND", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      show background "forest.png"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::SHOW_BACKGROUND));

  // Check that "forest.png" is in string table
  bool found = false;
  for (const auto &str : script.stringTable) {
    if (str == "forest.png") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("Compiler generates SHOW_CHARACTER", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    character Hero(name="Hero")

    scene test {
      show Hero at center
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::SHOW_CHARACTER));
}

TEST_CASE("Compiler generates HIDE_CHARACTER", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    character Hero(name="Hero")

    scene test {
      hide Hero
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::HIDE_CHARACTER));
}

TEST_CASE("Compiler generates SAY", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    character Hero(name="Hero")

    scene test {
      Hero "Hello, world!"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::SAY));
}

TEST_CASE("Compiler generates CHOICE", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      choice {
        "Option 1" -> {
          set x = 1
        }
        "Option 2" -> {
          set x = 2
        }
      }
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::CHOICE));
}

TEST_CASE("Compiler generates PLAY_SOUND", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      play sound "click.wav"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PLAY_SOUND));
}

TEST_CASE("Compiler generates PLAY_MUSIC", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      play music "theme.ogg"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::PLAY_MUSIC));
}

TEST_CASE("Compiler generates STOP_MUSIC", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      stop music
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::STOP_MUSIC));
}

TEST_CASE("Compiler generates WAIT", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      wait 1.5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::WAIT));
}

TEST_CASE("Compiler generates TRANSITION", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      show background "sky.png" with fade 1.0
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::TRANSITION));
}

TEST_CASE("Compiler generates MOVE_CHARACTER", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    character Hero(name="Hero")

    scene test {
      move Hero to left in 1.0
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::MOVE_CHARACTER));
}

TEST_CASE("Compiler generates GOTO_SCENE", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene first {
      goto second
    }

    scene second {
      set x = 1
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::GOTO_SCENE));
}

TEST_CASE("Compiler generates SET_FLAG", "[compiler][opcode][vn]") {
  auto result = compileScript(R"(
    scene test {
      set flag visited = true
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  REQUIRE(hasInstruction(script, OpCode::SET_FLAG));
}

// =============================================================================
// Jump Target Resolution Tests
// =============================================================================

TEST_CASE("Compiler resolves jump targets for if statements", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene test {
      if true {
        set x = 1
      }
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have JUMP_IF_NOT for else branch and JUMP to skip else
  REQUIRE(hasInstruction(script, OpCode::JUMP_IF_NOT));
  REQUIRE(hasInstruction(script, OpCode::JUMP));

  // Verify jump targets are valid (within instruction range)
  for (const auto &instr : script.instructions) {
    if (instr.opcode == OpCode::JUMP ||
        instr.opcode == OpCode::JUMP_IF ||
        instr.opcode == OpCode::JUMP_IF_NOT) {
      REQUIRE(instr.operand <= script.instructions.size());
    }
  }
}

TEST_CASE("Compiler resolves jump targets for if-else statements", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene test {
      if false {
        set x = 1
      } else {
        set x = 2
      }
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have JUMP_IF_NOT for else and JUMP to skip else
  REQUIRE(countInstructions(script, OpCode::JUMP_IF_NOT) >= 1);
  REQUIRE(countInstructions(script, OpCode::JUMP) >= 1);

  // Verify all jump targets are valid
  for (const auto &instr : script.instructions) {
    if (instr.opcode == OpCode::JUMP ||
        instr.opcode == OpCode::JUMP_IF ||
        instr.opcode == OpCode::JUMP_IF_NOT) {
      REQUIRE(instr.operand <= script.instructions.size());
    }
  }
}

TEST_CASE("Compiler resolves nested jump targets", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene test {
      if true {
        if false {
          set x = 1
        } else {
          set y = 2
        }
      }
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have multiple jump instructions for nested ifs
  REQUIRE(countInstructions(script, OpCode::JUMP_IF_NOT) >= 2);

  // All jump targets must be valid
  for (const auto &instr : script.instructions) {
    if (instr.opcode == OpCode::JUMP ||
        instr.opcode == OpCode::JUMP_IF ||
        instr.opcode == OpCode::JUMP_IF_NOT) {
      REQUIRE(instr.operand <= script.instructions.size());
    }
  }
}

TEST_CASE("Compiler resolves scene labels", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene first {
      goto second
    }

    scene second {
      set x = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Verify scene entry points are recorded
  REQUIRE(script.sceneEntryPoints.count("first") > 0);
  REQUIRE(script.sceneEntryPoints.count("second") > 0);

  // Verify entry points are valid instruction indices
  REQUIRE(script.sceneEntryPoints["first"] < script.instructions.size());
  REQUIRE(script.sceneEntryPoints["second"] < script.instructions.size());
}

TEST_CASE("Compiler reports error for undefined label", "[compiler][jump][error]") {
  auto result = compileScript(R"(
    scene test {
      goto nonexistent
    }
  )");

  // Should fail compilation due to undefined label
  REQUIRE(result.isError());
  REQUIRE(result.errorMessage().find("Undefined label") != std::string::npos);
}

TEST_CASE("Compiler handles forward references", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene first {
      goto second
    }

    scene second {
      goto third
    }

    scene third {
      set done = true
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // All three scenes should have entry points
  REQUIRE(script.sceneEntryPoints.size() == 3);
  REQUIRE(script.sceneEntryPoints.count("first") > 0);
  REQUIRE(script.sceneEntryPoints.count("second") > 0);
  REQUIRE(script.sceneEntryPoints.count("third") > 0);
}

TEST_CASE("Compiler handles backward references", "[compiler][jump]") {
  auto result = compileScript(R"(
    scene first {
      set x = 1
    }

    scene second {
      goto first
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Both scenes should be accessible
  REQUIRE(script.sceneEntryPoints.count("first") > 0);
  REQUIRE(script.sceneEntryPoints.count("second") > 0);
}

// =============================================================================
// Expression Compilation Tests
// =============================================================================

TEST_CASE("Compiler evaluates operator precedence", "[compiler][expression]") {
  auto result = compileScript(R"(
    scene test {
      set result = 2 + 3 * 4
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should compile in correct order: push 3, push 4, MUL, push 2, ADD
  // Find the sequence: PUSH_INT(3), PUSH_INT(4), MUL
  bool foundCorrectOrder = false;
  for (size_t i = 0; i + 2 < script.instructions.size(); ++i) {
    if (script.instructions[i].opcode == OpCode::PUSH_INT &&
        script.instructions[i].operand == 3 &&
        script.instructions[i+1].opcode == OpCode::PUSH_INT &&
        script.instructions[i+1].operand == 4 &&
        script.instructions[i+2].opcode == OpCode::MUL) {
      foundCorrectOrder = true;
      break;
    }
  }
  REQUIRE(foundCorrectOrder);
}

TEST_CASE("Compiler handles parenthesized expressions", "[compiler][expression]") {
  auto result = compileScript(R"(
    scene test {
      set result = (2 + 3) * 4
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should compile with ADD before MUL due to parentheses
  // Find sequence: PUSH_INT(2), PUSH_INT(3), ADD, PUSH_INT(4), MUL
  bool foundCorrectOrder = false;
  for (size_t i = 0; i + 4 < script.instructions.size(); ++i) {
    if (script.instructions[i].opcode == OpCode::PUSH_INT &&
        script.instructions[i].operand == 2 &&
        script.instructions[i+1].opcode == OpCode::PUSH_INT &&
        script.instructions[i+1].operand == 3 &&
        script.instructions[i+2].opcode == OpCode::ADD &&
        script.instructions[i+3].opcode == OpCode::PUSH_INT &&
        script.instructions[i+3].operand == 4 &&
        script.instructions[i+4].opcode == OpCode::MUL) {
      foundCorrectOrder = true;
      break;
    }
  }
  REQUIRE(foundCorrectOrder);
}

TEST_CASE("Compiler handles complex nested expressions", "[compiler][expression]") {
  auto result = compileScript(R"(
    scene test {
      set result = ((2 + 3) * (4 - 1)) / 5
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have all required opcodes
  REQUIRE(hasInstruction(script, OpCode::ADD));
  REQUIRE(hasInstruction(script, OpCode::SUB));
  REQUIRE(hasInstruction(script, OpCode::MUL));
  REQUIRE(hasInstruction(script, OpCode::DIV));
}

TEST_CASE("Compiler handles unary expressions", "[compiler][expression]") {
  SECTION("Unary minus") {
    auto result = compileScript(R"(
      scene test {
        set x = -5
        set y = -(3 + 2)
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(countInstructions(result.value(), OpCode::NEG) >= 2);
  }

  SECTION("Logical NOT") {
    auto result = compileScript(R"(
      scene test {
        set x = not true
        set y = not (false or true)
      }
    )");
    REQUIRE(result.isOk());
    REQUIRE(hasInstruction(result.value(), OpCode::NOT));
  }
}

TEST_CASE("Compiler handles short-circuit evaluation", "[compiler][expression]") {
  SECTION("AND short-circuit") {
    auto result = compileScript(R"(
      scene test {
        set result = false and expensive_call()
      }
    )");
    REQUIRE(result.isOk());
    // AND uses JUMP_IF_NOT for short-circuit
    REQUIRE(hasInstruction(result.value(), OpCode::JUMP_IF_NOT));
  }

  SECTION("OR short-circuit") {
    auto result = compileScript(R"(
      scene test {
        set result = true or expensive_call()
      }
    )");
    REQUIRE(result.isOk());
    // OR uses JUMP_IF_NOT for short-circuit
    REQUIRE(hasInstruction(result.value(), OpCode::JUMP_IF_NOT));
  }
}

TEST_CASE("Compiler handles comparison chains", "[compiler][expression]") {
  auto result = compileScript(R"(
    scene test {
      set result = 1 < 2 and 2 < 3 and 3 < 4
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have multiple LT comparisons
  REQUIRE(countInstructions(script, OpCode::LT) >= 3);
}

// =============================================================================
// String Table Management Tests
// =============================================================================

TEST_CASE("Compiler deduplicates strings in string table", "[compiler][stringtable]") {
  auto result = compileScript(R"(
    scene test {
      set a = "duplicate"
      set b = "duplicate"
      set c = "unique"
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Count occurrences of "duplicate" in string table
  int duplicateCount = 0;
  for (const auto &str : script.stringTable) {
    if (str == "duplicate") {
      duplicateCount++;
    }
  }

  // Should only appear once due to deduplication
  REQUIRE(duplicateCount == 1);
}

TEST_CASE("Compiler adds variable names to string table", "[compiler][stringtable]") {
  auto result = compileScript(R"(
    scene test {
      set myVariable = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Variable name should be in string table
  bool found = false;
  for (const auto &str : script.stringTable) {
    if (str == "myVariable") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

// =============================================================================
// Source Mapping Tests
// =============================================================================

TEST_CASE("Compiler records source mappings", "[compiler][debug]") {
  auto result = compileScript(R"(
    scene test {
      set x = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have source mappings for instructions
  REQUIRE(script.sourceMappings.size() > 0);

  // Check that source mappings have valid line numbers
  for (const auto &[ip, loc] : script.sourceMappings) {
    REQUIRE(loc.line > 0);
    REQUIRE(loc.filePath == "test.nm");
  }
}

TEST_CASE("Compiler records scene names in source mappings", "[compiler][debug]") {
  auto result = compileScript(R"(
    scene myScene {
      set x = 42
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Find source mappings with scene name
  bool foundSceneName = false;
  for (const auto &[ip, loc] : script.sourceMappings) {
    if (loc.sceneName == "myScene") {
      foundSceneName = true;
      break;
    }
  }
  REQUIRE(foundSceneName);
}

// =============================================================================
// Character Declaration Tests
// =============================================================================

TEST_CASE("Compiler records character declarations", "[compiler][character]") {
  auto result = compileScript(R"(
    character Hero(name="Hero", color="#FF0000")
    character Villain(name="Villain")

    scene test {
      set x = 1
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have both characters registered
  REQUIRE(script.characters.size() == 2);
  REQUIRE(script.characters.count("Hero") > 0);
  REQUIRE(script.characters.count("Villain") > 0);

  // Verify character properties
  REQUIRE(script.characters.at("Hero").name == "Hero");
  REQUIRE(script.characters.at("Villain").name == "Villain");
}

// =============================================================================
// Edge Cases and Error Handling Tests
// =============================================================================

TEST_CASE("Compiler handles empty scene", "[compiler][edge]") {
  auto result = compileScript(R"(
    scene empty {
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have scene entry point
  REQUIRE(script.sceneEntryPoints.count("empty") > 0);

  // Should still have HALT
  REQUIRE(script.instructions.back().opcode == OpCode::HALT);
}

TEST_CASE("Compiler handles multiple scenes", "[compiler][edge]") {
  auto result = compileScript(R"(
    scene first {
      set x = 1
    }

    scene second {
      set y = 2
    }

    scene third {
      set z = 3
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // All scenes should have entry points
  REQUIRE(script.sceneEntryPoints.size() == 3);
  REQUIRE(script.sceneEntryPoints.count("first") > 0);
  REQUIRE(script.sceneEntryPoints.count("second") > 0);
  REQUIRE(script.sceneEntryPoints.count("third") > 0);
}

TEST_CASE("Compiler handles deeply nested blocks", "[compiler][edge]") {
  auto result = compileScript(R"(
    scene test {
      if true {
        if true {
          if true {
            set x = 1
          }
        }
      }
    }
  )");

  REQUIRE(result.isOk());
  // Should compile without errors
}

TEST_CASE("Compiler handles choice with multiple options", "[compiler][edge]") {
  auto result = compileScript(R"(
    scene test {
      choice {
        "Option 1" -> { set x = 1 }
        "Option 2" -> { set x = 2 }
        "Option 3" -> { set x = 3 }
        "Option 4" -> { set x = 4 }
      }
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have CHOICE instruction
  REQUIRE(hasInstruction(script, OpCode::CHOICE));

  // Should have all option strings in string table
  bool found1 = false, found2 = false, found3 = false, found4 = false;
  for (const auto &str : script.stringTable) {
    if (str == "Option 1") found1 = true;
    if (str == "Option 2") found2 = true;
    if (str == "Option 3") found3 = true;
    if (str == "Option 4") found4 = true;
  }
  REQUIRE(found1);
  REQUIRE(found2);
  REQUIRE(found3);
  REQUIRE(found4);
}

// =============================================================================
// Constant Folding Tests (Note: Compiler does NOT perform constant folding)
// =============================================================================

TEST_CASE("Compiler does NOT perform constant folding", "[compiler][optimization]") {
  // The current compiler implementation does not perform constant folding
  // It emits instructions for all operations, even if operands are constants

  auto result = compileScript(R"(
    scene test {
      set x = 2 + 3
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should have PUSH_INT for both 2 and 3, followed by ADD
  // (constant folding would optimize this to just PUSH_INT 5)
  REQUIRE(hasInstruction(script, OpCode::PUSH_INT));
  REQUIRE(hasInstruction(script, OpCode::ADD));

  // Verify it's not folded - should have separate pushes
  int pushIntCount = countInstructions(script, OpCode::PUSH_INT);
  REQUIRE(pushIntCount >= 2); // At least 2 and 3
}

TEST_CASE("Compiler compiles boolean constant expressions literally", "[compiler][optimization]") {
  auto result = compileScript(R"(
    scene test {
      set always_true = true or false
    }
  )");

  REQUIRE(result.isOk());
  const auto &script = result.value();

  // Should emit PUSH_BOOL for both true and false, not fold to just true
  REQUIRE(hasInstruction(script, OpCode::PUSH_BOOL));
  REQUIRE(hasInstruction(script, OpCode::JUMP_IF_NOT)); // OR uses jump
}
