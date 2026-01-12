#pragma once

/**
 * @file crash_safety.hpp
 * @brief Crash Safety System for NovelMind Editor
 *
 * Provides robust error handling and recovery mechanisms:
 * - Runtime isolation (runtime errors don't crash editor)
 * - Automatic state recovery
 * - Hot-reload safety
 * - Error boundary system
 * - Graceful degradation
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class EditorRuntimeHost;
class CrashSafetyManager;

/**
 * @brief Severity of a runtime error
 */
enum class ErrorSeverity : u8 {
  Info,     // Non-critical information
  Warning,  // Potential issue, execution continues
  Error,    // Error, execution may be affected
  Critical, // Critical error, execution stopped
  Fatal     // Fatal error, requires restart
};

/**
 * @brief Type of runtime error
 */
enum class ErrorType : u8 {
  ScriptExecution, // Error in script VM
  AssetLoading,    // Failed to load asset
  StateCorruption, // Invalid runtime state
  MemoryError,     // Memory allocation failure
  Timeout,         // Execution timeout
  HotReload,       // Error during hot reload
  External,        // External system error
  Unknown          // Unknown error type
};

/**
 * @brief Detailed error information
 */
struct RuntimeError {
  ErrorSeverity severity;
  ErrorType type;
  std::string code;
  std::string message;
  std::string stackTrace;
  std::string context;

  // Location info
  std::string sceneName;
  std::string scriptFile;
  i32 lineNumber = -1;
  i32 instructionPointer = -1;

  // Timing
  u64 timestamp = 0;
  f64 runtimeTimeSeconds = 0.0;

  // Recovery info
  bool isRecoverable = true;
  std::string suggestedAction;
};

/**
 * @brief Checkpoint for state recovery
 */
struct RuntimeCheckpoint {
  u64 timestamp = 0;
  std::string sceneName;
  i32 scriptPosition = 0;
  std::vector<u8> sceneState;
  std::vector<u8> variableState;
  std::vector<u8> flagState;
  std::string description;

  // Metadata
  size_t memoryUsage = 0;
  f64 runtimeTimeSeconds = 0.0;
};

/**
 * @brief Configuration for crash safety
 */
struct CrashSafetyConfig {
  // Timeout settings
  f64 scriptTimeoutSeconds = 5.0;
  f64 assetLoadTimeoutSeconds = 10.0;
  f64 hotReloadTimeoutSeconds = 30.0;

  // Checkpoint settings
  bool enableAutoCheckpoints = true;
  f64 checkpointIntervalSeconds = 30.0;
  i32 maxCheckpoints = 10;

  // Recovery settings
  bool enableAutoRecovery = true;
  i32 maxRecoveryAttempts = 3;
  f64 recoveryDelaySeconds = 0.5;

  // Error handling
  bool pauseOnError = true;
  bool showErrorDialog = true;
  bool logErrorsToFile = true;
  std::string errorLogPath = "logs/runtime_errors.log";

  // Isolation settings
  bool isolateRuntime = true;
  size_t maxMemoryMB = 512;
  i32 maxInstructionsPerFrame = 100000;
};

/**
 * @brief Listener for crash safety events
 */
class ICrashSafetyListener {
public:
  virtual ~ICrashSafetyListener() = default;

  virtual void onErrorOccurred(const RuntimeError& error) = 0;
  virtual void onRecoveryStarted(const std::string& checkpointDescription) = 0;
  virtual void onRecoveryCompleted(bool success) = 0;
  virtual void onCheckpointCreated(const std::string& description) = 0;
  virtual void onRuntimeIsolated() = 0;
  virtual void onRuntimeResumed() = 0;
};

/**
 * @brief Error boundary for isolating runtime errors
 */
class ErrorBoundary {
public:
  ErrorBoundary(CrashSafetyManager* manager, const std::string& context);
  ~ErrorBoundary();

  /**
   * @brief Execute a function within the error boundary
   */
  template <typename F> Result<void> execute(F&& func);

  /**
   * @brief Check if an error occurred
   */
  [[nodiscard]] bool hasError() const { return m_hasError; }

  /**
   * @brief Get the captured error
   */
  [[nodiscard]] const RuntimeError& getError() const { return m_error; }

private:
  CrashSafetyManager* m_manager;
  std::string m_context;
  bool m_hasError = false;
  RuntimeError m_error;
};

/**
 * @brief Crash Safety Manager
 *
 * Provides comprehensive crash safety for the editor runtime:
 * - Isolates runtime errors from editor crashes
 * - Creates automatic checkpoints for state recovery
 * - Handles hot-reload errors gracefully
 * - Provides detailed error reporting to DiagnosticsPanel
 */
class CrashSafetyManager {
public:
  CrashSafetyManager();
  ~CrashSafetyManager() = default;

  /**
   * @brief Initialize with runtime host
   */
  void initialize(EditorRuntimeHost* runtimeHost);

  /**
   * @brief Set configuration
   */
  void setConfig(const CrashSafetyConfig& config);

  /**
   * @brief Get current configuration
   */
  [[nodiscard]] const CrashSafetyConfig& getConfig() const { return m_config; }

  /**
   * @brief Update (check for timeouts, create auto-checkpoints)
   */
  void update(f64 deltaTime);

  // Error handling

  /**
   * @brief Report a runtime error
   */
  void reportError(const RuntimeError& error);

  /**
   * @brief Create error from exception
   */
  RuntimeError createErrorFromException(const std::exception& ex, ErrorType type,
                                        const std::string& context);

  /**
   * @brief Get recent errors
   */
  [[nodiscard]] const std::vector<RuntimeError>& getRecentErrors() const { return m_recentErrors; }

  /**
   * @brief Clear error history
   */
  void clearErrors();

  /**
   * @brief Check if runtime is in error state
   */
  [[nodiscard]] bool isInErrorState() const { return m_isInErrorState; }

  // Checkpoint management

  /**
   * @brief Create a checkpoint
   */
  Result<void> createCheckpoint(const std::string& description = "");

  /**
   * @brief Restore to a checkpoint
   */
  Result<void> restoreCheckpoint(size_t checkpointIndex);

  /**
   * @brief Restore to most recent checkpoint
   */
  Result<void> restoreLatestCheckpoint();

  /**
   * @brief Get available checkpoints
   */
  [[nodiscard]] const std::vector<RuntimeCheckpoint>& getCheckpoints() const {
    return m_checkpoints;
  }

  /**
   * @brief Clear all checkpoints
   */
  void clearCheckpoints();

  // Recovery

  /**
   * @brief Attempt automatic recovery
   */
  Result<void> attemptRecovery();

  /**
   * @brief Reset runtime to clean state
   */
  Result<void> resetRuntime();

  /**
   * @brief Check if recovery is possible
   */
  [[nodiscard]] bool canRecover() const;

  // Isolation

  /**
   * @brief Isolate runtime (pause and prevent further execution)
   */
  void isolateRuntime();

  /**
   * @brief Resume runtime from isolation
   */
  void resumeRuntime();

  /**
   * @brief Check if runtime is isolated
   */
  [[nodiscard]] bool isRuntimeIsolated() const { return m_isIsolated; }

  // Watchdog

  /**
   * @brief Start watchdog timer
   */
  void startWatchdog(f64 timeoutSeconds);

  /**
   * @brief Reset watchdog timer
   */
  void resetWatchdog();

  /**
   * @brief Stop watchdog timer
   */
  void stopWatchdog();

  /**
   * @brief Check if watchdog has triggered
   */
  [[nodiscard]] bool hasWatchdogTriggered() const { return m_watchdogTriggered; }

  // Listeners

  /**
   * @brief Add crash safety listener
   */
  void addListener(ICrashSafetyListener* listener);

  /**
   * @brief Remove listener
   */
  void removeListener(ICrashSafetyListener* listener);

  // Logging

  /**
   * @brief Write error to log file
   */
  void logError(const RuntimeError& error);

  /**
   * @brief Get error log path
   */
  [[nodiscard]] std::string getErrorLogPath() const;

private:
  void createAutoCheckpoint();
  void trimCheckpoints();
  void notifyErrorOccurred(const RuntimeError& error);
  void notifyRecoveryStarted(const std::string& description);
  void notifyRecoveryCompleted(bool success);
  void notifyCheckpointCreated(const std::string& description);
  void notifyRuntimeIsolated();
  void notifyRuntimeResumed();

  RuntimeCheckpoint captureCurrentState();
  void restoreState(const RuntimeCheckpoint& checkpoint);

  std::string formatStackTrace();
  std::string getCurrentContext();

  EditorRuntimeHost* m_runtimeHost = nullptr;
  CrashSafetyConfig m_config;

  // Error state
  bool m_isInErrorState = false;
  std::vector<RuntimeError> m_recentErrors;
  size_t m_maxRecentErrors = 100;

  // Checkpoints
  std::vector<RuntimeCheckpoint> m_checkpoints;
  f64 m_timeSinceLastCheckpoint = 0.0;

  // Isolation
  bool m_isIsolated = false;
  i32 m_recoveryAttempts = 0;

  // Watchdog
  bool m_watchdogActive = false;
  bool m_watchdogTriggered = false;
  f64 m_watchdogTimeout = 0.0;
  f64 m_watchdogElapsed = 0.0;

  // Listeners
  std::vector<ICrashSafetyListener*> m_listeners;
};

/**
 * @brief RAII helper for error boundary
 */
class SafeExecution {
public:
  SafeExecution(CrashSafetyManager* manager, const std::string& context);
  ~SafeExecution();

  template <typename F> Result<void> run(F&& func);

  [[nodiscard]] bool succeeded() const { return m_succeeded; }
  [[nodiscard]] const RuntimeError& getError() const { return m_error; }

private:
  CrashSafetyManager* m_manager;
  std::string m_context;
  bool m_succeeded = true;
  RuntimeError m_error;
};

/**
 * @brief Memory guard for preventing out-of-memory crashes
 */
class MemoryGuard {
public:
  MemoryGuard(CrashSafetyManager* manager, size_t maxMemoryBytes);
  ~MemoryGuard();

  /**
   * @brief Check if memory limit is exceeded
   */
  [[nodiscard]] bool isLimitExceeded() const;

  /**
   * @brief Get current memory usage
   */
  [[nodiscard]] size_t getCurrentUsage() const;

  /**
   * @brief Get memory limit
   */
  [[nodiscard]] size_t getLimit() const { return m_maxMemory; }

private:
  CrashSafetyManager* m_manager;
  size_t m_maxMemory;
  size_t m_initialUsage;
};

/**
 * @brief Hot reload safety wrapper
 */
class HotReloadGuard {
public:
  HotReloadGuard(CrashSafetyManager* manager);
  ~HotReloadGuard();

  /**
   * @brief Execute hot reload with safety checks
   */
  Result<void> executeReload(std::function<Result<void>()> reloadFunc);

private:
  CrashSafetyManager* m_manager;
  RuntimeCheckpoint m_preReloadCheckpoint;
  bool m_checkpointCreated = false;
};

} // namespace NovelMind::editor
