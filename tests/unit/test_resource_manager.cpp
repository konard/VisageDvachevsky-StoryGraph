#include <catch2/catch_test_macros.hpp>
#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/vfs/memory_fs.hpp"

#include <fstream>
#include <filesystem>

using namespace NovelMind;
using namespace NovelMind::resource;
using namespace NovelMind::vfs;

namespace fs = std::filesystem;

namespace {

// Helper to create minimal valid PNG data (1x1 pixel red)
std::vector<u8> createMinimalPNG() {
    return {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, // IHDR chunk
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // 1x1 pixels
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
        0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, // IDAT chunk
        0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
        0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D,
        0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, // IEND chunk
        0x44, 0xAE, 0x42, 0x60, 0x82
    };
}

// Helper to create test binary data
std::vector<u8> createTestData(size_t size, u8 fillValue = 0) {
    return std::vector<u8>(size, fillValue);
}

// Helper to write test file
void writeTestFile(const std::string& path, const std::vector<u8>& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace

TEST_CASE("ResourceManager construction", "[resource][manager]")
{
    SECTION("Can construct without VFS")
    {
        ResourceManager manager;
        REQUIRE(manager.getTextureCount() == 0);
        REQUIRE(manager.getFontCount() == 0);
        REQUIRE(manager.getFontAtlasCount() == 0);
    }

    SECTION("Can construct with VFS")
    {
        MemoryFileSystem fs;
        ResourceManager manager(&fs);
        REQUIRE(manager.getTextureCount() == 0);
    }

    SECTION("Can set VFS after construction")
    {
        ResourceManager manager;
        MemoryFileSystem fs;
        manager.setVfs(&fs);
        // No crash means success
        REQUIRE(true);
    }
}

TEST_CASE("ResourceManager base path", "[resource][manager][path]")
{
    ResourceManager manager;

    SECTION("Can set base path")
    {
        manager.setBasePath("/assets");
        // Base path is set internally
        REQUIRE(true);
    }

    SECTION("Base path with trailing slash")
    {
        manager.setBasePath("/assets/");
        REQUIRE(true);
    }

    SECTION("Empty base path is valid")
    {
        manager.setBasePath("");
        REQUIRE(true);
    }
}

TEST_CASE("ResourceManager texture loading", "[resource][manager][texture]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Load valid texture from VFS")
    {
        auto pngData = createMinimalPNG();
        fs.addResource("test_texture.png", pngData, ResourceType::Texture);

        auto result = manager.loadTexture("test_texture.png");
        REQUIRE(result.isOk());
        REQUIRE(result.value() != nullptr);
        REQUIRE(result.value()->isValid());
        REQUIRE(manager.getTextureCount() == 1);
    }

    SECTION("Cached texture is returned on second load")
    {
        auto pngData = createMinimalPNG();
        fs.addResource("cached_texture.png", pngData, ResourceType::Texture);

        auto result1 = manager.loadTexture("cached_texture.png");
        auto result2 = manager.loadTexture("cached_texture.png");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        // Same shared_ptr should be returned (reference counted)
        REQUIRE(result1.value() == result2.value());
        REQUIRE(manager.getTextureCount() == 1);
    }

    SECTION("Empty texture ID returns error")
    {
        auto result = manager.loadTexture("");
        REQUIRE(result.isError());
        REQUIRE(result.error() == "Texture id is empty");
    }

    SECTION("Non-existent texture returns error")
    {
        auto result = manager.loadTexture("missing.png");
        REQUIRE(result.isError());
    }

    SECTION("Invalid image data returns error")
    {
        std::vector<u8> invalidData = {1, 2, 3, 4, 5}; // Not a PNG
        fs.addResource("invalid.png", invalidData, ResourceType::Texture);

        auto result = manager.loadTexture("invalid.png");
        REQUIRE(result.isError());
    }

    SECTION("Unload texture removes from cache")
    {
        auto pngData = createMinimalPNG();
        fs.addResource("unload_test.png", pngData, ResourceType::Texture);

        auto result = manager.loadTexture("unload_test.png");
        REQUIRE(result.isOk());
        REQUIRE(manager.getTextureCount() == 1);

        manager.unloadTexture("unload_test.png");
        REQUIRE(manager.getTextureCount() == 0);
    }

    SECTION("Unload non-existent texture is safe")
    {
        manager.unloadTexture("doesnt_exist.png");
        REQUIRE(manager.getTextureCount() == 0);
    }

    SECTION("Multiple different textures can be loaded")
    {
        auto png1 = createMinimalPNG();
        auto png2 = createMinimalPNG();

        fs.addResource("texture1.png", png1, ResourceType::Texture);
        fs.addResource("texture2.png", png2, ResourceType::Texture);

        auto result1 = manager.loadTexture("texture1.png");
        auto result2 = manager.loadTexture("texture2.png");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        REQUIRE(manager.getTextureCount() == 2);
    }
}

TEST_CASE("ResourceManager font loading", "[resource][manager][font]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    // Note: We can't easily test actual font loading without valid TTF data
    // But we can test the manager's behavior around font loading

    SECTION("Empty font ID returns error")
    {
        auto result = manager.loadFont("", 16);
        REQUIRE(result.isError());
        REQUIRE(result.error() == "Font id is empty");
    }

    SECTION("Invalid font size returns error")
    {
        auto result1 = manager.loadFont("font.ttf", 0);
        REQUIRE(result1.isError());
        REQUIRE(result1.error() == "Font size must be positive");

        auto result2 = manager.loadFont("font.ttf", -1);
        REQUIRE(result2.isError());
    }

    SECTION("Non-existent font returns error")
    {
        auto result = manager.loadFont("missing.ttf", 16);
        REQUIRE(result.isError());
    }

    SECTION("Font count tracks loaded fonts")
    {
        REQUIRE(manager.getFontCount() == 0);
    }

    SECTION("Unload font with specific size")
    {
        // Even if font doesn't load, unload should be safe
        manager.unloadFont("font.ttf", 16);
        REQUIRE(manager.getFontCount() == 0);
    }

    SECTION("Unload non-existent font is safe")
    {
        manager.unloadFont("doesnt_exist.ttf", 24);
        REQUIRE(true);
    }
}

TEST_CASE("ResourceManager font atlas loading", "[resource][manager][fontatlas]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Empty charset returns error")
    {
        auto result = manager.loadFontAtlas("font.ttf", 16, "");
        REQUIRE(result.isError());
        REQUIRE(result.error() == "Font atlas charset is empty");
    }

    SECTION("Font atlas requires valid font")
    {
        // Without valid font data, atlas loading should fail
        auto result = manager.loadFontAtlas("missing.ttf", 16, "ABC");
        REQUIRE(result.isError());
    }

    SECTION("Font atlas count is tracked")
    {
        REQUIRE(manager.getFontAtlasCount() == 0);
    }
}

TEST_CASE("ResourceManager data reading", "[resource][manager][data]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Read valid data from VFS")
    {
        std::vector<u8> testData = {10, 20, 30, 40, 50};
        fs.addResource("data.bin", testData, ResourceType::Data);

        auto result = manager.readData("data.bin");
        REQUIRE(result.isOk());
        REQUIRE(result.value() == testData);
    }

    SECTION("Read non-existent data returns error")
    {
        auto result = manager.readData("missing.bin");
        REQUIRE(result.isError());
    }

    SECTION("Read empty data is valid")
    {
        std::vector<u8> emptyData;
        fs.addResource("empty.bin", emptyData, ResourceType::Data);

        auto result = manager.readData("empty.bin");
        REQUIRE(result.isOk());
        REQUIRE(result.value().empty());
    }

    SECTION("Read large data block")
    {
        std::vector<u8> largeData(10000, 123);
        fs.addResource("large.bin", largeData, ResourceType::Data);

        auto result = manager.readData("large.bin");
        REQUIRE(result.isOk());
        REQUIRE(result.value().size() == 10000);
        REQUIRE(result.value()[0] == 123);
    }
}

TEST_CASE("ResourceManager cache management", "[resource][manager][cache]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Clear cache removes all cached resources")
    {
        auto pngData = createMinimalPNG();
        fs.addResource("texture1.png", pngData, ResourceType::Texture);
        fs.addResource("texture2.png", pngData, ResourceType::Texture);

        manager.loadTexture("texture1.png");
        manager.loadTexture("texture2.png");

        REQUIRE(manager.getTextureCount() == 2);

        manager.clearCache();

        REQUIRE(manager.getTextureCount() == 0);
        REQUIRE(manager.getFontCount() == 0);
        REQUIRE(manager.getFontAtlasCount() == 0);
    }

    SECTION("Clear empty cache is safe")
    {
        manager.clearCache();
        REQUIRE(manager.getTextureCount() == 0);
    }

    SECTION("Cache persists between operations")
    {
        auto pngData = createMinimalPNG();
        fs.addResource("persistent.png", pngData, ResourceType::Texture);

        auto result1 = manager.loadTexture("persistent.png");
        REQUIRE(manager.getTextureCount() == 1);

        // Perform other operations
        manager.readData("some_data");

        // Cache should still be intact
        REQUIRE(manager.getTextureCount() == 1);
    }
}

TEST_CASE("ResourceManager resource counting", "[resource][manager][count]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Texture count is accurate")
    {
        auto png = createMinimalPNG();
        fs.addResource("t1.png", png, ResourceType::Texture);
        fs.addResource("t2.png", png, ResourceType::Texture);
        fs.addResource("t3.png", png, ResourceType::Texture);

        REQUIRE(manager.getTextureCount() == 0);

        manager.loadTexture("t1.png");
        REQUIRE(manager.getTextureCount() == 1);

        manager.loadTexture("t2.png");
        REQUIRE(manager.getTextureCount() == 2);

        manager.loadTexture("t3.png");
        REQUIRE(manager.getTextureCount() == 3);
    }

    SECTION("Count decreases after unload")
    {
        auto png = createMinimalPNG();
        fs.addResource("texture.png", png, ResourceType::Texture);

        manager.loadTexture("texture.png");
        REQUIRE(manager.getTextureCount() == 1);

        manager.unloadTexture("texture.png");
        REQUIRE(manager.getTextureCount() == 0);
    }
}

TEST_CASE("ResourceManager with filesystem", "[resource][manager][fs]")
{
    // Create a temporary directory for testing
    const std::string tempDir = "test_resource_temp";
    fs::create_directories(tempDir);

    SECTION("Load texture from filesystem")
    {
        ResourceManager manager;
        manager.setBasePath(tempDir);

        const std::string texturePath = tempDir + "/test.png";
        writeTestFile(texturePath, createMinimalPNG());

        auto result = manager.loadTexture("test.png");
        REQUIRE(result.isOk());

        fs::remove(texturePath);
    }

    SECTION("Absolute path works")
    {
        ResourceManager manager;

        const std::string absolutePath = fs::absolute(tempDir + "/absolute.png").string();
        writeTestFile(absolutePath, createMinimalPNG());

        auto result = manager.loadTexture(absolutePath);
        REQUIRE(result.isOk());

        fs::remove(absolutePath);
    }

    SECTION("Relative path with base path")
    {
        ResourceManager manager;
        manager.setBasePath(tempDir);

        const std::string texturePath = tempDir + "/relative.png";
        writeTestFile(texturePath, createMinimalPNG());

        auto result = manager.loadTexture("relative.png");
        REQUIRE(result.isOk());

        fs::remove(texturePath);
    }

    // Cleanup
    fs::remove_all(tempDir);
}

TEST_CASE("ResourceManager error handling", "[resource][manager][error]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Multiple errors are properly reported")
    {
        auto result1 = manager.loadTexture("");
        auto result2 = manager.loadFont("", 16);

        REQUIRE(result1.isError());
        REQUIRE(result2.isError());
        REQUIRE_FALSE(result1.error().empty());
        REQUIRE_FALSE(result2.error().empty());
    }

    SECTION("Error doesn't affect cache state")
    {
        auto png = createMinimalPNG();
        fs.addResource("valid.png", png, ResourceType::Texture);

        manager.loadTexture("valid.png");
        REQUIRE(manager.getTextureCount() == 1);

        // Try to load invalid texture
        manager.loadTexture("invalid.png");

        // Cache should still have the valid texture
        REQUIRE(manager.getTextureCount() == 1);
    }
}

TEST_CASE("ResourceManager VFS fallback", "[resource][manager][vfs]")
{
    SECTION("VFS is used when file not found")
    {
        ResourceManager manager;
        MemoryFileSystem fs;

        auto png = createMinimalPNG();
        fs.addResource("vfs_texture.png", png, ResourceType::Texture);

        manager.setVfs(&fs);

        auto result = manager.loadTexture("vfs_texture.png");
        REQUIRE(result.isOk());
    }

    SECTION("Manager works without VFS for filesystem")
    {
        ResourceManager manager;
        // No VFS set

        const std::string tempDir = "test_no_vfs";
        fs::create_directories(tempDir);
        manager.setBasePath(tempDir);

        const std::string texturePath = tempDir + "/fs_only.png";
        writeTestFile(texturePath, createMinimalPNG());

        auto result = manager.loadTexture("fs_only.png");
        REQUIRE(result.isOk());

        fs::remove_all(tempDir);
    }
}

TEST_CASE("ResourceManager reference counting", "[resource][manager][refcount]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Shared pointer reference counting works")
    {
        auto png = createMinimalPNG();
        fs.addResource("shared.png", png, ResourceType::Texture);

        auto result1 = manager.loadTexture("shared.png");
        REQUIRE(result1.isOk());

        TextureHandle handle1 = result1.value();
        REQUIRE(handle1.use_count() >= 2); // Manager + handle1

        {
            TextureHandle handle2 = result1.value();
            REQUIRE(handle1.use_count() >= 3); // Manager + handle1 + handle2
        }

        // handle2 out of scope
        REQUIRE(handle1.use_count() >= 2); // Manager + handle1
    }

    SECTION("Unload releases manager's reference")
    {
        auto png = createMinimalPNG();
        fs.addResource("ref_test.png", png, ResourceType::Texture);

        auto result = manager.loadTexture("ref_test.png");
        TextureHandle handle = result.value();

        usize countBefore = handle.use_count();

        manager.unloadTexture("ref_test.png");

        // Reference count should decrease
        REQUIRE(handle.use_count() < countBefore);
    }

    SECTION("Clear cache releases all references")
    {
        auto png = createMinimalPNG();
        fs.addResource("clear_ref.png", png, ResourceType::Texture);

        auto result = manager.loadTexture("clear_ref.png");
        TextureHandle handle = result.value();

        usize countBefore = handle.use_count();

        manager.clearCache();

        // Reference count should decrease
        REQUIRE(handle.use_count() < countBefore);
    }
}

TEST_CASE("ResourceManager edge cases", "[resource][manager][edge]")
{
    MemoryFileSystem fs;
    ResourceManager manager(&fs);

    SECTION("Loading same resource multiple times reuses cache")
    {
        auto png = createMinimalPNG();
        fs.addResource("multi_load.png", png, ResourceType::Texture);

        for (int i = 0; i < 10; ++i)
        {
            auto result = manager.loadTexture("multi_load.png");
            REQUIRE(result.isOk());
        }

        // Should only have one cached entry
        REQUIRE(manager.getTextureCount() == 1);
    }

    SECTION("Resource IDs are case sensitive")
    {
        auto png = createMinimalPNG();
        fs.addResource("Texture.png", png, ResourceType::Texture);
        fs.addResource("texture.png", png, ResourceType::Texture);

        auto result1 = manager.loadTexture("Texture.png");
        auto result2 = manager.loadTexture("texture.png");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        // Different IDs, different cache entries
        REQUIRE(manager.getTextureCount() == 2);
    }

    SECTION("Special characters in resource ID")
    {
        auto png = createMinimalPNG();
        fs.addResource("texture-with_special.chars@123.png", png, ResourceType::Texture);

        auto result = manager.loadTexture("texture-with_special.chars@123.png");
        REQUIRE(result.isOk());
    }
}
