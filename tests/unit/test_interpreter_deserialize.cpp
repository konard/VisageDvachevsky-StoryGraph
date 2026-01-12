#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/interpreter.hpp"
#include <limits>
#include <cstring>

using namespace NovelMind::scripting;

// Helper function to create a bytecode header
std::vector<NovelMind::u8> createBytecodeHeader(
    NovelMind::u32 magic,
    NovelMind::u16 version,
    NovelMind::u16 flags,
    NovelMind::u32 instrCount,
    NovelMind::u32 constPoolSize,
    NovelMind::u32 stringCount,
    NovelMind::u32 symbolTableSize)
{
    std::vector<NovelMind::u8> bytecode(24); // Header size
    NovelMind::usize offset = 0;

    // Magic
    std::memcpy(bytecode.data() + offset, &magic, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    // Version
    std::memcpy(bytecode.data() + offset, &version, sizeof(NovelMind::u16));
    offset += sizeof(NovelMind::u16);

    // Flags
    std::memcpy(bytecode.data() + offset, &flags, sizeof(NovelMind::u16));
    offset += sizeof(NovelMind::u16);

    // Instruction count
    std::memcpy(bytecode.data() + offset, &instrCount, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    // Constant pool size
    std::memcpy(bytecode.data() + offset, &constPoolSize, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    // String count
    std::memcpy(bytecode.data() + offset, &stringCount, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    // Symbol table size
    std::memcpy(bytecode.data() + offset, &symbolTableSize, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    return bytecode;
}

// Helper function to append an instruction to bytecode
void appendInstruction(std::vector<NovelMind::u8> &bytecode, NovelMind::u8 opcode, NovelMind::u32 operand)
{
    bytecode.push_back(opcode);
    NovelMind::u8 operandBytes[4];
    std::memcpy(operandBytes, &operand, sizeof(NovelMind::u32));
    bytecode.insert(bytecode.end(), operandBytes, operandBytes + 4);
}

TEST_CASE("Interpreter deserialize max int - valid case", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    // Create valid bytecode with maximum valid instruction count
    constexpr NovelMind::u32 MAX_INSTRUCTION_COUNT = 10000000;
    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E; // "NMSC"

    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,      // version
        0,      // flags
        MAX_INSTRUCTION_COUNT,  // Maximum allowed instruction count
        0,      // constPoolSize
        0,      // stringCount
        0       // symbolTableSize
    );

    // Append the required number of instructions
    for (NovelMind::u32 i = 0; i < MAX_INSTRUCTION_COUNT; ++i) {
        appendInstruction(bytecode, 0x00, 0); // NOP instruction
    }

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isOk());
}

TEST_CASE("Interpreter deserialize overflow - instruction count exceeds max", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    constexpr NovelMind::u32 MAX_INSTRUCTION_COUNT = 10000000;

    // Try to load bytecode with instruction count > MAX_INSTRUCTION_COUNT
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        MAX_INSTRUCTION_COUNT + 1,  // Exceeds maximum
        0,
        0,
        0
    );

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("exceeds maximum") != std::string::npos);
}

TEST_CASE("Interpreter deserialize overflow - instruction count would overflow multiplication", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;

    // Use a value that would cause overflow when multiplied by INSTRUCTION_SIZE (5)
    // We want: instrCount * 5 > usize_max
    // So: instrCount > usize_max / 5
    NovelMind::usize maxSafeSize = std::numeric_limits<NovelMind::usize>::max() / 5;
    NovelMind::u32 dangerousInstrCount = std::numeric_limits<NovelMind::u32>::max();

    // If u32 max is larger than maxSafeSize, use it to trigger overflow
    if (static_cast<NovelMind::usize>(dangerousInstrCount) > maxSafeSize) {
        auto bytecode = createBytecodeHeader(
            SCRIPT_MAGIC,
            1,
            0,
            dangerousInstrCount,
            0,
            0,
            0
        );

        auto result = interpreter.loadFromBytecode(bytecode);
        REQUIRE(result.isError());
        REQUIRE(result.error().find("overflow") != std::string::npos);
    }
}

TEST_CASE("Interpreter deserialize overflow - addition overflow check", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;

    // Create bytecode with instruction count that would cause addition overflow
    // We need: offset + (instrCount * 5) to overflow
    // Header is 24 bytes, so offset starts at 24
    // We want a large instrCount but not so large it triggers the multiplication overflow check

    NovelMind::usize maxSafeSize = std::numeric_limits<NovelMind::usize>::max() / 5;
    NovelMind::u32 largeButSafeInstrCount = static_cast<NovelMind::u32>(
        std::min(maxSafeSize - 1, static_cast<NovelMind::usize>(std::numeric_limits<NovelMind::u32>::max()))
    );

    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        largeButSafeInstrCount,
        0,
        0,
        0
    );

    // Don't append instructions - bytecode will be too small
    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("too small") != std::string::npos);
}

TEST_CASE("Interpreter deserialize overflow - string count exceeds max", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    constexpr NovelMind::u32 MAX_STRING_COUNT = 10000000;

    // Try to load bytecode with string count > MAX_STRING_COUNT
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        0,  // No instructions
        0,
        MAX_STRING_COUNT + 1,  // Exceeds maximum
        0
    );

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("exceeds maximum") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - bytecode truncated at magic", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    // Create bytecode that's too small to contain the magic number
    std::vector<NovelMind::u8> bytecode = {0x4E, 0x4D};  // Only 2 bytes

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("truncated") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - bytecode truncated at version", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    std::vector<NovelMind::u8> bytecode(4);
    std::memcpy(bytecode.data(), &SCRIPT_MAGIC, sizeof(NovelMind::u32));
    // Missing version and rest of header

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("truncated") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - bytecode truncated at instruction count", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    std::vector<NovelMind::u8> bytecode(6);
    NovelMind::usize offset = 0;

    std::memcpy(bytecode.data() + offset, &SCRIPT_MAGIC, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    NovelMind::u16 version = 1;
    std::memcpy(bytecode.data() + offset, &version, sizeof(NovelMind::u16));
    // Missing instruction count and rest of header

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("truncated") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - bytecode truncated at string count", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    std::vector<NovelMind::u8> bytecode(16);
    NovelMind::usize offset = 0;

    std::memcpy(bytecode.data() + offset, &SCRIPT_MAGIC, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    NovelMind::u16 version = 1;
    std::memcpy(bytecode.data() + offset, &version, sizeof(NovelMind::u16));
    offset += sizeof(NovelMind::u16);

    NovelMind::u16 flags = 0;
    std::memcpy(bytecode.data() + offset, &flags, sizeof(NovelMind::u16));
    offset += sizeof(NovelMind::u16);

    NovelMind::u32 instrCount = 0;
    std::memcpy(bytecode.data() + offset, &instrCount, sizeof(NovelMind::u32));
    offset += sizeof(NovelMind::u32);

    NovelMind::u32 constPoolSize = 0;
    std::memcpy(bytecode.data() + offset, &constPoolSize, sizeof(NovelMind::u32));
    // Missing string count and symbol table size

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("truncated") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - invalid magic number", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    auto bytecode = createBytecodeHeader(
        0xDEADBEEF,  // Invalid magic
        1,
        0,
        0,
        0,
        0,
        0
    );

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Invalid script magic") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - unsupported version", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        99,  // Unsupported version
        0,
        0,
        0,
        0,
        0
    );

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Unsupported bytecode version") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - valid minimal bytecode", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        1,  // One instruction
        0,
        0,
        0
    );

    // Append one HALT instruction
    appendInstruction(bytecode, 0x01, 0);

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isOk());
}

TEST_CASE("Interpreter deserialize - string exceeds max length", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        1,  // One instruction
        0,
        1,  // One string
        0
    );

    // Append one instruction
    appendInstruction(bytecode, 0x01, 0);  // HALT

    // Append a string that exceeds MAX_STRING_LENGTH (1 MB)
    constexpr NovelMind::usize MAX_STRING_LENGTH = 1024 * 1024;
    for (NovelMind::usize i = 0; i <= MAX_STRING_LENGTH; ++i) {
        bytecode.push_back('A');
    }
    bytecode.push_back('\0');  // Null terminator

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("exceeds maximum allowed length") != std::string::npos);
}

TEST_CASE("Interpreter deserialize - unterminated string", "[interpreter][security]")
{
    ScriptInterpreter interpreter;

    constexpr NovelMind::u32 SCRIPT_MAGIC = 0x43534D4E;
    auto bytecode = createBytecodeHeader(
        SCRIPT_MAGIC,
        1,
        0,
        1,  // One instruction
        0,
        1,  // One string
        0
    );

    // Append one instruction
    appendInstruction(bytecode, 0x01, 0);  // HALT

    // Append a string without null terminator
    const char* str = "Hello";
    bytecode.insert(bytecode.end(), str, str + 5);
    // No null terminator - bytecode ends abruptly

    auto result = interpreter.loadFromBytecode(bytecode);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Unterminated string") != std::string::npos);
}
