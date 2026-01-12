#include "NovelMind/scripting/vm_security.hpp"
#include <algorithm>

namespace NovelMind::scripting {

VMSecurityGuard::VMSecurityGuard(const VMSecurityLimits& limits) : m_limits(limits) {}

void VMSecurityGuard::reset() {
  m_violations.clear();
  m_instructionCount = 0;
  m_loopIterations.clear();
  m_currentIp = 0;
}

bool VMSecurityGuard::checkStackPush(usize currentSize) {
  if (currentSize >= m_limits.maxStackSize) {
    recordViolation(SecurityViolationType::StackOverflow,
                    "Stack overflow: attempted to push when stack size is " +
                        std::to_string(currentSize) +
                        " (limit: " + std::to_string(m_limits.maxStackSize) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkCallDepth(usize currentDepth) {
  if (currentDepth >= m_limits.maxCallDepth) {
    recordViolation(SecurityViolationType::CallDepthExceeded,
                    "Call depth exceeded: " + std::to_string(currentDepth) +
                        " (limit: " + std::to_string(m_limits.maxCallDepth) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkInstructionCount() {
  ++m_instructionCount;
  if (m_instructionCount > m_limits.maxInstructionsPerStep) {
    recordViolation(SecurityViolationType::InstructionLimitExceeded,
                    "Instruction limit exceeded: " + std::to_string(m_instructionCount) +
                        " (limit: " + std::to_string(m_limits.maxInstructionsPerStep) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkStringLength(usize length) {
  if (length > m_limits.maxStringLength) {
    recordViolation(SecurityViolationType::StringTooLong,
                    "String too long: " + std::to_string(length) +
                        " (limit: " + std::to_string(m_limits.maxStringLength) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkVariableCount(usize count) {
  if (count >= m_limits.maxVariables) {
    recordViolation(SecurityViolationType::VariableLimitExceeded,
                    "Variable limit exceeded: " + std::to_string(count) +
                        " (limit: " + std::to_string(m_limits.maxVariables) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkLoopIteration(u32 loopId) {
  auto& count = m_loopIterations[loopId];
  ++count;

  if (count > m_limits.maxLoopIterations) {
    recordViolation(SecurityViolationType::InfiniteLoopDetected,
                    "Possible infinite loop detected at loop " + std::to_string(loopId) + ": " +
                        std::to_string(count) +
                        " iterations (limit: " + std::to_string(m_limits.maxLoopIterations) + ")");
    return false;
  }
  return true;
}

bool VMSecurityGuard::checkNativeCall(const std::string& functionName) {
  if (!m_limits.allowNativeCalls) {
    recordViolation(SecurityViolationType::UnauthorizedNativeCall, "Native calls are disabled");
    return false;
  }

  if (!m_allowedNativeFunctions.empty() &&
      m_allowedNativeFunctions.find(functionName) == m_allowedNativeFunctions.end()) {
    recordViolation(SecurityViolationType::UnauthorizedNativeCall,
                    "Unauthorized native call: " + functionName);
    return false;
  }

  return true;
}

void VMSecurityGuard::registerLoopEntry(u32 loopId) {
  if (m_loopIterations.find(loopId) == m_loopIterations.end()) {
    m_loopIterations[loopId] = 0;
  }
}

void VMSecurityGuard::registerLoopExit(u32 loopId) {
  m_loopIterations.erase(loopId);
}

void VMSecurityGuard::addAllowedNativeFunction(const std::string& name) {
  m_allowedNativeFunctions.insert(name);
}

void VMSecurityGuard::removeAllowedNativeFunction(const std::string& name) {
  m_allowedNativeFunctions.erase(name);
}

void VMSecurityGuard::clearAllowedNativeFunctions() {
  m_allowedNativeFunctions.clear();
}

const SecurityViolation* VMSecurityGuard::lastViolation() const {
  if (m_violations.empty()) {
    return nullptr;
  }
  return &m_violations.back();
}

void VMSecurityGuard::recordViolation(SecurityViolationType type, const std::string& message) {
  SecurityViolation violation;
  violation.type = type;
  violation.message = message;
  violation.instructionPointer = m_currentIp;

  m_violations.push_back(violation);

  if (m_callback) {
    m_callback(violation);
  }
}

void VMSandbox::setAllowedResourcePaths(const std::vector<std::string>& paths) {
  m_allowedResourcePaths = paths;
}

bool VMSandbox::isResourcePathAllowed(const std::string& path) const {
  if (m_allowedResourcePaths.empty()) {
    return true;
  }

  for (const auto& allowedPath : m_allowedResourcePaths) {
    if (path.find(allowedPath) == 0) {
      return true;
    }
  }

  return false;
}

bool VMSandbox::allocateMemory(usize bytes) {
  if (m_currentMemory + bytes > m_maxMemory) {
    return false;
  }

  m_currentMemory += bytes;
  return true;
}

void VMSandbox::freeMemory(usize bytes) {
  if (bytes > m_currentMemory) {
    m_currentMemory = 0;
  } else {
    m_currentMemory -= bytes;
  }
}

} // namespace NovelMind::scripting
