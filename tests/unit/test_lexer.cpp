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

TEST_CASE("Lexer handles Unicode identifiers from various scripts", "[lexer][unicode]") {
  Lexer lexer;

  SECTION("tokenizes Cyrillic identifiers") {
    // Russian: переменная (variable)
    auto result = lexer.tokenize("переменная");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2); // identifier + EOF
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "переменная");
  }

  SECTION("tokenizes Greek identifiers") {
    // Greek: μεταβλητή (variable)
    auto result = lexer.tokenize("μεταβλητή");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "μεταβλητή");
  }

  SECTION("tokenizes Arabic identifiers") {
    // Arabic: متغير (variable)
    auto result = lexer.tokenize("متغير");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "متغير");
  }

  SECTION("tokenizes Hebrew identifiers") {
    // Hebrew: משתנה (variable)
    auto result = lexer.tokenize("משתנה");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "משתנה");
  }

  SECTION("tokenizes Devanagari identifiers") {
    // Hindi: चर (variable)
    auto result = lexer.tokenize("चर");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "चर");
  }

  SECTION("tokenizes Tamil identifiers") {
    // Tamil: மாறி (variable)
    auto result = lexer.tokenize("மாறி");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "மாறி");
  }

  SECTION("tokenizes Thai identifiers") {
    // Thai: ตัวแปร (variable)
    auto result = lexer.tokenize("ตัวแปร");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ตัวแปร");
  }

  SECTION("tokenizes Chinese identifiers") {
    // Chinese: 变量 (variable)
    auto result = lexer.tokenize("变量");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "变量");
  }

  SECTION("tokenizes Japanese Hiragana identifiers") {
    // Japanese: へんすう (variable in hiragana)
    auto result = lexer.tokenize("へんすう");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "へんすう");
  }

  SECTION("tokenizes Japanese Katakana identifiers") {
    // Japanese: ヘンスウ (variable in katakana)
    auto result = lexer.tokenize("ヘンスウ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ヘンスウ");
  }

  SECTION("tokenizes Korean identifiers") {
    // Korean: 변수 (variable)
    auto result = lexer.tokenize("변수");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "변수");
  }

  SECTION("tokenizes Armenian identifiers") {
    // Armenian: փոփոխական (variable)
    auto result = lexer.tokenize("փոփոխական");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "փոփոխական");
  }

  SECTION("tokenizes Georgian identifiers") {
    // Georgian: ცვლადი (variable)
    auto result = lexer.tokenize("ცვლადი");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ცვლადი");
  }

  SECTION("tokenizes Ethiopic identifiers") {
    // Ethiopic (Amharic): ተለዋዋጭ (variable)
    auto result = lexer.tokenize("ተለዋዋጭ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ተለዋዋጭ");
  }

  SECTION("tokenizes Cherokee identifiers") {
    // Cherokee: ᎠᏍᎦᏯ (word)
    auto result = lexer.tokenize("ᎠᏍᎦᏯ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ᎠᏍᎦᏯ");
  }

  SECTION("tokenizes mixed script identifiers") {
    // Mix of English and Cyrillic
    auto result = lexer.tokenize("myПеременная");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "myПеременная");
  }

  SECTION("tokenizes identifiers with non-ASCII digits") {
    // Arabic-Indic digits: متغير١٢٣
    auto result = lexer.tokenize("متغير١٢٣");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "متغير١٢٣");
  }

  SECTION("tokenizes identifiers with Devanagari digits") {
    // Devanagari with Devanagari digits: चर१२३
    auto result = lexer.tokenize("चर१२३");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "चर१२३");
  }

  SECTION("tokenizes identifiers with combining marks") {
    // Latin with combining diacritical marks: café
    auto result = lexer.tokenize("café");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "café");
  }

  SECTION("tokenizes Bengali identifiers") {
    // Bengali: ভেরিয়েবল (variable)
    auto result = lexer.tokenize("ভেরিয়েবল");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ভেরিয়েবল");
  }

  SECTION("tokenizes Telugu identifiers") {
    // Telugu: వేరియబుల్ (variable)
    auto result = lexer.tokenize("వేరియబుల్");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "వేరియబుల్");
  }

  SECTION("tokenizes Kannada identifiers") {
    // Kannada: ವೇರಿಯಬಲ್ (variable)
    auto result = lexer.tokenize("ವೇರಿಯಬಲ್");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ವೇರಿಯಬಲ್");
  }

  SECTION("tokenizes Malayalam identifiers") {
    // Malayalam: വേരിയബിൾ (variable)
    auto result = lexer.tokenize("വേരിയബിൾ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "വേരിയബിൾ");
  }

  SECTION("tokenizes Gujarati identifiers") {
    // Gujarati: ચલ (variable)
    auto result = lexer.tokenize("ચલ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ચલ");
  }

  SECTION("tokenizes Khmer identifiers") {
    // Khmer: អថេរ (variable)
    auto result = lexer.tokenize("អថេរ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "អថេរ");
  }

  SECTION("tokenizes Lao identifiers") {
    // Lao: ຕົວແປ (variable)
    auto result = lexer.tokenize("ຕົວແປ");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "ຕົວແປ");
  }

  SECTION("tokenizes Tibetan identifiers") {
    // Tibetan: འགྱུར་ཅན (variable)
    auto result = lexer.tokenize("འགྱུར་ཅན");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "འགྱུར་ཅན");
  }

  SECTION("tokenizes multiple Unicode identifiers in a script") {
    // Multiple identifiers: переменная число строка
    auto result = lexer.tokenize("переменная число строка");
    REQUIRE(result.isOk());
    const auto& tokens = result.value();
    REQUIRE(tokens.size() == 4); // 3 identifiers + EOF
    REQUIRE(tokens[0].type == TokenType::Identifier);
    REQUIRE(tokens[0].lexeme == "переменная");
    REQUIRE(tokens[1].type == TokenType::Identifier);
    REQUIRE(tokens[1].lexeme == "число");
    REQUIRE(tokens[2].type == TokenType::Identifier);
    REQUIRE(tokens[2].lexeme == "строка");
  }
}
