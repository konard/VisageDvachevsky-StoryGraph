#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/vm.hpp"
#include <memory>

namespace NovelMind::scripting {

class ScriptInterpreter {
public:
  ScriptInterpreter();
  ~ScriptInterpreter();

  Result<void> loadFromBytecode(const std::vector<u8>& bytecode);
  void reset();

  bool step();
  void run();
  void pause();
  void resume();

  [[nodiscard]] bool isRunning() const;
  [[nodiscard]] bool isPaused() const;
  [[nodiscard]] bool isWaiting() const;

  void setVariable(const std::string& name, i32 value);
  void setVariable(const std::string& name, f32 value);
  void setVariable(const std::string& name, const std::string& value);
  void setVariable(const std::string& name, bool value);

  [[nodiscard]] std::optional<i32> getIntVariable(const std::string& name) const;
  [[nodiscard]] std::optional<f32> getFloatVariable(const std::string& name) const;
  [[nodiscard]] std::optional<std::string> getStringVariable(const std::string& name) const;
  [[nodiscard]] std::optional<bool> getBoolVariable(const std::string& name) const;

  void setFlag(const std::string& name, bool value);
  [[nodiscard]] bool getFlag(const std::string& name) const;

  void signalContinue();
  void signalChoice(i32 choice);

  void registerCallback(OpCode op, VirtualMachine::NativeCallback callback);

private:
  std::unique_ptr<VirtualMachine> m_vm;
};

} // namespace NovelMind::scripting
