#include <catch2/catch_test_macros.hpp>
#include "NovelMind/platform/clipboard.hpp"

using namespace NovelMind;
using namespace NovelMind::platform;

TEST_CASE("Clipboard basic operations", "[clipboard]")
{
    auto clipboard = createClipboard();
    REQUIRE(clipboard != nullptr);
}

#ifdef NOVELMIND_HAS_SDL2
// These tests only work when SDL2 is available

TEST_CASE("Clipboard set and get text", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    // Set text
    auto setResult = clipboard->setText("Hello, World!");
    REQUIRE(setResult.isOk());

    // Get text
    auto getResult = clipboard->getText();
    REQUIRE(getResult.isOk());
    REQUIRE(getResult.value() == "Hello, World!");
}

TEST_CASE("Clipboard has text", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    // Set text
    clipboard->setText("Test");

    // Check if has text
    REQUIRE(clipboard->hasText() == true);
}

TEST_CASE("Clipboard empty text", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    // Set empty text
    auto result = clipboard->setText("");
    REQUIRE(result.isOk());

    // Empty clipboard should still have text (empty string)
    REQUIRE(clipboard->hasText() == true);
}

TEST_CASE("Clipboard special characters", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    std::string specialText = "Test\nwith\nnewlines\tand\ttabs!@#$%^&*()";
    auto setResult = clipboard->setText(specialText);
    REQUIRE(setResult.isOk());

    auto getResult = clipboard->getText();
    REQUIRE(getResult.isOk());
    REQUIRE(getResult.value() == specialText);
}

TEST_CASE("Clipboard unicode text", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    std::string unicodeText = "Hello 世界 мир 🌍";
    auto setResult = clipboard->setText(unicodeText);
    REQUIRE(setResult.isOk());

    auto getResult = clipboard->getText();
    REQUIRE(getResult.isOk());
    REQUIRE(getResult.value() == unicodeText);
}

TEST_CASE("Clipboard overwrite text", "[clipboard][sdl]")
{
    auto clipboard = createClipboard();

    clipboard->setText("First");
    clipboard->setText("Second");

    auto result = clipboard->getText();
    REQUIRE(result.isOk());
    REQUIRE(result.value() == "Second");
}

#else
// Tests for null clipboard (when SDL2 is not available)

TEST_CASE("Null clipboard set text fails", "[clipboard][null]")
{
    auto clipboard = createClipboard();

    auto result = clipboard->setText("Test");
    REQUIRE(result.isError());
}

TEST_CASE("Null clipboard get text fails", "[clipboard][null]")
{
    auto clipboard = createClipboard();

    auto result = clipboard->getText();
    REQUIRE(result.isError());
}

TEST_CASE("Null clipboard has no text", "[clipboard][null]")
{
    auto clipboard = createClipboard();

    REQUIRE(clipboard->hasText() == false);
}

#endif // NOVELMIND_HAS_SDL2
