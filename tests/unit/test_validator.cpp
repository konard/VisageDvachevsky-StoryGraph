#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/validator.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/lexer.hpp"

using namespace NovelMind::scripting;

// Helper to create a simple program for testing
[[maybe_unused]] static Program createTestProgram()
{
    Program program;

    // Add a character
    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Герой";
    hero.color = "#FFCC00";
    program.characters.push_back(hero);

    // Add another character
    CharacterDecl villain;
    villain.id = "Villain";
    villain.displayName = "Злодей";
    villain.color = "#FF0000";
    program.characters.push_back(villain);

    return program;
}

TEST_CASE("Validator - Empty program validates successfully", "[validator]")
{
    Validator validator;
    Program program;

    auto result = validator.validate(program);

    CHECK(result.isValid);
    CHECK_FALSE(result.errors.hasErrors());
}

TEST_CASE("Validator - Duplicate character definition reports error", "[validator]")
{
    Validator validator;
    Program program;

    CharacterDecl hero1;
    hero1.id = "Hero";
    hero1.displayName = "Hero 1";
    program.characters.push_back(hero1);

    CharacterDecl hero2;
    hero2.id = "Hero";
    hero2.displayName = "Hero 2";
    program.characters.push_back(hero2);

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundDuplicateError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::DuplicateCharacterDefinition)
        {
            foundDuplicateError = true;
            break;
        }
    }
    CHECK(foundDuplicateError);
}

TEST_CASE("Validator - Duplicate scene definition reports error", "[validator]")
{
    Validator validator;
    Program program;

    SceneDecl scene1;
    scene1.name = "intro";
    program.scenes.push_back(std::move(scene1));

    SceneDecl scene2;
    scene2.name = "intro";
    program.scenes.push_back(std::move(scene2));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundDuplicateError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::DuplicateSceneDefinition)
        {
            foundDuplicateError = true;
            break;
        }
    }
    CHECK(foundDuplicateError);
}

TEST_CASE("Validator - Empty scene reports warning", "[validator]")
{
    Validator validator;
    validator.setReportDeadCode(true);

    Program program;

    SceneDecl scene;
    scene.name = "empty_scene";
    // No body
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasWarnings());

    bool foundEmptySceneWarning = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::EmptyScene)
        {
            foundEmptySceneWarning = true;
            break;
        }
    }
    CHECK(foundEmptySceneWarning);
}

TEST_CASE("Validator - Undefined character in show statement reports error", "[validator]")
{
    Validator validator;
    Program program;

    SceneDecl scene;
    scene.name = "test_scene";

    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Character;
    showStmt.identifier = "UndefinedCharacter";
    showStmt.position = Position::Center;

    scene.body.push_back(makeStmt(showStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundUndefinedError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedCharacter)
        {
            foundUndefinedError = true;
            break;
        }
    }
    CHECK(foundUndefinedError);
}

TEST_CASE("Validator - Undefined scene in goto reports error", "[validator]")
{
    Validator validator;
    Program program;

    SceneDecl scene;
    scene.name = "test_scene";

    GotoStmt gotoStmt;
    gotoStmt.target = "nonexistent_scene";

    scene.body.push_back(makeStmt(gotoStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundUndefinedError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedScene)
        {
            foundUndefinedError = true;
            break;
        }
    }
    CHECK(foundUndefinedError);
}

TEST_CASE("Validator - Valid goto to defined scene passes", "[validator]")
{
    Validator validator;
    validator.setReportUnused(false);  // Don't report unused scenes

    Program program;

    SceneDecl scene1;
    scene1.name = "scene1";

    GotoStmt gotoStmt;
    gotoStmt.target = "scene2";
    scene1.body.push_back(makeStmt(gotoStmt));
    program.scenes.push_back(std::move(scene1));

    SceneDecl scene2;
    scene2.name = "scene2";
    SayStmt sayStmt;
    sayStmt.text = "Hello";
    scene2.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene2));

    auto result = validator.validate(program);

    // Should not have any undefined scene errors
    bool foundUndefinedSceneError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedScene)
        {
            foundUndefinedSceneError = true;
            break;
        }
    }
    CHECK_FALSE(foundUndefinedSceneError);
}

TEST_CASE("Validator - Unused character reports warning", "[validator]")
{
    Validator validator;
    validator.setReportUnused(true);

    Program program;

    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Hero";
    program.characters.push_back(hero);

    // Add a scene that doesn't use the character
    SceneDecl scene;
    scene.name = "test_scene";
    SayStmt sayStmt;
    sayStmt.text = "Hello";
    scene.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasWarnings());

    bool foundUnusedWarning = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UnusedCharacter)
        {
            foundUnusedWarning = true;
            break;
        }
    }
    CHECK(foundUnusedWarning);
}

TEST_CASE("Validator - Used character does not report warning", "[validator]")
{
    Validator validator;
    validator.setReportUnused(true);

    Program program;

    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Hero";
    program.characters.push_back(hero);

    SceneDecl scene;
    scene.name = "test_scene";

    // Use the character in a show statement
    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Character;
    showStmt.identifier = "Hero";
    showStmt.position = Position::Center;
    scene.body.push_back(makeStmt(showStmt));

    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    bool foundUnusedCharacterWarning = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UnusedCharacter)
        {
            foundUnusedCharacterWarning = true;
            break;
        }
    }
    CHECK_FALSE(foundUnusedCharacterWarning);
}

TEST_CASE("Validator - Empty choice block reports error", "[validator]")
{
    Validator validator;
    Program program;

    SceneDecl scene;
    scene.name = "test_scene";

    ChoiceStmt choiceStmt;
    // No options
    scene.body.push_back(makeStmt(std::move(choiceStmt)));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundEmptyChoiceError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::EmptyChoiceBlock)
        {
            foundEmptyChoiceError = true;
            break;
        }
    }
    CHECK(foundEmptyChoiceError);
}

TEST_CASE("Validator - Undefined speaker in say reports error", "[validator]")
{
    Validator validator;
    Program program;

    SceneDecl scene;
    scene.name = "test_scene";

    SayStmt sayStmt;
    sayStmt.speaker = "UndefinedSpeaker";
    sayStmt.text = "Hello";
    scene.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundUndefinedError = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedCharacter)
        {
            foundUndefinedError = true;
            break;
        }
    }
    CHECK(foundUndefinedError);
}

TEST_CASE("Validator - Valid program validates successfully", "[validator]")
{
    Validator validator;
    validator.setReportUnused(false);
    validator.setReportDeadCode(false);

    Program program;

    // Add character
    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Hero";
    program.characters.push_back(hero);

    // Add scene with valid statements
    SceneDecl scene;
    scene.name = "intro";

    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Character;
    showStmt.identifier = "Hero";
    showStmt.position = Position::Center;
    scene.body.push_back(makeStmt(showStmt));

    SayStmt sayStmt;
    sayStmt.speaker = "Hero";
    sayStmt.text = "Hello, world!";
    scene.body.push_back(makeStmt(sayStmt));

    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.isValid);
    CHECK_FALSE(result.errors.hasErrors());
}

TEST_CASE("ScriptError - Format includes severity and location", "[script_error]")
{
    ScriptError error(
        ErrorCode::UndefinedCharacter,
        Severity::Error,
        "Character 'Test' is not defined",
        SourceLocation(10, 5)
    );

    std::string formatted = error.format();

    CHECK(formatted.find("error") != std::string::npos);
    CHECK(formatted.find("10:5") != std::string::npos);
    CHECK(formatted.find("Character 'Test' is not defined") != std::string::npos);
}

TEST_CASE("ErrorList - Counts errors and warnings correctly", "[script_error]")
{
    ErrorList list;

    list.addError(ErrorCode::UndefinedCharacter, "Error 1", SourceLocation(1, 1));
    list.addWarning(ErrorCode::UnusedVariable, "Warning 1", SourceLocation(2, 1));
    list.addError(ErrorCode::UndefinedScene, "Error 2", SourceLocation(3, 1));
    list.addWarning(ErrorCode::UnusedCharacter, "Warning 2", SourceLocation(4, 1));
    list.addInfo(ErrorCode::DeadCode, "Info 1", SourceLocation(5, 1));

    CHECK(list.errorCount() == 2);
    CHECK(list.warningCount() == 2);
    CHECK(list.size() == 5);
    CHECK(list.hasErrors());
    CHECK(list.hasWarnings());
}

// =============================================================================
// New tests for enhanced error messages (Issue #234)
// =============================================================================

TEST_CASE("levenshteinDistance - Calculates edit distance correctly", "[script_error]")
{
    SECTION("identical strings have distance 0")
    {
        CHECK(levenshteinDistance("hello", "hello") == 0);
        CHECK(levenshteinDistance("", "") == 0);
        CHECK(levenshteinDistance("Hero", "Hero") == 0);
    }

    SECTION("empty string vs non-empty string")
    {
        CHECK(levenshteinDistance("", "hello") == 5);
        CHECK(levenshteinDistance("abc", "") == 3);
    }

    SECTION("single character difference")
    {
        CHECK(levenshteinDistance("cat", "bat") == 1);  // substitution
        CHECK(levenshteinDistance("cat", "cats") == 1); // insertion
        CHECK(levenshteinDistance("cats", "cat") == 1); // deletion
    }

    SECTION("multiple differences")
    {
        CHECK(levenshteinDistance("kitten", "sitting") == 3);
        CHECK(levenshteinDistance("Villain", "Villian") == 2); // transposition-like
        CHECK(levenshteinDistance("Hero", "Heroe") == 1);
    }
}

TEST_CASE("findSimilarStrings - Finds similar candidates", "[script_error]")
{
    std::vector<std::string> candidates = {"Hero", "Heroine", "Villain", "NPC"};

    SECTION("finds close matches within threshold")
    {
        auto similar = findSimilarStrings("Heroe", candidates, 2, 3);
        REQUIRE_FALSE(similar.empty());
        CHECK(similar[0] == "Hero"); // edit distance 1
    }

    SECTION("finds multiple matches sorted by distance")
    {
        auto similar = findSimilarStrings("Her", candidates, 3, 3);
        // Should find Hero (distance 1), Heroine (distance 4 - too far)
        REQUIRE_FALSE(similar.empty());
        CHECK(similar[0] == "Hero");
    }

    SECTION("returns empty for no matches within threshold")
    {
        auto similar = findSimilarStrings("XYZ", candidates, 2, 3);
        CHECK(similar.empty());
    }

    SECTION("common typo: Villian vs Villain")
    {
        auto similar = findSimilarStrings("Villian", candidates, 2, 3);
        REQUIRE_FALSE(similar.empty());
        CHECK(similar[0] == "Villain");
    }
}

TEST_CASE("extractSourceContext - Extracts code context around error", "[script_error]")
{
    std::string source = "character Hero(name=\"Hero\")\n"
                         "\n"
                         "scene intro {\n"
                         "    say Villian \"I am evil\"\n"
                         "}\n";

    SECTION("extracts lines around error location")
    {
        std::string context = extractSourceContext(source, 4, 9, 1);
        CHECK(context.find("say Villian") != std::string::npos);
        CHECK(context.find("^") != std::string::npos); // caret indicator
    }

    SECTION("handles line 1 errors")
    {
        std::string context = extractSourceContext(source, 1, 1, 2);
        CHECK(context.find("character Hero") != std::string::npos);
    }

    SECTION("handles empty source")
    {
        std::string context = extractSourceContext("", 1, 1);
        CHECK(context.empty());
    }

    SECTION("handles out of bounds line")
    {
        std::string context = extractSourceContext(source, 100, 1);
        CHECK(context.empty());
    }
}

TEST_CASE("ScriptError - Format with file path", "[script_error]")
{
    ScriptError error(
        ErrorCode::UndefinedCharacter,
        Severity::Error,
        "Undefined character 'Villian'",
        SourceLocation(23, 10)
    );
    error.withFilePath("scripts/intro.nms");

    std::string formatted = error.format();

    CHECK(formatted.find("scripts/intro.nms") != std::string::npos);
    CHECK(formatted.find("23:10") != std::string::npos);
    CHECK(formatted.find("E3001") != std::string::npos);
}

TEST_CASE("ScriptError - formatRich with full context", "[script_error]")
{
    std::string source = "character Villain(name=\"Evil\")\n"
                         "\n"
                         "scene intro {\n"
                         "    say Villian \"I am evil\"\n"
                         "}\n";

    ScriptError error(
        ErrorCode::UndefinedCharacter,
        Severity::Error,
        "Undefined character 'Villian'",
        SourceLocation(4, 9)
    );
    error.withFilePath("scripts/intro.nms");
    error.withSource(source);
    error.withSuggestion("Did you mean 'Villain'?");
    error.withRelated(SourceLocation(1, 11), "Villain was defined here");

    std::string rich = error.formatRich();

    // Check header
    CHECK(rich.find("error[E3001]") != std::string::npos);
    CHECK(rich.find("scripts/intro.nms") != std::string::npos);

    // Check source context
    CHECK(rich.find("say Villian") != std::string::npos);

    // Check suggestion
    CHECK(rich.find("Did you mean 'Villain'?") != std::string::npos);

    // Check related info
    CHECK(rich.find("Villain was defined here") != std::string::npos);

    // Check help URL
    CHECK(rich.find("https://docs.novelmind.dev/errors/E3001") != std::string::npos);
}

TEST_CASE("ScriptError - errorCodeString and helpUrl", "[script_error]")
{
    ScriptError error(
        ErrorCode::UndefinedCharacter,
        Severity::Error,
        "Test",
        SourceLocation(1, 1)
    );

    CHECK(error.errorCodeString() == "E3001");
    CHECK(error.helpUrl() == "https://docs.novelmind.dev/errors/E3001");
}

TEST_CASE("Validator - Undefined character provides suggestions", "[validator]")
{
    Validator validator;
    validator.setReportUnused(false);

    Program program;

    // Add a character "Villain"
    CharacterDecl villain;
    villain.id = "Villain";
    villain.displayName = "Evil One";
    program.characters.push_back(villain);

    // Add scene with typo "Villian"
    SceneDecl scene;
    scene.name = "test_scene";

    SayStmt sayStmt;
    sayStmt.speaker = "Villian"; // typo!
    sayStmt.text = "I am evil";
    scene.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    // Should have error for undefined character
    REQUIRE(result.errors.hasErrors());

    // Find the undefined character error
    bool foundSuggestion = false;
    for (const auto& err : result.errors.all()) {
        if (err.code == ErrorCode::UndefinedCharacter) {
            // Should suggest "Villain" as the correct spelling
            for (const auto& suggestion : err.suggestions) {
                if (suggestion.find("Villain") != std::string::npos) {
                    foundSuggestion = true;
                    break;
                }
            }
        }
    }
    CHECK(foundSuggestion);
}

// Tests for resource validation (scene objects and assets)

TEST_CASE("Validator - Missing scene file warning with callback", "[validator][resources]")
{
    Validator validator;

    // Set up callback that says scene file doesn't exist
    validator.setSceneFileExistsCallback([](const std::string& sceneId) {
        return false; // Scene file doesn't exist
    });

    Program program;

    SceneDecl scene;
    scene.name = "intro";
    SayStmt sayStmt;
    sayStmt.text = "Hello";
    scene.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    // Should have a warning about missing scene file
    bool foundMissingSceneFile = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::MissingSceneFile)
        {
            foundMissingSceneFile = true;
            CHECK(error.severity == Severity::Warning);
            break;
        }
    }
    CHECK(foundMissingSceneFile);
}

TEST_CASE("Validator - Missing scene object warning with callback", "[validator][resources]")
{
    Validator validator;

    // Set up callbacks
    validator.setSceneFileExistsCallback([](const std::string&) {
        return true; // Scene file exists
    });

    validator.setSceneObjectExistsCallback([](const std::string& sceneId, const std::string& objectId) {
        // "Hero" doesn't exist in "intro" scene
        if (sceneId == "intro" && objectId == "Hero") {
            return false;
        }
        return true;
    });

    Program program;

    // Define Hero character
    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Hero";
    program.characters.push_back(hero);

    // Create scene that tries to show Hero
    SceneDecl scene;
    scene.name = "intro";

    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Character;
    showStmt.identifier = "Hero";
    scene.body.push_back(makeStmt(showStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    // Should have a warning about missing scene object
    bool foundMissingSceneObject = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::MissingSceneObject)
        {
            foundMissingSceneObject = true;
            CHECK(error.severity == Severity::Warning);
            CHECK(error.message.find("Hero") != std::string::npos);
            CHECK(error.message.find("intro") != std::string::npos);
            break;
        }
    }
    CHECK(foundMissingSceneObject);
}

TEST_CASE("Validator - Missing asset file warning for background", "[validator][resources]")
{
    Validator validator;

    // Set up callback that says asset doesn't exist
    validator.setAssetFileExistsCallback([](const std::string& assetPath) {
        if (assetPath == "bg_city.png") {
            return false; // Asset doesn't exist
        }
        return true;
    });

    Program program;

    SceneDecl scene;
    scene.name = "intro";

    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Background;
    showStmt.resource = "bg_city.png";
    scene.body.push_back(makeStmt(showStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundSuggestion = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedCharacter)
        {
            // Check for "Did you mean" suggestion
            for (const auto& suggestion : error.suggestions)
            {
                if (suggestion.find("Villain") != std::string::npos)
                {
                    foundSuggestion = true;
                    break;
                }
            }
            break;
        }
    }
    CHECK(foundSuggestion);
}

TEST_CASE("Validator - Undefined scene provides suggestions", "[validator]")
{
    Validator validator;
    validator.setReportUnused(false);

    Program program;

    // Add first scene "intro"
    SceneDecl scene1;
    scene1.name = "intro";
    GotoStmt gotoStmt;
    gotoStmt.target = "introo"; // typo!
    scene1.body.push_back(makeStmt(gotoStmt));
    program.scenes.push_back(std::move(scene1));

    // Add target scene "intro" (but we go to "introo" - wrong)
    SceneDecl scene2;
    scene2.name = "chapter1";
    SayStmt sayStmt;
    sayStmt.text = "Hello";
    scene2.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene2));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    bool foundSuggestion = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedScene)
        {
            // Check for "Did you mean" suggestion
            for (const auto& suggestion : error.suggestions)
            {
                if (suggestion.find("intro") != std::string::npos)
                {
                    foundSuggestion = true;
                    break;
                }
            }
            break;
        }
    }
    CHECK(foundSuggestion);
}

TEST_CASE("Validator - Source and file path propagate to errors", "[validator]")
{
    Validator validator;
    validator.setSource("scene test { say Unknown \"hello\" }");
    validator.setFilePath("test.nms");

    Program program;

    SceneDecl scene;
    scene.name = "test";
    SayStmt sayStmt;
    sayStmt.speaker = "Unknown";
    sayStmt.text = "hello";
    scene.body.push_back(makeStmt(sayStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    // Should have error for undefined character
    REQUIRE(result.errors.hasErrors());

    // Check that source and file path are propagated to errors
    bool foundError = false;
    for (const auto& error : result.errors.all()) {
        if (error.code == ErrorCode::UndefinedCharacter) {
            CHECK(error.source == "scene test { say Unknown \"hello\" }");
            CHECK(error.filePath == "test.nms");
            foundError = true;
            break;
        }
    }
    CHECK(foundError);
}

TEST_CASE("Validator - Missing asset file warning for play music", "[validator][resources]")
{
    Validator validator;

    // Set up callback that says asset doesn't exist
    validator.setAssetFileExistsCallback([](const std::string& assetPath) {
        if (assetPath == "theme.wav") {
            return false; // Asset doesn't exist
        }
        return true;
    });

    Program program;

    SceneDecl scene;
    scene.name = "intro";

    PlayStmt playStmt;
    playStmt.type = PlayStmt::MediaType::Music;
    playStmt.resource = "theme.wav";
    scene.body.push_back(makeStmt(playStmt));
    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    // Should have a warning about missing asset file
    bool foundMissingAsset = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::MissingAssetFile)
        {
            foundMissingAsset = true;
            CHECK(error.severity == Severity::Warning);
            CHECK(error.message.find("theme.wav") != std::string::npos);
            break;
        }
    }
    CHECK(foundMissingAsset);
}

TEST_CASE("Validator - No warnings when resources exist", "[validator][resources]")
{
    Validator validator;

    // Set up callbacks that say resources exist
    validator.setSceneFileExistsCallback([](const std::string&) {
        return true;
    });

    validator.setSceneObjectExistsCallback([](const std::string&, const std::string&) {
        return true;
    });

    validator.setAssetFileExistsCallback([](const std::string&) {
        return true;
    });

    Program program;

    // Define Hero character
    CharacterDecl hero;
    hero.id = "Hero";
    hero.displayName = "Hero";
    program.characters.push_back(hero);

    // Create scene
    SceneDecl scene;
    scene.name = "intro";

    ShowStmt showStmt;
    showStmt.target = ShowStmt::Target::Character;
    showStmt.identifier = "Hero";
    scene.body.push_back(makeStmt(showStmt));

    ShowStmt bgStmt;
    bgStmt.target = ShowStmt::Target::Background;
    bgStmt.resource = "bg_city.png";
    scene.body.push_back(makeStmt(bgStmt));

    PlayStmt playStmt;
    playStmt.type = PlayStmt::MediaType::Music;
    playStmt.resource = "theme.wav";
    scene.body.push_back(makeStmt(playStmt));

    program.scenes.push_back(std::move(scene));

    auto result = validator.validate(program);

    CHECK(result.errors.hasErrors());

    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::UndefinedCharacter)
        {
            CHECK(error.filePath.has_value());
            CHECK(error.filePath.value() == "test.nms");
            CHECK(error.source.has_value());
            CHECK_FALSE(error.source.value().empty());
            break;
        }
    }
    // Should not have any resource warnings
    bool foundResourceWarning = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::MissingSceneFile ||
            error.code == ErrorCode::MissingSceneObject ||
            error.code == ErrorCode::MissingAssetFile)
        {
            foundResourceWarning = true;
            break;
        }
    }
    CHECK_FALSE(foundResourceWarning);
}
