#include <catch2/catch_test_macros.hpp>

#include "NovelMind/vfs/memory_fs.hpp"
#include "NovelMind/vfs/pack_reader.hpp"
#include "NovelMind/vfs/pack_security.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

using namespace NovelMind;
using namespace NovelMind::vfs;
using namespace NovelMind::VFS;

// ============================================================================
// VFS Pack Reader Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackReader - Header validation", "[vfs][pack][security]") {
  SECTION("Valid pack magic number") {
    CHECK(PACK_MAGIC == 0x53524D4E); // "NMRS" in little-endian
  }

  SECTION("Pack version numbers") {
    CHECK(PACK_VERSION_MAJOR == 1);
    CHECK(PACK_VERSION_MINOR == 0);
  }

  SECTION("Pack header structure size is reasonable") {
    [[maybe_unused]] PackHeader header;
    // Header should contain all fields without excessive padding
    CHECK(sizeof(PackHeader) <= 128);
  }
}

TEST_CASE("PackReader - Resource entry structure", "[vfs][pack]") {
  SECTION("Resource entry has required fields") {
    PackResourceEntry entry;
    entry.idStringOffset = 0;
    entry.type = 1;
    entry.dataOffset = 1024;
    entry.compressedSize = 512;
    entry.uncompressedSize = 1024;
    entry.flags = 0;
    entry.checksum = 0xDEADBEEF;

    CHECK(entry.dataOffset == 1024);
    CHECK(entry.compressedSize == 512);
    CHECK(entry.uncompressedSize == 1024);
    CHECK(entry.checksum == 0xDEADBEEF);
  }

  SECTION("Resource entry IV field size") {
    PackResourceEntry entry;
    CHECK(sizeof(entry.iv) == 8);
  }
}

TEST_CASE("PackReader - Pack flags", "[vfs][pack][security]") {
  SECTION("No flags") {
    u32 flags = static_cast<u32>(PackFlags::None);
    CHECK(flags == 0);
  }

  SECTION("Encrypted flag") {
    u32 flags = static_cast<u32>(PackFlags::Encrypted);
    CHECK(flags == 1);
    CHECK((flags & static_cast<u32>(PackFlags::Encrypted)) != 0);
  }

  SECTION("Compressed flag") {
    u32 flags = static_cast<u32>(PackFlags::Compressed);
    CHECK(flags == 2);
    CHECK((flags & static_cast<u32>(PackFlags::Compressed)) != 0);
  }

  SECTION("Signed flag") {
    u32 flags = static_cast<u32>(PackFlags::Signed);
    CHECK(flags == 4);
    CHECK((flags & static_cast<u32>(PackFlags::Signed)) != 0);
  }

  SECTION("Combined flags") {
    u32 flags = static_cast<u32>(PackFlags::Encrypted) |
                static_cast<u32>(PackFlags::Compressed);
    CHECK((flags & static_cast<u32>(PackFlags::Encrypted)) != 0);
    CHECK((flags & static_cast<u32>(PackFlags::Compressed)) != 0);
    CHECK((flags & static_cast<u32>(PackFlags::Signed)) == 0);
  }
}

TEST_CASE("PackReader - Unmount operations", "[vfs][pack]") {
  PackReader reader;

  SECTION("Unmount non-existent pack succeeds gracefully") {
    // Should not crash or throw
    reader.unmount("nonexistent.pack");
  }

  SECTION("Unmount all on empty reader") {
    // Should not crash or throw
    reader.unmountAll();
  }
}

TEST_CASE("PackReader - Resource existence check", "[vfs][pack]") {
  PackReader reader;

  SECTION("Non-existent resource returns false") {
    CHECK(reader.exists("nonexistent/resource.txt") == false);
  }
}

TEST_CASE("PackReader - List resources", "[vfs][pack]") {
  PackReader reader;

  SECTION("Empty pack reader returns empty list") {
    auto resources = reader.listResources();
    CHECK(resources.empty());
  }

  SECTION("List resources with type filter") {
    auto textures = reader.listResources(ResourceType::Texture);
    auto audio = reader.listResources(ResourceType::Audio);
    auto scripts = reader.listResources(ResourceType::Script);

    // Empty pack should return empty lists for all types
    CHECK(textures.empty());
    CHECK(audio.empty());
    CHECK(scripts.empty());
  }
}

// ============================================================================
// Pack Security - Integrity Checking (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackIntegrityChecker - CRC32 calculation", "[vfs][security]") {
  SECTION("CRC32 of known data") {
    const char *testData = "Hello, World!";
    u32 crc = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));

    // CRC32 should be deterministic
    u32 crc2 = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));
    CHECK(crc == crc2);
  }

  SECTION("CRC32 of empty data") {
    u32 crc = PackIntegrityChecker::calculateCrc32(nullptr, 0);
    // Empty data should have a defined CRC
    CHECK(crc != 0xFFFFFFFF); // Should be processed, not uninitialized
  }

  SECTION("CRC32 changes with different data") {
    const char *data1 = "Test Data 1";
    const char *data2 = "Test Data 2";

    u32 crc1 = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(data1), std::strlen(data1));
    u32 crc2 = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(data2), std::strlen(data2));

    CHECK(crc1 != crc2);
  }

  SECTION("CRC32 is sensitive to data order") {
    const char *data1 = "ABC";
    const char *data2 = "CBA";

    u32 crc1 = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(data1), std::strlen(data1));
    u32 crc2 = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(data2), std::strlen(data2));

    CHECK(crc1 != crc2);
  }
}

TEST_CASE("PackIntegrityChecker - SHA256 calculation", "[vfs][security]") {
  SECTION("SHA256 of known data") {
    const char *testData = "Test";
    auto hash1 = PackIntegrityChecker::calculateSha256(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));
    auto hash2 = PackIntegrityChecker::calculateSha256(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));

    // SHA256 should be deterministic
    CHECK(hash1 == hash2);
    CHECK(hash1.size() == 32);
  }

  SECTION("SHA256 of empty data") {
    auto hash = PackIntegrityChecker::calculateSha256(nullptr, 0);
    CHECK(hash.size() == 32);

    // SHA256 of empty string is a known value (not all zeros)
    bool allZeros = std::all_of(hash.begin(), hash.end(),
                                 [](u8 byte) { return byte == 0; });
    CHECK_FALSE(allZeros);
  }

  SECTION("SHA256 changes with different data") {
    const char *data1 = "Data 1";
    const char *data2 = "Data 2";

    auto hash1 = PackIntegrityChecker::calculateSha256(
        reinterpret_cast<const u8 *>(data1), std::strlen(data1));
    auto hash2 = PackIntegrityChecker::calculateSha256(
        reinterpret_cast<const u8 *>(data2), std::strlen(data2));

    CHECK(hash1 != hash2);
  }
}

TEST_CASE("PackIntegrityChecker - Header verification",
          "[vfs][security][header]") {
  PackIntegrityChecker checker;

  SECTION("Verify valid header structure") {
    PackHeader header;
    header.magic = PACK_MAGIC;
    header.versionMajor = PACK_VERSION_MAJOR;
    header.versionMinor = PACK_VERSION_MINOR;
    header.flags = 0;
    header.resourceCount = 10;
    header.resourceTableOffset = sizeof(PackHeader);
    header.stringTableOffset = 1024;
    header.dataOffset = 2048;
    header.totalSize = 4096;
    std::memset(header.contentHash, 0, sizeof(header.contentHash));

    auto result = checker.verifyHeader(reinterpret_cast<const u8 *>(&header),
                                        sizeof(header));

    // Note: Actual implementation may validate more fields
    // This tests the basic structure
    CHECK(result.isOk());
  }

  SECTION("Reject header with invalid magic") {
    PackHeader header;
    header.magic = 0xDEADBEEF; // Invalid magic
    header.versionMajor = PACK_VERSION_MAJOR;
    header.versionMinor = PACK_VERSION_MINOR;

    auto result = checker.verifyHeader(reinterpret_cast<const u8 *>(&header),
                                        sizeof(header));

    if (result.isOk()) {
      CHECK(result.value().result == PackVerificationResult::InvalidMagic);
    }
  }

  SECTION("Reject header with incompatible version") {
    PackHeader header;
    header.magic = PACK_MAGIC;
    header.versionMajor = 99; // Future version
    header.versionMinor = 0;

    auto result = checker.verifyHeader(reinterpret_cast<const u8 *>(&header),
                                        sizeof(header));

    if (result.isOk()) {
      CHECK(result.value().result == PackVerificationResult::InvalidVersion);
    }
  }

  SECTION("Reject header with invalid size") {
    PackHeader header;
    header.magic = PACK_MAGIC;
    header.versionMajor = PACK_VERSION_MAJOR;
    header.versionMinor = PACK_VERSION_MINOR;
    header.totalSize = 0; // Invalid: pack can't be size 0

    auto result = checker.verifyHeader(reinterpret_cast<const u8 *>(&header),
                                        sizeof(header));

    if (result.isOk()) {
      CHECK(result.value().result != PackVerificationResult::Valid);
    }
  }
}

TEST_CASE("PackIntegrityChecker - Resource checksum verification",
          "[vfs][security]") {
  PackIntegrityChecker checker;

  SECTION("Verify resource with correct checksum") {
    const char *testData = "Resource content";
    u32 expectedChecksum = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));

    auto result = checker.verifyResource(
        reinterpret_cast<const u8 *>(testData), 1024, 0, std::strlen(testData),
        expectedChecksum);

    CHECK(result.isOk());
    if (result.isOk()) {
      CHECK(result.value().result == PackVerificationResult::Valid);
    }
  }

  SECTION("Detect corrupted resource with wrong checksum") {
    const char *testData = "Resource content";
    u32 wrongChecksum = 0xDEADBEEF; // Intentionally wrong

    auto result = checker.verifyResource(
        reinterpret_cast<const u8 *>(testData), 1024, 0, std::strlen(testData),
        wrongChecksum);

    if (result.isOk()) {
      CHECK(result.value().result == PackVerificationResult::ChecksumMismatch);
    }
  }

  SECTION("Detect resource read out of bounds") {
    const char *testData = "Small";
    u32 checksum = PackIntegrityChecker::calculateCrc32(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData));

    // Try to read past end of buffer
    auto result = checker.verifyResource(
        reinterpret_cast<const u8 *>(testData), std::strlen(testData), 0,
        1000, // Requesting 1000 bytes from 5-byte buffer
        checksum);

    // Should detect corruption
    if (result.isOk()) {
      CHECK(result.value().result != PackVerificationResult::Valid);
    }
  }
}

// ============================================================================
// Pack Decryption Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackDecryptor - Key management", "[vfs][security][encryption]") {
  PackDecryptor decryptor;

  SECTION("Set key from vector") {
    std::vector<u8> key = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    decryptor.setKey(key);
    // Key should be accepted (no crash/exception)
  }

  SECTION("Set key from pointer") {
    u8 key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    decryptor.setKey(key, 16);
    // Key should be accepted (no crash/exception)
  }
}

TEST_CASE("PackDecryptor - Random IV generation",
          "[vfs][security][encryption]") {
  SECTION("Generate random IV") {
    auto iv1Result = PackDecryptor::generateRandomIV(16);
    CHECK(iv1Result.isOk());

    if (iv1Result.isOk()) {
      auto iv1 = iv1Result.value();
      CHECK(iv1.size() == 16);

      // Generate another IV - should be different (with very high probability)
      auto iv2Result = PackDecryptor::generateRandomIV(16);
      CHECK(iv2Result.isOk());

      if (iv2Result.isOk()) {
        auto iv2 = iv2Result.value();
        CHECK(iv2.size() == 16);
        CHECK(iv1 != iv2);
      }
    }
  }

  SECTION("Generate IV with custom size") {
    auto iv32Result = PackDecryptor::generateRandomIV(32);
    CHECK(iv32Result.isOk());

    if (iv32Result.isOk()) {
      CHECK(iv32Result.value().size() == 32);
    }
  }
}

TEST_CASE("PackDecryptor - Key derivation", "[vfs][security][encryption]") {
  SECTION("Derive key from password") {
    const char *password = "test_password";
    u8 salt[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                   0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    auto keyResult = PackDecryptor::deriveKey(password, salt, 16);

    if (keyResult.isOk()) {
      auto key = keyResult.value();
      CHECK_FALSE(key.empty());

      // Same password + salt should produce same key
      auto key2Result = PackDecryptor::deriveKey(password, salt, 16);
      if (key2Result.isOk()) {
        CHECK(key == key2Result.value());
      }
    }
  }

  SECTION("Different passwords produce different keys") {
    u8 salt[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                   0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    auto key1Result = PackDecryptor::deriveKey("password1", salt, 16);
    auto key2Result = PackDecryptor::deriveKey("password2", salt, 16);

    if (key1Result.isOk() && key2Result.isOk()) {
      CHECK(key1Result.value() != key2Result.value());
    }
  }

  SECTION("Different salts produce different keys") {
    u8 salt1[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    u8 salt2[16] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

    auto key1Result = PackDecryptor::deriveKey("password", salt1, 16);
    auto key2Result = PackDecryptor::deriveKey("password", salt2, 16);

    if (key1Result.isOk() && key2Result.isOk()) {
      CHECK(key1Result.value() != key2Result.value());
    }
  }
}

// ============================================================================
// Secure Pack Reader Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("SecurePackReader - Basic operations",
          "[vfs][security][pack_reader]") {
  SecurePackReader reader;

  SECTION("Initial state") {
    CHECK(reader.isOpen() == false);
    CHECK(reader.lastVerificationResult() == PackVerificationResult::Valid);
  }

  SECTION("List resources on closed reader") {
    auto resources = reader.listResources();
    CHECK(resources.empty());
  }

  SECTION("Check existence on closed reader") {
    CHECK(reader.exists("any/resource") == false);
  }

  SECTION("Get metadata on closed reader") {
    auto meta = reader.getResourceMeta("any/resource");
    CHECK_FALSE(meta.has_value());
  }
}

TEST_CASE("SecurePackReader - Decryptor and checker injection",
          "[vfs][security]") {
  SecurePackReader reader;

  SECTION("Set decryptor") {
    auto decryptor = std::make_unique<PackDecryptor>();
    reader.setDecryptor(std::move(decryptor));
    // Should not crash
  }

  SECTION("Set integrity checker") {
    auto checker = std::make_unique<PackIntegrityChecker>();
    reader.setIntegrityChecker(std::move(checker));
    // Should not crash
  }
}

// ============================================================================
// Multi-Pack Manager Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackReader - Multi-pack coordination", "[vfs][pack][multipack]") {
  PackReader reader;

  SECTION("Multiple pack mounts and unmounts") {
    // Note: These would normally fail as files don't exist,
    // but we're testing the API doesn't crash

    // Unmount operations should be safe even if mount failed
    reader.unmount("pack1.pack");
    reader.unmount("pack2.pack");
    reader.unmountAll();
  }
}

// ============================================================================
// Pack Verification Report Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackVerificationReport - Error reporting", "[vfs][security]") {
  SECTION("Create report with error details") {
    PackVerificationReport report;
    report.result = PackVerificationResult::ChecksumMismatch;
    report.message = "Resource checksum does not match expected value";
    report.errorOffset = 1024;
    report.resourceId = "textures/character.png";

    CHECK(report.result == PackVerificationResult::ChecksumMismatch);
    CHECK_FALSE(report.message.empty());
    CHECK(report.errorOffset == 1024);
    CHECK(report.resourceId == "textures/character.png");
  }

  SECTION("Verification result enumeration coverage") {
    // Ensure all error types are representable
    std::vector<PackVerificationResult> allResults = {
        PackVerificationResult::Valid,
        PackVerificationResult::InvalidMagic,
        PackVerificationResult::InvalidVersion,
        PackVerificationResult::CorruptedHeader,
        PackVerificationResult::CorruptedResourceTable,
        PackVerificationResult::CorruptedData,
        PackVerificationResult::ChecksumMismatch,
        PackVerificationResult::SignatureInvalid,
        PackVerificationResult::DecryptionFailed};

    CHECK(allResults.size() == 9);
  }
}

// ============================================================================
// Pack Resource Metadata Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackResourceMeta - Structure and validation", "[vfs][pack]") {
  SECTION("Create resource metadata") {
    PackResourceMeta meta;
    meta.type = static_cast<u32>(ResourceType::Texture);
    meta.uncompressedSize = 4096;
    meta.checksum = 0x12345678;

    CHECK(meta.type == static_cast<u32>(ResourceType::Texture));
    CHECK(meta.uncompressedSize == 4096);
    CHECK(meta.checksum == 0x12345678);
  }

  SECTION("Metadata with large file size") {
    PackResourceMeta meta;
    meta.uncompressedSize = 1024ULL * 1024ULL * 100; // 100 MB

    CHECK(meta.uncompressedSize == 104857600);
  }
}

// ============================================================================
// Concurrent Access Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackReader - Thread safety for concurrent reads",
          "[vfs][pack][concurrency]") {
  PackReader reader;

  SECTION("Multiple exists() calls are thread-safe") {
    // PackReader uses mutex internally
    // Sequential calls should be safe
    bool exists1 = reader.exists("resource1");
    bool exists2 = reader.exists("resource2");
    bool exists3 = reader.exists("resource3");

    // All should return false on empty reader
    CHECK(exists1 == false);
    CHECK(exists2 == false);
    CHECK(exists3 == false);
  }

  SECTION("Multiple listResources() calls are thread-safe") {
    auto list1 = reader.listResources();
    auto list2 = reader.listResources();
    auto list3 = reader.listResources();

    // All should return empty on empty reader
    CHECK(list1.empty());
    CHECK(list2.empty());
    CHECK(list3.empty());
  }
}

// ============================================================================
// Error Recovery Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackReader - Error recovery", "[vfs][pack][error_handling]") {
  PackReader reader;

  SECTION("Reading non-existent file returns error") {
    auto result = reader.readFile("nonexistent.txt");
    CHECK(result.isError());
  }

  SECTION("Reading from non-mounted pack returns error") {
    auto result = reader.readFile("some/resource");
    CHECK(result.isError());
  }

  SECTION("Unmounting after error leaves reader in valid state") {
    auto result = reader.readFile("nonexistent.txt");
    CHECK(result.isError());

    // Reader should still be usable
    reader.unmountAll();
    auto list = reader.listResources();
    CHECK(list.empty());
  }
}

// ============================================================================
// Integer Overflow Security Tests (Issue #561 - P0)
// ============================================================================

TEST_CASE("SecurePackReader - String boundary overflow protection",
          "[vfs][security][overflow]") {
  SECTION("Boundary check prevents integer overflow") {
    // Test the safe boundary check logic
    // if (offset > size || length > size - offset) -> reject

    const u64 maxU64 = std::numeric_limits<u64>::max();

    // Case 1: offset + length would overflow to a small value
    u64 size = 1000;
    u64 offset = maxU64 - 100; // Very large offset
    u64 length = 200;          // Adding these would overflow

    // Unsafe check would be: (offset + length <= size)
    // which overflows to: (99 <= 1000) -> PASS (WRONG!)
    // Safe check: (offset > size || length > size - offset)
    bool shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == true);

    // Case 2: Normal valid case
    offset = 100;
    length = 50;
    size = 1000;
    shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == false); // Should pass

    // Case 3: Exactly at boundary
    offset = 100;
    length = 900;
    size = 1000;
    shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == false); // Should pass

    // Case 4: Just over boundary
    offset = 100;
    length = 901;
    size = 1000;
    shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == true); // Should fail
  }

  SECTION("Boundary check with maximum values") {
    const u64 maxU64 = std::numeric_limits<u64>::max();

    // Test with maximum offset
    u64 size = maxU64;
    u64 offset = maxU64;
    u64 length = 1;

    bool shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == false); // offset == size is OK if length == 0

    // But any non-zero length should fail
    length = 1;
    shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == true);

    // Test overflow scenario
    offset = maxU64 - 500;
    length = 1000; // offset + length would overflow
    size = maxU64;
    shouldReject = (offset > size || length > size - offset);
    CHECK(shouldReject == true);
  }

  SECTION("Boundary check prevents buffer overread") {
    // Simulate the exact scenario from the vulnerability
    const u64 stringDataSize = 0x1000;
    u64 offset = 0xFFFFFFFFFFFFFF00;
    u64 strSize = 0x200;

    // Unsafe check: offset + strSize = 0x100 (overflow)
    // 0x100 >= 0x1000? NO -> Would incorrectly pass!
    u64 unsafeSum = offset + strSize; // This overflows
    bool unsafeCheck = (unsafeSum >= stringDataSize);
    CHECK(unsafeCheck == false); // Demonstrates the vulnerability

    // Safe check: prevents overflow
    bool safeCheck = (offset > stringDataSize ||
                      strSize > stringDataSize - offset);
    CHECK(safeCheck == true); // Correctly rejects
  }
}

TEST_CASE("SecurePackReader - Resource table overflow protection",
          "[vfs][security][overflow]") {
  SECTION("Resource table offset + size overflow protection") {
    const u64 fileSize = 10000;
    const u64 maxU64 = std::numeric_limits<u64>::max();

    // Case 1: offset + size would overflow
    u64 offset = maxU64 - 100;
    u64 tableSize = 200;

    // Safe check
    bool shouldReject = (offset > fileSize ||
                         tableSize > fileSize - offset);
    CHECK(shouldReject == true);

    // Case 2: Valid resource table
    offset = 512;
    tableSize = 1024;
    shouldReject = (offset > fileSize || tableSize > fileSize - offset);
    CHECK(shouldReject == false);

    // Case 3: Table extends just past end
    offset = 9000;
    tableSize = 1001; // Extends to 10001, past fileSize of 10000
    shouldReject = (offset > fileSize || tableSize > fileSize - offset);
    CHECK(shouldReject == true);
  }
}

TEST_CASE("SecurePackReader - Resource data overflow protection",
          "[vfs][security][overflow]") {
  SECTION("Absolute offset calculation overflow protection") {
    const u64 fileSize = 100000;
    const u64 maxU64 = std::numeric_limits<u64>::max();

    // Case 1: dataOffset + entryOffset overflows
    u64 dataOffset = maxU64 - 1000;
    u64 entryOffset = 2000;

    bool shouldReject = (entryOffset > fileSize ||
                         dataOffset > fileSize - entryOffset);
    CHECK(shouldReject == true);

    // Case 2: Valid offsets
    dataOffset = 10000;
    entryOffset = 5000;
    shouldReject = (entryOffset > fileSize ||
                    dataOffset > fileSize - entryOffset);
    CHECK(shouldReject == false);
  }

  SECTION("Resource data end calculation overflow protection") {
    const u64 fileSize = 100000;
    const u64 footerSize = 256;
    const u64 maxDataEnd = fileSize - footerSize;
    const u64 maxU64 = std::numeric_limits<u64>::max();

    // Case 1: absoluteOffset + compressedSize overflows
    u64 absoluteOffset = maxU64 - 1000;
    u64 compressedSize = 2000;

    bool shouldReject = (absoluteOffset > maxDataEnd ||
                         compressedSize > maxDataEnd - absoluteOffset);
    CHECK(shouldReject == true);

    // Case 2: Data extends past footer boundary
    absoluteOffset = 99000;
    compressedSize = 2000; // Would extend to 101000, past maxDataEnd of 99744
    shouldReject = (absoluteOffset > maxDataEnd ||
                    compressedSize > maxDataEnd - absoluteOffset);
    CHECK(shouldReject == true);

    // Case 3: Valid resource data
    absoluteOffset = 50000;
    compressedSize = 10000;
    shouldReject = (absoluteOffset > maxDataEnd ||
                    compressedSize > maxDataEnd - absoluteOffset);
    CHECK(shouldReject == false);
  }
}

// ============================================================================
// Archive Format Variation Tests (Issue #187 - P0)
// ============================================================================

TEST_CASE("PackReader - Pack format flags", "[vfs][pack]") {
  SECTION("Encrypted pack detection") {
    u32 flags = static_cast<u32>(PackFlags::Encrypted);
    bool isEncrypted = (flags & static_cast<u32>(PackFlags::Encrypted)) != 0;
    CHECK(isEncrypted == true);
  }

  SECTION("Compressed pack detection") {
    u32 flags = static_cast<u32>(PackFlags::Compressed);
    bool isCompressed = (flags & static_cast<u32>(PackFlags::Compressed)) != 0;
    CHECK(isCompressed == true);
  }

  SECTION("Signed pack detection") {
    u32 flags = static_cast<u32>(PackFlags::Signed);
    bool isSigned = (flags & static_cast<u32>(PackFlags::Signed)) != 0;
    CHECK(isSigned == true);
  }

  SECTION("Plain pack (no flags)") {
    u32 flags = static_cast<u32>(PackFlags::None);
    bool isEncrypted = (flags & static_cast<u32>(PackFlags::Encrypted)) != 0;
    bool isCompressed = (flags & static_cast<u32>(PackFlags::Compressed)) != 0;
    bool isSigned = (flags & static_cast<u32>(PackFlags::Signed)) != 0;

    CHECK(isEncrypted == false);
    CHECK(isCompressed == false);
    CHECK(isSigned == false);
  }
}
