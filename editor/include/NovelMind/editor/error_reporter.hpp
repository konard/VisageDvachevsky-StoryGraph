#pragma once

/**
 * @file error_reporter.hpp
 * @brief Error Reporting Backend for NovelMind Editor
 *
 * Provides centralized error/warning/diagnostic reporting:
 * - Compilation errors from scripts
 * - AST/validation diagnostics
 * - Missing assets
 * - Missing voice files
 * - Graph validation errors
 * - Runtime errors during preview
 *
 * This aggregates all error sources into a unified stream
 * for the GUI Diagnostics Panel to display.
 */

#include "NovelMind/core/types.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Diagnostic severity levels
 */
enum class DiagnosticSeverity : u8 { Hint = 0, Info, Warning, Error, Fatal };

/**
 * @brief Diagnostic category
 */
enum class DiagnosticCategory : u8 {
  General = 0,
  Script,       // Script compilation errors
  AST,          // AST validation issues
  Graph,        // Story graph validation
  Asset,        // Missing/invalid assets
  Voice,        // Voice file issues
  Localization, // Missing translations
  Timeline,     // Timeline validation
  Scene,        // Scene validation
  Build,        // Build errors
  Runtime       // Runtime errors during preview
};

/**
 * @brief Source location for diagnostics
 */
struct SourceLocation {
  std::string file;
  u32 line = 0;
  u32 column = 0;
  u32 endLine = 0;
  u32 endColumn = 0;

  [[nodiscard]] bool isValid() const { return !file.empty() && line > 0; }

  [[nodiscard]] std::string toString() const {
    if (!isValid()) {
      return "";
    }
    return file + ":" + std::to_string(line) + ":" + std::to_string(column);
  }
};

/**
 * @brief Related information for a diagnostic
 */
struct DiagnosticRelated {
  SourceLocation location;
  std::string message;
};

/**
 * @brief Quick fix suggestion
 */
struct DiagnosticFix {
  std::string title;
  std::string description;
  std::string replacementText;
  SourceLocation range;
};

/**
 * @brief Single diagnostic entry
 */
struct Diagnostic {
  u64 id = 0;
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  DiagnosticCategory category = DiagnosticCategory::General;
  std::string code; // Error code (e.g., "E001", "W042")
  std::string message;
  std::string details; // Extended description
  SourceLocation location;
  std::vector<DiagnosticRelated> relatedInfo;
  std::vector<DiagnosticFix> fixes;
  u64 timestamp = 0;
  bool acknowledged = false;

  [[nodiscard]] bool isError() const {
    return severity == DiagnosticSeverity::Error || severity == DiagnosticSeverity::Fatal;
  }

  [[nodiscard]] bool isWarning() const { return severity == DiagnosticSeverity::Warning; }
};

/**
 * @brief Diagnostic filter
 */
struct DiagnosticFilter {
  std::optional<DiagnosticSeverity> minSeverity;
  std::optional<DiagnosticCategory> category;
  std::optional<std::string> filePattern;
  bool showAcknowledged = true;
};

/**
 * @brief Diagnostic summary
 */
struct DiagnosticSummary {
  u32 errorCount = 0;
  u32 warningCount = 0;
  u32 infoCount = 0;
  u32 hintCount = 0;
  u32 totalCount = 0;
  bool hasErrors = false;
};

/**
 * @brief Listener for diagnostic changes
 */
class IDiagnosticListener {
public:
  virtual ~IDiagnosticListener() = default;

  virtual void onDiagnosticAdded(const Diagnostic& /*diagnostic*/) {}
  virtual void onDiagnosticRemoved(u64 /*id*/) {}
  virtual void onDiagnosticsCleared(DiagnosticCategory /*category*/) {}
  virtual void onAllDiagnosticsCleared() {}
  virtual void onSummaryChanged(const DiagnosticSummary& /*summary*/) {}
};

/**
 * @brief Error reporter / diagnostics manager
 *
 * Responsibilities:
 * - Collect diagnostics from all sources
 * - Filter and organize diagnostics
 * - Provide quick navigation to error sources
 * - Support quick fixes
 * - Track diagnostic history
 */
class ErrorReporter {
public:
  ErrorReporter();
  ~ErrorReporter();

  // Prevent copying
  ErrorReporter(const ErrorReporter&) = delete;
  ErrorReporter& operator=(const ErrorReporter&) = delete;

  /**
   * @brief Get singleton instance
   */
  static ErrorReporter& instance();

  // =========================================================================
  // Reporting
  // =========================================================================

  /**
   * @brief Report a diagnostic
   * @return Diagnostic ID
   */
  u64 report(const Diagnostic& diagnostic);

  /**
   * @brief Report an error
   */
  u64 reportError(const std::string& message, const SourceLocation& location = {},
                  DiagnosticCategory category = DiagnosticCategory::General);

  /**
   * @brief Report a warning
   */
  u64 reportWarning(const std::string& message, const SourceLocation& location = {},
                    DiagnosticCategory category = DiagnosticCategory::General);

  /**
   * @brief Report info
   */
  u64 reportInfo(const std::string& message, const SourceLocation& location = {},
                 DiagnosticCategory category = DiagnosticCategory::General);

  /**
   * @brief Report script compilation error
   */
  u64 reportScriptError(const std::string& file, u32 line, u32 column, const std::string& message,
                        const std::string& code = "");

  /**
   * @brief Report missing asset
   */
  u64 reportMissingAsset(const std::string& assetPath, const std::string& referencedBy = "");

  /**
   * @brief Report missing voice file
   */
  u64 reportMissingVoice(const std::string& lineId, const std::string& expectedPath);

  /**
   * @brief Report graph validation error
   */
  u64 reportGraphError(const std::string& message, const std::string& nodeInfo = "");

  /**
   * @brief Report runtime error
   */
  u64 reportRuntimeError(const std::string& message, const std::string& stackTrace = "");

  // =========================================================================
  // Querying
  // =========================================================================

  /**
   * @brief Get all diagnostics
   */
  [[nodiscard]] std::vector<Diagnostic> getAllDiagnostics() const;

  /**
   * @brief Get diagnostics with filter
   */
  [[nodiscard]] std::vector<Diagnostic> getDiagnostics(const DiagnosticFilter& filter) const;

  /**
   * @brief Get diagnostic by ID
   */
  [[nodiscard]] std::optional<Diagnostic> getDiagnostic(u64 id) const;

  /**
   * @brief Get diagnostics for file
   */
  [[nodiscard]] std::vector<Diagnostic> getDiagnosticsForFile(const std::string& file) const;

  /**
   * @brief Get diagnostics by category
   */
  [[nodiscard]] std::vector<Diagnostic> getDiagnosticsByCategory(DiagnosticCategory category) const;

  /**
   * @brief Get summary
   */
  [[nodiscard]] DiagnosticSummary getSummary() const;

  /**
   * @brief Get summary for category
   */
  [[nodiscard]] DiagnosticSummary getSummary(DiagnosticCategory category) const;

  /**
   * @brief Check if there are errors
   */
  [[nodiscard]] bool hasErrors() const;

  /**
   * @brief Check if there are warnings
   */
  [[nodiscard]] bool hasWarnings() const;

  /**
   * @brief Get diagnostic count
   */
  [[nodiscard]] size_t getCount() const;

  // =========================================================================
  // Management
  // =========================================================================

  /**
   * @brief Remove a diagnostic
   */
  void remove(u64 id);

  /**
   * @brief Clear diagnostics by category
   */
  void clear(DiagnosticCategory category);

  /**
   * @brief Clear all diagnostics
   */
  void clearAll();

  /**
   * @brief Acknowledge a diagnostic (hide from default view)
   */
  void acknowledge(u64 id);

  /**
   * @brief Acknowledge all diagnostics
   */
  void acknowledgeAll();

  /**
   * @brief Set maximum diagnostics to keep
   */
  void setMaxDiagnostics(size_t max);

  // =========================================================================
  // Navigation
  // =========================================================================

  /**
   * @brief Callback type for navigation
   */
  using NavigationCallback = std::function<void(const SourceLocation&)>;

  /**
   * @brief Set navigation callback
   */
  void setNavigationCallback(NavigationCallback callback);

  /**
   * @brief Navigate to diagnostic location
   */
  void navigateTo(u64 id);

  /**
   * @brief Navigate to location
   */
  void navigateTo(const SourceLocation& location);

  // =========================================================================
  // Quick Fixes
  // =========================================================================

  /**
   * @brief Apply a quick fix
   */
  bool applyFix(u64 diagnosticId, size_t fixIndex);

  /**
   * @brief Get available fixes for diagnostic
   */
  [[nodiscard]] std::vector<DiagnosticFix> getFixes(u64 id) const;

  // =========================================================================
  // Listeners
  // =========================================================================

  void addListener(IDiagnosticListener* listener);
  void removeListener(IDiagnosticListener* listener);

  // =========================================================================
  // Batch Operations
  // =========================================================================

  /**
   * @brief Begin batch reporting (delays notifications)
   */
  void beginBatch();

  /**
   * @brief End batch reporting (triggers notifications)
   */
  void endBatch();

private:
  void notifyDiagnosticAdded(const Diagnostic& diagnostic);
  void notifyDiagnosticRemoved(u64 id);
  void notifyCategoryCleared(DiagnosticCategory category);
  void notifyAllCleared();
  void notifySummaryChanged();
  void trimDiagnostics();

  std::vector<Diagnostic> m_diagnostics;
  u64 m_nextId = 1;
  size_t m_maxDiagnostics = 1000;

  std::vector<IDiagnosticListener*> m_listeners;
  NavigationCallback m_navigationCallback;

  bool m_inBatch = false;
  std::vector<Diagnostic> m_batchDiagnostics;

  mutable std::mutex m_mutex;

  static std::unique_ptr<ErrorReporter> s_instance;
};

/**
 * @brief Severity to string conversion
 */
[[nodiscard]] const char* severityToString(DiagnosticSeverity severity);

/**
 * @brief Category to string conversion
 */
[[nodiscard]] const char* categoryToString(DiagnosticCategory category);

/**
 * @brief RAII helper for batch diagnostics
 */
class DiagnosticBatch {
public:
  explicit DiagnosticBatch(ErrorReporter* reporter = nullptr)
      : m_reporter(reporter ? reporter : &ErrorReporter::instance()) {
    m_reporter->beginBatch();
  }

  ~DiagnosticBatch() { m_reporter->endBatch(); }

  DiagnosticBatch(const DiagnosticBatch&) = delete;
  DiagnosticBatch& operator=(const DiagnosticBatch&) = delete;

private:
  ErrorReporter* m_reporter;
};

} // namespace NovelMind::editor
