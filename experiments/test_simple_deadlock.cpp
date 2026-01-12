/**
 * @file test_simple_deadlock.cpp
 * @brief Simple test to check if subscribe during dispatch causes deadlock
 */

#include "NovelMind/editor/event_bus.hpp"
#include <iostream>

using namespace NovelMind::editor;

struct TestEvent : EditorEvent {
  TestEvent() : EditorEvent(EditorEventType::Custom) {}
};

int main() {
  EventBus bus;

  std::cout << "Testing subscribe during dispatch...\n";

  EventSubscription newSub;

  // Subscribe a handler that tries to subscribe during dispatch
  auto sub = bus.subscribe([&](const EditorEvent &) {
    std::cout << "Handler called, trying to subscribe...\n";
    newSub = bus.subscribe([](const EditorEvent &) {
      std::cout << "New handler called\n";
    });
    std::cout << "Subscribe completed\n";
  });

  std::cout << "Publishing event...\n";
  TestEvent event;
  bus.publish(event);

  std::cout << "Event published\n";
  std::cout << "Test completed successfully - no deadlock!\n";

  bus.unsubscribe(sub);
  bus.unsubscribe(newSub);

  return 0;
}
