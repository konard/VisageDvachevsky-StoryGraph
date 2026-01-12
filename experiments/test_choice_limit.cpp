#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/compiler.hpp"

using namespace NovelMind::scripting;

/**
 * Test for choice count limit validation (Issue #510)
 *
 * Verifies that the compiler enforces a maximum of 256 choices
 * and reports clear error when the limit is exceeded.
 */
TEST_CASE("Compiler enforces choice count limit", "[compiler][choice-limit]")
{
    Lexer lexer;
    Parser parser;
    Compiler compiler;

    SECTION("accepts 256 choices (at limit)")
    {
        // Generate a script with exactly 256 choices
        std::string script = "scene test {\n  choice {\n";
        for (int i = 0; i < 256; i++) {
            script += "    \"Choice " + std::to_string(i) + "\" -> goto label" + std::to_string(i) + "\n";
        }
        script += "  }\n}";

        auto tokens = lexer.tokenize(script);
        REQUIRE(tokens.isOk());

        auto parseResult = parser.parse(tokens.value());
        REQUIRE(parseResult.isOk());

        auto compileResult = compiler.compile(parseResult.value());
        REQUIRE(compileResult.isOk());
    }

    SECTION("rejects 257 choices (over limit)")
    {
        // Generate a script with 257 choices (one over the limit)
        std::string script = "scene test {\n  choice {\n";
        for (int i = 0; i < 257; i++) {
            script += "    \"Choice " + std::to_string(i) + "\" -> goto label" + std::to_string(i) + "\n";
        }
        script += "  }\n}";

        auto tokens = lexer.tokenize(script);
        REQUIRE(tokens.isOk());

        auto parseResult = parser.parse(tokens.value());
        REQUIRE(parseResult.isOk());

        auto compileResult = compiler.compile(parseResult.value());
        REQUIRE(compileResult.isError());

        // Verify error message contains relevant information
        const auto& errors = compiler.getErrors();
        REQUIRE(errors.size() > 0);
        REQUIRE(errors[0].message.find("Too many choices") != std::string::npos);
        REQUIRE(errors[0].message.find("257") != std::string::npos);
        REQUIRE(errors[0].message.find("256") != std::string::npos);
    }

    SECTION("rejects 1000 choices (far over limit)")
    {
        // Generate a script with 1000 choices to verify protection against extreme cases
        std::string script = "scene test {\n  choice {\n";
        for (int i = 0; i < 1000; i++) {
            script += "    \"Choice " + std::to_string(i) + "\" -> goto label" + std::to_string(i) + "\n";
        }
        script += "  }\n}";

        auto tokens = lexer.tokenize(script);
        REQUIRE(tokens.isOk());

        auto parseResult = parser.parse(tokens.value());
        REQUIRE(parseResult.isOk());

        auto compileResult = compiler.compile(parseResult.value());
        REQUIRE(compileResult.isError());

        const auto& errors = compiler.getErrors();
        REQUIRE(errors.size() > 0);
        REQUIRE(errors[0].message.find("Too many choices") != std::string::npos);
    }

    SECTION("accepts small number of choices")
    {
        std::string script = R"(
            scene test {
                choice {
                    "Option 1" -> goto opt1
                    "Option 2" -> goto opt2
                    "Option 3" -> goto opt3
                }
            }
        )";

        auto tokens = lexer.tokenize(script);
        REQUIRE(tokens.isOk());

        auto parseResult = parser.parse(tokens.value());
        REQUIRE(parseResult.isOk());

        auto compileResult = compiler.compile(parseResult.value());
        REQUIRE(compileResult.isOk());
    }
}
