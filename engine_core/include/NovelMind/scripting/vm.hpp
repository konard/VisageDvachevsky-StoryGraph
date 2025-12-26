#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/opcode.hpp"
#include "NovelMind/scripting/value.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::scripting {

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
  [[nodiscard]] u32 getProgramSize() const { return static_cast<u32>(m_program.size()); }
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

private:
  void executeInstruction(const Instruction &instr);
  void push(Value value);
  Value pop();
  [[nodiscard]] const std::string &getString(u32 index) const;

  std::vector<Instruction> m_program;
  std::vector<std::string> m_stringTable;
  std::vector<Value> m_stack;
  std::unordered_map<std::string, Value> m_variables;
  std::unordered_map<std::string, bool> m_flags;
  std::unordered_map<OpCode, NativeCallback> m_callbacks;

  u32 m_ip;
  bool m_running;
  bool m_paused;
  bool m_waiting;
  bool m_halted;
  i32 m_choiceResult;
};

} // namespace NovelMind::scripting
