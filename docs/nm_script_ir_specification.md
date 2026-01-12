# NM Script Intermediate Representation (IR)

**Graph-Based Representation for Bidirectional Conversion**

Version: 1.0
Last Updated: 2026-01-11

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Node Types](#3-node-types)
4. [Connections & Ports](#4-connections--ports)
5. [Conversion Pipeline](#5-conversion-pipeline)
6. [Voice & Localization](#6-voice--localization)
7. [Development Workflows](#7-development-workflows)
8. [Examples](#8-examples)

---

## 1. Overview

The **Intermediate Representation (IR)** is a graph-based format that serves as the bridge between:

- **NM Script text** (code-first approach)
- **Visual Graph** (node-based editor)
- **Bytecode** (runtime execution)

### Purpose

The IR enables **bidirectional conversion**:

```
┌─────────────┐
│ NM Script   │  (.nms files)
│   Text      │
└──────┬──────┘
       │
       ↓ Parse & Convert
┌──────────────┐
│     AST      │  (Abstract Syntax Tree)
└──────┬───────┘
       │
       ↓ Transform
┌──────────────┐
│      IR      │  ← Graph representation
│    (Graph)   │
└──────┬───────┘
       │
       ├──→ Visual Graph (node editor)
       │
       ├──→ Bytecode (VM execution)
       │
       └──→ Text regeneration
```

### Key Features

1. **Lossless**: Preserves all semantic information
2. **Bidirectional**: Convert between text ↔ graph ↔ bytecode
3. **Extensible**: Supports custom node types
4. **Localizable**: Integrated localization support
5. **Voice-aware**: Voice clip binding metadata

---

## 2. Architecture

### 2.1 Core Components

```
┌──────────────────────────────────────────┐
│            IRGraph                       │
├──────────────────────────────────────────┤
│  • Nodes (vector<IRNode>)                │
│  • Connections (vector<IRConnection>)    │
│  • Entry points (map<scene, NodeId>)     │
│  • Metadata (version, workflow, etc.)    │
└──────────────────────────────────────────┘
           │
           ├─── IRNode
           │     • Type (IRNodeType)
           │     • ID (NodeId)
           │     • Ports (input/output)
           │     • Data (variant)
           │
           └─── IRConnection
                 • Source (PortId)
                 • Target (PortId)
                 • Label (optional)
```

### 2.2 Node Structure

```cpp
struct IRNode {
    NodeId id;                      // Unique identifier
    IRNodeType type;                // Node type (enum)
    std::string label;              // Display name

    std::vector<Port> inputPorts;   // Input connections
    std::vector<Port> outputPorts;  // Output connections

    std::variant<...> data;         // Type-specific data

    Position position;              // Editor position (x, y)
    std::map<std::string,
             std::string> metadata; // Custom metadata
};
```

### 2.3 Connection Structure

```cpp
struct IRConnection {
    PortId source;      // Output port (nodeId, portName)
    PortId target;      // Input port (nodeId, portName)
    std::string label;  // Optional label for debugging
};
```

### 2.4 Port Structure

```cpp
struct PortId {
    NodeId nodeId;         // Node this port belongs to
    std::string portName;  // Port identifier ("exec", "value", etc.)
    bool isOutput;         // true = output, false = input
};
```

---

## 3. Node Types

### 3.1 Structure Nodes

#### SceneStart

**Purpose**: Entry point for a scene

**Ports**:
- **Output**: `exec` (execution flow)

**Data**:
```cpp
struct SceneStartData {
    std::string sceneName;
};
```

**Example**:
```
[SceneStart: intro]
  → exec
```

#### SceneEnd

**Purpose**: Exit point of a scene

**Ports**:
- **Input**: `exec` (execution flow)

**Data**: None

**Example**:
```
  exec ←
[SceneEnd]
```

#### Comment

**Purpose**: Documentation/annotation

**Ports**: None

**Data**:
```cpp
struct CommentData {
    std::string text;
    Color color;  // Optional visual hint
};
```

---

### 3.2 Flow Control Nodes

#### Sequence

**Purpose**: Sequential execution of multiple nodes

**Ports**:
- **Input**: `exec`
- **Output**: `exec` (to next node)

**Data**:
```cpp
struct SequenceData {
    std::vector<NodeId> children;  // Ordered list
};
```

#### Branch (If/Else)

**Purpose**: Conditional branching

**Ports**:
- **Input**: `exec`, `condition` (bool)
- **Output**: `true_exec`, `false_exec`

**Data**:
```cpp
struct BranchData {
    NodeId conditionExpression;
    NodeId trueBranch;
    NodeId falseBranch;  // Optional
};
```

**Example**:
```
     [Branch]
    /        \
true_exec  false_exec
   |          |
  [A]        [B]
```

#### Goto

**Purpose**: Jump to another scene or label

**Ports**:
- **Input**: `exec`
- **Output**: None (terminal)

**Data**:
```cpp
struct GotoData {
    std::string targetScene;
};
```

---

### 3.3 Visual Novel Nodes

#### ShowCharacter

**Purpose**: Display a character on screen

**Ports**:
- **Input**: `exec`, `characterId`, `position`, `expression` (optional)
- **Output**: `exec`

**Data**:
```cpp
struct ShowCharacterData {
    std::string characterId;
    Position position;        // left, center, right, or (x, y)
    std::string expression;   // Optional sprite variant
};
```

#### HideCharacter

**Purpose**: Remove character from screen

**Ports**:
- **Input**: `exec`, `characterId`
- **Output**: `exec`

**Data**:
```cpp
struct HideCharacterData {
    std::string characterId;
};
```

#### ShowBackground

**Purpose**: Display background image

**Ports**:
- **Input**: `exec`, `textureId`
- **Output**: `exec`

**Data**:
```cpp
struct ShowBackgroundData {
    std::string textureId;
};
```

#### Dialogue

**Purpose**: Display dialogue text (say statement)

**Ports**:
- **Input**: `exec`, `characterId`, `text`
- **Output**: `exec`

**Data**:
```cpp
struct DialogueData {
    std::string characterId;
    std::string text;
    std::string localizationKey;  // Auto-generated or manual
    VoiceClipData voiceData;      // Voice binding info
    TranslationStatus translationStatus;
};
```

**Voice Clip Data**:
```cpp
struct VoiceClipData {
    std::string voiceFilePath;
    std::string localizationKey;
    VoiceBindingStatus bindingStatus;  // Unbound, Bound, MissingFile, etc.
    float voiceDuration;               // Cached duration in seconds
    bool autoDetected;
};
```

#### Choice

**Purpose**: Present player with options

**Ports**:
- **Input**: `exec`
- **Output**: `option0_exec`, `option1_exec`, ... (one per option)

**Data**:
```cpp
struct ChoiceData {
    std::vector<ChoiceOption> options;
};

struct ChoiceOption {
    std::string text;
    std::optional<NodeId> condition;  // If present, option is conditional
    NodeId target;                    // Action to execute
};
```

**Example**:
```
      [Choice]
     /    |    \
opt0  opt1  opt2
  |     |     |
[A]   [B]   [C]
```

---

### 3.4 Audio Nodes

#### PlayMusic

**Purpose**: Start background music

**Ports**:
- **Input**: `exec`, `trackId`, `loop` (bool)
- **Output**: `exec`

**Data**:
```cpp
struct PlayMusicData {
    std::string trackId;
    bool loop = true;
};
```

#### StopMusic

**Purpose**: Stop background music

**Ports**:
- **Input**: `exec`, `fadeOutDuration` (optional)
- **Output**: `exec`

**Data**:
```cpp
struct StopMusicData {
    float fadeOutDuration = 0.0f;
};
```

#### PlaySound

**Purpose**: Play sound effect

**Ports**:
- **Input**: `exec`, `soundId`
- **Output**: `exec`

**Data**:
```cpp
struct PlaySoundData {
    std::string soundId;
};
```

---

### 3.5 Effect Nodes

#### Transition

**Purpose**: Visual transition effect

**Ports**:
- **Input**: `exec`, `transitionType`, `duration`
- **Output**: `exec`

**Data**:
```cpp
struct TransitionData {
    TransitionType type;  // fade, dissolve, slide_left, etc.
    float duration;
};
```

#### Wait

**Purpose**: Delay execution

**Ports**:
- **Input**: `exec`, `duration`
- **Output**: `exec`

**Data**:
```cpp
struct WaitData {
    float duration;  // Seconds
};
```

---

### 3.6 Variable Nodes

#### SetVariable

**Purpose**: Assign value to variable

**Ports**:
- **Input**: `exec`, `variableName`, `value`
- **Output**: `exec`

**Data**:
```cpp
struct SetVariableData {
    std::string variableName;
    bool isFlag;  // true for boolean flags
    NodeId valueExpression;
};
```

#### GetVariable

**Purpose**: Read variable value

**Ports**:
- **Input**: `variableName`
- **Output**: `value`

**Data**:
```cpp
struct GetVariableData {
    std::string variableName;
    bool isFlag;
};
```

---

### 3.7 Expression Nodes

#### Expression

**Purpose**: Evaluate arithmetic/logical expression

**Ports**:
- **Input**: Operands (depends on operator)
- **Output**: `result`

**Data**:
```cpp
struct ExpressionData {
    ExpressionType type;  // ADD, SUB, MUL, DIV, EQ, LT, AND, OR, etc.
    std::vector<NodeId> operands;
};
```

**Example**:
```
[GetVar: x]  [GetVar: y]
     \           /
      \         /
    [Expression: ADD]
           |
       result (x + y)
```

---

## 4. Connections & Ports

### 4.1 Port Types

| Port Type | Description | Data Type |
|-----------|-------------|-----------|
| `exec` | Execution flow | Control flow |
| `value` | Data value | int, float, bool, string |
| `condition` | Boolean value | bool |
| `characterId` | Character reference | string |
| `textureId` | Texture reference | string |
| `soundId` | Audio reference | string |

### 4.2 Connection Rules

1. **Type matching**: Output and input ports must be compatible types
2. **Single input**: Input ports can have at most one connection
3. **Multiple outputs**: Output ports can connect to multiple inputs
4. **No cycles**: Execution flow cannot create loops (except explicit Loop nodes)

### 4.3 Connection Example

```
[SceneStart]
     │ exec
     ↓
[ShowBackground] ← textureId: "bg_forest"
     │ exec
     ↓
[ShowCharacter] ← characterId: "Hero", position: "center"
     │ exec
     ↓
[Dialogue] ← characterId: "Hero", text: "Hello!"
     │ exec
     ↓
[Choice]
   ├─→ option0: "Go left" → [GotoScene: left_path]
   └─→ option1: "Go right" → [GotoScene: right_path]
```

---

## 5. Conversion Pipeline

### 5.1 Text → IR

**Process**:
1. **Lex**: Source text → Tokens
2. **Parse**: Tokens → AST
3. **Transform**: AST → IR Graph

**Example**:

**Input (NM Script)**:
```nms
scene intro {
    show background "bg_forest"
    say Hero "Hello, World!"
}
```

**Output (IR)**:
```
Nodes:
  [0] SceneStart(sceneName="intro")
  [1] ShowBackground(textureId="bg_forest")
  [2] Dialogue(characterId="Hero", text="Hello, World!")
  [3] SceneEnd()

Connections:
  [0].exec → [1].exec
  [1].exec → [2].exec
  [2].exec → [3].exec
```

### 5.2 IR → Text

**Process**:
1. **Traverse**: Walk IR graph in execution order
2. **Generate**: Emit NM Script statements
3. **Format**: Pretty-print with indentation

**Example**:

**Input (IR)**:
```
[0] SceneStart("intro")
     ↓
[1] ShowBackground("bg_forest")
     ↓
[2] Dialogue(Hero, "Hello!")
     ↓
[3] SceneEnd
```

**Output (NM Script)**:
```nms
scene intro {
    show background "bg_forest"
    say Hero "Hello!"
}
```

### 5.3 IR → Bytecode

**Process**:
1. **Analyze**: Determine execution order
2. **Emit**: Generate bytecode instructions
3. **Link**: Resolve scene/label addresses

**Example**:

**Input (IR)**:
```
[ShowBackground] → [Dialogue] → [SceneEnd]
```

**Output (Bytecode)**:
```
0: PUSH_STRING 0         ; "bg_forest"
1: SHOW_BACKGROUND 0
2: PUSH_STRING 1         ; "Hello!"
3: PUSH_INT 0            ; Hero ID
4: SAY 0
5: HALT
```

---

## 6. Voice & Localization

### 6.1 Voice Binding

**Voice Binding Status**:

| Status | Description |
|--------|-------------|
| `Unbound` | No voice file assigned |
| `Bound` | Voice file assigned and verified to exist |
| `MissingFile` | Voice file assigned but file not found |
| `AutoMapped` | Automatically mapped by pattern matching |
| `Pending` | Waiting for voice import/recording |

**Voice Clip Data**:
```cpp
struct VoiceClipData {
    std::string voiceFilePath;        // "audio/voices/hero_intro_001.wav"
    std::string localizationKey;      // "scene.intro.dialogue.001"
    VoiceBindingStatus bindingStatus;
    float voiceDuration;              // 2.5 seconds
    bool autoDetected;                // true if auto-mapped
};
```

### 6.2 Localization

**Translation Status**:

| Status | Description |
|--------|-------------|
| `NotLocalizable` | Text not meant for translation |
| `Untranslated` | No translation for current locale |
| `Translated` | Translation exists and complete |
| `NeedsReview` | Translation may be outdated |
| `Missing` | Key exists but translation empty |

**Localization Key Format**:
```
scene.<sceneId>.dialogue.<nodeId>
scene.<sceneId>.choice.<optionIndex>
```

**Example**:
```
scene.intro.dialogue.0001  → "Hello, World!"
scene.intro.dialogue.0002  → "What should I do?"
scene.intro.choice.0       → "Go left"
scene.intro.choice.1       → "Go right"
```

---

## 7. Development Workflows

### 7.1 Workflow Modes

```cpp
enum class DevelopmentWorkflow {
    VisualFirst,  // Use embedded dialogue graphs in scene nodes
    CodeFirst,    // Use NMScript files for dialogue logic
    Hybrid        // Allow both approaches per scene
};
```

#### Visual-First

- **Editor**: Node-based visual graph editor
- **Storage**: IR graph stored directly
- **Export**: Can generate .nms files for version control

**Use Case**: Game designers, non-programmers

#### Code-First

- **Editor**: Text editor with .nms files
- **Storage**: .nms source files
- **Import**: Parsed into IR for visual editing

**Use Case**: Programmers, version control workflows

#### Hybrid

- **Mix**: Some scenes in visual graph, others in code
- **Flexibility**: Choose best tool per scene

**Use Case**: Teams with mixed skill sets

### 7.2 Workflow Comparison

| Feature | Visual-First | Code-First | Hybrid |
|---------|--------------|------------|--------|
| **Editor** | Node graph | Text editor | Both |
| **Version Control** | JSON/Binary | .nms text | Mixed |
| **Learning Curve** | Low | Medium | Medium |
| **Flexibility** | Medium | High | High |
| **Team Collaboration** | Easy | Good | Best |

---

## 8. Examples

### 8.1 Simple Dialogue

**NM Script**:
```nms
scene intro {
    show background "bg_room"
    say Hero "Hello!"
}
```

**IR Graph**:
```
[SceneStart: intro]
        ↓ exec
[ShowBackground]
  data: {textureId: "bg_room"}
        ↓ exec
[Dialogue]
  data: {characterId: "Hero", text: "Hello!"}
        ↓ exec
[SceneEnd]
```

### 8.2 Conditional Branch

**NM Script**:
```nms
scene check {
    if points >= 100 {
        say Hero "I win!"
    } else {
        say Hero "I lose!"
    }
}
```

**IR Graph**:
```
[SceneStart: check]
        ↓
[GetVariable: points]
        ↓ value
[Expression: GE] ← const: 100
        ↓ result
[Branch]
   ├─→ true_exec → [Dialogue: "I win!"]
   └─→ false_exec → [Dialogue: "I lose!"]
```

### 8.3 Choice with Consequences

**NM Script**:
```nms
scene choice_demo {
    say Hero "What should I do?"

    choice {
        "Go left" -> {
            set flag went_left = true
            goto left_path
        }
        "Go right" -> goto right_path
    }
}
```

**IR Graph**:
```
[SceneStart: choice_demo]
        ↓
[Dialogue: "What should I do?"]
        ↓
[Choice]
  options:
    [0] "Go left"
          ↓
        [Sequence]
          ├─→ [SetVariable: went_left = true]
          └─→ [Goto: left_path]
    [1] "Go right"
          ↓
        [Goto: right_path]
```

### 8.4 Complex Expression

**NM Script**:
```nms
set total = (points * 2) + bonus
```

**IR Graph**:
```
[GetVariable: points]
        ↓ value
[Expression: MUL] ← const: 2
        ↓ result
[Expression: ADD] ← [GetVariable: bonus]
        ↓ result
[SetVariable: total]
```

---

## Appendix A: Complete Node Type Reference

| Node Type | Category | Inputs | Outputs | Description |
|-----------|----------|--------|---------|-------------|
| `SceneStart` | Structure | - | exec | Scene entry point |
| `SceneEnd` | Structure | exec | - | Scene exit point |
| `Comment` | Structure | - | - | Documentation |
| `Scene` | Structure | - | exec | Complete scene (visual-first) |
| `Sequence` | Flow | exec | exec | Sequential execution |
| `Branch` | Flow | exec, condition | true_exec, false_exec | If/else |
| `Switch` | Flow | exec, value | case0_exec, case1_exec, ... | Multi-way branch |
| `Loop` | Flow | exec, condition | body_exec, exit_exec | Loop construct |
| `Goto` | Flow | exec | - | Jump to scene |
| `Label` | Flow | - | exec | Jump target |
| `ShowCharacter` | VN | exec, characterId, position | exec | Display character |
| `HideCharacter` | VN | exec, characterId | exec | Hide character |
| `ShowBackground` | VN | exec, textureId | exec | Display background |
| `Dialogue` | VN | exec, characterId, text | exec | Say statement |
| `Choice` | VN | exec | option0_exec, ... | Player choice |
| `ChoiceOption` | VN | - | exec | Single option |
| `PlayMusic` | Audio | exec, trackId, loop | exec | Play music |
| `StopMusic` | Audio | exec, fadeOut | exec | Stop music |
| `PlaySound` | Audio | exec, soundId | exec | Play sound |
| `Transition` | Effect | exec, type, duration | exec | Visual effect |
| `Wait` | Effect | exec, duration | exec | Delay |
| `SetVariable` | Variable | exec, name, value | exec | Assign variable |
| `GetVariable` | Variable | name | value | Read variable |
| `Expression` | Advanced | operands | result | Evaluate expression |
| `FunctionCall` | Advanced | exec, args | exec, return | Call function |
| `Custom` | Advanced | - | - | User-defined |

---

## Appendix B: IR JSON Schema (Example)

```json
{
  "version": "1.0",
  "workflow": "CodeFirst",
  "nodes": [
    {
      "id": 0,
      "type": "SceneStart",
      "label": "intro",
      "data": {
        "sceneName": "intro"
      },
      "position": {"x": 100, "y": 100},
      "outputs": ["exec"]
    },
    {
      "id": 1,
      "type": "Dialogue",
      "label": "Hero says hello",
      "data": {
        "characterId": "Hero",
        "text": "Hello, World!",
        "localizationKey": "scene.intro.dialogue.001",
        "voiceData": {
          "voiceFilePath": "audio/voices/hero_hello.wav",
          "bindingStatus": "Bound",
          "voiceDuration": 1.5
        }
      },
      "position": {"x": 100, "y": 200},
      "inputs": ["exec"],
      "outputs": ["exec"]
    }
  ],
  "connections": [
    {
      "source": {"nodeId": 0, "portName": "exec", "isOutput": true},
      "target": {"nodeId": 1, "portName": "exec", "isOutput": false}
    }
  ]
}
```

---

*End of NM Script IR Specification*
