# NM Script Virtual Machine Architecture

**Bytecode Execution Engine for NovelMind Scripting Language**

Version: 1.0
Last Updated: 2026-01-11

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Bytecode Format](#3-bytecode-format)
4. [Instruction Set](#4-instruction-set)
5. [Execution Model](#5-execution-model)
6. [Memory Management](#6-memory-management)
7. [Call Stack](#7-call-stack)
8. [Security & Constraints](#8-security--constraints)
9. [Debugging Support](#9-debugging-support)
10. [Performance](#10-performance)

---

## 1. Overview

The NM Script Virtual Machine (VM) is a **stack-based bytecode interpreter** designed for executing compiled NovelMind scripts.

### Key Characteristics

- **Stack-based**: Operations use an evaluation stack
- **Register-free**: No CPU-style registers
- **Type-tagged values**: Runtime type information
- **Bytecode interpreter**: No JIT compilation
- **Deterministic**: Same input always produces same output

### Compilation Pipeline

```
┌──────────────┐
│ Source Code  │ (.nms file)
│  (NM Script) │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    Lexer     │ Tokenization
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    Parser    │ AST Construction
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  Validator   │ Semantic Analysis
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   Compiler   │ Bytecode Generation
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   Bytecode   │ (CompiledScript)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Virtual      │ Execution
│ Machine (VM) │
└──────────────┘
```

---

## 2. Architecture

### 2.1 VM Components

```
┌─────────────────────────────────────────┐
│         NovelMind Virtual Machine       │
├─────────────────────────────────────────┤
│                                         │
│  ┌───────────────┐  ┌───────────────┐ │
│  │ Instruction   │  │  Evaluation   │ │
│  │ Pointer (IP)  │  │    Stack      │ │
│  └───────────────┘  └───────────────┘ │
│                                         │
│  ┌───────────────┐  ┌───────────────┐ │
│  │  Call Stack   │  │  Global Vars  │ │
│  └───────────────┘  └───────────────┘ │
│                                         │
│  ┌───────────────┐  ┌───────────────┐ │
│  │ String Table  │  │ Scene Entries │ │
│  └───────────────┘  └───────────────┘ │
│                                         │
│  ┌───────────────┐  ┌───────────────┐ │
│  │   Debugger    │  │   Security    │ │
│  │   Support     │  │  Constraints  │ │
│  └───────────────┘  └───────────────┘ │
│                                         │
└─────────────────────────────────────────┘
```

### 2.2 Memory Layout

```
┌──────────────────────────────────────┐
│  CompiledScript                      │
├──────────────────────────────────────┤
│  • Bytecode (vector<Instruction>)    │
│  • String Table (vector<string>)     │
│  • Scene Entry Points (map)          │
│  • Character Declarations (map)      │
│  • Debug Info (map)                  │
└──────────────────────────────────────┘
           ↓
┌──────────────────────────────────────┐
│  VM Runtime State                    │
├──────────────────────────────────────┤
│  • Instruction Pointer (u32)         │
│  • Evaluation Stack (stack<Value>)   │
│  • Call Stack (stack<CallFrame>)     │
│  • Global Variables (map<string,     │
│                       Value>)        │
│  • Flags (map<string, bool>)         │
└──────────────────────────────────────┘
```

---

## 3. Bytecode Format

### 3.1 Instruction Format

Each instruction is 5 bytes:

```
┌─────────┬──────────────────┐
│ OpCode  │     Operand      │
│ (1 byte)│    (4 bytes)     │
└─────────┴──────────────────┘
```

- **OpCode**: Operation to perform (u8)
- **Operand**: Instruction-specific parameter (u32)

### 3.2 CompiledScript Structure

```cpp
struct CompiledScript {
    // Bytecode instructions
    std::vector<Instruction> instructions;

    // String constant pool
    std::vector<std::string> stringTable;

    // Scene name → instruction index
    std::map<std::string, u32> sceneEntryPoints;

    // Character declarations
    std::map<std::string, CharacterDecl> characters;

    // Debug info: instruction index → source location
    std::map<u32, DebugSourceLocation> sourceMappings;
};
```

### 3.3 String Table

Strings are stored in a **constant pool** to avoid duplication:

**Example:**
```
String Table:
[0] "Hello, World!"
[1] "bg_forest"
[2] "Alex"
[3] "Elder Sage"
```

**Bytecode:**
```
PUSH_STRING 0    // Push "Hello, World!"
PUSH_STRING 1    // Push "bg_forest"
```

### 3.4 Scene Entry Points

Maps scene names to instruction addresses:

```cpp
sceneEntryPoints = {
    {"intro", 0},
    {"chapter1", 156},
    {"ending", 423}
}
```

---

## 4. Instruction Set

### 4.1 OpCode Categories

| Category | OpCode Range | Count |
|----------|--------------|-------|
| Control Flow | 0x00 - 0x0F | 7 |
| Stack Operations | 0x10 - 0x1F | 7 |
| Variables | 0x20 - 0x2F | 4 |
| Arithmetic | 0x30 - 0x3F | 6 |
| Comparison | 0x40 - 0x4F | 6 |
| Logical | 0x50 - 0x5F | 3 |
| VN Commands | 0x60 - 0x6F | 14 |

### 4.2 Control Flow Instructions

| OpCode | Name | Operand | Description |
|--------|------|---------|-------------|
| 0x00 | `NOP` | - | No operation |
| 0x01 | `HALT` | - | Stop execution |
| 0x02 | `JUMP` | address | Unconditional jump to address |
| 0x03 | `JUMP_IF` | address | Jump if top of stack is true |
| 0x04 | `JUMP_IF_NOT` | address | Jump if top of stack is false |
| 0x05 | `CALL` | address | Call function at address |
| 0x06 | `RETURN` | - | Return from function |

**Examples:**
```
JUMP 100         // Jump to instruction 100
JUMP_IF 50       // If true, jump to instruction 50
JUMP_IF_NOT 75   // If false, jump to instruction 75
```

### 4.3 Stack Operations

| OpCode | Name | Operand | Description |
|--------|------|---------|-------------|
| 0x10 | `PUSH_INT` | value | Push integer onto stack |
| 0x11 | `PUSH_FLOAT` | value* | Push float onto stack |
| 0x12 | `PUSH_STRING` | index | Push string from table |
| 0x13 | `PUSH_BOOL` | value | Push boolean (0 or 1) |
| 0x14 | `PUSH_NULL` | - | Push null/void value |
| 0x15 | `POP` | - | Pop top value from stack |
| 0x16 | `DUP` | - | Duplicate top of stack |

*Note: Float stored as u32 bit pattern (reinterpret_cast)

**Examples:**
```
PUSH_INT 42       // Stack: [42]
PUSH_FLOAT 3.14   // Stack: [42, 3.14]
DUP               // Stack: [42, 3.14, 3.14]
POP               // Stack: [42, 3.14]
```

### 4.4 Variable Operations

| OpCode | Name | Operand | Description |
|--------|------|---------|-------------|
| 0x20 | `LOAD_VAR` | index | Load local variable |
| 0x21 | `STORE_VAR` | index | Store to local variable |
| 0x22 | `LOAD_GLOBAL` | str_index | Load global variable |
| 0x23 | `STORE_GLOBAL` | str_index | Store to global variable |

**Examples:**
```
LOAD_GLOBAL 5     // Load global var (name at stringTable[5])
STORE_GLOBAL 5    // Store to global var
```

### 4.5 Arithmetic Operations

| OpCode | Name | Operands | Result | Description |
|--------|------|----------|--------|-------------|
| 0x30 | `ADD` | a, b | a + b | Addition |
| 0x31 | `SUB` | a, b | a - b | Subtraction |
| 0x32 | `MUL` | a, b | a * b | Multiplication |
| 0x33 | `DIV` | a, b | a / b | Division (always float) |
| 0x34 | `MOD` | a, b | a % b | Modulo (int only) |
| 0x35 | `NEG` | a | -a | Negation |

**Stack Effect:**
```
Before:  [... a b]
ADD
After:   [... (a+b)]
```

### 4.6 Comparison Operations

| OpCode | Name | Operands | Result | Description |
|--------|------|----------|--------|-------------|
| 0x40 | `EQ` | a, b | a == b | Equal |
| 0x41 | `NE` | a, b | a != b | Not equal |
| 0x42 | `LT` | a, b | a < b | Less than |
| 0x43 | `LE` | a, b | a <= b | Less or equal |
| 0x44 | `GT` | a, b | a > b | Greater than |
| 0x45 | `GE` | a, b | a >= b | Greater or equal |

**All comparison ops return boolean.**

### 4.7 Logical Operations

| OpCode | Name | Operands | Result | Description |
|--------|------|----------|--------|-------------|
| 0x50 | `AND` | a, b | a && b | Logical AND |
| 0x51 | `OR` | a, b | a \|\| b | Logical OR |
| 0x52 | `NOT` | a | !a | Logical NOT |

### 4.8 Visual Novel Commands

| OpCode | Name | Operand | Description |
|--------|------|---------|-------------|
| 0x60 | `SHOW_BACKGROUND` | str_index | Show background texture |
| 0x61 | `SHOW_CHARACTER` | char_id | Show character on screen |
| 0x62 | `HIDE_CHARACTER` | char_id | Hide character |
| 0x63 | `SAY` | str_index | Display dialogue |
| 0x64 | `CHOICE` | count | Present choice menu |
| 0x65 | `SET_FLAG` | str_index | Set boolean flag |
| 0x66 | `CHECK_FLAG` | str_index | Load flag value |
| 0x67 | `PLAY_SOUND` | str_index | Play sound effect |
| 0x68 | `PLAY_MUSIC` | str_index | Play background music |
| 0x69 | `STOP_MUSIC` | - | Stop music |
| 0x6A | `WAIT` | duration* | Pause execution |
| 0x6B | `TRANSITION` | type | Visual transition |
| 0x6C | `GOTO_SCENE` | str_index | Jump to scene |
| 0x6D | `MOVE_CHARACTER` | char_id | Animate movement |

*Duration stored as float bit pattern

---

## 5. Execution Model

### 5.1 Fetch-Decode-Execute Cycle

```cpp
while (running) {
    // FETCH
    Instruction inst = instructions[IP];

    // DECODE
    OpCode op = inst.opcode;
    u32 operand = inst.operand;

    // EXECUTE
    switch (op) {
        case OpCode::PUSH_INT:
            stack.push(Value::make_int(operand));
            break;
        case OpCode::ADD:
            Value b = stack.pop();
            Value a = stack.pop();
            stack.push(a + b);
            break;
        // ... other opcodes
    }

    // INCREMENT
    IP++;
}
```

### 5.2 Value Type

Runtime values are tagged unions:

```cpp
struct Value {
    enum Type { INT, FLOAT, BOOL, STRING, VOID } type;

    union {
        int32_t int_value;
        float float_value;
        bool bool_value;
        std::string* string_value;  // Heap-allocated
    } data;

    // Type-safe operations
    Value operator+(const Value& other);
    bool to_bool() const;
    // ...
};
```

### 5.3 Execution Example

**Source Code:**
```nms
set x = 10 + 5
```

**Compiled Bytecode:**
```
0: PUSH_INT 10           // Stack: [10]
1: PUSH_INT 5            // Stack: [10, 5]
2: ADD                   // Stack: [15]
3: STORE_GLOBAL 0        // Store to "x", Stack: []
```

**Execution Trace:**
```
IP=0: PUSH_INT 10        → Stack: [10]
IP=1: PUSH_INT 5         → Stack: [10, 5]
IP=2: ADD                → Stack: [15]
IP=3: STORE_GLOBAL 0     → globals["x"] = 15, Stack: []
```

### 5.4 Control Flow Example

**Source Code:**
```nms
if x > 10 {
    say Hero "High"
} else {
    say Hero "Low"
}
```

**Compiled Bytecode:**
```
0: LOAD_GLOBAL 0         // Load "x"
1: PUSH_INT 10
2: GT                    // x > 10
3: JUMP_IF_NOT 7         // If false, jump to else
4: PUSH_STRING 0         // "High"
5: SAY 0                 // Hero says "High"
6: JUMP 9                // Skip else
7: PUSH_STRING 1         // "Low"
8: SAY 0                 // Hero says "Low"
9: ...                   // Continue
```

---

## 6. Memory Management

### 6.1 Stack-Based Memory

The VM uses three stacks:

1. **Evaluation Stack**: Operand values
2. **Call Stack**: Function call frames
3. **Local Variables**: Per-function locals (not implemented in current version)

### 6.2 Evaluation Stack

- **Type**: `std::stack<Value>`
- **Max Size**: 1024 entries (configurable)
- **Overflow**: Runtime error

**Operations:**
```cpp
void push(Value v);      // Push value
Value pop();             // Pop value (error if empty)
Value peek();            // Peek top without popping
size_t size();           // Current stack depth
```

### 6.3 Global Variables

- **Type**: `std::map<std::string, Value>`
- **Scope**: Global to entire script
- **Lifetime**: Entire execution

**Access:**
```cpp
variables["x"] = Value::make_int(42);
Value v = variables["x"];
```

### 6.4 String Memory

- **Immutable**: Strings are never modified
- **Reference-counted**: Shared ownership
- **Constant pool**: Stored in `stringTable`

---

## 7. Call Stack

### 7.1 CallFrame Structure

```cpp
struct CallFrame {
    u32 returnAddress;       // Instruction to return to
    u32 basePointer;         // Stack frame base
    std::map<std::string, Value> locals;  // Local variables
};
```

### 7.2 Function Call Mechanism

**CALL instruction:**
```
1. Push current IP + 1 onto call stack (return address)
2. Set IP to function address
```

**RETURN instruction:**
```
1. Pop return address from call stack
2. Set IP to return address
```

### 7.3 Stack Depth Limit

- **Max depth**: 256 call frames
- **Overflow**: Runtime error (stack overflow)
- **Purpose**: Prevent infinite recursion

---

## 8. Security & Constraints

### 8.1 Execution Limits

| Limit | Default | Purpose |
|-------|---------|---------|
| Max instructions | 1,000,000 | Prevent infinite loops |
| Max stack size | 1024 | Prevent stack overflow |
| Max call depth | 256 | Prevent recursion overflow |
| Max string length | 10,000 chars | Prevent memory exhaustion |

### 8.2 Security Features

1. **Bounds checking**: All array/stack accesses checked
2. **Type safety**: Runtime type validation
3. **No arbitrary code execution**: Only predefined opcodes
4. **No file system access**: (except through engine API)
5. **No network access**: (sandbox execution)

### 8.3 Resource Constraints

```cpp
struct VMConstraints {
    u32 maxInstructions = 1'000'000;    // Instructions per execution
    u32 maxStackSize = 1024;            // Stack entries
    u32 maxCallDepth = 256;             // Call frames
    u32 maxStringLength = 10'000;       // Characters
    u32 maxGlobalVars = 10'000;         // Global variables
};
```

---

## 9. Debugging Support

### 9.1 Debug Info

**Source Mapping:**
```cpp
struct DebugSourceLocation {
    std::string filename;
    u32 line;
    u32 column;
};

// Map: instruction index → source location
std::map<u32, DebugSourceLocation> sourceMappings;
```

### 9.2 Debugger Features

1. **Breakpoints**: Pause at specific instructions
2. **Step execution**: Execute one instruction at a time
3. **Stack inspection**: View evaluation stack
4. **Variable inspection**: View global/local variables
5. **Call stack trace**: View function call history

### 9.3 Debugger API

```cpp
class VMDebugger {
public:
    void setBreakpoint(u32 instructionIndex);
    void removeBreakpoint(u32 instructionIndex);
    void stepOver();
    void stepInto();
    void stepOut();
    void continue();

    std::vector<Value> getStack();
    std::map<std::string, Value> getGlobals();
    std::vector<CallFrame> getCallStack();
};
```

---

## 10. Performance

### 10.1 Execution Speed

- **Typical**: ~1,000,000 instructions/second
- **Depends on**: Instruction mix, host CPU
- **Bottlenecks**: String operations, hash map lookups

### 10.2 Memory Usage

**Per CompiledScript:**
- Instructions: ~5 bytes per instruction
- Strings: Variable, ~50-100 bytes average
- Overhead: ~1KB for metadata

**Per VM Instance:**
- Stack: ~40KB (1024 × 40-byte Value)
- Globals: Variable, ~1-10KB typical
- Call stack: ~8KB (256 × 32-byte CallFrame)

### 10.3 Optimization Techniques

**Compiler Optimizations:**
1. **Constant folding**: `set x = 2 + 3` → `PUSH_INT 5`
2. **Dead code elimination**: Remove unreachable code
3. **Jump optimization**: Remove redundant jumps

**Runtime Optimizations:**
1. **String interning**: Reuse identical strings
2. **Inline caching**: Cache variable lookups (not implemented)
3. **Instruction dispatch**: Computed goto (not implemented)

### 10.4 Benchmarks

**Typical Script (1000 lines):**
- Compile time: ~0.01 seconds
- Memory: ~50 KB compiled
- Execution: ~0.001 seconds for dialogue-heavy script

**Large Script (10,000 lines):**
- Compile time: ~0.1 seconds
- Memory: ~500 KB compiled
- Execution: ~0.01 seconds

---

## Appendix A: Complete Instruction Reference

### A.1 Instruction Format Table

| OpCode | Mnemonic | Operand Type | Stack Effect | Description |
|--------|----------|--------------|--------------|-------------|
| 0x00 | `NOP` | - | - | No operation |
| 0x01 | `HALT` | - | - | Stop VM |
| 0x02 | `JUMP` | u32 addr | - | IP = addr |
| 0x03 | `JUMP_IF` | u32 addr | [bool] → [] | If true: IP = addr |
| 0x04 | `JUMP_IF_NOT` | u32 addr | [bool] → [] | If false: IP = addr |
| 0x05 | `CALL` | u32 addr | - | Call function |
| 0x06 | `RETURN` | - | - | Return from call |
| 0x10 | `PUSH_INT` | i32 value | [] → [int] | Push integer |
| 0x11 | `PUSH_FLOAT` | f32 value* | [] → [float] | Push float |
| 0x12 | `PUSH_STRING` | u32 index | [] → [string] | Push from table |
| 0x13 | `PUSH_BOOL` | u32 0/1 | [] → [bool] | Push boolean |
| 0x14 | `PUSH_NULL` | - | [] → [void] | Push null |
| 0x15 | `POP` | - | [any] → [] | Discard top |
| 0x16 | `DUP` | - | [a] → [a, a] | Duplicate top |
| 0x20 | `LOAD_VAR` | u32 index | [] → [value] | Load local |
| 0x21 | `STORE_VAR` | u32 index | [value] → [] | Store local |
| 0x22 | `LOAD_GLOBAL` | u32 str_idx | [] → [value] | Load global |
| 0x23 | `STORE_GLOBAL` | u32 str_idx | [value] → [] | Store global |
| 0x30 | `ADD` | - | [a, b] → [a+b] | Addition |
| 0x31 | `SUB` | - | [a, b] → [a-b] | Subtraction |
| 0x32 | `MUL` | - | [a, b] → [a*b] | Multiplication |
| 0x33 | `DIV` | - | [a, b] → [a/b] | Division |
| 0x34 | `MOD` | - | [a, b] → [a%b] | Modulo |
| 0x35 | `NEG` | - | [a] → [-a] | Negation |
| 0x40 | `EQ` | - | [a, b] → [a==b] | Equal |
| 0x41 | `NE` | - | [a, b] → [a!=b] | Not equal |
| 0x42 | `LT` | - | [a, b] → [a<b] | Less than |
| 0x43 | `LE` | - | [a, b] → [a<=b] | Less or equal |
| 0x44 | `GT` | - | [a, b] → [a>b] | Greater than |
| 0x45 | `GE` | - | [a, b] → [a>=b] | Greater or equal |
| 0x50 | `AND` | - | [a, b] → [a&&b] | Logical AND |
| 0x51 | `OR` | - | [a, b] → [a\|\|b] | Logical OR |
| 0x52 | `NOT` | - | [a] → [!a] | Logical NOT |
| 0x60 | `SHOW_BACKGROUND` | u32 str_idx | [] → [] | Show background |
| 0x61 | `SHOW_CHARACTER` | u32 char_id | [] → [] | Show character |
| 0x62 | `HIDE_CHARACTER` | u32 char_id | [] → [] | Hide character |
| 0x63 | `SAY` | u32 str_idx | [] → [] | Display dialogue |
| 0x64 | `CHOICE` | u32 count | [] → [index] | Present choices |
| 0x65 | `SET_FLAG` | u32 str_idx | [bool] → [] | Set flag |
| 0x66 | `CHECK_FLAG` | u32 str_idx | [] → [bool] | Load flag |
| 0x67 | `PLAY_SOUND` | u32 str_idx | [] → [] | Play sound |
| 0x68 | `PLAY_MUSIC` | u32 str_idx | [] → [] | Play music |
| 0x69 | `STOP_MUSIC` | - | [] → [] | Stop music |
| 0x6A | `WAIT` | f32 duration* | [] → [] | Pause |
| 0x6B | `TRANSITION` | u32 type | [] → [] | Transition |
| 0x6C | `GOTO_SCENE` | u32 str_idx | [] → [] | Jump to scene |
| 0x6D | `MOVE_CHARACTER` | u32 char_id | [] → [] | Animate move |

*Float stored as u32 bit pattern

---

## Appendix B: Bytecode Example

**Source:**
```nms
character Hero(name="Alex")

scene intro {
    show background "bg_forest"
    show Hero at center

    set points = 0

    say Hero "Hello, World!"

    if points == 0 {
        say Hero "Starting fresh!"
    }
}
```

**Compiled Bytecode (Pseudo-assembly):**
```
; Scene: intro (entry point 0)
0:  PUSH_STRING 0           ; "bg_forest"
1:  SHOW_BACKGROUND 0
2:  PUSH_INT 0              ; Character ID for Hero
3:  PUSH_INT 1              ; Position: center (enum value)
4:  SHOW_CHARACTER 0

5:  PUSH_INT 0
6:  STORE_GLOBAL 1          ; "points"

7:  PUSH_STRING 2           ; "Hello, World!"
8:  PUSH_INT 0              ; Hero ID
9:  SAY 0

10: LOAD_GLOBAL 1           ; "points"
11: PUSH_INT 0
12: EQ
13: JUMP_IF_NOT 17

14: PUSH_STRING 3           ; "Starting fresh!"
15: PUSH_INT 0              ; Hero ID
16: SAY 0

17: HALT

; String Table:
; [0] "bg_forest"
; [1] "points"
; [2] "Hello, World!"
; [3] "Starting fresh!"

; Scene Entry Points:
; intro → 0
```

---

*End of NM Script VM Architecture Documentation*
