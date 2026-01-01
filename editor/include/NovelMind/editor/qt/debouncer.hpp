#pragma once

/**
 * @file debouncer.hpp
 * @brief Debounce utility for preventing rapid event handling
 *
 * Provides a reusable debounce mechanism for Qt applications to prevent
 * excessive updates during rapid user interactions like typing or property
 * changes. This helps improve performance by grouping rapid changes into
 * a single update after a configurable delay.
 *
 * Issue #173: Implements debounce for frequent events to reduce redundant
 * recalculations during rapid text input or property changes.
 */

#include <QObject>
#include <QTimer>
#include <functional>

namespace NovelMind::editor::qt {

/**
 * @brief Debounce utility class for delaying and coalescing rapid events
 *
 * When multiple calls to trigger() occur within the delay period,
 * only the last callback will be executed after the delay expires.
 *
 * Example usage:
 * @code
 *   Debouncer debouncer(300); // 300ms delay
 *   debouncer.trigger([this]() {
 *     runExpensiveOperation();
 *   });
 * @endcode
 */
class Debouncer : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a debouncer with specified delay
   * @param delayMs Delay in milliseconds before callback is executed
   * @param parent Parent QObject
   */
  explicit Debouncer(int delayMs = 300, QObject *parent = nullptr)
      : QObject(parent), m_delayMs(delayMs) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Debouncer::onTimeout);
  }

  /**
   * @brief Trigger the debouncer with a callback
   *
   * If called multiple times within the delay period, the timer resets
   * and only the most recent callback will be executed.
   *
   * @param callback Function to execute after delay
   */
  void trigger(std::function<void()> callback) {
    m_pendingCallback = std::move(callback);
    m_timer.start(m_delayMs);
  }

  /**
   * @brief Cancel any pending callback
   */
  void cancel() {
    m_timer.stop();
    m_pendingCallback = nullptr;
  }

  /**
   * @brief Check if a callback is pending
   * @return true if a callback is scheduled
   */
  [[nodiscard]] bool isPending() const { return m_timer.isActive(); }

  /**
   * @brief Immediately execute any pending callback and cancel the timer
   */
  void flush() {
    if (m_timer.isActive()) {
      m_timer.stop();
      onTimeout();
    }
  }

  /**
   * @brief Set the delay in milliseconds
   * @param delayMs New delay value
   */
  void setDelay(int delayMs) { m_delayMs = delayMs; }

  /**
   * @brief Get the current delay in milliseconds
   * @return Current delay value
   */
  [[nodiscard]] int delay() const { return m_delayMs; }

signals:
  /**
   * @brief Emitted when the debounced callback is about to execute
   */
  void triggered();

private slots:
  void onTimeout() {
    if (m_pendingCallback) {
      emit triggered();
      m_pendingCallback();
      m_pendingCallback = nullptr;
    }
  }

private:
  QTimer m_timer;
  int m_delayMs;
  std::function<void()> m_pendingCallback;
};

/**
 * @brief Property change debouncer specialized for property editing
 *
 * Tracks property name and value to coalesce rapid changes to the same
 * property, useful for text fields and spinboxes during user editing.
 */
class PropertyDebouncer : public Debouncer {
  Q_OBJECT

public:
  explicit PropertyDebouncer(int delayMs = 300, QObject *parent = nullptr)
      : Debouncer(delayMs, parent) {}

  /**
   * @brief Trigger a property change with debouncing
   *
   * @param propertyName Name of the property being changed
   * @param newValue New value for the property
   * @param callback Callback to execute with the final value
   */
  void triggerPropertyChange(const QString &propertyName,
                             const QString &newValue,
                             std::function<void(const QString &, const QString &)> callback) {
    m_lastPropertyName = propertyName;
    m_lastValue = newValue;
    m_propertyCallback = std::move(callback);

    trigger([this]() {
      if (m_propertyCallback) {
        m_propertyCallback(m_lastPropertyName, m_lastValue);
      }
    });
  }

  /**
   * @brief Get the last property name that was debounced
   * @return Last property name
   */
  [[nodiscard]] QString lastPropertyName() const { return m_lastPropertyName; }

  /**
   * @brief Get the last value that was debounced
   * @return Last value
   */
  [[nodiscard]] QString lastValue() const { return m_lastValue; }

private:
  QString m_lastPropertyName;
  QString m_lastValue;
  std::function<void(const QString &, const QString &)> m_propertyCallback;
};

} // namespace NovelMind::editor::qt
