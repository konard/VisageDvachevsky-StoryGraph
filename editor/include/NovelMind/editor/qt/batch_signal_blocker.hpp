#pragma once

/**
 * @file batch_signal_blocker.hpp
 * @brief RAII helper for blocking signals during batch operations
 *
 * Provides utilities to prevent multiple signal emissions during batch
 * operations like multi-select updates. This helps avoid cascading updates
 * and improves performance when modifying multiple items at once.
 *
 * Issue #173: Fixes multiple signal emissions during batch operations.
 */

#include <QObject>
#include <QSignalBlocker>
#include <QVector>
#include <functional>
#include <memory>

namespace NovelMind::editor::qt {

/**
 * @brief RAII helper that blocks signals on multiple objects
 *
 * When destroyed, all blocked objects have their signals restored.
 * Optionally emits a single batch update signal after restoration.
 *
 * Example usage:
 * @code
 *   {
 *     BatchSignalBlocker blocker;
 *     blocker.block(widget1);
 *     blocker.block(widget2);
 *
 *     for (auto& item : items) {
 *       updateItem(item);
 *     }
 *   } // Signals restored here
 *   emitBatchUpdate(items);
 * @endcode
 */
class BatchSignalBlocker {
public:
  BatchSignalBlocker() = default;

  /**
   * @brief Construct with a list of objects to block
   * @param objects Objects to block signals on
   */
  explicit BatchSignalBlocker(const QVector<QObject *> &objects) {
    for (auto *obj : objects) {
      block(obj);
    }
  }

  ~BatchSignalBlocker() {
    // Blockers are automatically released in reverse order
    m_blockers.clear();

    // Execute completion callback if set
    if (m_completionCallback) {
      m_completionCallback();
    }
  }

  // Non-copyable, non-movable for safety
  BatchSignalBlocker(const BatchSignalBlocker &) = delete;
  BatchSignalBlocker &operator=(const BatchSignalBlocker &) = delete;
  BatchSignalBlocker(BatchSignalBlocker &&) = delete;
  BatchSignalBlocker &operator=(BatchSignalBlocker &&) = delete;

  /**
   * @brief Block signals on an object
   * @param obj Object to block signals on
   */
  void block(QObject *obj) {
    if (obj) {
      m_blockers.push_back(std::make_unique<QSignalBlocker>(obj));
    }
  }

  /**
   * @brief Set a callback to be called when the blocker is destroyed
   *
   * Useful for emitting a single batch update signal after all
   * individual updates are complete.
   *
   * @param callback Function to call on destruction
   */
  void setCompletionCallback(std::function<void()> callback) {
    m_completionCallback = std::move(callback);
  }

  /**
   * @brief Get the number of blocked objects
   * @return Number of blocked objects
   */
  [[nodiscard]] int blockedCount() const {
    return static_cast<int>(m_blockers.size());
  }

private:
  QVector<std::unique_ptr<QSignalBlocker>> m_blockers;
  std::function<void()> m_completionCallback;
};

/**
 * @brief Scoped batch operation context with automatic signal management
 *
 * Provides a more structured approach to batch operations with
 * automatic signal blocking and batch notification.
 *
 * Example usage:
 * @code
 *   BatchOperation batch(widget, [this]() {
 *     emit batchUpdateCompleted();
 *   });
 *
 *   batch.execute([&]() {
 *     for (auto& item : items) {
 *       updateItem(item);
 *     }
 *   });
 * @endcode
 */
class BatchOperation {
public:
  /**
   * @brief Construct a batch operation context
   * @param target Primary object to block signals on
   * @param completionCallback Callback to execute after batch completes
   */
  BatchOperation(QObject *target,
                 std::function<void()> completionCallback = nullptr)
      : m_target(target), m_completionCallback(std::move(completionCallback)),
        m_signalBlocker(target) {}

  /**
   * @brief Execute a batch operation
   *
   * The operation runs with signals blocked, and the completion
   * callback is invoked after the operation completes.
   *
   * @param operation Function containing the batch operations
   */
  template <typename Func> void execute(Func &&operation) {
    operation();

    // Allow signals before calling completion callback
    // The signal blocker will be released when this object is destroyed
  }

  ~BatchOperation() {
    // Signal blocker releases here
    if (m_completionCallback) {
      m_completionCallback();
    }
  }

  // Non-copyable, non-movable
  BatchOperation(const BatchOperation &) = delete;
  BatchOperation &operator=(const BatchOperation &) = delete;
  BatchOperation(BatchOperation &&) = delete;
  BatchOperation &operator=(BatchOperation &&) = delete;

private:
  QObject *m_target;
  std::function<void()> m_completionCallback;
  QSignalBlocker m_signalBlocker;
};

/**
 * @brief Helper class to track whether we're in a batch update mode
 *
 * Useful for preventing nested batch operations and for checking
 * if signals should be emitted immediately or deferred.
 */
class BatchUpdateGuard {
public:
  explicit BatchUpdateGuard(bool &flag) : m_flag(flag), m_wasActive(flag) {
    m_flag = true;
  }

  ~BatchUpdateGuard() { m_flag = m_wasActive; }

  BatchUpdateGuard(const BatchUpdateGuard &) = delete;
  BatchUpdateGuard &operator=(const BatchUpdateGuard &) = delete;

  /**
   * @brief Check if a batch update was already active when guard was created
   * @return true if nested batch operation
   */
  [[nodiscard]] bool wasAlreadyActive() const { return m_wasActive; }

private:
  bool &m_flag;
  bool m_wasActive;
};

} // namespace NovelMind::editor::qt
