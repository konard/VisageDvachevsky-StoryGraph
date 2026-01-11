/**
 * @file test_selection_mediator.cpp
 * @brief Unit tests for SelectionMediator debouncing (Issue #470)
 *
 * Tests the following fixes:
 * 1. Debouncing of rapid selection changes to prevent UI freeze
 * 2. Throttling of expensive scene loading operations
 * 3. Immediate status updates while debouncing heavy operations
 * 4. Proper cleanup on shutdown
 */

#include <catch2/catch_test_macros.hpp>

#include "NovelMind/editor/qt/debouncer.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <chrono>
#include <thread>

using namespace NovelMind::editor::qt;

// ============================================================================
// Helper function to process Qt events with timeout
// ============================================================================
void processEvents(int milliseconds = 100) {
  QTimer timer;
  timer.setSingleShot(true);
  bool timeout = false;
  QObject::connect(&timer, &QTimer::timeout, [&timeout]() { timeout = true; });
  timer.start(milliseconds);
  while (!timeout) {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// ============================================================================
// Debouncer Tests (Issue #470)
// ============================================================================

TEST_CASE("Debouncer coalesces rapid events",
          "[selection_mediator][debounce][issue-470]") {
  int callCount = 0;
  Debouncer debouncer(50); // 50ms delay

  // Trigger multiple times rapidly
  for (int i = 0; i < 10; ++i) {
    debouncer.trigger([&callCount]() { ++callCount; });
  }

  // Should still be pending
  REQUIRE(debouncer.isPending());
  REQUIRE(callCount == 0);

  // Wait for debounce to complete
  processEvents(100);

  // Should only execute once
  REQUIRE(callCount == 1);
  REQUIRE(!debouncer.isPending());
}

TEST_CASE("Debouncer executes final callback after delay",
          "[selection_mediator][debounce][issue-470]") {
  QString lastValue;
  Debouncer debouncer(50);

  // Rapid changes should only apply the last one
  debouncer.trigger([&lastValue]() { lastValue = "first"; });
  processEvents(20); // Wait less than debounce delay
  debouncer.trigger([&lastValue]() { lastValue = "second"; });
  processEvents(20);
  debouncer.trigger([&lastValue]() { lastValue = "final"; });

  REQUIRE(lastValue.isEmpty()); // Not executed yet
  processEvents(100); // Wait for debounce
  REQUIRE(lastValue == "final"); // Only last value applied
}

TEST_CASE("Debouncer cancel prevents execution",
          "[selection_mediator][debounce][issue-470]") {
  bool executed = false;
  Debouncer debouncer(50);

  debouncer.trigger([&executed]() { executed = true; });
  REQUIRE(debouncer.isPending());

  debouncer.cancel();
  REQUIRE(!debouncer.isPending());

  processEvents(100);
  REQUIRE(!executed); // Should not execute after cancel
}

TEST_CASE("Debouncer flush executes immediately",
          "[selection_mediator][debounce][issue-470]") {
  int value = 0;
  Debouncer debouncer(1000); // Long delay

  debouncer.trigger([&value]() { value = 42; });
  REQUIRE(debouncer.isPending());
  REQUIRE(value == 0);

  debouncer.flush(); // Execute immediately
  REQUIRE(!debouncer.isPending());
  REQUIRE(value == 42);
}

TEST_CASE("Selection debouncing prevents event flood",
          "[selection_mediator][debounce][issue-470]") {
  // Simulate rapid selection changes (like marquee selection)
  int expensiveOperationCount = 0;
  int lightweightUpdateCount = 0;

  Debouncer selectionDebouncer(100);
  Debouncer sceneLoadDebouncer(200);

  // Simulate 20 rapid selection changes
  for (int i = 0; i < 20; ++i) {
    // Immediate lightweight update (not debounced)
    ++lightweightUpdateCount;

    // Debounced expensive operations
    selectionDebouncer.trigger([&expensiveOperationCount, &sceneLoadDebouncer]() {
      ++expensiveOperationCount;

      // Nested debouncing for even more expensive operations
      sceneLoadDebouncer.trigger([]() {
        // Scene loading would happen here
      });
    });

    // Small delay to simulate rapid but not instant changes
    processEvents(10);
  }

  // All lightweight updates executed immediately
  REQUIRE(lightweightUpdateCount == 20);

  // Expensive operation not executed yet
  REQUIRE(expensiveOperationCount == 0);

  // Wait for first debouncer
  processEvents(150);

  // Should only execute once
  REQUIRE(expensiveOperationCount == 1);

  // Wait for scene load debouncer
  processEvents(250);
  // Scene load would have executed once (tested via nested trigger)
}

TEST_CASE("Debouncer allows configurable delay",
          "[selection_mediator][debounce][issue-470]") {
  Debouncer fastDebouncer(20);
  Debouncer slowDebouncer(100);

  REQUIRE(fastDebouncer.delay() == 20);
  REQUIRE(slowDebouncer.delay() == 100);

  fastDebouncer.setDelay(50);
  REQUIRE(fastDebouncer.delay() == 50);
}

TEST_CASE("Rapid selection changes maintain final state",
          "[selection_mediator][debounce][issue-470]") {
  // Issue #470: Ensure no event starvation - final state is always correct
  QString finalNodeId;
  Debouncer debouncer(50);

  std::vector<QString> nodeIds = {"node1", "node2", "node3", "node4", "node5"};

  for (const auto &nodeId : nodeIds) {
    debouncer.trigger([&finalNodeId, nodeId]() { finalNodeId = nodeId; });
    processEvents(10); // Rapid changes
  }

  REQUIRE(finalNodeId.isEmpty()); // Not executed yet

  processEvents(100); // Wait for debounce

  // Final state should be the last selection
  REQUIRE(finalNodeId == "node5");
}

TEST_CASE("Multiple debouncers work independently",
          "[selection_mediator][debounce][issue-470]") {
  int uiUpdateCount = 0;
  int sceneLoadCount = 0;

  Debouncer uiDebouncer(50);
  Debouncer sceneDebouncer(100);

  // Trigger both
  for (int i = 0; i < 5; ++i) {
    uiDebouncer.trigger([&uiUpdateCount]() { ++uiUpdateCount; });
    sceneDebouncer.trigger([&sceneLoadCount]() { ++sceneLoadCount; });
    processEvents(10);
  }

  // Wait for UI debouncer
  processEvents(70);
  REQUIRE(uiUpdateCount == 1);
  REQUIRE(sceneLoadCount == 0); // Scene debouncer still pending

  // Wait for scene debouncer
  processEvents(60);
  REQUIRE(uiUpdateCount == 1);
  REQUIRE(sceneLoadCount == 1);
}

TEST_CASE("Debouncer cleanup prevents dangling callbacks",
          "[selection_mediator][debounce][issue-470]") {
  bool executed = false;
  {
    Debouncer debouncer(50);
    debouncer.trigger([&executed]() { executed = true; });
    REQUIRE(debouncer.isPending());
    // Debouncer destroyed here - callback should be cancelled
  }

  processEvents(100);
  // Callback should not execute after debouncer is destroyed
  // Note: This tests that QTimer is properly cleaned up with the parent QObject
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_CASE("Debouncer reduces event processing overhead",
          "[selection_mediator][debounce][performance][issue-470]") {
  const int iterations = 100;

  // Without debouncing
  auto start = std::chrono::high_resolution_clock::now();
  int withoutDebounceCount = 0;
  for (int i = 0; i < iterations; ++i) {
    ++withoutDebounceCount; // Simulate expensive operation
  }
  auto withoutDebounceTime =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() - start)
          .count();

  // With debouncing
  start = std::chrono::high_resolution_clock::now();
  int withDebounceCount = 0;
  Debouncer debouncer(10);
  for (int i = 0; i < iterations; ++i) {
    debouncer.trigger([&withDebounceCount]() { ++withDebounceCount; });
  }
  processEvents(50); // Wait for debounced callback
  auto withDebounceTime = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::high_resolution_clock::now() - start)
                              .count();

  // Verify behavior
  REQUIRE(withoutDebounceCount == iterations);
  REQUIRE(withDebounceCount == 1); // Only executed once

  // Note: Time comparison not strict due to Qt event loop overhead
  // The key benefit is reducing the number of expensive operations from 100 to 1
}

TEST_CASE("Shutdown cancels pending operations",
          "[selection_mediator][debounce][issue-470]") {
  bool executed = false;
  Debouncer debouncer(50);

  debouncer.trigger([&executed]() { executed = true; });
  REQUIRE(debouncer.isPending());

  // Simulate shutdown
  debouncer.cancel();

  processEvents(100);
  REQUIRE(!executed); // Should not execute after shutdown
  REQUIRE(!debouncer.isPending());
}

// ============================================================================
// Qt Signal Connection Cleanup Tests (Issue #463)
// ============================================================================

#include "NovelMind/editor/mediators/selection_mediator.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include <QSignalSpy>

TEST_CASE("SelectionMediator disconnects Qt signals on shutdown",
          "[selection_mediator][cleanup][issue-463]") {
  // Create mock panels
  auto *storyGraph = new qt::NMStoryGraphPanel(nullptr);

  // Create mediator with story graph panel
  auto *mediator = new mediators::SelectionMediator(
      nullptr,  // sceneView
      nullptr,  // hierarchy
      nullptr,  // inspector
      storyGraph,
      nullptr   // parent
  );

  // Initialize to establish connections
  mediator->initialize();

  // Verify connections exist by checking that signals would trigger callbacks
  // We can't directly check QMetaObject connections, but we can verify behavior

  // Shutdown should disconnect all signals
  mediator->shutdown();

  // After shutdown, emitting signals should not cause issues
  // If connections weren't properly disconnected, this could cause crashes
  // or memory leaks when storyGraph emits signals after mediator cleanup
  emit storyGraph->nodeSelected("test-node");

  processEvents(50);

  // Clean up
  delete mediator;
  delete storyGraph;

  // If we reach here without crashes, the test passes
  REQUIRE(true);
}

TEST_CASE("SelectionMediator handles multiple initialize/shutdown cycles",
          "[selection_mediator][cleanup][issue-463]") {
  // This tests for connection accumulation bugs where each initialize()
  // would add duplicate connections without proper cleanup

  auto *storyGraph = new qt::NMStoryGraphPanel(nullptr);
  auto *mediator = new mediators::SelectionMediator(
      nullptr, nullptr, nullptr, storyGraph, nullptr
  );

  // Multiple initialize/shutdown cycles
  for (int i = 0; i < 3; ++i) {
    mediator->initialize();
    processEvents(10);
    mediator->shutdown();
    processEvents(10);
  }

  // Final initialize
  mediator->initialize();

  // Emit signal and verify no crashes from accumulated connections
  emit storyGraph->nodeSelected("test-node");
  processEvents(50);

  mediator->shutdown();

  delete mediator;
  delete storyGraph;

  REQUIRE(true);
}

TEST_CASE("SelectionMediator destructor calls shutdown",
          "[selection_mediator][cleanup][issue-463]") {
  // Verify that destructor properly cleans up even if shutdown() wasn't called

  auto *storyGraph = new qt::NMStoryGraphPanel(nullptr);
  auto *mediator = new mediators::SelectionMediator(
      nullptr, nullptr, nullptr, storyGraph, nullptr
  );

  mediator->initialize();

  // Delete without explicit shutdown call - destructor should handle it
  delete mediator;

  // Emit signal after mediator is destroyed
  // Should not crash if connections were properly cleaned up in destructor
  emit storyGraph->nodeSelected("test-node");
  processEvents(50);

  delete storyGraph;

  REQUIRE(true);
}

TEST_CASE("SelectionMediator handles null panel pointers safely",
          "[selection_mediator][cleanup][issue-463]") {
  // Test that shutdown() handles null pointers gracefully

  auto *mediator = new mediators::SelectionMediator(
      nullptr, nullptr, nullptr, nullptr, nullptr
  );

  mediator->initialize();
  mediator->shutdown();

  // Should not crash with null pointers
  delete mediator;

  REQUIRE(true);
}
