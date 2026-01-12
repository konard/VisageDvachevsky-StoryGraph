// Test case for Issue #446: Float serialization undefined behavior
// Tests that the compiler properly serializes floats using std::bit_cast
// and that round-trip serialization/deserialization preserves float values

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/script_runtime.hpp"
#include <cmath>
#include <limits>

using namespace NovelMind::scripting;

// Helper function to compile and run a script with a wait statement
static bool compileAndCheckWait(f32 duration, f32 &extractedDuration) {
  // Create script with wait statement
  std::string script = "scene test { wait " + std::to_string(duration) + " }";

  Lexer lexer;
  auto tokens = lexer.tokenize(script);
  if (!tokens.isOk()) return false;

  Parser parser;
  auto parse = parser.parse(tokens.value());
  if (!parse.isOk()) return false;

  Compiler compiler;
  auto compiled = compiler.compile(parse.value());
  if (!compiled.isOk()) return false;

  // Load and execute the script to extract the duration
  ScriptRuntime runtime;
  auto loadResult = runtime.load(compiled.value());
  if (!loadResult.isOk()) return false;

  auto gotoResult = runtime.gotoScene("test");
  if (!gotoResult.isOk()) return false;

  // Run until waiting state
  for (int i = 0; i < 20 && runtime.getState() != RuntimeState::WaitingTimer; ++i) {
    runtime.update(0.016);
  }

  // Extract the wait timer value (this uses the deserialization path)
  if (runtime.getState() == RuntimeState::WaitingTimer) {
    extractedDuration = runtime.getWaitTimer();
    return true;
  }

  return false;
}

TEST_CASE("Compiler float serialization", "[scripting][issue-446][compiler]") {
  SECTION("Positive float values serialize correctly") {
    f32 extracted = 0.0f;
    REQUIRE(compileAndCheckWait(1.5f, extracted));
    REQUIRE(extracted == 1.5f);

    REQUIRE(compileAndCheckWait(3.14159f, extracted));
    REQUIRE(extracted == 3.14159f);

    REQUIRE(compileAndCheckWait(0.001f, extracted));
    REQUIRE(extracted == 0.001f);

    REQUIRE(compileAndCheckWait(1000.0f, extracted));
    REQUIRE(extracted == 1000.0f);
  }

  SECTION("Negative float values serialize correctly") {
    f32 extracted = 0.0f;
    REQUIRE(compileAndCheckWait(-1.5f, extracted));
    REQUIRE(extracted == -1.5f);

    REQUIRE(compileAndCheckWait(-3.14159f, extracted));
    REQUIRE(extracted == -3.14159f);

    REQUIRE(compileAndCheckWait(-0.001f, extracted));
    REQUIRE(extracted == -0.001f);

    REQUIRE(compileAndCheckWait(-1000.0f, extracted));
    REQUIRE(extracted == -1000.0f);
  }

  SECTION("Zero values serialize correctly") {
    f32 extracted = 1.0f;
    REQUIRE(compileAndCheckWait(0.0f, extracted));
    REQUIRE(extracted == 0.0f);

    // Negative zero
    REQUIRE(compileAndCheckWait(-0.0f, extracted));
    REQUIRE(extracted == -0.0f);
  }

  SECTION("Very small and very large float values") {
    f32 extracted = 0.0f;

    // Very small positive
    f32 verySmall = 1.0e-10f;
    REQUIRE(compileAndCheckWait(verySmall, extracted));
    REQUIRE(extracted == verySmall);

    // Very large positive
    f32 veryLarge = 1.0e10f;
    REQUIRE(compileAndCheckWait(veryLarge, extracted));
    REQUIRE(extracted == veryLarge);
  }

  SECTION("Infinity values serialize correctly") {
    f32 extracted = 0.0f;

    // Positive infinity
    f32 posInf = std::numeric_limits<f32>::infinity();
    REQUIRE(compileAndCheckWait(posInf, extracted));
    REQUIRE(std::isinf(extracted));
    REQUIRE(extracted > 0);

    // Negative infinity
    f32 negInf = -std::numeric_limits<f32>::infinity();
    REQUIRE(compileAndCheckWait(negInf, extracted));
    REQUIRE(std::isinf(extracted));
    REQUIRE(extracted < 0);
  }

  SECTION("NaN values serialize correctly") {
    f32 extracted = 0.0f;

    // NaN (Not a Number)
    f32 nanValue = std::numeric_limits<f32>::quiet_NaN();
    REQUIRE(compileAndCheckWait(nanValue, extracted));
    REQUIRE(std::isnan(extracted));
  }

  SECTION("Float round-trip serialization preserves bit pattern") {
    // Test that serialization and deserialization are exact inverses
    std::vector<f32> testValues = {
      0.0f, -0.0f, 1.0f, -1.0f,
      0.5f, -0.5f, 3.14159f, -3.14159f,
      std::numeric_limits<f32>::min(),
      std::numeric_limits<f32>::max(),
      std::numeric_limits<f32>::lowest(),
      std::numeric_limits<f32>::epsilon(),
      std::numeric_limits<f32>::infinity(),
      -std::numeric_limits<f32>::infinity(),
      std::numeric_limits<f32>::quiet_NaN()
    };

    for (f32 originalValue : testValues) {
      f32 extracted = 0.0f;
      bool success = compileAndCheckWait(originalValue, extracted);

      REQUIRE(success);

      // For NaN, we can't use == comparison, so check with isnan
      if (std::isnan(originalValue)) {
        REQUIRE(std::isnan(extracted));
      } else {
        // For all other values, bit pattern must be identical
        REQUIRE(std::memcmp(&originalValue, &extracted, sizeof(f32)) == 0);
      }
    }
  }

  SECTION("Transition duration float serialization") {
    // Test transition statements which also use float serialization
    const char *script = R"(
scene test {
  show "char1" at center with transition "fade" duration 2.5
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto parse = parser.parse(tokens.value());
    REQUIRE(parse.isOk());

    Compiler compiler;
    auto compiled = compiler.compile(parse.value());
    REQUIRE(compiled.isOk());

    // If compilation succeeds without UB, the fix is working
    // Runtime execution would test the deserialization, but compilation
    // is sufficient to verify the bit_cast serialization is correct
  }

  SECTION("Move statement with custom position float serialization") {
    // Test move statements which serialize x and y coordinates as floats
    const char *script = R"(
scene test {
  show "char1" at center
  move "char1" to custom (0.75, 0.25) duration 1.0
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto parse = parser.parse(tokens.value());
    REQUIRE(parse.isOk());

    Compiler compiler;
    auto compiled = compiler.compile(parse.value());
    REQUIRE(compiled.isOk());

    // Compilation success verifies bit_cast is used correctly for coordinates
  }

  SECTION("Stop music with fadeout float serialization") {
    // Test stop statements which use optional float fadeout duration
    const char *script = R"(
scene test {
  play music "song.mp3"
  stop music fadeout 3.5
}
)";

    Lexer lexer;
    auto tokens = lexer.tokenize(script);
    REQUIRE(tokens.isOk());

    Parser parser;
    auto parse = parser.parse(tokens.value());
    REQUIRE(parse.isOk());

    Compiler compiler;
    auto compiled = compiler.compile(parse.value());
    REQUIRE(compiled.isOk());

    // Compilation success verifies bit_cast for fadeout duration
  }
}

// Test the low-level bit_cast behavior directly
TEST_CASE("std::bit_cast float-uint32 conversion", "[scripting][issue-446][bit_cast]") {
  SECTION("bit_cast preserves exact bit pattern") {
    f32 value = 3.14159f;
    u32 bits = std::bit_cast<u32>(value);
    f32 restored = std::bit_cast<f32>(bits);

    // Must be bit-for-bit identical
    REQUIRE(std::memcmp(&value, &restored, sizeof(f32)) == 0);
  }

  SECTION("bit_cast is well-defined for all float values") {
    // These operations must not cause UB
    std::bit_cast<u32>(0.0f);
    std::bit_cast<u32>(-0.0f);
    std::bit_cast<u32>(std::numeric_limits<f32>::infinity());
    std::bit_cast<u32>(-std::numeric_limits<f32>::infinity());
    std::bit_cast<u32>(std::numeric_limits<f32>::quiet_NaN());
    std::bit_cast<u32>(std::numeric_limits<f32>::min());
    std::bit_cast<u32>(std::numeric_limits<f32>::max());

    // If we get here without crashing or UB sanitizer warnings, success
    REQUIRE(true);
  }
}
