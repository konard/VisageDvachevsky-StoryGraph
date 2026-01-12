# NM Script Type System

**Detailed Type System Specification for NovelMind Scripting Language**

Version: 1.0
Last Updated: 2026-01-11

---

## Table of Contents

1. [Overview](#1-overview)
2. [Fundamental Types](#2-fundamental-types)
3. [Type Coercion](#3-type-coercion)
4. [Type Inference](#4-type-inference)
5. [Type Checking](#5-type-checking)
6. [Type Operations](#6-type-operations)
7. [Type Safety](#7-type-safety)
8. [Runtime Type System](#8-runtime-type-system)
9. [Examples](#9-examples)

---

## 1. Overview

NM Script uses a **dynamic type system** with **static type inference** where possible. The type system is designed to be:

- **Simple**: Only 5 fundamental types
- **Predictable**: Clear coercion rules
- **Safe**: Type errors caught at compile-time when possible
- **Flexible**: Dynamic typing where needed

### Type System Architecture

```
┌─────────────────────────────────────┐
│         Compile Time                │
├─────────────────────────────────────┤
│ • Type inference                    │
│ • Static type checking              │
│ • Type coercion analysis            │
│ • Error detection                   │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│          Runtime                    │
├─────────────────────────────────────┤
│ • Dynamic type storage              │
│ • Runtime type checking             │
│ • Type coercion execution           │
│ • Error handling                    │
└─────────────────────────────────────┘
```

---

## 2. Fundamental Types

### 2.1 Integer (`int`)

**Description**: 32-bit signed integer

**Range**: -2,147,483,648 to 2,147,483,647

**Literal Syntax**:
```nms
42          // Positive integer
-17         // Negative integer
0           // Zero
2147483647  // Maximum value
```

**Memory Representation**:
- **Size**: 4 bytes
- **Format**: Two's complement
- **Alignment**: 4-byte boundary

**Operations**:
```nms
set a = 10 + 5      // Addition: 15
set b = 10 - 5      // Subtraction: 5
set c = 10 * 5      // Multiplication: 50
set d = 10 / 5      // Division: 2.0 (returns float!)
set e = 10 % 3      // Modulo: 1
set f = -10         // Negation: -10
```

**Special Cases**:
```nms
// Integer overflow wraps around
set x = 2147483647 + 1  // x = -2147483648

// Division by zero is runtime error
set y = 10 / 0  // ERROR: Division by zero
```

### 2.2 Floating-Point (`float`)

**Description**: IEEE 754 single-precision floating-point

**Range**: ±3.4028235 × 10³⁸ (approximate)

**Precision**: ~6-7 decimal digits

**Literal Syntax**:
```nms
3.14        // Standard notation
0.5         // Less than 1
-2.71828    // Negative
1.0         // Explicit float
```

**Memory Representation**:
- **Size**: 4 bytes
- **Format**: IEEE 754 single precision
  - 1 bit: Sign
  - 8 bits: Exponent
  - 23 bits: Mantissa
- **Alignment**: 4-byte boundary

**Operations**:
```nms
set a = 3.14 + 2.0     // Addition: 5.14
set b = 10.0 / 3.0     // Division: 3.333333
set c = -3.14          // Negation: -3.14
```

**Special Values**:
```nms
// Positive infinity (overflow)
set inf = 1e38 * 1e38

// Negative infinity
set neg_inf = -1e38 * 1e38

// NaN (not a number) - not directly creatable
// Can result from 0.0 / 0.0 (implementation-defined)
```

**Precision Considerations**:
```nms
// Floating-point arithmetic is not exact
set x = 0.1 + 0.2      // x ≈ 0.30000001 (may not be exactly 0.3)

// Comparison should use epsilon
// if abs(a - b) < 0.0001 { ... }  // (not directly supported)
```

### 2.3 Boolean (`bool`)

**Description**: Logical true/false value

**Values**: `true`, `false`

**Literal Syntax**:
```nms
true
false
```

**Memory Representation**:
- **Size**: 1 byte (stored as 0 or 1)
- **Alignment**: 1-byte boundary

**Operations**:
```nms
set a = true && false   // AND: false
set b = true || false   // OR: true
set c = !true           // NOT: false
```

**Truthiness** (see [Section 3.4](#34-truthiness-conversions)):
```nms
// Non-boolean values convert to bool:
// - 0, 0.0, "" → false
// - All other values → true
```

### 2.4 String (`string`)

**Description**: UTF-8 encoded text

**Encoding**: UTF-8

**Maximum Length**: 2³¹-1 bytes (implementation limit)

**Literal Syntax**:
```nms
"Hello, World!"
"Multi\nLine\nString"
"Escaped \"quotes\""
"Unicode: こんにちは 🌟"
```

**Memory Representation**:
- **Size**: Variable (length + data)
- **Format**: Length-prefixed UTF-8 byte array
- **Internal Structure**:
  ```
  [4-byte length][UTF-8 bytes...]
  ```

**Operations**:
```nms
// String concatenation NOT supported
// Use separate say statements instead

// String comparison
set same = "hello" == "hello"   // true
set diff = "abc" < "xyz"        // true (lexicographic)
```

**Escape Sequences**:
| Sequence | Character | Unicode |
|----------|-----------|---------|
| `\n` | Newline | U+000A |
| `\t` | Tab | U+0009 |
| `\\` | Backslash | U+005C |
| `\"` | Double quote | U+0022 |
| `\{` | Literal brace | U+007B |

**Empty String**:
```nms
set empty = ""   // Length: 0, evaluates to false in boolean context
```

### 2.5 Void (`void`)

**Description**: Absence of a value

**Usage**: Return type for statements/commands with no value

**Not a Value**: Cannot be assigned to variables

**Examples**:
```nms
// These statements return void:
say Hero "Hello"       // void
show Hero at center    // void
goto next_scene        // void (and doesn't return)
```

**Cannot be used**:
```nms
// ERROR: Cannot assign void
// set x = (say Hero "Hi")
```

---

## 3. Type Coercion

Type coercion is the automatic conversion between types.

### 3.1 Implicit Coercion

Automatic type conversion performed by the compiler/runtime.

#### 3.1.1 Numeric Conversions

**int → float**

- **Rule**: Exact conversion
- **Example**: `42` → `42.0`
- **Trigger**: Mixed arithmetic, assignment to float variable

```nms
set x = 5 + 3.14    // 5 converted to 5.0, result: 8.14
set y = 10          // y is int
set z = y + 0.0     // y converted to 10.0, z is float
```

**float → int**

- **Rule**: Truncates toward zero (removes fractional part)
- **Example**: `3.9` → `3`, `-3.9` → `-3`
- **Trigger**: Assignment to int variable (explicit cast needed)

```nms
// Note: float to int is NOT automatic in expressions
set x = 3.9
// To convert to int, use truncation via int context (implementation-specific)
```

#### 3.1.2 Boolean Conversions

**int → bool**

- **Rule**: `0` is `false`, non-zero is `true`

```nms
set x = 0
if x {               // false (0 converts to false)
    // Not executed
}

set y = 5
if y {               // true (5 converts to true)
    // Executed
}
```

**float → bool**

- **Rule**: `0.0` is `false`, non-zero is `true`

```nms
set x = 0.0
if x {               // false

set y = 0.0001
if y {               // true
```

**string → bool**

- **Rule**: Empty string `""` is `false`, non-empty is `true`

```nms
set x = ""
if x {               // false

set y = "hello"
if y {               // true
```

#### 3.1.3 String Conversions

**int → string**

- **Rule**: Base-10 representation
- **Trigger**: String context (implementation-specific)

```nms
// Automatic in some contexts (e.g., localization interpolation)
// 42 → "42"
// -17 → "-17"
```

**float → string**

- **Rule**: Decimal representation, up to 6 digits precision
- **Example**: `3.14159265` → `"3.14159"`

```nms
// 3.14 → "3.14"
// 0.5 → "0.5"
// 1.0 → "1.0" (trailing zero preserved)
```

**bool → string**

- **Rule**: Lowercase keyword

```nms
// true → "true"
// false → "false"
```

### 3.2 Explicit Coercion

**Not supported** - NM Script does not have explicit cast operators.

Use conditional logic instead:

```nms
// To convert bool to int:
set result = 0
if condition {
    set result = 1
}

// To "convert" string to number - not supported
// Use predefined variables instead
```

### 3.3 Coercion in Operations

#### Arithmetic Operations

| Operation | Left | Right | Result | Coercion |
|-----------|------|-------|--------|----------|
| `+` | `int` | `int` | `int` | None |
| `+` | `int` | `float` | `float` | Left to `float` |
| `+` | `float` | `int` | `float` | Right to `float` |
| `+` | `float` | `float` | `float` | None |
| `-` | Same rules as `+` | | | |
| `*` | Same rules as `+` | | | |
| `/` | `int` | `int` | **`float`** | Both to `float` |
| `/` | Any numeric | Any numeric | `float` | Both to `float` |
| `%` | `int` | `int` | `int` | **Both must be int** |
| `%` | Any other | | | **ERROR** |

**Examples**:
```nms
set a = 5 + 3       // int + int = int (8)
set b = 5 + 3.0     // int + float = float (8.0)
set c = 10 / 3      // int / int = float (3.333333)
set d = 10 % 3      // int % int = int (1)
set e = 10 % 3.0    // ERROR: modulo requires int
```

#### Comparison Operations

| Operation | Left | Right | Coercion |
|-----------|------|-------|----------|
| `==`, `!=` | `int` | `float` | Both to `float` |
| `<`, `<=`, `>`, `>=` | `int` | `float` | Both to `float` |
| Any comparison | `string` | `string` | Lexicographic |
| Any comparison | Different types | | Type-specific rules |

**Examples**:
```nms
set x = (5 == 5.0)      // true (5 converted to 5.0)
set y = (3 < 3.1)       // true (both float)
set z = ("abc" < "xyz") // true (lexicographic)
```

#### Logical Operations

| Operation | Operand | Coercion |
|-----------|---------|----------|
| `&&`, `\|\|` | Any type | To `bool` (truthiness) |
| `!` | Any type | To `bool` (truthiness) |

**Examples**:
```nms
set x = 5 && 10         // true && true = true
set y = 0 || "hello"    // false || true = true
set z = !""             // !false = true
```

### 3.4 Truthiness Conversions

**Truthiness Table**:

| Type | Value | Boolean |
|------|-------|---------|
| `int` | `0` | `false` |
| `int` | Non-zero | `true` |
| `float` | `0.0` | `false` |
| `float` | Non-zero | `true` |
| `bool` | `false` | `false` |
| `bool` | `true` | `true` |
| `string` | `""` | `false` |
| `string` | Non-empty | `true` |

**Examples**:
```nms
if 0 { }              // false
if 1 { }              // true
if -5 { }             // true

if 0.0 { }            // false
if 0.0001 { }         // true

if "" { }             // false
if "0" { }            // true (non-empty string!)
if "false" { }        // true (non-empty string!)
```

---

## 4. Type Inference

The compiler infers types from context:

### 4.1 Variable Declaration

Type inferred from assigned value:

```nms
set x = 10          // x is int
set y = 3.14        // y is float
set z = "hello"     // z is string
set w = true        // w is bool
```

### 4.2 Expression Type Inference

```nms
set x = 5 + 3       // Both operands int → result int
set y = 5 + 3.0     // Mixed → result float
set z = 5 > 3       // Comparison → result bool
```

### 4.3 Type Refinement

Type can change on reassignment:

```nms
set x = 10          // x is int
set x = "hello"     // x is now string (dynamic typing)
```

---

## 5. Type Checking

### 5.1 Compile-Time Type Checking

The compiler performs type checking on:

1. **Operator operands**:
   ```nms
   set x = 10 % 3.0    // ERROR: modulo requires int
   ```

2. **Function arguments**:
   ```nms
   wait "hello"        // ERROR: wait requires number
   ```

3. **Property assignments**:
   ```nms
   character Hero(name=42)  // ERROR: name requires string
   ```

### 5.2 Runtime Type Checking

Runtime checks occur for:

1. **Variable type mismatches**:
   ```nms
   set x = "hello"
   set y = x + 10      // Runtime error: cannot add string and int
   ```

2. **Division by zero**:
   ```nms
   set x = 10 / 0      // Runtime error
   ```

3. **Invalid operations**:
   ```nms
   set x = "hello" % 2  // Runtime error: modulo on string
   ```

---

## 6. Type Operations

### 6.1 Type-Specific Operations

#### Integer Operations

```nms
set sum = 10 + 5        // Addition: 15
set diff = 10 - 5       // Subtraction: 5
set prod = 10 * 5       // Multiplication: 50
set quot = 10 / 5       // Division: 2.0 (float!)
set mod = 10 % 3        // Modulo: 1
set neg = -10           // Negation: -10

set cmp1 = 10 == 10     // Equality: true
set cmp2 = 10 < 20      // Less than: true
```

#### Float Operations

```nms
set sum = 3.14 + 2.0    // Addition: 5.14
set diff = 3.14 - 1.0   // Subtraction: 2.14
set prod = 3.14 * 2.0   // Multiplication: 6.28
set quot = 10.0 / 3.0   // Division: 3.333333
// Modulo not supported for float
set neg = -3.14         // Negation: -3.14
```

#### Boolean Operations

```nms
set and_op = true && false   // Logical AND: false
set or_op = true || false    // Logical OR: true
set not_op = !true           // Logical NOT: false

set cmp = true == false      // Equality: false
```

#### String Operations

```nms
set eq = "hello" == "hello"      // Equality: true
set ne = "hello" != "world"      // Inequality: true
set lt = "abc" < "xyz"           // Less than: true
set le = "abc" <= "abc"          // Less or equal: true
// Concatenation not supported
```

### 6.2 Type Promotion Table

| Left Type | Operator | Right Type | Result Type | Promotion |
|-----------|----------|------------|-------------|-----------|
| `int` | `+`/`-`/`*` | `int` | `int` | None |
| `int` | `+`/`-`/`*` | `float` | `float` | Left → `float` |
| `float` | `+`/`-`/`*` | `int` | `float` | Right → `float` |
| `float` | `+`/`-`/`*` | `float` | `float` | None |
| `int` | `/` | `int` | `float` | Both → `float` |
| Any numeric | `/` | Any numeric | `float` | Both → `float` |
| `int` | `%` | `int` | `int` | None |
| Any | `%` | Non-int | - | **ERROR** |
| Any | `==`/`!=`/`<`/etc | Any | `bool` | Common type |
| Any | `&&`/`\|\|` | Any | `bool` | Both → `bool` |

---

## 7. Type Safety

### 7.1 Type Safety Guarantees

NM Script provides:

1. **No null pointer errors**: No null/nil value
2. **No buffer overflows**: Bounds-checked arrays (in VM)
3. **Type errors caught**: At compile-time or runtime
4. **Memory safety**: Managed memory, no manual deallocation

### 7.2 Type Error Examples

**Compile-Time Errors**:
```nms
// Invalid modulo operands
set x = 10 % 3.0        // ERROR: E4002

// Invalid function argument
wait "hello"            // ERROR: E4003
```

**Runtime Errors**:
```nms
// Division by zero
set x = 10 / 0          // ERROR: R4001

// Type mismatch in variable operation
set x = "hello"
set y = x + 10          // ERROR: R4002
```

---

## 8. Runtime Type System

### 8.1 Value Representation

At runtime, values are stored as tagged unions:

```cpp
struct Value {
    enum Type { INT, FLOAT, BOOL, STRING, VOID } type;
    union {
        int32_t int_value;
        float float_value;
        bool bool_value;
        std::string* string_value;  // Heap-allocated
    } data;
};
```

### 8.2 Type Tags

Every runtime value carries a type tag:

| Type | Tag Value | Data Size |
|------|-----------|-----------|
| `int` | 0 | 4 bytes |
| `float` | 1 | 4 bytes |
| `bool` | 2 | 1 byte |
| `string` | 3 | Pointer (8 bytes) |
| `void` | 4 | 0 bytes |

### 8.3 Type Checking at Runtime

```cpp
// Runtime type check example (C++ pseudocode)
Value add(Value a, Value b) {
    if (a.type == INT && b.type == INT) {
        return Value::make_int(a.int_value + b.int_value);
    } else if (a.type == FLOAT || b.type == FLOAT) {
        float left = a.to_float();
        float right = b.to_float();
        return Value::make_float(left + right);
    } else {
        throw TypeError("Cannot add " + type_name(a) + " and " + type_name(b));
    }
}
```

---

## 9. Examples

### 9.1 Type Coercion Examples

```nms
// Integer to float
set x = 5        // int
set y = 3.14     // float
set z = x + y    // z = 8.14 (float), x promoted to 5.0

// Division always returns float
set a = 10 / 3   // a = 3.333333 (float)

// Modulo requires int
set b = 10 % 3   // b = 1 (int)
// set c = 10 % 3.0  // ERROR: modulo requires int operands

// Boolean conversion
if 0 {
    // Not executed (0 is false)
}

if "hello" {
    // Executed (non-empty string is true)
}

// Comparison with mixed types
set result = (5 == 5.0)  // true (5 converted to 5.0)
```

### 9.2 Type Inference Examples

```nms
// Type inferred from literal
set age = 25              // int
set pi = 3.14159          // float
set name = "Alice"        // string
set ready = true          // bool

// Type inferred from expression
set sum = age + 5         // int (int + int)
set area = pi * 10.0      // float (float * float)
set can_proceed = ready && (age >= 18)  // bool

// Type changes on reassignment
set x = 10                // x is int
set x = "now string"      // x is now string
```

### 9.3 Type Safety Examples

```nms
// Safe operations
set a = 10
set b = 20
set c = a + b             // OK: int + int = int

// Type promotion
set d = a + 3.14          // OK: int promoted to float

// Type error caught at compile time
// wait "hello"           // ERROR: wait requires number

// Type error caught at runtime
set x = "hello"
// set y = x + 10         // ERROR: cannot add string and int
```

### 9.4 Complex Type Example

```nms
character Hero(name="Alex")
character Sage(name="Elder")

scene type_demo {
    // Integer arithmetic
    set points = 0
    set bonus = 50
    set total = points + bonus    // int

    // Float arithmetic
    set multiplier = 1.5
    set final_score = total * multiplier  // float (int promoted)

    // Boolean logic
    set high_score = final_score >= 100.0
    set unlocked = high_score && flag completed_tutorial

    // String in dialogue
    if high_score {
        say Sage "Impressive score!"
    } else {
        say Sage "Keep trying!"
    }

    // Conditional with type coercion
    if total {  // total > 0 evaluates to true
        say Hero "I earned some points!"
    }

    // Assignment changes type
    set result = 42        // result is int
    set result = "done"    // result is now string
}
```

---

## Appendix: Type Compatibility Matrix

### Implicit Conversion Compatibility

|  From ↓ / To → | `int` | `float` | `bool` | `string` | `void` |
|----------------|-------|---------|--------|----------|--------|
| **`int`** | ✓ | ✓ Auto | ✓ Auto | ✓ Context | ✗ |
| **`float`** | ✓ Trunc | ✓ | ✓ Auto | ✓ Context | ✗ |
| **`bool`** | ✗ | ✗ | ✓ | ✓ Context | ✗ |
| **`string`** | ✗ | ✗ | ✓ Auto | ✓ | ✗ |
| **`void`** | ✗ | ✗ | ✗ | ✗ | ✓ |

**Legend**:
- ✓ - Allowed (identity)
- ✓ Auto - Automatic implicit conversion
- ✓ Context - Conversion in specific contexts
- ✓ Trunc - Conversion with truncation
- ✗ - Not allowed

---

*End of NM Script Type System Documentation*
