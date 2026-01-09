#include "NovelMind/scripting/vm_debugger.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/scripting/vm.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace NovelMind::scripting {

VMDebugger::VMDebugger(VirtualMachine *vm)
    : m_vm(vm), m_nextBreakpointId(1), m_isPaused(false),
      m_stepMode(DebugStepMode::None), m_stepStartDepth(0) {
  if (!m_vm) {
    NOVELMIND_LOG_ERROR("VMDebugger created with null VM");
  }
}

VMDebugger::~VMDebugger() = default;

// =========================================================================
// Breakpoint Management
// =========================================================================

u32 VMDebugger::addBreakpoint(u32 ip) {
  Breakpoint bp(m_nextBreakpointId++, ip);
  m_breakpoints[bp.id] = bp;
  m_breakpointIPs.insert(ip);
  NOVELMIND_LOG_DEBUG("Added breakpoint " + std::to_string(bp.id) + " at IP " + std::to_string(ip));
  return bp.id;
}

u32 VMDebugger::addBreakpoint(u32 ip, const std::string &sourceFile,
                               u32 sourceLine) {
  Breakpoint bp(m_nextBreakpointId++, ip);
  bp.sourceFile = sourceFile;
  bp.sourceLine = sourceLine;
  m_breakpoints[bp.id] = bp;
  m_breakpointIPs.insert(ip);
  NOVELMIND_LOG_DEBUG("Added breakpoint " + std::to_string(bp.id) + " at " + sourceFile + ":" + std::to_string(sourceLine) + " (IP " + std::to_string(ip) + ")");
  return bp.id;
}

u32 VMDebugger::addConditionalBreakpoint(u32 ip, const std::string &condition) {
  Breakpoint bp(m_nextBreakpointId++, ip);
  bp.type = BreakpointType::Conditional;
  bp.condition = condition;
  m_breakpoints[bp.id] = bp;
  m_breakpointIPs.insert(ip);
  NOVELMIND_LOG_DEBUG("Added conditional breakpoint " + std::to_string(bp.id) + " at IP " + std::to_string(ip) + " with condition: " + condition);
  return bp.id;
}

u32 VMDebugger::addLogpoint(u32 ip, const std::string &message) {
  Breakpoint bp(m_nextBreakpointId++, ip);
  bp.type = BreakpointType::Logpoint;
  bp.logMessage = message;
  m_breakpoints[bp.id] = bp;
  m_breakpointIPs.insert(ip);
  NOVELMIND_LOG_DEBUG("Added logpoint " + std::to_string(bp.id) + " at IP " + std::to_string(ip) + ": " + message);
  return bp.id;
}

bool VMDebugger::removeBreakpoint(u32 breakpointId) {
  auto it = m_breakpoints.find(breakpointId);
  if (it == m_breakpoints.end()) {
    return false;
  }

  u32 ip = it->second.instructionPointer;
  m_breakpoints.erase(it);

  // Check if any other breakpoints are at the same IP
  bool hasOtherAtIP = false;
  for (const auto &[id, bp] : m_breakpoints) {
    if (bp.instructionPointer == ip) {
      hasOtherAtIP = true;
      break;
    }
  }
  if (!hasOtherAtIP) {
    m_breakpointIPs.erase(ip);
  }

  NOVELMIND_LOG_DEBUG("Removed breakpoint " + std::to_string(breakpointId));
  return true;
}

u32 VMDebugger::removeBreakpointsAt(u32 ip) {
  u32 removed = 0;
  std::vector<u32> toRemove;

  for (const auto &[id, bp] : m_breakpoints) {
    if (bp.instructionPointer == ip) {
      toRemove.push_back(id);
    }
  }

  for (u32 id : toRemove) {
    m_breakpoints.erase(id);
    ++removed;
  }

  if (removed > 0) {
    m_breakpointIPs.erase(ip);
    NOVELMIND_LOG_DEBUG("Removed " + std::to_string(removed) + " breakpoint(s) at IP " + std::to_string(ip));
  }

  return removed;
}

bool VMDebugger::setBreakpointEnabled(u32 breakpointId, bool enabled) {
  auto it = m_breakpoints.find(breakpointId);
  if (it == m_breakpoints.end()) {
    return false;
  }
  it->second.enabled = enabled;
  NOVELMIND_LOG_DEBUG("Breakpoint " + std::to_string(breakpointId) + " " + (enabled ? "enabled" : "disabled"));
  return true;
}

bool VMDebugger::toggleBreakpoint(u32 breakpointId) {
  auto it = m_breakpoints.find(breakpointId);
  if (it == m_breakpoints.end()) {
    return false;
  }
  it->second.enabled = !it->second.enabled;
  return it->second.enabled;
}

bool VMDebugger::hasBreakpointAt(u32 ip) const {
  if (m_breakpointIPs.find(ip) == m_breakpointIPs.end()) {
    return false;
  }
  // Check if any enabled breakpoint exists at this IP
  for (const auto &[id, bp] : m_breakpoints) {
    if (bp.instructionPointer == ip && bp.enabled) {
      return true;
    }
  }
  return false;
}

std::vector<Breakpoint> VMDebugger::getAllBreakpoints() const {
  std::vector<Breakpoint> result;
  result.reserve(m_breakpoints.size());
  for (const auto &[id, bp] : m_breakpoints) {
    result.push_back(bp);
  }
  // Sort by IP for consistent display
  std::sort(result.begin(), result.end(),
            [](const Breakpoint &a, const Breakpoint &b) {
              return a.instructionPointer < b.instructionPointer;
            });
  return result;
}

std::optional<Breakpoint> VMDebugger::getBreakpoint(u32 breakpointId) const {
  auto it = m_breakpoints.find(breakpointId);
  if (it != m_breakpoints.end()) {
    return it->second;
  }
  return std::nullopt;
}

void VMDebugger::clearAllBreakpoints() {
  m_breakpoints.clear();
  m_breakpointIPs.clear();
  NOVELMIND_LOG_DEBUG("Cleared all breakpoints");
}

// =========================================================================
// Execution Control
// =========================================================================

void VMDebugger::continueExecution() {
  m_isPaused = false;
  m_stepMode = DebugStepMode::None;
  if (m_vm) {
    m_vm->resume();
  }
  NOVELMIND_LOG_DEBUG("Continuing execution");
}

void VMDebugger::pause() {
  m_isPaused = true;
  m_stepMode = DebugStepMode::None;
  if (m_vm) {
    m_vm->pause();
  }
  if (m_onExecutionPaused) {
    m_onExecutionPaused(getCurrentIP(), "User requested pause");
  }
  NOVELMIND_LOG_DEBUG("Execution paused at IP " + std::to_string(getCurrentIP()));
}

void VMDebugger::stepInto() {
  m_isPaused = false;
  m_stepMode = DebugStepMode::StepInto;
  m_stepStartDepth = getCallStackDepth();
  if (m_vm) {
    m_vm->resume();
  }
  NOVELMIND_LOG_DEBUG("Step into from IP " + std::to_string(getCurrentIP()));
}

void VMDebugger::stepOver() {
  m_isPaused = false;
  m_stepMode = DebugStepMode::StepOver;
  m_stepStartDepth = getCallStackDepth();
  if (m_vm) {
    m_vm->resume();
  }
  NOVELMIND_LOG_DEBUG("Step over from IP " + std::to_string(getCurrentIP()));
}

void VMDebugger::stepOut() {
  m_isPaused = false;
  m_stepMode = DebugStepMode::StepOut;
  m_stepStartDepth = getCallStackDepth();
  if (m_vm) {
    m_vm->resume();
  }
  NOVELMIND_LOG_DEBUG("Step out from IP " + std::to_string(getCurrentIP()) + " (depth " + std::to_string(m_stepStartDepth) + ")");
}

void VMDebugger::stop() {
  m_isPaused = true;
  m_stepMode = DebugStepMode::None;
  m_callStack.clear();
  if (m_vm) {
    m_vm->reset();
  }
  NOVELMIND_LOG_DEBUG("Execution stopped");
}

// =========================================================================
// State Inspection
// =========================================================================

u32 VMDebugger::getCurrentIP() const {
  return m_vm ? m_vm->getIP() : 0;
}

std::optional<SourceLocation> VMDebugger::getCurrentSourceLocation() const {
  return getSourceLocation(getCurrentIP());
}

std::optional<SourceLocation> VMDebugger::getSourceLocation(u32 ip) const {
  auto it = m_sourceMappings.find(ip);
  if (it != m_sourceMappings.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<CallStackFrame> VMDebugger::getCallStack() const {
  return m_callStack;
}

std::string VMDebugger::getCurrentScene() const {
  return m_currentScene;
}

std::unordered_map<std::string, Value> VMDebugger::getAllVariables() const {
  if (m_vm) {
    return m_vm->getAllVariables();
  }
  return {};
}

std::unordered_map<std::string, bool> VMDebugger::getAllFlags() const {
  if (m_vm) {
    return m_vm->getAllFlags();
  }
  return {};
}

std::vector<VariableChangeEvent>
VMDebugger::getRecentVariableChanges(u32 count) const {
  if (count >= m_variableHistory.size()) {
    return m_variableHistory;
  }
  // Return the most recent changes
  return std::vector<VariableChangeEvent>(
      m_variableHistory.end() - static_cast<std::ptrdiff_t>(count),
      m_variableHistory.end());
}

// =========================================================================
// Source Location Mapping
// =========================================================================

void VMDebugger::setSourceMapping(u32 ip, const SourceLocation &location) {
  m_sourceMappings[ip] = location;
}

void VMDebugger::loadSourceMappings(
    const std::unordered_map<u32, SourceLocation> &mappings) {
  m_sourceMappings = mappings;
  NOVELMIND_LOG_DEBUG("Loaded " + std::to_string(mappings.size()) + " source mappings");
}

void VMDebugger::clearSourceMappings() {
  m_sourceMappings.clear();
}

// =========================================================================
// Variable Modification
// =========================================================================

bool VMDebugger::setVariable(const std::string &name, const Value &value) {
  if (!m_vm) {
    return false;
  }

  Value oldValue = m_vm->getVariable(name);
  m_vm->setVariable(name, value);

  // Track the change
  VariableChangeEvent event;
  event.name = name;
  event.oldValue = oldValue;
  event.newValue = value;
  event.instructionPointer = getCurrentIP();
  if (auto loc = getCurrentSourceLocation()) {
    event.sourceLine = loc->line;
  }
  trackVariableChange(name, oldValue, value);

  NOVELMIND_LOG_DEBUG("Set variable " + name + " during debugging");
  return true;
}

bool VMDebugger::setFlag(const std::string &name, bool value) {
  if (!m_vm) {
    return false;
  }

  m_vm->setFlag(name, value);
  NOVELMIND_LOG_DEBUG("Set flag " + name + " = " + (value ? "true" : "false") + " during debugging");
  return true;
}

// =========================================================================
// Debug Hooks
// =========================================================================

bool VMDebugger::beforeInstruction(u32 ip) {
  // Check step modes first
  if (m_stepMode != DebugStepMode::None) {
    bool shouldPause = false;
    std::string reason;

    switch (m_stepMode) {
    case DebugStepMode::StepInto:
      // Always pause on next instruction
      shouldPause = true;
      reason = "Step into";
      break;

    case DebugStepMode::StepOver:
      // Pause only if we're at or above the starting depth
      if (getCallStackDepth() <= m_stepStartDepth) {
        shouldPause = true;
        reason = "Step over";
      }
      break;

    case DebugStepMode::StepOut:
      // Pause only when we've returned from the starting depth
      if (getCallStackDepth() < m_stepStartDepth) {
        shouldPause = true;
        reason = "Step out";
      }
      break;

    default:
      break;
    }

    if (shouldPause) {
      m_isPaused = true;
      m_stepMode = DebugStepMode::None;
      if (m_onExecutionPaused) {
        m_onExecutionPaused(ip, reason);
      }
      return false; // Don't continue
    }
  }

  // Check breakpoints
  if (m_breakpointIPs.find(ip) != m_breakpointIPs.end()) {
    for (auto &[id, bp] : m_breakpoints) {
      if (bp.instructionPointer == ip && bp.enabled) {
        ++bp.hitCount;

        switch (bp.type) {
        case BreakpointType::Normal:
          m_isPaused = true;
          if (m_onBreakpointHit) {
            m_onBreakpointHit(bp, ip);
          }
          NOVELMIND_LOG_DEBUG("Breakpoint " + std::to_string(bp.id) + " hit at IP " + std::to_string(ip));
          return false; // Don't continue

        case BreakpointType::Conditional:
          if (evaluateCondition(bp.condition)) {
            m_isPaused = true;
            if (m_onBreakpointHit) {
              m_onBreakpointHit(bp, ip);
            }
            NOVELMIND_LOG_DEBUG("Conditional breakpoint " + std::to_string(bp.id) + " hit at IP " + std::to_string(ip) + " (condition: " + bp.condition + ")");
            return false; // Don't continue
          }
          break;

        case BreakpointType::Logpoint: {
          std::string message = formatLogpointMessage(bp.logMessage);
          if (m_onLogpointTriggered) {
            m_onLogpointTriggered(message, ip);
          }
          NOVELMIND_LOG_INFO("[Logpoint] " + message);
          // Don't pause for logpoints
          break;
        }
        }
      }
    }
  }

  return !m_isPaused; // Continue if not paused
}

void VMDebugger::afterInstruction(u32 /*ip*/) {
  // Called after instruction execution
  // Can be used for post-instruction analysis if needed
}

void VMDebugger::trackVariableChange(const std::string &name,
                                     const Value &oldValue,
                                     const Value &newValue) {
  VariableChangeEvent event;
  event.name = name;
  event.oldValue = oldValue;
  event.newValue = newValue;
  event.instructionPointer = getCurrentIP();
  if (auto loc = getCurrentSourceLocation()) {
    event.sourceLine = loc->line;
  }

  m_variableHistory.push_back(event);

  // Limit history size
  if (m_variableHistory.size() > MAX_VARIABLE_HISTORY) {
    m_variableHistory.erase(m_variableHistory.begin());
  }

  if (m_onVariableChanged) {
    m_onVariableChanged(event);
  }
}

void VMDebugger::notifySceneEntered(const std::string &sceneName,
                                    u32 returnAddress) {
  CallStackFrame frame;
  frame.sceneName = sceneName;
  frame.instructionPointer = getCurrentIP();
  frame.returnAddress = returnAddress;
  if (auto loc = getCurrentSourceLocation()) {
    frame.sourceFile = loc->filePath;
    frame.sourceLine = loc->line;
  }

  m_callStack.push_back(frame);
  m_currentScene = sceneName;

  if (m_onSceneEntered) {
    m_onSceneEntered(sceneName);
  }

  NOVELMIND_LOG_DEBUG("Entered scene: " + sceneName + " (stack depth: " + std::to_string(m_callStack.size()) + ")");
}

void VMDebugger::notifySceneExited(const std::string &sceneName) {
  if (!m_callStack.empty() && m_callStack.back().sceneName == sceneName) {
    m_callStack.pop_back();
  }

  // Update current scene
  m_currentScene = m_callStack.empty() ? "" : m_callStack.back().sceneName;

  if (m_onSceneExited) {
    m_onSceneExited(sceneName);
  }

  NOVELMIND_LOG_DEBUG("Exited scene: " + sceneName + " (stack depth: " + std::to_string(m_callStack.size()) + ")");
}

// =========================================================================
// Private Helpers
// =========================================================================

bool VMDebugger::evaluateCondition(const std::string &condition) const {
  if (!m_vm || condition.empty()) {
    return false;
  }

  // Simple condition evaluation for common patterns:
  // - "variable_name > value"
  // - "variable_name == value"
  // - "variable_name < value"
  // - "flag_name" (boolean check)

  // Try to parse as "variable op value"
  static const std::regex simpleCompare(
      R"((\w+)\s*(==|!=|<|<=|>|>=)\s*(-?\d+(?:\.\d+)?|true|false|"[^"]*"))");

  std::smatch match;
  if (std::regex_match(condition, match, simpleCompare)) {
    std::string varName = match[1].str();
    std::string op = match[2].str();
    std::string valueStr = match[3].str();

    Value varValue = m_vm->getVariable(varName);

    // Parse the comparison value
    Value compareValue;
    if (valueStr == "true") {
      compareValue = true;
    } else if (valueStr == "false") {
      compareValue = false;
    } else if (valueStr.front() == '"') {
      // String value - remove quotes
      compareValue = valueStr.substr(1, valueStr.length() - 2);
    } else if (valueStr.find('.') != std::string::npos) {
      compareValue = std::stof(valueStr);
    } else {
      compareValue = std::stoi(valueStr);
    }

    // Perform comparison
    if (op == "==" || op == "==") {
      return asFloat(varValue) == asFloat(compareValue);
    } else if (op == "!=") {
      return asFloat(varValue) != asFloat(compareValue);
    } else if (op == "<") {
      return asFloat(varValue) < asFloat(compareValue);
    } else if (op == "<=") {
      return asFloat(varValue) <= asFloat(compareValue);
    } else if (op == ">") {
      return asFloat(varValue) > asFloat(compareValue);
    } else if (op == ">=") {
      return asFloat(varValue) >= asFloat(compareValue);
    }
  }

  // Check if it's just a flag name
  if (m_vm->getFlag(condition)) {
    return true;
  }

  // Try as a variable name (truthy check)
  Value val = m_vm->getVariable(condition);
  return asBool(val);
}

std::string
VMDebugger::formatLogpointMessage(const std::string &message) const {
  if (!m_vm) {
    return message;
  }

  std::string result = message;

  // Replace {variable_name} with actual values
  static const std::regex varPattern(R"(\{(\w+)\})");

  std::smatch match;
  std::string::const_iterator searchStart(result.cbegin());
  std::string formatted;
  size_t lastPos = 0;

  while (std::regex_search(searchStart, result.cend(), match, varPattern)) {
    // Append text before the match
    size_t matchPos =
        static_cast<size_t>(match.position()) + (searchStart - result.cbegin());
    formatted += result.substr(lastPos, matchPos - lastPos);

    // Get variable value
    std::string varName = match[1].str();
    Value val = m_vm->getVariable(varName);
    formatted += asString(val);

    lastPos = matchPos + match.length();
    searchStart = match.suffix().first;
  }

  // Append remaining text
  formatted += result.substr(lastPos);

  return formatted;
}

} // namespace NovelMind::scripting
