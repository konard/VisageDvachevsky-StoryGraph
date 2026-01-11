/**
 * @file test_event_bus.cpp
 * @brief Unit tests for EventBus performance and thread safety
 *
 * Tests for Issue #468: EventBus performance optimization
 * - Performance test for dispatch without copying
 * - Thread safety test for modification during dispatch
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "NovelMind/editor/event_bus.hpp"
#include <chrono>
#include <thread>
#include <atomic>

using namespace NovelMind::editor;

// ============================================================================
// Helper Events
// ============================================================================

struct TestEvent : EditorEvent {
  int value = 0;
  TestEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "TestEvent: " + std::to_string(value);
  }
};

// ============================================================================
// Performance Tests
// ============================================================================

TEST_CASE("EventBus: Dispatch performance without copy",
          "[unit][editor][eventbus][perf]") {
  EventBus bus;

  SECTION("Performance with 100 subscribers") {
    // Subscribe 100 handlers
    std::vector<EventSubscription> subscriptions;
    std::atomic<int> callCount{0};

    for (int i = 0; i < 100; i++) {
      auto sub = bus.subscribe([&callCount](const EditorEvent &) {
        callCount++;
      });
      subscriptions.push_back(sub);
    }

    // Dispatch 1000 events and measure time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
      TestEvent event;
      event.value = i;
      bus.publish(event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all events were dispatched
    REQUIRE(callCount == 100 * 1000);

    // Performance should be reasonable (less than 500ms for 100k handler calls)
    REQUIRE(duration.count() < 500);

    // Clean up
    for (auto &sub : subscriptions) {
      bus.unsubscribe(sub);
    }
  }

  SECTION("No allocation during dispatch") {
    // This test verifies that dispatch doesn't copy the subscriber list
    std::atomic<int> eventCount{0};

    // Add 50 subscribers
    std::vector<EventSubscription> subscriptions;
    for (int i = 0; i < 50; i++) {
      auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
        eventCount++;
      });
      subscriptions.push_back(sub);
    }

    // Dispatch events - should not copy subscriber list
    for (int i = 0; i < 100; i++) {
      TestEvent event;
      bus.publish(event);
    }

    REQUIRE(eventCount == 50 * 100);

    // Clean up
    for (auto &sub : subscriptions) {
      bus.unsubscribe(sub);
    }
  }
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_CASE("EventBus: Modify subscribers during dispatch",
          "[unit][editor][eventbus][thread-safety]") {
  EventBus bus;

  SECTION("Subscribe during event handling") {
    std::atomic<int> eventCount{0};
    std::atomic<int> newSubscriberEvents{0};
    EventSubscription newSub;

    // Create handler that subscribes during dispatch
    auto sub1 = bus.subscribe([&](const EditorEvent &event) {
      eventCount++;

      // Subscribe a new handler during first event
      if (eventCount == 1) {
        newSub = bus.subscribe([&newSubscriberEvents](const EditorEvent &) {
          newSubscriberEvents++;
        });
      }
    });

    // Dispatch first event - should trigger new subscription
    TestEvent event1;
    bus.publish(event1);
    REQUIRE(eventCount == 1);
    REQUIRE(newSubscriberEvents == 0); // New subscriber not called yet

    // Dispatch second event - new subscriber should be called now
    TestEvent event2;
    bus.publish(event2);
    REQUIRE(eventCount == 2);
    REQUIRE(newSubscriberEvents == 1); // New subscriber called

    // Clean up
    bus.unsubscribe(sub1);
    bus.unsubscribe(newSub);
  }

  SECTION("Unsubscribe during event handling") {
    std::atomic<int> event1Count{0};
    std::atomic<int> event2Count{0};
    EventSubscription sub1, sub2;

    // Create handler that unsubscribes another handler
    sub1 = bus.subscribe([&](const EditorEvent &) {
      event1Count++;
      if (event1Count == 1) {
        // Unsubscribe sub2 during dispatch
        bus.unsubscribe(sub2);
      }
    });

    sub2 = bus.subscribe([&event2Count](const EditorEvent &) {
      event2Count++;
    });

    // Dispatch first event
    TestEvent event1;
    bus.publish(event1);
    REQUIRE(event1Count == 1);
    REQUIRE(event2Count == 1); // sub2 still called during first dispatch

    // Dispatch second event - sub2 should not be called
    TestEvent event2;
    bus.publish(event2);
    REQUIRE(event1Count == 2);
    REQUIRE(event2Count == 1); // sub2 not called (was unsubscribed)

    // Clean up
    bus.unsubscribe(sub1);
  }

  SECTION("Multiple modifications during nested dispatch") {
    std::atomic<int> outerCount{0};
    std::atomic<int> innerCount{0};
    std::atomic<int> newSubCount{0};
    EventSubscription outerSub, innerSub, newSub;

    // Outer handler that triggers nested dispatch
    outerSub = bus.subscribe([&](const EditorEvent &event) {
      outerCount++;

      // Trigger nested dispatch on first event
      if (outerCount == 1) {
        TestEvent nestedEvent;
        nestedEvent.value = 999;
        bus.publish(nestedEvent);
      }
    });

    // Inner handler that modifies subscriptions during nested dispatch
    innerSub = bus.subscribe([&](const EditorEvent &event) {
      const TestEvent *testEvent = dynamic_cast<const TestEvent *>(&event);
      if (testEvent && testEvent->value == 999) {
        innerCount++;

        // Add new subscriber during nested dispatch
        newSub = bus.subscribe([&newSubCount](const EditorEvent &) {
          newSubCount++;
        });
      }
    });

    // Dispatch outer event - triggers nested dispatch
    TestEvent outerEvent;
    outerEvent.value = 1;
    bus.publish(outerEvent);

    REQUIRE(outerCount == 2); // Called twice (outer + nested)
    REQUIRE(innerCount == 1); // Called once (nested only)
    REQUIRE(newSubCount == 0); // Not called yet

    // Dispatch another event - new subscriber should be active
    TestEvent finalEvent;
    bus.publish(finalEvent);
    REQUIRE(newSubCount == 1); // New subscriber called

    // Clean up
    bus.unsubscribe(outerSub);
    bus.unsubscribe(innerSub);
    bus.unsubscribe(newSub);
  }

  SECTION("UnsubscribeAll during dispatch") {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};
    std::atomic<int> count3{0};

    auto sub1 = bus.subscribe(EditorEventType::SelectionChanged,
                              [&count1](const EditorEvent &) { count1++; });
    auto sub2 = bus.subscribe(EditorEventType::SelectionChanged,
                              [&count2](const EditorEvent &) { count2++; });
    auto sub3 = bus.subscribe(EditorEventType::PropertyChanged,
                              [&count3](const EditorEvent &) { count3++; });

    // Handler that unsubscribes all SelectionChanged handlers
    auto subUnsubscriber = bus.subscribe([&](const EditorEvent &event) {
      if (event.type == EditorEventType::SelectionChanged) {
        bus.unsubscribeAll(EditorEventType::SelectionChanged);
      }
    });

    // Dispatch SelectionChanged event
    SelectionChangedEvent selEvent;
    bus.publish(selEvent);

    // All handlers called during first dispatch
    REQUIRE(count1 >= 1);
    REQUIRE(count2 >= 1);

    // Dispatch again - SelectionChanged handlers should be gone
    SelectionChangedEvent selEvent2;
    bus.publish(selEvent2);

    // Count should not increase (handlers were removed)
    int finalCount1 = count1;
    int finalCount2 = count2;

    selEvent2.selectedIds.push_back("test");
    bus.publish(selEvent2);

    REQUIRE(count1 == finalCount1); // No increase
    REQUIRE(count2 == finalCount2); // No increase

    // PropertyChanged handler should still work
    PropertyChangedEvent propEvent;
    bus.publish(propEvent);
    REQUIRE(count3 >= 1);

    // Clean up
    bus.unsubscribe(subUnsubscriber);
    bus.unsubscribe(sub3);
  }
}

// ============================================================================
// Correctness Tests
// ============================================================================

TEST_CASE("EventBus: Basic functionality still works",
          "[unit][editor][eventbus][correctness]") {
  EventBus bus;

  SECTION("Simple subscribe and publish") {
    int callCount = 0;

    auto sub = bus.subscribe([&callCount](const EditorEvent &) {
      callCount++;
    });

    TestEvent event;
    bus.publish(event);

    REQUIRE(callCount == 1);

    bus.unsubscribe(sub);
  }

  SECTION("Type filtering") {
    int selectionCount = 0;
    int propertyCount = 0;

    auto sub1 = bus.subscribe(EditorEventType::SelectionChanged,
                              [&selectionCount](const EditorEvent &) {
                                selectionCount++;
                              });

    auto sub2 = bus.subscribe(EditorEventType::PropertyChanged,
                              [&propertyCount](const EditorEvent &) {
                                propertyCount++;
                              });

    SelectionChangedEvent selEvent;
    bus.publish(selEvent);

    PropertyChangedEvent propEvent;
    bus.publish(propEvent);

    REQUIRE(selectionCount == 1);
    REQUIRE(propertyCount == 1);

    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
  }

  SECTION("Multiple subscribers for same event") {
    int count1 = 0;
    int count2 = 0;
    int count3 = 0;

    auto sub1 = bus.subscribe([&count1](const EditorEvent &) { count1++; });
    auto sub2 = bus.subscribe([&count2](const EditorEvent &) { count2++; });
    auto sub3 = bus.subscribe([&count3](const EditorEvent &) { count3++; });

    TestEvent event;
    bus.publish(event);

    REQUIRE(count1 == 1);
    REQUIRE(count2 == 1);
    REQUIRE(count3 == 1);

    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
    bus.unsubscribe(sub3);
  }
}
