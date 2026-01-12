# NovelMind Script Bytecode Serialization Format

## Overview

This document defines the binary serialization format for NovelMind Script bytecode, with a focus on ensuring cross-platform portability and compatibility.

## Endianness

**All multi-byte values in the bytecode format use little-endian byte order.**

This ensures consistent behavior across different platforms:
- x86/x86-64 (little-endian)
- ARM (typically little-endian, but can be either)
- MIPS (can be either endian)
- PowerPC (big-endian)

By standardizing on little-endian, we ensure that bytecode compiled on one platform will execute correctly on all other platforms, regardless of the native endianness.

## Float Serialization

### Portable Float Encoding

32-bit floating-point values (`f32`) are serialized using the following approach:

1. The float value is interpreted as its raw 32-bit representation (IEEE 754 binary32 format)
2. The 32-bit value is converted to little-endian byte order
3. The result is stored as a `u32` operand in the bytecode

This is implemented via the `serializeFloat()` and `deserializeFloat()` functions in `engine_core/include/NovelMind/core/endian.hpp`.

### Example

```cpp
// Serialization (compiler)
f32 duration = 1.5f;
u32 encoded = serializeFloat(duration);  // Little-endian representation
emitOp(OpCode::PUSH_FLOAT, encoded);

// Deserialization (VM)
f32 duration = deserializeFloat(instr.operand);
```

### IEEE 754 Compatibility

This approach assumes that all target platforms use IEEE 754 binary32 format for 32-bit floats, which is true for all modern architectures. If a platform uses a different floating-point format, additional conversion would be required.

## Bytecode Structure

### Instruction Format

Each instruction consists of:
- `OpCode` (8-bit enum value)
- `operand` (32-bit unsigned integer, little-endian)

```cpp
struct Instruction {
    OpCode opcode;     // 1 byte
    u32 operand;       // 4 bytes (little-endian)
};
```

### OpCodes Using Float Operands

The following opcodes encode float values in their operands:
- `OpCode::PUSH_FLOAT` - Push a float literal onto the stack
- `OpCode::WAIT` - Wait for a duration (float seconds)

Additionally, the following opcodes may have float values pushed onto the stack before execution:
- `OpCode::TRANSITION` - Duration parameter
- `OpCode::MOVE_CHARACTER` - Duration and optional X/Y coordinates
- `OpCode::STOP_MUSIC` - Optional fade-out duration

## String Table

String literals are stored in a separate string table and referenced by index in bytecode operands. The string table uses:
- UTF-8 encoding for all strings
- Null-terminator separators
- 32-bit indices (little-endian)

## Integer Values

All integer operands are stored as unsigned 32-bit values in little-endian byte order. Signed integers are cast to `u32` for serialization and cast back during execution.

## Cross-Platform Testing

To ensure portability, bytecode should be tested on:
1. Little-endian platforms (x86-64, ARM little-endian)
2. Big-endian platforms (if available: PowerPC, MIPS big-endian)
3. Different compilers (GCC, Clang, MSVC)
4. Different operating systems (Windows, Linux, macOS)

## Implementation Details

### Endianness Detection

The `isLittleEndian()` constexpr function in `endian.hpp` detects the host platform's endianness at compile time using preprocessor macros:

- `__BYTE_ORDER__` and related macros (GCC/Clang)
- Platform-specific macros (`_WIN32`, `__i386__`, etc.)
- Conservative fallback to little-endian (most common)

### Byte Swapping

When the host platform is big-endian, the `byteSwap32()` function efficiently reverses byte order using:
- Compiler intrinsics (`__builtin_bswap32`, `_byteswap_ulong`)
- Manual bit manipulation fallback

### No Runtime Overhead on Little-Endian Platforms

On little-endian platforms (x86, most ARM), the conversion functions compile to no-ops, ensuring zero runtime overhead.

## Version History

| Version | Change |
|---------|--------|
| 1.0 | Initial bytecode format (endianness not specified) |
| 1.1 | Standardized on little-endian for all multi-byte values |

## See Also

- `engine_core/include/NovelMind/core/endian.hpp` - Endianness utilities
- `engine_core/src/scripting/compiler.cpp` - Bytecode compiler
- `engine_core/src/scripting/vm.cpp` - Bytecode virtual machine
- `docs/pack_file_format.md` - Resource pack format (also uses little-endian)
