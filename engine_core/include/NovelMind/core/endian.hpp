#pragma once

#include "NovelMind/core/types.hpp"
#include <cstring>

namespace NovelMind {

/**
 * @brief Endianness utilities for portable binary serialization
 *
 * This header provides utilities for converting between native byte order
 * and a standardized little-endian format for portable serialization.
 *
 * All serialized data in the NovelMind engine uses little-endian byte order
 * to ensure compatibility across different platforms (x86, ARM, etc.).
 */

/**
 * @brief Detect host endianness at compile time
 */
constexpr bool isLittleEndian() {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
  return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
#elif defined(_WIN32) || defined(_WIN64)
  // Windows is always little-endian
  return true;
#elif defined(__LITTLE_ENDIAN__) || defined(__i386__) || defined(__x86_64__) || defined(__amd64__) || defined(_M_IX86) || defined(_M_X64) || defined(__aarch64__) || defined(__arm__)
  return true;
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
  return false;
#else
  // Conservative fallback: assume little-endian (most common)
  // At runtime, this can be detected if needed
  return true;
#endif
}

/**
 * @brief Swap byte order of a 32-bit value
 */
inline constexpr u32 byteSwap32(u32 value) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(value);
#elif defined(_MSC_VER)
  return _byteswap_ulong(value);
#else
  return ((value & 0xFF000000u) >> 24) |
         ((value & 0x00FF0000u) >> 8)  |
         ((value & 0x0000FF00u) << 8)  |
         ((value & 0x000000FFu) << 24);
#endif
}

/**
 * @brief Convert 32-bit value from native to little-endian
 */
inline constexpr u32 toLittleEndian32(u32 value) {
  if constexpr (isLittleEndian()) {
    return value;
  } else {
    return byteSwap32(value);
  }
}

/**
 * @brief Convert 32-bit value from little-endian to native
 */
inline constexpr u32 fromLittleEndian32(u32 value) {
  if constexpr (isLittleEndian()) {
    return value;
  } else {
    return byteSwap32(value);
  }
}

/**
 * @brief Serialize a float to a portable 32-bit little-endian representation
 *
 * This function converts a 32-bit float to a 32-bit unsigned integer
 * using little-endian byte order, making it portable across platforms.
 *
 * @param value The float value to serialize
 * @return The serialized value as a 32-bit unsigned integer
 */
inline u32 serializeFloat(f32 value) {
  u32 bits;
  std::memcpy(&bits, &value, sizeof(f32));
  return toLittleEndian32(bits);
}

/**
 * @brief Deserialize a float from a portable 32-bit little-endian representation
 *
 * This function converts a 32-bit unsigned integer in little-endian byte order
 * back to a 32-bit float.
 *
 * @param serialized The serialized value as a 32-bit unsigned integer
 * @return The deserialized float value
 */
inline f32 deserializeFloat(u32 serialized) {
  u32 bits = fromLittleEndian32(serialized);
  f32 value;
  std::memcpy(&value, &bits, sizeof(f32));
  return value;
}

} // namespace NovelMind
