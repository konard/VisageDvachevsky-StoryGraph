#include "NovelMind/editor/error_reporter.hpp"
#include <algorithm>
#include <chrono>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<ErrorReporter> ErrorReporter::s_instance = nullptr;

ErrorReporter::ErrorReporter() = default;

ErrorReporter::~ErrorReporter() = default;

ErrorReporter& ErrorReporter::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<ErrorReporter>();
  }
  return *s_instance;
}

// ============================================================================
// Reporting
// ============================================================================

u64 ErrorReporter::report(const Diagnostic& diagnostic) {
  std::lock_guard<std::mutex> lock(m_mutex);

  Diagnostic diag = diagnostic;
  diag.id = m_nextId++;
  diag.timestamp = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());

  if (m_inBatch) {
    m_batchDiagnostics.push_back(diag);
  } else {
    m_diagnostics.push_back(diag);
    trimDiagnostics();
    notifyDiagnosticAdded(diag);
    notifySummaryChanged();
  }

  return diag.id;
}

u64 ErrorReporter::reportError(const std::string& message, const SourceLocation& location,
                               DiagnosticCategory category) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Error;
  diag.category = category;
  diag.message = message;
  diag.location = location;
  return report(diag);
}

u64 ErrorReporter::reportWarning(const std::string& message, const SourceLocation& location,
                                 DiagnosticCategory category) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Warning;
  diag.category = category;
  diag.message = message;
  diag.location = location;
  return report(diag);
}

u64 ErrorReporter::reportInfo(const std::string& message, const SourceLocation& location,
                              DiagnosticCategory category) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Info;
  diag.category = category;
  diag.message = message;
  diag.location = location;
  return report(diag);
}

u64 ErrorReporter::reportScriptError(const std::string& file, u32 line, u32 column,
                                     const std::string& message, const std::string& code) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Error;
  diag.category = DiagnosticCategory::Script;
  diag.message = message;
  diag.code = code;
  diag.location.file = file;
  diag.location.line = line;
  diag.location.column = column;
  return report(diag);
}

u64 ErrorReporter::reportMissingAsset(const std::string& assetPath,
                                      const std::string& referencedBy) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Error;
  diag.category = DiagnosticCategory::Asset;
  diag.message = "Missing asset: " + assetPath;
  if (!referencedBy.empty()) {
    diag.details = "Referenced by: " + referencedBy;
  }
  return report(diag);
}

u64 ErrorReporter::reportMissingVoice(const std::string& lineId, const std::string& expectedPath) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Warning;
  diag.category = DiagnosticCategory::Voice;
  diag.message = "Missing voice file for line: " + lineId;
  diag.details = "Expected path: " + expectedPath;
  return report(diag);
}

u64 ErrorReporter::reportGraphError(const std::string& message, const std::string& nodeInfo) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Error;
  diag.category = DiagnosticCategory::Graph;
  diag.message = message;
  if (!nodeInfo.empty()) {
    diag.details = "Node: " + nodeInfo;
  }
  return report(diag);
}

u64 ErrorReporter::reportRuntimeError(const std::string& message, const std::string& stackTrace) {
  Diagnostic diag;
  diag.severity = DiagnosticSeverity::Error;
  diag.category = DiagnosticCategory::Runtime;
  diag.message = message;
  diag.details = stackTrace;
  return report(diag);
}

// ============================================================================
// Querying
// ============================================================================

std::vector<Diagnostic> ErrorReporter::getAllDiagnostics() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_diagnostics;
}

std::vector<Diagnostic> ErrorReporter::getDiagnostics(const DiagnosticFilter& filter) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<Diagnostic> result;

  for (const auto& diag : m_diagnostics) {
    bool match = true;

    if (filter.minSeverity.has_value() &&
        static_cast<u8>(diag.severity) < static_cast<u8>(filter.minSeverity.value())) {
      match = false;
    }

    if (filter.category.has_value() && diag.category != filter.category.value()) {
      match = false;
    }

    if (!filter.showAcknowledged && diag.acknowledged) {
      match = false;
    }

    if (filter.filePattern.has_value() && !filter.filePattern->empty()) {
      // Simple pattern matching (would use regex in production)
      if (diag.location.file.find(filter.filePattern.value()) == std::string::npos) {
        match = false;
      }
    }

    if (match) {
      result.push_back(diag);
    }
  }

  return result;
}

std::optional<Diagnostic> ErrorReporter::getDiagnostic(u64 id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = std::find_if(m_diagnostics.begin(), m_diagnostics.end(),
                         [id](const Diagnostic& d) { return d.id == id; });

  if (it != m_diagnostics.end()) {
    return *it;
  }
  return std::nullopt;
}

std::vector<Diagnostic> ErrorReporter::getDiagnosticsForFile(const std::string& file) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<Diagnostic> result;

  for (const auto& diag : m_diagnostics) {
    if (diag.location.file == file) {
      result.push_back(diag);
    }
  }

  return result;
}

std::vector<Diagnostic> ErrorReporter::getDiagnosticsByCategory(DiagnosticCategory category) const {
  DiagnosticFilter filter;
  filter.category = category;
  return getDiagnostics(filter);
}

DiagnosticSummary ErrorReporter::getSummary() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  DiagnosticSummary summary;

  for (const auto& diag : m_diagnostics) {
    summary.totalCount++;

    switch (diag.severity) {
    case DiagnosticSeverity::Error:
    case DiagnosticSeverity::Fatal:
      summary.errorCount++;
      summary.hasErrors = true;
      break;
    case DiagnosticSeverity::Warning:
      summary.warningCount++;
      break;
    case DiagnosticSeverity::Info:
      summary.infoCount++;
      break;
    case DiagnosticSeverity::Hint:
      summary.hintCount++;
      break;
    }
  }

  return summary;
}

DiagnosticSummary ErrorReporter::getSummary(DiagnosticCategory category) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  DiagnosticSummary summary;

  for (const auto& diag : m_diagnostics) {
    if (diag.category != category) {
      continue;
    }

    summary.totalCount++;

    switch (diag.severity) {
    case DiagnosticSeverity::Error:
    case DiagnosticSeverity::Fatal:
      summary.errorCount++;
      summary.hasErrors = true;
      break;
    case DiagnosticSeverity::Warning:
      summary.warningCount++;
      break;
    case DiagnosticSeverity::Info:
      summary.infoCount++;
      break;
    case DiagnosticSeverity::Hint:
      summary.hintCount++;
      break;
    }
  }

  return summary;
}

bool ErrorReporter::hasErrors() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto& diag : m_diagnostics) {
    if (diag.isError()) {
      return true;
    }
  }
  return false;
}

bool ErrorReporter::hasWarnings() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto& diag : m_diagnostics) {
    if (diag.isWarning()) {
      return true;
    }
  }
  return false;
}

size_t ErrorReporter::getCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_diagnostics.size();
}

// ============================================================================
// Management
// ============================================================================

void ErrorReporter::remove(u64 id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = std::find_if(m_diagnostics.begin(), m_diagnostics.end(),
                         [id](const Diagnostic& d) { return d.id == id; });

  if (it != m_diagnostics.end()) {
    m_diagnostics.erase(it);
    notifyDiagnosticRemoved(id);
    notifySummaryChanged();
  }
}

void ErrorReporter::clear(DiagnosticCategory category) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_diagnostics.erase(
      std::remove_if(m_diagnostics.begin(), m_diagnostics.end(),
                     [category](const Diagnostic& d) { return d.category == category; }),
      m_diagnostics.end());

  notifyCategoryCleared(category);
  notifySummaryChanged();
}

void ErrorReporter::clearAll() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_diagnostics.clear();
  notifyAllCleared();
  notifySummaryChanged();
}

void ErrorReporter::acknowledge(u64 id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = std::find_if(m_diagnostics.begin(), m_diagnostics.end(),
                         [id](const Diagnostic& d) { return d.id == id; });

  if (it != m_diagnostics.end()) {
    it->acknowledged = true;
  }
}

void ErrorReporter::acknowledgeAll() {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto& diag : m_diagnostics) {
    diag.acknowledged = true;
  }
}

void ErrorReporter::setMaxDiagnostics(size_t max) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_maxDiagnostics = max;
  trimDiagnostics();
}

// ============================================================================
// Navigation
// ============================================================================

void ErrorReporter::setNavigationCallback(NavigationCallback callback) {
  m_navigationCallback = std::move(callback);
}

void ErrorReporter::navigateTo(u64 id) {
  auto diag = getDiagnostic(id);
  if (diag.has_value() && diag->location.isValid()) {
    navigateTo(diag->location);
  }
}

void ErrorReporter::navigateTo(const SourceLocation& location) {
  if (m_navigationCallback && location.isValid()) {
    m_navigationCallback(location);
  }
}

// ============================================================================
// Quick Fixes
// ============================================================================

bool ErrorReporter::applyFix(u64 diagnosticId, size_t fixIndex) {
  auto diag = getDiagnostic(diagnosticId);
  if (!diag.has_value() || fixIndex >= diag->fixes.size()) {
    return false;
  }

  // Would apply the fix to the source file
  // For now, just remove the diagnostic
  remove(diagnosticId);
  return true;
}

std::vector<DiagnosticFix> ErrorReporter::getFixes(u64 id) const {
  auto diag = getDiagnostic(id);
  if (diag.has_value()) {
    return diag->fixes;
  }
  return {};
}

// ============================================================================
// Listeners
// ============================================================================

void ErrorReporter::addListener(IDiagnosticListener* listener) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void ErrorReporter::removeListener(IDiagnosticListener* listener) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

// ============================================================================
// Batch Operations
// ============================================================================

void ErrorReporter::beginBatch() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_inBatch = true;
  m_batchDiagnostics.clear();
}

void ErrorReporter::endBatch() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_inBatch) {
    return;
  }

  m_inBatch = false;

  for (const auto& diag : m_batchDiagnostics) {
    m_diagnostics.push_back(diag);
  }

  trimDiagnostics();

  for (const auto& diag : m_batchDiagnostics) {
    notifyDiagnosticAdded(diag);
  }

  notifySummaryChanged();
  m_batchDiagnostics.clear();
}

// ============================================================================
// Private Methods
// ============================================================================

void ErrorReporter::notifyDiagnosticAdded(const Diagnostic& diagnostic) {
  for (auto* listener : m_listeners) {
    listener->onDiagnosticAdded(diagnostic);
  }
}

void ErrorReporter::notifyDiagnosticRemoved(u64 id) {
  for (auto* listener : m_listeners) {
    listener->onDiagnosticRemoved(id);
  }
}

void ErrorReporter::notifyCategoryCleared(DiagnosticCategory category) {
  for (auto* listener : m_listeners) {
    listener->onDiagnosticsCleared(category);
  }
}

void ErrorReporter::notifyAllCleared() {
  for (auto* listener : m_listeners) {
    listener->onAllDiagnosticsCleared();
  }
}

void ErrorReporter::notifySummaryChanged() {
  DiagnosticSummary summary = getSummary();
  for (auto* listener : m_listeners) {
    listener->onSummaryChanged(summary);
  }
}

void ErrorReporter::trimDiagnostics() {
  while (m_diagnostics.size() > m_maxDiagnostics) {
    // Remove oldest acknowledged first, then oldest overall
    auto it = std::find_if(m_diagnostics.begin(), m_diagnostics.end(),
                           [](const Diagnostic& d) { return d.acknowledged; });

    if (it != m_diagnostics.end()) {
      m_diagnostics.erase(it);
    } else {
      m_diagnostics.erase(m_diagnostics.begin());
    }
  }
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* severityToString(DiagnosticSeverity severity) {
  switch (severity) {
  case DiagnosticSeverity::Hint:
    return "Hint";
  case DiagnosticSeverity::Info:
    return "Info";
  case DiagnosticSeverity::Warning:
    return "Warning";
  case DiagnosticSeverity::Error:
    return "Error";
  case DiagnosticSeverity::Fatal:
    return "Fatal";
  }
  return "Unknown";
}

const char* categoryToString(DiagnosticCategory category) {
  switch (category) {
  case DiagnosticCategory::General:
    return "General";
  case DiagnosticCategory::Script:
    return "Script";
  case DiagnosticCategory::AST:
    return "AST";
  case DiagnosticCategory::Graph:
    return "Graph";
  case DiagnosticCategory::Asset:
    return "Asset";
  case DiagnosticCategory::Voice:
    return "Voice";
  case DiagnosticCategory::Localization:
    return "Localization";
  case DiagnosticCategory::Timeline:
    return "Timeline";
  case DiagnosticCategory::Scene:
    return "Scene";
  case DiagnosticCategory::Build:
    return "Build";
  case DiagnosticCategory::Runtime:
    return "Runtime";
  }
  return "Unknown";
}

} // namespace NovelMind::editor
