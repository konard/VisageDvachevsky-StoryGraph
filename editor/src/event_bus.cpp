#include "NovelMind/editor/event_bus.hpp"
#include <algorithm>

namespace NovelMind::editor {

EventBus::EventBus() = default;

EventBus::~EventBus() = default;

EventBus &EventBus::instance() {
  // Thread-safe in C++11+ due to magic statics
  static EventBus instance;
  return instance;
}

// ============================================================================
// Publishing
// ============================================================================

void EventBus::publish(const EditorEvent &event) {
  if (m_synchronous) {
    dispatchEvent(event);
  } else {
    // Create a copy for queuing
    auto eventCopy = std::make_unique<EditorEvent>(event);
    queueEvent(std::move(eventCopy));
  }
}

void EventBus::publish(std::unique_ptr<EditorEvent> event) {
  if (!event) {
    return;
  }

  if (m_synchronous) {
    dispatchEvent(*event);
  } else {
    queueEvent(std::move(event));
  }
}

void EventBus::queueEvent(std::unique_ptr<EditorEvent> event) {
  if (!event) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_eventQueue.push(std::move(event));
}

void EventBus::processQueuedEvents() {
  std::queue<std::unique_ptr<EditorEvent>> eventsToProcess;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::swap(eventsToProcess, m_eventQueue);
  }

  while (!eventsToProcess.empty()) {
    auto event = std::move(eventsToProcess.front());
    eventsToProcess.pop();

    if (event) {
      dispatchEvent(*event);
    }
  }
}

void EventBus::dispatchEvent(const EditorEvent &event) {
  // Record in history if enabled
  if (m_historyEnabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHistory.push_back(event.getDescription());
    if (m_eventHistory.size() > MAX_HISTORY_SIZE) {
      m_eventHistory.erase(m_eventHistory.begin());
    }
  }

  // Increment dispatch depth to defer modifications
  m_dispatchDepth++;

  // Access subscribers directly without copying
  // Lock only during iteration to read subscribers
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Dispatch to all matching subscribers
    for (const auto &subscriber : m_subscribers) {
      bool shouldHandle = true;

      // Check type filter
      if (subscriber.typeFilter.has_value() &&
          subscriber.typeFilter.value() != event.type) {
        shouldHandle = false;
      }

      // Check custom filter
      if (shouldHandle && subscriber.customFilter.has_value() &&
          !subscriber.customFilter.value()(event)) {
        shouldHandle = false;
      }

      if (shouldHandle && subscriber.handler) {
        try {
          subscriber.handler(event);
        } catch (...) {
          // Log error but continue dispatching
        }
      }
    }
  }

  // Decrement dispatch depth and process pending operations
  m_dispatchDepth--;
  if (m_dispatchDepth == 0) {
    processPendingOperations();
  }
}

void EventBus::processPendingOperations() {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &op : m_pendingOperations) {
    switch (op.type) {
    case PendingOperation::Type::Add:
      m_subscribers.push_back(op.subscriber);
      break;

    case PendingOperation::Type::Remove:
      m_subscribers.erase(
          std::remove_if(m_subscribers.begin(), m_subscribers.end(),
                         [&op](const Subscriber &sub) {
                           return sub.id == op.subscriptionId;
                         }),
          m_subscribers.end());
      break;

    case PendingOperation::Type::RemoveByType:
      m_subscribers.erase(
          std::remove_if(m_subscribers.begin(), m_subscribers.end(),
                         [&op](const Subscriber &sub) {
                           return sub.typeFilter.has_value() &&
                                  sub.typeFilter.value() == op.eventType;
                         }),
          m_subscribers.end());
      break;

    case PendingOperation::Type::RemoveAll:
      m_subscribers.clear();
      break;
    }
  }

  m_pendingOperations.clear();
}

// ============================================================================
// Subscription
// ============================================================================

EventSubscription EventBus::subscribe(EventHandler handler) {
  std::lock_guard<std::mutex> lock(m_mutex);

  Subscriber sub;
  sub.id = m_nextSubscriberId++;
  sub.handler = std::move(handler);

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::Add;
    op.subscriber = sub;
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.push_back(std::move(sub));
  }

  return EventSubscription(sub.id);
}

EventSubscription EventBus::subscribe(EditorEventType type,
                                      EventHandler handler) {
  std::lock_guard<std::mutex> lock(m_mutex);

  Subscriber sub;
  sub.id = m_nextSubscriberId++;
  sub.handler = std::move(handler);
  sub.typeFilter = type;

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::Add;
    op.subscriber = sub;
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.push_back(std::move(sub));
  }

  return EventSubscription(sub.id);
}

EventSubscription EventBus::subscribe(EventFilter filter,
                                      EventHandler handler) {
  std::lock_guard<std::mutex> lock(m_mutex);

  Subscriber sub;
  sub.id = m_nextSubscriberId++;
  sub.handler = std::move(handler);
  sub.customFilter = std::move(filter);

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::Add;
    op.subscriber = sub;
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.push_back(std::move(sub));
  }

  return EventSubscription(sub.id);
}

void EventBus::unsubscribe(const EventSubscription &subscription) {
  if (!subscription.isValid()) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::Remove;
    op.subscriptionId = subscription.getId();
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.erase(
        std::remove_if(m_subscribers.begin(), m_subscribers.end(),
                       [&subscription](const Subscriber &sub) {
                         return sub.id == subscription.getId();
                       }),
        m_subscribers.end());
  }
}

void EventBus::unsubscribeAll(EditorEventType type) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::RemoveByType;
    op.eventType = type;
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.erase(
        std::remove_if(m_subscribers.begin(), m_subscribers.end(),
                       [type](const Subscriber &sub) {
                         return sub.typeFilter.has_value() &&
                                sub.typeFilter.value() == type;
                       }),
        m_subscribers.end());
  }
}

void EventBus::unsubscribeAll() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_dispatchDepth > 0) {
    // Defer operation until dispatch completes
    PendingOperation op;
    op.type = PendingOperation::Type::RemoveAll;
    m_pendingOperations.push_back(std::move(op));
  } else {
    // Apply immediately
    m_subscribers.clear();
  }
}

// ============================================================================
// Event History
// ============================================================================

void EventBus::setHistoryEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_historyEnabled = enabled;
}

std::vector<std::string> EventBus::getRecentEvents(size_t count) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (count >= m_eventHistory.size()) {
    return m_eventHistory;
  }

  return std::vector<std::string>(m_eventHistory.end() -
                                      static_cast<std::ptrdiff_t>(count),
                                  m_eventHistory.end());
}

void EventBus::clearHistory() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_eventHistory.clear();
}

// ============================================================================
// Configuration
// ============================================================================

void EventBus::setSynchronous(bool sync) { m_synchronous = sync; }

bool EventBus::isSynchronous() const { return m_synchronous; }

} // namespace NovelMind::editor
