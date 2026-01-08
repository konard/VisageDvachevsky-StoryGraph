#pragma once

#include "NovelMind/core/types.hpp"

namespace NovelMind::scripting {

enum class OpCode : u8 {
  // Control flow
  NOP = 0x00,
  HALT = 0x01,
  JUMP = 0x02,
  JUMP_IF = 0x03,
  JUMP_IF_NOT = 0x04,
  CALL = 0x05,
  RETURN = 0x06,

  // Stack operations
  PUSH_INT = 0x10,
  PUSH_FLOAT = 0x11,
  PUSH_STRING = 0x12,
  PUSH_BOOL = 0x13,
  PUSH_NULL = 0x14,
  POP = 0x15,
  DUP = 0x16,

  // Variables
  LOAD_VAR = 0x20,
  STORE_VAR = 0x21,
  LOAD_GLOBAL = 0x22,
  STORE_GLOBAL = 0x23,

  // Arithmetic
  ADD = 0x30,
  SUB = 0x31,
  MUL = 0x32,
  DIV = 0x33,
  MOD = 0x34,
  NEG = 0x35,

  // Comparison
  EQ = 0x40,
  NE = 0x41,
  LT = 0x42,
  LE = 0x43,
  GT = 0x44,
  GE = 0x45,

  // Logical
  AND = 0x50,
  OR = 0x51,
  NOT = 0x52,

  // Visual novel commands
  SHOW_BACKGROUND = 0x60,
  SHOW_CHARACTER = 0x61,
  HIDE_CHARACTER = 0x62,
  SAY = 0x63,
  CHOICE = 0x64,
  SET_FLAG = 0x65,
  CHECK_FLAG = 0x66,
  PLAY_SOUND = 0x67,
  PLAY_MUSIC = 0x68,
  STOP_MUSIC = 0x69,
  WAIT = 0x6A,
  TRANSITION = 0x6B,
  GOTO_SCENE = 0x6C,
  MOVE_CHARACTER = 0x6D
};

struct Instruction {
  OpCode opcode;
  u32 operand;

  Instruction() : opcode(OpCode::NOP), operand(0) {}
  Instruction(OpCode op, u32 op_val = 0) : opcode(op), operand(op_val) {}
};

} // namespace NovelMind::scripting
