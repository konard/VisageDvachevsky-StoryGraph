# EventBus Architecture Documentation

> **See also**: [gui_architecture.md](gui_architecture.md) - GUI architecture overview
> **See also**: [SCENE_STORY_GRAPH_PIPELINE.md](SCENE_STORY_GRAPH_PIPELINE.md) - Scene and Story Graph integration

This document provides comprehensive architecture documentation for the NovelMind editor's EventBus system, including event flow patterns, subscriber lifecycle, best practices, and thread safety guarantees.

## Table of Contents

1. [Overview](#overview)
2. [Core Architecture](#core-architecture)
3. [Event Type System](#event-type-system)
4. [Event Flow Patterns](#event-flow-patterns)
5. [Subscriber Lifecycle](#subscriber-lifecycle)
6. [Thread Safety Guarantees](#thread-safety-guarantees)
7. [Qt Integration (QtEventBus)](#qt-integration-qteventbus)
8. [Common Usage Patterns](#common-usage-patterns)
9. [Best Practices](#best-practices)
10. [Performance Considerations](#performance-considerations)
11. [Debugging and Troubleshooting](#debugging-and-troubleshooting)

---

## Overview

The EventBus system is a **centralized, thread-safe event dispatching system** that enables loose coupling between editor components. It implements the Observer pattern and serves as the communication backbone for the NovelMind editor.

### Key Features

- **Type-safe event publishing and subscription**
- **Thread-safe concurrent dispatch** with mutex protection
- **Synchronous and asynchronous processing modes**
- **Event filtering** by type or custom predicates
- **RAII-based subscription management**
- **Event history** for debugging (optional)
- **Deferred subscription modifications** to prevent iterator invalidation
- **Qt integration layer** for seamless Qt signals/slots interoperability

### Design Goals

1. **Loose Coupling**: Panels and systems communicate without direct references
2. **Type Safety**: Compile-time type checking for event handlers
3. **Performance**: Minimal overhead, optimized for high-frequency events
4. **Thread Safety**: Safe concurrent access from multiple threads
5. **Debuggability**: Event history and diagnostic tools

---

## Core Architecture

### Class Hierarchy

```
EventBus (Singleton)
├── EditorEvent (base class for all events)
│   ├── SelectionChangedEvent
│   ├── PropertyChangedEvent
│   ├── GraphNodeAddedEvent
│   └── ... (80+ event types)
├── EventSubscription (handle for managing subscriptions)
└── ScopedEventSubscription (RAII wrapper)

QtEventBus (Qt wrapper)
├── Qt signals/slots integration
└── Re-entrance protection
```

### EventBus Class

**Location**: `editor/include/NovelMind/editor/event_bus.hpp` and `editor/src/event_bus.cpp`

```cpp
class EventBus {
public:
    // Singleton access
    static EventBus& instance();

    // Publishing events
    void publish(const EditorEvent& event);
    void publish(std::unique_ptr<EditorEvent> event);
    void queueEvent(std::unique_ptr<EditorEvent> event);
    void processQueuedEvents();

    // Subscribing to events
    EventSubscription subscribe(EventHandler handler);
    EventSubscription subscribe(EditorEventType type, EventHandler handler);
    EventSubscription subscribe(EventFilter filter, EventHandler handler);
    template <typename T>
    EventSubscription subscribe(TypedEventHandler<T> handler);

    // Unsubscribing
    void unsubscribe(const EventSubscription& subscription);
    void unsubscribeAll(EditorEventType type);
    void unsubscribeAll();

    // Configuration
    void setSynchronous(bool sync);
    void setHistoryEnabled(bool enabled);
    const std::vector<std::unique_ptr<EditorEvent>>& getEventHistory() const;

private:
    EventBus();  // Private constructor (singleton)
    ~EventBus();

    std::vector<SubscriberInfo> m_subscribers;
    std::queue<std::unique_ptr<EditorEvent>> m_eventQueue;
    std::vector<std::unique_ptr<EditorEvent>> m_eventHistory;
    std::mutex m_mutex;
    std::atomic<int> m_dispatchDepth{0};
    std::vector<std::function<void()>> m_pendingOperations;
    bool m_synchronous{true};
    bool m_historyEnabled{false};
};
```

### EditorEvent Base Class

All events inherit from `EditorEvent`:

```cpp
class EditorEvent {
public:
    EditorEventType type;
    std::string source;  // Component that generated the event
    std::chrono::steady_clock::time_point timestamp;

    virtual ~EditorEvent() = default;

protected:
    EditorEvent(EditorEventType type, const std::string& source = "");
};
```

### EventSubscription Handle

Subscription handles use RAII for automatic cleanup:

```cpp
class EventSubscription {
public:
    EventSubscription() = default;
    EventSubscription(uint64_t id);

    uint64_t id() const { return m_id; }
    bool isValid() const { return m_id != 0; }

private:
    uint64_t m_id{0};
};

// RAII wrapper
class ScopedEventSubscription {
public:
    ScopedEventSubscription(EventBus* bus, EventSubscription sub);
    ~ScopedEventSubscription();

    ScopedEventSubscription(const ScopedEventSubscription&) = delete;
    ScopedEventSubscription& operator=(const ScopedEventSubscription&) = delete;

private:
    EventBus* m_bus;
    EventSubscription m_subscription;
};
```

---

## Event Type System

The EventBus supports **80+ event types** organized into categories.

### Event Type Enumeration

**Location**: `editor/include/NovelMind/editor/event_bus.hpp:48-120`

```cpp
enum class EditorEventType {
    // Selection Events (2)
    SelectionChanged = 0,
    PrimarySelectionChanged,

    // Property Events (3)
    PropertyChanged,
    PropertyChangeStarted,
    PropertyChangeEnded,

    // Graph Events (6)
    GraphNodeAdded,
    GraphNodeRemoved,
    GraphNodeMoved,
    GraphConnectionAdded,
    GraphConnectionRemoved,
    GraphValidationChanged,

    // Timeline Events (6)
    TimelineKeyframeAdded,
    TimelineKeyframeRemoved,
    TimelineKeyframeMoved,
    TimelineTrackAdded,
    TimelineTrackRemoved,
    TimelinePlaybackChanged,

    // Scene Events (5)
    SceneObjectAdded,
    SceneObjectRemoved,
    SceneObjectMoved,
    SceneObjectTransformed,
    SceneLayerChanged,

    // Project Events (5)
    ProjectCreated,
    ProjectOpened,
    ProjectClosed,
    ProjectSaved,
    ProjectModified,

    // Undo/Redo Events (3)
    UndoPerformed,
    RedoPerformed,
    UndoStackChanged,

    // Play Mode Events (5)
    PlayModeStarted,
    PlayModePaused,
    PlayModeResumed,
    PlayModeStopped,
    PlayModeFrameAdvanced,

    // Asset Events (5)
    AssetImported,
    AssetDeleted,
    AssetRenamed,
    AssetMoved,
    AssetModified,

    // Error/Diagnostic Events (4)
    ErrorOccurred,
    WarningOccurred,
    DiagnosticAdded,
    DiagnosticCleared,

    // UI Events (3)
    PanelFocusChanged,
    LayoutChanged,
    ThemeChanged,

    // Custom Events (extensible)
    CustomEventBase = 1000
};
```

### Domain-Specific Events

**Location**: `editor/include/NovelMind/editor/events/panel_events.hpp`

Additional **40+ specialized events** including:

- **Scene Events**: SceneObjectSelectedEvent, SceneObjectPositionChangedEvent
- **Story Graph Events**: StoryGraphNodeSelectedEvent, SceneNodeDoubleClickedEvent
- **Voice/Audio Events**: VoiceClipAssignRequestedEvent, VoiceRecordingRequestedEvent
- **Navigation Events**: NavigationRequestedEvent, NavigateToScriptDefinitionEvent
- **Scene Registry Events**: SceneCreatedEvent, SceneRenamedEvent, SceneThumbnailUpdatedEvent

### Common Event Types

#### SelectionChangedEvent

```cpp
struct SelectionChangedEvent : public EditorEvent {
    std::vector<std::string> selectedIds;
    std::string selectionType;  // "scene_object", "graph_node", etc.

    SelectionChangedEvent() : EditorEvent(EditorEventType::SelectionChanged) {}
};
```

#### PropertyChangedEvent

```cpp
struct PropertyChangedEvent : public EditorEvent {
    std::string objectId;
    std::string propertyName;
    std::any oldValue;
    std::any newValue;

    PropertyChangedEvent() : EditorEvent(EditorEventType::PropertyChanged) {}
};
```

#### GraphNodeAddedEvent

```cpp
struct GraphNodeAddedEvent : public EditorEvent {
    std::string nodeId;
    std::string nodeType;
    QPointF position;

    GraphNodeAddedEvent() : EditorEvent(EditorEventType::GraphNodeAdded) {}
};
```

---

## Event Flow Patterns

### Pattern 1: Direct Event Flow (Panel to Panel)

```
┌─────────────┐                                    ┌─────────────┐
│  Panel A    │                                    │  Panel B    │
│             │                                    │             │
│ User Action │                                    │             │
└─────┬───────┘                                    └─────▲───────┘
      │                                                  │
      │ publish(event)                                   │
      │                                                  │
      └──────────────►┌──────────────┐                  │
                      │  EventBus    │                  │
                      │  (Singleton) │                  │
                      └──────────────┘                  │
                             │                          │
                             │ notify subscribers       │
                             └──────────────────────────┘
```

**Example**: User selects object in SceneView → Inspector updates properties

### Pattern 2: Mediator Pattern (Coordinated Flow)

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Panel A    │     │  Panel B    │     │  Panel C    │
└──────┬──────┘     └──────┬──────┘     └──────▲──────┘
       │                   │                    │
       │ publish(event1)   │                    │
       │                   │                    │
       └───────────────────┼────────────────────┤
                           │                    │
                 ┌─────────▼────────┐           │
                 │   EventBus       │           │
                 └─────────┬────────┘           │
                           │                    │
                 ┌─────────▼────────┐           │
                 │  Mediator        │           │
                 │  (Subscriber)    │           │
                 │  - Coordinates   │           │
                 │  - Transforms    │           │
                 │  - Validates     │           │
                 └─────────┬────────┘           │
                           │                    │
                           │ publish(event2)    │
                           │                    │
                           └────────────────────┘
```

**Example**: PropertyMediator subscribes to PropertyChanged events, applies changes to backend, and publishes completion events

### Pattern 3: Qt Signal Bridge

```
┌─────────────────┐
│  Qt Widget      │
│  (Qt Signals)   │
└────────┬────────┘
         │ Qt signal
         ▼
┌─────────────────┐
│  connect()      │
│  Lambda/Slot    │
└────────┬────────┘
         │
         │ publish(event)
         ▼
┌─────────────────┐
│   EventBus      │
└────────┬────────┘
         │ notify
         ▼
┌─────────────────┐
│  Subscribers    │
│  (EventBus)     │
└─────────────────┘
```

**Example**: Qt button clicked → Lambda converts to EventBus event → All subscribers notified

### Pattern 4: Debounced Events

```
Time: ─────────────────────────────────────────────►

Events:  E  E E  E    E E E E E         E
         │  │ │  │    │ │ │ │ │         │
         └──┴─┴──┴────┴─┴─┴─┴─┴─────────┘
                                         │
                          ┌──────────────▼───────────┐
                          │  Debounce Timer (300ms)  │
                          └──────────────┬───────────┘
                                         │
                              ┌──────────▼──────────┐
                              │ Final Event Fired   │
                              └─────────────────────┘
```

**Example**: SelectionMediator debounces expensive operations (inspector updates, scene loading) while allowing immediate lightweight updates (status bar)

---

## Subscriber Lifecycle

### Subscription Methods

#### 1. Subscribe to All Events

```cpp
EventSubscription sub = EventBus::instance().subscribe(
    [](const EditorEvent& event) {
        // Handle all events
        std::cout << "Event: " << (int)event.type << std::endl;
    }
);
```

#### 2. Subscribe by Event Type

```cpp
EventSubscription sub = EventBus::instance().subscribe(
    EditorEventType::SelectionChanged,
    [](const EditorEvent& event) {
        // Handle only SelectionChanged events
    }
);
```

#### 3. Subscribe with Custom Filter

```cpp
EventSubscription sub = EventBus::instance().subscribe(
    [](const EditorEvent& event) {
        return event.source == "SceneView" &&
               event.type == EditorEventType::SelectionChanged;
    },
    [](const EditorEvent& event) {
        // Handle filtered events
    }
);
```

#### 4. Type-Safe Subscription (Recommended)

```cpp
EventSubscription sub = EventBus::instance().subscribe<SelectionChangedEvent>(
    [](const SelectionChangedEvent& event) {
        // Automatically casts to correct type
        for (const auto& id : event.selectedIds) {
            std::cout << "Selected: " << id << std::endl;
        }
    }
);
```

### Unsubscription

#### Manual Unsubscription

```cpp
EventSubscription sub = EventBus::instance().subscribe(...);

// Later...
EventBus::instance().unsubscribe(sub);
```

#### RAII Unsubscription (Recommended)

```cpp
{
    ScopedEventSubscription scoped(&EventBus::instance(), subscription);
    // Automatically unsubscribes when scope exits
}

// Or using convenience macro
auto sub = NM_SUBSCRIBE_EVENT(EventBus::instance(), SelectionChanged, handler);
```

#### Unsubscribe All by Type

```cpp
// Remove all handlers for a specific event type
EventBus::instance().unsubscribeAll(EditorEventType::SelectionChanged);

// Remove ALL handlers
EventBus::instance().unsubscribeAll();
```

### Lifecycle in Mediators

**Typical mediator pattern**:

```cpp
class SelectionMediator {
public:
    void initialize() {
        auto& bus = EventBus::instance();

        // Subscribe to multiple events
        m_subscriptions.push_back(
            bus.subscribe<events::SceneObjectSelectedEvent>(
                [this](const auto& e) { onSceneObjectSelected(e); }
            )
        );

        m_subscriptions.push_back(
            bus.subscribe<events::StoryGraphNodeSelectedEvent>(
                [this](const auto& e) { onGraphNodeSelected(e); }
            )
        );
    }

    void shutdown() {
        // Unsubscribe all
        for (auto& sub : m_subscriptions) {
            EventBus::instance().unsubscribe(sub);
        }
        m_subscriptions.clear();
    }

private:
    std::vector<EventSubscription> m_subscriptions;
};
```

---

## Thread Safety Guarantees

### Thread Safety Features

The EventBus provides the following thread safety guarantees:

1. **Concurrent Dispatch**: Multiple threads can call `publish()` simultaneously
2. **Atomic Dispatch Depth**: Uses `std::atomic<int>` for tracking nested dispatch
3. **Mutex-Protected Modifications**: All subscription changes are mutex-protected
4. **Copy-Before-Dispatch**: Subscriber list is copied before iteration to allow safe modifications during dispatch

### Implementation Details

#### Dispatch Depth Tracking

```cpp
std::atomic<int> m_dispatchDepth{0};

void EventBus::publish(const EditorEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_dispatchDepth++;  // Atomic increment

    // Copy subscriber list to allow modifications during dispatch
    auto subscribers = m_subscribers;

    for (const auto& subscriber : subscribers) {
        if (subscriber.filter(event)) {
            subscriber.handler(event);
        }
    }

    m_dispatchDepth--;  // Atomic decrement

    // Process pending operations after dispatch completes
    if (m_dispatchDepth == 0 && !m_pendingOperations.empty()) {
        for (auto& op : m_pendingOperations) {
            op();
        }
        m_pendingOperations.clear();
    }
}
```

#### Deferred Subscription Modifications

When a handler subscribes or unsubscribes during dispatch, the operation is deferred:

```cpp
void EventBus::subscribe(...) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_dispatchDepth > 0) {
        // Defer subscription until dispatch completes
        m_pendingOperations.push_back([this, handler]() {
            m_subscribers.push_back({...});
        });
    } else {
        // Apply immediately
        m_subscribers.push_back({...});
    }
}
```

### Thread Safety Best Practices

**DO**:
- ✅ Call `publish()` from any thread
- ✅ Subscribe/unsubscribe from any thread
- ✅ Modify subscriptions during event handling
- ✅ Publish events from within event handlers (nested dispatch)

**DON'T**:
- ❌ Hold locks while publishing events (deadlock risk)
- ❌ Assume event order across threads
- ❌ Store raw pointers to event data (copy instead)

---

## Qt Integration (QtEventBus)

**Location**: `editor/include/NovelMind/editor/qt/qt_event_bus.hpp`

### Overview

`QtEventBus` wraps the core EventBus with Qt signals/slots for seamless integration with Qt widgets.

### Features

- **Qt Signals**: Qt-compatible signals for all major event types
- **Re-entrance Protection**: Prevents infinite loops using `thread_local` flag
- **Convenience Methods**: Simplified publishing for common events
- **Automatic Bridging**: Converts EventBus events to Qt signals

### Class Structure

```cpp
class QtEventBus : public QObject {
    Q_OBJECT
public:
    static QtEventBus& instance();

    // Convenience publishing methods
    void publishSelectionChanged(const QStringList& ids, const QString& type);
    void publishPropertyChanged(const QString& objectId,
                                const QString& propertyName,
                                const QVariant& oldValue,
                                const QVariant& newValue);
    void publishGraphNodeAdded(const QString& nodeId, const QString& nodeType);

signals:
    void selectionChanged(const QStringList& selectedIds, const QString& type);
    void propertyChanged(const QString& objectId, const QString& propertyName,
                        const QVariant& oldValue, const QVariant& newValue);
    void graphNodeAdded(const QString& nodeId, const QString& nodeType);
    void projectOpened(const QString& projectPath);
    void playModeChanged(bool playing, bool paused);
    // ... and more

private:
    QtEventBus();
    void bridgeToQtSignals();
};
```

### Usage Example

```cpp
// Publishing from Qt code
void MyPanel::onButtonClicked() {
    QtEventBus::instance().publishSelectionChanged(
        QStringList{"obj1", "obj2"},
        "scene_object"
    );
}

// Subscribing from Qt code
connect(&QtEventBus::instance(), &QtEventBus::selectionChanged,
        this, [this](const QStringList& ids, const QString& type) {
            updateSelection(ids);
        });
```

### Re-entrance Protection

```cpp
// Prevents infinite loops when Qt signals trigger EventBus events
// that trigger Qt signals again
thread_local bool g_inQtEventBusEmit = false;

void QtEventBus::emitSignalFromEvent(const EditorEvent& event) {
    if (g_inQtEventBusEmit) {
        return;  // Prevent re-entrance
    }

    g_inQtEventBusEmit = true;
    emit someQtSignal(data);
    g_inQtEventBusEmit = false;
}
```

---

## Common Usage Patterns

### Pattern 1: Mediator Coordination

**Scenario**: SelectionMediator coordinates selection between multiple panels

```cpp
class SelectionMediator {
public:
    void initialize() {
        auto& bus = EventBus::instance();

        // Subscribe to selection events from all panels
        m_subscriptions.push_back(
            bus.subscribe<events::SceneObjectSelectedEvent>(
                [this](const events::SceneObjectSelectedEvent& e) {
                    onSceneObjectSelected(e);
                }
            )
        );

        m_subscriptions.push_back(
            bus.subscribe<events::StoryGraphNodeSelectedEvent>(
                [this](const events::StoryGraphNodeSelectedEvent& e) {
                    onGraphNodeSelected(e);
                }
            )
        );

        // Bridge Qt signals to EventBus
        if (m_storyGraph) {
            connect(m_storyGraph, &NMStoryGraphPanel::nodeSelected,
                    this, [this](const QString& nodeId) {
                        events::StoryGraphNodeSelectedEvent event;
                        event.nodeIdString = nodeId;
                        EventBus::instance().publish(event);
                    });
        }
    }

private:
    void onSceneObjectSelected(const events::SceneObjectSelectedEvent& e) {
        // Update status bar immediately (lightweight)
        updateStatusBar(e.objectId);

        // Debounce expensive operations
        m_inspectorUpdateTimer.start(300);  // 300ms debounce
    }

    std::vector<EventSubscription> m_subscriptions;
    QTimer m_inspectorUpdateTimer;
};
```

### Pattern 2: Property Mediator

**Scenario**: PropertyMediator applies property changes to backend

```cpp
class PropertyMediator {
public:
    void initialize() {
        auto& bus = EventBus::instance();

        m_subscriptions.push_back(
            bus.subscribe<events::InspectorPropertyChangedEvent>(
                [this](const events::InspectorPropertyChangedEvent& e) {
                    // Apply to backend
                    applyPropertyToSceneObject(e.objectId,
                                              e.propertyName,
                                              e.newValue);

                    // Create undo command
                    auto cmd = new PropertyChangeCommand(
                        e.objectId, e.propertyName, e.oldValue, e.newValue
                    );
                    NMUndoManager::instance().pushCommand(cmd);

                    // Notify dependents
                    events::PropertyChangedEvent notification;
                    notification.objectId = e.objectId;
                    notification.propertyName = e.propertyName;
                    bus.publish(notification);
                }
            )
        );
    }
};
```

### Pattern 3: Scene Registry Auto-Sync

**Scenario**: Story Graph automatically updates when scenes change (Issue #213)

```cpp
class NMStoryGraphPanel {
public:
    void onInitialize() {
        auto& bus = EventBus::instance();

        // Subscribe to scene registry events
        m_eventSubscriptions.push_back(
            bus.subscribe<events::SceneThumbnailUpdatedEvent>(
                [this](const events::SceneThumbnailUpdatedEvent& e) {
                    onSceneThumbnailUpdated(e.sceneId, e.thumbnailPath);
                }
            )
        );

        m_eventSubscriptions.push_back(
            bus.subscribe<events::SceneRenamedEvent>(
                [this](const events::SceneRenamedEvent& e) {
                    onSceneRenamed(e.sceneId, e.newName);
                }
            )
        );

        m_eventSubscriptions.push_back(
            bus.subscribe<events::SceneDeletedEvent>(
                [this](const events::SceneDeletedEvent& e) {
                    onSceneDeleted(e.sceneId);
                }
            )
        );
    }

private:
    void onSceneRenamed(const std::string& sceneId, const std::string& newName) {
        // Update all nodes referencing this scene
        updateAllNodesWithScene(sceneId, newName);
    }

    std::vector<EventSubscription> m_eventSubscriptions;
};
```

---

## Best Practices

### DO ✅

1. **Use Type-Safe Subscriptions**
   ```cpp
   // Good: Type-safe
   bus.subscribe<SelectionChangedEvent>([](const SelectionChangedEvent& e) {
       // Compiler ensures correct type
   });
   ```

2. **Use RAII for Subscription Management**
   ```cpp
   // Good: Automatic cleanup
   ScopedEventSubscription scoped(&bus, subscription);
   ```

3. **Store Subscriptions in Member Variables**
   ```cpp
   class MyPanel {
       std::vector<EventSubscription> m_subscriptions;
   };
   ```

4. **Use Debouncing for Expensive Operations**
   ```cpp
   // Debounce inspector updates to reduce load
   m_inspectorUpdateTimer.start(300);  // 300ms
   ```

5. **Set Meaningful Event Sources**
   ```cpp
   event.source = "SceneView";  // Helps with debugging
   ```

6. **Use Mediators for Complex Coordination**
   ```cpp
   // Mediator handles cross-panel logic
   SelectionMediator mediates between panels
   ```

### DON'T ❌

1. **Don't Create Circular Event Dependencies**
   ```cpp
   // Bad: Panel A publishes → Panel B publishes → Panel A publishes → ...
   // Use mediator to break cycles
   ```

2. **Don't Hold Locks While Publishing**
   ```cpp
   // Bad: Deadlock risk
   std::lock_guard<std::mutex> lock(m_mutex);
   bus.publish(event);  // ❌ Other handlers might need the lock
   ```

3. **Don't Store Raw Pointers to Event Data**
   ```cpp
   // Bad: Dangling pointer after event dispatch
   const EditorEvent* storedEvent = &event;  // ❌

   // Good: Copy the data
   std::string storedId = event.objectId;  // ✅
   ```

4. **Don't Publish Events for Every Keystroke**
   ```cpp
   // Bad: Too many events
   void onTextChanged(const QString& text) {
       bus.publish(TextChangedEvent{text});  // ❌ High frequency
   }

   // Good: Debounce
   m_textChangeTimer.start(500);  // ✅ Wait for typing pause
   ```

5. **Don't Forget to Unsubscribe**
   ```cpp
   // Bad: Memory leak
   bus.subscribe(...);  // ❌ No cleanup

   // Good: Store and unsubscribe
   m_subscription = bus.subscribe(...);  // ✅
   // In destructor: bus.unsubscribe(m_subscription);
   ```

### Naming Conventions

- **Event Names**: Use past tense for completed actions (`SelectionChanged`, `PropertyModified`)
- **Event Structs**: Suffix with `Event` (`SelectionChangedEvent`)
- **Handlers**: Prefix with `on` (`onSelectionChanged`)
- **Sources**: Use component name (`"SceneView"`, `"StoryGraph"`)

---

## Performance Considerations

### Optimization Techniques (Issue #468)

The EventBus is optimized for high-frequency event dispatch:

1. **No Allocation During Dispatch**: Subscriber list is copied once before iteration
2. **Deferred Modifications**: Subscribe/unsubscribe during dispatch are queued
3. **Atomic Dispatch Depth**: Lock-free tracking of nested dispatch
4. **Minimal Locking**: Mutex only held during critical sections

### Performance Targets

- **Dispatch time**: < 5µs per subscriber for simple events
- **Throughput**: 100,000 handler invocations in < 500ms
- **Memory**: No heap allocations during dispatch (after warm-up)

### Benchmarking

**Test**: `tests/unit/test_event_bus.cpp`

```cpp
TEST_CASE("Dispatch performance with 100 subscribers") {
    EventBus bus;

    // Subscribe 100 handlers
    for (int i = 0; i < 100; i++) {
        bus.subscribe([](const EditorEvent&) {
            // Minimal work
        });
    }

    // Dispatch 1000 events
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        TestEvent event;
        bus.publish(event);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    ).count();

    REQUIRE(duration < 500);  // 100k handler calls in < 500ms
}
```

### When to Use Asynchronous Mode

```cpp
// For low-priority events, use queued dispatch
bus.setSynchronous(false);
bus.queueEvent(std::make_unique<LowPriorityEvent>());

// Process queue when ready
bus.processQueuedEvents();
```

**Use cases for async**:
- Analytics/telemetry events
- Background logging
- Non-critical notifications
- Batch processing

---

## Debugging and Troubleshooting

### Event History

Enable event history for debugging:

```cpp
EventBus::instance().setHistoryEnabled(true);

// ... perform operations ...

// Inspect history
const auto& history = EventBus::instance().getEventHistory();
for (const auto& event : history) {
    std::cout << "Event: " << (int)event->type
              << " from " << event->source
              << " at " << formatTimestamp(event->timestamp)
              << std::endl;
}
```

**Note**: History is bounded to last 100 events to prevent memory growth.

### Common Issues

#### Issue: Event Not Received

**Symptoms**: Subscriber not called when event published

**Debugging**:
1. Check event type matches:
   ```cpp
   // Ensure types match exactly
   bus.subscribe(EditorEventType::SelectionChanged, ...);
   bus.publish(SelectionChangedEvent{});  // type = SelectionChanged
   ```

2. Check filter logic:
   ```cpp
   // Filter might be too restrictive
   bus.subscribe([](const EditorEvent& e) {
       return e.source == "SceneView";  // Only matches this source
   }, handler);
   ```

3. Verify subscription is active:
   ```cpp
   auto sub = bus.subscribe(...);
   REQUIRE(sub.isValid());  // Check subscription created successfully
   ```

#### Issue: Circular Event Loop

**Symptoms**: Stack overflow, infinite recursion

**Solution**: Use mediator to break cycles:

```cpp
// Bad: Direct cycle
Panel A publishes → Panel B handles & publishes → Panel A handles & publishes → ...

// Good: Mediator breaks cycle
Panel A publishes → Mediator handles & coordinates → Panel B updates (no publish)
```

#### Issue: Performance Degradation

**Symptoms**: Slow UI updates, high CPU usage

**Solutions**:
1. Enable debouncing:
   ```cpp
   QTimer debounceTimer;
   debounceTimer.setSingleShot(true);
   debounceTimer.setInterval(300);
   ```

2. Profile handler execution:
   ```cpp
   bus.subscribe<ExpensiveEvent>([](const auto& e) {
       auto start = std::chrono::high_resolution_clock::now();
       // ... handler logic ...
       auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
           std::chrono::high_resolution_clock::now() - start
       ).count();
       if (duration > 1000) {
           qWarning() << "Slow handler:" << duration << "µs";
       }
   });
   ```

3. Reduce subscription count:
   ```cpp
   // Instead of subscribing to every event, filter efficiently
   bus.subscribe<SelectionChangedEvent>(...);  // Specific type
   // Not: bus.subscribe([](const EditorEvent&) {...});  // All events
   ```

---

## Event Flow Diagrams

### Diagram 1: Basic Event Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    Event Publishing Flow                      │
└──────────────────────────────────────────────────────────────┘

   Publisher                EventBus                 Subscribers
       │                        │                          │
       │  publish(event)        │                          │
       ├───────────────────────►│                          │
       │                        │                          │
       │                        │ Lock mutex               │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │                        │                          │
       │                        │ Increment dispatch depth │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │                        │                          │
       │                        │ Copy subscriber list     │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │                        │                          │
       │                        │ For each subscriber:     │
       │                        │   - Check filter         │
       │                        │   - Call handler         │
       │                        ├─────────────────────────►│
       │                        │                          │ handler(event)
       │                        │                          ├──────────┐
       │                        │                          │          │
       │                        │                          │◄─────────┘
       │                        │                          │
       │                        │ Decrement dispatch depth │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │                        │                          │
       │                        │ Process pending ops      │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │                        │                          │
       │                        │ Unlock mutex             │
       │                        ├─────────┐                │
       │                        │         │                │
       │                        │◄────────┘                │
       │◄───────────────────────┤                          │
       │  return                │                          │
       │                        │                          │
```

### Diagram 2: Subscription Lifecycle

```
┌──────────────────────────────────────────────────────────────┐
│                  Subscription Lifecycle                       │
└──────────────────────────────────────────────────────────────┘

┌─────────────┐
│   Created   │
│ (unsubscr.) │
└──────┬──────┘
       │
       │ subscribe(handler)
       │
       ▼
┌─────────────┐
│   Active    │────────────┐
│ (receiving) │            │ event published
└──────┬──────┘            │
       │                   ▼
       │            ┌─────────────┐
       │            │   Handler   │
       │            │  Executed   │
       │            └─────────────┘
       │
       │ unsubscribe() or
       │ ScopedEventSubscription destroyed
       │
       ▼
┌─────────────┐
│   Inactive  │
│  (cleanup)  │
└─────────────┘
```

### Diagram 3: Mediator Pattern Flow

```
┌──────────────────────────────────────────────────────────────┐
│                   Mediator Coordination                       │
└──────────────────────────────────────────────────────────────┘

SceneView          EventBus         Mediator        Inspector
    │                  │                │                │
    │ User selects     │                │                │
    │ object           │                │                │
    ├─────────┐        │                │                │
    │         │        │                │                │
    │◄────────┘        │                │                │
    │                  │                │                │
    │ publish(         │                │                │
    │  SelectionEvent) │                │                │
    ├─────────────────►│                │                │
    │                  │                │                │
    │                  │ notify         │                │
    │                  ├───────────────►│                │
    │                  │                │ onSelection()  │
    │                  │                ├──────────┐     │
    │                  │                │          │     │
    │                  │                │ - Validate     │
    │                  │                │ - Transform    │
    │                  │                │ - Debounce     │
    │                  │                │          │     │
    │                  │                │◄─────────┘     │
    │                  │                │                │
    │                  │                │ publish(       │
    │                  │                │  UpdateEvent)  │
    │                  │◄───────────────┤                │
    │                  │                │                │
    │                  │ notify         │                │
    │                  ├────────────────────────────────►│
    │                  │                │                │
    │                  │                │                │ update UI
    │                  │                │                ├──────┐
    │                  │                │                │      │
    │                  │                │                │◄─────┘
```

---

## Summary

The EventBus system provides a robust, thread-safe, and high-performance foundation for editor component communication. Key takeaways:

1. **Use type-safe subscriptions** (`subscribe<T>()`) for compile-time safety
2. **Leverage mediators** for complex cross-panel coordination
3. **Apply debouncing** for high-frequency or expensive operations
4. **Enable RAII** (`ScopedEventSubscription`) for automatic cleanup
5. **Trust thread safety** - publish and subscribe from any thread safely
6. **Profile performance** if handling thousands of events per second
7. **Use event history** for debugging event flow issues

For implementation examples, see:
- **Tests**: `tests/unit/test_event_bus.cpp`
- **Mediators**: `editor/src/mediators/`
- **Panels**: `editor/src/qt/panels/`

For related documentation:
- [gui_architecture.md](gui_architecture.md) - Overall GUI architecture
- [SCENE_STORY_GRAPH_PIPELINE.md](SCENE_STORY_GRAPH_PIPELINE.md) - Scene/graph integration

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Related Issues**: #527 (Documentation), #468 (Performance), #569 (Thread Safety), #213 (Scene Registry)
