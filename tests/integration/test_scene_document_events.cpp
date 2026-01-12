/**
 * @file test_scene_document_events.cpp
 * @brief Integration tests for scene document event debouncing
 *
 * Tests the implementation for Issue #521:
 * - SceneDocumentModifiedEvent debouncing
 * - Dirty flag optimization
 * - Event batching during rapid changes
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include "NovelMind/editor/qt/debouncer.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <atomic>

using namespace NovelMind::editor;
using namespace NovelMind::editor::qt;

// Helper to ensure QCoreApplication exists for Qt tests
class QtTestFixture {
public:
  QtTestFixture() {
    if (!QCoreApplication::instance()) {
      static int argc = 1;
      static char* argv[] = {const_cast<char*>("test"), nullptr};
      m_app = std::make_unique<QCoreApplication>(argc, argv);
    }
  }

private:
  std::unique_ptr<QCoreApplication> m_app;
};

// =============================================================================
// Scene Document Event Debouncing Tests (Issue #521)
// =============================================================================

TEST_CASE("SceneDocumentModifiedEvent: Debouncer batches rapid saves",
          "[integration][editor][scene][debounce][issue-521]") {
  QtTestFixture fixture;

  SECTION("Rapid changes trigger only one save after debounce delay") {
    Debouncer saveDebouncer(100); // 100ms delay
    std::atomic<int> saveCount{0};
    std::atomic<bool> documentDirty{false};

    // Simulate rapid property changes (10 changes in quick succession)
    for (int i = 0; i < 10; ++i) {
      documentDirty = true;
      saveDebouncer.trigger([&saveCount, &documentDirty]() {
        if (documentDirty) {
          ++saveCount;
          documentDirty = false;
        }
      });
    }

    // At this point, save should not have been called yet
    REQUIRE(saveCount == 0);

    // Wait for debounce delay + margin
    QEventLoop loop;
    QTimer::singleShot(150, &loop, &QEventLoop::quit);
    loop.exec();

    // After debounce delay, should have saved exactly once
    REQUIRE(saveCount == 1);
    REQUIRE(documentDirty == false);
  }

  SECTION("Dirty flag prevents redundant event publishing") {
    std::atomic<bool> documentDirty{false};
    std::atomic<int> eventPublishCount{0};

    auto simulateSave = [&documentDirty, &eventPublishCount]() {
      if (documentDirty) {
        ++eventPublishCount;
        documentDirty = false;
      }
    };

    // First change sets dirty and triggers save
    documentDirty = true;
    simulateSave();
    REQUIRE(eventPublishCount == 1);
    REQUIRE(documentDirty == false);

    // Second save without changes should not publish event
    simulateSave();
    REQUIRE(eventPublishCount == 1); // Still 1, not incremented

    // New change sets dirty again
    documentDirty = true;
    simulateSave();
    REQUIRE(eventPublishCount == 2);
  }

  SECTION("Flush executes pending save immediately") {
    Debouncer saveDebouncer(1000); // Long delay
    std::atomic<int> saveCount{0};
    std::atomic<bool> documentDirty{false};

    // Trigger a save with long delay
    documentDirty = true;
    saveDebouncer.trigger([&saveCount, &documentDirty]() {
      if (documentDirty) {
        ++saveCount;
        documentDirty = false;
      }
    });

    REQUIRE(saveCount == 0); // Not saved yet

    // Flush should execute immediately
    saveDebouncer.flush();

    REQUIRE(saveCount == 1); // Saved immediately
    REQUIRE(documentDirty == false);
  }
}

TEST_CASE("SceneDocumentModifiedEvent: Event bus integration",
          "[integration][editor][scene][events][issue-521]") {
  QtTestFixture fixture;

  SECTION("Events are published through EventBus") {
    auto& bus = EventBus::instance();
    std::atomic<int> eventCount{0};

    // Subscribe to SceneDocumentModifiedEvent
    auto subscription = bus.subscribe<events::SceneDocumentModifiedEvent>(
        [&eventCount](const events::SceneDocumentModifiedEvent&) { ++eventCount; });

    // Publish an event
    events::SceneDocumentModifiedEvent event;
    event.sceneId = "test_scene";
    bus.publish(event);

    // Event should be received
    REQUIRE(eventCount == 1);

    // Publish another event
    event.sceneId = "test_scene_2";
    bus.publish(event);

    REQUIRE(eventCount == 2);
  }

  SECTION("Debouncer reduces event spam to EventBus") {
    auto& bus = EventBus::instance();
    std::atomic<int> eventCount{0};
    Debouncer saveDebouncer(50);

    // Subscribe to events
    auto subscription = bus.subscribe<events::SceneDocumentModifiedEvent>(
        [&eventCount](const events::SceneDocumentModifiedEvent&) { ++eventCount; });

    // Simulate 20 rapid changes
    for (int i = 0; i < 20; ++i) {
      saveDebouncer.trigger([&bus]() {
        events::SceneDocumentModifiedEvent event;
        event.sceneId = "test_scene";
        bus.publish(event);
      });
    }

    // Events should not have been published yet
    REQUIRE(eventCount == 0);

    // Wait for debounce
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    // Should have published only once
    REQUIRE(eventCount == 1);
  }
}

TEST_CASE("SceneDocumentModifiedEvent: Flush on scene transitions",
          "[integration][editor][scene][transitions][issue-521]") {
  QtTestFixture fixture;

  SECTION("Pending saves are flushed when switching scenes") {
    Debouncer saveDebouncer(1000); // Long delay
    std::atomic<int> saveCount{0};
    std::atomic<bool> documentDirty{false};
    QString currentSceneId = "scene_1";

    // Make changes to scene_1
    documentDirty = true;
    saveDebouncer.trigger([&saveCount, &documentDirty]() {
      if (documentDirty) {
        ++saveCount;
        documentDirty = false;
      }
    });

    REQUIRE(saveCount == 0); // Not saved yet

    // Switch to scene_2 - should flush pending save
    QString newSceneId = "scene_2";
    if (!currentSceneId.isEmpty() && currentSceneId != newSceneId) {
      saveDebouncer.flush(); // Flush pending save
    }
    currentSceneId = newSceneId;

    // Save should have been executed
    REQUIRE(saveCount == 1);
    REQUIRE(documentDirty == false);
  }
}

TEST_CASE("SceneDocumentModifiedEvent: Performance characteristics",
          "[integration][editor][scene][performance][issue-521]") {
  QtTestFixture fixture;

  SECTION("Debouncer handles high-frequency updates") {
    Debouncer saveDebouncer(50);
    std::atomic<int> saveCount{0};

    // Simulate 100 rapid updates (e.g., dragging object with mouse)
    for (int i = 0; i < 100; ++i) {
      saveDebouncer.trigger([&saveCount]() { ++saveCount; });
    }

    // Wait for debounce
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    // Should have coalesced all 100 updates into 1 save
    REQUIRE(saveCount == 1);
  }

  SECTION("Multiple consecutive batches are handled correctly") {
    Debouncer saveDebouncer(30);
    std::atomic<int> saveCount{0};

    // First batch of changes
    for (int i = 0; i < 10; ++i) {
      saveDebouncer.trigger([&saveCount]() { ++saveCount; });
    }

    // Wait for first batch to complete
    QEventLoop loop1;
    QTimer::singleShot(50, &loop1, &QEventLoop::quit);
    loop1.exec();
    REQUIRE(saveCount == 1);

    // Second batch of changes
    for (int i = 0; i < 10; ++i) {
      saveDebouncer.trigger([&saveCount]() { ++saveCount; });
    }

    // Wait for second batch to complete
    QEventLoop loop2;
    QTimer::singleShot(50, &loop2, &QEventLoop::quit);
    loop2.exec();
    REQUIRE(saveCount == 2);
  }
}
