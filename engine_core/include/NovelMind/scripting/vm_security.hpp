#pragma once

#include "NovelMind/core/types.hpp"
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace NovelMind::scripting {

struct VMSecurityLimits {
  usize maxStackSize = 1024;
  usize maxCallDepth = 64;
  usize maxInstructionsPerStep = 10000;
  usize maxStringLength = 65536;
  usize maxVariables = 1024;
  usize maxLoopIterations = 100000;
  bool allowNativeCalls = true;
  bool allowFileAccess = false;
  bool allowNetworkAccess = false;
};

enum class SecurityViolationType {
  StackOverflow,
  CallDepthExceeded,
  InstructionLimitExceeded,
  StringTooLong,
  VariableLimitExceeded,
  InfiniteLoopDetected,
  UnauthorizedNativeCall,
  UnauthorizedFileAccess,
  UnauthorizedNetworkAccess,
  InvalidMemoryAccess,
  InvalidOpcode,
  DivisionByZero,
  TypeMismatch
};

struct SecurityViolation {
  SecurityViolationType type;
  std::string message;
  u32 instructionPointer = 0;
  std::string context;
};

class VMSecurityGuard {
public:
  VMSecurityGuard() = default;
  explicit VMSecurityGuard(const VMSecurityLimits& limits);

  void setLimits(const VMSecurityLimits& limits) { m_limits = limits; }
  [[nodiscard]] const VMSecurityLimits& limits() const { return m_limits; }

  void reset();

  bool checkStackPush(usize currentSize);
  bool checkCallDepth(usize currentDepth);
  bool checkInstructionCount();
  bool checkStringLength(usize length);
  bool checkVariableCount(usize count);
  bool checkLoopIteration(u32 loopId);
  bool checkNativeCall(const std::string& functionName);

  void registerLoopEntry(u32 loopId);
  void registerLoopExit(u32 loopId);

  void addAllowedNativeFunction(const std::string& name);
  void removeAllowedNativeFunction(const std::string& name);
  void clearAllowedNativeFunctions();

  [[nodiscard]] bool hasViolation() const { return !m_violations.empty(); }
  [[nodiscard]] const std::vector<SecurityViolation>& violations() const { return m_violations; }
  [[nodiscard]] const SecurityViolation* lastViolation() const;

  void clearViolations() { m_violations.clear(); }

  using ViolationCallback = std::function<void(const SecurityViolation&)>;
  void setViolationCallback(ViolationCallback callback) { m_callback = std::move(callback); }

private:
  void recordViolation(SecurityViolationType type, const std::string& message);

  VMSecurityLimits m_limits;
  std::vector<SecurityViolation> m_violations;
  ViolationCallback m_callback;

  usize m_instructionCount = 0;
  std::unordered_map<u32, usize> m_loopIterations;
  std::unordered_set<std::string> m_allowedNativeFunctions;

  u32 m_currentIp = 0;
};

class VMSandbox {
public:
  VMSandbox() = default;

  void setSecurityGuard(VMSecurityGuard* guard) { m_guard = guard; }
  [[nodiscard]] VMSecurityGuard* securityGuard() const { return m_guard; }

  void setAllowedResourcePaths(const std::vector<std::string>& paths);
  [[nodiscard]] bool isResourcePathAllowed(const std::string& path) const;

  void setMaxMemoryUsage(usize bytes) { m_maxMemory = bytes; }
  [[nodiscard]] usize maxMemoryUsage() const { return m_maxMemory; }

  bool allocateMemory(usize bytes);
  void freeMemory(usize bytes);
  [[nodiscard]] usize currentMemoryUsage() const { return m_currentMemory; }

private:
  VMSecurityGuard* m_guard = nullptr;
  std::vector<std::string> m_allowedResourcePaths;
  usize m_maxMemory = 64 * 1024 * 1024;
  usize m_currentMemory = 0;
};

} // namespace NovelMind::scripting
