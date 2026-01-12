/**
 * @file test_event_bus_race.cpp
 * @brief Experiment to reproduce race condition in EventBus
 *
 * This experiment tries to trigger the race condition described in issue #569
 * by having multiple threads dispatch events concurrently while subscribers
 * are being added/removed.
 */

#include "NovelMind/editor/event_bus.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace NovelMind::editor;

struct TestEvent : EditorEvent {
  int value = 0;
  TestEvent() : EditorEvent(EditorEventType::Custom) {}
};

int main() {
  EventBus bus;
  std::atomic<int> eventCount{0};
  std::atomic<int> subscribeCount{0};
  std::atomic<bool> running{true};

  std::cout << "Starting race condition test...\n";
  std::cout << "This will run multiple threads concurrently:\n";
  std::cout << "  - Dispatcher threads publishing events\n";
  std::cout << "  - Subscriber threads adding/removing subscriptions\n";
  std::cout << "  - Handlers that modify subscriptions during dispatch\n\n";

  // Thread 1-4: Dispatch events continuously
  auto dispatcher = [&]() {
    while (running) {
      TestEvent event;
      event.value = eventCount.fetch_add(1);
      bus.publish(event);
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  };

  // Thread 5-6: Add/remove subscribers continuously
  auto subscriber = [&]() {
    std::vector<EventSubscription> subs;
    while (running) {
      // Add subscriber
      auto sub = bus.subscribe([&](const EditorEvent &) {
        subscribeCount++;
      });
      subs.push_back(sub);

      std::this_thread::sleep_for(std::chrono::microseconds(50));

      // Remove oldest subscriber if we have many
      if (subs.size() > 10) {
        bus.unsubscribe(subs.front());
        subs.erase(subs.begin());
      }
    }

    // Cleanup
    for (auto &sub : subs) {
      bus.unsubscribe(sub);
    }
  };

  // Thread 7: Handler that modifies subscriptions during dispatch
  std::vector<EventSubscription> recursiveSubs;
  auto recursiveHandler = [&]() {
    auto sub = bus.subscribe([&](const EditorEvent &) {
      // Try to subscribe during dispatch
      if (recursiveSubs.size() < 5) {
        auto newSub = bus.subscribe([](const EditorEvent &) {});
        recursiveSubs.push_back(newSub);
      }
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    bus.unsubscribe(sub);
    for (auto &s : recursiveSubs) {
      bus.unsubscribe(s);
    }
  };

  // Start threads
  std::vector<std::thread> threads;

  // 4 dispatcher threads
  for (int i = 0; i < 4; i++) {
    threads.emplace_back(dispatcher);
  }

  // 2 subscriber threads
  for (int i = 0; i < 2; i++) {
    threads.emplace_back(subscriber);
  }

  // 1 recursive handler thread
  threads.emplace_back(recursiveHandler);

  // Run for 2 seconds
  std::this_thread::sleep_for(std::chrono::seconds(2));
  running = false;

  // Wait for all threads
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  std::cout << "Test completed!\n";
  std::cout << "Total events dispatched: " << eventCount << "\n";
  std::cout << "Total handler calls: " << subscribeCount << "\n";
  std::cout << "\nIf no crash occurred, the race condition is handled.\n";
  std::cout << "Run with TSan to detect data races:\n";
  std::cout << "  cmake -B build -DNOVELMIND_ENABLE_TSAN=ON\n";
  std::cout << "  cmake --build build\n";
  std::cout << "  ./build/bin/test_event_bus_race\n";

  return 0;
}
