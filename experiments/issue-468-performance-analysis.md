# Issue #468: EventBus Performance Analysis

## Problem Statement

The EventBus::dispatchEvent() function performs a full copy of the subscriber list on every event dispatch (lines 79-84 in event_bus.cpp):

```cpp
// Copy subscribers to avoid issues if handlers modify subscriptions
std::vector<Subscriber> subscribersCopy;
{
  std::lock_guard<std::mutex> lock(m_mutex);
  subscribersCopy = m_subscribers;  // Full copy!
}
```

### Performance Impact

With N subscribers, each dispatch:
- Allocates memory for N subscribers
- Copies N Subscriber objects (each containing std::function, std::optional with filters)
- Deallocates the copy after dispatch

For a typical editor with 50-100 subscribers, this means:
- 50-100 object copies per event
- Memory allocation/deallocation overhead
- Cache misses during iteration

## Current Implementation Analysis

The copy is defensive - it prevents issues when event handlers modify the subscriber list during dispatch (subscribe/unsubscribe during event handling).

### Thread Safety Model
- Mutex protects m_subscribers
- Lock is held only during copy (lines 82-84)
- Dispatch happens without lock (lines 87-109)

## Solution Options

### Option 1: Deferred Modifications (Recommended)
**Pros:**
- No copy needed during dispatch
- Simple implementation
- Maintains thread safety
- O(1) cost for dispatch

**Cons:**
- Modifications delayed until after dispatch
- Slightly more complex subscription/unsubscription logic

**Implementation:**
- Add m_dispatchDepth counter
- Add m_pendingOperations queue
- During dispatch: increment depth, queue modifications
- After dispatch: decrement depth, apply pending operations

### Option 2: Copy-on-Write (Complex)
**Pros:**
- Zero cost if no modifications during dispatch

**Cons:**
- More complex implementation
- Still requires copy when modifications happen
- std::shared_ptr overhead

### Option 3: Lock-Free Data Structure (Overkill)
**Pros:**
- No locks
- Maximum performance

**Cons:**
- Very complex
- Harder to debug
- Overkill for editor use case

## Recommended Solution: Deferred Modifications

### Implementation Plan

1. Add members:
   ```cpp
   std::atomic<int> m_dispatchDepth{0};
   struct PendingOperation {
     enum class Type { Add, Remove } type;
     Subscriber subscriber;  // For Add
     u64 subscriptionId;     // For Remove
   };
   std::vector<PendingOperation> m_pendingOperations;
   ```

2. Modify dispatchEvent():
   ```cpp
   void EventBus::dispatchEvent(const EditorEvent &event) {
     // Increment dispatch depth
     m_dispatchDepth++;

     // Lock only to access subscriber list
     std::vector<Subscriber> const* subscribersPtr;
     {
       std::lock_guard<std::mutex> lock(m_mutex);
       subscribersPtr = &m_subscribers;
     }

     // Dispatch without holding lock
     for (const auto &subscriber : *subscribersPtr) {
       // ... filter and dispatch logic ...
     }

     // Decrement and process pending
     m_dispatchDepth--;
     if (m_dispatchDepth == 0) {
       processPendingOperations();
     }
   }
   ```

3. Modify subscribe/unsubscribe:
   ```cpp
   void EventBus::subscribe(...) {
     if (m_dispatchDepth > 0) {
       // Queue for later
       std::lock_guard<std::mutex> lock(m_mutex);
       m_pendingOperations.push_back({Type::Add, subscriber, 0});
     } else {
       // Apply immediately
       std::lock_guard<std::mutex> lock(m_mutex);
       m_subscribers.push_back(subscriber);
     }
   }
   ```

### Safety Considerations

1. **Thread Safety**: Must still use mutex for m_subscribers access
2. **Reentrant Dispatch**: m_dispatchDepth handles nested dispatch calls
3. **Exception Safety**: Use RAII for depth management

## Expected Performance Improvement

### Before:
- O(N) copy operation per dispatch
- Memory allocation per dispatch
- ~50-100 object copies for typical use

### After:
- O(1) reference access per dispatch
- No allocation during dispatch
- Modifications queued only when they occur

### Benchmark Plan:
1. Create 100 subscribers
2. Dispatch 10,000 events
3. Measure time before/after fix
4. Expected improvement: 50-90% reduction in dispatch time
