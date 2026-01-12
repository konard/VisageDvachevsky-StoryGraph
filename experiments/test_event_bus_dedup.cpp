/**
 * @file test_event_bus_dedup.cpp
 * @brief Standalone test for EventBus deduplication feature (Issue #480)
 */

#include "NovelMind/editor/event_bus.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

using namespace NovelMind::editor;

struct TestEvent : EditorEvent {
  int value = 0;
  TestEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "TestEvent: " + std::to_string(value);
  }
};

int main() {
  EventBus bus;
  std::atomic<int> eventCount{0};

  // Subscribe to events
  auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
    eventCount++;
  });

  std::cout << "Testing EventBus deduplication (Issue #480)\n";
  std::cout << "===========================================\n\n";

  // Test 1: Deduplication disabled (default)
  std::cout << "Test 1: Deduplication disabled (default)\n";
  eventCount = 0;
  for (int i = 0; i < 10; i++) {
    TestEvent event;
    bus.publish(event);
  }
  std::cout << "  Published 10 identical events, received: " << eventCount << " events\n";
  std::cout << "  Expected: 10, Result: " << (eventCount == 10 ? "PASS" : "FAIL") << "\n\n";

  // Test 2: Deduplication enabled
  std::cout << "Test 2: Deduplication enabled with 100ms window\n";
  bus.setDeduplicationEnabled(true);
  bus.setDeduplicationWindow(100);
  eventCount = 0;

  for (int i = 0; i < 10; i++) {
    TestEvent event;
    bus.publish(event);
  }
  std::cout << "  Published 10 identical events rapidly, received: " << eventCount << " events\n";
  std::cout << "  Expected: 1, Result: " << (eventCount == 1 ? "PASS" : "FAIL") << "\n\n";

  // Test 3: Events after window expire are processed
  std::cout << "Test 3: Events after window expiration\n";
  std::cout << "  Waiting 150ms for window to expire...\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  TestEvent event2;
  bus.publish(event2);
  std::cout << "  Published event after window, total received: " << eventCount << " events\n";
  std::cout << "  Expected: 2, Result: " << (eventCount == 2 ? "PASS" : "FAIL") << "\n\n";

  // Test 4: Configurable window
  std::cout << "Test 4: Custom window size (50ms)\n";
  // Disable and re-enable to clear cache
  bus.setDeduplicationEnabled(false);
  bus.setDeduplicationWindow(50);
  bus.setDeduplicationEnabled(true);
  std::cout << "  Window set to: " << bus.getDeduplicationWindow() << "ms\n";
  eventCount = 0;

  TestEvent event3;
  bus.publish(event3);
  std::cout << "  First event received: " << eventCount << " events\n";

  // Wait less than window
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  TestEvent event4;
  bus.publish(event4);
  std::cout << "  After 25ms (within window), received: " << eventCount << " events\n";
  std::cout << "  Expected: 1, Result: " << (eventCount == 1 ? "PASS" : "FAIL") << "\n";

  // Wait for window to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  TestEvent event5;
  bus.publish(event5);
  std::cout << "  After 60ms more (window expired), received: " << eventCount << " events\n";
  std::cout << "  Expected: 2, Result: " << (eventCount == 2 ? "PASS" : "FAIL") << "\n\n";

  // Test 5: Different event types
  std::cout << "Test 5: Different event types are not deduplicated\n";
  std::atomic<int> selectionCount{0};
  std::atomic<int> propertyCount{0};

  auto sub1 = bus.subscribe(EditorEventType::SelectionChanged,
                            [&selectionCount](const EditorEvent &) {
                              selectionCount++;
                            });

  auto sub2 = bus.subscribe(EditorEventType::PropertyChanged,
                            [&propertyCount](const EditorEvent &) {
                              propertyCount++;
                            });

  for (int i = 0; i < 5; i++) {
    SelectionChangedEvent selEvent;
    bus.publish(selEvent);

    PropertyChangedEvent propEvent;
    bus.publish(propEvent);
  }

  std::cout << "  Published 5x SelectionChanged, received: " << selectionCount << " events\n";
  std::cout << "  Published 5x PropertyChanged, received: " << propertyCount << " events\n";
  std::cout << "  Expected: 1 each, Result: " << (selectionCount == 1 && propertyCount == 1 ? "PASS" : "FAIL") << "\n\n";

  bus.unsubscribe(sub);
  bus.unsubscribe(sub1);
  bus.unsubscribe(sub2);

  std::cout << "===========================================\n";
  std::cout << "All tests completed!\n";

  return 0;
}
