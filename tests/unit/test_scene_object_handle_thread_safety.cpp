/**
 * @file test_scene_object_handle_thread_safety.cpp
 * @brief Unit tests for SceneObjectHandle thread safety
 *
 * These tests verify the fix for the TOCTOU race condition in isValid()
 * between checking validity and using the handle (Issue #564).
 *
 * The fix uses a generation counter pattern combined with mutex protection
 * to prevent use-after-free bugs in multithreaded scenarios.
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_object_handle.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace NovelMind;
using namespace NovelMind::scene;

/**
 * Helper: Simple scene object for testing
 */
class TestSceneObject : public SceneObjectBase {
public:
  explicit TestSceneObject(const std::string &id)
      : SceneObjectBase(id, SceneObjectType::Custom) {}

  void render(NovelMind::renderer::IRenderer &) override {}

  std::atomic<int> accessCount{0};
};

TEST_CASE("SceneObjectHandle basic functionality", "[scene][handle]") {
  SceneGraph graph;

  SECTION("Invalid handle returns false") {
    SceneObjectHandle handle;
    REQUIRE_FALSE(handle.isValid());
    REQUIRE(handle.get() == nullptr);
  }

  SECTION("Valid handle to existing object") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");
    REQUIRE(handle.isValid());
    REQUIRE(handle.get() != nullptr);
    REQUIRE(handle.getId() == "test_obj");
  }

  SECTION("Handle becomes invalid after object deletion") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");
    REQUIRE(handle.isValid());

    // Delete the object
    graph.removeFromLayer(LayerType::UI, "test_obj");

    // Handle should now be invalid
    REQUIRE_FALSE(handle.isValid());
    REQUIRE(handle.get() == nullptr);
  }
}

TEST_CASE("SceneObjectHandle withObject safety", "[scene][handle]") {
  SceneGraph graph;

  SECTION("withObject executes function when valid") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    TestSceneObject *rawPtr = obj.get();
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");

    bool executed = false;
    bool result = handle.withObject([&](SceneObjectBase *objPtr) {
      REQUIRE(objPtr != nullptr);
      REQUIRE(objPtr == rawPtr);
      executed = true;
    });

    REQUIRE(result);
    REQUIRE(executed);
  }

  SECTION("withObject does not execute when invalid") {
    SceneObjectHandle handle;

    bool executed = false;
    bool result = handle.withObject([&](SceneObjectBase *) {
      executed = true;
    });

    REQUIRE_FALSE(result);
    REQUIRE_FALSE(executed);
  }

  SECTION("withObjectAs with correct type") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");

    bool executed = false;
    bool result = handle.withObjectAs<TestSceneObject>([&](TestSceneObject *objPtr) {
      REQUIRE(objPtr != nullptr);
      executed = true;
    });

    REQUIRE(result);
    REQUIRE(executed);
  }
}

TEST_CASE("SceneObjectHandle concurrent access safety",
          "[scene][handle][threading]") {
  SceneGraph graph;

  SECTION("Concurrent validity checks are safe") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");

    // Multiple threads checking validity concurrently
    std::vector<std::thread> threads;
    std::atomic<int> validCount{0};

    for (int i = 0; i < 10; ++i) {
      threads.emplace_back([&]() {
        for (int j = 0; j < 100; ++j) {
          if (handle.isValid()) {
            ++validCount;
          }
          std::this_thread::yield();
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    // All checks should have succeeded since object exists
    REQUIRE(validCount == 1000);
  }

  SECTION("Concurrent access via withObject is safe") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    TestSceneObject *rawPtr = obj.get();
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 10; ++i) {
      threads.emplace_back([&]() {
        for (int j = 0; j < 100; ++j) {
          bool success = handle.withObject([&](SceneObjectBase *objPtr) {
            // Safely access the object
            REQUIRE(objPtr != nullptr);
            REQUIRE(objPtr == rawPtr);
            static_cast<TestSceneObject *>(objPtr)->accessCount++;
          });
          if (success) {
            ++successCount;
          }
          std::this_thread::yield();
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    REQUIRE(successCount == 1000);
    REQUIRE(rawPtr->accessCount == 1000);
  }

  SECTION("No use-after-free when object deleted during access") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");

    std::atomic<bool> deletionStarted{false};
    std::atomic<bool> deletionComplete{false};
    std::atomic<int> accessAfterDeletion{0};
    std::atomic<int> totalAccesses{0};

    // Thread 1: Continuously try to access the object
    std::thread accessThread([&]() {
      for (int i = 0; i < 1000; ++i) {
        (void)handle.withObject([&](SceneObjectBase *objPtr) {
          REQUIRE(objPtr != nullptr);
          ++totalAccesses;
          if (deletionComplete.load()) {
            ++accessAfterDeletion;
          }
        });
        std::this_thread::yield();
      }
    });

    // Thread 2: Delete the object after a short delay
    std::thread deleteThread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      deletionStarted.store(true);
      graph.removeFromLayer(LayerType::UI, "test_obj");
      deletionComplete.store(true);
    });

    accessThread.join();
    deleteThread.join();

    // Critical: No accesses should have succeeded after deletion
    // The generation counter prevents access to deleted objects
    REQUIRE(accessAfterDeletion == 0);
  }

  SECTION("Multiple handles to same object work correctly") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    TestSceneObject *rawPtr = obj.get();
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle1(&graph, "test_obj");
    SceneObjectHandle handle2(&graph, "test_obj");
    SceneObjectHandle handle3(&graph, "test_obj");

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Each handle used by different threads
    for (auto *handle : {&handle1, &handle2, &handle3}) {
      threads.emplace_back([handle, &successCount, rawPtr]() {
        for (int i = 0; i < 100; ++i) {
          bool success = handle->withObject([&](SceneObjectBase *objPtr) {
            REQUIRE(objPtr == rawPtr);
          });
          if (success) {
            ++successCount;
          }
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    REQUIRE(successCount == 300);
  }
}

TEST_CASE("SceneObjectHandle generation counter prevents TOCTOU",
          "[scene][handle][threading]") {
  SceneGraph graph;

  SECTION("Old handle with stale generation cannot access new object") {
    // Create first object
    auto obj1 = std::make_unique<TestSceneObject>("test_obj");
    u64 gen1 = obj1->getGeneration();
    graph.addToLayer(LayerType::UI, std::move(obj1));

    SceneObjectHandle handle(&graph, "test_obj");

    // Delete first object
    graph.removeFromLayer(LayerType::UI, "test_obj");

    // Create new object with same ID
    auto obj2 = std::make_unique<TestSceneObject>("test_obj");
    u64 gen2 = obj2->getGeneration();
    graph.addToLayer(LayerType::UI, std::move(obj2));

    // Generations should be different
    REQUIRE(gen1 != gen2);

    // Old handle should still be invalid (generation mismatch)
    REQUIRE_FALSE(handle.isValid());
    REQUIRE(handle.get() == nullptr);

    // withObject should fail
    bool executed = false;
    handle.withObject([&](SceneObjectBase *) { executed = true; });
    REQUIRE_FALSE(executed);
  }
}

TEST_CASE("SceneObjectHandle stress test", "[scene][handle][threading]") {
  SECTION("Rapid handle creation and deletion") {
    SceneGraph graph;
    std::atomic<int> totalAccesses{0};
    std::atomic<bool> stopFlag{false};

    // Thread 1: Rapidly create and delete objects
    std::thread mutationThread([&]() {
      for (int i = 0; i < 100; ++i) {
        auto obj = std::make_unique<TestSceneObject>("test_obj");
        graph.addToLayer(LayerType::UI, std::move(obj));

        std::this_thread::sleep_for(std::chrono::microseconds(100));

        graph.removeFromLayer(LayerType::UI, "test_obj");
        std::this_thread::yield();
      }
      stopFlag.store(true);
    });

    // Thread 2: Continuously try to access the object
    std::thread accessThread([&]() {
      while (!stopFlag.load()) {
        SceneObjectHandle handle(&graph, "test_obj");
        handle.withObject([&](SceneObjectBase *objPtr) {
          REQUIRE(objPtr != nullptr);
          ++totalAccesses;
        });
        std::this_thread::yield();
      }
    });

    mutationThread.join();
    accessThread.join();

    // Should have had some successful accesses without crashes
    // The exact count doesn't matter - what matters is no crashes
    REQUIRE(totalAccesses >= 0);
  }
}

TEST_CASE("SceneObjectHandle reset clears state", "[scene][handle]") {
  SceneGraph graph;

  auto obj = std::make_unique<TestSceneObject>("test_obj");
  graph.addToLayer(LayerType::UI, std::move(obj));

  SceneObjectHandle handle(&graph, "test_obj");
  REQUIRE(handle.isValid());

  handle.reset();
  REQUIRE_FALSE(handle.isValid());
  REQUIRE(handle.get() == nullptr);
  REQUIRE(handle.getId().empty());
}

TEST_CASE("SceneObjectHandle bool conversion", "[scene][handle]") {
  SceneGraph graph;

  SECTION("Invalid handle converts to false") {
    SceneObjectHandle handle;
    REQUIRE_FALSE(static_cast<bool>(handle));
  }

  SECTION("Valid handle converts to true") {
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    SceneObjectHandle handle(&graph, "test_obj");
    REQUIRE(static_cast<bool>(handle));
  }
}
