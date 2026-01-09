#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"

using namespace NovelMind::scripting;

TEST_CASE("Parser parses character declarations", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses simple character declaration")
    {
        auto tokens = lexer.tokenize("character Hero");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.characters.size() == 1);
        REQUIRE(program.characters[0].id == "Hero");
    }

    SECTION("parses character with properties")
    {
        auto tokens = lexer.tokenize(R"(character Hero(name="Alex", color="#FFCC00"))");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.characters.size() == 1);
        REQUIRE(program.characters[0].id == "Hero");
        REQUIRE(program.characters[0].displayName == "Alex");
        REQUIRE(program.characters[0].color == "#FFCC00");
    }
}

TEST_CASE("Parser parses scene declarations", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses empty scene")
    {
        auto tokens = lexer.tokenize("scene intro { }");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.scenes.size() == 1);
        REQUIRE(program.scenes[0].name == "intro");
        REQUIRE(program.scenes[0].body.empty());
    }

    SECTION("parses scene with statements")
    {
        auto tokens = lexer.tokenize(R"(
            scene intro {
                show background "bg_city"
                say Hero "Hello!"
            }
        )");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.scenes.size() == 1);
        REQUIRE(program.scenes[0].body.size() == 2);
    }
}

TEST_CASE("Parser parses show statements", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses show background")
    {
        auto tokens = lexer.tokenize(R"(show background "bg_city")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<ShowStmt>(stmt->data));

        const auto& show = std::get<ShowStmt>(stmt->data);
        REQUIRE(show.target == ShowStmt::Target::Background);
        REQUIRE(show.resource.value() == "bg_city");
    }

    SECTION("parses show character at position")
    {
        auto tokens = lexer.tokenize("show Hero at center");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<ShowStmt>(stmt->data));

        const auto& show = std::get<ShowStmt>(stmt->data);
        REQUIRE(show.target == ShowStmt::Target::Character);
        REQUIRE(show.identifier == "Hero");
        REQUIRE(show.position.value() == Position::Center);
    }
}

TEST_CASE("Parser parses say statements", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses say with speaker")
    {
        auto tokens = lexer.tokenize(R"(say Hero "Hello, world!")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<SayStmt>(stmt->data));

        const auto& say = std::get<SayStmt>(stmt->data);
        REQUIRE(say.speaker.value() == "Hero");
        REQUIRE(say.text == "Hello, world!");
    }

    SECTION("parses narrator say (no speaker)")
    {
        auto tokens = lexer.tokenize(R"(say "This is narration.")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<SayStmt>(stmt->data));

        const auto& say = std::get<SayStmt>(stmt->data);
        REQUIRE(!say.speaker.has_value());
        REQUIRE(say.text == "This is narration.");
    }

    SECTION("parses shorthand say syntax")
    {
        auto tokens = lexer.tokenize(R"(Hero "Quick dialogue")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<SayStmt>(stmt->data));

        const auto& say = std::get<SayStmt>(stmt->data);
        REQUIRE(say.speaker.value() == "Hero");
        REQUIRE(say.text == "Quick dialogue");
    }
}

TEST_CASE("Parser parses control flow", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses if statement")
    {
        auto tokens = lexer.tokenize(R"(
            if flag_met_hero {
                show Hero at center
            }
        )");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<IfStmt>(stmt->data));

        const auto& ifStmt = std::get<IfStmt>(stmt->data);
        REQUIRE(ifStmt.thenBranch.size() == 1);
        REQUIRE(ifStmt.elseBranch.empty());
    }

    SECTION("parses if-else statement")
    {
        auto tokens = lexer.tokenize(R"(
            if flag_met_hero {
                show Hero at center
            } else {
                show Stranger at center
            }
        )");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<IfStmt>(stmt->data));

        const auto& ifStmt = std::get<IfStmt>(stmt->data);
        REQUIRE(ifStmt.thenBranch.size() == 1);
        REQUIRE(ifStmt.elseBranch.size() == 1);
    }

    SECTION("parses goto statement")
    {
        auto tokens = lexer.tokenize("goto next_scene");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<GotoStmt>(stmt->data));

        const auto& gotoStmt = std::get<GotoStmt>(stmt->data);
        REQUIRE(gotoStmt.target == "next_scene");
    }
}

TEST_CASE("Parser parses expressions", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses set statement with expression")
    {
        auto tokens = lexer.tokenize("set counter = 10");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<SetStmt>(stmt->data));

        const auto& setStmt = std::get<SetStmt>(stmt->data);
        REQUIRE(setStmt.variable == "counter");
    }

    SECTION("parses comparison expressions")
    {
        auto tokens = lexer.tokenize("if counter > 5 { }");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());
    }

    SECTION("parses boolean expressions")
    {
        auto tokens = lexer.tokenize("if flag1 and flag2 { }");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());
    }
}

TEST_CASE("Parser parses choice blocks", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses choice with goto")
    {
        auto tokens = lexer.tokenize(R"(
            choice {
                "Go left" -> goto left_path
                "Go right" -> goto right_path
            }
        )");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<ChoiceStmt>(stmt->data));

        const auto& choice = std::get<ChoiceStmt>(stmt->data);
        REQUIRE(choice.options.size() == 2);
        REQUIRE(choice.options[0].text == "Go left");
        REQUIRE(choice.options[0].gotoTarget.value() == "left_path");
        REQUIRE(choice.options[1].text == "Go right");
        REQUIRE(choice.options[1].gotoTarget.value() == "right_path");
    }
}

TEST_CASE("Parser parses audio commands", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses play sound")
    {
        auto tokens = lexer.tokenize(R"(play sound "click.ogg")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<PlayStmt>(stmt->data));

        const auto& play = std::get<PlayStmt>(stmt->data);
        REQUIRE(play.type == PlayStmt::MediaType::Sound);
        REQUIRE(play.resource == "click.ogg");
    }

    SECTION("parses play music")
    {
        auto tokens = lexer.tokenize(R"(play music "bgm.ogg")");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<PlayStmt>(stmt->data));

        const auto& play = std::get<PlayStmt>(stmt->data);
        REQUIRE(play.type == PlayStmt::MediaType::Music);
    }

    SECTION("parses stop music")
    {
        auto tokens = lexer.tokenize("stop music");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<StopStmt>(stmt->data));
    }
}

TEST_CASE("Parser parses wait and transition", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses wait statement")
    {
        auto tokens = lexer.tokenize("wait 2.5");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<WaitStmt>(stmt->data));

        const auto& wait = std::get<WaitStmt>(stmt->data);
        REQUIRE(wait.duration == Catch::Approx(2.5f));
    }

    SECTION("parses transition statement")
    {
        auto tokens = lexer.tokenize("transition fade 1.0");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<TransitionStmt>(stmt->data));

        const auto& trans = std::get<TransitionStmt>(stmt->data);
        REQUIRE(trans.type == "fade");
        REQUIRE(trans.duration == Catch::Approx(1.0f));
    }
}

TEST_CASE("Parser parses move statements", "[parser]")
{
    Lexer lexer;
    Parser parser;

    SECTION("parses move to left")
    {
        auto tokens = lexer.tokenize("move Hero to left");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.characterId == "Hero");
        REQUIRE(move.position == Position::Left);
        REQUIRE(move.duration == Catch::Approx(0.5f)); // Default duration
    }

    SECTION("parses move to center")
    {
        auto tokens = lexer.tokenize("move Alice to center");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.characterId == "Alice");
        REQUIRE(move.position == Position::Center);
    }

    SECTION("parses move to right")
    {
        auto tokens = lexer.tokenize("move Bob to right");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.characterId == "Bob");
        REQUIRE(move.position == Position::Right);
    }

    SECTION("parses move with duration")
    {
        auto tokens = lexer.tokenize("move Hero to left duration=0.5");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.characterId == "Hero");
        REQUIRE(move.position == Position::Left);
        REQUIRE(move.duration == Catch::Approx(0.5f));
    }

    SECTION("parses move with different duration")
    {
        auto tokens = lexer.tokenize("move Alice to center duration=1.5");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.characterId == "Alice");
        REQUIRE(move.position == Position::Center);
        REQUIRE(move.duration == Catch::Approx(1.5f));
    }

    SECTION("parses move with integer duration")
    {
        auto tokens = lexer.tokenize("move Hero to right duration=2");
        REQUIRE(tokens.isOk());

        auto result = parser.parse(tokens.value());
        REQUIRE(result.isOk());

        const auto& program = result.value();
        REQUIRE(program.globalStatements.size() == 1);

        const auto& stmt = program.globalStatements[0];
        REQUIRE(std::holds_alternative<MoveStmt>(stmt->data));

        const auto& move = std::get<MoveStmt>(stmt->data);
        REQUIRE(move.duration == Catch::Approx(2.0f));
    }
}
