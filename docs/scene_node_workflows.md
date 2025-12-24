# Scene Node Workflows / Рабочие процессы для узлов сцен

> **См. также**: [architecture_overview.md](architecture_overview.md) — обзор архитектуры.
> **См. также**: [nm_script_specification.md](nm_script_specification.md) — спецификация NMScript.

This document describes the two primary workflows for creating scenes in NovelMind: **Visual-First** and **Code-First**.

Этот документ описывает два основных рабочих процесса для создания сцен в NovelMind: **Visual-First** и **Code-First**.

---

## Overview / Обзор

NovelMind supports two approaches to scene development:

| Workflow | Description | Best For |
|----------|-------------|----------|
| **Visual-First** | Create scenes visually with embedded dialogue graphs | Rapid prototyping, non-programmers |
| **Code-First** | Define scenes in NMScript with visual layout support | Version control, complex logic, programmers |
| **Hybrid** | Combine both approaches per scene | Flexibility, team collaboration |

---

## Visual-First Workflow / Визуальный подход

### Концепция
"Создавайте сцены визуально, встраивайте диалоги/выборы внутри каждой сцены, соединяйте сцены вместе"

### Steps / Шаги

#### 1. Create Scene Nodes in Story Graph
- Open the **Story Graph** panel
- Click **Scene** in the node palette (or right-click → Add Node → Scene)
- Each Scene Node represents one complete scene (background + characters + dialogues)
- Connect Scene Nodes to define the story flow

```
Story Graph:
  [Scene: Intro] → [Scene: Cafe Meeting] → [Choice: Trust or Leave] → ...
```

#### 2. Enter Scene for Visual Editing
- **Double-click** on a Scene Node → Opens **Scene View** panel
- Visual canvas shows scene composition
- Drag-and-drop backgrounds, characters, effects
- Use gizmos to position/scale/rotate objects

#### 3. Edit Embedded Dialogue Flow
- Right-click on Scene Node → **Edit Dialogue Flow**
- Opens embedded dialogue graph view
- Add nodes: Dialogue, Choice, Character Animation
- This sub-graph is PART of the Scene Node, not a separate entity
- Defines the sequence of events happening IN THIS SCENE

#### 4. Visual Clarity
- **Story Graph**: Shows scene-to-scene flow (high-level narrative)
- **Scene View**: Shows spatial layout (where things are positioned)
- **Embedded Dialogue Graph**: Shows temporal flow within one scene (what happens when)

### Example / Пример

```
Story Graph:
  [Scene: Intro] → [Scene: Cafe Meeting] → [Choice: Trust or Leave]

Double-click on "Cafe Meeting":
  Opens Scene View:
    - Background: cafe_interior.png
    - Character "Alice" at position (200, 300)
    - Character "Bob" at position (800, 300)

Click "Edit Dialogue Flow":
  Embedded Dialogue Graph:
    [Dialogue: "Alice: Welcome!"]
       → [Dialogue: "Bob: Nice place"]
       → [Choice: "Order coffee / Leave"]
          → [Anim: Bob drinks coffee]
          → [Dialogue: "Bob: Thanks!"]
```

### Key Features / Ключевые особенности
- ✅ No programming required
- ✅ Clear spatial + temporal organization
- ✅ See exactly what happens where
- ✅ Fast prototyping

---

## Code-First Workflow / Программный подход

### Концепция
"Определите структуру сцен визуально, пишите логику в NMScript"

### Steps / Шаги

#### 1. Create Scene Nodes in Story Graph
- Add Scene Nodes with names/IDs
- Connect scenes to define flow
- Set entry point

#### 2. Visual Scene Setup (Optional)
- Double-click on Scene Node → Configure initial layout in Scene View
- Position characters, backgrounds, UI elements
- Save as scene template

#### 3. Write NMScript Code
- Each Scene Node has an associated `.nms` script file
- Right-click on Scene Node → **Open Script**
- Script controls: dialogues, choices, character movement, effects, branching
- Full programmatic control

### Example / Пример

```
Story Graph:
  [Scene: intro] → [Scene: cafe_meeting] → [Scene: cafe_choice]

cafe_meeting.nms:
  scene cafe_meeting:
    background "cafe_interior"

    show alice at center_left
    show bob at center_right

    alice "Welcome to my favorite cafe!"
    bob "Nice place. I like it."

    menu:
      "Order coffee":
        bob "I'll have a cappuccino."
        animate bob drink_coffee
        alice "Good choice!"
        jump cafe_ending_good

      "Leave":
        bob "Actually, I need to go."
        jump cafe_ending_bad
```

### Key Features / Ключевые особенности
- ✅ Full programmatic control with NMScript
- ✅ Version control friendly (text files)
- ✅ Powerful for complex logic
- ✅ Visual scene structure provides context

---

## Hybrid Workflow / Гибридный подход

Combine both approaches:
- Use **Visual-First** for simple scenes with straightforward dialogue
- Use **Code-First** for complex branching and game logic
- Mix within the same project

### Project Settings

Configure default workflow in project settings:

```json
{
  "workflow": "hybrid",
  "defaultWorkflow": "visual-first",
  "allowMixedWorkflows": true
}
```

---

## Scene Node Properties / Свойства узла сцены

| Property | Type | Description |
|----------|------|-------------|
| `sceneId` | string | Unique identifier for the scene |
| `displayName` | string | User-friendly display name |
| `scriptPath` | string | Path to `.nms` file (Code-First) |
| `hasEmbeddedDialogue` | bool | Uses embedded dialogue graph |
| `dialogueCount` | int | Number of dialogue nodes in scene |
| `thumbnailPath` | string | Optional scene preview image |

---

## Context Menu Options / Опции контекстного меню

Right-click on a Scene Node to access:

| Action | Description | Workflow |
|--------|-------------|----------|
| **Edit Scene Layout** | Open Scene View | All |
| **Edit Dialogue Flow** | Open embedded graph | Visual-First, Hybrid |
| **Open Script** | Open `.nms` file | Code-First, Hybrid |
| **Set as Entry** | Mark as starting scene | All |
| **Rename Scene** | Change scene name | All |
| **Duplicate Scene** | Create a copy | All |
| **Delete Scene** | Remove scene | All |

---

## Best Practices / Лучшие практики

### Visual-First
1. Keep embedded dialogue graphs focused on one scene's events
2. Use Scene Nodes for major story beats
3. Preview scenes frequently during development
4. Name scenes descriptively (e.g., "cafe_introduction" not "scene_1")

### Code-First
1. Organize scripts in logical folders
2. Use comments to document complex logic
3. Leverage visual layout for initial positioning
4. Test scripts with Play-in-Editor mode

### Hybrid
1. Choose workflow based on scene complexity
2. Document which scenes use which approach
3. Consider team member preferences
4. Keep complex logic in scripts, simple dialogue visual

---

## Keyboard Shortcuts / Горячие клавиши

| Shortcut | Action |
|----------|--------|
| `Double-click` | Open Scene View for scene |
| `Enter` | Edit selected scene's dialogue flow |
| `Ctrl+E` | Open script for selected scene |
| `Ctrl+D` | Duplicate selected scene |
| `Delete` | Delete selected scene |

---

## Related Topics / Связанные темы

- [Timeline Integration](timeline_integration.md) - Animating scene objects
- [Localization Support](localization_support.md) - Multi-language dialogue
- [Voice-over Integration](voice_integration.md) - Adding audio to dialogue
