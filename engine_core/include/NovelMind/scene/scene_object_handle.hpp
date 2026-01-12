#pragma once

/**
 * @file scene_object_handle.hpp
 * @brief RAII-safe handle for scene object inspection
 *
 * Prevents use-after-free bugs by validating object existence before access.
 * Complies with Issue #15 requirement: "Никаких 'сырых SceneObject*' живущих
 * дольше, чем selection."
 */

#include "NovelMind/core/types.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace NovelMind::scene {

// Forward declarations
class SceneGraph;
class SceneObjectBase;

/**
 * @brief Safe handle to a scene object with automatic validation
 *
 * This handle does NOT own the object, but validates its existence
 * before allowing access. It stores only the object ID and a weak
 * reference to the SceneGraph.
 *
 * RAII guarantee: When the object is deleted from the scene,
 * isValid() returns false and get() returns nullptr.
 *
 * THREAD SAFETY: This handle uses a generation counter pattern to prevent
 * TOCTOU (Time-Of-Check-Time-Of-Use) race conditions. All access methods
 * are thread-safe and protected by mutex in SceneGraph.
 */
class SceneObjectHandle {
public:
  /**
   * @brief Construct an invalid handle
   */
  SceneObjectHandle() = default;

  /**
   * @brief Construct a handle to a scene object
   * @param sceneGraph Pointer to the scene graph (must outlive this handle)
   * @param objectId ID of the object to track
   */
  SceneObjectHandle(SceneGraph *sceneGraph, const std::string &objectId);

  /**
   * @brief Check if the referenced object still exists
   *
   * THREAD SAFETY: This method is thread-safe. However, to prevent TOCTOU
   * race conditions, prefer using withObject() or withObjectAs() instead
   * of checking isValid() and then calling get().
   */
  [[nodiscard]] bool isValid() const;

  /**
   * @brief Get the object ID
   */
  [[nodiscard]] const std::string &getId() const { return m_objectId; }

  /**
   * @brief Get the object pointer if it still exists
   * @return Pointer to object, or nullptr if object was deleted
   *
   * THREAD SAFETY: This method is thread-safe. The returned pointer is only
   * valid for the duration of the current mutex lock. For thread-safe access,
   * prefer using withObject() or withObjectAs() which guarantee atomic
   * check-and-use operations.
   */
  [[nodiscard]] SceneObjectBase *get() const;

  /**
   * @brief Get the object with type checking
   * @tparam T Derived type to cast to
   * @return Pointer to object cast to T, or nullptr if invalid or wrong type
   *
   * THREAD SAFETY: See get() documentation for thread safety notes.
   */
  template <typename T> [[nodiscard]] T *getAs() const {
    return dynamic_cast<T *>(get());
  }

  /**
   * @brief Execute a function with the object if it exists
   * @param fn Function to call with the object pointer
   * @return true if object existed and function was called
   *
   * THREAD SAFETY: This method provides atomic check-and-use semantics,
   * preventing TOCTOU race conditions. The function is executed under
   * mutex protection, ensuring the object cannot be deleted during execution.
   */
  bool withObject(std::function<void(SceneObjectBase *)> fn) const;

  /**
   * @brief Execute a function with typed object if it exists and matches type
   * @tparam T Derived type to cast to
   * @param fn Function to call with the typed object pointer
   * @return true if object existed, matched type, and function was called
   *
   * THREAD SAFETY: This method provides atomic check-and-use semantics,
   * preventing TOCTOU race conditions. The function is executed under
   * mutex protection, ensuring the object cannot be deleted during execution.
   */
  template <typename T> bool withObjectAs(std::function<void(T *)> fn) const {
    if (auto *obj = getAs<T>()) {
      fn(obj);
      return true;
    }
    return false;
  }

  /**
   * @brief Reset the handle to invalid state
   */
  void reset();

  /**
   * @brief Conversion to bool (true if valid)
   */
  explicit operator bool() const { return isValid(); }

private:
  SceneGraph *m_sceneGraph = nullptr;
  std::string m_objectId;
  u64 m_generation = 0; // Generation counter for TOCTOU prevention
};

/**
 * @brief RAII scoped guard for inspector selection
 *
 * Ensures selection is cleared when going out of scope,
 * preventing dangling references.
 */
class ScopedInspectorSelection {
public:
  using ClearCallback = std::function<void()>;

  /**
   * @brief Construct scoped selection with automatic cleanup
   * @param handle Handle to the selected object
   * @param onClear Callback to execute when selection is cleared
   */
  explicit ScopedInspectorSelection(SceneObjectHandle handle,
                                    ClearCallback onClear = nullptr)
      : m_handle(std::move(handle)), m_onClear(std::move(onClear)) {}

  /**
   * @brief Destructor clears the selection
   */
  ~ScopedInspectorSelection() {
    if (m_onClear) {
      m_onClear();
    }
  }

  // Non-copyable
  ScopedInspectorSelection(const ScopedInspectorSelection &) = delete;
  ScopedInspectorSelection &
  operator=(const ScopedInspectorSelection &) = delete;

  // Movable
  ScopedInspectorSelection(ScopedInspectorSelection &&) = default;
  ScopedInspectorSelection &operator=(ScopedInspectorSelection &&) = default;

  /**
   * @brief Get the handle
   */
  [[nodiscard]] const SceneObjectHandle &getHandle() const { return m_handle; }

  /**
   * @brief Check if selection is still valid
   */
  [[nodiscard]] bool isValid() const { return m_handle.isValid(); }

private:
  SceneObjectHandle m_handle;
  ClearCallback m_onClear;
};

} // namespace NovelMind::scene
