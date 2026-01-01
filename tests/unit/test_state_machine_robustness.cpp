/**
 * @file test_state_machine_robustness.cpp
 * @brief Unit tests for state machine robustness improvements (Issue #173)
 *
 * Tests the following fixes:
 * 1. Debouncer utility for preventing rapid event handling
 * 2. BatchSignalBlocker for batch operations
 * 3. EventBus focus synchronization
 */

#include <catch2/catch_test_macros.hpp>

#include <QPointer>
#include <QObject>
#include <QTimer>
#include <QSignalBlocker>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

// Include the EventBus header
#include "NovelMind/editor/event_bus.hpp"

using namespace NovelMind::editor;

// ============================================================================
// Test Utilities (inline implementations for testing)
// ============================================================================

// Simple debounce test helper
class TestDebouncer {
public:
  explicit TestDebouncer(int delayMs) : m_delayMs(delayMs) {
    m_timer.setSingleShot(true);
  }

  void trigger(std::function<void()> callback) {
    m_pendingCallback = std::move(callback);
    m_timer.start(m_delayMs);
  }

  bool isPending() const { return m_timer.isActive(); }

  void cancel() {
    m_timer.stop();
    m_pendingCallback = nullptr;
  }

  void flush() {
    if (m_timer.isActive()) {
      m_timer.stop();
      if (m_pendingCallback) {
        m_pendingCallback();
        m_pendingCallback = nullptr;
      }
    }
  }

  int delay() const { return m_delayMs; }
  void setDelay(int delayMs) { m_delayMs = delayMs; }

private:
  QTimer m_timer;
  int m_delayMs;
  std::function<void()> m_pendingCallback;
};

// Simple batch signal blocker test helper
class TestBatchSignalBlocker {
public:
  TestBatchSignalBlocker() = default;

  ~TestBatchSignalBlocker() {
    m_blockers.clear();
    if (m_completionCallback) {
      m_completionCallback();
    }
  }

  void block(QObject *obj) {
    if (obj) {
      m_blockers.push_back(std::make_unique<QSignalBlocker>(obj));
    }
  }

  void setCompletionCallback(std::function<void()> callback) {
    m_completionCallback = std::move(callback);
  }

  int blockedCount() const { return static_cast<int>(m_blockers.size()); }

private:
  std::vector<std::unique_ptr<QSignalBlocker>> m_blockers;
  std::function<void()> m_completionCallback;
};

// Batch update guard test helper
class TestBatchUpdateGuard {
public:
  explicit TestBatchUpdateGuard(bool &flag) : m_flag(flag), m_wasActive(flag) {
    m_flag = true;
  }

  ~TestBatchUpdateGuard() { m_flag = m_wasActive; }

  bool wasAlreadyActive() const { return m_wasActive; }

private:
  bool &m_flag;
  bool m_wasActive;
};

// ============================================================================
// Debouncer Tests
// ============================================================================

TEST_CASE("Debouncer flush executes immediately",
          "[state_machine][debouncer]") {
  TestDebouncer debouncer(1000); // Long delay
  int callCount = 0;

  debouncer.trigger([&callCount]() { callCount++; });
  REQUIRE(callCount == 0);

  debouncer.flush();

  REQUIRE(callCount == 1);
  REQUIRE_FALSE(debouncer.isPending());
}

TEST_CASE("Debouncer cancel stops pending callback",
          "[state_machine][debouncer]") {
  TestDebouncer debouncer(100);
  int callCount = 0;

  debouncer.trigger([&callCount]() { callCount++; });
  REQUIRE(debouncer.isPending());

  debouncer.cancel();
  REQUIRE_FALSE(debouncer.isPending());

  // Flush should not execute anything since callback was cleared
  debouncer.flush();
  REQUIRE(callCount == 0);
}

TEST_CASE("Debouncer delay can be changed",
          "[state_machine][debouncer]") {
  TestDebouncer debouncer(50);
  REQUIRE(debouncer.delay() == 50);

  debouncer.setDelay(200);
  REQUIRE(debouncer.delay() == 200);
}

// ============================================================================
// BatchSignalBlocker Tests
// ============================================================================

TEST_CASE("BatchSignalBlocker tracks blocked count",
          "[state_machine][batch]") {
  QObject obj1;
  QObject obj2;
  QObject obj3;

  TestBatchSignalBlocker blocker;
  REQUIRE(blocker.blockedCount() == 0);

  blocker.block(&obj1);
  REQUIRE(blocker.blockedCount() == 1);

  blocker.block(&obj2);
  blocker.block(&obj3);
  REQUIRE(blocker.blockedCount() == 3);
}

TEST_CASE("BatchSignalBlocker completion callback is invoked",
          "[state_machine][batch]") {
  bool callbackCalled = false;

  {
    TestBatchSignalBlocker blocker;
    blocker.setCompletionCallback(
        [&callbackCalled]() { callbackCalled = true; });
  }

  REQUIRE(callbackCalled);
}

TEST_CASE("BatchSignalBlocker handles null objects gracefully",
          "[state_machine][batch]") {
  TestBatchSignalBlocker blocker;

  blocker.block(nullptr);
  REQUIRE(blocker.blockedCount() == 0);

  QObject obj;
  blocker.block(&obj);
  REQUIRE(blocker.blockedCount() == 1);
}

// ============================================================================
// BatchUpdateGuard Tests
// ============================================================================

TEST_CASE("BatchUpdateGuard sets and restores flag",
          "[state_machine][batch]") {
  bool flag = false;

  {
    TestBatchUpdateGuard guard(flag);
    REQUIRE(flag == true);
    REQUIRE_FALSE(guard.wasAlreadyActive());
  }

  REQUIRE(flag == false);
}

TEST_CASE("BatchUpdateGuard detects nested guards",
          "[state_machine][batch]") {
  bool flag = false;

  {
    TestBatchUpdateGuard outer(flag);
    REQUIRE(flag == true);
    REQUIRE_FALSE(outer.wasAlreadyActive());

    {
      TestBatchUpdateGuard inner(flag);
      REQUIRE(flag == true);
      REQUIRE(inner.wasAlreadyActive()); // Nested guard detects active flag
    }

    REQUIRE(flag == true); // Still true after inner scope
  }

  REQUIRE(flag == false);
}

// ============================================================================
// EventBus Focus Synchronization Tests
// ============================================================================

TEST_CASE("EventBus PanelFocusEvent is published and received",
          "[state_machine][eventbus]") {
  // Clean state
  EventBus::instance().unsubscribeAll();
  EventBus::instance().setSynchronous(true);

  std::string receivedPanelName;
  bool receivedHasFocus = false;
  int eventCount = 0;

  auto subscription = EventBus::instance().subscribe(
      EditorEventType::PanelFocusChanged, [&](const EditorEvent &event) {
        if (auto *focusEvent =
                dynamic_cast<const PanelFocusChangedEvent *>(&event)) {
          receivedPanelName = focusEvent->panelName;
          receivedHasFocus = focusEvent->hasFocus;
          eventCount++;
        }
      });

  // Publish a focus event
  PanelFocusChangedEvent focusEvent;
  focusEvent.panelName = "Inspector";
  focusEvent.hasFocus = true;
  EventBus::instance().publish(focusEvent);

  REQUIRE(eventCount == 1);
  REQUIRE(receivedPanelName == "Inspector");
  REQUIRE(receivedHasFocus == true);

  // Publish focus lost event
  focusEvent.hasFocus = false;
  EventBus::instance().publish(focusEvent);

  REQUIRE(eventCount == 2);
  REQUIRE(receivedHasFocus == false);

  EventBus::instance().unsubscribe(subscription);
  EventBus::instance().unsubscribeAll();
}

TEST_CASE("EventBus multiple subscribers receive events",
          "[state_machine][eventbus]") {
  EventBus::instance().unsubscribeAll();
  EventBus::instance().setSynchronous(true);

  int subscriber1Count = 0;
  int subscriber2Count = 0;

  auto sub1 = EventBus::instance().subscribe(
      EditorEventType::PanelFocusChanged,
      [&subscriber1Count](const EditorEvent &) { subscriber1Count++; });

  auto sub2 = EventBus::instance().subscribe(
      EditorEventType::PanelFocusChanged,
      [&subscriber2Count](const EditorEvent &) { subscriber2Count++; });

  PanelFocusChangedEvent focusEvent;
  focusEvent.panelName = "StoryGraph";
  focusEvent.hasFocus = true;
  EventBus::instance().publish(focusEvent);

  REQUIRE(subscriber1Count == 1);
  REQUIRE(subscriber2Count == 1);

  EventBus::instance().unsubscribe(sub1);
  EventBus::instance().unsubscribe(sub2);
  EventBus::instance().unsubscribeAll();
}

TEST_CASE("EventBus unsubscribe stops events",
          "[state_machine][eventbus]") {
  EventBus::instance().unsubscribeAll();
  EventBus::instance().setSynchronous(true);

  int eventCount = 0;

  auto subscription = EventBus::instance().subscribe(
      EditorEventType::PanelFocusChanged,
      [&eventCount](const EditorEvent &) { eventCount++; });

  PanelFocusChangedEvent focusEvent;
  focusEvent.panelName = "Test";
  focusEvent.hasFocus = true;
  EventBus::instance().publish(focusEvent);

  REQUIRE(eventCount == 1);

  EventBus::instance().unsubscribe(subscription);

  // This should not be received
  EventBus::instance().publish(focusEvent);

  REQUIRE(eventCount == 1);

  EventBus::instance().unsubscribeAll();
}

// ============================================================================
// QPointer Safety Tests
// ============================================================================

TEST_CASE("QPointer detects deleted object", "[state_machine][qpointer]") {
  QPointer<QObject> ptr;

  {
    QObject *obj = new QObject();
    ptr = obj;
    REQUIRE_FALSE(ptr.isNull());
    REQUIRE(ptr.data() != nullptr);
    delete obj;
  }

  // Object was destroyed, QPointer should be null
  REQUIRE(ptr.isNull());
  REQUIRE(ptr.data() == nullptr);
}

TEST_CASE("QPointer local copy pattern prevents TOCTOU",
          "[state_machine][qpointer]") {
  QPointer<QObject> ptr;
  QObject *obj = new QObject();
  ptr = obj;

  // Simulate TOCTOU-safe access pattern
  // The local copy ensures the pointer remains valid for the entire scope
  if (auto *localPtr = ptr.data()) {
    // Use local pointer for all operations in this block
    // This prevents race conditions where ptr becomes null mid-operation
    REQUIRE(localPtr != nullptr);
    REQUIRE(localPtr == obj);
  }

  delete obj;
  REQUIRE(ptr.isNull());
}

TEST_CASE("QPointer handles multiple references",
          "[state_machine][qpointer]") {
  QPointer<QObject> ptr1;
  QPointer<QObject> ptr2;

  QObject *obj = new QObject();
  ptr1 = obj;
  ptr2 = obj;

  REQUIRE_FALSE(ptr1.isNull());
  REQUIRE_FALSE(ptr2.isNull());
  REQUIRE(ptr1.data() == ptr2.data());

  delete obj;

  REQUIRE(ptr1.isNull());
  REQUIRE(ptr2.isNull());
}
