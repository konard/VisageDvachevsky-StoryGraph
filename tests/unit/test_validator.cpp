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

    // Should have a warning about missing asset file
    bool foundMissingAsset = false;
    for (const auto& error : result.errors.all())
    {
        if (error.code == ErrorCode::MissingAssetFile)
        {
            foundMissingAsset = true;
            CHECK(error.severity == Severity::Warning);
            CHECK(error.message.find("bg_city.png") != std::string::npos);
            break;
        }
    }
    CHECK(foundMissingAsset);
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
