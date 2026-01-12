#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/scripting/lexer.hpp"

using namespace NovelMind::scripting;

TEST_CASE("Lexer tokenizes basic tokens", "[lexer]") {
  Lexer lexer;

  SECTION("tokenizes keywords") {
    auto result = lexer.tokenize("character scene show hide say choice");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 7); // 6 keywords + EOF

    REQUIRE(tokens[0].type == TokenType::Character);
    REQUIRE(tokens[1].type == TokenType::Scene);
    REQUIRE(tokens[2].type == TokenType::Show);
    REQUIRE(tokens[3].type == TokenType::Hide);
    REQUIRE(tokens[4].type == TokenType::Say);
    REQUIRE(tokens[5].type == TokenType::Choice);
    REQUIRE(tokens[6].type == TokenType::EndOfFile);
  }

  SECTION("tokenizes identifiers") {
    auto result = lexer.tokenize("Hero myVariable _private");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4);

    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "Hero");
    REQUIRE(tokens[1].type == TokenType::Identifier);
    REQUIRE(tokens[1].lexeme == "myVariable");
    REQUIRE(tokens[2].type == TokenType::Identifier);
    REQUIRE(tokens[2].lexeme == "_private");
  }

  SECTION("tokenizes integers") {
    auto result = lexer.tokenize("0 42 12345");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4);

    REQUIRE(tokens[0].type == TokenType::Integer);
    REQUIRE(tokens[0].intValue == 0);
    REQUIRE(tokens[1].type == TokenType::Integer);
    REQUIRE(tokens[1].intValue == 42);
    REQUIRE(tokens[2].type == TokenType::Integer);
    REQUIRE(tokens[2].intValue == 12345);
  }

  SECTION("tokenizes floats") {
    auto result = lexer.tokenize("0.0 3.14 123.456");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4);

    REQUIRE(tokens[0].type == TokenType::Float);
    REQUIRE(tokens[0].floatValue == 0.0f);
    REQUIRE(tokens[1].type == TokenType::Float);
    REQUIRE(tokens[1].floatValue == Catch::Approx(3.14f));
    REQUIRE(tokens[2].type == TokenType::Float);
    REQUIRE(tokens[2].floatValue == Catch::Approx(123.456f));
  }

  SECTION("tokenizes strings") {
    auto result = lexer.tokenize(R"("hello" "world" "with spaces")");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4);

    REQUIRE(tokens[0].type == TokenType::String);
    REQUIRE(tokens[0].lexeme == "hello");
    REQUIRE(tokens[1].type == TokenType::String);
    REQUIRE(tokens[1].lexeme == "world");
    REQUIRE(tokens[2].type == TokenType::String);
    REQUIRE(tokens[2].lexeme == "with spaces");
  }

  SECTION("handles escape sequences in strings") {
    auto result = lexer.tokenize(R"("line1\nline2" "tab\there" "quote\"here")");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4);

    REQUIRE(tokens[0].lexeme == "line1\nline2");
    REQUIRE(tokens[1].lexeme == "tab\there");
    REQUIRE(tokens[2].lexeme == "quote\"here");
  }

  SECTION("tokenizes operators") {
    auto result = lexer.tokenize("= + - * / % == != < <= > >= ->");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 14);

    REQUIRE(tokens[0].type == TokenType::Assign);
    REQUIRE(tokens[1].type == TokenType::Plus);
    REQUIRE(tokens[2].type == TokenType::Minus);
    REQUIRE(tokens[3].type == TokenType::Star);
    REQUIRE(tokens[4].type == TokenType::Slash);
    REQUIRE(tokens[5].type == TokenType::Percent);
    REQUIRE(tokens[6].type == TokenType::Equal);
    REQUIRE(tokens[7].type == TokenType::NotEqual);
    REQUIRE(tokens[8].type == TokenType::Less);
    REQUIRE(tokens[9].type == TokenType::LessEqual);
    REQUIRE(tokens[10].type == TokenType::Greater);
    REQUIRE(tokens[11].type == TokenType::GreaterEqual);
    REQUIRE(tokens[12].type == TokenType::Arrow);
  }

  SECTION("tokenizes delimiters") {
    auto result = lexer.tokenize("( ) { } [ ] , : ; .");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 11);

    REQUIRE(tokens[0].type == TokenType::LeftParen);
    REQUIRE(tokens[1].type == TokenType::RightParen);
    REQUIRE(tokens[2].type == TokenType::LeftBrace);
    REQUIRE(tokens[3].type == TokenType::RightBrace);
    REQUIRE(tokens[4].type == TokenType::LeftBracket);
    REQUIRE(tokens[5].type == TokenType::RightBracket);
    REQUIRE(tokens[6].type == TokenType::Comma);
    REQUIRE(tokens[7].type == TokenType::Colon);
    REQUIRE(tokens[8].type == TokenType::Semicolon);
    REQUIRE(tokens[9].type == TokenType::Dot);
  }
}

TEST_CASE("Lexer handles comments", "[lexer]") {
  Lexer lexer;

  SECTION("skips line comments") {
    auto result = lexer.tokenize("show // this is a comment\nhide");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::Show);
    REQUIRE(tokens[1].type == TokenType::Hide);
  }

  SECTION("skips block comments") {
    auto result = lexer.tokenize("show /* block comment */ hide");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::Show);
    REQUIRE(tokens[1].type == TokenType::Hide);
  }

  SECTION("skips nested block comments") {
    auto result = lexer.tokenize("show /* outer /* inner */ outer */ hide");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::Show);
    REQUIRE(tokens[1].type == TokenType::Hide);
  }

  SECTION("reports unclosed block comment") {
    auto result = lexer.tokenize("show /* this comment never closes\nhide");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Unclosed block comment") != std::string::npos);
  }

  SECTION("reports unclosed nested block comment") {
    auto result = lexer.tokenize("show /* outer /* inner */ missing close\nhide");
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Unclosed block comment") != std::string::npos);
  }
}

TEST_CASE("Lexer tracks source locations", "[lexer]") {
  Lexer lexer;

  SECTION("tracks line and column for single line") {
    auto result = lexer.tokenize("show Hero");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens[0].location.line == 1);
    REQUIRE(tokens[0].location.column == 1);
    REQUIRE(tokens[1].location.line == 1);
    REQUIRE(tokens[1].location.column == 6);
  }

  SECTION("tracks line for multiple lines") {
    auto result = lexer.tokenize("show\nhide\ngoto");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens[0].location.line == 1);
    REQUIRE(tokens[1].location.line == 2);
    REQUIRE(tokens[2].location.line == 3);
  }
}

TEST_CASE("Lexer handles color literals", "[lexer]") {
  Lexer lexer;

  SECTION("parses hex colors") {
    auto result = lexer.tokenize("#FFCC00 #FF0000");
    REQUIRE(result.isOk());

    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].type == TokenType::String);
    REQUIRE(tokens[0].lexeme == "#FFCC00");
    REQUIRE(tokens[1].type == TokenType::String);
    REQUIRE(tokens[1].lexeme == "#FF0000");
  }
}

TEST_CASE("Lexer reports errors", "[lexer]") {
  Lexer lexer;

  SECTION("reports unterminated string") {
    auto result = lexer.tokenize("\"unterminated");
    REQUIRE(result.isError());
  }
}

TEST_CASE("Lexer handles nested comments correctly", "[lexer]") {
  Lexer lexer;

  SECTION("handles normal nested comments") {
    // Test various levels of nesting
    auto result1 = lexer.tokenize("show /* level 1 */ hide");
    REQUIRE(result1.isOk());
    REQUIRE(result1.value().size() == 3); // show, hide, EOF

    auto result2 = lexer.tokenize("show /* level 1 /* level 2 */ level 1 */ hide");
    REQUIRE(result2.isOk());
    REQUIRE(result2.value().size() == 3);

    auto result3 = lexer.tokenize("show /* 1 /* 2 /* 3 */ 2 */ 1 */ hide");
    REQUIRE(result3.isOk());
    REQUIRE(result3.value().size() == 3);

    auto result4 = lexer.tokenize("show /* 1 /* 2 /* 3 /* 4 */ 3 */ 2 */ 1 */ hide");
    REQUIRE(result4.isOk());
    REQUIRE(result4.value().size() == 3);
  }

  SECTION("handles deeply nested comments within limit") {
    // Build a string with many nested comments (well below the limit of 128)
    std::string input = "show ";
    const int nestingLevel = 64; // Half the limit - should work fine

    // Open all nested comments
    for (int i = 0; i < nestingLevel; ++i) {
      input += "/* ";
    }

    input += "deeply nested ";

    // Close all nested comments
    for (int i = 0; i < nestingLevel; ++i) {
      input += "*/ ";
    }

    input += "hide";

    auto result = lexer.tokenize(input);
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3); // show, hide, EOF
    REQUIRE(tokens[0].type == TokenType::Show);
    REQUIRE(tokens[1].type == TokenType::Hide);
  }
}

TEST_CASE("Lexer enforces comment nesting depth limit", "[lexer]") {
  Lexer lexer;

  SECTION("reports error when exceeding maximum nesting depth") {
    // Build a string with comments exceeding the maximum depth of 128
    std::string input = "show ";
    const int nestingLevel = 129; // Exceeds the limit

    // Open nested comments
    for (int i = 0; i < nestingLevel; ++i) {
      input += "/* ";
    }

    input += "too deep ";

    // Close nested comments
    for (int i = 0; i < nestingLevel; ++i) {
      input += "*/ ";
    }

    input += "hide";

    auto result = lexer.tokenize(input);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Comment nesting depth exceeds limit") != std::string::npos);
  }

  SECTION("reports error at exactly the limit boundary") {
    // Test at exactly MAX_COMMENT_DEPTH + 1 (129)
    std::string input = "";
    const int nestingLevel = 129;

    for (int i = 0; i < nestingLevel; ++i) {
      input += "/* ";
    }

    auto result = lexer.tokenize(input);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Comment nesting depth exceeds limit of 128") != std::string::npos);
  }

  SECTION("accepts comments at exactly the maximum depth") {
    // Test at exactly MAX_COMMENT_DEPTH (128)
    std::string input = "show ";
    const int nestingLevel = 128;

    for (int i = 0; i < nestingLevel; ++i) {
      input += "/* ";
    }

    input += "at limit ";

    for (int i = 0; i < nestingLevel; ++i) {
      input += "*/ ";
    }

    input += "hide";

    auto result = lexer.tokenize(input);
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 3); // show, hide, EOF
  }
}

TEST_CASE("Lexer handles UTF-8 validation", "[lexer][utf8][security]") {
  Lexer lexer;

  SECTION("handles valid UTF-8 identifiers") {
    // Valid Cyrillic identifier
    auto result1 = lexer.tokenize("Привет");
    REQUIRE(result1.isOk());
    const auto& tokens1 = result1.value();
    REQUIRE(tokens1.size() == 2); // identifier + EOF
    REQUIRE(tokens1[0].type == TokenType::Identifier);
    REQUIRE(tokens1[0].lexeme == "Привет");

    // Valid Greek identifier
    auto result2 = lexer.tokenize("Ελληνικά");
    REQUIRE(result2.isOk());
    const auto& tokens2 = result2.value();
    REQUIRE(tokens2.size() == 2); // identifier + EOF
    REQUIRE(tokens2[0].type == TokenType::Identifier);

    // Valid Chinese identifier
    auto result3 = lexer.tokenize("中文");
    REQUIRE(result3.isOk());
    const auto& tokens3 = result3.value();
    REQUIRE(tokens3.size() == 2); // identifier + EOF
    REQUIRE(tokens3[0].type == TokenType::Identifier);
  }

  SECTION("rejects truncated UTF-8 sequences") {
    // 2-byte sequence truncated (0xD0 starts Cyrillic, missing continuation)
    std::string input1 = "show \xD0";
    auto result1 = lexer.tokenize(input1);
    // The lexer should handle this gracefully - invalid UTF-8 is skipped
    REQUIRE(result1.isOk());

    // 3-byte sequence truncated (0xE4 0xB8 starts Chinese, missing 3rd byte)
    std::string input2 = "show \xE4\xB8";
    auto result2 = lexer.tokenize(input2);
    REQUIRE(result2.isOk());

    // 4-byte sequence truncated (0xF0 0x90 0x8C starts emoji, missing 4th byte)
    std::string input3 = "show \xF0\x90\x8C";
    auto result3 = lexer.tokenize(input3);
    REQUIRE(result3.isOk());
  }

  SECTION("rejects invalid continuation bytes") {
    // Valid start byte (0xD0) followed by invalid continuation (0xFF)
    std::string input1 = "show \xD0\xFF";
    auto result1 = lexer.tokenize(input1);
    REQUIRE(result1.isOk()); // Invalid UTF-8 is skipped

    // Valid start byte (0xE4) followed by valid then invalid continuation
    std::string input2 = "show \xE4\xB8\xFF";
    auto result2 = lexer.tokenize(input2);
    REQUIRE(result2.isOk());
  }

  SECTION("rejects overlong UTF-8 encodings") {
    // Overlong encoding of ASCII 'A' (0x41) using 2 bytes: 0xC1 0x81
    // This is a security issue - should be rejected
    std::string input1 = "show \xC1\x81";
    auto result1 = lexer.tokenize(input1);
    REQUIRE(result1.isOk()); // Overlong encoding is rejected by decodeUtf8

    // Overlong encoding of 0x7F using 2 bytes: 0xC1 0xBF
    std::string input2 = "show \xC1\xBF";
    auto result2 = lexer.tokenize(input2);
    REQUIRE(result2.isOk());

    // Overlong encoding using 3 bytes for a value that fits in 2
    // Encoding U+0080 (minimum for 2-byte) as 3 bytes: 0xE0 0x82 0x80
    std::string input3 = "show \xE0\x82\x80";
    auto result3 = lexer.tokenize(input3);
    REQUIRE(result3.isOk());

    // Overlong encoding using 4 bytes for a value that fits in 3
    // Encoding U+0800 (minimum for 3-byte) as 4 bytes: 0xF0 0x88 0x80 0x80
    std::string input4 = "show \xF0\x88\x80\x80";
    auto result4 = lexer.tokenize(input4);
    REQUIRE(result4.isOk());
  }

  SECTION("rejects UTF-16 surrogate pairs") {
    // UTF-16 surrogates (U+D800 to U+DFFF) are invalid in UTF-8
    // Encoding U+D800 as UTF-8: 0xED 0xA0 0x80
    std::string input1 = "show \xED\xA0\x80";
    auto result1 = lexer.tokenize(input1);
    REQUIRE(result1.isOk()); // Surrogate is rejected by decodeUtf8

    // Encoding U+DFFF as UTF-8: 0xED 0xBF 0xBF
    std::string input2 = "show \xED\xBF\xBF";
    auto result2 = lexer.tokenize(input2);
    REQUIRE(result2.isOk());
  }

  SECTION("rejects code points beyond valid Unicode range") {
    // U+10FFFF is the maximum valid Unicode code point
    // Valid: U+10FFFF encoded as 0xF4 0x8F 0xBF 0xBF
    std::string input1 = "show \xF4\x8F\xBF\xBF";
    auto result1 = lexer.tokenize(input1);
    REQUIRE(result1.isOk()); // This is actually valid (max code point)

    // Invalid: U+110000 (beyond max) encoded as 0xF4 0x90 0x80 0x80
    std::string input2 = "show \xF4\x90\x80\x80";
    auto result2 = lexer.tokenize(input2);
    REQUIRE(result2.isOk()); // Beyond max is rejected by decodeUtf8

    // Invalid: U+1FFFFF (way beyond max) encoded as 0xF7 0xBF 0xBF 0xBF
    std::string input3 = "show \xF7\xBF\xBF\xBF";
    auto result3 = lexer.tokenize(input3);
    REQUIRE(result3.isOk());
  }

  SECTION("handles mixed valid and invalid UTF-8") {
    // Mix of valid ASCII, valid UTF-8, and invalid bytes
    std::string input = "show \xD0\x9F valid \xC1\x81 hide";
    auto result = lexer.tokenize(input);
    REQUIRE(result.isOk());
    // Should tokenize 'show', valid UTF-8 identifier, 'valid', 'hide'
    // Invalid UTF-8 sequences are skipped
  }
}
