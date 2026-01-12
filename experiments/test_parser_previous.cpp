#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"

using namespace NovelMind::scripting;

// Test that previous() has proper bounds checking
TEST_CASE("Parser previous() bounds check", "[parser][experiment]")
{
    Lexer lexer;
    Parser parser;

    SECTION("empty input doesn't crash")
    {
        auto tokens = lexer.tokenize("");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());
    }

    SECTION("single token input works")
    {
        auto tokens = lexer.tokenize("goto scene1");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());
    }

    SECTION("multiple tokens work correctly")
    {
        auto tokens = lexer.tokenize(R"(
            show Hero at center
            say Hero "Hello"
            hide Hero
        )");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());
    }
}
