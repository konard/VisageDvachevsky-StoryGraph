# NM Script Standard Library Reference

**Built-in Functions, Commands, and Features**

Version: 1.0
Last Updated: 2026-01-11

---

## Table of Contents

1. [Overview](#1-overview)
2. [Visual Commands](#2-visual-commands)
3. [Dialogue Commands](#3-dialogue-commands)
4. [Audio Commands](#4-audio-commands)
5. [Flow Control](#5-flow-control)
6. [Variable & State Management](#6-variable--state-management)
7. [Effect Commands](#7-effect-commands)
8. [Localization Functions](#8-localization-functions)
9. [Text Formatting](#9-text-formatting)
10. [Constants](#10-constants)

---

## 1. Overview

The NM Script standard library provides built-in commands and functions for creating visual novels. All commands are **keywords** and cannot be used as variable names.

### Command Categories

| Category | Commands | Purpose |
|----------|----------|---------|
| Visual | `show`, `hide`, `move` | Display and manipulate visual elements |
| Dialogue | `say`, `choice` | Present text and player choices |
| Audio | `play`, `stop` | Control music and sound effects |
| Flow Control | `goto`, `if`, `choice` | Navigate and branch story |
| State | `set`, `flag` | Manage variables and flags |
| Effects | `transition`, `wait` | Visual effects and timing |
| Localization | `loc()` | Multi-language support |

---

## 2. Visual Commands

### 2.1 show background

**Purpose**: Display a background image

**Syntax**:
```nms
show background "<texture_id>"
```

**Parameters**:
- `texture_id` (string): Asset ID of background texture

**Example**:
```nms
show background "bg_forest_day"
show background "bg_city_night"
```

**Behavior**:
- Replaces current background (if any)
- Background fills entire screen
- Non-blocking (continues immediately)

---

### 2.2 show <character>

**Purpose**: Display a character sprite on screen

**Syntax**:
```nms
show <character_id> at <position>
show <character_id> at <position> with <expression>
show <character_id> <sprite_variant> at <position>
```

**Parameters**:
- `character_id` (identifier): Character to show (must be declared)
- `position`: Where to place character
  - **Predefined**: `left`, `center`, `right`
  - **Custom**: `(x, y)` pixel coordinates
- `expression` (string, optional): Sprite variant/expression name
- `sprite_variant` (string, optional): Alternative syntax for expression

**Examples**:
```nms
// Basic positioning
show Hero at center
show Villain at right
show Sage at left

// With expressions
show Hero at center with "happy"
show Villain at right with "angry"

// Alternative expression syntax
show Hero "hero_sad" at center

// Custom position (x=640, y=360)
show Hero at (640, 360)
```

**Behavior**:
- Character appears at specified position
- If character already shown, updates position/expression
- Non-blocking

**Notes**:
- Character must be declared with `character <id>(...)` before use
- Multiple characters can be shown simultaneously
- Z-order determined by show order (later = in front)

---

### 2.3 hide <character>

**Purpose**: Remove character from screen

**Syntax**:
```nms
hide <character_id>
```

**Parameters**:
- `character_id` (identifier): Character to hide

**Example**:
```nms
hide Hero
hide Villain
```

**Behavior**:
- Character removed from screen
- No effect if character not currently shown
- Non-blocking

---

### 2.4 hide background

**Purpose**: Remove background image

**Syntax**:
```nms
hide background
```

**Example**:
```nms
hide background
```

**Behavior**:
- Background removed, reveals default color
- Non-blocking

---

### 2.5 move <character>

**Purpose**: Animate character movement

**Syntax**:
```nms
move <character_id> to <position> duration=<seconds>
```

**Parameters**:
- `character_id` (identifier): Character to move
- `position`: Target position (same format as `show`)
- `duration` (float): Animation duration in seconds

**Examples**:
```nms
// Move to predefined position
move Hero to right duration=1.0

// Move to custom position
move Hero to (800, 360) duration=0.5

// Quick movement
move Villain to left duration=0.2
```

**Behavior**:
- Smoothly animates character to new position
- Linear interpolation (lerp)
- Non-blocking (continues while animating)

---

## 3. Dialogue Commands

### 3.1 say

**Purpose**: Display dialogue text

**Syntax**:
```nms
say <character_id> "<text>"
say <character_id> "<text>" voice "<audio_path>"
```

**Parameters**:
- `character_id` (identifier): Speaking character
- `text` (string): Dialogue text (supports formatting commands)
- `audio_path` (string, optional): Voice audio file path

**Examples**:
```nms
// Basic dialogue
say Hero "Hello, World!"

// With voice
say Hero "Welcome!" voice "audio/voices/hero_welcome.wav"

// With text formatting
say Hero "Wait...{w=1.0}what was that?"
say Villain "{color=#FF0000}You will pay!{/color}"

// Narrator (character with empty name)
say Narrator "The story begins..."
```

**Behavior**:
- Displays text in dialogue box with character name
- Blocks until player advances (click/space)
- Text can be revealed gradually (typewriter effect)

**See Also**: [Text Formatting](#9-text-formatting) for inline commands

---

### 3.2 choice

**Purpose**: Present player with choices

**Syntax**:
```nms
choice {
    "<option_text>" -> <action>
    "<option_text>" if <condition> -> <action>
}
```

**Parameters**:
- `option_text` (string): Text shown to player
- `condition` (bool expression, optional): Condition for option visibility
- `action`: What happens when option selected
  - `goto <scene_id>` - Jump to scene
  - `{ ... }` - Execute block of statements

**Examples**:
```nms
// Simple choice
choice {
    "Go left" -> goto left_path
    "Go right" -> goto right_path
}

// With inline actions
choice {
    "Attack" -> {
        say Hero "Take this!"
        set flag chose_combat = true
        goto combat_scene
    }
    "Defend" -> {
        say Hero "I'll protect myself!"
        goto defense_scene
    }
}

// Conditional option
choice {
    "Use key" if flag has_key -> {
        say Hero "I'll unlock the door."
        goto unlocked_room
    }
    "Force door" -> {
        say Hero "I'll break it down!"
        goto forced_entry
    }
}
```

**Behavior**:
- Displays choice menu to player
- Blocks until player selects option
- Conditional options hidden if condition false
- Returns selected option index (0-based) to stack (for VM)

**Rules**:
- At least one option must be visible
- Empty choice blocks are compile errors
- Options evaluated top-to-bottom

---

## 4. Audio Commands

### 4.1 play music

**Purpose**: Play background music

**Syntax**:
```nms
play music "<track_id>"
play music "<track_id>" loop=<bool>
```

**Parameters**:
- `track_id` (string): Music asset ID
- `loop` (bool, optional): Loop track (default: `true`)

**Examples**:
```nms
// Play looping music (default)
play music "theme_exploration"

// Play once, no loop
play music "jingle_victory" loop=false

// Explicit loop
play music "boss_battle" loop=true
```

**Behavior**:
- Starts playing music track
- Stops previous music (if any)
- Music continues across scenes
- Non-blocking

**Notes**:
- Only one music track plays at a time
- Use `stop music` to silence
- Music volume controlled by engine settings

---

### 4.2 play sound

**Purpose**: Play sound effect

**Syntax**:
```nms
play sound "<sound_id>"
```

**Parameters**:
- `sound_id` (string): Sound effect asset ID

**Examples**:
```nms
play sound "door_open"
play sound "footsteps"
play sound "sword_clash"
```

**Behavior**:
- Plays sound effect once
- Does not interrupt music
- Multiple sounds can play simultaneously
- Non-blocking

---

### 4.3 stop music

**Purpose**: Stop background music

**Syntax**:
```nms
stop music
stop music fade=<duration>
```

**Parameters**:
- `duration` (float, optional): Fade-out time in seconds (default: 0)

**Examples**:
```nms
// Immediate stop
stop music

// Fade out over 2 seconds
stop music fade=2.0
```

**Behavior**:
- Stops currently playing music
- Fade creates smooth volume transition
- Non-blocking

---

## 5. Flow Control

### 5.1 goto

**Purpose**: Jump to another scene

**Syntax**:
```nms
goto <scene_id>
```

**Parameters**:
- `scene_id` (identifier): Target scene name

**Examples**:
```nms
goto chapter1
goto ending_good
goto game_over
```

**Behavior**:
- Immediately jumps to target scene
- Does not return (not a function call)
- Validates scene existence at compile time

**Notes**:
- Target scene must exist
- Undefined scenes cause compile errors
- Can create infinite loops (be careful!)

---

### 5.2 if / else if / else

**Purpose**: Conditional execution

**Syntax**:
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

**Parameters**:
- `condition` (bool expression): Condition to evaluate

**Examples**:
```nms
// Simple if
if points >= 100 {
    say Narrator "Victory!"
}

// If-else
if health > 0 {
    say Hero "I can continue!"
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

// With flags
if flag has_key {
    say Hero "I can unlock this door."
}

if !flag visited_shop {
    say Shopkeeper "First time here?"
    set flag visited_shop = true
}
```

**Behavior**:
- Evaluates condition(s) top-to-bottom
- Executes first matching block
- Skips remaining branches

---

## 6. Variable & State Management

### 6.1 set

**Purpose**: Assign value to variable

**Syntax**:
```nms
set <variable_name> = <expression>
```

**Parameters**:
- `variable_name` (identifier): Variable to assign
- `expression`: Value to assign (any type)

**Examples**:
```nms
// Integer
set points = 0
set score = points + 100

// Float
set health = 100.0
set damage_multiplier = 1.5

// String
set player_name = "Alex"

// Boolean
set is_ready = true

// Expressions
set total = (points * 2) + bonus
set can_proceed = health > 0 && has_key
```

**Behavior**:
- Creates variable if it doesn't exist
- Updates value if variable exists
- Type changes with assigned value (dynamic typing)
- Global scope (accessible in all scenes)

---

### 6.2 set flag

**Purpose**: Set boolean flag

**Syntax**:
```nms
set flag <flag_name> = <bool_expression>
```

**Parameters**:
- `flag_name` (identifier): Flag to set
- `bool_expression` (bool): Boolean value

**Examples**:
```nms
// Simple flag
set flag visited_shop = true
set flag has_key = false

// From expression
set flag can_enter = points >= 50
set flag quest_complete = flag task1_done && flag task2_done
```

**Behavior**:
- Creates flag if doesn't exist (defaults to `false`)
- Flags are boolean-only variables
- Global scope

**Usage in Conditions**:
```nms
// Check flag value
if flag has_key {
    // ...
}

// Check negated
if !flag door_opened {
    // ...
}
```

---

## 7. Effect Commands

### 7.1 transition

**Purpose**: Apply visual transition effect

**Syntax**:
```nms
transition <type> <duration>
```

**Parameters**:
- `type` (identifier): Transition effect type
- `duration` (float): Effect duration in seconds

**Transition Types**:
| Type | Description |
|------|-------------|
| `fade` | Fade to black and back |
| `dissolve` | Cross-dissolve |
| `slide_left` | Slide screen left |
| `slide_right` | Slide screen right |
| `slide_up` | Slide screen up |
| `slide_down` | Slide screen down |

**Examples**:
```nms
// Fade transition
transition fade 1.0

// Quick dissolve
transition dissolve 0.3

// Slide transition
transition slide_left 0.5
```

**Behavior**:
- Applies screen-wide visual effect
- Typically used between scenes
- **Blocking**: Waits for effect to complete

**Typical Usage**:
```nms
scene intro {
    // Start with fade-in
    transition fade 1.0

    show background "bg_room"
    // ...
}

scene next_chapter {
    // Transition between scenes
    transition dissolve 0.5

    show background "bg_forest"
    // ...
}
```

---

### 7.2 wait

**Purpose**: Pause execution

**Syntax**:
```nms
wait <seconds>
```

**Parameters**:
- `seconds` (float): Duration to wait

**Examples**:
```nms
wait 1.0     // Wait 1 second
wait 2.5     // Wait 2.5 seconds
wait 0.5     // Wait half second
```

**Behavior**:
- **Blocking**: Pauses script execution
- Used for pacing, dramatic pauses
- Does not affect animation/audio

**Typical Usage**:
```nms
say Hero "Wait..."
wait 1.0
say Hero "Did you hear that?"

// Dramatic pause
show background "bg_dark"
wait 2.0
say Narrator "And then..."
```

---

## 8. Localization Functions

### 8.1 loc()

**Purpose**: Retrieve localized string

**Syntax**:
```nms
loc("<localization_key>")
```

**Parameters**:
- `localization_key` (string): Key for localization lookup

**Examples**:
```nms
say Hero loc("dialog.intro.greeting")
say Narrator loc("ui.tutorial.hint_001")

choice {
    loc("menu.option_continue") -> goto continue_game
    loc("menu.option_quit") -> goto main_menu
}
```

**Behavior**:
- Looks up key in current locale's translation file
- Returns translated string
- Falls back to key if translation missing

**Key Format Convention**:
```
<category>.<context>.<identifier>

Examples:
dialog.intro.greeting
ui.menu.start_game
scene.chapter1.narrator_001
```

**Auto-Generated Keys**:

The system auto-generates keys for dialogue nodes:
```
scene.<sceneId>.dialogue.<nodeId>

Example:
scene.intro.dialogue.0001
scene.boss_fight.dialogue.0042
```

---

## 9. Text Formatting

### 9.1 Inline Commands

Text in `say` statements supports embedded formatting commands:

| Command | Effect | Example |
|---------|--------|---------|
| `{w=N}` | Wait N seconds | `"Wait...{w=1.0}okay"` |
| `{speed=N}` | Set text reveal speed (chars/sec) | `"{speed=30}Slow"` |
| `{color=#HEX}` | Change text color | `"{color=#FF0000}Red{/color}"` |
| `{/color}` | Reset color to default | `"Colored{/color} normal"` |
| `{shake}...{/shake}` | Shake effect | `"{shake}Scary!{/shake}"` |
| `{wave}...{/wave}` | Wave effect | `"{wave}Wavy~{/wave}"` |

### 9.2 Examples

**Wait Command**:
```nms
say Hero "Wait...{w=1.0}is anyone there?"
say Narrator "Three...{w=0.5}two...{w=0.5}one..."
```

**Speed Control**:
```nms
say Hero "{speed=20}Time...seemed...to...slow...down..."
say Hero "{speed=60}Then everything happened fast!"
```

**Color**:
```nms
say Hero "I found a {color=#FFD700}golden key{/color}!"
say Villain "{color=#FF0000}You will pay for this!{/color}"
```

**Combined Effects**:
```nms
say Ghost "{shake}{color=#AAAAAA}Whooooo...{/color}{/shake}"
say Sign "{wave}Welcome to the village!{/wave}"
```

**Escaping Braces**:
```nms
say Programmer "Use \{w=1} to wait in dialogue."
```

---

## 10. Constants

### 10.1 Boolean Constants

```nms
true    // Boolean true
false   // Boolean false
```

**Usage**:
```nms
set flag ready = true
set flag error = false

if true {
    // Always executes
}
```

### 10.2 Position Constants

**Predefined Positions**:
```nms
left      // Left side of screen
center    // Center of screen
right     // Right side of screen
```

**Usage**:
```nms
show Hero at left
show Villain at right
show Narrator at center
```

**Custom Positions**:
```nms
show Hero at (100, 200)   // x=100, y=200 pixels
show Villain at (640, 360) // Screen center (typical)
```

---

## Appendix A: Complete Command Reference

### Commands by Category

**Visual**:
- `show background "<id>"`
- `show <char> at <pos>`
- `show <char> at <pos> with "<expr>"`
- `hide <char>`
- `hide background`
- `move <char> to <pos> duration=<sec>`

**Dialogue**:
- `say <char> "<text>"`
- `say <char> "<text>" voice "<path>"`
- `choice { ... }`

**Audio**:
- `play music "<id>"`
- `play music "<id>" loop=<bool>`
- `play sound "<id>"`
- `stop music`
- `stop music fade=<sec>`

**Flow Control**:
- `goto <scene>`
- `if <cond> { ... }`
- `if <cond> { ... } else { ... }`
- `if <cond> { ... } else if <cond> { ... } else { ... }`

**Variables**:
- `set <var> = <expr>`
- `set flag <name> = <bool>`

**Effects**:
- `transition <type> <duration>`
- `wait <seconds>`

**Functions**:
- `loc("<key>")`

---

## Appendix B: Reserved Keywords

**All commands and keywords**:
```
and, at, background, center, character, choice,
dissolve, duration, else, fade, false, flag,
goto, hide, if, left, loc, loop, move, music,
not, or, play, right, say, scene, set, show,
slide_down, slide_left, slide_right, slide_up,
sound, stop, then, to, transition, true, voice,
wait, with
```

**Cannot be used** as variable, character, or scene names.

---

## Appendix C: Command Execution Model

### Blocking vs Non-Blocking

| Command | Blocking | Description |
|---------|----------|-------------|
| `show` | No | Returns immediately |
| `hide` | No | Returns immediately |
| `move` | No | Animates in background |
| `say` | **Yes** | Waits for player input |
| `choice` | **Yes** | Waits for player selection |
| `play` | No | Starts playback, returns |
| `stop` | No | Stops playback, returns |
| `goto` | **Yes** | Jumps, doesn't return |
| `if` | No | Conditional, executes branch |
| `set` | No | Assigns value, returns |
| `transition` | **Yes** | Waits for effect |
| `wait` | **Yes** | Pauses script |

**Blocking commands** pause script execution until complete.
**Non-blocking commands** return immediately, allowing script to continue.

---

*End of NM Script Standard Library Reference*
