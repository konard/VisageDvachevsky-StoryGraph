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
  // Check for duplicate events if deduplication is enabled
  if (m_deduplicationEnabled) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string eventKey = event.getEventKey();
    u64 currentTime = event.timestamp;

    // Clean up old entries outside the time window
    auto it = m_recentEvents.begin();
    while (it != m_recentEvents.end()) {
      u64 eventAge = currentTime - it->second;
      // Convert milliseconds to nanoseconds for comparison with timestamp
      u64 windowNs = m_deduplicationWindowMs * 1000000;

      if (eventAge > windowNs) {
        it = m_recentEvents.erase(it);
      } else {
        ++it;
      }
    }

    // Check if this event is a duplicate
    auto found = m_recentEvents.find(eventKey);
    if (found != m_recentEvents.end()) {
      u64 timeSinceLastEvent = currentTime - found->second;
      u64 windowNs = m_deduplicationWindowMs * 1000000;

      if (timeSinceLastEvent <= windowNs) {
        // This is a duplicate event within the time window - skip it
        return;
      }
    }

    // Record this event
    m_recentEvents[eventKey] = currentTime;
  }

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

  // Copy subscriber list to avoid holding mutex during handler execution
  // This prevents deadlock when handlers call subscribe/unsubscribe
  std::vector<Subscriber> subscribersCopy;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    subscribersCopy = m_subscribers;
  }

  // Dispatch to all matching subscribers without holding the mutex
  // This allows handlers to safely call subscribe/unsubscribe
  for (const auto &subscriber : subscribersCopy) {
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

void EventBus::setDeduplicationEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_deduplicationEnabled = enabled;
  if (!enabled) {
    // Clear deduplication cache when disabled
    m_recentEvents.clear();
  }
}

bool EventBus::isDeduplicationEnabled() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_deduplicationEnabled;
}

void EventBus::setDeduplicationWindow(u64 windowMs) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_deduplicationWindowMs = windowMs;
}

u64 EventBus::getDeduplicationWindow() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_deduplicationWindowMs;
}

} // namespace NovelMind::editor
