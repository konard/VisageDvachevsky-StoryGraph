# Scene Preview ↔ Story Graph Integration Pipeline

> **Status**: Design Document (Issue #205)
> **See also**: [architecture_overview.md](architecture_overview.md) — Architecture overview
> **See also**: [gui_architecture.md](gui_architecture.md) — GUI architecture
> **See also**: [workflow_pipeline.md](workflow_pipeline.md) — Workflow pipeline
> **See also**: [scene_node_workflows.md](scene_node_workflows.md) — Scene node workflows

## Overview

This document defines the complete pipeline for integrating **Scene Preview** (SceneViewPanel) and **Story Graph** (NMStoryGraphPanel) components in the NovelMind editor. It addresses:

1. How scenes are created and edited in Scene Preview
2. How Story Graph manages the sequence of scenes, dialogues, and choices
3. How these components synchronize
4. Current implementation gaps and proposed solutions

---

## User Context

### Desired Workflow
> "In Story Graph we organize what's inside the novel, in what order, etc.
> The scenes themselves (asset positions, effects, etc.) we create in Scene Preview"

### Working Mode
> "Parallel work" - you can work in any order, panels synchronize automatically

### Story Graph Content
- Dialogues and text ✓
- Choices and branching ✓
- Scene transitions ✓
- But NOT visual scene assembly (that's in Scene Preview)

### Connection Method
> "By scene name in the node" - in Story Graph node, a `scene_id` is specified which loads the corresponding `.nmscene` file

---

## Architectural Principles

### Separation of Concerns

| Component | Responsibility | Does NOT Handle |
|-----------|---------------|-----------------|
| **Scene Preview** | Visual composition of scenes: backgrounds, characters, positions, effects | Dialogue sequence, choice logic, transitions |
| **Story Graph** | Narrative flow: dialogues, choices, conditions, scene transitions | Visual layout, object positions, effects configuration |

### Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PROJECT DATA                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─────────────────────────┐      ┌──────────────────────────┐     │
│  │   story_graph.json      │      │   scenes/*.nmscene       │     │
│  │   ─────────────────     │      │   ──────────────────     │     │
│  │   - Node positions      │      │   - Object positions     │     │
│  │   - Connections         │      │   - Transforms           │     │
│  │   - Dialogue text       │      │   - Z-order              │     │
│  │   - Choices             │      │   - Properties           │     │
│  │   - Scene ID refs       │◄────►│   - Asset references     │     │
│  └───────────┬─────────────┘      └────────────┬─────────────┘     │
│              │                                  │                    │
│              ▼                                  ▼                    │
│  ┌─────────────────────────┐      ┌──────────────────────────┐     │
│  │   Story Graph Panel     │      │   Scene View Panel       │     │
│  │   ──────────────────    │      │   ────────────────       │     │
│  │   - Node editing        │      │   - Object editing       │     │
│  │   - Connection drawing  │      │   - Gizmo transforms     │     │
│  │   - Dialogue editing    │◄────►│   - Visual preview       │     │
│  │   - Flow visualization  │      │   - Runtime preview      │     │
│  └───────────┬─────────────┘      └────────────┬─────────────┘     │
│              │                                  │                    │
│              └──────────────┬───────────────────┘                   │
│                             ▼                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Event Bus + Mediators                      │   │
│  │   ─────────────────────────────────────────────────────────   │   │
│  │   - SelectionMediator: Sync selection across panels          │   │
│  │   - WorkflowMediator: Scene loading, Script navigation       │   │
│  │   - PropertyMediator: Property changes propagation           │   │
│  │   - PlaybackMediator: Animation preview synchronization      │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

### Scene Preview (SceneViewPanel)

**Primary Purpose**: Visual editing of scene composition

**Owns**:
- Object positions and transforms (x, y, rotation, scale)
- Object visibility and alpha
- Z-order (layering)
- Visual effects configuration
- Asset references (backgrounds, sprites)
- Scene document persistence (`.nmscene` files)

**Receives from Story Graph**:
- Current scene ID (for loading)
- Dialogue preview (speaker, text, choices)
- Runtime snapshots during play mode

**Sends to Story Graph**:
- Scene modification notifications
- Object selection events
- Thumbnail updates

**Key Files**:
| File | Lines | Purpose |
|------|-------|---------|
| `nm_scene_view_panel.hpp` | 289 | Main header |
| `nm_scene_view_panel.cpp` | ~300 | Main logic |
| `nm_scene_view_panel_document.cpp` | ~250 | Load/save .nmscene |
| `nm_scene_view_panel_objects.cpp` | ~300 | CRUD for objects |
| `nm_scene_view_panel_runtime.cpp` | 574 | Runtime integration |
| `nm_scene_view_panel_ui.cpp` | 821 | UI setup |
| `nm_scene_view_scene.cpp` | 611 | Graphics scene |

### Story Graph (NMStoryGraphPanel)

**Primary Purpose**: Visual narrative flow editor

**Owns**:
- Node structure and connections
- Dialogue content (speaker, text)
- Choice options and targets
- Condition expressions
- Scene ID references (linking to `.nmscene` files)
- Graph layout persistence

**Receives from Scene Preview**:
- Scene thumbnail updates
- Scene existence validation
- Selection synchronization

**Sends to Scene Preview**:
- Scene load requests
- Dialogue preview updates
- Runtime state during play

**Key Files**:
| File | Lines | Purpose |
|------|-------|---------|
| `nm_story_graph_panel.hpp` | 704 | Main header |
| `nm_story_graph_panel.cpp` | 780 | Main logic |
| `nm_story_graph_panel_handlers.cpp` | 1383 | Event handlers |
| `nm_story_graph_node.cpp` | ~500 | Node rendering |
| `nm_story_graph_connection.cpp` | ~300 | Connection rendering |
| `nm_story_graph_minimap.cpp` | ~200 | Minimap navigation |

---

## Node Types and Scene Document Relationship

### Story Graph Node Types

```cpp
// Scene Node - reference to a visual scene
struct SceneNode {
    QString sceneId;              // Links to {sceneId}.nmscene
    QString displayName;          // User-friendly name
    QString thumbnailPath;        // Preview image path
    bool hasEmbeddedDialogue;     // Has dialogue within scene
    int dialogueCount;            // Number of embedded dialogues
};

// Dialogue Node - character speech
struct DialogueNode {
    QString speaker;              // Character speaking
    QString dialogueText;         // What they say
    QString voiceClipPath;        // Optional voice-over
    QString localizationKey;      // Translation key
};

// Choice Node - player decision
struct ChoiceNode {
    QStringList choiceOptions;    // Options to choose
    QHash<QString, QString> choiceTargets; // Option → target node
};

// Condition Node - branching logic
struct ConditionNode {
    QString conditionExpression;  // Boolean expression
    QStringList conditionOutputs; // e.g., ["true", "false"]
    QHash<QString, QString> conditionTargets; // Output → target node
};
```

### Scene Document Structure (.nmscene)

```cpp
struct SceneDocumentObject {
    std::string id;                  // Unique object ID
    std::string name;                // Display name
    std::string type;                // "Background", "Character", "UI", "Effect"
    float x, y, rotation;            // Transform
    float scaleX, scaleY, alpha;     // Scale and opacity
    bool visible;                    // Visibility flag
    int zOrder;                      // Layering order
    std::unordered_map<std::string, std::string> properties; // Custom props
};

struct SceneDocument {
    std::string sceneId;             // Matches Scene Node sceneId
    std::vector<SceneDocumentObject> objects; // Scene objects
};
```

**Relationship**:
- Scene Node `sceneId` → loads `scenes/{sceneId}.nmscene`
- Multiple Story Graph nodes can reference the same scene
- Scene document stores ONLY visual composition, not narrative content

---

## Synchronization Events

### Event Bus Architecture

The editor uses a centralized event bus for decoupled communication:

```cpp
// Current implementation in event_bus.hpp
class EventBus {
public:
    static EventBus& instance();

    template<typename Event>
    void publish(const Event& event);

    template<typename Event>
    void subscribe(std::function<void(const Event&)> handler);
};
```

### Existing Events (panel_events.hpp)

| Event | Source | Handlers | Purpose |
|-------|--------|----------|---------|
| `SceneObjectSelectedEvent` | Scene View | Inspector, Hierarchy | Object selection sync |
| `StoryGraphNodeSelectedEvent` | Story Graph | Inspector | Node selection sync |
| `StoryGraphNodeActivatedEvent` | Story Graph | Workflow Mediator | Double-click on node |
| `SceneNodeDoubleClickedEvent` | Story Graph | Scene View | Open scene for editing |
| `LoadSceneDocumentRequestedEvent` | Story Graph | Scene View | Load a scene |
| `SceneObjectPositionChangedEvent` | Scene View | Inspector | Transform sync |
| `SceneObjectsChangedEvent` | Scene View | Story Graph | Thumbnail refresh |

### Proposed New Events

```cpp
// Scene lifecycle events
struct SceneCreatedEvent {
    QString sceneId;
    QString displayName;
};

struct SceneRenamedEvent {
    QString oldSceneId;
    QString newSceneId;
};

struct SceneDeletedEvent {
    QString sceneId;
};

struct SceneDocumentModifiedEvent {
    QString sceneId;
    bool needsThumbnailRefresh;
};

// Cross-panel synchronization
struct SyncSceneToGraphEvent {
    QString sceneId;
    QImage thumbnail;
};

struct DialoguePreviewRequestedEvent {
    QString speaker;
    QString text;
    QStringList choices;
};
```

---

## Workflows

### Workflow 1: Scene-First (Visual-First)

```
┌──────────────────────────────────────────────────────────────────┐
│ WORKFLOW: SCENE-FIRST                                             │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. CREATE SCENE IN SCENE PREVIEW                                │
│     ├── Open Scene View Panel                                    │
│     ├── Click "New Scene" → Enter scene name (e.g., "forest")   │
│     ├── Add background: forest_bg.png                            │
│     ├── Add character: alice.png at position (200, 400)         │
│     ├── Add character: bob.png at position (600, 400)           │
│     └── Save → Creates scenes/forest.nmscene                     │
│                                                                   │
│  2. LINK IN STORY GRAPH                                          │
│     ├── Open Story Graph Panel                                   │
│     ├── Create Scene Node → Set sceneId = "forest"              │
│     ├── Scene Node auto-validates → Shows green check           │
│     └── Scene Node shows thumbnail from .nmscene                 │
│                                                                   │
│  3. ADD DIALOGUE FLOW                                            │
│     ├── Add Dialogue Node → "Alice: Welcome to the forest!"     │
│     ├── Add Dialogue Node → "Bob: It's beautiful here."         │
│     ├── Add Choice Node → "Explore / Leave"                      │
│     └── Connect: Scene → Dialogue1 → Dialogue2 → Choice         │
│                                                                   │
│  4. PREVIEW                                                       │
│     ├── Play Mode → Loads forest.nmscene                         │
│     ├── Shows characters at saved positions                      │
│     └── Executes dialogue sequence from Story Graph              │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Workflow 2: Graph-First

```
┌──────────────────────────────────────────────────────────────────┐
│ WORKFLOW: GRAPH-FIRST                                             │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. CREATE STORY STRUCTURE                                       │
│     ├── Open Story Graph Panel                                   │
│     ├── Create Scene Node → "intro_scene"                        │
│     ├── Create Dialogue Nodes for intro                          │
│     ├── Create Scene Node → "main_hall"                          │
│     └── Connect scenes and dialogues                             │
│                                                                   │
│  2. SCENE NODE CREATES PLACEHOLDER                               │
│     ├── Scene Node with sceneId = "intro_scene"                  │
│     ├── No .nmscene file exists → Shows warning icon            │
│     └── Right-click → "Create Scene Document"                    │
│           → Creates scenes/intro_scene.nmscene (empty template)  │
│                                                                   │
│  3. EDIT VISUAL COMPOSITION                                      │
│     ├── Double-click Scene Node                                  │
│     ├── Opens Scene View with intro_scene.nmscene               │
│     ├── Add backgrounds, characters                              │
│     └── Save → .nmscene updated                                  │
│                                                                   │
│  4. STORY GRAPH UPDATES                                          │
│     ├── Receives SceneDocumentModifiedEvent                      │
│     ├── Scene Node shows updated thumbnail                       │
│     └── Warning icon removed (valid scene)                       │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Workflow 3: Parallel Editing

```
┌──────────────────────────────────────────────────────────────────┐
│ WORKFLOW: PARALLEL EDITING                                        │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  SIMULTANEOUS WORK IN BOTH PANELS                                │
│                                                                   │
│  ┌─────────────────────┐     ┌─────────────────────┐            │
│  │  SCENE VIEW PANEL   │     │  STORY GRAPH PANEL  │            │
│  ├─────────────────────┤     ├─────────────────────┤            │
│  │ - Editing positions │     │ - Editing dialogues │            │
│  │ - Adding effects    │     │ - Adding choices    │            │
│  │ - Adjusting z-order │     │ - Setting conditions│            │
│  └──────────┬──────────┘     └──────────┬──────────┘            │
│             │                            │                       │
│             └────────────┬───────────────┘                       │
│                          │                                       │
│                          ▼                                       │
│             ┌─────────────────────────┐                         │
│             │      EVENT BUS          │                         │
│             ├─────────────────────────┤                         │
│             │ SelectionMediator       │                         │
│             │ PropertyMediator        │                         │
│             │ WorkflowMediator        │                         │
│             └─────────────────────────┘                         │
│                          │                                       │
│                          ▼                                       │
│             ┌─────────────────────────┐                         │
│             │  SYNCHRONIZED STATE     │                         │
│             │  - Current scene        │                         │
│             │  - Selected objects     │                         │
│             │  - Dialogue preview     │                         │
│             └─────────────────────────┘                         │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## Scene Registry System (Proposed)

### Purpose

Centralized management of scene IDs, validation, and cross-reference tracking.

### Interface

```cpp
class SceneRegistry {
public:
    static SceneRegistry& instance();

    // Scene management
    QString registerScene(const QString& name);  // Returns new unique ID
    bool renameScene(const QString& oldId, const QString& newId);
    bool deleteScene(const QString& sceneId);

    // Queries
    bool sceneExists(const QString& sceneId) const;
    QString getScenePath(const QString& sceneId) const;
    QStringList getAllSceneIds() const;
    QStringList getScenesInDirectory(const QString& dir) const;

    // Validation
    QStringList getOrphanedSceneDocuments() const;  // .nmscene without refs
    QStringList getInvalidSceneReferences() const;  // Scene nodes with missing .nmscene

    // Thumbnails
    void generateThumbnail(const QString& sceneId);
    QString getThumbnailPath(const QString& sceneId) const;

    // Cross-references
    QStringList getNodesReferencingScene(const QString& sceneId) const;

signals:
    void sceneRegistered(const QString& sceneId);
    void sceneRenamed(const QString& oldId, const QString& newId);
    void sceneDeleted(const QString& sceneId);
    void thumbnailUpdated(const QString& sceneId);
};
```

### Storage Format

```json
// project/scene_registry.json
{
  "version": 1,
  "scenes": [
    {
      "id": "intro_scene",
      "displayName": "Introduction",
      "documentPath": "scenes/intro_scene.nmscene",
      "thumbnailPath": "scenes/.thumbnails/intro_scene.png",
      "created": "2026-01-08T12:00:00Z",
      "modified": "2026-01-08T13:30:00Z"
    },
    {
      "id": "forest_clearing",
      "displayName": "Forest Clearing",
      "documentPath": "scenes/forest_clearing.nmscene",
      "thumbnailPath": "scenes/.thumbnails/forest_clearing.png",
      "created": "2026-01-08T12:15:00Z",
      "modified": "2026-01-08T14:00:00Z"
    }
  ]
}
```

### Benefits

1. **Centralized ID management**: Single source of truth for scene identifiers
2. **Reference validation**: Detect broken links before runtime
3. **Easy renaming**: Update registry, not individual files
4. **Thumbnail caching**: Efficient preview generation
5. **Cross-reference tracking**: Know which nodes use which scenes

---

## Runtime Integration

### Current Implementation

```cpp
// EditorRuntimeHost handles play mode
class EditorRuntimeHost {
public:
    // Play control
    void start();
    void pause();
    void stop();
    void step();

    // State inspection
    SceneSnapshot getSceneSnapshot() const;
    QVariantMap getVariables() const;
    QStringList getCallStack() const;

    // Callbacks
    OnStateChanged      // Play state changes
    OnSceneChanged      // Scene transition
    OnDialogueChanged   // New dialogue
    OnChoicesChanged    // Choice menu shown
};

// Scene View integration
void NMSceneViewPanel::applyRuntimeSnapshot(const SceneSnapshot& snapshot) {
    // Hide editor objects
    hideEditorObjects();

    // Show runtime objects
    for (const auto& obj : snapshot.objects) {
        createRuntimeObject(obj);
    }

    // Update dialogue overlay
    if (snapshot.dialogueVisible) {
        showDialogueOverlay(snapshot.dialogueSpeaker, snapshot.dialogueText);
    }

    // Update choice menu
    if (snapshot.choiceMenuVisible) {
        showChoiceMenu(snapshot.choiceOptions);
    }
}
```

### Runtime Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ RUNTIME EXECUTION FLOW                                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. PLAY BUTTON PRESSED                                          │
│     └── EditorRuntimeHost::start()                              │
│                                                                  │
│  2. COMPILE PROJECT                                              │
│     ├── Parse story_graph.json or .nms files                    │
│     ├── Generate bytecode / IR                                   │
│     └── Validate all scene references                           │
│                                                                  │
│  3. SCENE CHANGE EVENT                                           │
│     ├── ScriptRuntime emits SceneChange(sceneId)                │
│     ├── EditorRuntimeHost::onRuntimeEvent(SceneChange)          │
│     ├── Load scenes/{sceneId}.nmscene                           │
│     └── Apply to Scene View via applyRuntimeSnapshot()          │
│                                                                  │
│  4. DIALOGUE EVENT                                               │
│     ├── ScriptRuntime emits DialogueStart(speaker, text)        │
│     ├── EditorRuntimeHost::onRuntimeEvent(DialogueStart)        │
│     └── Scene View shows dialogue overlay                       │
│                                                                  │
│  5. CHOICE EVENT                                                 │
│     ├── ScriptRuntime emits ChoiceMenu(options)                 │
│     ├── Scene View shows choice buttons                         │
│     ├── User clicks option                                       │
│     └── ScriptRuntime::selectChoice(index)                      │
│                                                                  │
│  6. STOP BUTTON PRESSED                                          │
│     ├── EditorRuntimeHost::stop()                               │
│     └── Scene View restores editor objects                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Current Gaps and Solutions

### Gap 1: Duplicate Content Risk

**Problem**: Both Scene Preview and Story Graph can contain dialogue information.

**Current State**:
- Story Graph: Owns dialogue text (speaker, text)
- Scene Preview: Shows dialogue preview via `setStoryPreview()`
- Scene Preview does NOT edit dialogue content

**Solution**: ✅ Already correctly implemented - Story Graph is authoritative for dialogue content.

### Gap 2: Scene Node ↔ Scene Document Linking

**Problem**: No automatic validation of scene ID references.

**Current State**:
- Scene Node stores `sceneId` as string
- `.nmscene` files are named `{sceneId}.nmscene`
- No validation that referenced file exists

**Solution**: Implement SceneRegistry with validation:
```cpp
// In Story Graph Panel
void NMStoryGraphPanel::validateSceneNode(NMGraphNodeItem* node) {
    const QString sceneId = node->sceneId();
    if (!SceneRegistry::instance().sceneExists(sceneId)) {
        node->setValidationState(ValidationState::Error);
        node->setValidationMessage(tr("Scene '%1' not found").arg(sceneId));
    }
}
```

### Gap 3: Missing Auto-Sync

**Problem**: Changes in one panel don't auto-update the other.

**Current State**:
- Inspector changes apply to Story Graph ✓
- Scene Preview does NOT auto-update from Story Graph changes ❌
- Scene rename doesn't update Story Graph references ❌

**Solution**: Event-driven synchronization:
```cpp
// Scene renamed in Scene Preview
void NMSceneViewPanel::renameScene(const QString& oldId, const QString& newId) {
    // 1. Save document with new name
    saveSceneDocument(newId);

    // 2. Remove old file
    QFile::remove(getSceneDocumentPath(oldId));

    // 3. Update registry
    SceneRegistry::instance().renameScene(oldId, newId);

    // 4. Emit event - Story Graph will receive and update
    // (handled automatically by SceneRegistry signal)
}

// Story Graph receives rename event
void NMStoryGraphPanel::onSceneRenamed(const QString& oldId, const QString& newId) {
    for (auto* node : m_scene->nodes()) {
        if (node->sceneId() == oldId) {
            node->setSceneId(newId);
            node->update();
        }
    }
    saveGraphLayout();
}
```

### Gap 4: Scene ID Management

**Problem**: No centralized scene ID management.

**Current State**:
- Scene IDs are user-entered strings
- No uniqueness validation
- No standard naming convention

**Solution**: SceneRegistry with ID generation:
```cpp
QString SceneRegistry::registerScene(const QString& displayName) {
    // Sanitize name to valid ID
    QString id = sanitizeToId(displayName);

    // Ensure uniqueness
    if (m_scenes.contains(id)) {
        id = makeUnique(id);
    }

    // Register
    SceneInfo info;
    info.id = id;
    info.displayName = displayName;
    info.documentPath = QString("scenes/%1.nmscene").arg(id);
    info.created = QDateTime::currentDateTime();
    m_scenes[id] = info;

    emit sceneRegistered(id);
    return id;
}
```

---

## User Stories

### US-1: Create Scene Visual-First
**As a** visual novel author
**I want to** create a scene visually in Scene Preview
**So that** I can see exactly how it will look in-game

**Acceptance Criteria**:
- [ ] Can create new scene from Scene View menu
- [ ] Can add/position backgrounds and characters
- [ ] Scene auto-saves to `.nmscene` file
- [ ] Scene appears in Story Graph node picker

### US-2: Link Scene to Story Graph
**As a** narrative designer
**I want to** reference a visual scene from Story Graph
**So that** the scene plays at the right point in the story

**Acceptance Criteria**:
- [ ] Scene Node shows dropdown of available scenes
- [ ] Invalid scene references show warning icon
- [ ] Double-click Scene Node opens Scene View
- [ ] Scene thumbnail shows in Story Graph

### US-3: Parallel Editing
**As a** content creator
**I want to** edit scenes and dialogues simultaneously
**So that** I can work efficiently without switching contexts

**Acceptance Criteria**:
- [ ] Changes in Scene View reflect in Story Graph thumbnails
- [ ] Changes in Story Graph dialogues show in Scene View preview
- [ ] Selection syncs between panels
- [ ] No data loss when switching panels

### US-4: Scene Rename
**As a** project organizer
**I want to** rename a scene
**So that** I can maintain consistent naming conventions

**Acceptance Criteria**:
- [ ] Rename from Scene View updates all Story Graph references
- [ ] Rename from Story Graph updates `.nmscene` filename
- [ ] No broken references after rename
- [ ] Undo/redo works for rename

### US-5: Validate Before Play
**As a** developer
**I want to** validate scene references before play mode
**So that** I catch errors early

**Acceptance Criteria**:
- [ ] Missing `.nmscene` files show in Diagnostics panel
- [ ] Scene Nodes with invalid references are highlighted
- [ ] Play mode shows clear error for missing scenes
- [ ] "Fix All" option creates missing scene documents

---

## Implementation Checklist

### Phase 1: Scene Registry System
- [ ] Create `SceneRegistry` class
- [ ] Implement `scene_registry.json` persistence
- [ ] Add scene validation to Story Graph nodes
- [ ] Add scene picker dropdown to Inspector

### Phase 2: Auto-Sync Events
- [ ] Implement `SceneRenamedEvent`
- [ ] Implement `SceneDocumentModifiedEvent`
- [ ] Implement `SceneDeletedEvent`
- [ ] Add handlers in both panels

### Phase 3: UI Improvements
- [ ] Scene ID picker in Inspector for Scene nodes
- [ ] "Create Scene" button from Story Graph
- [ ] Warning icons for invalid scene references
- [ ] Thumbnail generation and caching

### Phase 4: Validation & Error Handling
- [ ] Pre-play validation of all scene references
- [ ] Diagnostics panel integration
- [ ] "Create missing scene" quick-fix
- [ ] Orphan detection for unused `.nmscene` files

---

## Follow-up Issues

Based on this design document, the following implementation issues have been created:

1. **#211 - Implement Scene Registry System** (HIGH priority)
   - Create SceneRegistry class
   - Persistence to scene_registry.json
   - Thumbnail caching

2. **#212 - Add Scene ID Picker to Inspector** (HIGH priority)
   - Dropdown for existing scenes
   - "Create New Scene" button
   - "Edit Scene" button

3. **#213 - Add Auto-sync Events** (MEDIUM priority)
   - SceneRenamedEvent handling
   - SceneDocumentModifiedEvent handling
   - Thumbnail refresh on scene change

4. **#214 - Improve Story Preview in Scene View** (MEDIUM priority)
   - Better dialogue overlay
   - Choice buttons rendering
   - Navigation controls

5. **#215 - Add Validation & Error Handling** (MEDIUM priority)
   - Pre-play validation
   - Diagnostics integration
   - Warning icons on nodes

6. **#216 - Create Scene Templates** (LOW priority)
   - Empty scene template
   - Dialogue scene template
   - Choice scene template

---

## Related Components

- **Scene Preview** (`nm_scene_view_panel.*`)
- **Story Graph Panel** (`nm_story_graph_panel.*`)
- **Inspector Panel** (`nm_inspector_panel.*`)
- **Selection Mediator** (`selection_mediator.*`)
- **Workflow Mediator** (`workflow_mediator.*`)
- **Property Mediator** (`property_mediator.*`)
- **Event Bus** (`event_bus.*`, `qt_event_bus.*`)
- **Project Manager** (`project_manager.*`)
- **Scene Document** (`scene_document.*`)
- **Scene Registry** (NEW - to be implemented)

---

## Appendix: Architecture Diagram

```
                           ┌─────────────────────────────────────────────────────────┐
                           │                    NovelMind Editor                      │
                           │                                                          │
  ┌────────────────────────┼──────────────────────────────────────────────────────────┼────────────────────────┐
  │                        │                     GUI Layer                            │                        │
  │  ┌─────────────────────▼─────────────────────┐      ┌────────────────────────────▼───────────────────────┐│
  │  │           Scene View Panel                │      │              Story Graph Panel                     ││
  │  │  ┌─────────────────────────────────────┐  │      │  ┌─────────────────────────────────────────────┐  ││
  │  │  │     NMSceneGraphicsScene            │  │      │  │           NMStoryGraphScene                 │  ││
  │  │  │  ┌────────────────────────────────┐ │  │      │  │  ┌──────────────────────────────────────┐  │  ││
  │  │  │  │  NMSceneObject (Background)    │ │  │      │  │  │  NMGraphNodeItem (Scene)            │  │  ││
  │  │  │  │  NMSceneObject (Character)     │ │  │      │  │  │  NMGraphNodeItem (Dialogue)         │  │  ││
  │  │  │  │  NMSceneObject (Effect)        │ │  │      │  │  │  NMGraphNodeItem (Choice)           │  │  ││
  │  │  │  └────────────────────────────────┘ │  │      │  │  │  NMGraphConnectionItem              │  │  ││
  │  │  │  ┌────────────────────────────────┐ │  │      │  │  └──────────────────────────────────────┘  │  ││
  │  │  │  │  NMTransformGizmo              │ │  │      │  └─────────────────────────────────────────────┘  ││
  │  │  │  │  NMSceneInfoOverlay            │ │  │      │  ┌─────────────────────────────────────────────┐  ││
  │  │  │  └────────────────────────────────┘ │  │      │  │  NMNodePalette                             │  ││
  │  │  └─────────────────────────────────────┘  │      │  │  NMMinimap                                 │  ││
  │  │  ┌─────────────────────────────────────┐  │      │  └─────────────────────────────────────────────┘  ││
  │  │  │  Dialogue Preview Overlay          │  │      │                                                    ││
  │  │  └─────────────────────────────────────┘  │      │                                                    ││
  │  └───────────────────────┬───────────────────┘      └───────────────────────────┬────────────────────────┘│
  │                          │                                                       │                        │
  └──────────────────────────┼───────────────────────────────────────────────────────┼────────────────────────┘
                             │                                                       │
                             ▼                                                       ▼
  ┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │                                         Editor Core Layer                                                 │
  │                                                                                                           │
  │  ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐ │
  │  │                                           Event Bus                                                  │ │
  │  │  ┌───────────────┐ ┌────────────────┐ ┌────────────────┐ ┌────────────────┐ ┌────────────────────┐  │ │
  │  │  │ Selection     │ │ Workflow       │ │ Property       │ │ Playback       │ │ Scene Registry     │  │ │
  │  │  │ Mediator      │ │ Mediator       │ │ Mediator       │ │ Mediator       │ │ (NEW)              │  │ │
  │  │  └───────────────┘ └────────────────┘ └────────────────┘ └────────────────┘ └────────────────────┘  │ │
  │  └─────────────────────────────────────────────────────────────────────────────────────────────────────┘ │
  │                                                                                                           │
  │  ┌───────────────────────────────┐  ┌───────────────────────────────┐  ┌──────────────────────────────┐  │
  │  │      Project Manager          │  │     Undo/Redo System          │  │    Editor Runtime Host       │  │
  │  │  - Project loading/saving     │  │  - Command pattern            │  │  - Play/Pause/Stop           │  │
  │  │  - Asset management           │  │  - History stack              │  │  - Script runtime            │  │
  │  │  - Settings                   │  │  - Merge operations           │  │  - Scene snapshots           │  │
  │  └───────────────────────────────┘  └───────────────────────────────┘  └──────────────────────────────┘  │
  │                                                                                                           │
  └─────────────────────────────────────────────────────────────────────────────────────────────────────────┬─┘
                                                                                                             │
                                                                                                             ▼
  ┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │                                              Data Layer                                                       │
  │                                                                                                               │
  │  ┌─────────────────────────┐  ┌─────────────────────────┐  ┌─────────────────────────────────────────────┐   │
  │  │   story_graph.json      │  │   scenes/*.nmscene      │  │   scene_registry.json (NEW)                │   │
  │  │   ─────────────────     │  │   ──────────────────    │  │   ──────────────────────                   │   │
  │  │   - Node positions      │  │   - Object positions    │  │   - Scene ID → path mapping               │   │
  │  │   - Node connections    │  │   - Transforms          │  │   - Thumbnails                            │   │
  │  │   - Dialogue content    │  │   - Asset references    │  │   - Metadata                              │   │
  │  │   - Choice options      │  │   - Z-order             │  │   - Timestamps                            │   │
  │  │   - Scene ID refs       │  │   - Properties          │  │                                            │   │
  │  └─────────────────────────┘  └─────────────────────────┘  └─────────────────────────────────────────────┘   │
  │                                                                                                               │
  └───────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

*Document Version: 1.0*
*Created: 2026-01-08*
*Issue Reference: #205*
