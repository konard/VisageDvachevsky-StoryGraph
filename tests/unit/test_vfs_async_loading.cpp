#include <catch2/catch_test_macros.hpp>

#include "NovelMind/vfs/pack_reader.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace NovelMind;
using namespace NovelMind::vfs;

// ============================================================================
// VFS Async Pack Loading Tests (Issue #547 - P2)
// ============================================================================

TEST_CASE("PackReader - Async mount API exists", "[vfs][pack][async]") {
  PackReader reader;

  SECTION("mountAsync returns a future") {
    // This test verifies the API exists and returns the correct type
    // We don't actually mount a pack here since we don't have test data
    // Just verify the function signature compiles
    auto future = reader.mountAsync("nonexistent.pack");
    CHECK(future.valid());
  }
}

TEST_CASE("PackReader - Progress callback functionality", "[vfs][pack][async]") {
  PackReader reader;

  SECTION("Progress callback is invoked during async mount") {
    std::atomic<int> progressCallCount{0};
    std::atomic<usize> lastStep{0};
    std::atomic<usize> totalSteps{0};

    auto progressCallback = [&](usize current, usize total, const std::string& description) {
      progressCallCount++;
      lastStep = current;
      totalSteps = total;
    };

    // Attempt to mount a non-existent pack (will fail, but should still call progress)
    auto future = reader.mountAsync("nonexistent.pack", progressCallback);

    // Wait for completion
    auto result = future.get();

    // Progress should have been called at least once (for the initial step)
    CHECK(progressCallCount > 0);
  }
}

TEST_CASE("PackReader - Async mount does not block", "[vfs][pack][async]") {
  SECTION("Async mount returns immediately") {
    PackReader reader;

    auto startTime = std::chrono::steady_clock::now();

    // Launch async mount
    auto future = reader.mountAsync("nonexistent.pack");

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Should return almost immediately (< 100ms) even though actual loading may take longer
    CHECK(duration.count() < 100);

    // Clean up future
    future.wait();
  }
}

TEST_CASE("PackReader - Async mount error handling", "[vfs][pack][async]") {
  PackReader reader;

  SECTION("Non-existent file returns error") {
    auto future = reader.mountAsync("nonexistent_pack_file.nmpack");
    auto result = future.get();

    CHECK(result.isError());
    CHECK_FALSE(result.error().empty());
  }

  SECTION("Progress callback receives error context") {
    std::string lastDescription;

    auto progressCallback = [&](usize current, usize total, const std::string& description) {
      lastDescription = description;
    };

    auto future = reader.mountAsync("nonexistent.pack", progressCallback);
    auto result = future.get();

    CHECK(result.isError());
    // Progress callback should have been called at least once
    CHECK_FALSE(lastDescription.empty());
  }
}

TEST_CASE("ProgressCallback - Type definition", "[vfs][pack][async]") {
  SECTION("ProgressCallback is a valid function type") {
    // Test that we can create a ProgressCallback
    ProgressCallback callback = [](usize current, usize total, const std::string& desc) {
      // No-op callback
    };

    // Test that we can call it
    callback(1, 4, "Test step");

    // Test nullptr callback is valid
    ProgressCallback nullCallback = nullptr;
    CHECK(nullCallback == nullptr);
  }
}

TEST_CASE("PackReader - Multiple async mounts", "[vfs][pack][async]") {
  SECTION("Can launch multiple async mount operations") {
    PackReader reader1;
    PackReader reader2;

    auto future1 = reader1.mountAsync("pack1.nmpack");
    auto future2 = reader2.mountAsync("pack2.nmpack");

    // Both futures should be valid
    CHECK(future1.valid());
    CHECK(future2.valid());

    // Wait for both to complete
    auto result1 = future1.get();
    auto result2 = future2.get();

    // Both should fail (non-existent files), but shouldn't crash
    CHECK(result1.isError());
    CHECK(result2.isError());
  }
}

TEST_CASE("PackReader - Synchronous mount still works", "[vfs][pack][async]") {
  PackReader reader;

  SECTION("Original mount() function is not broken") {
    // Verify synchronous API still exists and works
    auto result = reader.mount("nonexistent.pack");

    CHECK(result.isError());
    CHECK_FALSE(result.error().empty());
  }
}
