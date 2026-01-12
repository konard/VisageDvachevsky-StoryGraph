#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace NovelMind;
using namespace NovelMind::scripting;

// =============================================================================
// Compiler Tests
// =============================================================================

TEST_CASE("Compiler: undefined label produces compilation error", "[compiler]") {
    // Test that the compiler produces an error when a goto references an
    // undefined label/scene, as per issue #455

    const char* script = R"(
scene start {
    say Hero "Starting..."
    goto nonexistent_scene
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should fail with an error
    REQUIRE(result.isError());

    // Error message should mention the undefined label
    std::string errorMsg = result.error();
    REQUIRE(errorMsg.find("Undefined label") != std::string::npos);
    REQUIRE(errorMsg.find("nonexistent_scene") != std::string::npos);
}

TEST_CASE("Compiler: undefined label in choice produces compilation error", "[compiler]") {
    // Test that the compiler produces an error when a choice option references
    // an undefined label/scene

    const char* script = R"(
scene start {
    choice {
        "Go to main" -> goto main_scene
        "Go nowhere" -> goto undefined_scene
    }
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should fail
    REQUIRE(result.isError());

    // Error message should mention the undefined label
    std::string errorMsg = result.error();
    REQUIRE(errorMsg.find("Undefined label") != std::string::npos);
}

TEST_CASE("Compiler: forward reference resolved correctly", "[compiler]") {
    // Test that the compiler correctly resolves forward references to labels
    // that are defined later in the script

    const char* script = R"(
scene start {
    say Hero "Starting..."
    goto end_scene
}

scene end_scene {
    say Hero "The end!"
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should succeed
    REQUIRE(result.isOk());

    // The compiled script should have both scene entry points
    auto& compiled = result.value();
    REQUIRE(compiled.sceneEntryPoints.count("start") > 0);
    REQUIRE(compiled.sceneEntryPoints.count("end_scene") > 0);

    // Verify the jump was resolved (operand should be non-zero)
    // The GOTO_SCENE instruction should have been patched with the target address
    bool foundResolvedJump = false;
    for (const auto& instruction : compiled.instructions) {
        if (instruction.opcode == OpCode::GOTO_SCENE && instruction.operand != 0) {
            foundResolvedJump = true;
            break;
        }
    }
    REQUIRE(foundResolvedJump);
}

TEST_CASE("Compiler: forward reference in choice resolved correctly", "[compiler]") {
    // Test that forward references in choice options are resolved correctly

    const char* script = R"(
scene start {
    choice {
        "Continue" -> goto middle
        "Skip to end" -> goto end
    }
}

scene middle {
    say Hero "Middle scene"
}

scene end {
    say Hero "End scene"
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should succeed
    REQUIRE(result.isOk());

    // All scenes should be registered
    auto& compiled = result.value();
    REQUIRE(compiled.sceneEntryPoints.count("start") > 0);
    REQUIRE(compiled.sceneEntryPoints.count("middle") > 0);
    REQUIRE(compiled.sceneEntryPoints.count("end") > 0);
}

TEST_CASE("Compiler: backward reference works correctly", "[compiler]") {
    // Test that backward references (jumping to a previously defined scene) work

    const char* script = R"(
scene start {
    say Hero "First scene"
}

scene second {
    say Hero "Second scene"
    goto start
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should succeed
    REQUIRE(result.isOk());
}

TEST_CASE("Compiler: multiple undefined labels produce multiple errors", "[compiler]") {
    // Test that multiple undefined labels are all caught and reported

    const char* script = R"(
scene start {
    goto undefined_one
    goto undefined_two
    goto undefined_three
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should fail
    REQUIRE(result.isError());

    // Check that all errors were collected
    const auto& errors = compiler.getErrors();
    REQUIRE(errors.size() >= 3);
}

TEST_CASE("Compiler: error includes source location", "[compiler]") {
    // Test that compilation errors include source location information

    const char* script = R"(
scene start {
    say Hero "Line 3"
    goto undefined_label
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto program = parser.parse(tokens.value());
    REQUIRE(program.isOk());

    Compiler compiler;
    auto result = compiler.compile(program.value());

    // Compilation should fail
    REQUIRE(result.isError());

    // Check that error has location information
    const auto& errors = compiler.getErrors();
    REQUIRE(errors.size() > 0);

    const auto& error = errors[0];
    // The location should be valid (line > 0)
    REQUIRE(error.location.line > 0);
    // The goto statement is on line 4, so location should reflect that
    REQUIRE(error.location.line == 4);
}
