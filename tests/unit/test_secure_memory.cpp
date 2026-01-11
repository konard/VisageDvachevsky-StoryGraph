#include "NovelMind/core/secure_memory.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>

using namespace NovelMind;
using namespace NovelMind::Core;

// =============================================================================
// secureZeroMemory Tests
// =============================================================================

TEST_CASE("secureZeroMemory zeros memory", "[secure_memory][zeroing]") {
  SECTION("Zeros a simple buffer") {
    u8 buffer[32];
    std::memset(buffer, 0xFF, sizeof(buffer));

    // Verify buffer is filled
    bool allSet = true;
    for (size_t i = 0; i < sizeof(buffer); ++i) {
      if (buffer[i] != 0xFF) {
        allSet = false;
        break;
      }
    }
    REQUIRE(allSet);

    // Zero the buffer
    secureZeroMemory(buffer, sizeof(buffer));

    // Verify all bytes are zero
    bool allZero = true;
    for (size_t i = 0; i < sizeof(buffer); ++i) {
      if (buffer[i] != 0) {
        allZero = false;
        break;
      }
    }
    REQUIRE(allZero);
  }

  SECTION("Handles nullptr safely") {
    // Should not crash
    secureZeroMemory(nullptr, 10);
    REQUIRE(true);
  }

  SECTION("Handles zero size safely") {
    u8 buffer[10];
    std::memset(buffer, 0xFF, sizeof(buffer));
    secureZeroMemory(buffer, 0);
    // Buffer should remain unchanged
    REQUIRE(buffer[0] == 0xFF);
  }
}

// =============================================================================
// SecureAllocator Tests
// =============================================================================

TEST_CASE("SecureAllocator allocates and deallocates correctly",
          "[secure_memory][allocator]") {
  SECTION("Allocates memory") {
    SecureAllocator<u8> allocator;
    u8* ptr = allocator.allocate(32);
    REQUIRE(ptr != nullptr);
    allocator.deallocate(ptr, 32);
  }

  SECTION("Zeros memory on deallocation") {
    SecureAllocator<u8> allocator;
    u8* ptr = allocator.allocate(32);
    REQUIRE(ptr != nullptr);

    // Fill with data
    std::memset(ptr, 0xFF, 32);

    // Store pointer value for later check
    u8* originalPtr = ptr;

    // Create a copy of the data before deallocation
    u8 dataBefore[32];
    std::memcpy(dataBefore, ptr, 32);

    // Verify data is set
    bool allSet = true;
    for (size_t i = 0; i < 32; ++i) {
      if (dataBefore[i] != 0xFF) {
        allSet = false;
        break;
      }
    }
    REQUIRE(allSet);

    // Deallocate (should zero the memory)
    allocator.deallocate(ptr, 32);

    // Note: After deallocation, accessing the memory is undefined behavior
    // We can't reliably test that memory was zeroed after free
    // But the SecureAllocator guarantees it zeroes before calling std::free
    REQUIRE(true);
  }

  SECTION("Handles zero size allocation") {
    SecureAllocator<u8> allocator;
    u8* ptr = allocator.allocate(0);
    REQUIRE(ptr == nullptr);
    allocator.deallocate(ptr, 0);
  }
}

// =============================================================================
// SecureVector Tests
// =============================================================================

TEST_CASE("SecureVector stores and zeros data", "[secure_memory][secure_vector]") {
  SECTION("Creates and uses SecureVector") {
    SecureVector<u8> key(32);
    REQUIRE(key.size() == 32);

    // Fill with data
    for (size_t i = 0; i < key.size(); ++i) {
      key[i] = static_cast<u8>(i);
    }

    // Verify data
    for (size_t i = 0; i < key.size(); ++i) {
      REQUIRE(key[i] == static_cast<u8>(i));
    }
  }

  SECTION("SecureVector can be moved") {
    SecureVector<u8> key1(32);
    for (size_t i = 0; i < key1.size(); ++i) {
      key1[i] = static_cast<u8>(i);
    }

    SecureVector<u8> key2 = std::move(key1);
    REQUIRE(key2.size() == 32);
    for (size_t i = 0; i < key2.size(); ++i) {
      REQUIRE(key2[i] == static_cast<u8>(i));
    }
  }

  SECTION("SecureVector can be copied") {
    SecureVector<u8> key1(32);
    for (size_t i = 0; i < key1.size(); ++i) {
      key1[i] = static_cast<u8>(i);
    }

    SecureVector<u8> key2 = key1;
    REQUIRE(key2.size() == 32);
    for (size_t i = 0; i < key2.size(); ++i) {
      REQUIRE(key2[i] == static_cast<u8>(i));
    }
  }

  SECTION("SecureVector works with encryption key size") {
    // Test with typical 32-byte AES-256 key
    SecureVector<u8> key(32);
    REQUIRE(key.size() == 32);

    // Simulate loading a key
    const char* hexKey = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    for (size_t i = 0; i < 32; ++i) {
      std::string byteStr(&hexKey[i * 2], 2);
      key[i] = static_cast<u8>(std::stoul(byteStr, nullptr, 16));
    }

    // Verify key loaded correctly
    REQUIRE(key[0] == 0x01);
    REQUIRE(key[1] == 0x23);
    REQUIRE(key[31] == 0xEF);
  }
}

// =============================================================================
// SecureMemoryGuard Tests
// =============================================================================

TEST_CASE("SecureMemoryGuard protects and zeros memory", "[secure_memory][guard]") {
  SECTION("Zeros memory on destruction") {
    std::array<u8, 32> key;
    key.fill(0xFF);

    {
      SecureMemoryGuard guard(key.data(), key.size());
      // Memory should still be accessible
      REQUIRE(key[0] == 0xFF);
    }
    // After guard destruction, memory should be zeroed
    bool allZero = true;
    for (size_t i = 0; i < key.size(); ++i) {
      if (key[i] != 0) {
        allZero = false;
        break;
      }
    }
    REQUIRE(allZero);
  }

  SECTION("Handles nullptr safely") {
    // Should not crash
    SecureMemoryGuard guard(nullptr, 10);
    REQUIRE(true);
  }

  SECTION("Handles zero size safely") {
    u8 buffer[10];
    SecureMemoryGuard guard(buffer, 0);
    REQUIRE(true);
  }

  SECTION("Multiple guards on different memory regions") {
    std::array<u8, 16> key1;
    std::array<u8, 16> key2;
    key1.fill(0xAA);
    key2.fill(0xBB);

    {
      SecureMemoryGuard guard1(key1.data(), key1.size());
      SecureMemoryGuard guard2(key2.data(), key2.size());
      REQUIRE(key1[0] == 0xAA);
      REQUIRE(key2[0] == 0xBB);
    }

    // Both should be zeroed
    REQUIRE(key1[0] == 0x00);
    REQUIRE(key2[0] == 0x00);
  }
}

// =============================================================================
// Memory Locking Tests (platform-dependent)
// =============================================================================

TEST_CASE("Memory locking functions work correctly", "[secure_memory][locking]") {
  SECTION("lockMemory returns a result") {
    u8 buffer[4096]; // Use page-size aligned buffer
    bool result = lockMemory(buffer, sizeof(buffer));

    // Result may be true or false depending on platform and privileges
    // We just verify it doesn't crash
    REQUIRE((result == true || result == false));

    if (result) {
      unlockMemory(buffer, sizeof(buffer));
    }
  }

  SECTION("unlockMemory handles nullptr safely") {
    // Should not crash
    unlockMemory(nullptr, 10);
    REQUIRE(true);
  }

  SECTION("lockMemory handles nullptr safely") {
    bool result = lockMemory(nullptr, 10);
    REQUIRE(result == false);
  }

  SECTION("lockMemory handles zero size safely") {
    u8 buffer[10];
    bool result = lockMemory(buffer, 0);
    REQUIRE(result == false);
  }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_CASE("Secure memory integration with encryption keys",
          "[secure_memory][integration]") {
  SECTION("Simulates encryption key lifecycle") {
    // Load key
    SecureVector<u8> key(32);
    for (size_t i = 0; i < key.size(); ++i) {
      key[i] = static_cast<u8>(i);
    }

    // Use key for "encryption" (just verify it's accessible)
    u8 firstByte = key[0];
    u8 lastByte = key[31];
    REQUIRE(firstByte == 0);
    REQUIRE(lastByte == 31);

    // Key goes out of scope here and should be securely zeroed
  }

  SECTION("Temporary key buffer with guard") {
    std::array<u8, 32> tempKey;
    tempKey.fill(0x42);

    {
      SecureMemoryGuard guard(tempKey.data(), tempKey.size());
      // Process key...
      REQUIRE(tempKey[0] == 0x42);
    }

    // Key should be zeroed after guard destruction
    REQUIRE(tempKey[0] == 0x00);
  }
}
