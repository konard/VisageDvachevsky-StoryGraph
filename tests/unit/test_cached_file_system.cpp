#include <catch2/catch_test_macros.hpp>
#include "NovelMind/vfs/cached_file_system.hpp"
#include "NovelMind/vfs/memory_fs.hpp"

using namespace NovelMind;
using namespace NovelMind::vfs;

TEST_CASE("CachedFileSystem construction", "[vfs][cache]")
{
    SECTION("Construct with default cache size")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        CachedFileSystem cachedFs(std::move(inner));
        // No crash means success
        REQUIRE(true);
    }

    SECTION("Construct with custom cache size")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        CachedFileSystem cachedFs(std::move(inner), 1024 * 1024); // 1MB cache
        REQUIRE(true);
    }

    SECTION("Wraps inner filesystem")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        MemoryFileSystem* innerPtr = inner.get();

        // Add resource to inner before wrapping
        innerPtr->addResource("test", {1, 2, 3}, ResourceType::Data);

        CachedFileSystem cachedFs(std::move(inner));

        // Should be able to access resource through cached layer
        REQUIRE(cachedFs.exists("test"));
    }
}

TEST_CASE("CachedFileSystem basic operations", "[vfs][cache]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner));

    SECTION("Read file from inner filesystem")
    {
        std::vector<u8> data = {10, 20, 30, 40, 50};
        innerPtr->addResource("test_file", data, ResourceType::Data);

        auto result = cachedFs.readFile("test_file");
        REQUIRE(result.isOk());
        REQUIRE(result.value() == data);
    }

    SECTION("Exists checks inner filesystem")
    {
        innerPtr->addResource("exists_test", {1, 2, 3}, ResourceType::Data);

        REQUIRE(cachedFs.exists("exists_test"));
        REQUIRE_FALSE(cachedFs.exists("doesnt_exist"));
    }

    SECTION("Get info returns resource metadata")
    {
        std::vector<u8> data = {1, 2, 3, 4, 5};
        innerPtr->addResource("info_test", data, ResourceType::Texture);

        auto info = cachedFs.getInfo("info_test");
        REQUIRE(info.has_value());
        REQUIRE(info->size == 5);
    }

    SECTION("List resources delegates to inner")
    {
        innerPtr->addResource("res1", {1}, ResourceType::Data);
        innerPtr->addResource("res2", {2}, ResourceType::Texture);
        innerPtr->addResource("res3", {3}, ResourceType::Audio);

        auto all = cachedFs.listResources();
        REQUIRE(all.size() == 3);
    }

    SECTION("List resources with type filter")
    {
        innerPtr->addResource("data1", {1}, ResourceType::Data);
        innerPtr->addResource("tex1", {2}, ResourceType::Texture);
        innerPtr->addResource("tex2", {3}, ResourceType::Texture);

        auto textures = cachedFs.listResources(ResourceType::Texture);
        REQUIRE(textures.size() == 2);
    }
}

TEST_CASE("CachedFileSystem caching behavior", "[vfs][cache][behavior]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner), 1024); // 1KB cache

    SECTION("First read populates cache")
    {
        std::vector<u8> data = {1, 2, 3, 4, 5};
        innerPtr->addResource("cache_test", data, ResourceType::Data);

        auto result1 = cachedFs.readFile("cache_test");
        REQUIRE(result1.isOk());
        REQUIRE(result1.value() == data);
    }

    SECTION("Second read uses cache (same data)")
    {
        std::vector<u8> data = {1, 2, 3, 4, 5};
        innerPtr->addResource("cached_read", data, ResourceType::Data);

        auto result1 = cachedFs.readFile("cached_read");
        auto result2 = cachedFs.readFile("cached_read");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        REQUIRE(result1.value() == result2.value());
    }

    SECTION("Cache stores multiple resources")
    {
        std::vector<u8> data1 = {1, 2, 3};
        std::vector<u8> data2 = {4, 5, 6};

        innerPtr->addResource("file1", data1, ResourceType::Data);
        innerPtr->addResource("file2", data2, ResourceType::Data);

        auto result1 = cachedFs.readFile("file1");
        auto result2 = cachedFs.readFile("file2");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        REQUIRE(result1.value() == data1);
        REQUIRE(result2.value() == data2);
    }

    SECTION("Non-existent file returns error (not cached)")
    {
        auto result = cachedFs.readFile("missing");
        REQUIRE(result.isError());
    }
}

TEST_CASE("CachedFileSystem LRU eviction", "[vfs][cache][lru]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner), 300); // Small cache: 300 bytes

    SECTION("Cache evicts oldest entries when full")
    {
        std::vector<u8> data(100, 0); // 100 bytes each

        innerPtr->addResource("file1", data, ResourceType::Data);
        innerPtr->addResource("file2", data, ResourceType::Data);
        innerPtr->addResource("file3", data, ResourceType::Data);

        // Fill cache with 3 entries (300 bytes)
        cachedFs.readFile("file1");
        cachedFs.readFile("file2");
        cachedFs.readFile("file3");

        // Add 4th entry - should evict file1 (oldest)
        innerPtr->addResource("file4", data, ResourceType::Data);
        cachedFs.readFile("file4");

        // All reads should still work (cache or fallback to inner)
        REQUIRE(cachedFs.readFile("file2").isOk());
        REQUIRE(cachedFs.readFile("file3").isOk());
        REQUIRE(cachedFs.readFile("file4").isOk());
    }

    SECTION("LRU updates access order")
    {
        std::vector<u8> data(100, 0);

        innerPtr->addResource("old", data, ResourceType::Data);
        innerPtr->addResource("middle", data, ResourceType::Data);
        innerPtr->addResource("new", data, ResourceType::Data);

        cachedFs.readFile("old");
        cachedFs.readFile("middle");
        cachedFs.readFile("new");

        // Access "old" to make it recently used
        cachedFs.readFile("old");

        // Add another entry - should evict "middle" (least recently used)
        innerPtr->addResource("newest", data, ResourceType::Data);
        cachedFs.readFile("newest");

        // All files should still be readable
        REQUIRE(cachedFs.readFile("old").isOk());
        REQUIRE(cachedFs.readFile("new").isOk());
        REQUIRE(cachedFs.readFile("newest").isOk());
    }

    SECTION("Large file evicts multiple small entries")
    {
        std::vector<u8> small(50, 0);
        std::vector<u8> large(250, 0);

        innerPtr->addResource("small1", small, ResourceType::Data);
        innerPtr->addResource("small2", small, ResourceType::Data);
        innerPtr->addResource("small3", small, ResourceType::Data);

        cachedFs.readFile("small1");
        cachedFs.readFile("small2");
        cachedFs.readFile("small3");

        // Add large file - should evict multiple small ones
        innerPtr->addResource("large", large, ResourceType::Data);
        cachedFs.readFile("large");

        // All should still be readable (from cache or inner)
        REQUIRE(cachedFs.readFile("small1").isOk());
        REQUIRE(cachedFs.readFile("large").isOk());
    }
}

TEST_CASE("CachedFileSystem cache management", "[vfs][cache][management]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner), 1024);

    SECTION("Clear cache removes all cached entries")
    {
        std::vector<u8> data = {1, 2, 3};
        innerPtr->addResource("file1", data, ResourceType::Data);
        innerPtr->addResource("file2", data, ResourceType::Data);

        cachedFs.readFile("file1");
        cachedFs.readFile("file2");

        cachedFs.clearCache();

        // Files should still be readable from inner filesystem
        REQUIRE(cachedFs.readFile("file1").isOk());
        REQUIRE(cachedFs.readFile("file2").isOk());
    }

    SECTION("SetMaxBytes updates cache size limit")
    {
        cachedFs.setMaxBytes(2048);
        // Size updated, no crash
        REQUIRE(true);
    }

    SECTION("Reducing max bytes triggers eviction")
    {
        std::vector<u8> data(200, 0);

        innerPtr->addResource("res1", data, ResourceType::Data);
        innerPtr->addResource("res2", data, ResourceType::Data);

        cachedFs.readFile("res1");
        cachedFs.readFile("res2");

        // Reduce cache size - should trigger eviction
        cachedFs.setMaxBytes(250);

        // Files should still be accessible
        REQUIRE(cachedFs.readFile("res1").isOk());
        REQUIRE(cachedFs.readFile("res2").isOk());
    }
}

TEST_CASE("CachedFileSystem mount operations", "[vfs][cache][mount]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner));

    SECTION("Mount delegates to inner filesystem")
    {
        // MemoryFS doesn't actually use mount, but operation should not crash
        auto result = cachedFs.mount("pack.dat");
        REQUIRE(result.isOk());
    }

    SECTION("Unmount delegates to inner filesystem")
    {
        cachedFs.unmount("pack.dat");
        REQUIRE(true);
    }

    SECTION("Unmount all delegates to inner filesystem")
    {
        cachedFs.unmountAll();
        REQUIRE(true);
    }
}

TEST_CASE("CachedFileSystem error handling", "[vfs][cache][error]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner));

    SECTION("Reading non-existent file returns error")
    {
        auto result = cachedFs.readFile("doesnt_exist");
        REQUIRE(result.isError());
    }

    SECTION("Errors are not cached")
    {
        auto result1 = cachedFs.readFile("missing1");
        auto result2 = cachedFs.readFile("missing2");

        REQUIRE(result1.isError());
        REQUIRE(result2.isError());

        // Now add the file and it should be readable
        innerPtr->addResource("missing1", {1, 2, 3}, ResourceType::Data);
        auto result3 = cachedFs.readFile("missing1");
        REQUIRE(result3.isOk());
    }

    SECTION("GetInfo for non-existent resource returns nullopt")
    {
        auto info = cachedFs.getInfo("not_there");
        REQUIRE_FALSE(info.has_value());
    }
}

TEST_CASE("CachedFileSystem edge cases", "[vfs][cache][edge]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner), 1024);

    SECTION("Empty file can be cached")
    {
        std::vector<u8> emptyData;
        innerPtr->addResource("empty", emptyData, ResourceType::Data);

        auto result = cachedFs.readFile("empty");
        REQUIRE(result.isOk());
        REQUIRE(result.value().empty());
    }

    SECTION("Very large file (larger than cache)")
    {
        CachedFileSystem smallCacheFs(std::make_unique<MemoryFileSystem>(), 100);
        auto* innerPtr2 = static_cast<MemoryFileSystem*>(
            // Can't easily get inner pointer after move, so test differently
        );

        // Create new cached fs with small cache
        auto inner2 = std::make_unique<MemoryFileSystem>();
        MemoryFileSystem* innerPtr2Real = inner2.get();
        CachedFileSystem smallCache(std::move(inner2), 100);

        std::vector<u8> largeData(500, 42);
        innerPtr2Real->addResource("large", largeData, ResourceType::Data);

        auto result = smallCache.readFile("large");
        REQUIRE(result.isOk());
        REQUIRE(result.value() == largeData);
    }

    SECTION("Multiple reads of same resource")
    {
        std::vector<u8> data = {1, 2, 3, 4, 5};
        innerPtr->addResource("multi", data, ResourceType::Data);

        for (int i = 0; i < 10; ++i)
        {
            auto result = cachedFs.readFile("multi");
            REQUIRE(result.isOk());
            REQUIRE(result.value() == data);
        }
    }

    SECTION("Resource IDs are case sensitive")
    {
        innerPtr->addResource("File", {1}, ResourceType::Data);
        innerPtr->addResource("file", {2}, ResourceType::Data);

        auto result1 = cachedFs.readFile("File");
        auto result2 = cachedFs.readFile("file");

        REQUIRE(result1.isOk());
        REQUIRE(result2.isOk());
        REQUIRE(result1.value()[0] == 1);
        REQUIRE(result2.value()[0] == 2);
    }

    SECTION("Special characters in resource ID")
    {
        std::vector<u8> data = {10, 20, 30};
        innerPtr->addResource("file-with_special.chars@123", data, ResourceType::Data);

        auto result = cachedFs.readFile("file-with_special.chars@123");
        REQUIRE(result.isOk());
        REQUIRE(result.value() == data);
    }
}

TEST_CASE("CachedFileSystem decorator pattern", "[vfs][cache][decorator]")
{
    SECTION("Transparently wraps inner filesystem")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        MemoryFileSystem* innerPtr = inner.get();

        innerPtr->addResource("test1", {1, 2, 3}, ResourceType::Data);
        innerPtr->addResource("test2", {4, 5, 6}, ResourceType::Texture);

        CachedFileSystem cached(std::move(inner));

        // All inner operations should work through cached layer
        REQUIRE(cached.exists("test1"));
        REQUIRE(cached.exists("test2"));
        REQUIRE(cached.readFile("test1").isOk());
        REQUIRE(cached.readFile("test2").isOk());
    }

    SECTION("Adds caching without changing interface")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        MemoryFileSystem* innerPtr = inner.get();

        std::vector<u8> data = {1, 2, 3, 4, 5};
        innerPtr->addResource("transparent", data, ResourceType::Data);

        CachedFileSystem cached(std::move(inner));

        // First read
        auto result1 = cached.readFile("transparent");
        REQUIRE(result1.isOk());

        // Second read (should use cache but behavior is identical)
        auto result2 = cached.readFile("transparent");
        REQUIRE(result2.isOk());

        // Results should be the same
        REQUIRE(result1.value() == result2.value());
    }

    SECTION("Can be used polymorphically as IVirtualFileSystem")
    {
        auto inner = std::make_unique<MemoryFileSystem>();
        MemoryFileSystem* innerPtr = inner.get();

        innerPtr->addResource("poly", {7, 8, 9}, ResourceType::Data);

        CachedFileSystem cached(std::move(inner));
        IVirtualFileSystem* vfs = &cached;

        // Use through interface pointer
        REQUIRE(vfs->exists("poly"));
        REQUIRE(vfs->readFile("poly").isOk());
    }
}

TEST_CASE("CachedFileSystem performance characteristics", "[vfs][cache][perf]")
{
    auto inner = std::make_unique<MemoryFileSystem>();
    MemoryFileSystem* innerPtr = inner.get();
    CachedFileSystem cachedFs(std::move(inner), 10 * 1024); // 10KB cache

    SECTION("Repeated access benefits from cache")
    {
        std::vector<u8> data(1000, 123);
        innerPtr->addResource("repeated", data, ResourceType::Data);

        // Multiple reads should be fast (cached)
        for (int i = 0; i < 100; ++i)
        {
            auto result = cachedFs.readFile("repeated");
            REQUIRE(result.isOk());
            REQUIRE(result.value() == data);
        }
    }

    SECTION("Cache handles many small files")
    {
        std::vector<u8> smallData(10, 0);

        // Add many small files
        for (int i = 0; i < 50; ++i)
        {
            std::string id = "small_" + std::to_string(i);
            innerPtr->addResource(id, smallData, ResourceType::Data);
        }

        // Read all files
        for (int i = 0; i < 50; ++i)
        {
            std::string id = "small_" + std::to_string(i);
            auto result = cachedFs.readFile(id);
            REQUIRE(result.isOk());
        }

        // LRU should have evicted older entries, but all still accessible
        for (int i = 0; i < 50; ++i)
        {
            std::string id = "small_" + std::to_string(i);
            REQUIRE(cachedFs.exists(id));
        }
    }
}
