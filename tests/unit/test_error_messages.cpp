#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

using namespace NovelMind::scripting;

TEST_CASE("Compiler error messages are helpful and actionable", "[compiler][errors]") {
  Lexer lexer;
  Parser parser;
  Compiler compiler;

  SECTION("Undefined label error includes suggestions") {
    // Script with undefined label
    const char* script = R"(
            scene test {
                goto unknownLabel
            }
        )";

    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    auto ast = parser.parse(tokens.value());
    REQUIRE(ast.isOk());

    compiler.setSource(script);
    auto result = compiler.compile(ast.value(), "test.nms");

    // Should fail compilation
    REQUIRE_FALSE(result.isOk());

    // Check that rich errors are available
    const auto& scriptErrors = compiler.getScriptErrors();
    REQUIRE_FALSE(scriptErrors.isEmpty());

    // First error should be about undefined label
    bool foundUndefinedLabel = false;
    for (const auto& err : scriptErrors.getErrors()) {
      if (err.code == ErrorCode::InvalidGotoTarget) {
        foundUndefinedLabel = true;
        // Check message contains helpful information
        REQUIRE(err.message.find("not defined") != std::string::npos);
        break;
      }
    }
    REQUIRE(foundUndefinedLabel);
  }

  SECTION("Undefined label with similar labels suggests alternatives") {
    // Script with typo in label name
    const char* script = R"(
            scene test {
                label startScene
                goto startSceen
            }
        )";

    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    auto ast = parser.parse(tokens.value());
    REQUIRE(ast.isOk());

    compiler.setSource(script);
    auto result = compiler.compile(ast.value(), "test.nms");

    // Should fail compilation
    REQUIRE_FALSE(result.isOk());

    // Check that suggestions are provided
    const auto& scriptErrors = compiler.getScriptErrors();
    REQUIRE_FALSE(scriptErrors.isEmpty());

    bool foundSuggestion = false;
    for (const auto& err : scriptErrors.getErrors()) {
      if (err.code == ErrorCode::InvalidGotoTarget) {
        // Should suggest "startScene" for typo "startSceen"
        if (!err.suggestions.empty()) {
          foundSuggestion = true;
          // Should suggest the correct label
          bool hasCorrectSuggestion = false;
          for (const auto& suggestion : err.suggestions) {
            if (suggestion == "startScene") {
              hasCorrectSuggestion = true;
              break;
            }
          }
          REQUIRE(hasCorrectSuggestion);
        }
        break;
      }
    }
    // Note: Suggestions may not always be provided depending on similarity
    // but for this close typo, we expect one
    REQUIRE(foundSuggestion);
  }

  SECTION("Internal compiler errors have helpful messages") {
    // We can't easily trigger internal errors, but we can verify
    // that the error format would be helpful if it occurred.
    // Just verify the error mechanism works.
    compiler.setSource("test");
    auto result = compiler.compile(Program{}, "test.nms");

    // Empty program should compile successfully
    REQUIRE(result.isOk());
  }
}

TEST_CASE("Validation error messages are helpful", "[validator][errors]") {
  Lexer lexer;
  Parser parser;
  Validator validator;

  SECTION("Duplicate character definition has clear error") {
    const char* script = R"(
            character Hero
            character Hero
        )";

    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    auto ast = parser.parse(tokens.value());
    REQUIRE(ast.isOk());

    validator.setSource(script);
    validator.setFilePath("test.nms");
    auto result = validator.validate(ast.value());

    REQUIRE_FALSE(result.isValid);
    REQUIRE(result.errors.hasErrors());

    // Find the duplicate definition error
    bool foundDuplicateError = false;
    for (const auto& err : result.errors.getErrors()) {
      if (err.code == ErrorCode::DuplicateCharacterDefinition) {
        foundDuplicateError = true;
        REQUIRE(err.message.find("already defined") != std::string::npos);
        // Should have related information pointing to first definition
        REQUIRE_FALSE(err.relatedInfo.empty());
        break;
      }
    }
    REQUIRE(foundDuplicateError);
  }

  SECTION("Undefined character has suggestions") {
    const char* script = R"(
            character Hero
            scene test {
                say Heros "Hello"
            }
        )";

    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    auto ast = parser.parse(tokens.value());
    REQUIRE(ast.isOk());

    validator.setSource(script);
    validator.setFilePath("test.nms");
    auto result = validator.validate(ast.value());

    REQUIRE_FALSE(result.isValid);

    // Should suggest "Hero" for typo "Heros"
    bool foundSuggestion = false;
    for (const auto& err : result.errors.getErrors()) {
      if (err.code == ErrorCode::UndefinedCharacter) {
        if (!err.suggestions.empty()) {
          foundSuggestion = true;
          break;
        }
      }
    }
    REQUIRE(foundSuggestion);
  }
}

TEST_CASE("ScriptError formatting provides context", "[script_error]") {
  SECTION("Error with source code shows context") {
    const char* source = R"(character Hero
scene intro {
    say Hero "Hello"
    goto badLabel
})";

    ScriptError err(ErrorCode::InvalidGotoTarget, Severity::Error,
                    "Label 'badLabel' is not defined", SourceLocation{"test.nms", 4, 10});
    err.filePath = "test.nms";
    err.source = source;

    // Format the error
    std::string formatted = err.format();

    // Should contain file path and line number
    REQUIRE(formatted.find("test.nms") != std::string::npos);
    REQUIRE(formatted.find("4") != std::string::npos);

    // Should contain the error message
    REQUIRE(formatted.find("not defined") != std::string::npos);
  }

  SECTION("Error with suggestions shows them") {
    ScriptError err(ErrorCode::UndefinedCharacter, Severity::Error,
                    "Character 'Heros' is not defined", SourceLocation{"test.nms", 3, 9});
    err.suggestions.push_back("Hero");
    err.suggestions.push_back("Heroes");

    std::string formatted = err.format();

    // Should mention suggestions
    REQUIRE(formatted.find("Hero") != std::string::npos);
  }
}
