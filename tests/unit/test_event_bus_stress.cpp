/**
 * @file test_event_bus_stress.cpp
 * @brief Stress tests for EventBus performance under high load
 *
 * Tests for Issue #546: EventBus stress testing
 * - High frequency event publishing
 * - Many subscribers scalability
 * - Long handler chains
 * - Memory usage under load
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "NovelMind/editor/event_bus.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <random>

using namespace NovelMind::editor;

// ============================================================================
// Helper Events
// ============================================================================

struct StressTestEvent : EditorEvent {
  int sequenceNumber = 0;
  std::string payload;

  StressTestEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "StressTestEvent #" + std::to_string(sequenceNumber);
  }
};

struct LargePayloadEvent : EditorEvent {
  std::vector<char> data;

  LargePayloadEvent() : EditorEvent(EditorEventType::Custom) {}

  [[nodiscard]] std::string getDescription() const override {
    return "LargePayloadEvent (" + std::to_string(data.size()) + " bytes)";
  }
};

// ============================================================================
// High Frequency Event Publishing Tests
// ============================================================================

TEST_CASE("EventBus Stress: High frequency event publishing",
          "[unit][editor][eventbus][stress][performance]") {
  EventBus bus;

  SECTION("10,000 events with single subscriber") {
    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    auto start = std::chrono::high_resolution_clock::now();

    // Publish 10,000 events as fast as possible
    for (int i = 0; i < 10000; i++) {
      StressTestEvent event;
      event.sequenceNumber = i;
      bus.publish(event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    REQUIRE(eventCount == 10000);
    // Should complete in under 1 second for 10k events
    REQUIRE(duration.count() < 1000);

    bus.unsubscribe(sub);
  }

  SECTION("1,000 events per second for 5 seconds") {
    std::atomic<int> eventCount{0};
    std::atomic<bool> stopFlag{false};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    // Simulate high-frequency event stream
    auto publisherThread = std::thread([&bus, &stopFlag]() {
      int count = 0;
      auto nextTime = std::chrono::steady_clock::now();

      for (int sec = 0; sec < 5 && !stopFlag; sec++) {
        for (int i = 0; i < 1000; i++) {
          StressTestEvent event;
          event.sequenceNumber = count++;
          bus.publish(event);

          // Target 1000 events/sec = 1 event per millisecond
          nextTime += std::chrono::microseconds(1000);
          std::this_thread::sleep_until(nextTime);
        }
      }
    });

    publisherThread.join();

    // Should receive close to 5000 events (5 seconds * 1000 events/sec)
    REQUIRE(eventCount >= 4900);  // Allow some tolerance for timing
    REQUIRE(eventCount <= 5100);

    bus.unsubscribe(sub);
  }

  SECTION("Burst publishing with multiple event types") {
    std::atomic<int> selectionEvents{0};
    std::atomic<int> propertyEvents{0};
    std::atomic<int> graphEvents{0};

    auto sub1 = bus.subscribe(EditorEventType::SelectionChanged,
                              [&selectionEvents](const EditorEvent &) {
                                selectionEvents++;
                              });

    auto sub2 = bus.subscribe(EditorEventType::PropertyChanged,
                              [&propertyEvents](const EditorEvent &) {
                                propertyEvents++;
                              });

    auto sub3 = bus.subscribe(EditorEventType::GraphNodeAdded,
                              [&graphEvents](const EditorEvent &) {
                                graphEvents++;
                              });

    auto start = std::chrono::high_resolution_clock::now();

    // Publish bursts of different event types
    for (int burst = 0; burst < 100; burst++) {
      for (int i = 0; i < 10; i++) {
        SelectionChangedEvent selEvent;
        bus.publish(selEvent);

        PropertyChangedEvent propEvent;
        bus.publish(propEvent);

        GraphNodeAddedEvent graphEvent;
        bus.publish(graphEvent);
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    REQUIRE(selectionEvents == 1000);
    REQUIRE(propertyEvents == 1000);
    REQUIRE(graphEvents == 1000);
    // 3000 events should complete quickly
    REQUIRE(duration.count() < 500);

    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
    bus.unsubscribe(sub3);
  }
}

// ============================================================================
// Many Subscribers Scalability Tests
// ============================================================================

TEST_CASE("EventBus Stress: Many subscribers",
          "[unit][editor][eventbus][stress][scalability]") {
  EventBus bus;

  SECTION("1,000 subscribers receiving same event") {
    std::atomic<int> totalCalls{0};
    std::vector<EventSubscription> subscriptions;

    // Subscribe 1,000 handlers
    for (int i = 0; i < 1000; i++) {
      auto sub = bus.subscribe([&totalCalls](const EditorEvent &) {
        totalCalls++;
      });
      subscriptions.push_back(sub);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Publish 100 events
    for (int i = 0; i < 100; i++) {
      StressTestEvent event;
      event.sequenceNumber = i;
      bus.publish(event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Each of 100 events should trigger 1000 handlers
    REQUIRE(totalCalls == 100000);
    // Should complete in reasonable time (less than 2 seconds)
    REQUIRE(duration.count() < 2000);

    // Clean up
    for (auto &sub : subscriptions) {
      bus.unsubscribe(sub);
    }
  }

  SECTION("500 subscribers with different type filters") {
    std::atomic<int> selectionCount{0};
    std::atomic<int> propertyCount{0};
    std::atomic<int> graphCount{0};
    std::vector<EventSubscription> subscriptions;

    // Subscribe 500 handlers across 3 event types
    for (int i = 0; i < 500; i++) {
      if (i % 3 == 0) {
        auto sub = bus.subscribe(EditorEventType::SelectionChanged,
                                [&selectionCount](const EditorEvent &) {
                                  selectionCount++;
                                });
        subscriptions.push_back(sub);
      } else if (i % 3 == 1) {
        auto sub = bus.subscribe(EditorEventType::PropertyChanged,
                                [&propertyCount](const EditorEvent &) {
                                  propertyCount++;
                                });
        subscriptions.push_back(sub);
      } else {
        auto sub = bus.subscribe(EditorEventType::GraphNodeAdded,
                                [&graphCount](const EditorEvent &) {
                                  graphCount++;
                                });
        subscriptions.push_back(sub);
      }
    }

    // Publish mix of events
    for (int i = 0; i < 300; i++) {
      if (i % 3 == 0) {
        SelectionChangedEvent event;
        bus.publish(event);
      } else if (i % 3 == 1) {
        PropertyChangedEvent event;
        bus.publish(event);
      } else {
        GraphNodeAddedEvent event;
        bus.publish(event);
      }
    }

    // Each event type should have ~167 subscribers and ~100 events
    REQUIRE(selectionCount > 15000);  // ~167 * 100
    REQUIRE(propertyCount > 15000);
    REQUIRE(graphCount > 15000);

    // Clean up
    for (auto &sub : subscriptions) {
      bus.unsubscribe(sub);
    }
  }

  SECTION("Adding/removing subscribers during high event rate") {
    std::atomic<int> eventCount{0};
    std::atomic<bool> stopFlag{false};
    std::vector<EventSubscription> activeSubs;
    std::mutex subsMutex;

    // Publisher thread - continuous event stream
    auto publisherThread = std::thread([&bus, &stopFlag]() {
      int count = 0;
      while (!stopFlag) {
        StressTestEvent event;
        event.sequenceNumber = count++;
        bus.publish(event);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    });

    // Subscriber management thread - continuously add/remove
    auto subscriberThread = std::thread([&bus, &activeSubs, &subsMutex, &stopFlag, &eventCount]() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dis(0, 1);

      for (int i = 0; i < 100 && !stopFlag; i++) {
        // Add a subscriber
        {
          std::lock_guard<std::mutex> lock(subsMutex);
          auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
            eventCount++;
          });
          activeSubs.push_back(sub);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Randomly remove a subscriber if we have many
        if (dis(gen) == 0 && activeSubs.size() > 5) {
          std::lock_guard<std::mutex> lock(subsMutex);
          bus.unsubscribe(activeSubs.back());
          activeSubs.pop_back();
        }
      }
    });

    subscriberThread.join();
    stopFlag = true;
    publisherThread.join();

    // Should have received many events without crashes
    REQUIRE(eventCount > 0);

    // Clean up remaining subscribers
    for (auto &sub : activeSubs) {
      bus.unsubscribe(sub);
    }
  }
}

// ============================================================================
// Long Handler Chains Tests
// ============================================================================

TEST_CASE("EventBus Stress: Long handler chains",
          "[unit][editor][eventbus][stress][chains]") {
  EventBus bus;

  SECTION("Handler chain triggering nested events") {
    std::atomic<int> depth0Count{0};
    std::atomic<int> depth1Count{0};
    std::atomic<int> depth2Count{0};
    std::atomic<int> depth3Count{0};

    // Depth 3: Leaf handler
    auto sub3 = bus.subscribe(EditorEventType::ErrorOccurred,
                              [&depth3Count](const EditorEvent &) {
                                depth3Count++;
                              });

    // Depth 2: Triggers ErrorOccurred
    auto sub2 = bus.subscribe(EditorEventType::WarningOccurred,
                              [&bus, &depth2Count](const EditorEvent &) {
                                depth2Count++;
                                ErrorEvent errEvent(EditorEventType::ErrorOccurred);
                                errEvent.message = "Chain depth 2 -> 3";
                                bus.publish(errEvent);
                              });

    // Depth 1: Triggers WarningOccurred
    auto sub1 = bus.subscribe(EditorEventType::PropertyChanged,
                              [&bus, &depth1Count](const EditorEvent &) {
                                depth1Count++;
                                ErrorEvent warnEvent(EditorEventType::WarningOccurred);
                                warnEvent.message = "Chain depth 1 -> 2";
                                bus.publish(warnEvent);
                              });

    // Depth 0: Triggers PropertyChanged
    auto sub0 = bus.subscribe(EditorEventType::SelectionChanged,
                              [&bus, &depth0Count](const EditorEvent &) {
                                depth0Count++;
                                PropertyChangedEvent propEvent;
                                propEvent.objectId = "test";
                                bus.publish(propEvent);
                              });

    // Trigger the chain
    SelectionChangedEvent selEvent;
    bus.publish(selEvent);

    // Verify all handlers in the chain were called
    REQUIRE(depth0Count == 1);
    REQUIRE(depth1Count == 1);
    REQUIRE(depth2Count == 1);
    REQUIRE(depth3Count == 1);

    // Clean up
    bus.unsubscribe(sub0);
    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
    bus.unsubscribe(sub3);
  }

  SECTION("Multiple handlers each triggering more events") {
    std::atomic<int> primaryEvents{0};
    std::atomic<int> secondaryEvents{0};

    // Secondary handlers (triggered by primary handlers)
    auto sub2 = bus.subscribe(EditorEventType::PropertyChanged,
                              [&secondaryEvents](const EditorEvent &) {
                                secondaryEvents++;
                              });

    // Primary handlers that each trigger a new event
    std::vector<EventSubscription> primarySubs;
    for (int i = 0; i < 10; i++) {
      auto sub = bus.subscribe(EditorEventType::SelectionChanged,
                               [&bus, &primaryEvents](const EditorEvent &) {
                                 primaryEvents++;
                                 PropertyChangedEvent propEvent;
                                 bus.publish(propEvent);
                               });
      primarySubs.push_back(sub);
    }

    // Publish single event that triggers the chain
    SelectionChangedEvent selEvent;
    bus.publish(selEvent);

    // 1 SelectionChanged triggers 10 handlers, each publishing PropertyChanged
    REQUIRE(primaryEvents == 10);
    REQUIRE(secondaryEvents == 10);

    // Clean up
    for (auto &sub : primarySubs) {
      bus.unsubscribe(sub);
    }
    bus.unsubscribe(sub2);
  }

  SECTION("Deep recursion with event chains (10 levels)") {
    std::vector<std::atomic<int>> depthCounts(10);
    std::vector<EventSubscription> subs;

    // Create chain of 10 handlers, each triggering the next
    for (int depth = 0; depth < 10; depth++) {
      EditorEventType currentType = static_cast<EditorEventType>(
          static_cast<u32>(EditorEventType::Custom) + depth);
      EditorEventType nextType = static_cast<EditorEventType>(
          static_cast<u32>(EditorEventType::Custom) + depth + 1);

      if (depth < 9) {
        // Intermediate handler - triggers next level
        auto sub = bus.subscribe(currentType,
                                 [&bus, &depthCounts, depth, nextType](const EditorEvent &) {
                                   depthCounts[depth]++;
                                   StressTestEvent nextEvent;
                                   nextEvent.type = nextType;
                                   nextEvent.sequenceNumber = depth + 1;
                                   bus.publish(nextEvent);
                                 });
        subs.push_back(sub);
      } else {
        // Leaf handler - doesn't trigger more events
        auto sub = bus.subscribe(currentType,
                                 [&depthCounts, depth](const EditorEvent &) {
                                   depthCounts[depth]++;
                                 });
        subs.push_back(sub);
      }
    }

    // Trigger the chain
    StressTestEvent rootEvent;
    rootEvent.type = static_cast<EditorEventType>(
        static_cast<u32>(EditorEventType::Custom));
    bus.publish(rootEvent);

    // Verify all depths were reached
    for (int i = 0; i < 10; i++) {
      REQUIRE(depthCounts[i] == 1);
    }

    // Clean up
    for (auto &sub : subs) {
      bus.unsubscribe(sub);
    }
  }
}

// ============================================================================
// Memory Usage Under Load Tests
// ============================================================================

TEST_CASE("EventBus Stress: Memory usage under load",
          "[unit][editor][eventbus][stress][memory]") {
  EventBus bus;

  SECTION("Large payload events") {
    std::atomic<size_t> totalBytesReceived{0};

    auto sub = bus.subscribe([&totalBytesReceived](const EditorEvent &event) {
      if (auto *largeEvent = dynamic_cast<const LargePayloadEvent *>(&event)) {
        totalBytesReceived += largeEvent->data.size();
      }
    });

    // Publish 1000 events with 10KB payload each
    for (int i = 0; i < 1000; i++) {
      auto event = std::make_unique<LargePayloadEvent>();
      event->data.resize(10 * 1024);  // 10KB
      bus.publish(std::move(event));
    }

    // Should have processed 10MB total
    REQUIRE(totalBytesReceived == 1000 * 10 * 1024);

    bus.unsubscribe(sub);
  }

  SECTION("Rapid subscribe/unsubscribe cycles") {
    std::atomic<int> eventCount{0};

    // Create and destroy 10,000 subscriptions
    for (int cycle = 0; cycle < 100; cycle++) {
      std::vector<EventSubscription> subs;

      // Add 100 subscribers
      for (int i = 0; i < 100; i++) {
        auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
          eventCount++;
        });
        subs.push_back(sub);
      }

      // Publish one event
      StressTestEvent event;
      bus.publish(event);

      // Remove all subscribers
      for (auto &sub : subs) {
        bus.unsubscribe(sub);
      }
    }

    // Each cycle published 1 event to 100 subscribers
    REQUIRE(eventCount == 100 * 100);
  }

  SECTION("Event queue growth and processing") {
    bus.setSynchronous(false);  // Enable queued mode

    std::atomic<int> processedEvents{0};

    auto sub = bus.subscribe([&processedEvents](const EditorEvent &) {
      processedEvents++;
      // Simulate slow handler
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    });

    // Queue many events rapidly
    for (int i = 0; i < 5000; i++) {
      auto event = std::make_unique<StressTestEvent>();
      event->sequenceNumber = i;
      bus.queueEvent(std::move(event));
    }

    // Process all queued events
    auto start = std::chrono::high_resolution_clock::now();
    bus.processQueuedEvents();
    auto end = std::chrono::high_resolution_clock::now();

    REQUIRE(processedEvents == 5000);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // With 10µs per event, 5000 events should take ~50ms minimum
    REQUIRE(duration.count() >= 40);  // Allow some tolerance

    bus.setSynchronous(true);  // Restore default
    bus.unsubscribe(sub);
  }

  SECTION("Memory leak check with event history") {
    bus.setHistoryEnabled(true);

    std::atomic<int> eventCount{0};

    auto sub = bus.subscribe([&eventCount](const EditorEvent &) {
      eventCount++;
    });

    // Publish many events to fill history buffer multiple times
    // History is limited to MAX_HISTORY_SIZE (100 events)
    for (int i = 0; i < 1000; i++) {
      StressTestEvent event;
      event.sequenceNumber = i;
      bus.publish(event);
    }

    REQUIRE(eventCount == 1000);

    // Get recent events - should only return up to 100
    auto recent = bus.getRecentEvents(200);
    REQUIRE(recent.size() <= 100);

    bus.setHistoryEnabled(false);
    bus.unsubscribe(sub);
  }

  SECTION("Stress test with all event types") {
    std::atomic<int> totalEvents{0};

    auto sub = bus.subscribe([&totalEvents](const EditorEvent &) {
      totalEvents++;
    });

    // Publish 100 of each standard event type
    for (int i = 0; i < 100; i++) {
      SelectionChangedEvent selEvent;
      bus.publish(selEvent);

      PropertyChangedEvent propEvent;
      bus.publish(propEvent);

      GraphNodeAddedEvent graphEvent;
      bus.publish(graphEvent);

      TimelinePlaybackChangedEvent timelineEvent;
      bus.publish(timelineEvent);

      ProjectModifiedEvent projectEvent;
      bus.publish(projectEvent);

      UndoStackChangedEvent undoEvent;
      bus.publish(undoEvent);

      PlayModeEvent playEvent(EditorEventType::PlayModeStarted);
      bus.publish(playEvent);

      AssetEvent assetEvent(EditorEventType::AssetImported);
      bus.publish(assetEvent);

      ErrorEvent errorEvent(EditorEventType::ErrorOccurred);
      bus.publish(errorEvent);

      PanelFocusChangedEvent panelEvent;
      bus.publish(panelEvent);
    }

    // 10 event types * 100 iterations
    REQUIRE(totalEvents == 1000);

    bus.unsubscribe(sub);
  }
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

TEST_CASE("EventBus Benchmark: Publishing performance",
          "[unit][editor][eventbus][benchmark][!benchmark]") {
  EventBus bus;

  BENCHMARK("Publish 1000 events with 1 subscriber") {
    std::atomic<int> count{0};
    auto sub = bus.subscribe([&count](const EditorEvent &) {
      count++;
    });

    for (int i = 0; i < 1000; i++) {
      StressTestEvent event;
      bus.publish(event);
    }

    bus.unsubscribe(sub);
    return count.load();
  };

  BENCHMARK("Publish 1000 events with 100 subscribers") {
    std::atomic<int> count{0};
    std::vector<EventSubscription> subs;

    for (int i = 0; i < 100; i++) {
      subs.push_back(bus.subscribe([&count](const EditorEvent &) {
        count++;
      }));
    }

    for (int i = 0; i < 1000; i++) {
      StressTestEvent event;
      bus.publish(event);
    }

    for (auto &sub : subs) {
      bus.unsubscribe(sub);
    }
    return count.load();
  };

  BENCHMARK("Publish 1000 events with type filtering") {
    std::atomic<int> count{0};
    auto sub = bus.subscribe(EditorEventType::SelectionChanged,
                            [&count](const EditorEvent &) {
                              count++;
                            });

    for (int i = 0; i < 1000; i++) {
      SelectionChangedEvent event;
      bus.publish(event);
    }

    bus.unsubscribe(sub);
    return count.load();
  };
}

TEST_CASE("EventBus Benchmark: Subscription performance",
          "[unit][editor][eventbus][benchmark][!benchmark]") {
  EventBus bus;

  BENCHMARK("Subscribe/unsubscribe 1000 handlers") {
    std::vector<EventSubscription> subs;

    for (int i = 0; i < 1000; i++) {
      subs.push_back(bus.subscribe([](const EditorEvent &) {}));
    }

    for (auto &sub : subs) {
      bus.unsubscribe(sub);
    }

    return subs.size();
  };

  BENCHMARK("Queue and process 1000 events") {
    bus.setSynchronous(false);

    std::atomic<int> count{0};
    auto sub = bus.subscribe([&count](const EditorEvent &) {
      count++;
    });

    for (int i = 0; i < 1000; i++) {
      auto event = std::make_unique<StressTestEvent>();
      bus.queueEvent(std::move(event));
    }

    bus.processQueuedEvents();

    bus.setSynchronous(true);
    bus.unsubscribe(sub);
    return count.load();
  };
}
