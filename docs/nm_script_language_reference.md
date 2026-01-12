# NM Script Language Reference 1.0

**Complete Language Specification for NovelMind Visual Novel Engine**

## Version Information

- **Version**: 1.0
- **Status**: Draft
- **Last Updated**: 2026-01-11

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Lexical Structure](#2-lexical-structure)
3. [Type System](#3-type-system)
4. [Character Declarations](#4-character-declarations)
5. [Scene Declarations](#5-scene-declarations)
6. [Statements](#6-statements)
7. [Expressions](#7-expressions)
8. [Expression Evaluation Rules](#8-expression-evaluation-rules)
9. [Control Flow](#9-control-flow)
10. [Built-in Commands Reference](#10-built-in-commands-reference)
11. [Comments](#11-comments)
12. [Reserved Keywords](#12-reserved-keywords)
13. [Grammar Specification (EBNF)](#13-grammar-specification-ebnf)
14. [Semantic Rules](#14-semantic-rules)
15. [Localization System](#15-localization-system)
16. [Voice Binding System](#16-voice-binding-system)
17. [Compatibility Policy](#17-compatibility-policy)
18. [Error Reference](#18-error-reference)
19. [Complete Examples](#19-complete-examples)

---

## 1. Introduction

NM Script is a domain-specific language designed for creating visual novels in the NovelMind engine. It provides a clean and intuitive syntax for defining characters, scenes, dialogues, choices, and game logic.

### Design Philosophy

- **Readability**: Code should read like a story script
- **Simplicity**: Minimal syntax for common operations
- **Type Safety**: Static type checking where possible
- **Internationalization**: Full Unicode support for identifiers and strings

### File Format

- **Extension**: `.nms` (NovelMind Script)
- **Encoding**: UTF-8
- **Line Endings**: LF or CRLF (automatically normalized)

---

## 2. Lexical Structure

### 2.1 Identifiers

Identifiers must start with a letter (Unicode category `\p{L}`) or underscore (`_`), followed by any combination of letters, digits (Unicode category `\p{N}`), or underscores.

**Syntax:**
```
identifier ::= [\p{L}_][\p{L}\p{N}_]*
```

**Valid Examples:**
```nms
hero           // ASCII
_private       // Starts with underscore
character1     // Contains digit
MainScene      // CamelCase

// Unicode identifiers:
пролог         // Cyrillic
персонаж       // Cyrillic
εισαγωγή       // Greek
導入            // Japanese (Kanji)
주인공          // Korean (Hangul)
Алексей_1      // Mixed
```

**Invalid Examples:**
```nms
// 123scene    // ERROR: Starts with digit
// my-scene    // ERROR: Contains hyphen
// scene name  // ERROR: Contains space
```

> **Note**: The Story Graph editor automatically sanitizes invalid identifiers by adding underscores and numeric suffixes.

### 2.2 String Literals

String literals are enclosed in double quotes. Escape sequences are supported:

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |
| `\{` | Literal opening brace (escapes text formatting command) |

**Examples:**
```nms
"Hello, World!"
"Line 1\nLine 2"
"He said \"Hello\""
"Use \{w=1} to show literal braces"
```

### 2.3 Numeric Literals

Numbers can be integers or floating-point:

```nms
42          // Integer
3.14        // Float
-17         // Negative integer
0.5         // Decimal fraction less than 1
```

**Format:**
```
integer ::= ['-']? digit+
float   ::= ['-']? digit+ '.' digit+
```

### 2.4 Boolean Literals

```nms
true
false
```

### 2.5 Whitespace

Whitespace (spaces, tabs, newlines) is ignored except:
- Inside string literals
- As token separators

---

## 3. Type System

NM Script supports **5 fundamental types**:

| Type | Description | Size | Range/Capacity |
|------|-------------|------|----------------|
| `int` | Signed integer | 32-bit | -2,147,483,648 to 2,147,483,647 |
| `float` | Floating-point | 32-bit | IEEE 754 single precision |
| `bool` | Boolean | 1-bit | `true` or `false` |
| `string` | Text data | Variable | UTF-8 encoded |
| `void` | No value | 0 | Used for commands with no return value |

### 3.1 Type Coercion Rules

#### Numeric Conversions

| From | To | Rule | Example |
|------|-----|------|---------|
| `int` | `float` | **Implicit**, exact conversion | `42` → `42.0` |
| `float` | `int` | **Implicit**, truncates toward zero | `3.9` → `3`, `-3.9` → `-3` |
| `int` | `bool` | **Implicit**, `0` is `false`, non-zero is `true` | `0` → `false`, `5` → `true` |
| `float` | `bool` | **Implicit**, `0.0` is `false`, non-zero is `true` | `0.0` → `false`, `0.1` → `true` |

#### String Conversions

| From | To | Rule | Example |
|------|-----|------|---------|
| Any | `string` | **Implicit** in string contexts | `42` → `"42"` |
| `int` | `string` | Base-10 representation | `42` → `"42"` |
| `float` | `string` | Decimal representation, up to 6 digits precision | `3.14` → `"3.14"` |
| `bool` | `string` | Lowercase keyword | `true` → `"true"` |
| `string` | `int` | **Not supported**, runtime error | - |
| `string` | `float` | **Not supported**, runtime error | - |

#### Boolean Conversions

| From | To | Rule | Example |
|------|-----|------|---------|
| `string` | `bool` | Empty string is `false`, non-empty is `true` | `""` → `false`, `"hi"` → `true` |
| `int` | `bool` | Zero is `false`, non-zero is `true` | `0` → `false`, `-5` → `true` |
| `float` | `bool` | Zero is `false`, non-zero is `true` | `0.0` → `false`, `0.001` → `true` |

### 3.2 Type Inference

Variables are **dynamically typed** - their type is inferred from the assigned value:

```nms
set x = 10        // x is int
set y = 3.14      // y is float
set name = "Alex" // name is string
set flag ready = true  // ready is bool (flag)
```

### 3.3 Type Checking

Type checking occurs at:
- **Compile time**: For expressions with known types
- **Runtime**: For operations involving variables

**Type errors result in:**
- Compile-time error (if detectable)
- Runtime error (for dynamic type mismatches)

---

## 4. Character Declarations

Characters must be declared before use. Declarations define the character's identifier, display name, and optional styling.

### Syntax

```nms
character <id>(<properties>)
```

### Properties

| Property | Type | Required | Description | Default |
|----------|------|----------|-------------|---------|
| `name` | `string` | **Yes** | Display name in dialogue | - |
| `color` | `string` | No | Name color (hex: `#RRGGBB`) | `#FFFFFF` |
| `voice` | `string` | No | Voice identifier for audio binding | `""` |
| `defaultSprite` | `string` | No | Default sprite asset | `""` |

### Color Format

- **Hex RGB**: `#RRGGBB` where each component is `00`-`FF`
- **Examples**: `#FF0000` (red), `#00FF00` (green), `#0000FF` (blue)

### Examples

```nms
// Basic character
character Hero(name="Alex")

// Character with styling
character Villain(name="Lord Darkness", color="#FF0000")

// Character with voice
character Narrator(name="Narrator", voice="narrator_voice")

// Full declaration
character Sage(name="Elder Sage", color="#FFD700", voice="sage_voice", defaultSprite="sage_neutral")

// Narrator with no name (common pattern)
character Narrator(name="", color="#AAAAAA")
```

### Semantic Rules

1. Character IDs must be unique
2. Character IDs must follow identifier rules
3. Characters must be declared before use in `say` or `show` statements
4. Unused character declarations generate warnings

---

## 5. Scene Declarations

Scenes are the primary organizational unit in NM Script. Each scene contains a sequence of statements.

### Syntax

```nms
scene <id> {
    <statements>
}
```

### Rules

- Scene IDs must be unique within the script
- At least one scene must be defined
- Scenes can reference other scenes via `goto`
- Empty scenes generate warnings
- Unreachable scenes generate warnings

### Example

```nms
scene intro {
    show background "bg_city_night"
    show Hero at center

    say Hero "Welcome to the adventure!"

    goto chapter1
}

scene chapter1 {
    // Chapter 1 content
}
```

---

## 6. Statements

### 6.1 Show Statement

Displays visual elements on screen.

**Syntax:**
```nms
// Show background
show background "<texture_id>"

// Show character
show <character_id> at <position>
show <character_id> at <position> with <expression>
```

**Positions:**
- `left` - Left side of screen
- `center` - Center of screen
- `right` - Right side of screen
- `(x, y)` - Custom coordinates (pixels from top-left)

**Examples:**
```nms
show background "bg_forest"
show Hero at left
show Hero at center with "happy"
show Hero at (640, 360)  // Custom position
```

### 6.2 Hide Statement

Removes visual elements from screen.

**Syntax:**
```nms
hide <character_id>
hide background
```

**Examples:**
```nms
hide Hero
hide background
```

### 6.3 Say Statement

Displays dialogue text.

**Syntax:**
```nms
say <character_id> "<text>"
say <character_id> "<text>" voice "<audio_path>"
```

**Examples:**
```nms
say Hero "Hello!"
say Narrator "The story begins..."
say Hero "Welcome!" voice "audio/hero_greeting.wav"
```

**Embedded Text Formatting:**

Text can include formatting commands:

| Command | Effect | Example |
|---------|--------|---------|
| `{w=N}` | Wait N seconds | `"Wait...{w=0.5}done"` |
| `{speed=N}` | Set text reveal speed (chars/sec) | `"{speed=50}Slow text"` |
| `{color=#HEX}` | Change text color | `"{color=#FF0000}Red{/color}"` |
| `{/color}` | Reset color | `"{/color}"` |
| `{shake}...{/shake}` | Shake effect | `"{shake}Scary!{/shake}"` |
| `{wave}...{/wave}` | Wave effect | `"{wave}Hello~{/wave}"` |

**Example:**
```nms
say Hero "Hello...{w=1.0}is anyone there?"
say Hero "{color=#FF0000}Danger!{/color} Watch out!"
```

### 6.4 Choice Statement

Presents options to the player.

**Syntax:**
```nms
choice {
    "<text>" -> <action>
    "<text>" if <condition> -> <action>
}
```

**Actions:**
- `goto <scene_id>` - Jump to scene
- `{ ... }` - Execute block of statements

**Examples:**
```nms
choice {
    "Go left" -> goto left_path
    "Go right" -> goto right_path
}

// With conditions
choice {
    "Open door" if flag has_key -> goto inside
    "Force door" -> {
        say Hero "I'll force it open!"
        set flag door_broken = true
        goto inside
    }
}
```

### 6.5 Set Statement

Assigns values to variables or flags.

**Syntax:**
```nms
set <variable> = <expression>
set flag <flag_name> = <bool_expression>
```

**Examples:**
```nms
set points = 10
set player_name = "Alex"
set health = health - 10
set flag visited_shop = true
set flag has_key = points >= 50
```

### 6.6 Wait Statement

Pauses execution.

**Syntax:**
```nms
wait <seconds>
```

**Examples:**
```nms
wait 1.0
wait 2.5
```

### 6.7 Goto Statement

Jumps to another scene.

**Syntax:**
```nms
goto <scene_id>
```

**Example:**
```nms
goto next_scene
```

### 6.8 Transition Statement

Applies visual transition effect.

**Syntax:**
```nms
transition <type> <duration>
```

**Transition Types:**
- `fade` - Fade to black and back
- `dissolve` - Cross-dissolve
- `slide_left` - Slide left
- `slide_right` - Slide right
- `slide_up` - Slide up
- `slide_down` - Slide down

**Examples:**
```nms
transition fade 0.5
transition dissolve 1.0
transition slide_left 0.3
```

### 6.9 Play Statement

Plays audio (music or sound effects).

**Syntax:**
```nms
play music "<track_id>"
play music "<track_id>" loop=<bool>
play sound "<sound_id>"
```

**Examples:**
```nms
play music "theme_exploration"
play music "boss_battle" loop=true
play music "oneshot_jingle" loop=false
play sound "door_open"
```

### 6.10 Stop Statement

Stops audio playback.

**Syntax:**
```nms
stop music
stop music fade=<duration>
```

**Examples:**
```nms
stop music
stop music fade=1.0  // Fade out over 1 second
```

### 6.11 Move Statement

Animates character movement.

**Syntax:**
```nms
move <character_id> to <position> duration=<seconds>
```

**Examples:**
```nms
move Hero to left duration=1.0
move Hero to (100, 200) duration=0.5
```

---

## 7. Expressions

### 7.1 Operators

#### Arithmetic Operators

| Operator | Description | Type | Example | Result Type |
|----------|-------------|------|---------|-------------|
| `+` | Addition | Binary | `a + b` | `int` or `float` |
| `-` | Subtraction | Binary | `a - b` | `int` or `float` |
| `*` | Multiplication | Binary | `a * b` | `int` or `float` |
| `/` | Division | Binary | `a / b` | `float` (always) |
| `%` | Modulo | Binary | `a % b` | `int` |
| `-` | Negation | Unary | `-a` | Same as operand |

**Division Behavior:**
```nms
set x = 10 / 3   // x = 3.333333 (float)
set y = 10 % 3   // y = 1 (int)
set z = -5 % 3   // z = -2 (int)
```

#### Comparison Operators

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `==` | Equal | `a == b` | `bool` |
| `!=` | Not equal | `a != b` | `bool` |
| `<` | Less than | `a < b` | `bool` |
| `<=` | Less or equal | `a <= b` | `bool` |
| `>` | Greater than | `a > b` | `bool` |
| `>=` | Greater or equal | `a >= b` | `bool` |

**Comparison Rules:**
- Numbers compare by value
- Strings compare lexicographically (UTF-8 byte order)
- Booleans: `false < true`
- Different types: conversion to common type first

#### Logical Operators

| Operator | Description | Example | Short-circuits |
|----------|-------------|---------|----------------|
| `&&` | Logical AND | `a && b` | Yes |
| `\|\|` | Logical OR | `a \|\| b` | Yes |
| `!` | Logical NOT | `!a` | No |

**Short-circuit Evaluation:**
```nms
// b is not evaluated if a is false
if a && b { ... }

// b is not evaluated if a is true
if a || b { ... }
```

### 7.2 Operator Precedence

From highest to lowest precedence:

1. `!` (unary NOT), `-` (unary negation)
2. `*`, `/`, `%`
3. `+`, `-`
4. `<`, `<=`, `>`, `>=`
5. `==`, `!=`
6. `&&`
7. `||`

**Examples:**
```nms
// Equivalent to: (a + b) * c
set x = a + b * c    // NO! Incorrect assumption

// Actually: a + (b * c)
// Correct grouping:
set x = (a + b) * c

// Complex expression
set result = a + b * c > d && e || f
// Evaluated as: ((a + (b * c)) > d) && e) || f
```

### 7.3 Expression Types

**Literal Expressions:**
```nms
42              // int literal
3.14            // float literal
"Hello"         // string literal
true            // bool literal
```

**Identifier Expressions:**
```nms
points          // Variable reference
flag has_key    // Flag reference
```

**Binary Expressions:**
```nms
a + b
points >= 100
flag visited && flag talked
```

**Unary Expressions:**
```nms
-health
!flag done
```

**Parenthesized Expressions:**
```nms
(a + b) * c
```

**Function Call Expressions:**
```nms
loc("greeting.hello")  // Localization function
```

---

## 8. Expression Evaluation Rules

### 8.1 Evaluation Order

1. **Parentheses**: Innermost first
2. **Operators**: By precedence (highest first)
3. **Left-to-right**: For operators of equal precedence
4. **Short-circuit**: For `&&` and `||`

**Example:**
```nms
set result = (a + b) * c / (d - e)
// Evaluation order:
// 1. (a + b) → temp1
// 2. (d - e) → temp2
// 3. temp1 * c → temp3
// 4. temp3 / temp2 → result
```

### 8.2 Short-Circuit Evaluation

**AND operator (`&&`):**
```nms
if expensive_check() && other_check() {
    // other_check() is NOT called if expensive_check() returns false
}
```

**OR operator (`||`):**
```nms
if cheap_check() || expensive_check() {
    // expensive_check() is NOT called if cheap_check() returns true
}
```

### 8.3 Type Promotion in Mixed Expressions

When mixing `int` and `float`:

```nms
set x = 5 + 3.14    // 5 promoted to 5.0, result is 8.14 (float)
set y = 10 / 3      // Result is 3.333333 (float)
set z = 10 % 3.5    // ERROR: modulo requires int operands
```

**Promotion Rules:**
- `int` + `float` → `float`
- `int` - `float` → `float`
- `int` * `float` → `float`
- `int` / `int` → `float` (division always returns float)
- `int` % `int` → `int` (modulo requires int operands)

### 8.4 String Concatenation

**Not supported** - use separate `say` statements:

```nms
// ERROR: No string concatenation
// set message = "Hello " + name

// Correct approach:
say Hero "Hello"  // Use interpolation in engine if needed
```

---

## 9. Control Flow

### 9.1 If Statement

Conditional execution based on boolean expression.

**Syntax:**
```nms
if <condition> {
    <statements>
}

if <condition> {
    <statements>
} else {
    <statements>
}

if <condition> {
    <statements>
} else if <condition> {
    <statements>
} else {
    <statements>
}
```

**Examples:**
```nms
// Simple if
if points >= 100 {
    say Narrator "You've won!"
    goto victory
}

// If-else
if health > 0 {
    say Hero "I can still fight!"
} else {
    say Hero "I can't go on..."
    goto game_over
}

// If-else if-else
if points >= 100 {
    goto best_ending
} else if points >= 50 {
    goto good_ending
} else {
    goto bad_ending
}
```

### 9.2 Flag Conditions

Flags are boolean variables with special syntax:

```nms
// Check if flag is true
if flag visited_shop {
    say Shopkeeper "Welcome back!"
}

// Check if flag is false
if !flag has_key {
    say Hero "The door is locked..."
}

// Complex flag logic
if flag has_key && !flag door_opened {
    say Hero "I can unlock this door."
    set flag door_opened = true
}
```

### 9.3 Choice Branching

Choices create branching narratives:

```nms
say Hero "What should I do?"

choice {
    "Attack" -> {
        set flag chose_combat = true
        goto combat_scene
    }
    "Negotiate" -> {
        set flag chose_diplomacy = true
        goto diplomacy_scene
    }
    "Run away" if health < 20 -> {
        say Hero "I'm too weak to fight!"
        goto escape_scene
    }
}
```

---

## 10. Built-in Commands Reference

### 10.1 Visual Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `show background` | `show background "<id>"` | Display background image |
| `show character` | `show <char> at <pos>` | Show character at position |
| `hide character` | `hide <char>` | Remove character from screen |
| `hide background` | `hide background` | Remove background |
| `move` | `move <char> to <pos> duration=<sec>` | Animate character movement |

### 10.2 Dialogue Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `say` | `say <char> "<text>"` | Display dialogue |
| `say` (with voice) | `say <char> "<text>" voice "<path>"` | Display dialogue with voice |

### 10.3 Flow Control Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `goto` | `goto <scene>` | Jump to scene |
| `choice` | `choice { ... }` | Present player choices |
| `if` | `if <cond> { ... }` | Conditional execution |
| `wait` | `wait <seconds>` | Pause execution |

### 10.4 Audio Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `play music` | `play music "<id>"` | Play background music (loops) |
| `play music` (no loop) | `play music "<id>" loop=false` | Play music once |
| `play sound` | `play sound "<id>"` | Play sound effect |
| `stop music` | `stop music` | Stop music immediately |
| `stop music` (fade) | `stop music fade=<sec>` | Fade out music |

### 10.5 State Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `set` | `set <var> = <expr>` | Assign variable |
| `set flag` | `set flag <name> = <bool>` | Set boolean flag |

### 10.6 Effect Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `transition` | `transition <type> <duration>` | Visual transition effect |

---

## 11. Comments

### Single-line Comments

```nms
// This is a single-line comment
character Hero(name="Alex")  // Inline comment
```

### Multi-line Comments

```nms
/*
 * This is a multi-line comment
 * It can span multiple lines
 */
character Hero(name="Alex")
```

**Nesting:**
- Multi-line comments **do not nest**
- `/* /* nested */ */` will close at first `*/`

---

## 12. Reserved Keywords

The following identifiers are reserved and cannot be used as variable, character, or scene names:

```
and         character   choice      else        false
flag        goto        hide        if          music
not         or          play        say         scene
set         show        sound       stop        then
transition  true        wait        with        move
loop        fade        at          to          duration
voice       background  loc
```

**Attempting to use reserved keywords as identifiers will result in a compile error.**

---

## 13. Grammar Specification (EBNF)

```ebnf
(* NM Script Grammar in Extended Backus-Naur Form *)

program         = { character_decl | scene_decl } ;

(* Character Declarations *)
character_decl  = "character" identifier "(" property_list ")" ;
property_list   = [ property { "," property } ] ;
property        = identifier "=" expression ;

(* Scene Declarations *)
scene_decl      = "scene" identifier "{" { statement } "}" ;

(* Statements *)
statement       = show_stmt
                | hide_stmt
                | say_stmt
                | choice_stmt
                | if_stmt
                | set_stmt
                | goto_stmt
                | wait_stmt
                | transition_stmt
                | play_stmt
                | stop_stmt
                | move_stmt
                | block_stmt ;

show_stmt       = "show" ( "background" string
                         | identifier [ "at" position ] [ "with" string ] ) ;
hide_stmt       = "hide" ( identifier | "background" ) ;
say_stmt        = "say" identifier string [ "voice" string ] ;
choice_stmt     = "choice" "{" { choice_option } "}" ;
choice_option   = string [ "if" expression ] "->" ( goto_stmt | block_stmt ) ;
if_stmt         = "if" expression block_stmt
                  { "else" "if" expression block_stmt }
                  [ "else" block_stmt ] ;
set_stmt        = "set" [ "flag" ] identifier "=" expression ;
goto_stmt       = "goto" identifier ;
wait_stmt       = "wait" number ;
transition_stmt = "transition" identifier number ;
play_stmt       = "play" ( "music" | "sound" ) string [ play_options ] ;
play_options    = { identifier "=" expression } ;
stop_stmt       = "stop" "music" [ "fade" "=" number ] ;
move_stmt       = "move" identifier "to" position "duration" "=" number ;
block_stmt      = "{" { statement } "}" ;

(* Positions *)
position        = "left"
                | "center"
                | "right"
                | "(" number "," number ")" ;

(* Expressions *)
expression      = or_expr ;
or_expr         = and_expr { "||" and_expr } ;
and_expr        = equality_expr { "&&" equality_expr } ;
equality_expr   = comparison_expr { ( "==" | "!=" ) comparison_expr } ;
comparison_expr = additive_expr { ( "<" | "<=" | ">" | ">=" ) additive_expr } ;
additive_expr   = multiplicative_expr { ( "+" | "-" ) multiplicative_expr } ;
multiplicative_expr = unary_expr { ( "*" | "/" | "%" ) unary_expr } ;
unary_expr      = [ "!" | "-" ] primary_expr ;
primary_expr    = number
                | string
                | "true"
                | "false"
                | identifier [ "(" [ expression { "," expression } ] ")" ]
                | "flag" identifier
                | "(" expression ")" ;

(* Lexical Elements *)
identifier      = letter { letter | digit | "_" } ;
number          = digit { digit } [ "." digit { digit } ] ;
string          = '"' { character | escape_sequence } '"' ;
escape_sequence = "\\" ( "n" | "t" | "\\" | '"' | "{" ) ;
letter          = ? Unicode letter category \p{L} ? | "_" ;
digit           = ? Unicode digit category \p{N} ? ;
character       = ? Any UTF-8 character except '"' and unescaped newline ? ;
```

---

## 14. Semantic Rules

### 14.1 Character Rules

1. Characters **must be declared** before use in `say` or `show` statements
2. Character IDs must be **unique**
3. Character `name` property is **required**
4. Character names can contain **any UTF-8 characters**
5. Unused characters generate **warnings** (code E3003)

### 14.2 Scene Rules

1. Scene IDs must be **unique**
2. At least **one scene** must be defined
3. All `goto` targets must reference **existing scenes**
4. Empty scenes generate **warnings** (code E3104)
5. Unreachable scenes generate **warnings** (code E3105)

### 14.3 Variable Rules

1. Variables are **dynamically created** on first assignment
2. Variable names must follow **identifier rules**
3. Variables have **global scope** within the script
4. Uninitialized variable access results in **runtime error**
5. Unused variables generate **warnings** (code E3202)

### 14.4 Flag Rules

1. Flags are **boolean-only** variables
2. Flags are accessed with `flag` prefix in conditions
3. Flags default to `false` if not set
4. Flag assignment requires `set flag` syntax

### 14.5 Choice Rules

1. Choices must have **at least one option**
2. Each option must have a **destination** (`goto` or block)
3. Conditional options use `if` syntax
4. Empty choice blocks generate **errors** (code E3601)

### 14.6 Type Rules

1. Type mismatches in expressions cause **compile errors** (if detectable) or **runtime errors**
2. Implicit type coercion follows rules in [Section 3.1](#31-type-coercion-rules)
3. Division by zero causes **runtime error**
4. Modulo of non-integers causes **compile error**

---

## 15. Localization System

### 15.1 Localization Function

The `loc()` function retrieves localized strings:

**Syntax:**
```nms
loc("<localization_key>")
```

**Example:**
```nms
say Alice loc("dialog.intro.alice_1")
say Hero loc("dialog.choice.option_1")
```

### 15.2 Auto-generated Keys

The system auto-generates localization keys for dialogue nodes:

**Format:**
```
scene.<sceneId>.dialogue.<nodeId>
```

**Example:**
```
scene.intro.dialogue.0001
scene.chapter1.dialogue.0042
```

### 15.3 Localization Files

Localization data is stored in JSON format:

```json
{
  "en": {
    "dialog.intro.alice_1": "Hello, welcome!",
    "dialog.choice.option_1": "Go left"
  },
  "ru": {
    "dialog.intro.alice_1": "Привет, добро пожаловать!",
    "dialog.choice.option_1": "Пойти налево"
  }
}
```

---

## 16. Voice Binding System

### 16.1 Voice Binding in Dialogue

**Syntax:**
```nms
say <character> "<text>" voice "<audio_path>"
```

**Example:**
```nms
say Alice "Hello!" voice "audio/voices/alice_hello.wav"
```

### 16.2 Voice Binding States

| State | Description |
|-------|-------------|
| `Unbound` | No voice file assigned |
| `Bound` | Voice file assigned and exists |
| `MissingFile` | Voice file assigned but not found |
| `AutoMapped` | Automatically mapped via localization key |
| `Pending` | Awaiting voice recording |

### 16.3 Auto-mapping

The system can automatically map voices using localization keys:

**Process:**
1. Generate localization key from dialogue node
2. Look up voice file with matching key: `voices/<locale>/<key>.wav`
3. Bind if file exists, mark as `AutoMapped`

---

## 17. Compatibility Policy

### 17.1 Versioning

NM Script uses **semantic versioning**:

- **Major** (X.y.z): Breaking changes
- **Minor** (x.Y.z): New features, backward compatible
- **Patch** (x.y.Z): Bug fixes

### 17.2 Forward Compatibility

Scripts written for NM Script 1.0 will work with:
- All 1.x versions
- Higher versions may require migration tools

### 17.3 Deprecation Policy

1. Features are **marked deprecated** in minor versions
2. Deprecated features generate **compiler warnings**
3. Deprecated features are **removed** in next major version

### 17.4 Migration Path

When breaking changes occur:
1. **Migration guide** provided in release notes
2. **Automated migration tool** available
3. **Grace period** of at least one major version

---

## 18. Error Reference

### Compile-Time Errors

| Code | Severity | Description | Example |
|------|----------|-------------|---------|
| E3001 | Error | Undefined character | `say UnknownChar "Hi"` |
| E3002 | Error | Duplicate character definition | Two `character Hero(...)` |
| E3003 | Warning | Unused character | Character declared but never used |
| E3101 | Error | Undefined scene | `goto NonExistentScene` |
| E3102 | Error | Duplicate scene definition | Two `scene intro {...}` |
| E3103 | Warning | Unused scene | Scene defined but never reached |
| E3104 | Warning | Empty scene | `scene test { }` |
| E3105 | Warning | Unreachable scene | Scene with no incoming `goto` |
| E3201 | Error | Undefined variable | Using variable before assignment |
| E3202 | Warning | Unused variable | Variable assigned but never read |
| E3301 | Warning | Unreachable code | Code after `goto` with no label |
| E3601 | Error | Empty choice block | `choice { }` |

### Runtime Errors

| Code | Description | Example |
|------|-------------|---------|
| R4001 | Division by zero | `set x = 10 / 0` |
| R4002 | Type mismatch | `set x = "hello" % 2` |
| R4003 | Stack overflow | Excessive recursion via `goto` |
| R4004 | Missing asset | Referenced texture/audio not found |
| R4005 | Invalid position | Position coordinates out of bounds |

---

## 19. Complete Examples

### 19.1 Basic Visual Novel

```nms
// Character definitions
character Hero(name="Alex", color="#00AAFF")
character Sage(name="Elder Sage", color="#FFD700")
character Narrator(name="", color="#AAAAAA")

// Introduction scene
scene intro {
    transition fade 1.0
    show background "bg_forest_dawn"
    wait 1.0

    say Narrator "The morning sun broke through the ancient trees..."
    show Hero at center

    say Hero "Today is the day. I must find the Elder Sage."
    play music "exploration_theme" loop=true

    choice {
        "Head north" -> forest_path
        "Check inventory" -> {
            say Hero "Let me see what I have..."
            set flag checked_inventory = true
            goto intro
        }
    }
}

// Forest path scene
scene forest_path {
    show background "bg_forest_deep"
    transition dissolve 0.5

    if flag checked_inventory {
        say Hero "Good thing I checked my supplies."
        set points = points + 10
    }

    show Sage at right
    say Sage "I have been expecting you, young one."
    set points = points + 50

    goto sage_dialogue
}

// Sage dialogue scene
scene sage_dialogue {
    say Sage "The path ahead is treacherous."

    choice {
        "I am ready." -> {
            set flag accepted_quest = true
            set points = points + 25
            goto trial_begin
        }
        "I need more time." -> {
            say Sage "Time is a luxury we may not have."
            goto sage_dialogue
        }
    }
}

// Trial beginning
scene trial_begin {
    play music "epic_theme" loop=true
    transition fade 0.3

    say Narrator "And so, the trial began..."

    if points >= 100 {
        say Narrator "The hero triumphed brilliantly!"
        goto best_ending
    } else if points >= 50 {
        say Narrator "The hero succeeded, but with difficulty."
        goto good_ending
    } else {
        say Narrator "The hero barely survived..."
        goto bad_ending
    }
}

scene best_ending {
    show background "bg_victory"
    say Hero "I have proven myself worthy!"
}

scene good_ending {
    show background "bg_completion"
    say Hero "I survived... but barely."
}

scene bad_ending {
    show background "bg_defeat"
    say Hero "That was... harder than expected."
}
```

### 19.2 Advanced Features Example

```nms
character Alice(name="Alice", color="#4A90D9")
character Bob(name="Bob", color="#D94A4A")

// Using variables and complex conditions
scene complex_logic {
    set relationship = 0
    set flag alice_angry = false

    show background "bg_cafe"
    show Alice at left
    show Bob at right

    say Alice "We need to talk."

    choice {
        "I'm listening." -> {
            set relationship = relationship + 10
            say Alice "Thank you for being patient."
        }
        "Not now!" -> {
            set relationship = relationship - 20
            set flag alice_angry = true
            say Alice "Fine! Be that way!"
        }
    }

    // Complex condition
    if relationship >= 10 && !flag alice_angry {
        goto good_outcome
    } else if relationship < -10 || flag alice_angry {
        goto bad_outcome
    } else {
        goto neutral_outcome
    }
}

scene good_outcome {
    say Alice "I'm glad we could talk this through."
    set flag resolved_peacefully = true
}

scene bad_outcome {
    say Alice "I can't believe you!"
    hide Alice
    say Bob "(I really messed up...)"
}

scene neutral_outcome {
    say Alice "Well... that could have gone better."
}
```

### 19.3 Text Formatting Example

```nms
character Narrator(name="", color="#FFFFFF")
character Ghost(name="???", color="#AAAAAA")

scene spooky_encounter {
    show background "bg_dark_hallway"

    say Narrator "The hallway was silent..."
    wait 2.0

    say Narrator "Too silent.{w=1.5}"

    // Text effects
    say Narrator "{shake}A chill ran down my spine.{/shake}"

    // Color changes
    say Ghost "{color=#FF0000}WHO DARES ENTER?{/color}"

    // Speed changes
    say Narrator "{speed=20}Time... seemed... to... slow... down..."

    choice {
        "Run away!" -> escape
        "Stand your ground!" -> confront
    }
}

scene escape {
    transition slide_left 0.3
    say Narrator "I ran as fast as I could!"
}

scene confront {
    say Narrator "I stood firm, facing the unknown."
}
```

---

## Appendix A: Unicode Support

NM Script fully supports Unicode for:

- **Identifiers**: Use any Unicode letters
- **Strings**: Full UTF-8 encoding
- **Comments**: Any Unicode characters

**Examples:**

```nms
// Russian
character Герой(name="Алексей", color="#00AAFF")
scene пролог {
    say Герой "Привет, мир!"
}

// Japanese
character 主人公(name="太郎", color="#00AAFF")
scene 序章 {
    say 主人公 "こんにちは、世界！"
}

// Greek
character ήρωας(name="Αλέξανδρος", color="#00AAFF")
scene πρόλογος {
    say ήρωας "Γεια σου κόσμε!"
}
```

---

## Appendix B: Performance Considerations

### Compilation

- **Average speed**: ~10,000 lines/second on modern hardware
- **Memory**: ~1 MB per 1000 lines of script
- **Optimization**: Constant folding, dead code elimination

### Runtime

- **Execution**: ~1,000,000 instructions/second
- **Stack size**: 1024 entries (configurable)
- **Call depth**: 256 levels (configurable)

### Best Practices

1. **Minimize goto chains**: Excessive jumps impact readability
2. **Use flags efficiently**: Flags are faster than numeric comparisons
3. **Preload assets**: Reference assets in order of use
4. **Avoid deep nesting**: Keep if-statements to 3-4 levels max

---

*End of NM Script Language Reference 1.0*
