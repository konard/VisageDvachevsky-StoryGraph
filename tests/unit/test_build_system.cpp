#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/build_system.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>

using namespace NovelMind;
using namespace NovelMind::editor;
namespace fs = std::filesystem;

// Test fixture helpers
static std::string createTempDir() {
    std::string tempPath = fs::temp_directory_path().string() + "/nm_build_test_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(tempPath);
    return tempPath;
}

static void cleanupTempDir(const std::string& path) {
    if (fs::exists(path)) {
        fs::remove_all(path);
    }
}

// =============================================================================
// CRC32 Tests
// =============================================================================

TEST_CASE("CRC32 calculation produces expected values", "[build_system][crc32]")
{
    SECTION("Empty data") {
        u8 empty[] = {};
        u32 crc = BuildSystem::calculateCrc32(empty, 0);
        // CRC32 of empty data is 0 (initial XOR with final)
        REQUIRE(crc == 0);
    }

    SECTION("Single byte") {
        u8 data[] = {'a'};
        u32 crc = BuildSystem::calculateCrc32(data, 1);
        REQUIRE(crc != 0);
    }

    SECTION("Same input produces same CRC") {
        u8 data[] = "Hello, World!";
        u32 crc1 = BuildSystem::calculateCrc32(data, sizeof(data) - 1);
        u32 crc2 = BuildSystem::calculateCrc32(data, sizeof(data) - 1);
        REQUIRE(crc1 == crc2);
    }

    SECTION("Different input produces different CRC") {
        u8 data1[] = "Hello";
        u8 data2[] = "World";
        u32 crc1 = BuildSystem::calculateCrc32(data1, sizeof(data1) - 1);
        u32 crc2 = BuildSystem::calculateCrc32(data2, sizeof(data2) - 1);
        REQUIRE(crc1 != crc2);
    }
}

// =============================================================================
// SHA256 Tests
// =============================================================================

TEST_CASE("SHA256 calculation produces consistent hashes", "[build_system][sha256]")
{
    SECTION("Same input produces same hash") {
        u8 data[] = "NovelMind Test Data";
        auto hash1 = BuildSystem::calculateSha256(data, sizeof(data) - 1);
        auto hash2 = BuildSystem::calculateSha256(data, sizeof(data) - 1);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Different input produces different hash") {
        u8 data1[] = "Input1";
        u8 data2[] = "Input2";
        auto hash1 = BuildSystem::calculateSha256(data1, sizeof(data1) - 1);
        auto hash2 = BuildSystem::calculateSha256(data2, sizeof(data2) - 1);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Hash is 32 bytes") {
        u8 data[] = "test";
        auto hash = BuildSystem::calculateSha256(data, sizeof(data) - 1);
        REQUIRE(hash.size() == 32);
    }
}

// =============================================================================
// VFS Path Normalization Tests
// =============================================================================

TEST_CASE("VFS path normalization", "[build_system][vfs]")
{
    SECTION("Converts backslashes to forward slashes") {
        std::string path = "assets\\images\\bg.png";
        std::string normalized = BuildSystem::normalizeVfsPath(path);
        REQUIRE(normalized.find('\\') == std::string::npos);
        REQUIRE(normalized == "assets/images/bg.png");
    }

    SECTION("Converts to lowercase") {
        std::string path = "Assets/Images/BG.PNG";
        std::string normalized = BuildSystem::normalizeVfsPath(path);
        REQUIRE(normalized == "assets/images/bg.png");
    }

    SECTION("Removes leading slashes") {
        std::string path = "/assets/image.png";
        std::string normalized = BuildSystem::normalizeVfsPath(path);
        REQUIRE(normalized[0] != '/');
        REQUIRE(normalized == "assets/image.png");
    }

    SECTION("Removes trailing slashes") {
        std::string path = "assets/folder/";
        std::string normalized = BuildSystem::normalizeVfsPath(path);
        REQUIRE(normalized.back() != '/');
        REQUIRE(normalized == "assets/folder");
    }

    SECTION("Handles empty string") {
        std::string path = "";
        std::string normalized = BuildSystem::normalizeVfsPath(path);
        REQUIRE(normalized.empty());
    }
}

// =============================================================================
// Resource Type Detection Tests
// =============================================================================

TEST_CASE("Resource type detection from extension", "[build_system][resource_type]")
{
    SECTION("Texture types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.png") == ResourceType::Texture);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.jpg") == ResourceType::Texture);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.jpeg") == ResourceType::Texture);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.bmp") == ResourceType::Texture);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.webp") == ResourceType::Texture);
    }

    SECTION("Audio types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.wav") == ResourceType::Audio);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.flac") == ResourceType::Audio);
    }

    SECTION("Music types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.ogg") == ResourceType::Music);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.mp3") == ResourceType::Music);
    }

    SECTION("Font types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.ttf") == ResourceType::Font);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.otf") == ResourceType::Font);
    }

    SECTION("Script types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.nms") == ResourceType::Script);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.nmscript") == ResourceType::Script);
    }

    SECTION("Data types") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.json") == ResourceType::Data);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.xml") == ResourceType::Data);
    }

    SECTION("Case insensitive") {
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.PNG") == ResourceType::Texture);
        REQUIRE(BuildSystem::getResourceTypeFromExtension("test.OGG") == ResourceType::Music);
    }
}

// =============================================================================
// Build Configuration Tests
// =============================================================================

TEST_CASE("BuildConfig default values", "[build_system][config]")
{
    BuildConfig config;

    REQUIRE(config.version == "1.0.0");
    REQUIRE(config.buildNumber == 1);
    REQUIRE(config.platform == BuildPlatform::Windows);
    REQUIRE(config.buildType == BuildType::Release);
    REQUIRE(config.packAssets == true);
    REQUIRE(config.encryptAssets == false);
    REQUIRE(config.compression == CompressionLevel::Balanced);
    REQUIRE(config.deterministicBuild == true);
    REQUIRE(config.fixedBuildTimestamp == 0);
    REQUIRE(config.signPacks == false);
}

// =============================================================================
// Pack File Format Tests
// =============================================================================

TEST_CASE("Pack file format validation", "[build_system][pack]")
{
    std::string tempDir = createTempDir();

    SECTION("Empty pack has valid structure") {
        std::string packPath = tempDir + "/empty.nmres";

        BuildConfig config;
        config.projectPath = tempDir;
        config.outputPath = tempDir;
        config.deterministicBuild = true;
        config.fixedBuildTimestamp = 1704067200; // Fixed timestamp

        BuildSystem buildSystem;
        buildSystem.configure(config);

        std::vector<std::string> emptyFiles;
        auto result = buildSystem.buildPack(packPath, emptyFiles, false, false);
        REQUIRE(result.isOk());
        REQUIRE(fs::exists(packPath));

        // Verify pack structure
        std::ifstream packFile(packPath, std::ios::binary);
        REQUIRE(packFile.is_open());

        // Check magic number (4 bytes)
        char magic[4];
        packFile.read(magic, 4);
        REQUIRE(std::strncmp(magic, "NMRS", 4) == 0);

        // Check version (4 bytes total: 2 major + 2 minor)
        u16 versionMajor, versionMinor;
        packFile.read(reinterpret_cast<char*>(&versionMajor), 2);
        packFile.read(reinterpret_cast<char*>(&versionMinor), 2);
        REQUIRE(versionMajor == 1);
        REQUIRE(versionMinor == 0);

        // Check resource count (4 bytes)
        u32 resourceCount;
        packFile.read(reinterpret_cast<char*>(&resourceCount), 4);
        REQUIRE(resourceCount == 0);

        // Verify footer magic at end
        packFile.seekg(-32, std::ios::end);
        char footerMagic[4];
        packFile.read(footerMagic, 4);
        REQUIRE(std::strncmp(footerMagic, "NMRF", 4) == 0);

        packFile.close();
    }

    cleanupTempDir(tempDir);
}

// =============================================================================
// Deterministic Build Tests
// =============================================================================

TEST_CASE("Deterministic build timestamp", "[build_system][determinism]")
{
    BuildConfig config;
    config.deterministicBuild = true;
    config.fixedBuildTimestamp = 1704067200; // 2024-01-01 00:00:00 UTC

    BuildSystem buildSystem;
    buildSystem.configure(config);

    SECTION("Uses fixed timestamp when set") {
        u64 ts = buildSystem.getBuildTimestamp();
        REQUIRE(ts == 1704067200);
    }

    SECTION("Returns consistent timestamp on multiple calls") {
        u64 ts1 = buildSystem.getBuildTimestamp();
        u64 ts2 = buildSystem.getBuildTimestamp();
        REQUIRE(ts1 == ts2);
    }
}

TEST_CASE("Deterministic build uses current time when no fixed timestamp", "[build_system][determinism]")
{
    BuildConfig config;
    config.deterministicBuild = true;
    config.fixedBuildTimestamp = 0; // No fixed timestamp

    BuildSystem buildSystem;
    buildSystem.configure(config);

    u64 ts = buildSystem.getBuildTimestamp();
    u64 now = static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    // Timestamp should be close to current time (within 5 seconds)
    REQUIRE(ts >= now - 5);
    REQUIRE(ts <= now + 5);
}

// =============================================================================
// Key Management Tests
// =============================================================================

TEST_CASE("Encryption key loading from file", "[build_system][encryption]")
{
    std::string tempDir = createTempDir();

    SECTION("Loads 32-byte key file") {
        std::string keyPath = tempDir + "/test.key";

        // Create a 32-byte key file
        std::vector<u8> testKey(32, 0xAB);
        std::ofstream keyFile(keyPath, std::ios::binary);
        keyFile.write(reinterpret_cast<const char*>(testKey.data()), 32);
        keyFile.close();

        auto result = BuildSystem::loadEncryptionKeyFromFile(keyPath);
        REQUIRE(result.isOk());
        REQUIRE(result.value().size() == 32);
        REQUIRE(result.value()[0] == 0xAB);
    }

    SECTION("Rejects invalid key file") {
        std::string keyPath = tempDir + "/short.key";

        // Create a too-short key file
        std::vector<u8> shortKey(16, 0x00);
        std::ofstream keyFile(keyPath, std::ios::binary);
        keyFile.write(reinterpret_cast<const char*>(shortKey.data()), 16);
        keyFile.close();

        auto result = BuildSystem::loadEncryptionKeyFromFile(keyPath);
        REQUIRE(result.isError());
    }

    SECTION("Returns error for non-existent file") {
        auto result = BuildSystem::loadEncryptionKeyFromFile("/nonexistent/key.bin");
        REQUIRE(result.isError());
    }

    cleanupTempDir(tempDir);
}

// =============================================================================
// Compression Tests
// =============================================================================

TEST_CASE("Data compression", "[build_system][compression]")
{
    std::vector<u8> testData(1024, 'A'); // Highly compressible data

    SECTION("Compression with None level returns original") {
        auto result = BuildSystem::compressData(testData, CompressionLevel::None);
        REQUIRE(result.isOk());
        REQUIRE(result.value() == testData);
    }

    // Note: Other compression levels depend on zlib availability
}

// =============================================================================
// Build Utilities Tests
// =============================================================================

TEST_CASE("BuildUtils helper functions", "[build_system][utils]")
{
    SECTION("Platform name") {
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::Windows) == "Windows");
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::Linux) == "Linux");
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::MacOS) == "macOS");
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::Web) == "Web");
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::Android) == "Android");
        REQUIRE(BuildUtils::getPlatformName(BuildPlatform::iOS) == "iOS");
    }

    SECTION("Executable extension") {
        REQUIRE(BuildUtils::getExecutableExtension(BuildPlatform::Windows) == ".exe");
        REQUIRE(BuildUtils::getExecutableExtension(BuildPlatform::Linux) == "");
        REQUIRE(BuildUtils::getExecutableExtension(BuildPlatform::MacOS) == "");
    }

    SECTION("File size formatting") {
        REQUIRE(BuildUtils::formatFileSize(0) == "0 B");
        REQUIRE(BuildUtils::formatFileSize(512) == "512 B");
        REQUIRE(BuildUtils::formatFileSize(1024) == "1.00 KB");
        REQUIRE(BuildUtils::formatFileSize(1024 * 1024) == "1.00 MB");
        REQUIRE(BuildUtils::formatFileSize(1024LL * 1024 * 1024) == "1.00 GB");
    }
}
