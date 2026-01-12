#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/opcode.hpp"
#include "NovelMind/scripting/value.hpp"
#include "NovelMind/scripting/vm_security.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::scripting {

// Forward declaration for debugger integration
class VMDebugger;

class VirtualMachine {
public:
  using NativeCallback = std::function<void(const std::vector<Value> &)>;

  VirtualMachine();
  ~VirtualMachine();

  Result<void> load(const std::vector<Instruction> &program,
                    const std::vector<std::string> &stringTable);
  void reset();

  bool step();
  void run();
  void pause();
  void resume();

  [[nodiscard]] bool isRunning() const;
  [[nodiscard]] bool isPaused() const;
  [[nodiscard]] bool isWaiting() const;
  [[nodiscard]] bool isHalted() const;
  [[nodiscard]] u32 getIP() const { return m_ip; }
  [[nodiscard]] u32 getProgramSize() const {
    return static_cast<u32>(m_program.size());
  }
  void setIP(u32 ip);

  void setVariable(const std::string &name, Value value);
  [[nodiscard]] Value getVariable(const std::string &name) const;
  [[nodiscard]] bool hasVariable(const std::string &name) const;
  [[nodiscard]] std::unordered_map<std::string, Value> getAllVariables() const {
    return m_variables;
  }

  void setFlag(const std::string &name, bool value);
  [[nodiscard]] bool getFlag(const std::string &name) const;
  [[nodiscard]] std::unordered_map<std::string, bool> getAllFlags() const {
    return m_flags;
  }

  void registerCallback(OpCode op, NativeCallback callback);

  void signalContinue();
  void signalChoice(i32 choice);

  // =========================================================================
  // Debugger Integration
  // =========================================================================

  /**
   * @brief Attach a debugger to this VM
   * @param debugger Pointer to debugger (ownership not transferred)
   */
  void attachDebugger(VMDebugger *debugger);

  /**
   * @brief Detach the current debugger
   */
  void detachDebugger();

  /**
   * @brief Check if a debugger is attached
   */
  [[nodiscard]] bool hasDebugger() const { return m_debugger != nullptr; }

  /**
   * @brief Get the attached debugger
   */
  [[nodiscard]] VMDebugger *debugger() const { return m_debugger; }

  /**
   * @brief Get the current instruction at IP (for debugging display)
   * @return Pointer to instruction, or nullptr if invalid
   */
  [[nodiscard]] const Instruction *getCurrentInstruction() const {
    if (m_ip < m_program.size()) {
      return &m_program[m_ip];
    }
    return nullptr;
  }

  /**
   * @brief Get instruction at specified IP
   * @param ip Instruction pointer
   * @return Pointer to instruction, or nullptr if invalid
   */
  [[nodiscard]] const Instruction *getInstructionAt(u32 ip) const {
    if (ip < m_program.size()) {
      return &m_program[ip];
    }
    return nullptr;
  }

  /**
   * @brief Get current stack contents (for debugging)
   */
  [[nodiscard]] const std::vector<Value> &getStack() const { return m_stack; }

  /**
   * @brief Get string from string table (for debugging)
   */
  [[nodiscard]] std::string getStringAt(u32 index) const {
    if (index < m_stringTable.size()) {
      return m_stringTable[index];
    }
    return "";
  }

  [[nodiscard]] VMSecurityGuard &securityGuard() { return m_securityGuard; }
  [[nodiscard]] const VMSecurityGuard &securityGuard() const {
    return m_securityGuard;
  }

private:
  void executeInstruction(const Instruction &instr);
  void push(Value value);
  Value pop();
  [[nodiscard]] bool ensureStack(size_t required);
  [[nodiscard]] const std::string &getString(u32 index) const;

  std::vector<Instruction> m_program;
  std::vector<std::string> m_stringTable;
  std::vector<Value> m_stack;
  std::unordered_map<std::string, Value> m_variables;
  std::unordered_map<std::string, bool> m_flags;
  std::unordered_map<OpCode, NativeCallback> m_callbacks;

  VMSecurityGuard m_securityGuard;

  u32 m_ip;
  bool m_running;
  bool m_paused;
  bool m_waiting;
  mutable bool m_halted;  // mutable to allow setting in const getString() on error
  bool m_skipNextIncrement; // Used for JUMP to address 0
  i32 m_choiceResult;

  // Debugger integration
  VMDebugger *m_debugger = nullptr; ///< Attached debugger (not owned)
};

} // namespace NovelMind::scripting
