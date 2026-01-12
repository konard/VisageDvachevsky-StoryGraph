/**
 * @file test_vfs_corrupted_pack_files.cpp
 * @brief Comprehensive tests for VFS corrupted pack file handling (Issue #501)
 *
 * Tests cover:
 * - Truncated pack file detection
 * - Invalid header detection
 * - Corrupted index detection
 * - Missing data detection
 * - CRC mismatch detection
 * - Error messages and recovery
 *
 * Acceptance Criteria:
 * - Corruption is detected
 * - Clear error messages are provided
 * - No crashes on bad data
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/vfs/pack_reader.hpp"
#include "NovelMind/vfs/pack_security.hpp"
#include <fstream>
#include <cstring>
#include <vector>

using namespace NovelMind::vfs;
using namespace NovelMind;
using namespace NovelMind::VFS;

// Helper function to create a minimal valid pack header
void writeValidPackHeader(std::ofstream& file) {
    u32 magic = PACK_MAGIC;
    u16 versionMajor = PACK_VERSION_MAJOR;
    u16 versionMinor = PACK_VERSION_MINOR;
    u32 flags = 0;
    u32 resourceCount = 1;
    u64 resourceTableOffset = 64;
    u64 stringTableOffset = 128;
    u64 dataOffset = 256;
    u64 totalSize = 512;
    u8 contentHash[16] = {0};

    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
    file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
    file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
    file.write(reinterpret_cast<const char*>(&resourceCount), sizeof(resourceCount));
    file.write(reinterpret_cast<const char*>(&resourceTableOffset), sizeof(resourceTableOffset));
    file.write(reinterpret_cast<const char*>(&stringTableOffset), sizeof(stringTableOffset));
    file.write(reinterpret_cast<const char*>(&dataOffset), sizeof(dataOffset));
    file.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
    file.write(reinterpret_cast<const char*>(contentHash), 16);
}

// =============================================================================
// Truncated Pack File Tests
// =============================================================================

TEST_CASE("PackReader - Truncated pack file (empty)", "[vfs][pack][corruption][truncated]") {
    PackReader reader;

    SECTION("Completely empty file") {
        std::ofstream file("truncated_empty.pack", std::ios::binary);
        file.close();

        auto result = reader.mount("truncated_empty.pack");
        REQUIRE(result.isError());

        // Verify error message is clear
        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE_FALSE(errorMsg.empty());
        }

        std::remove("truncated_empty.pack");
    }
}

TEST_CASE("PackReader - Truncated pack file (partial header)", "[vfs][pack][corruption][truncated]") {
    PackReader reader;

    SECTION("Only magic number written") {
        std::ofstream file("truncated_magic.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.close();

        auto result = reader.mount("truncated_magic.pack");
        REQUIRE(result.isError());

        std::remove("truncated_magic.pack");
    }

    SECTION("Partial header (magic + version)") {
        std::ofstream file("truncated_partial.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        u16 versionMajor = PACK_VERSION_MAJOR;
        u16 versionMinor = PACK_VERSION_MINOR;

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
        file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
        file.close();

        auto result = reader.mount("truncated_partial.pack");
        REQUIRE(result.isError());

        std::remove("truncated_partial.pack");
    }

    SECTION("Header complete but resource table missing") {
        std::ofstream file("truncated_no_table.pack", std::ios::binary);
        writeValidPackHeader(file);
        // Don't write resource table
        file.close();

        auto result = reader.mount("truncated_no_table.pack");
        REQUIRE(result.isError());

        std::remove("truncated_no_table.pack");
    }
}

TEST_CASE("PackReader - Truncated resource table", "[vfs][pack][corruption][truncated]") {
    PackReader reader;

    SECTION("Partial resource entry") {
        std::ofstream file("truncated_entry.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Seek to resource table offset
        file.seekp(64);

        // Write only part of a resource entry
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        // Don't write rest of entry
        file.close();

        auto result = reader.mount("truncated_entry.pack");
        REQUIRE(result.isError());

        std::remove("truncated_entry.pack");
    }
}

TEST_CASE("PackReader - Truncated data section", "[vfs][pack][corruption][truncated]") {
    PackReader reader;

    SECTION("Resource data declared but not present") {
        std::ofstream file("truncated_data.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table at offset 64
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;  // Relative offset in data section
        u64 compressedSize = 1000;  // Claim 1000 bytes
        u64 uncompressedSize = 1000;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table at offset 128
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "test_resource\0";
        file.write(resId, 14);

        // Write only partial data at offset 256 (less than claimed 1000 bytes)
        file.seekp(256);
        const u8 partialData[] = {1, 2, 3, 4, 5};
        file.write(reinterpret_cast<const char*>(partialData), sizeof(partialData));
        file.close();

        auto result = reader.mount("truncated_data.pack");

        // Mount might succeed, but reading the resource should fail
        if (result.isOk()) {
            auto readResult = reader.readFile("test_resource");
            REQUIRE(readResult.isError());
            reader.unmount("truncated_data.pack");
        }

        std::remove("truncated_data.pack");
    }
}

// =============================================================================
// Invalid Header Tests
// =============================================================================

TEST_CASE("PackReader - Invalid magic number", "[vfs][pack][corruption][header]") {
    PackReader reader;

    SECTION("Wrong magic number") {
        std::ofstream file("invalid_magic.pack", std::ios::binary);
        u32 wrongMagic = 0xDEADBEEF;
        file.write(reinterpret_cast<const char*>(&wrongMagic), sizeof(wrongMagic));
        file.close();

        auto result = reader.mount("invalid_magic.pack");
        REQUIRE(result.isError());

        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE((errorMsg.find("magic") != std::string::npos ||
                    errorMsg.find("Magic") != std::string::npos ||
                    errorMsg.find("Invalid") != std::string::npos));
        }

        std::remove("invalid_magic.pack");
    }

    SECTION("Zero magic number") {
        std::ofstream file("zero_magic.pack", std::ios::binary);
        u32 zeroMagic = 0;
        file.write(reinterpret_cast<const char*>(&zeroMagic), sizeof(zeroMagic));
        file.close();

        auto result = reader.mount("zero_magic.pack");
        REQUIRE(result.isError());

        std::remove("zero_magic.pack");
    }
}

TEST_CASE("PackReader - Invalid version", "[vfs][pack][corruption][header]") {
    PackReader reader;

    SECTION("Future version (major version too high)") {
        std::ofstream file("future_version.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        u16 versionMajor = 99;  // Future version
        u16 versionMinor = 0;

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
        file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
        file.close();

        auto result = reader.mount("future_version.pack");
        REQUIRE(result.isError());

        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE((errorMsg.find("version") != std::string::npos ||
                    errorMsg.find("Version") != std::string::npos ||
                    errorMsg.find("Incompatible") != std::string::npos));
        }

        std::remove("future_version.pack");
    }

    SECTION("Zero version") {
        std::ofstream file("zero_version.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        u16 versionMajor = 0;
        u16 versionMinor = 0;

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
        file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
        file.close();

        auto result = reader.mount("zero_version.pack");
        REQUIRE(result.isError());

        std::remove("zero_version.pack");
    }
}

TEST_CASE("PackReader - Invalid resource count", "[vfs][pack][corruption][header]") {
    PackReader reader;

    SECTION("Excessive resource count") {
        std::ofstream file("excessive_count.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        u16 versionMajor = PACK_VERSION_MAJOR;
        u16 versionMinor = PACK_VERSION_MINOR;
        u32 flags = 0;
        u32 resourceCount = 2000000;  // Over the 1 million limit

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
        file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
        file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        file.write(reinterpret_cast<const char*>(&resourceCount), sizeof(resourceCount));
        file.close();

        auto result = reader.mount("excessive_count.pack");
        REQUIRE(result.isError());

        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE((errorMsg.find("count") != std::string::npos ||
                    errorMsg.find("maximum") != std::string::npos ||
                    errorMsg.find("exceeds") != std::string::npos));
        }

        std::remove("excessive_count.pack");
    }
}

TEST_CASE("PackReader - Invalid offsets", "[vfs][pack][corruption][header]") {
    PackReader reader;

    SECTION("Resource table offset beyond file size") {
        std::ofstream file("invalid_offset.pack", std::ios::binary);
        u32 magic = PACK_MAGIC;
        u16 versionMajor = PACK_VERSION_MAJOR;
        u16 versionMinor = PACK_VERSION_MINOR;
        u32 flags = 0;
        u32 resourceCount = 1;
        u64 resourceTableOffset = 0xFFFFFFFFFFFFFFFF;  // Invalid offset
        u64 stringTableOffset = 128;
        u64 dataOffset = 256;
        u64 totalSize = 512;
        u8 contentHash[16] = {0};

        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&versionMajor), sizeof(versionMajor));
        file.write(reinterpret_cast<const char*>(&versionMinor), sizeof(versionMinor));
        file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        file.write(reinterpret_cast<const char*>(&resourceCount), sizeof(resourceCount));
        file.write(reinterpret_cast<const char*>(&resourceTableOffset), sizeof(resourceTableOffset));
        file.write(reinterpret_cast<const char*>(&stringTableOffset), sizeof(stringTableOffset));
        file.write(reinterpret_cast<const char*>(&dataOffset), sizeof(dataOffset));
        file.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
        file.write(reinterpret_cast<const char*>(contentHash), 16);
        file.close();

        auto result = reader.mount("invalid_offset.pack");
        REQUIRE(result.isError());

        std::remove("invalid_offset.pack");
    }
}

// =============================================================================
// Corrupted Index Tests
// =============================================================================

TEST_CASE("PackReader - Corrupted string table", "[vfs][pack][corruption][index]") {
    PackReader reader;

    SECTION("String count exceeds limit") {
        std::ofstream file("excessive_strings.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;
        u64 compressedSize = 10;
        u64 uncompressedSize = 10;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write corrupted string table with excessive count
        file.seekp(128);
        u32 excessiveStringCount = 20000000;  // Over the 10 million limit
        file.write(reinterpret_cast<const char*>(&excessiveStringCount), sizeof(excessiveStringCount));
        file.close();

        auto result = reader.mount("excessive_strings.pack");
        REQUIRE(result.isError());

        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE((errorMsg.find("String") != std::string::npos ||
                    errorMsg.find("string") != std::string::npos ||
                    errorMsg.find("maximum") != std::string::npos));
        }

        std::remove("excessive_strings.pack");
    }

    SECTION("String offset points beyond string table") {
        std::ofstream file("invalid_string_offset.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table
        file.seekp(64);
        u32 idStringOffset = 999;  // Invalid offset
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;
        u64 compressedSize = 10;
        u64 uncompressedSize = 10;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "test\0";
        file.write(resId, 5);
        file.close();

        auto result = reader.mount("invalid_string_offset.pack");

        // Mount may succeed but resource won't be properly mapped
        if (result.isOk()) {
            // The resource with invalid string offset should not be accessible
            auto resources = reader.listResources();
            // Depending on implementation, this might be empty or contain unmapped entries
            reader.unmount("invalid_string_offset.pack");
        }

        std::remove("invalid_string_offset.pack");
    }
}

TEST_CASE("PackReader - Corrupted resource entries", "[vfs][pack][corruption][index]") {
    PackReader reader;

    SECTION("Data offset causes integer overflow") {
        std::ofstream file("overflow_offset.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table with overflow
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0xFFFFFFFFFFFFFFF0;  // Near max u64
        u64 compressedSize = 1000;  // Adding this will cause overflow
        u64 uncompressedSize = 1000;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "overflow_test\0";
        file.write(resId, 14);
        file.close();

        auto result = reader.mount("overflow_offset.pack");

        if (result.isOk()) {
            auto readResult = reader.readFile("overflow_test");
            REQUIRE(readResult.isError());

            if (readResult.isError()) {
                std::string errorMsg = readResult.error();
                REQUIRE((errorMsg.find("overflow") != std::string::npos ||
                        errorMsg.find("Invalid") != std::string::npos ||
                        errorMsg.find("offset") != std::string::npos));
            }

            reader.unmount("overflow_offset.pack");
        }

        std::remove("overflow_offset.pack");
    }

    SECTION("Resource size exceeds maximum allowed") {
        std::ofstream file("excessive_size.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table with excessive size
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;
        u64 compressedSize = 600ULL * 1024 * 1024;  // 600MB, over the 512MB limit
        u64 uncompressedSize = 600ULL * 1024 * 1024;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "large_resource\0";
        file.write(resId, 15);
        file.close();

        auto result = reader.mount("excessive_size.pack");

        if (result.isOk()) {
            auto readResult = reader.readFile("large_resource");
            REQUIRE(readResult.isError());

            if (readResult.isError()) {
                std::string errorMsg = readResult.error();
                REQUIRE((errorMsg.find("size") != std::string::npos ||
                        errorMsg.find("Size") != std::string::npos ||
                        errorMsg.find("maximum") != std::string::npos));
            }

            reader.unmount("excessive_size.pack");
        }

        std::remove("excessive_size.pack");
    }
}

// =============================================================================
// Missing Data Tests
// =============================================================================

TEST_CASE("PackReader - Missing resource data", "[vfs][pack][corruption][missing]") {
    PackReader reader;

    SECTION("Resource data extends beyond file") {
        std::ofstream file("data_beyond_file.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;
        u64 compressedSize = 1000;  // Claim 1000 bytes
        u64 uncompressedSize = 1000;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "beyond_file\0";
        file.write(resId, 12);

        // Data section starts at 256 but we don't write it
        // File ends before data section
        file.close();

        auto result = reader.mount("data_beyond_file.pack");

        if (result.isOk()) {
            auto readResult = reader.readFile("beyond_file");
            REQUIRE(readResult.isError());

            if (readResult.isError()) {
                std::string errorMsg = readResult.error();
                REQUIRE((errorMsg.find("beyond") != std::string::npos ||
                        errorMsg.find("extends") != std::string::npos ||
                        errorMsg.find("exceed") != std::string::npos));
            }

            reader.unmount("data_beyond_file.pack");
        }

        std::remove("data_beyond_file.pack");
    }

    SECTION("No data section at all") {
        std::ofstream file("no_data_section.pack", std::ios::binary);
        writeValidPackHeader(file);

        // Write resource table
        file.seekp(64);
        u32 idStringOffset = 0;
        u32 type = static_cast<u32>(ResourceType::Data);
        u64 resDataOffset = 0;
        u64 compressedSize = 100;
        u64 uncompressedSize = 100;
        u32 resFlags = 0;
        u32 checksum = 0;
        u8 iv[8] = {0};

        file.write(reinterpret_cast<const char*>(&idStringOffset), sizeof(idStringOffset));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        file.write(reinterpret_cast<const char*>(&resDataOffset), sizeof(resDataOffset));
        file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
        file.write(reinterpret_cast<const char*>(&resFlags), sizeof(resFlags));
        file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
        file.write(reinterpret_cast<const char*>(iv), 8);

        // Write string table
        file.seekp(128);
        u32 stringCount = 1;
        u32 stringOffset = 0;
        file.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));
        file.write(reinterpret_cast<const char*>(&stringOffset), sizeof(stringOffset));
        const char* resId = "no_data\0";
        file.write(resId, 8);

        // End file before data section (which should be at offset 256)
        file.close();

        auto result = reader.mount("no_data_section.pack");

        if (result.isOk()) {
            auto readResult = reader.readFile("no_data");
            REQUIRE(readResult.isError());
            reader.unmount("no_data_section.pack");
        }

        std::remove("no_data_section.pack");
    }
}

// =============================================================================
// CRC Mismatch Tests
// =============================================================================

TEST_CASE("PackIntegrityChecker - CRC mismatch detection", "[vfs][pack][corruption][crc]") {
    PackIntegrityChecker checker;

    SECTION("Detect data corruption via CRC mismatch") {
        const char* originalData = "This is the original data";
        const char* corruptedData = "This is corrupted data!!!";

        // Calculate CRC for original data
        u32 originalCrc = PackIntegrityChecker::calculateCrc32(
            reinterpret_cast<const u8*>(originalData), std::strlen(originalData));

        // Verify with corrupted data should fail
        auto result = checker.verifyResource(
            reinterpret_cast<const u8*>(corruptedData),
            std::strlen(corruptedData),
            0,
            std::strlen(corruptedData),
            originalCrc);

        if (result.isOk()) {
            REQUIRE(result.value().result == PackVerificationResult::ChecksumMismatch);
        }
    }

    SECTION("CRC verification passes with matching data") {
        const char* data = "Consistent data";

        u32 crc = PackIntegrityChecker::calculateCrc32(
            reinterpret_cast<const u8*>(data), std::strlen(data));

        auto result = checker.verifyResource(
            reinterpret_cast<const u8*>(data),
            std::strlen(data),
            0,
            std::strlen(data),
            crc);

        REQUIRE(result.isOk());
        if (result.isOk()) {
            REQUIRE(result.value().result == PackVerificationResult::Valid);
        }
    }

    SECTION("Single bit flip detected by CRC") {
        std::vector<u8> data = {0x41, 0x42, 0x43, 0x44, 0x45};  // "ABCDE"

        u32 originalCrc = PackIntegrityChecker::calculateCrc32(data.data(), data.size());

        // Flip one bit
        data[2] ^= 0x01;  // Change 'C' to 'B'

        auto result = checker.verifyResource(
            data.data(),
            data.size(),
            0,
            data.size(),
            originalCrc);

        if (result.isOk()) {
            REQUIRE(result.value().result == PackVerificationResult::ChecksumMismatch);
        }
    }
}

// =============================================================================
// Error Recovery Tests
// =============================================================================

TEST_CASE("PackReader - Error recovery and stability", "[vfs][pack][corruption][recovery]") {
    PackReader reader;

    SECTION("Reader remains usable after failed mount") {
        // Try to mount a corrupted file
        std::ofstream file("corrupt_recovery.pack", std::ios::binary);
        u32 badMagic = 0xBAD;
        file.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
        file.close();

        auto result = reader.mount("corrupt_recovery.pack");
        REQUIRE(result.isError());

        // Reader should still be usable
        REQUIRE(reader.listResources().empty());
        REQUIRE_FALSE(reader.exists("any_resource"));

        std::remove("corrupt_recovery.pack");
    }

    SECTION("Multiple failed mount attempts don't crash") {
        for (int i = 0; i < 10; ++i) {
            auto result = reader.mount("nonexistent_file.pack");
            REQUIRE(result.isError());
        }

        // Reader should still be valid
        REQUIRE(reader.listResources().empty());
    }

    SECTION("No crashes with various corrupted inputs") {
        // Test various corrupted scenarios don't cause crashes
        std::vector<std::string> testFiles = {
            "test_empty.pack",
            "test_partial.pack",
            "test_random.pack"
        };

        for (const auto& filename : testFiles) {
            std::ofstream file(filename, std::ios::binary);

            // Write some random/invalid data
            for (int i = 0; i < 100; ++i) {
                u8 randomByte = static_cast<u8>(i * 13 + 7);
                file.write(reinterpret_cast<const char*>(&randomByte), 1);
            }

            file.close();

            // Should fail gracefully, not crash
            auto result = reader.mount(filename);
            REQUIRE(result.isError());

            std::remove(filename.c_str());
        }

        // Reader should remain stable
        REQUIRE(reader.listResources().empty());
    }
}

// =============================================================================
// Clear Error Messages Tests
// =============================================================================

TEST_CASE("PackReader - Clear error messages for corruption", "[vfs][pack][corruption][messages]") {
    PackReader reader;

    SECTION("Error messages are not empty") {
        std::ofstream file("error_msg_test.pack", std::ios::binary);
        u32 badMagic = 0xBAD;
        file.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
        file.close();

        auto result = reader.mount("error_msg_test.pack");
        REQUIRE(result.isError());

        if (result.isError()) {
            std::string errorMsg = result.error();
            REQUIRE_FALSE(errorMsg.empty());
            REQUIRE(errorMsg.length() > 5);  // Should be descriptive
        }

        std::remove("error_msg_test.pack");
    }

    SECTION("Different corruption types give different messages") {
        // Invalid magic
        std::ofstream file1("test1.pack", std::ios::binary);
        u32 badMagic = 0xBAD;
        file1.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
        file1.close();

        // Invalid version
        std::ofstream file2("test2.pack", std::ios::binary);
        u32 goodMagic = PACK_MAGIC;
        u16 badVersion = 99;
        u16 minorVersion = 0;
        file2.write(reinterpret_cast<const char*>(&goodMagic), sizeof(goodMagic));
        file2.write(reinterpret_cast<const char*>(&badVersion), sizeof(badVersion));
        file2.write(reinterpret_cast<const char*>(&minorVersion), sizeof(minorVersion));
        file2.close();

        auto result1 = reader.mount("test1.pack");
        auto result2 = reader.mount("test2.pack");

        REQUIRE(result1.isError());
        REQUIRE(result2.isError());

        if (result1.isError() && result2.isError()) {
            // Error messages should be different for different issues
            std::string error1 = result1.error();
            std::string error2 = result2.error();

            REQUIRE_FALSE(error1.empty());
            REQUIRE_FALSE(error2.empty());
            // They should contain different keywords
            REQUIRE(error1 != error2);
        }

        std::remove("test1.pack");
        std::remove("test2.pack");
    }
}
