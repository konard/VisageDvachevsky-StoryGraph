#include <catch2/catch_test_macros.hpp>
#include "NovelMind/save/save_manager.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace NovelMind::save;

// Test fixture to manage temporary test directory
class SaveManagerTestFixture {
public:
    SaveManagerTestFixture() {
        testDir = std::filesystem::temp_directory_path() / "novelmind_save_tests";
        std::filesystem::create_directories(testDir);
    }

    ~SaveManagerTestFixture() {
        try {
            std::filesystem::remove_all(testDir);
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    std::string getTestPath() const {
        return testDir.string();
    }

    std::filesystem::path testDir;
};

// Helper function to create a basic SaveData for testing
SaveData createTestSaveData() {
    SaveData data;
    data.sceneId = "test_scene_1";
    data.nodeId = "test_node_42";
    data.intVariables["health"] = 100;
    data.intVariables["score"] = 9999;
    data.floatVariables["stamina"] = 75.5f;
    data.floatVariables["speed"] = 10.0f;
    data.flags["quest_completed"] = true;
    data.flags["tutorial_shown"] = false;
    data.stringVariables["player_name"] = "TestPlayer";
    data.stringVariables["current_location"] = "Village";
    data.timestamp = 0;
    data.checksum = 0;
    return data;
}

// Helper function to verify SaveData equality
bool savesAreEqual(const SaveData& a, const SaveData& b) {
    if (a.sceneId != b.sceneId || a.nodeId != b.nodeId) return false;
    if (a.intVariables != b.intVariables) return false;
    if (a.floatVariables != b.floatVariables) return false;
    if (a.flags != b.flags) return false;
    if (a.stringVariables != b.stringVariables) return false;
    if (a.thumbnailWidth != b.thumbnailWidth || a.thumbnailHeight != b.thumbnailHeight) return false;
    if (a.thumbnailData != b.thumbnailData) return false;
    return true;
}

// ============================================================================
// SECTION: Basic Save Creation and Loading
// ============================================================================

TEST_CASE("SaveManager basic construction", "[save_manager][basic]") {
    SaveManager manager;
    REQUIRE(manager.getMaxSlots() == 100);
    REQUIRE(manager.getSavePath() == "./saves/");
}

TEST_CASE("SaveManager custom save path", "[save_manager][basic]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;

    std::string testPath = fixture.getTestPath();
    manager.setSavePath(testPath);

    REQUIRE(manager.getSavePath() == testPath + "/");
}

TEST_CASE("SaveManager save path adds trailing slash", "[save_manager][basic]") {
    SaveManager manager;
    manager.setSavePath("/custom/path");
    REQUIRE(manager.getSavePath() == "/custom/path/");
}

TEST_CASE("SaveManager save and load to slot", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData original = createTestSaveData();

    // Save to slot 5
    auto saveResult = manager.save(5, original);
    REQUIRE(saveResult.isOk());

    // Verify slot exists
    REQUIRE(manager.slotExists(5));

    // Load from slot 5
    auto loadResult = manager.load(5);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(savesAreEqual(original, loaded));

    // Timestamp and checksum should be set by save operation
    REQUIRE(loaded.timestamp > 0);
    REQUIRE(loaded.checksum > 0);
}

TEST_CASE("SaveManager save to multiple slots", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data1 = createTestSaveData();
    data1.sceneId = "scene_1";

    SaveData data2 = createTestSaveData();
    data2.sceneId = "scene_2";

    SaveData data3 = createTestSaveData();
    data3.sceneId = "scene_3";

    // Save to different slots
    REQUIRE(manager.save(0, data1).isOk());
    REQUIRE(manager.save(10, data2).isOk());
    REQUIRE(manager.save(99, data3).isOk());

    // Verify all slots exist
    REQUIRE(manager.slotExists(0));
    REQUIRE(manager.slotExists(10));
    REQUIRE(manager.slotExists(99));

    // Load and verify
    auto loaded1 = manager.load(0);
    auto loaded2 = manager.load(10);
    auto loaded3 = manager.load(99);

    REQUIRE(loaded1.isOk());
    REQUIRE(loaded2.isOk());
    REQUIRE(loaded3.isOk());

    REQUIRE(loaded1.value().sceneId == "scene_1");
    REQUIRE(loaded2.value().sceneId == "scene_2");
    REQUIRE(loaded3.value().sceneId == "scene_3");
}

TEST_CASE("SaveManager save with empty data", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData empty;
    empty.sceneId = "empty_scene";
    empty.nodeId = "empty_node";

    auto saveResult = manager.save(0, empty);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.sceneId == "empty_scene");
    REQUIRE(loaded.nodeId == "empty_node");
    REQUIRE(loaded.intVariables.empty());
    REQUIRE(loaded.floatVariables.empty());
    REQUIRE(loaded.flags.empty());
    REQUIRE(loaded.stringVariables.empty());
}

TEST_CASE("SaveManager save with thumbnail data", "[save_manager][save_load][thumbnail]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Create fake thumbnail data (e.g., 10x10 RGBA image)
    data.thumbnailWidth = 10;
    data.thumbnailHeight = 10;
    data.thumbnailData.resize(10 * 10 * 4, 0xFF);

    auto saveResult = manager.save(0, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.thumbnailWidth == 10);
    REQUIRE(loaded.thumbnailHeight == 10);
    REQUIRE(loaded.thumbnailData.size() == 400);
}

// ============================================================================
// SECTION: Invalid Slot Handling
// ============================================================================

TEST_CASE("SaveManager invalid slot numbers", "[save_manager][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Test negative slot
    auto result1 = manager.save(-1, data);
    REQUIRE(result1.isError());
    REQUIRE(result1.error() == "Invalid save slot");

    // Test slot >= MAX_SLOTS
    auto result2 = manager.save(100, data);
    REQUIRE(result2.isError());
    REQUIRE(result2.error() == "Invalid save slot");

    // Test load with invalid slots
    auto result3 = manager.load(-5);
    REQUIRE(result3.isError());
    REQUIRE(result3.error() == "Invalid save slot");

    auto result4 = manager.load(150);
    REQUIRE(result4.isError());
    REQUIRE(result4.error() == "Invalid save slot");
}

TEST_CASE("SaveManager load non-existent slot", "[save_manager][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Try to load from slot that doesn't exist
    auto result = manager.load(42);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("not found") != std::string::npos);
}

TEST_CASE("SaveManager slotExists returns false for non-existent slots", "[save_manager][slot_management]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    REQUIRE_FALSE(manager.slotExists(0));
    REQUIRE_FALSE(manager.slotExists(50));
    REQUIRE_FALSE(manager.slotExists(99));
}

// ============================================================================
// SECTION: Delete Save
// ============================================================================

TEST_CASE("SaveManager delete existing save", "[save_manager][slot_management]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Create a save
    REQUIRE(manager.save(10, data).isOk());
    REQUIRE(manager.slotExists(10));

    // Delete it
    auto deleteResult = manager.deleteSave(10);
    REQUIRE(deleteResult.isOk());

    // Verify it's gone
    REQUIRE_FALSE(manager.slotExists(10));

    // Try to load - should fail
    auto loadResult = manager.load(10);
    REQUIRE(loadResult.isError());
}

TEST_CASE("SaveManager delete non-existent save", "[save_manager][slot_management]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Try to delete a slot that doesn't exist
    auto result = manager.deleteSave(42);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Failed to delete") != std::string::npos);
}

TEST_CASE("SaveManager delete with invalid slot number", "[save_manager][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    auto result1 = manager.deleteSave(-1);
    REQUIRE(result1.isError());
    REQUIRE(result1.error() == "Invalid save slot");

    auto result2 = manager.deleteSave(100);
    REQUIRE(result2.isError());
    REQUIRE(result2.error() == "Invalid save slot");
}

// ============================================================================
// SECTION: Autosave
// ============================================================================

TEST_CASE("SaveManager autosave functionality", "[save_manager][autosave]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Initially no autosave exists
    REQUIRE_FALSE(manager.autoSaveExists());

    // Create autosave
    auto saveResult = manager.saveAuto(data);
    REQUIRE(saveResult.isOk());

    // Verify autosave exists
    REQUIRE(manager.autoSaveExists());

    // Load autosave
    auto loadResult = manager.loadAuto();
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(savesAreEqual(data, loaded));
}

TEST_CASE("SaveManager autosave overwrite", "[save_manager][autosave]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data1 = createTestSaveData();
    data1.sceneId = "autosave_1";

    SaveData data2 = createTestSaveData();
    data2.sceneId = "autosave_2";

    // First autosave
    REQUIRE(manager.saveAuto(data1).isOk());

    // Second autosave (should overwrite)
    REQUIRE(manager.saveAuto(data2).isOk());

    // Load should return the second one
    auto loadResult = manager.loadAuto();
    REQUIRE(loadResult.isOk());
    REQUIRE(loadResult.value().sceneId == "autosave_2");
}

TEST_CASE("SaveManager load autosave when none exists", "[save_manager][autosave][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    auto result = manager.loadAuto();
    REQUIRE(result.isError());
    REQUIRE(result.error().find("not found") != std::string::npos);
}

TEST_CASE("SaveManager autosave timing simulation", "[save_manager][autosave]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Simulate multiple autosaves over time
    for (int i = 0; i < 5; i++) {
        SaveData data = createTestSaveData();
        data.sceneId = "autosave_iteration_" + std::to_string(i);

        REQUIRE(manager.saveAuto(data).isOk());

        // Small delay to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Final load should have the last iteration
    auto result = manager.loadAuto();
    REQUIRE(result.isOk());
    REQUIRE(result.value().sceneId == "autosave_iteration_4");
}

// ============================================================================
// SECTION: Metadata and Timestamps
// ============================================================================

TEST_CASE("SaveManager get slot timestamp", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Save to slot
    REQUIRE(manager.save(5, data).isOk());

    // Get timestamp
    auto timestamp = manager.getSlotTimestamp(5);
    REQUIRE(timestamp.has_value());
    REQUIRE(timestamp.value() > 0);
}

TEST_CASE("SaveManager get timestamp for non-existent slot", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    auto timestamp = manager.getSlotTimestamp(42);
    REQUIRE_FALSE(timestamp.has_value());
}

TEST_CASE("SaveManager get slot metadata", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    data.thumbnailWidth = 128;
    data.thumbnailHeight = 128;
    data.thumbnailData.resize(128 * 128 * 4, 0x80);

    REQUIRE(manager.save(7, data).isOk());

    auto metadata = manager.getSlotMetadata(7);
    REQUIRE(metadata.has_value());
    REQUIRE(metadata->timestamp > 0);
    REQUIRE(metadata->hasThumbnail == true);
    REQUIRE(metadata->thumbnailWidth == 128);
    REQUIRE(metadata->thumbnailHeight == 128);
    REQUIRE(metadata->thumbnailSize == 128 * 128 * 4);
}

TEST_CASE("SaveManager metadata for slot without thumbnail", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    // No thumbnail data

    REQUIRE(manager.save(8, data).isOk());

    auto metadata = manager.getSlotMetadata(8);
    REQUIRE(metadata.has_value());
    REQUIRE(metadata->hasThumbnail == false);
    REQUIRE(metadata->thumbnailSize == 0);
}

TEST_CASE("SaveManager metadata for invalid slots", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    REQUIRE_FALSE(manager.getSlotMetadata(-1).has_value());
    REQUIRE_FALSE(manager.getSlotMetadata(100).has_value());
    REQUIRE_FALSE(manager.getSlotMetadata(500).has_value());
}

TEST_CASE("SaveManager timestamps are monotonically increasing", "[save_manager][metadata]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Save to multiple slots with delays
    REQUIRE(manager.save(0, data).isOk());
    auto ts1 = manager.getSlotTimestamp(0);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(manager.save(1, data).isOk());
    auto ts2 = manager.getSlotTimestamp(1);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(manager.save(2, data).isOk());
    auto ts3 = manager.getSlotTimestamp(2);

    REQUIRE(ts1.has_value());
    REQUIRE(ts2.has_value());
    REQUIRE(ts3.has_value());

    // Later saves should have later timestamps
    REQUIRE(ts2.value() >= ts1.value());
    REQUIRE(ts3.value() >= ts2.value());
}

// ============================================================================
// SECTION: Configuration
// ============================================================================

TEST_CASE("SaveManager default configuration", "[save_manager][config]") {
    SaveManager manager;

    SaveConfig config = manager.getConfig();
    REQUIRE(config.enableCompression == true);
    REQUIRE(config.enableEncryption == false);
    REQUIRE(config.encryptionKey.empty());
}

TEST_CASE("SaveManager set configuration", "[save_manager][config]") {
    SaveManager manager;

    SaveConfig config;
    config.enableCompression = false;
    config.enableEncryption = false;

    manager.setConfig(config);

    SaveConfig retrieved = manager.getConfig();
    REQUIRE(retrieved.enableCompression == false);
    REQUIRE(retrieved.enableEncryption == false);
}

TEST_CASE("SaveManager save with compression disabled", "[save_manager][config]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveConfig config;
    config.enableCompression = false;
    manager.setConfig(config);

    SaveData data = createTestSaveData();

    auto saveResult = manager.save(0, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());
    REQUIRE(savesAreEqual(data, loadResult.value()));
}

#if defined(NOVELMIND_HAS_ZLIB)
TEST_CASE("SaveManager save with compression enabled", "[save_manager][compression]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveConfig config;
    config.enableCompression = true;
    manager.setConfig(config);

    // Create data with repetitive content (good for compression)
    SaveData data = createTestSaveData();
    for (int i = 0; i < 100; i++) {
        data.stringVariables["repeated_key_" + std::to_string(i)] = "repeated_value_repeated_value_repeated_value";
    }

    auto saveResult = manager.save(0, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());
    REQUIRE(savesAreEqual(data, loadResult.value()));
}
#endif

#if defined(NOVELMIND_HAS_OPENSSL)
TEST_CASE("SaveManager save with encryption enabled", "[save_manager][encryption]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveConfig config;
    config.enableEncryption = true;
    // Create a 32-byte (256-bit) encryption key
    config.encryptionKey.resize(32);
    for (size_t i = 0; i < 32; i++) {
        config.encryptionKey[i] = static_cast<NovelMind::u8>(i * 7 + 13);
    }
    manager.setConfig(config);

    SaveData data = createTestSaveData();
    data.stringVariables["secret"] = "encrypted_content";

    auto saveResult = manager.save(0, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());
    REQUIRE(savesAreEqual(data, loadResult.value()));
    REQUIRE(loadResult.value().stringVariables["secret"] == "encrypted_content");
}

TEST_CASE("SaveManager encryption with wrong key fails", "[save_manager][encryption][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveConfig config;
    config.enableEncryption = true;
    config.encryptionKey.resize(32, 0x42);
    manager.setConfig(config);

    SaveData data = createTestSaveData();
    REQUIRE(manager.save(0, data).isOk());

    // Change the encryption key
    SaveConfig wrongConfig;
    wrongConfig.enableEncryption = true;
    wrongConfig.encryptionKey.resize(32, 0x99);
    manager.setConfig(wrongConfig);

    // Load should fail with wrong key
    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isError());
}

TEST_CASE("SaveManager encryption with compression", "[save_manager][encryption][compression]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveConfig config;
    config.enableCompression = true;
    config.enableEncryption = true;
    config.encryptionKey.resize(32);
    for (size_t i = 0; i < 32; i++) {
        config.encryptionKey[i] = static_cast<NovelMind::u8>(i);
    }
    manager.setConfig(config);

    SaveData data = createTestSaveData();
    for (int i = 0; i < 50; i++) {
        data.intVariables["var_" + std::to_string(i)] = i * 100;
    }

    auto saveResult = manager.save(0, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(0);
    REQUIRE(loadResult.isOk());
    REQUIRE(savesAreEqual(data, loadResult.value()));
}
#endif

// ============================================================================
// SECTION: Corruption Detection
// ============================================================================

TEST_CASE("SaveManager detect corrupted file - invalid magic", "[save_manager][corruption]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Create a valid save first
    SaveData data = createTestSaveData();
    REQUIRE(manager.save(0, data).isOk());

    // Corrupt the file by changing the magic number
    std::string filename = fixture.getTestPath() + "/save_0.nmsav";
    std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
    REQUIRE(file.is_open());

    NovelMind::u32 badMagic = 0xDEADBEEF;
    file.write(reinterpret_cast<const char*>(&badMagic), sizeof(badMagic));
    file.close();

    // Try to load - should fail
    auto result = manager.load(0);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Invalid save file format") != std::string::npos);
}

TEST_CASE("SaveManager detect corrupted file - checksum mismatch", "[save_manager][corruption]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    REQUIRE(manager.save(0, data).isOk());

    // Read the entire file
    std::string filename = fixture.getTestPath() + "/save_0.nmsav";
    std::ifstream readFile(filename, std::ios::binary);
    std::vector<NovelMind::u8> fileContents(
        (std::istreambuf_iterator<char>(readFile)),
        std::istreambuf_iterator<char>()
    );
    readFile.close();

    // Corrupt some data in the payload (near the end)
    if (fileContents.size() > 100) {
        fileContents[fileContents.size() - 50] ^= 0xFF;
    }

    // Write back
    std::ofstream writeFile(filename, std::ios::binary);
    writeFile.write(reinterpret_cast<const char*>(fileContents.data()), fileContents.size());
    writeFile.close();

    // Try to load - should fail with checksum error
    auto result = manager.load(0);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("corrupted") != std::string::npos);
}

TEST_CASE("SaveManager detect truncated file", "[save_manager][corruption]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    REQUIRE(manager.save(0, data).isOk());

    // Truncate the file
    std::string filename = fixture.getTestPath() + "/save_0.nmsav";
    std::filesystem::resize_file(filename, 20); // Keep only header

    // Try to load - should fail
    auto result = manager.load(0);
    REQUIRE(result.isError());
}

TEST_CASE("SaveManager handle empty file", "[save_manager][corruption]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Create an empty file
    std::string filename = fixture.getTestPath() + "/save_5.nmsav";
    std::ofstream file(filename);
    file.close();

    // Try to load - should fail
    auto result = manager.load(5);
    REQUIRE(result.isError());
}

// ============================================================================
// SECTION: Legacy Version (v1) Migration
// ============================================================================

TEST_CASE("SaveManager load legacy v1 format", "[save_manager][migration]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Create a minimal legacy v1 save file manually
    std::string filename = fixture.getTestPath() + "/save_20.nmsav";
    std::ofstream file(filename, std::ios::binary);

    // Write magic and version
    NovelMind::u32 magic = 0x564D4E53; // "SNMV"
    NovelMind::u16 version = 1; // Legacy version
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write sceneId
    std::string sceneId = "legacy_scene";
    NovelMind::u32 sceneLen = static_cast<NovelMind::u32>(sceneId.size());
    file.write(reinterpret_cast<const char*>(&sceneLen), sizeof(sceneLen));
    file.write(sceneId.data(), sceneLen);

    // Write nodeId
    std::string nodeId = "legacy_node";
    NovelMind::u32 nodeLen = static_cast<NovelMind::u32>(nodeId.size());
    file.write(reinterpret_cast<const char*>(&nodeLen), sizeof(nodeLen));
    file.write(nodeId.data(), nodeLen);

    // Write int variables (1 variable)
    NovelMind::u32 intCount = 1;
    file.write(reinterpret_cast<const char*>(&intCount), sizeof(intCount));
    std::string intName = "legacy_int";
    NovelMind::u32 intNameLen = static_cast<NovelMind::u32>(intName.size());
    file.write(reinterpret_cast<const char*>(&intNameLen), sizeof(intNameLen));
    file.write(intName.data(), intNameLen);
    NovelMind::i32 intValue = 777;
    file.write(reinterpret_cast<const char*>(&intValue), sizeof(intValue));

    // Write flags (1 flag)
    NovelMind::u32 flagCount = 1;
    file.write(reinterpret_cast<const char*>(&flagCount), sizeof(flagCount));
    std::string flagName = "legacy_flag";
    NovelMind::u32 flagNameLen = static_cast<NovelMind::u32>(flagName.size());
    file.write(reinterpret_cast<const char*>(&flagNameLen), sizeof(flagNameLen));
    file.write(flagName.data(), flagNameLen);
    NovelMind::u8 flagValue = 1;
    file.write(reinterpret_cast<const char*>(&flagValue), sizeof(flagValue));

    // Write string variables (0)
    NovelMind::u32 strCount = 0;
    file.write(reinterpret_cast<const char*>(&strCount), sizeof(strCount));

    // Write timestamp
    NovelMind::u64 timestamp = 123456789;
    file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));

    // Calculate and write checksum (simple version for testing)
    // For v1, we need to match the checksum calculation
    SaveData tempData;
    tempData.sceneId = sceneId;
    tempData.nodeId = nodeId;
    tempData.intVariables[intName] = intValue;
    tempData.flags[flagName] = true;
    tempData.timestamp = timestamp;

    // Use a simple checksum calculation that matches the implementation
    NovelMind::u32 checksum = 0;
    for (char c : sceneId) checksum = checksum * 31 + static_cast<NovelMind::u32>(c);
    for (char c : nodeId) checksum = checksum * 31 + static_cast<NovelMind::u32>(c);
    for (char c : intName) checksum = checksum * 31 + static_cast<NovelMind::u32>(c);
    checksum = checksum * 31 + static_cast<NovelMind::u32>(intValue);
    for (char c : flagName) checksum = checksum * 31 + static_cast<NovelMind::u32>(c);
    checksum = checksum * 31 + 1;

    file.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    file.close();

    // Try to load the legacy file
    auto result = manager.load(20);
    REQUIRE(result.isOk());

    SaveData loaded = result.value();
    REQUIRE(loaded.sceneId == "legacy_scene");
    REQUIRE(loaded.nodeId == "legacy_node");
    REQUIRE(loaded.intVariables["legacy_int"] == 777);
    REQUIRE(loaded.flags["legacy_flag"] == true);
    REQUIRE(loaded.timestamp == 123456789);
}

TEST_CASE("SaveManager reject unsupported version", "[save_manager][migration][errors]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    // Create a file with unsupported version
    std::string filename = fixture.getTestPath() + "/save_30.nmsav";
    std::ofstream file(filename, std::ios::binary);

    NovelMind::u32 magic = 0x564D4E53;
    NovelMind::u16 version = 99; // Unsupported version
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.close();

    auto result = manager.load(30);
    REQUIRE(result.isError());
    REQUIRE(result.error().find("Unsupported save file version") != std::string::npos);
}

// ============================================================================
// SECTION: Slot Management and Queries
// ============================================================================

TEST_CASE("SaveManager overwrite existing slot", "[save_manager][slot_management]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data1 = createTestSaveData();
    data1.sceneId = "first_save";

    SaveData data2 = createTestSaveData();
    data2.sceneId = "second_save";

    // Save first
    REQUIRE(manager.save(15, data1).isOk());
    auto loaded1 = manager.load(15);
    REQUIRE(loaded1.isOk());
    REQUIRE(loaded1.value().sceneId == "first_save");

    // Overwrite
    REQUIRE(manager.save(15, data2).isOk());
    auto loaded2 = manager.load(15);
    REQUIRE(loaded2.isOk());
    REQUIRE(loaded2.value().sceneId == "second_save");
}

TEST_CASE("SaveManager max slots boundary", "[save_manager][slot_management]") {
    SaveManager manager;

    REQUIRE(manager.getMaxSlots() == 100);

    // Slot 0 is valid
    REQUIRE_FALSE(manager.slotExists(-1));

    // Slot 99 is valid (last slot)
    SaveManagerTestFixture fixture;
    manager.setSavePath(fixture.getTestPath());
    SaveData data = createTestSaveData();
    REQUIRE(manager.save(99, data).isOk());
    REQUIRE(manager.slotExists(99));

    // Slot 100 is invalid
    auto result = manager.save(100, data);
    REQUIRE(result.isError());
}

TEST_CASE("SaveManager large dataset handling", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();

    // Add many variables
    for (int i = 0; i < 1000; i++) {
        data.intVariables["int_" + std::to_string(i)] = i;
        data.floatVariables["float_" + std::to_string(i)] = static_cast<float>(i) * 0.5f;
        data.flags["flag_" + std::to_string(i)] = (i % 2 == 0);
        data.stringVariables["str_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    auto saveResult = manager.save(50, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(50);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.intVariables.size() == 1000);
    REQUIRE(loaded.floatVariables.size() == 1000);
    REQUIRE(loaded.flags.size() == 1000);
    REQUIRE(loaded.stringVariables.size() == 1000);

    // Spot check some values
    REQUIRE(loaded.intVariables["int_500"] == 500);
    REQUIRE(loaded.floatVariables["float_999"] == 999.0f * 0.5f);
    REQUIRE(loaded.flags["flag_100"] == true);
    REQUIRE(loaded.stringVariables["str_0"] == "value_0");
}

TEST_CASE("SaveManager special characters in strings", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    data.sceneId = "scene_with_特殊字符_и_emoji_🎮";
    data.stringVariables["unicode"] = "Hello, 世界! Привет, мир! 🌍";
    data.stringVariables["newlines"] = "Line1\nLine2\nLine3";
    data.stringVariables["tabs"] = "Col1\tCol2\tCol3";

    auto saveResult = manager.save(60, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(60);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.sceneId == "scene_with_特殊字符_и_emoji_🎮");
    REQUIRE(loaded.stringVariables["unicode"] == "Hello, 世界! Привет, мир! 🌍");
    REQUIRE(loaded.stringVariables["newlines"] == "Line1\nLine2\nLine3");
    REQUIRE(loaded.stringVariables["tabs"] == "Col1\tCol2\tCol3");
}

TEST_CASE("SaveManager edge case - zero values", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    data.intVariables["zero_int"] = 0;
    data.floatVariables["zero_float"] = 0.0f;
    data.flags["false_flag"] = false;
    data.stringVariables["empty_string"] = "";

    auto saveResult = manager.save(70, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(70);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.intVariables["zero_int"] == 0);
    REQUIRE(loaded.floatVariables["zero_float"] == 0.0f);
    REQUIRE(loaded.flags["false_flag"] == false);
    REQUIRE(loaded.stringVariables["empty_string"] == "");
}

TEST_CASE("SaveManager negative int values", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    data.intVariables["negative"] = -12345;
    data.intVariables["min_int"] = std::numeric_limits<NovelMind::i32>::min();
    data.intVariables["max_int"] = std::numeric_limits<NovelMind::i32>::max();

    auto saveResult = manager.save(80, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(80);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.intVariables["negative"] == -12345);
    REQUIRE(loaded.intVariables["min_int"] == std::numeric_limits<NovelMind::i32>::min());
    REQUIRE(loaded.intVariables["max_int"] == std::numeric_limits<NovelMind::i32>::max());
}

TEST_CASE("SaveManager float special values", "[save_manager][save_load]") {
    SaveManagerTestFixture fixture;
    SaveManager manager;
    manager.setSavePath(fixture.getTestPath());

    SaveData data = createTestSaveData();
    data.floatVariables["negative"] = -3.14159f;
    data.floatVariables["very_small"] = 0.000001f;
    data.floatVariables["very_large"] = 999999.9f;

    auto saveResult = manager.save(85, data);
    REQUIRE(saveResult.isOk());

    auto loadResult = manager.load(85);
    REQUIRE(loadResult.isOk());

    SaveData loaded = loadResult.value();
    REQUIRE(loaded.floatVariables["negative"] == -3.14159f);
    REQUIRE(loaded.floatVariables["very_small"] == 0.000001f);
    REQUIRE(loaded.floatVariables["very_large"] == 999999.9f);
}
