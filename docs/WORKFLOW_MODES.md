# NovelMind Workflow Modes

This document explains the different workflow modes available in NovelMind and how to choose the right one for your project.

## Overview

NovelMind supports three workflow modes that determine the **source of truth** for your story content:

| Mode | Source of Truth | Best For |
|------|----------------|----------|
| **Script** | NMScript files (`.nms`) | Writers who prefer text-based editing |
| **Graph** | Story Graph (visual editor) | Visual thinkers, non-programmers |
| **Mixed** | Both (Script + Graph overrides) | Teams, complex projects |

## Script Mode (Default)

**Source of Truth:** `scripts/main.nms` and other `.nms` files

In Script mode, NMScript files are the authoritative source for all story content:

- Edit story content directly in `.nms` files
- Changes in scripts immediately affect playback
- Story Graph is read-only (for visualization only)
- Use "Sync to Graph" to update visual representation

### When to Use Script Mode

- You prefer text-based editing
- You want full control over script syntax
- You're working alone or with experienced developers
- Your story has complex logic (variables, conditions)

### Folder Structure

```
scripts/
├── main.nms        # Main entry point (edit this)
└── generated/      # Empty in Script mode
```

## Graph Mode

**Source of Truth:** Story Graph visual editor

In Graph mode, the visual Story Graph is the authoritative source:

- Create and edit nodes visually in the Story Graph panel
- Changes in the graph immediately affect playback
- Script files are read-only (for export/backup)
- Use "Sync to Script" to generate script files

### When to Use Graph Mode

- You prefer visual editing
- You're new to NovelMind or scripting
- Your story has a simple structure
- You want to see the story flow visually

### Folder Structure

```
scripts/
├── main.nms        # Read-only backup
└── generated/      # Auto-generated from Graph
story_graph/
└── story.nmgraph   # Source of truth (edit here)
```

## Mixed Mode

**Source of Truth:** Scripts + Graph overrides

Mixed mode combines both approaches:

1. Scripts provide the base content
2. Graph additions/overrides take priority on conflicts
3. Both sources are merged at runtime

### When to Use Mixed Mode

- Teams with different skill levels
- Complex projects with modular structure
- You want visual overview + text control
- Gradual migration between modes

### How Mixed Mode Works

1. **Script content is loaded first** from `scripts/*.nms`
2. **Graph content is appended** from `story_graph/*.nmgraph`
3. **Graph scenes override script scenes** with the same name
4. **Entry scene** is taken from the Graph if available

### Important Notes for Mixed Mode

- If you edit `scripts/main.nms`, ensure the scene names don't conflict with Graph scenes
- Use `scripts/generated/` for auto-generated backup of Graph content
- To see your script changes take effect, ensure the scene name is unique

### Folder Structure

```
scripts/
├── main.nms        # Base content (edit this)
└── generated/      # Graph-generated backup
story_graph/
└── story.nmgraph   # Visual overrides
```

## Changing Workflow Mode

### Via Project Settings

1. Open **Project Settings** (Edit → Project Settings)
2. Find the **Workflow Mode** section
3. Select your preferred mode
4. Click **Apply**

### Via Play Toolbar

1. Look at the Play Toolbar at the top
2. Find the **Source** dropdown
3. Select: Script, Graph, or Mixed

### Via project.json

Add or modify the `playbackSourceMode` field:

```json
{
  "playbackSourceMode": "Script"
}
```

Valid values: `"Script"`, `"Graph"`, `"Mixed"`

## Synchronization

### Sync Graph to Script (Graph → Script)

Copies dialogue and scene content from the Story Graph to script files.

**How to use:**
1. Open the Story Graph panel
2. Click the **Sync to Script** button in the toolbar
3. Wait for the operation to complete
4. Check your script files for updates

**Use cases:**
- Export visual edits to text format
- Create backup of Graph content
- Prepare for Script mode transition

### Sync Script to Graph (Script → Graph)

Parses NMScript files and creates/updates nodes in the Story Graph.

**How to use:**
1. Open the Story Graph panel
2. Click the **Sync to Graph** button in the toolbar
3. Confirm the import operation
4. Review the created nodes and connections

**What gets imported:**
- Scene blocks → Graph nodes
- Dialogue statements (`say`) → Node dialogue text
- Choice blocks → Choice nodes with options
- Goto statements → Node connections
- Conditions (`if`) → Condition nodes

**Use cases:**
- Visualize existing script content
- Review story flow visually
- Prepare for Graph mode transition
- Understand complex script structure

**Notes:**
- This operation replaces existing graph content
- Connections are created based on goto/choice targets
- Entry point is set to the first scene found
- Unicode identifiers (Cyrillic, etc.) are fully supported

## Best Practices

### For Script Mode

1. Keep one story per `.nms` file for organization
2. Use comments (`//`) to document complex logic
3. Test frequently with the Play button

### For Graph Mode

1. Use meaningful node labels
2. Connect all nodes properly
3. Set the entry node (right-click → Set as Start)

### For Mixed Mode

1. Decide which scenes are "script-owned" vs "graph-owned"
2. Avoid duplicate scene names between Script and Graph
3. Use `scripts/generated/` for backup
4. Document your workflow in the project README

## Troubleshooting

### "Changes in main.nms don't affect playback"

**Cause:** You may be in Graph or Mixed mode where Graph takes priority.

**Solution:**
1. Check your current mode in Project Settings
2. Switch to Script mode if you want scripts to be authoritative
3. Or rename your script scenes to avoid conflicts with Graph

### "scripts/generated is empty"

**Cause:** The `generated/` folder is for auto-generated backups.

**Solution:**
1. In Graph mode: Click "Sync to Script" to populate it
2. In Script mode: This folder remains empty (that's normal)
3. In Mixed mode: It will contain Graph exports after sync

### "Linter shows 'goto' errors for Graph scenes"

**Cause:** The linter validates against scripts, not Graph content.

**Solution:**
1. Use "Sync Graph to Script" to export scenes to scripts
2. Or ignore linter warnings for Graph-only scenes
3. Consider switching to Graph mode if Graph is your source of truth

## See Also

- [Project Structure](PROJECT_STRUCTURE.md)
- [NMScript Language Reference](nmscript_reference.md)
- [Story Graph Tutorial](story_graph_tutorial.md)
