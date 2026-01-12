#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/vfs/resource_cache.hpp"

#include <thread>
#include <vector>

using namespace NovelMind;
using namespace NovelMind::VFS;

TEST_CASE("ResourceCache basic operations", "[resource][cache]")
{
    ResourceCache cache(1024); // 1KB cache for testing

    SECTION("Empty cache returns no data")
    {
        ResourceId id("test_resource");
        auto result = cache.get(id);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(cache.contains(id));
    }

    SECTION("Can store and retrieve data")
    {
        ResourceId id("test_data");
        std::vector<u8> data = {1, 2, 3, 4, 5};

        cache.put(id, data);

        REQUIRE(cache.contains(id));
        auto result = cache.get(id);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
    }

    SECTION("Contains returns true for cached resources")
    {
        ResourceId id("cached");
        std::vector<u8> data = {10, 20, 30};

        cache.put(id, data);
        REQUIRE(cache.contains(id));
    }

    SECTION("Contains returns false for non-cached resources")
    {
        ResourceId id("not_cached");
        REQUIRE_FALSE(cache.contains(id));
    }

    SECTION("Remove deletes cached data")
    {
        ResourceId id("to_remove");
        std::vector<u8> data = {1, 2, 3};

        cache.put(id, data);
        REQUIRE(cache.contains(id));

        cache.remove(id);
        REQUIRE_FALSE(cache.contains(id));
        REQUIRE_FALSE(cache.get(id).has_value());
    }

    SECTION("Clear removes all cached data")
    {
        ResourceId id1("res1");
        ResourceId id2("res2");
        std::vector<u8> data = {1, 2, 3};

        cache.put(id1, data);
        cache.put(id2, data);
        REQUIRE(cache.entryCount() == 2);

        cache.clear();
        REQUIRE(cache.entryCount() == 0);
        REQUIRE_FALSE(cache.contains(id1));
        REQUIRE_FALSE(cache.contains(id2));
    }

    SECTION("Current size tracks memory usage")
    {
        ResourceId id("size_test");
        std::vector<u8> data(100, 42);

        cache.put(id, data);
        REQUIRE(cache.currentSize() == 100);
    }

    SECTION("Entry count tracks number of cached items")
    {
        cache.put(ResourceId("res1"), {1, 2, 3});
        cache.put(ResourceId("res2"), {4, 5, 6});
        cache.put(ResourceId("res3"), {7, 8, 9});

        REQUIRE(cache.entryCount() == 3);
    }
}

TEST_CASE("ResourceCache statistics tracking", "[resource][cache][stats]")
{
    ResourceCache cache(1024);

    SECTION("Statistics are initially zero")
    {
        auto stats = cache.stats();
        REQUIRE(stats.hitCount == 0);
        REQUIRE(stats.missCount == 0);
        REQUIRE(stats.evictionCount == 0);
        REQUIRE(stats.entryCount == 0);
        REQUIRE(stats.totalSize == 0);
        REQUIRE(stats.hitRate() == Catch::Approx(0.0));
    }

    SECTION("Cache hit increments hit count")
    {
        ResourceId id("hit_test");
        std::vector<u8> data = {1, 2, 3};

        cache.put(id, data);
        cache.get(id); // Hit
        cache.get(id); // Hit

        auto stats = cache.stats();
        REQUIRE(stats.hitCount == 2);
    }

    SECTION("Cache miss increments miss count")
    {
        ResourceId missing("missing");

        cache.get(missing); // Miss
        cache.get(missing); // Miss

        auto stats = cache.stats();
        REQUIRE(stats.missCount == 2);
    }

    SECTION("Hit rate calculation is correct")
    {
        ResourceId cached("cached");
        ResourceId missing("missing");
        std::vector<u8> data = {1, 2, 3};

        cache.put(cached, data);

        cache.get(cached);  // Hit
        cache.get(cached);  // Hit
        cache.get(missing); // Miss

        auto stats = cache.stats();
        // 2 hits, 1 miss = 2/3 = 0.666...
        REQUIRE(stats.hitRate() == Catch::Approx(2.0 / 3.0));
    }

    SECTION("Statistics reflect current state")
    {
        std::vector<u8> data(100, 0);

        cache.put(ResourceId("r1"), data);
        cache.put(ResourceId("r2"), data);

        auto stats = cache.stats();
        REQUIRE(stats.entryCount == 2);
        REQUIRE(stats.totalSize == 200);
    }

    SECTION("Reset statistics clears counters but not cache")
    {
        ResourceId id("test");
        std::vector<u8> data = {1, 2, 3};

        cache.put(id, data);
        cache.get(id); // Hit

        cache.resetStats();

        auto stats = cache.stats();
        REQUIRE(stats.hitCount == 0);
        REQUIRE(stats.missCount == 0);
        REQUIRE(stats.evictionCount == 0);
        // Cache entry still exists
        REQUIRE(cache.contains(id));
    }
}

TEST_CASE("ResourceCache LRU eviction", "[resource][cache][lru]")
{
    // Small cache that can hold ~3 entries of 100 bytes each
    ResourceCache cache(300);

    SECTION("Eviction occurs when cache is full")
    {
        std::vector<u8> data(100, 0);

        // Fill cache with 3 entries
        cache.put(ResourceId("res1"), data);
        cache.put(ResourceId("res2"), data);
        cache.put(ResourceId("res3"), data);

        REQUIRE(cache.entryCount() == 3);

        // Add 4th entry - should evict oldest (res1)
        cache.put(ResourceId("res4"), data);

        REQUIRE(cache.entryCount() <= 3); // May be less if evicted more
        REQUIRE_FALSE(cache.contains(ResourceId("res1"))); // Oldest evicted
    }

    SECTION("LRU evicts least recently used")
    {
        std::vector<u8> data(100, 0);

        cache.put(ResourceId("old"), data);
        cache.put(ResourceId("middle"), data);
        cache.put(ResourceId("new"), data);

        // Access "old" to make it recently used
        cache.get(ResourceId("old"));

        // Add another entry - should evict "middle" (least recently used)
        cache.put(ResourceId("newest"), data);

        REQUIRE(cache.contains(ResourceId("old"))); // Was accessed, should stay
        REQUIRE_FALSE(cache.contains(ResourceId("middle"))); // LRU, evicted
    }

    SECTION("Eviction count is tracked")
    {
        std::vector<u8> data(100, 0);

        cache.put(ResourceId("r1"), data);
        cache.put(ResourceId("r2"), data);
        cache.put(ResourceId("r3"), data);
        cache.put(ResourceId("r4"), data); // Triggers eviction

        auto stats = cache.stats();
        REQUIRE(stats.evictionCount > 0);
    }

    SECTION("Large entry evicts multiple small entries")
    {
        std::vector<u8> small(50, 0);
        std::vector<u8> large(250, 0);

        cache.put(ResourceId("s1"), small);
        cache.put(ResourceId("s2"), small);
        cache.put(ResourceId("s3"), small);

        REQUIRE(cache.entryCount() == 3);

        // Add large entry - should evict multiple small ones
        cache.put(ResourceId("large"), large);

        auto stats = cache.stats();
        REQUIRE(stats.evictionCount >= 2); // At least 2 evictions
    }
}

TEST_CASE("ResourceCache max size configuration", "[resource][cache][config]")
{
    SECTION("Default max size is 64MB")
    {
        ResourceCache cache;
        REQUIRE(cache.maxSize() == 64 * 1024 * 1024);
    }

    SECTION("Can set custom max size")
    {
        ResourceCache cache(2048);
        REQUIRE(cache.maxSize() == 2048);
    }

    SECTION("SetMaxSize updates max size")
    {
        ResourceCache cache(1024);
        REQUIRE(cache.maxSize() == 1024);

        cache.setMaxSize(4096);
        REQUIRE(cache.maxSize() == 4096);
    }

    SECTION("Reducing max size triggers eviction")
    {
        ResourceCache cache(1024);
        std::vector<u8> data(200, 0);

        cache.put(ResourceId("r1"), data);
        cache.put(ResourceId("r2"), data);
        cache.put(ResourceId("r3"), data);

        usize initialCount = cache.entryCount();

        // Reduce max size to 250 bytes - should trigger evictions
        cache.setMaxSize(250);

        REQUIRE(cache.entryCount() < initialCount);
        REQUIRE(cache.currentSize() <= 250);
    }
}

TEST_CASE("ResourceCache thread safety", "[resource][cache][thread]")
{
    ResourceCache cache(10 * 1024); // 10KB cache
    const int numThreads = 4;
    const int operationsPerThread = 100;

    SECTION("Concurrent puts and gets are safe")
    {
        std::vector<std::thread> threads;

        for (int t = 0; t < numThreads; ++t)
        {
            threads.emplace_back([&cache, t]() {
                for (int i = 0; i < operationsPerThread; ++i)
                {
                    std::string idStr = "thread_" + std::to_string(t) + "_item_" + std::to_string(i);
                    ResourceId id(idStr);
                    std::vector<u8> data(50, static_cast<u8>(t));

                    cache.put(id, data);
                    auto result = cache.get(id);

                    if (result.has_value())
                    {
                        REQUIRE(result.value() == data);
                    }
                }
            });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        // No crashes means thread safety works
        REQUIRE(true);
    }

    SECTION("Concurrent access to same resource is safe")
    {
        ResourceId sharedId("shared_resource");
        std::vector<u8> data = {1, 2, 3, 4, 5};
        cache.put(sharedId, data);

        std::vector<std::thread> threads;

        for (int t = 0; t < numThreads; ++t)
        {
            threads.emplace_back([&cache, &sharedId, &data]() {
                for (int i = 0; i < operationsPerThread; ++i)
                {
                    auto result = cache.get(sharedId);
                    if (result.has_value())
                    {
                        REQUIRE(result.value() == data);
                    }
                }
            });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        REQUIRE(cache.contains(sharedId));
    }

    SECTION("Concurrent removes and gets are safe")
    {
        // Pre-populate cache
        for (int i = 0; i < 50; ++i)
        {
            std::string idStr = "item_" + std::to_string(i);
            cache.put(ResourceId(idStr), std::vector<u8>(20, static_cast<u8>(i)));
        }

        std::vector<std::thread> threads;

        // Some threads remove, others try to read
        for (int t = 0; t < numThreads; ++t)
        {
            threads.emplace_back([&cache, t]() {
                for (int i = 0; i < 50; ++i)
                {
                    std::string idStr = "item_" + std::to_string(i);
                    ResourceId id(idStr);

                    if (t % 2 == 0)
                    {
                        cache.remove(id);
                    }
                    else
                    {
                        cache.get(id); // May or may not exist
                    }
                }
            });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        // No crashes means thread safety works
        REQUIRE(true);
    }
}

TEST_CASE("ResourceCache edge cases", "[resource][cache][edge]")
{
    ResourceCache cache(1024);

    SECTION("Empty data can be cached")
    {
        ResourceId id("empty");
        std::vector<u8> emptyData;

        cache.put(id, emptyData);

        REQUIRE(cache.contains(id));
        auto result = cache.get(id);
        REQUIRE(result.has_value());
        REQUIRE(result.value().empty());
    }

    SECTION("Very large single entry fills cache")
    {
        ResourceId id("large");
        std::vector<u8> largeData(1024, 255);

        cache.put(id, largeData);

        REQUIRE(cache.contains(id));
        REQUIRE(cache.currentSize() == 1024);
    }

    SECTION("Entry larger than max size is not cached")
    {
        ResourceCache smallCache(100);
        ResourceId id("too_large");
        std::vector<u8> tooLarge(200, 0);

        smallCache.put(id, tooLarge);

        // Entry too large to fit, should not be cached
        REQUIRE(smallCache.entryCount() == 0);
        REQUIRE_FALSE(smallCache.contains(id));
    }

    SECTION("Updating existing entry replaces data")
    {
        ResourceId id("update_test");
        std::vector<u8> data1 = {1, 2, 3};
        std::vector<u8> data2 = {4, 5, 6, 7, 8};

        cache.put(id, data1);
        REQUIRE(cache.currentSize() == 3);

        cache.put(id, data2);
        REQUIRE(cache.currentSize() == 5);

        auto result = cache.get(id);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data2);
    }

    SECTION("Remove non-existent entry is safe")
    {
        ResourceId id("doesnt_exist");

        // Should not crash
        cache.remove(id);
        REQUIRE(true);
    }

    SECTION("Clear on empty cache is safe")
    {
        ResourceCache emptyCache;

        // Should not crash
        emptyCache.clear();
        REQUIRE(emptyCache.entryCount() == 0);
    }
}

TEST_CASE("ResourceCache access patterns", "[resource][cache][access]")
{
    ResourceCache cache(1024);

    SECTION("Repeated access updates LRU order")
    {
        std::vector<u8> data(100, 0);

        cache.put(ResourceId("first"), data);
        cache.put(ResourceId("second"), data);
        cache.put(ResourceId("third"), data);

        // Access first multiple times
        for (int i = 0; i < 5; ++i)
        {
            cache.get(ResourceId("first"));
        }

        // Add entries to trigger eviction
        cache.put(ResourceId("fourth"), data);
        cache.put(ResourceId("fifth"), data);

        // "first" should still be cached due to frequent access
        REQUIRE(cache.contains(ResourceId("first")));
    }

    SECTION("Get on non-existent resource doesn't affect cache")
    {
        ResourceId id("missing");

        auto statsBefore = cache.stats();
        cache.get(id);
        auto statsAfter = cache.stats();

        REQUIRE(statsAfter.missCount == statsBefore.missCount + 1);
        REQUIRE(statsAfter.entryCount == statsBefore.entryCount);
    }
}
