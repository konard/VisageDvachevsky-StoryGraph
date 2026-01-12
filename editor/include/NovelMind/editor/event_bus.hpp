#pragma once

/**
 * @file event_bus.hpp
 * @brief Editor Event Bus / Messaging System for NovelMind
 *
 * Provides a centralized event/message system for communication between
 * editor components. This is critical for v0.2.0 GUI as it enables:
 * - Loose coupling between panels and systems
 * - Real-time updates when state changes
 * - Undo/Redo notification propagation
 * - Play mode state synchronization
 *
 * Supported event types:
 * - SelectionChanged: When selection changes in any panel
 * - PropertyChanged: When a property is modified
 * - GraphChanged: When story graph is modified
 * - TimelineChanged: When timeline is modified
 * - ProjectOpened/Closed: Project lifecycle events
 * - UndoRedoPerformed: When undo/redo happens
 * - PlayModeStarted/Stopped: Play mode state changes
 * - AssetImported/Deleted/Renamed: Asset operations
 * - ErrorOccurred: Error reporting
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <any>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class EventBus;

/**
 * @brief Event type enumeration for quick filtering
 */
enum class EditorEventType : u32 {
  // Selection events
  SelectionChanged = 0,
  PrimarySelectionChanged,

  // Property events
  PropertyChanged,
  PropertyChangeStarted,
  PropertyChangeEnded,

  // Graph events
  GraphNodeAdded,
  GraphNodeRemoved,
  GraphNodeMoved,
  GraphConnectionAdded,
  GraphConnectionRemoved,
  GraphValidationChanged,

  // Timeline events
  TimelineKeyframeAdded,
  TimelineKeyframeRemoved,
  TimelineKeyframeMoved,
  TimelineTrackAdded,
  TimelineTrackRemoved,
  TimelinePlaybackChanged,

  // Scene events
  SceneObjectAdded,
  SceneObjectRemoved,
  SceneObjectMoved,
  SceneObjectTransformed,
  SceneLayerChanged,

  // Project events
  ProjectCreated,
  ProjectOpened,
  ProjectClosed,
  ProjectSaved,
  ProjectModified,

  // Undo/Redo events
  UndoPerformed,
  RedoPerformed,
  UndoStackChanged,

  // Play mode events
  PlayModeStarted,
  PlayModePaused,
  PlayModeResumed,
  PlayModeStopped,
  PlayModeFrameAdvanced,

  // Asset events
  AssetImported,
  AssetDeleted,
  AssetRenamed,
  AssetMoved,
  AssetModified,

  // Error events
  ErrorOccurred,
  WarningOccurred,
  DiagnosticAdded,
  DiagnosticCleared,

  // UI events
  PanelFocusChanged,
  LayoutChanged,
  ThemeChanged,

  // Custom event marker
  Custom = 1000
};

/**
 * @brief Base class for all editor events
 */
struct EditorEvent {
  EditorEventType type;
  u64 timestamp = 0;
  std::string source; // Source component that generated the event

  explicit EditorEvent(EditorEventType t) : type(t) {
    timestamp = static_cast<u64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
  }

  virtual ~EditorEvent() = default;

  [[nodiscard]] virtual std::string getDescription() const {
    return "EditorEvent";
  }

  /**
   * @brief Generate a key for event deduplication
   *
   * Returns a string key that uniquely identifies this event for deduplication
   * purposes. Events with the same key within the deduplication window will be
   * considered duplicates. Override this in derived classes to customize
   * deduplication behavior.
   *
   * @return String key representing the event's identity
   */
  [[nodiscard]] virtual std::string getEventKey() const {
    // Default: deduplicate only by event type
    return std::to_string(static_cast<u32>(type));
  }
};

// ============================================================================
// Selection Events
// ============================================================================

struct SelectionChangedEvent : EditorEvent {
  std::vector<std::string> selectedIds;
  std::string selectionType; // "SceneObject", "GraphNode", "TimelineItem", etc.

  SelectionChangedEvent() : EditorEvent(EditorEventType::SelectionChanged) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Selection changed: " + std::to_string(selectedIds.size()) +
           " items";
  }
};

// ============================================================================
// Property Events
// ============================================================================

struct PropertyChangedEvent : EditorEvent {
  std::string objectId;
  std::string propertyName;
  std::string oldValue;
  std::string newValue;

  PropertyChangedEvent() : EditorEvent(EditorEventType::PropertyChanged) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Property '" + propertyName + "' changed on " + objectId;
  }
};

struct PropertyChangeStartedEvent : EditorEvent {
  std::string objectId;
  std::string propertyName;

  PropertyChangeStartedEvent()
      : EditorEvent(EditorEventType::PropertyChangeStarted) {}
};

struct PropertyChangeEndedEvent : EditorEvent {
  std::string objectId;
  std::string propertyName;

  PropertyChangeEndedEvent()
      : EditorEvent(EditorEventType::PropertyChangeEnded) {}
};

// ============================================================================
// Graph Events
// ============================================================================

struct GraphNodeAddedEvent : EditorEvent {
  scripting::NodeId nodeId = 0;
  scripting::IRNodeType nodeType = scripting::IRNodeType::Dialogue;
  f32 x = 0.0f;
  f32 y = 0.0f;

  GraphNodeAddedEvent() : EditorEvent(EditorEventType::GraphNodeAdded) {}

  [[nodiscard]] std::string getDescription() const override {
    return "Node " + std::to_string(nodeId) + " added";
  }
};

struct GraphNodeRemovedEvent : EditorEvent {
  scripting::NodeId nodeId = 0;

  GraphNodeRemovedEvent() : EditorEvent(EditorEventType::GraphNodeRemoved) {}
};

struct GraphNodeMovedEvent : EditorEvent {
  std::vector<scripting::NodeId> nodeIds;
  f32 deltaX = 0.0f;
  f32 deltaY = 0.0f;

  GraphNodeMovedEvent() : EditorEvent(EditorEventType::GraphNodeMoved) {}
};

struct GraphConnectionAddedEvent : EditorEvent {
  scripting::NodeId fromNode = 0;
  std::string fromPort;
  scripting::NodeId toNode = 0;
  std::string toPort;

  GraphConnectionAddedEvent()
      : EditorEvent(EditorEventType::GraphConnectionAdded) {}
};

struct GraphConnectionRemovedEvent : EditorEvent {
  scripting::NodeId fromNode = 0;
  std::string fromPort;
  scripting::NodeId toNode = 0;
  std::string toPort;

  GraphConnectionRemovedEvent()
      : EditorEvent(EditorEventType::GraphConnectionRemoved) {}
};

struct GraphValidationChangedEvent : EditorEvent {
  bool isValid = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  GraphValidationChangedEvent()
      : EditorEvent(EditorEventType::GraphValidationChanged) {}
};

// ============================================================================
// Timeline Events
// ============================================================================

struct TimelineKeyframeEvent : EditorEvent {
  std::string trackId;
  u64 keyframeIndex = 0;
  f64 time = 0.0;

  explicit TimelineKeyframeEvent(EditorEventType eventType)
      : EditorEvent(eventType) {}
};

struct TimelineTrackEvent : EditorEvent {
  std::string trackId;
  std::string trackType;

  explicit TimelineTrackEvent(EditorEventType eventType)
      : EditorEvent(eventType) {}
};

struct TimelinePlaybackChangedEvent : EditorEvent {
  f64 currentTime = 0.0;
  bool isPlaying = false;
  bool isPaused = false;
  f64 playbackSpeed = 1.0;

  TimelinePlaybackChangedEvent()
      : EditorEvent(EditorEventType::TimelinePlaybackChanged) {}
};

// ============================================================================
// Scene Events
// ============================================================================

struct SceneObjectEvent : EditorEvent {
  std::string objectId;
  std::string objectType;

  explicit SceneObjectEvent(EditorEventType eventType)
      : EditorEvent(eventType) {}
};

struct SceneObjectTransformedEvent : EditorEvent {
  std::string objectId;
  f32 oldX = 0.0f, oldY = 0.0f;
  f32 newX = 0.0f, newY = 0.0f;
  f32 oldRotation = 0.0f, newRotation = 0.0f;
  f32 oldScaleX = 1.0f, oldScaleY = 1.0f;
  f32 newScaleX = 1.0f, newScaleY = 1.0f;

  SceneObjectTransformedEvent()
      : EditorEvent(EditorEventType::SceneObjectTransformed) {}
};

// ============================================================================
// Project Events
// ============================================================================

struct ProjectEvent : EditorEvent {
  std::string projectPath;
  std::string projectName;

  explicit ProjectEvent(EditorEventType eventType) : EditorEvent(eventType) {}
};

struct ProjectModifiedEvent : EditorEvent {
  bool hasUnsavedChanges = true;
  std::string modifiedComponent; // What was modified

  ProjectModifiedEvent() : EditorEvent(EditorEventType::ProjectModified) {}
};

// ============================================================================
// Undo/Redo Events
// ============================================================================

struct UndoRedoEvent : EditorEvent {
  std::string actionDescription;
  bool canUndo = false;
  bool canRedo = false;

  explicit UndoRedoEvent(EditorEventType eventType) : EditorEvent(eventType) {}
};

struct UndoStackChangedEvent : EditorEvent {
  bool canUndo = false;
  bool canRedo = false;
  std::string nextUndoDescription;
  std::string nextRedoDescription;

  UndoStackChangedEvent() : EditorEvent(EditorEventType::UndoStackChanged) {}
};

// ============================================================================
// Play Mode Events
// ============================================================================

struct PlayModeEvent : EditorEvent {
  std::string currentSceneId;
  scripting::NodeId currentNodeId = 0;
  f64 playTime = 0.0;

  explicit PlayModeEvent(EditorEventType eventType) : EditorEvent(eventType) {}
};

// ============================================================================
// Asset Events
// ============================================================================

struct AssetEvent : EditorEvent {
  std::string assetPath;
  std::string assetType;
  std::string assetId;

  explicit AssetEvent(EditorEventType eventType) : EditorEvent(eventType) {}
};

struct AssetRenamedEvent : EditorEvent {
  std::string oldPath;
  std::string newPath;

  AssetRenamedEvent() : EditorEvent(EditorEventType::AssetRenamed) {}
};

// ============================================================================
// Error Events
// ============================================================================

struct ErrorEvent : EditorEvent {
  enum class Severity : u8 { Info, Warning, Error, Fatal };

  std::string message;
  std::string details;
  Severity severity = Severity::Error;
  std::string location; // File:line or component name

  explicit ErrorEvent(EditorEventType eventType) : EditorEvent(eventType) {}
};

struct DiagnosticEvent : EditorEvent {
  struct Diagnostic {
    std::string message;
    std::string file;
    u32 line = 0;
    u32 column = 0;
    bool isError = false;
  };

  std::vector<Diagnostic> diagnostics;

  explicit DiagnosticEvent(EditorEventType eventType)
      : EditorEvent(eventType) {}
};

// ============================================================================
// UI Events
// ============================================================================

struct PanelFocusChangedEvent : EditorEvent {
  std::string panelName;
  bool hasFocus = false;

  PanelFocusChangedEvent() : EditorEvent(EditorEventType::PanelFocusChanged) {}
};

struct LayoutChangedEvent : EditorEvent {
  std::string layoutName;

  LayoutChangedEvent() : EditorEvent(EditorEventType::LayoutChanged) {}
};

struct ThemeChangedEvent : EditorEvent {
  std::string themeName;

  ThemeChangedEvent() : EditorEvent(EditorEventType::ThemeChanged) {}
};

// ============================================================================
// Event Handler Types
// ============================================================================

/**
 * @brief Type-erased event handler
 */
using EventHandler = std::function<void(const EditorEvent &)>;

/**
 * @brief Typed event handler
 */
template <typename T> using TypedEventHandler = std::function<void(const T &)>;

/**
 * @brief Event filter predicate
 */
using EventFilter = std::function<bool(const EditorEvent &)>;

/**
 * @brief Subscription handle for unsubscribing
 */
class EventSubscription {
public:
  EventSubscription() = default;
  explicit EventSubscription(u64 id) : m_id(id) {}

  [[nodiscard]] u64 getId() const { return m_id; }
  [[nodiscard]] bool isValid() const { return m_id != 0; }

  bool operator==(const EventSubscription &other) const {
    return m_id == other.m_id;
  }

private:
  u64 m_id = 0;
};

// ============================================================================
// Event Bus
// ============================================================================

/**
 * @brief Central event bus for editor communication
 *
 * Features:
 * - Type-safe event publishing and subscription
 * - Event filtering by type
 * - Priority-based handler ordering
 * - Deferred event processing
 * - Thread-safe (with optional locking)
 */
class EventBus {
public:
  EventBus();
  ~EventBus();

  // Prevent copying
  EventBus(const EventBus &) = delete;
  EventBus &operator=(const EventBus &) = delete;

  /**
   * @brief Get singleton instance
   */
  static EventBus &instance();

  // =========================================================================
  // Publishing
  // =========================================================================

  /**
   * @brief Publish an event immediately
   */
  void publish(const EditorEvent &event);

  /**
   * @brief Publish an event with move semantics
   */
  void publish(std::unique_ptr<EditorEvent> event);

  /**
   * @brief Queue an event for deferred processing
   */
  void queueEvent(std::unique_ptr<EditorEvent> event);

  /**
   * @brief Process all queued events
   */
  void processQueuedEvents();

  /**
   * @brief Convenience method to publish typed event
   * @note Named emitEvent instead of emit to avoid conflict with Qt's emit
   * macro
   */
  template <typename T, typename... Args>
  void emitEvent([[maybe_unused]] Args &&...args) {
    auto event = std::make_unique<T>();
    // Initialize event with args if needed (via aggregate init or setters)
    publish(std::move(event));
  }

  // =========================================================================
  // Subscription
  // =========================================================================

  /**
   * @brief Subscribe to all events
   */
  EventSubscription subscribe(EventHandler handler);

  /**
   * @brief Subscribe to events of a specific type
   */
  EventSubscription subscribe(EditorEventType type, EventHandler handler);

  /**
   * @brief Subscribe to events matching a filter
   */
  EventSubscription subscribe(EventFilter filter, EventHandler handler);

  /**
   * @brief Subscribe with typed handler
   */
  template <typename T>
  EventSubscription subscribe(TypedEventHandler<T> handler) {
    return subscribe([handler = std::move(handler)](const EditorEvent &event) {
      if (auto *typed = dynamic_cast<const T *>(&event)) {
        handler(*typed);
      }
    });
  }

  /**
   * @brief Unsubscribe using subscription handle
   */
  void unsubscribe(const EventSubscription &subscription);

  /**
   * @brief Unsubscribe all handlers for a specific event type
   */
  void unsubscribeAll(EditorEventType type);

  /**
   * @brief Unsubscribe all handlers
   */
  void unsubscribeAll();

  // =========================================================================
  // Event History (for debugging)
  // =========================================================================

  /**
   * @brief Enable/disable event history
   */
  void setHistoryEnabled(bool enabled);

  /**
   * @brief Get recent event history
   */
  [[nodiscard]] std::vector<std::string> getRecentEvents(size_t count) const;

  /**
   * @brief Clear event history
   */
  void clearHistory();

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Set whether to process events synchronously (default) or queue them
   */
  void setSynchronous(bool sync);

  /**
   * @brief Check if event processing is synchronous
   */
  [[nodiscard]] bool isSynchronous() const;

  /**
   * @brief Enable or disable event deduplication
   * @param enabled Whether to deduplicate identical events
   */
  void setDeduplicationEnabled(bool enabled);

  /**
   * @brief Check if event deduplication is enabled
   */
  [[nodiscard]] bool isDeduplicationEnabled() const;

  /**
   * @brief Set the deduplication time window in milliseconds
   *
   * Events with the same key within this time window will be deduplicated.
   * Default is 100ms.
   *
   * @param windowMs Time window in milliseconds
   */
  void setDeduplicationWindow(u64 windowMs);

  /**
   * @brief Get the deduplication time window in milliseconds
   */
  [[nodiscard]] u64 getDeduplicationWindow() const;

private:
  struct Subscriber {
    u64 id;
    EventHandler handler;
    std::optional<EditorEventType> typeFilter;
    std::optional<EventFilter> customFilter;
  };

  // Pending operation for deferred subscription modifications
  struct PendingOperation {
    enum class Type { Add, Remove, RemoveByType, RemoveAll } type;
    Subscriber subscriber;        // For Add
    u64 subscriptionId = 0;       // For Remove
    EditorEventType eventType;    // For RemoveByType
  };

  void dispatchEvent(const EditorEvent &event);
  void processPendingOperations();

  std::vector<Subscriber> m_subscribers;
  std::queue<std::unique_ptr<EditorEvent>> m_eventQueue;
  u64 m_nextSubscriberId = 1;

  // Deferred modification support
  std::atomic<int> m_dispatchDepth{0};
  std::vector<PendingOperation> m_pendingOperations;

  bool m_synchronous = true;
  bool m_historyEnabled = false;
  std::vector<std::string> m_eventHistory;
  static constexpr size_t MAX_HISTORY_SIZE = 100;

  // Deduplication support
  bool m_deduplicationEnabled = false;
  u64 m_deduplicationWindowMs = 100; // Default: 100ms window
  std::unordered_map<std::string, u64> m_recentEvents; // event key -> timestamp

  mutable std::mutex m_mutex;
};

/**
 * @brief RAII helper for event subscription
 */
class ScopedEventSubscription {
public:
  ScopedEventSubscription() = default;

  ScopedEventSubscription(EventBus *bus, EventSubscription subscription)
      : m_bus(bus), m_subscription(subscription) {}

  ~ScopedEventSubscription() {
    if (m_bus && m_subscription.isValid()) {
      m_bus->unsubscribe(m_subscription);
    }
  }

  // Move only
  ScopedEventSubscription(ScopedEventSubscription &&other) noexcept
      : m_bus(other.m_bus), m_subscription(other.m_subscription) {
    other.m_bus = nullptr;
    other.m_subscription = EventSubscription();
  }

  ScopedEventSubscription &operator=(ScopedEventSubscription &&other) noexcept {
    if (this != &other) {
      if (m_bus && m_subscription.isValid()) {
        m_bus->unsubscribe(m_subscription);
      }
      m_bus = other.m_bus;
      m_subscription = other.m_subscription;
      other.m_bus = nullptr;
      other.m_subscription = EventSubscription();
    }
    return *this;
  }

  ScopedEventSubscription(const ScopedEventSubscription &) = delete;
  ScopedEventSubscription &operator=(const ScopedEventSubscription &) = delete;

  [[nodiscard]] bool isValid() const { return m_subscription.isValid(); }

private:
  EventBus *m_bus = nullptr;
  EventSubscription m_subscription;
};

/**
 * @brief Helper macro to subscribe to events with automatic unsubscription
 */
#define NM_SUBSCRIBE_EVENT(bus, type, handler)                                 \
  ScopedEventSubscription(&(bus),                                              \
                          (bus).subscribe(EditorEventType::type, handler))

} // namespace NovelMind::editor
