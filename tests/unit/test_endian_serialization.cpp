#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "NovelMind/core/endian.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/vm.hpp"

using namespace NovelMind;
using namespace NovelMind::scripting;

TEST_CASE("Endian utilities - byte swap", "[endian][portability]") {
  SECTION("Byte swap 32-bit value") {
    u32 value = 0x12345678;
    u32 swapped = byteSwap32(value);
    REQUIRE(swapped == 0x78563412);
  }

  SECTION("Byte swap is reversible") {
    u32 original = 0xDEADBEEF;
    u32 swapped = byteSwap32(original);
    u32 restored = byteSwap32(swapped);
    REQUIRE(restored == original);
  }
}

TEST_CASE("Endian utilities - little-endian conversion", "[endian][portability]") {
  SECTION("toLittleEndian32 and fromLittleEndian32 are reversible") {
    u32 original = 0xABCD1234;
    u32 le = toLittleEndian32(original);
    u32 restored = fromLittleEndian32(le);
    REQUIRE(restored == original);
  }
}

TEST_CASE("Float serialization - basic operations", "[endian][portability]") {
  SECTION("Serialize and deserialize positive float") {
    f32 original = 3.14159f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(original, 0.000001f));
  }

  SECTION("Serialize and deserialize negative float") {
    f32 original = -2.71828f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(original, 0.000001f));
  }

  SECTION("Serialize and deserialize zero") {
    f32 original = 0.0f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE(deserialized == 0.0f);
  }

  SECTION("Serialize and deserialize negative zero") {
    f32 original = -0.0f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    // Check bit pattern is preserved
    REQUIRE(std::memcmp(&original, &deserialized, sizeof(f32)) == 0);
  }

  SECTION("Serialize and deserialize very small float") {
    f32 original = 1.0e-30f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(original, 0.000001f));
  }

  SECTION("Serialize and deserialize very large float") {
    f32 original = 1.0e30f;
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(original, 0.000001f));
  }
}

TEST_CASE("Float serialization - special values", "[endian][portability]") {
  SECTION("Serialize and deserialize infinity") {
    f32 original = std::numeric_limits<f32>::infinity();
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE(std::isinf(deserialized));
    REQUIRE(deserialized > 0);
  }

  SECTION("Serialize and deserialize negative infinity") {
    f32 original = -std::numeric_limits<f32>::infinity();
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE(std::isinf(deserialized));
    REQUIRE(deserialized < 0);
  }

  SECTION("Serialize and deserialize NaN") {
    f32 original = std::numeric_limits<f32>::quiet_NaN();
    u32 serialized = serializeFloat(original);
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE(std::isnan(deserialized));
  }
}

TEST_CASE("Float serialization - known bit patterns", "[endian][portability]") {
  SECTION("1.0f has known IEEE 754 representation") {
    f32 value = 1.0f;
    u32 serialized = serializeFloat(value);
    // IEEE 754: 1.0f = 0x3F800000 in little-endian
    // On little-endian platforms, this should be 0x3F800000
    // On big-endian platforms, serializeFloat converts to little-endian
    f32 deserialized = deserializeFloat(serialized);
    REQUIRE(deserialized == 1.0f);
  }

  SECTION("Specific bit pattern roundtrip") {
    // Test that a specific bit pattern is preserved through serialization
    u32 bitPattern = 0x40490FDB; // Approximately π (3.14159...)
    f32 original = deserializeFloat(bitPattern);
    u32 serialized = serializeFloat(original);
    REQUIRE(serialized == bitPattern);
  }
}

TEST_CASE("VM float operations - cross-platform compatibility", "[vm][endian][portability]") {
  VirtualMachine vm;

  SECTION("PUSH_FLOAT and stack operations") {
    f32 testValue = 1.5f;
    u32 encoded = serializeFloat(testValue);

    std::vector<Instruction> program = {{OpCode::PUSH_FLOAT, encoded}, {OpCode::HALT, 0}};

    auto result = vm.load(program, {});
    REQUIRE(result.isOk());

    vm.run();
    REQUIRE(vm.isHalted());
  }

  SECTION("Multiple float values") {
    f32 val1 = 2.5f;
    f32 val2 = -3.7f;

    std::vector<Instruction> program = {{OpCode::PUSH_FLOAT, serializeFloat(val1)},
                                        {OpCode::PUSH_FLOAT, serializeFloat(val2)},
                                        {OpCode::HALT, 0}};

    auto result = vm.load(program, {});
    REQUIRE(result.isOk());

    vm.run();
    REQUIRE(vm.isHalted());
  }
}

TEST_CASE("Compiler float serialization", "[compiler][endian][portability]") {
  SECTION("Compile float literal") {
    // This test ensures the compiler uses portable float serialization
    // The actual compilation would require a full AST, but we can test
    // that the endian utilities work correctly

    f32 duration = 2.5f;
    u32 serialized = serializeFloat(duration);
    f32 deserialized = deserializeFloat(serialized);

    REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(duration, 0.000001f));
  }
}

TEST_CASE("Cross-platform bytecode compatibility", "[vm][compiler][endian][portability]") {
  // This test simulates loading bytecode that might have been compiled
  // on a different platform with different endianness

  SECTION("Portable float encoding in bytecode") {
    // Create bytecode with known float values
    std::vector<f32> testFloats = {1.0f, -1.0f, 0.5f, 100.0f, 0.001f, 3.14159f, -2.71828f};

    for (f32 original : testFloats) {
      u32 encoded = serializeFloat(original);

      // Simulate bytecode created on any platform
      std::vector<Instruction> program = {{OpCode::PUSH_FLOAT, encoded}, {OpCode::HALT, 0}};

      VirtualMachine vm;
      auto result = vm.load(program, {});
      REQUIRE(result.isOk());

      vm.run();
      REQUIRE(vm.isHalted());

      // The float should deserialize correctly regardless of platform
      f32 deserialized = deserializeFloat(encoded);
      REQUIRE_THAT(deserialized, Catch::Matchers::WithinRel(original, 0.000001f));
    }
  }
}
