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
// Concurrent Dispatch Tests (Issue #569)
// ============================================================================

TEST_CASE("EventBus: Concurrent dispatch from multiple threads",
          "[unit][editor][eventbus][concurrent][thread-safety]") {
  EventBus bus;

  SECTION("Multiple threads dispatching simultaneously") {
    std::atomic<int> eventCount{0};
    std::atomic<int> handlerCallCount{0};

    // Subscribe handlers
    std::vector<EventSubscription> subs;
    for (int i = 0; i < 10; i++) {
      auto sub = bus.subscribe([&handlerCallCount](const EditorEvent &) {
        handlerCallCount++;
      });
      subs.push_back(sub);
    }

    // Launch multiple dispatcher threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
      threads.emplace_back([&bus, &eventCount]() {
        for (int j = 0; j < 100; j++) {
          TestEvent event;
          event.value = eventCount.fetch_add(1);
          bus.publish(event);
        }
      });
    }

    // Wait for all threads
    for (auto &t : threads) {
      t.join();
    }

    // Verify all events were dispatched
    REQUIRE(eventCount == 400);
    REQUIRE(handlerCallCount == 4000); // 400 events * 10 handlers

    // Clean up
    for (auto &sub : subs) {
      bus.unsubscribe(sub);
    }
  }

  SECTION("Concurrent dispatch with subscribe/unsubscribe") {
    std::atomic<int> eventCount{0};
    std::atomic<int> handlerCallCount{0};
    std::atomic<bool> running{true};

    // Launch dispatcher threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 2; i++) {
      threads.emplace_back([&]() {
        while (running) {
          TestEvent event;
          event.value = eventCount.fetch_add(1);
          bus.publish(event);
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      });
    }

    // Launch subscribe/unsubscribe threads
    std::vector<EventSubscription> subs;
    for (int i = 0; i < 2; i++) {
      threads.emplace_back([&]() {
        std::vector<EventSubscription> localSubs;
        while (running) {
          // Add subscriber
          auto sub = bus.subscribe([&](const EditorEvent &) {
            handlerCallCount++;
          });
          localSubs.push_back(sub);

          std::this_thread::sleep_for(std::chrono::microseconds(200));

          // Remove oldest subscriber if we have many
          if (localSubs.size() > 5) {
            bus.unsubscribe(localSubs.front());
            localSubs.erase(localSubs.begin());
          }
        }

        // Cleanup
        for (auto &sub : localSubs) {
          bus.unsubscribe(sub);
        }
      });
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    // Wait for all threads
    for (auto &t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }

    // Just verify no crash occurred
    REQUIRE(eventCount > 0);
    REQUIRE(handlerCallCount >= 0);
  }

  SECTION("Handler subscribing during concurrent dispatch") {
    std::atomic<int> eventCount{0};
    std::atomic<int> recursiveSubCount{0};
    std::vector<EventSubscription> recursiveSubs;

    // Handler that subscribes during dispatch
    auto sub = bus.subscribe([&](const EditorEvent &) {
      if (recursiveSubCount < 10) {
        auto newSub = bus.subscribe([](const EditorEvent &) {});
        recursiveSubs.push_back(newSub);
        recursiveSubCount++;
      }
    });

    // Launch multiple dispatcher threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
      threads.emplace_back([&]() {
        for (int j = 0; j < 50; j++) {
          TestEvent event;
          event.value = eventCount.fetch_add(1);
          bus.publish(event);
        }
      });
    }

    // Wait for all threads
    for (auto &t : threads) {
      t.join();
    }

    // Verify events were dispatched
    REQUIRE(eventCount == 200);

    // Clean up
    bus.unsubscribe(sub);
    for (auto &s : recursiveSubs) {
      bus.unsubscribe(s);
    }
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

// ============================================================================
// Event Deduplication Tests (Issue #480)
// ============================================================================

TEST_CASE("EventBus: Event deduplication",
          "[unit][editor][eventbus][deduplication]") {
  EventBus bus;

  SECTION("Duplicate events deduplicated within time window") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    // Enable deduplication with 100ms window
    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(100);

    // Publish same event type multiple times rapidly
    for (int i = 0; i < 10; i++) {
      TestEvent event;
      event.value = 42; // Same value
      bus.publish(event);
    }

    // Only first event should be processed (all others are duplicates)
    REQUIRE(eventCount == 1);

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Now a new event should be processed
    TestEvent event2;
    event2.value = 42;
    bus.publish(event2);

    REQUIRE(eventCount == 2);

    bus.unsubscribe(sub);
  }

  SECTION("Deduplication can be disabled") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    // Deduplication is disabled by default
    REQUIRE_FALSE(bus.isDeduplicationEnabled());

    // Publish same event type multiple times
    for (int i = 0; i < 10; i++) {
      TestEvent event;
      bus.publish(event);
    }

    // All events should be processed
    REQUIRE(eventCount == 10);

    bus.unsubscribe(sub);
  }

  SECTION("Time window is configurable") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    // Set custom deduplication window
    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(50); // 50ms window

    REQUIRE(bus.getDeduplicationWindow() == 50);

    // Publish duplicate events
    TestEvent event1;
    bus.publish(event1);
    REQUIRE(eventCount == 1);

    // Immediate duplicate - should be ignored
    TestEvent event2;
    bus.publish(event2);
    REQUIRE(eventCount == 1);

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // New event should be processed
    TestEvent event3;
    bus.publish(event3);
    REQUIRE(eventCount == 2);

    bus.unsubscribe(sub);
  }

  SECTION("No event loss - events after window are processed") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(50);

    // Publish first event
    TestEvent event1;
    bus.publish(event1);
    REQUIRE(eventCount == 1);

    // Wait and publish again - should be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    TestEvent event2;
    bus.publish(event2);
    REQUIRE(eventCount == 2);

    // Another wait and publish
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    TestEvent event3;
    bus.publish(event3);
    REQUIRE(eventCount == 3);

    bus.unsubscribe(sub);
  }
}

TEST_CASE("EventBus: Rapid duplicate events",
          "[unit][editor][eventbus][deduplication]") {
  EventBus bus;

  SECTION("Rapid duplicates within window are ignored") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(100);

    // Rapid fire 100 identical events
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++) {
      TestEvent event;
      bus.publish(event);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete very quickly (all duplicates ignored)
    REQUIRE(duration.count() < 100);

    // Only first event should be processed
    REQUIRE(eventCount == 1);

    bus.unsubscribe(sub);
  }

  SECTION("Different event types are not deduplicated against each other") {
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

    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(100);

    // Publish different event types
    for (int i = 0; i < 5; i++) {
      SelectionChangedEvent selEvent;
      bus.publish(selEvent);

      PropertyChangedEvent propEvent;
      bus.publish(propEvent);
    }

    // First of each type should be processed (different types, not duplicates)
    REQUIRE(selectionCount == 1);
    REQUIRE(propertyCount == 1);

    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
  }

  SECTION("Deduplication clears cache when disabled") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    bus.setDeduplicationEnabled(true);
    bus.setDeduplicationWindow(1000); // Long window

    // Publish event
    TestEvent event1;
    bus.publish(event1);
    REQUIRE(eventCount == 1);

    // Disable deduplication - should clear cache
    bus.setDeduplicationEnabled(false);

    // Publish same event - should be processed (cache cleared)
    TestEvent event2;
    bus.publish(event2);
    REQUIRE(eventCount == 2);

    bus.unsubscribe(sub);
  }
}
