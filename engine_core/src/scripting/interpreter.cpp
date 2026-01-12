#include "NovelMind/scripting/interpreter.hpp"
#include "NovelMind/core/logger.hpp"
#include <cstring>
#include <limits>

namespace NovelMind::scripting {

constexpr u32 SCRIPT_MAGIC = 0x43534D4E; // "NMSC"

ScriptInterpreter::ScriptInterpreter() : m_vm(std::make_unique<VirtualMachine>()) {}

ScriptInterpreter::~ScriptInterpreter() = default;

Result<void> ScriptInterpreter::loadFromBytecode(const std::vector<u8>& bytecode) {
  // Header structure: magic(4) + version(2) + flags(2) + instrCount(4) +
  // constPool(4) + stringCount(4) + symbolTable(4) = 24 bytes
  constexpr usize HEADER_SIZE = 24;

  if (bytecode.size() < HEADER_SIZE) {
    return Result<void>::error("Bytecode too small for header");
  }

  usize offset = 0;

  // Read magic
  u32 magic;
  if (offset + sizeof(u32) > bytecode.size()) {
    return Result<void>::error("Bytecode truncated at magic");
  }
  std::memcpy(&magic, bytecode.data() + offset, sizeof(u32));
  offset += sizeof(u32);

  if (magic != SCRIPT_MAGIC) {
    return Result<void>::error("Invalid script magic");
  }

  // Read version
  u16 version;
  if (offset + sizeof(u16) > bytecode.size()) {
    return Result<void>::error("Bytecode truncated at version");
  }
  std::memcpy(&version, bytecode.data() + offset, sizeof(u16));
  offset += sizeof(u16);

  // Validate version (currently only version 1 is supported)
  constexpr u16 SUPPORTED_VERSION = 1;
  if (version > SUPPORTED_VERSION) {
    return Result<void>::error("Unsupported bytecode version: " + std::to_string(version));
  }

  offset += sizeof(u16); // Skip flags

  // Read instruction count
  u32 instrCount;
  if (offset + sizeof(u32) > bytecode.size()) {
    return Result<void>::error("Bytecode truncated at instruction count");
  }
  std::memcpy(&instrCount, bytecode.data() + offset, sizeof(u32));
  offset += sizeof(u32);

  // Security: Validate instruction count to prevent excessive allocations
  constexpr u32 MAX_INSTRUCTION_COUNT = 10000000; // 10 million instructions max
  if (instrCount > MAX_INSTRUCTION_COUNT) {
    return Result<void>::error("Instruction count exceeds maximum allowed");
  }

  // Read constant pool size (skip for now)
  offset += sizeof(u32);

  // Read string table size
  u32 stringCount;
  if (offset + sizeof(u32) > bytecode.size()) {
    return Result<void>::error("Bytecode truncated at string count");
  }
  std::memcpy(&stringCount, bytecode.data() + offset, sizeof(u32));
  offset += sizeof(u32);

  // Security: Validate string count to prevent excessive allocations
  constexpr u32 MAX_STRING_COUNT = 10000000; // 10 million strings max
  if (stringCount > MAX_STRING_COUNT) {
    return Result<void>::error("String count exceeds maximum allowed");
  }

  // Skip symbol table size
  offset += sizeof(u32);

  // Validate that bytecode is large enough for instructions
  constexpr usize INSTRUCTION_SIZE = 5; // 1 byte opcode + 4 bytes operand
  // Check for potential overflow in multiplication
  constexpr usize MAX_SAFE_SIZE = std::numeric_limits<usize>::max() / INSTRUCTION_SIZE;
  if (static_cast<usize>(instrCount) > MAX_SAFE_SIZE) {
    return Result<void>::error("Instruction count would cause overflow");
  }
  usize requiredSize = offset + (static_cast<usize>(instrCount) * INSTRUCTION_SIZE);
  if (requiredSize > bytecode.size() || requiredSize < offset) {
    return Result<void>::error("Bytecode too small for declared instruction count");
  }

  // Read instructions
  std::vector<Instruction> program;
  program.reserve(instrCount);

  for (u32 i = 0; i < instrCount; ++i) {
    if (offset + INSTRUCTION_SIZE > bytecode.size()) {
      return Result<void>::error("Unexpected end of bytecode at instruction " + std::to_string(i));
    }

    Instruction instr;
    instr.opcode = static_cast<OpCode>(bytecode[offset]);
    offset += 1;

    std::memcpy(&instr.operand, bytecode.data() + offset, sizeof(u32));
    offset += sizeof(u32);

    program.push_back(instr);
  }

  // Read string table with bounds checking
  std::vector<std::string> stringTable;
  stringTable.reserve(stringCount);

  // Security: Limit individual string length to prevent excessive allocations
  constexpr usize MAX_STRING_LENGTH = 1024 * 1024; // 1 MB per string max

  for (u32 i = 0; i < stringCount; ++i) {
    if (offset >= bytecode.size()) {
      return Result<void>::error("Unexpected end of bytecode at string " + std::to_string(i));
    }

    std::string str;
    while (offset < bytecode.size() && bytecode[offset] != 0) {
      str += static_cast<char>(bytecode[offset]);
      ++offset;

      if (str.size() > MAX_STRING_LENGTH) {
        return Result<void>::error("String " + std::to_string(i) +
                                   " exceeds maximum allowed length");
      }
    }

    if (offset >= bytecode.size()) {
      return Result<void>::error("Unterminated string at index " + std::to_string(i));
    }
    ++offset; // Skip null terminator
    stringTable.push_back(str);
  }

  return m_vm->load(program, stringTable);
}

void ScriptInterpreter::reset() {
  m_vm->reset();
}

bool ScriptInterpreter::step() {
  return m_vm->step();
}

void ScriptInterpreter::run() {
  m_vm->run();
}

void ScriptInterpreter::pause() {
  m_vm->pause();
}

void ScriptInterpreter::resume() {
  m_vm->resume();
}

bool ScriptInterpreter::isRunning() const {
  return m_vm->isRunning();
}

bool ScriptInterpreter::isPaused() const {
  return m_vm->isPaused();
}

bool ScriptInterpreter::isWaiting() const {
  return m_vm->isWaiting();
}

void ScriptInterpreter::setVariable(const std::string& name, i32 value) {
  m_vm->setVariable(name, value);
}

void ScriptInterpreter::setVariable(const std::string& name, f32 value) {
  m_vm->setVariable(name, value);
}

void ScriptInterpreter::setVariable(const std::string& name, const std::string& value) {
  m_vm->setVariable(name, value);
}

void ScriptInterpreter::setVariable(const std::string& name, bool value) {
  m_vm->setVariable(name, value);
}

std::optional<i32> ScriptInterpreter::getIntVariable(const std::string& name) const {
  if (!m_vm->hasVariable(name)) {
    return std::nullopt;
  }
  Value val = m_vm->getVariable(name);
  if (auto* p = std::get_if<i32>(&val)) {
    return *p;
  }
  return std::nullopt;
}

std::optional<f32> ScriptInterpreter::getFloatVariable(const std::string& name) const {
  if (!m_vm->hasVariable(name)) {
    return std::nullopt;
  }
  Value val = m_vm->getVariable(name);
  if (auto* p = std::get_if<f32>(&val)) {
    return *p;
  }
  return std::nullopt;
}

std::optional<std::string> ScriptInterpreter::getStringVariable(const std::string& name) const {
  if (!m_vm->hasVariable(name)) {
    return std::nullopt;
  }
  Value val = m_vm->getVariable(name);
  if (auto* p = std::get_if<std::string>(&val)) {
    return *p;
  }
  return std::nullopt;
}

std::optional<bool> ScriptInterpreter::getBoolVariable(const std::string& name) const {
  if (!m_vm->hasVariable(name)) {
    return std::nullopt;
  }
  Value val = m_vm->getVariable(name);
  if (auto* p = std::get_if<bool>(&val)) {
    return *p;
  }
  return std::nullopt;
}

void ScriptInterpreter::setFlag(const std::string& name, bool value) {
  m_vm->setFlag(name, value);
}

bool ScriptInterpreter::getFlag(const std::string& name) const {
  return m_vm->getFlag(name);
}

void ScriptInterpreter::signalContinue() {
  m_vm->signalContinue();
}

void ScriptInterpreter::signalChoice(i32 choice) {
  m_vm->signalChoice(choice);
}

void ScriptInterpreter::registerCallback(OpCode op, VirtualMachine::NativeCallback callback) {
  m_vm->registerCallback(op, std::move(callback));
}

} // namespace NovelMind::scripting
