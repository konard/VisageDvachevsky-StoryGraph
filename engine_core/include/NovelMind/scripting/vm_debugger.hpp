#pragma once

/**
 * @file vm_debugger.hpp
 * @brief Virtual Machine debugging interface for script runtime inspection
 *
 * Provides debugging capabilities for the scripting VM including:
 * - Breakpoint management (set, remove, enable/disable)
 * - Step debugging (step into, over, out)
 * - Call stack inspection
 * - Variable/flag change tracking
 * - Source location mapping
 *
 * This file is part of Issue #244: Add Script Runtime Inspector and Debugger Panel
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/value.hpp"
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::scripting {

// Forward declaration
class VirtualMachine;

/**
 * @brief Debug stepping mode for execution control
 */
enum class DebugStepMode : u8 {
  None,     ///< Normal execution, no stepping
  StepInto, ///< Step into function calls
  StepOver, ///< Step over function calls (execute them as one step)
  StepOut   ///< Step out of current function/scene
};

/**
 * @brief Breakpoint type for different debugging scenarios
 */
enum class BreakpointType : u8 {
  Normal,      ///< Regular breakpoint - always stops
  Conditional, ///< Stops only when condition is true
  Logpoint     ///< Logs message without stopping
};

/**
 * @brief Represents a single call stack frame
 */
struct CallStackFrame {
  std::string sceneName;      ///< Name of the scene/function
  u32 instructionPointer;     ///< IP where the call was made
  u32 returnAddress;          ///< IP to return to after call
  std::string sourceFile;     ///< Source file path (if available)
  u32 sourceLine;             ///< Source line number (if available)
  std::unordered_map<std::string, Value> localVariables; ///< Local variables in this frame
};

/**
 * @brief Represents a breakpoint with optional conditions
 */
struct Breakpoint {
  u32 id;                     ///< Unique breakpoint identifier
  u32 instructionPointer;     ///< IP where breakpoint is set
  BreakpointType type;        ///< Type of breakpoint
  bool enabled;               ///< Whether breakpoint is active
  std::string condition;      ///< Condition expression (for conditional breakpoints)
  std::string logMessage;     ///< Log message (for logpoints)
  std::string sourceFile;     ///< Source file (for display)
  u32 sourceLine;             ///< Source line number (for display)
  u32 hitCount;               ///< Number of times this breakpoint was hit

  Breakpoint()
      : id(0), instructionPointer(0), type(BreakpointType::Normal),
        enabled(true), sourceLine(0), hitCount(0) {}

  Breakpoint(u32 bp_id, u32 ip)
      : id(bp_id), instructionPointer(ip), type(BreakpointType::Normal),
        enabled(true), sourceLine(0), hitCount(0) {}
};

/**
 * @brief Source location mapping from IP to source code
 */
struct SourceLocation {
  std::string filePath;  ///< Path to source file
  u32 line;              ///< Line number (1-based)
  u32 column;            ///< Column number (1-based)
  std::string sceneName; ///< Scene name at this location

  SourceLocation() : line(0), column(0) {}
  SourceLocation(const std::string &path, u32 l, u32 c = 1)
      : filePath(path), line(l), column(c) {}

  bool isValid() const { return line > 0; }
};

/**
 * @brief Variable change event for tracking state modifications
 */
struct VariableChangeEvent {
  std::string name;      ///< Variable name
  Value oldValue;        ///< Previous value
  Value newValue;        ///< New value
  u32 instructionPointer;///< IP where change occurred
  u32 sourceLine;        ///< Source line where change occurred
};

/**
 * @brief VM Debugger class for script debugging
 *
 * This class wraps a VirtualMachine and provides debugging capabilities.
 * It intercepts execution to check breakpoints and handle step debugging.
 */
class VMDebugger {
public:
  // Callback types for debugging events
  using BreakpointHitCallback = std::function<void(const Breakpoint &bp, u32 ip)>;
  using ExecutionPausedCallback = std::function<void(u32 ip, const std::string &reason)>;
  using VariableChangedCallback = std::function<void(const VariableChangeEvent &event)>;
  using SceneEnteredCallback = std::function<void(const std::string &sceneName)>;
  using SceneExitedCallback = std::function<void(const std::string &sceneName)>;
  using LogpointTriggeredCallback = std::function<void(const std::string &message, u32 ip)>;

  /**
   * @brief Construct a debugger for a VM
   * @param vm The virtual machine to debug (must outlive the debugger)
   */
  explicit VMDebugger(VirtualMachine *vm);
  ~VMDebugger();

  // =========================================================================
  // Breakpoint Management
  // =========================================================================

  /**
   * @brief Add a breakpoint at the specified instruction pointer
   * @param ip Instruction pointer to break at
   * @return Breakpoint ID
   */
  u32 addBreakpoint(u32 ip);

  /**
   * @brief Add a breakpoint with source location info
   * @param ip Instruction pointer
   * @param sourceFile Source file path
   * @param sourceLine Source line number
   * @return Breakpoint ID
   */
  u32 addBreakpoint(u32 ip, const std::string &sourceFile, u32 sourceLine);

  /**
   * @brief Add a conditional breakpoint
   * @param ip Instruction pointer
   * @param condition Condition expression (e.g., "hero_trust > 50")
   * @return Breakpoint ID
   */
  u32 addConditionalBreakpoint(u32 ip, const std::string &condition);

  /**
   * @brief Add a logpoint (logs without stopping)
   * @param ip Instruction pointer
   * @param message Message to log (can include {variable} placeholders)
   * @return Breakpoint ID
   */
  u32 addLogpoint(u32 ip, const std::string &message);

  /**
   * @brief Remove a breakpoint by ID
   * @param breakpointId The breakpoint to remove
   * @return true if removed, false if not found
   */
  bool removeBreakpoint(u32 breakpointId);

  /**
   * @brief Remove all breakpoints at a specific IP
   * @param ip Instruction pointer
   * @return Number of breakpoints removed
   */
  u32 removeBreakpointsAt(u32 ip);

  /**
   * @brief Enable or disable a breakpoint
   * @param breakpointId The breakpoint to toggle
   * @param enabled Whether to enable or disable
   * @return true if successful
   */
  bool setBreakpointEnabled(u32 breakpointId, bool enabled);

  /**
   * @brief Toggle breakpoint enabled state
   * @param breakpointId The breakpoint to toggle
   * @return New enabled state
   */
  bool toggleBreakpoint(u32 breakpointId);

  /**
   * @brief Check if there's a breakpoint at the given IP
   * @param ip Instruction pointer to check
   * @return true if breakpoint exists and is enabled
   */
  [[nodiscard]] bool hasBreakpointAt(u32 ip) const;

  /**
   * @brief Get all breakpoints
   * @return Vector of all breakpoints
   */
  [[nodiscard]] std::vector<Breakpoint> getAllBreakpoints() const;

  /**
   * @brief Get breakpoint by ID
   * @param breakpointId The breakpoint ID
   * @return Optional breakpoint if found
   */
  [[nodiscard]] std::optional<Breakpoint> getBreakpoint(u32 breakpointId) const;

  /**
   * @brief Clear all breakpoints
   */
  void clearAllBreakpoints();

  // =========================================================================
  // Execution Control
  // =========================================================================

  /**
   * @brief Continue execution until next breakpoint
   */
  void continueExecution();

  /**
   * @brief Pause execution at current instruction
   */
  void pause();

  /**
   * @brief Step into the next instruction (including function calls)
   */
  void stepInto();

  /**
   * @brief Step over the next instruction (execute functions as one step)
   */
  void stepOver();

  /**
   * @brief Step out of the current function/scene
   */
  void stepOut();

  /**
   * @brief Stop execution completely
   */
  void stop();

  /**
   * @brief Check if execution is paused
   */
  [[nodiscard]] bool isPaused() const { return m_isPaused; }

  /**
   * @brief Get current step mode
   */
  [[nodiscard]] DebugStepMode getStepMode() const { return m_stepMode; }

  // =========================================================================
  // State Inspection
  // =========================================================================

  /**
   * @brief Get the current instruction pointer
   */
  [[nodiscard]] u32 getCurrentIP() const;

  /**
   * @brief Get the current source location
   * @return Source location if mapping exists
   */
  [[nodiscard]] std::optional<SourceLocation> getCurrentSourceLocation() const;

  /**
   * @brief Get source location for a given IP
   * @param ip Instruction pointer
   * @return Source location if mapping exists
   */
  [[nodiscard]] std::optional<SourceLocation> getSourceLocation(u32 ip) const;

  /**
   * @brief Get the call stack
   * @return Vector of call stack frames (most recent first)
   */
  [[nodiscard]] std::vector<CallStackFrame> getCallStack() const;

  /**
   * @brief Get current call stack depth
   */
  [[nodiscard]] u32 getCallStackDepth() const {
    return static_cast<u32>(m_callStack.size());
  }

  /**
   * @brief Get the current scene name
   */
  [[nodiscard]] std::string getCurrentScene() const;

  /**
   * @brief Get all variables with their current values
   */
  [[nodiscard]] std::unordered_map<std::string, Value> getAllVariables() const;

  /**
   * @brief Get all flags with their current values
   */
  [[nodiscard]] std::unordered_map<std::string, bool> getAllFlags() const;

  /**
   * @brief Get recent variable changes
   * @param count Maximum number of changes to return
   * @return Vector of recent variable change events
   */
  [[nodiscard]] std::vector<VariableChangeEvent> getRecentVariableChanges(u32 count = 10) const;

  // =========================================================================
  // Source Location Mapping
  // =========================================================================

  /**
   * @brief Set source location mapping for an instruction
   * @param ip Instruction pointer
   * @param location Source location
   */
  void setSourceMapping(u32 ip, const SourceLocation &location);

  /**
   * @brief Load source mappings from compiled script metadata
   * @param mappings Map of IP to source location
   */
  void loadSourceMappings(const std::unordered_map<u32, SourceLocation> &mappings);

  /**
   * @brief Clear all source mappings
   */
  void clearSourceMappings();

  // =========================================================================
  // Variable Modification (Hot Reload)
  // =========================================================================

  /**
   * @brief Set a variable value during debugging
   * @param name Variable name
   * @param value New value
   * @return true if successful
   */
  bool setVariable(const std::string &name, const Value &value);

  /**
   * @brief Set a flag value during debugging
   * @param name Flag name
   * @param value New value
   * @return true if successful
   */
  bool setFlag(const std::string &name, bool value);

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setBreakpointHitCallback(BreakpointHitCallback callback) {
    m_onBreakpointHit = std::move(callback);
  }

  void setExecutionPausedCallback(ExecutionPausedCallback callback) {
    m_onExecutionPaused = std::move(callback);
  }

  void setVariableChangedCallback(VariableChangedCallback callback) {
    m_onVariableChanged = std::move(callback);
  }

  void setSceneEnteredCallback(SceneEnteredCallback callback) {
    m_onSceneEntered = std::move(callback);
  }

  void setSceneExitedCallback(SceneExitedCallback callback) {
    m_onSceneExited = std::move(callback);
  }

  void setLogpointTriggeredCallback(LogpointTriggeredCallback callback) {
    m_onLogpointTriggered = std::move(callback);
  }

  // =========================================================================
  // Debug Hook (called by VM)
  // =========================================================================

  /**
   * @brief Called before each instruction execution
   * @param ip Current instruction pointer
   * @return true to continue, false to pause
   */
  bool beforeInstruction(u32 ip);

  /**
   * @brief Called after each instruction execution
   * @param ip Instruction pointer that was executed
   */
  void afterInstruction(u32 ip);

  /**
   * @brief Track variable change (called by VM)
   * @param name Variable name
   * @param oldValue Previous value
   * @param newValue New value
   */
  void trackVariableChange(const std::string &name, const Value &oldValue,
                           const Value &newValue);

  /**
   * @brief Notify scene entry (called by VM)
   * @param sceneName Scene being entered
   * @param returnAddress IP to return to
   */
  void notifySceneEntered(const std::string &sceneName, u32 returnAddress);

  /**
   * @brief Notify scene exit (called by VM)
   * @param sceneName Scene being exited
   */
  void notifySceneExited(const std::string &sceneName);

private:
  /**
   * @brief Evaluate a condition expression
   * @param condition The condition string
   * @return true if condition evaluates to true
   */
  bool evaluateCondition(const std::string &condition) const;

  /**
   * @brief Format a logpoint message (substitute {variables})
   * @param message The message template
   * @return Formatted message
   */
  std::string formatLogpointMessage(const std::string &message) const;

  VirtualMachine *m_vm;                              ///< Associated VM
  std::unordered_map<u32, Breakpoint> m_breakpoints; ///< All breakpoints by ID
  std::set<u32> m_breakpointIPs;                     ///< Set of IPs with breakpoints (for fast lookup)
  std::unordered_map<u32, SourceLocation> m_sourceMappings; ///< IP to source location mapping
  std::vector<CallStackFrame> m_callStack;           ///< Current call stack
  std::vector<VariableChangeEvent> m_variableHistory;///< Recent variable changes
  u32 m_nextBreakpointId;                            ///< Next breakpoint ID to assign
  bool m_isPaused;                                   ///< Whether execution is paused
  DebugStepMode m_stepMode;                          ///< Current step mode
  u32 m_stepStartDepth;                              ///< Call stack depth when step started
  std::string m_currentScene;                        ///< Current scene name
  static constexpr u32 MAX_VARIABLE_HISTORY = 100;   ///< Max variable changes to track

  // Callbacks
  BreakpointHitCallback m_onBreakpointHit;
  ExecutionPausedCallback m_onExecutionPaused;
  VariableChangedCallback m_onVariableChanged;
  SceneEnteredCallback m_onSceneEntered;
  SceneExitedCallback m_onSceneExited;
  LogpointTriggeredCallback m_onLogpointTriggered;
};

} // namespace NovelMind::scripting
