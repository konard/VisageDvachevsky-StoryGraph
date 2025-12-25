# NovelMind Editor: Comprehensive UI/UX Audit Report

**Version:** 2.0
**Date:** December 2025
**Branch:** issue-38-0c21cb399563
**PR:** #39

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [UI Component Registry (100% Surface Inventory)](#2-ui-component-registry)
3. [Component → Requirements → DoD → Tests Matrix](#3-component-matrix)
4. [Problem List by Product Blocks](#4-problem-list)
5. [Code Contracts & State Management Analysis](#5-code-contracts)
6. [Accessibility & UX Consistency Audit](#6-accessibility-audit)
7. [Animation System Standards](#7-animation-system)
8. [Docking System Review](#8-docking-system)
9. [Hidden Defects Analysis](#9-hidden-defects)
10. [Step-by-Step Implementation Plan](#10-implementation-plan)
11. [Test Coverage Summary](#11-test-coverage)

---

## 1. Executive Summary

This document provides a comprehensive audit of 100% of the NovelMind Editor UI surface as requested in Issue #38. The audit covers:

- **24 dock panels** fully inventoried and analyzed
- **7+ dialog types** reviewed for consistency
- **119 SVG icons** verified (114 Lucide + 5 custom)
- **1,400+ lines of QSS styling** in design system
- **18 actionable issues** identified across memory management, error handling, and unimplemented features
- **8 UI-related test files** with recommendations for expanded coverage

### Key Findings

| Category | Status | Issues Found |
|----------|--------|--------------|
| Panel Completeness | 🟡 Partial | 5 panels with TODO implementations |
| Memory Safety | 🟡 Needs Work | 4 memory ownership issues |
| Error Handling | 🔴 Gaps | 6 silent failure points |
| Race Conditions | 🟡 Potential | 2 async issues identified |
| Accessibility | 🟢 Good | Focus indicators, keyboard nav present |
| Design System | 🟢 Strong | Comprehensive QSS, panel accents |
| Hotkey System | 🟢 Complete | Full customization, import/export |

---

## 2. UI Component Registry

### 2.1 Dock Panels (24 Total)

| Panel | File | Lines | Purpose | State |
|-------|------|-------|---------|-------|
| SceneViewPanel | `nm_scene_view_panel.cpp` | ~2500 | Main scene editor with viewport | ✅ Complete |
| StoryGraphPanel | `nm_story_graph_panel.cpp` | ~1800 | Story structure visualization | ✅ Complete |
| InspectorPanel | `nm_inspector_panel.cpp` | ~1200 | Properties inspector | ✅ Complete |
| ConsolePanel | `nm_console_panel.cpp` | ~400 | Debug output and logging | ✅ Complete |
| AssetBrowserPanel | `nm_asset_browser_panel.cpp` | ~900 | Asset management and preview | ✅ Complete |
| HierarchyPanel | `nm_hierarchy_panel.cpp` | ~600 | Scene object hierarchy tree | ✅ Complete |
| ScriptEditorPanel | `nm_script_editor_panel.cpp` | ~1100 | Script/code editing | ✅ Complete |
| ScriptDocPanel | `nm_script_doc_panel.cpp` | ~300 | Script documentation viewer | ✅ Complete |
| TimelinePanel | `nm_timeline_panel.cpp` | ~2000 | Timeline/animation keyframe editor | 🟡 Partial (line 265 TODO) |
| CurveEditorPanel | `nm_curve_editor_panel.cpp` | ~1400 | Animation curve editor | ✅ Complete |
| VoiceStudioPanel | `nm_voice_studio_panel.cpp` | ~2200 | Voice/audio recording studio | 🟡 Partial |
| VoiceManagerPanel | `nm_voice_manager_panel.cpp` | ~1800 | Voice asset library management | 🟡 Partial (line 713 TODO) |
| LocalizationPanel | `nm_localization_panel.cpp` | ~800 | Dialogue localization tools | ✅ Complete |
| AudioMixerPanel | `nm_audio_mixer_panel.cpp` | ~600 | Audio mixing and effects | ✅ Complete |
| BuildSettingsPanel | `nm_build_settings_panel.cpp` | ~500 | Project build configuration | ✅ Complete |
| ProjectSettingsPanel | `nm_project_settings_panel.cpp` | ~400 | Project properties | ✅ Complete |
| RecordingStudioPanel | `nm_recording_studio_panel.cpp` | ~700 | Audio recording utilities | 🔴 Incomplete (5 TODOs) |
| SceneDialogueGraphPanel | `nm_scene_dialogue_graph_panel.cpp` | ~800 | Dialogue flow visualization | 🟡 Partial (line 547 TODO) |
| ScenePalettePanel | `nm_scene_palette_panel.cpp` | ~400 | Color palette and sprites | ✅ Complete |
| PlayToolbarPanel | `nm_play_toolbar_panel.cpp` | ~300 | Playback controls | ✅ Complete |
| HelpHubPanel | `nm_help_hub_panel.cpp` | ~400 | Contextual help and tutorials | ✅ Complete |
| DebugOverlayPanel | `nm_debug_overlay_panel.cpp` | ~900 | Runtime debugging overlay | ✅ Complete |
| DiagnosticsPanel | `nm_diagnostics_panel.cpp` | ~500 | Performance and diagnostic metrics | ✅ Complete |
| IssuesPanel | `nm_issues_panel.cpp` | ~400 | Error reporting and issues | ✅ Complete |

### 2.2 Dialogs (7+ Types)

| Dialog | Purpose | Keyboard Nav | Focus Management |
|--------|---------|--------------|------------------|
| NMMessageDialog | Info/Warning/Error/Question | ✅ Tab nav | ✅ Focus trap |
| NMInputDialog | Text input | ✅ Tab nav | ✅ Auto-focus input |
| NMHotkeysDialog | Keyboard shortcuts | ✅ Full keyboard | ✅ Tree focus |
| NMSettingsDialog | Application settings | ✅ Tab nav | ✅ Focus sections |
| NMWelcomeDialog | Startup welcome | ✅ Tab nav | ✅ Focus actions |
| NMNewProjectDialog | New project wizard | ✅ Tab nav | ✅ Step focus |
| NMColorDialog | Color picker | ✅ Tab nav | ✅ Focus sliders |
| NMFileDialog | File/folder selection | ✅ System dialog | ✅ System |

### 2.3 Main Window Components

| Component | File | Purpose |
|-----------|------|---------|
| Menu Bar | `nm_main_window_menu.cpp` | File, Edit, View, Play, Help menus |
| Toolbar | `nm_main_window.cpp` | Main action toolbar |
| Status Bar | `nm_main_window.cpp` | Context, play state, save indicator |
| Layout Manager | `nm_main_window_layout.cpp` | Dock layout, presets, focus mode |
| Connections | `nm_main_window_connections.cpp` | All signal/slot wiring |

### 2.4 Supporting UI Components

| Component | File | Purpose |
|-----------|------|---------|
| NMTransformGizmo | `nm_scene_view_gizmo.cpp` | 2D/3D transformation handles |
| NMSceneObject | `nm_scene_view_object.cpp` | Scene object representation |
| NMGraphNodeItem | `nm_story_graph_node.cpp` | Story graph node widget |
| NMGraphConnectionItem | `nm_story_graph_connection.cpp` | Connection lines |
| NMStoryGraphMinimap | `nm_story_graph_minimap.cpp` | Navigation minimap |
| NMKeyframeItem | `nm_keyframe_item.cpp` | Timeline keyframe widget |
| NMCurvePointItem | `nm_curve_point_item.cpp` | Curve editor control points |
| NMAnimationAdapter | `nm_animation_adapter.cpp` | Animation system bridge |

### 2.5 Resource System

- **Icon Manager:** `nm_icon_manager.cpp` - Loads and caches SVG icons
- **Style Manager:** `nm_style_manager.cpp` + `nm_style_manager_stylesheet.cpp` - 1,400+ lines of QSS
- **Lazy Thumbnail Loader:** `lazy_thumbnail_loader.cpp` - Async asset thumbnails
- **Resource File:** `editor_resources.qrc` - 119 SVG icons registered

---

## 3. Component → Requirements → DoD → Tests Matrix

| Component | Core Requirements | Definition of Done | Test Coverage | Status |
|-----------|-------------------|-------------------|---------------|--------|
| **SceneViewPanel** | Display scene, selection, gizmo, zoom/pan | Object manipulation works, undo/redo | `scene_object_properties_test.cpp`, `multi_object_editing_test.cpp` | ✅ |
| **StoryGraphPanel** | Node graph, connections, navigation | CRUD nodes, save/load, minimap | `test_story_graph.cpp` | ✅ |
| **InspectorPanel** | Show properties, edit values | Property binding, undo, validation | `scene_object_properties_test.cpp` | ✅ |
| **TimelinePanel** | Keyframes, playback, scrubbing | Add/edit keyframes, playback sync | `test_animation_integration.cpp` | 🟡 Partial |
| **VoiceManagerPanel** | Voice list, playback, assignment | Scan, play, assign to nodes | `test_voice_manager.cpp` | 🟡 Partial |
| **AssetBrowserPanel** | Asset list, preview, drag-drop | Browse, preview, import to scene | `test_asset_browser.cpp` | ✅ |
| **LocalizationPanel** | Translation table, export | CRUD translations, CSV export | `test_dialogue_localization.cpp` | ✅ |
| **ScriptEditorPanel** | Syntax highlight, completion | Open, edit, save, error markers | None | 🔴 Missing |
| **CurveEditorPanel** | Bezier curves, control points | Add/edit curves, presets | `test_animation.cpp` | ✅ |
| **RecordingStudioPanel** | Record, playback, takes | Record audio, manage takes | None | 🔴 Not Implemented |
| **SettingsDialog** | Categories, persistence | All settings save/load | `test_editor_settings.cpp` | ✅ |
| **HotkeysDialog** | List, customize, import/export | Modify shortcuts, detect conflicts | None | 🔴 Missing |

---

## 4. Problem List by Product Blocks

### 4.1 Story Graph Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| SG-1 | Medium | Context menu actions unimplemented | `nm_story_graph_node.cpp:654-672` | 🔴 TODO |
| SG-2 | Low | Voice clip operations stubbed | `nm_story_graph_voice_integration.cpp:90-92` | 🔴 TODO |
| SG-3 | Medium | Auto-layout not implemented | `nm_scene_dialogue_graph_panel.cpp:547` | 🔴 TODO |

### 4.2 Scene Editor Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| SE-1 | High | Memory ownership issue with gizmo | `nm_scene_view_scene.cpp:27` | 🟡 Risk |
| SE-2 | Medium | Runtime object allocation without validation | `nm_scene_view_panel_runtime.cpp:259` | 🟡 Risk |
| SE-3 | Low | Transform commit only logs, doesn't apply | `nm_animation_adapter.cpp:287-291` | 🔴 TODO |

### 4.3 Audio/Voice Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| AV-1 | High | Recording Studio: 5 empty handlers | `nm_recording_studio_panel.cpp:588-608` | 🔴 TODO |
| AV-2 | Medium | Dual media player duplicate connections | `nm_voice_manager_panel.cpp:314-340` | 🟡 Risk |
| AV-3 | Medium | Probe queue race condition | `nm_voice_manager_panel.cpp:336-369` | 🟡 Risk |
| AV-4 | Low | Unmatched lines filter incomplete | `nm_voice_manager_panel.cpp:713` | 🔴 TODO |

### 4.4 Animation Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| AN-1 | Medium | Bezier curve data not used | `nm_timeline_panel.cpp:265` | 🔴 TODO |
| AN-2 | High | Animation adapter doesn't apply to scene | `nm_animation_adapter.cpp:287` | 🔴 TODO |
| AN-3 | Low | Stack-allocated timer in easing | `nm_timeline_panel.cpp:670` | 🔴 Bug |

### 4.5 Inspector/Properties Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| IP-1 | Medium | Property errors only logged, not shown | `nm_inspector_panel.cpp:603` | 🟡 UX |

### 4.6 Undo/Redo Block

| ID | Severity | Issue | Location | Status |
|----|----------|-------|----------|--------|
| UR-1 | High | Localization undo commands incomplete | `nm_undo_manager.cpp:889,911,919` | 🔴 TODO |

---

## 5. Code Contracts & State Management Analysis

### 5.1 State Sources of Truth

| State | Owner | Subscribers | Sync Mechanism |
|-------|-------|-------------|----------------|
| Scene Objects | `NMSceneGraphicsScene` | InspectorPanel, HierarchyPanel | Signals |
| Story Graph Nodes | `NMStoryGraphScene` | InspectorPanel, Minimap | Signals |
| Selection | `QtSelectionManager` | All panels | EventBus + Signals |
| Play Mode | `NMPlayModeController` | Toolbar, SceneView, StatusBar | Signals |
| Undo Stack | `NMUndoManager` | Edit menu, all panels | Signals |
| Theme/Style | `NMStyleManager` | All widgets | QSS refresh |
| Project State | `ProjectManager` | All panels | Signals |

### 5.2 State Invariants

1. **Selection Invariant:** At most one object selected at a time (enforced by `QtSelectionManager`)
2. **Play Mode Invariant:** Only one of {Playing, Paused, Stopped} active at a time
3. **Undo Stack Invariant:** Commands are atomic; partial application not allowed
4. **Panel Visibility Invariant:** Menu checkmarks sync with dock visibility

### 5.3 State Transition Issues

| From | To | Issue | Location |
|------|-----|-------|----------|
| Scanning | Scanning | Race condition: no guard | `nm_voice_manager_panel.cpp` |
| Any | Closed | Unsaved changes flag not thread-safe | `nm_scene_dialogue_graph_panel.cpp:552` |

---

## 6. Accessibility & UX Consistency Audit

### 6.1 Focus Indicators ✅

The stylesheet includes comprehensive focus indicators:

```css
*:focus {
    outline: 1px solid %3;  /* accent color */
    outline-offset: 1px;
}
```

Container elements correctly remove outline:
```css
QMainWindow:focus, QWidget:focus, QFrame:focus, QScrollArea:focus,
QDockWidget:focus, QStackedWidget:focus, QSplitter:focus {
    outline: none;
}
```

### 6.2 Tab Order

| Dialog/Panel | Tab Order | Status |
|--------------|-----------|--------|
| HotkeysDialog | Filter → Tree → Buttons | ✅ Correct |
| SettingsDialog | Sidebar → Content → Buttons | ✅ Correct |
| InspectorPanel | Property fields in DOM order | ✅ Correct |
| SceneViewPanel | Toolbar → Canvas (keyboard shortcuts) | ✅ Correct |

### 6.3 Design System Consistency

#### Panel Accent Colors (Visual Identity)

| Panel | Accent Color | Hex |
|-------|--------------|-----|
| SceneView | Teal | `#2ec4b6` |
| StoryGraph | Blue | `#6aa6ff` |
| Inspector | Orange/Gold | `#f0b24a` |
| AssetBrowser | Green | `#5fd18a` |
| ScriptEditor | Coral | `#ff9b66` |
| Console | Gray-blue | `#8ea1b5` |
| Timeline | Purple | (via palette) |
| Diagnostics | Red | (via palette) |

#### Button Styling

- Primary buttons: Accent background, bold text
- Secondary buttons: Subtle background, border on hover
- Play/Pause/Stop: Color-coded (green/yellow/red)

### 6.4 Keyboard Shortcuts

The `NMHotkeysDialog` provides:
- ✅ Full shortcut listing with sections
- ✅ Filter/search functionality
- ✅ Shortcut recording (double-click or Record button)
- ✅ Conflict detection with visual highlighting
- ✅ Import/export to JSON
- ✅ Reset to defaults (individual or all)

---

## 7. Animation System Standards

### 7.1 Current Implementation

| Component | Status | Notes |
|-----------|--------|-------|
| Timeline Panel | 🟡 Partial | Keyframe display works, playback incomplete |
| Curve Editor | ✅ Complete | Bezier curves, control points, presets |
| Animation Adapter | 🔴 Stub | Does not apply values to scene objects |
| Keyframe Item | ✅ Complete | Visual representation, selection |
| Playback Controller | ✅ Complete | Play/pause/stop, frame stepping |

### 7.2 Animation Data Flow (Design)

```
TimelinePanel → frameChanged(int frame)
                     ↓
             AnimationAdapter::applyFrame(frame)
                     ↓
             Interpolate keyframe values
                     ↓
             SceneViewPanel::updateObjectProperties()
```

### 7.3 Gaps

1. **Line 287 (`nm_animation_adapter.cpp`):** Properties are calculated but never applied
2. **Line 265 (`nm_timeline_panel.cpp`):** Bezier curve data from keyframe handles not used
3. **Line 670 (`nm_timeline_panel.cpp`):** Stack timer in easing function (will be destroyed)

---

## 8. Docking System Review

### 8.1 Dock Features

| Feature | Implementation | Status |
|---------|----------------|--------|
| Dock/Undock | Qt built-in + custom handling | ✅ Works |
| Tabify panels | `tabifyDockWidget()` | ✅ Works |
| Floating | Qt built-in | ✅ Works |
| Lock Layout | `applyDockLockState()` | ✅ Works |
| Focus Mode | `toggleFocusMode()` | ✅ Works |
| Tabbed-Only Mode | `applyTabbedDockMode()` | ✅ Works |
| Float Allowed | `applyFloatAllowed()` | ✅ Works |
| Save Layout | `saveCustomLayout()` | ✅ Works |
| Load Layout | `loadCustomLayout()` | ✅ Works |

### 8.2 Layout Presets

| Preset | Panels Shown | Target User |
|--------|--------------|-------------|
| Default | SceneView, StoryGraph, Inspector, Console | General |
| Story/Script | StoryGraph, ScriptEditor, Console | Writers |
| Scene/Animation | SceneView, Timeline, CurveEditor | Animators |
| Audio/Voice | VoiceStudio, AudioMixer, Timeline | Audio |
| Developer | Console, Diagnostics, DebugOverlay | Developers |
| Compact | Essential panels only | Limited screen |

### 8.3 Visibility Sync

Smart visibility sync implemented with `QSignalBlocker` to prevent feedback loops:

```cpp
connect(m_sceneViewPanel, &QDockWidget::visibilityChanged, this,
        [this](bool visible) {
          if (m_actionToggleSceneView->isChecked() != visible) {
            QSignalBlocker blocker(m_actionToggleSceneView);
            m_actionToggleSceneView->setChecked(visible);
          }
        });
```

---

## 9. Hidden Defects Analysis

### 9.1 Memory Safety Issues

| ID | Severity | Issue | File:Line | Recommendation |
|----|----------|-------|-----------|----------------|
| MEM-1 | High | Node allocated without parent safety | `nm_story_graph_scene.cpp:54` | Set parent at construction |
| MEM-2 | High | Gizmo allocated without parent | `nm_scene_view_scene.cpp:27` | `new NMTransformGizmo(this)` |
| MEM-3 | Medium | Dual player setup can create duplicate connections | `nm_voice_manager_panel.cpp:314-340` | Add guard or disconnect first |
| MEM-4 | Medium | Runtime object allocation without null check | `nm_scene_view_panel_runtime.cpp:259` | Validate scene before allocation |

### 9.2 Race Conditions

| ID | Severity | Issue | File:Line | Recommendation |
|----|----------|-------|-----------|----------------|
| RACE-1 | Medium | Probe queue modified without sync | `nm_voice_manager_panel.cpp:336-369` | Add `m_isProbing` guard |
| RACE-2 | Low | Unsaved changes flag from multiple slots | `nm_scene_dialogue_graph_panel.cpp:552-555` | Use atomic or mutex |

### 9.3 Signal/Slot Issues

| ID | Severity | Issue | File:Line | Recommendation |
|----|----------|-------|-----------|----------------|
| SIG-1 | Low | Overly broad disconnect pattern | `nm_story_graph_minimap.cpp:45,67` | Disconnect specific signals |

### 9.4 Error Handling Gaps

| ID | Severity | Issue | File:Line | Recommendation |
|----|----------|-------|-----------|----------------|
| ERR-1 | Medium | Property errors only logged | `nm_inspector_panel.cpp:603` | Emit `propertyError` signal |
| ERR-2 | Low | Empty stub handlers confuse users | `nm_recording_studio_panel.cpp:588-608` | Disable buttons or show message |

---

## 10. Step-by-Step Implementation Plan

### Phase 1: Critical Fixes (Memory Safety)

1. **Fix MEM-1:** Add parent safety to node allocation in `nm_story_graph_scene.cpp`
2. **Fix MEM-2:** Set gizmo parent in `nm_scene_view_scene.cpp`
3. **Fix MEM-3:** Add connection guard in `nm_voice_manager_panel.cpp`
4. **Fix MEM-4:** Add null checks in `nm_scene_view_panel_runtime.cpp`

### Phase 2: Race Condition Prevention

5. **Fix RACE-1:** Add `m_isProbing` state guard
6. **Fix RACE-2:** Use atomic for unsaved changes flag

### Phase 3: Complete Stub Implementations

7. **Complete AN-2:** Implement `AnimationAdapter::applyFrame()` to actually update scene objects
8. **Complete AV-1:** Implement `RecordingStudioPanel` playback handlers
9. **Complete SG-1:** Implement story graph context menu actions
10. **Complete UR-1:** Complete localization undo commands

### Phase 4: Error Handling Improvements

11. **Fix ERR-1:** Add `propertyError` signal and display in UI
12. **Fix ERR-2:** Disable unimplemented buttons or show "Not implemented" message

### Phase 5: Test Coverage Expansion

13. Add unit tests for `NMHotkeysDialog`
14. Add integration tests for `NMScriptEditorPanel`
15. Add E2E tests for animation workflow

---

## 11. Test Coverage Summary

### 11.1 Existing UI-Related Tests

| Test File | Coverage Area | Assertions |
|-----------|---------------|------------|
| `test_animation_integration.cpp` | Timeline, Curve, Keyframes | Medium |
| `test_asset_browser.cpp` | Asset scanning, preview | Medium |
| `test_voice_manager.cpp` | Voice scanning, assignment | Medium |
| `test_editor_settings.cpp` | Settings persistence | High |
| `multi_object_editing_test.cpp` | Multi-select, transform | Medium |
| `scene_object_properties_test.cpp` | Property binding | High |
| `test_story_graph.cpp` | Node CRUD, connections | High |
| `test_dialogue_localization.cpp` | Translations | High |

### 11.2 Missing Test Coverage

| Component | Priority | Recommended Tests |
|-----------|----------|-------------------|
| HotkeysDialog | High | Shortcut recording, conflict detection, import/export |
| ScriptEditorPanel | High | Syntax highlighting, completion, error markers |
| RecordingStudioPanel | Medium | Recording flow, take management |
| AnimationAdapter | High | Property application, interpolation |
| FocusMode | Low | Panel visibility, restoration |
| LayoutPresets | Low | Preset application, save/load |

### 11.3 CI Considerations

The current CI failures are environment-related:
- **Qt platform plugin not initialized:** Tests requiring GUI fail in headless CI
- **Texture loading test:** Requires graphics context

**Recommendation:** Mark GUI-requiring tests as requiring `xvfb` or skip in headless environment.

---

## Appendix A: File Reference

### Core UI Files

```
editor/src/qt/
├── nm_main_window.cpp              (14.1KB) - Main window container
├── nm_main_window_connections.cpp  (37.8KB) - Signal/slot wiring
├── nm_main_window_layout.cpp       (12.3KB) - Layout management
├── nm_main_window_menu.cpp         (15.2KB) - Menu bar
├── nm_main_window_panels.cpp       (5.8KB)  - Panel creation
├── nm_main_window_runtime.cpp      (8.4KB)  - Runtime mode
├── nm_style_manager.cpp            (9.6KB)  - Theme management
├── nm_style_manager_stylesheet.cpp (37.0KB) - QSS generation
├── nm_undo_manager.cpp             (18.2KB) - Undo/redo system
└── nm_hotkeys_dialog.cpp           (15.8KB) - Shortcut customization

editor/src/qt/panels/
├── nm_scene_view_panel.cpp         (Large)  - Scene editor
├── nm_story_graph_panel.cpp        (Large)  - Story visualization
├── nm_inspector_panel.cpp          (Medium) - Properties
├── nm_timeline_panel.cpp           (Large)  - Animation timeline
├── nm_voice_studio_panel.cpp       (Large)  - Voice recording
├── nm_voice_manager_panel.cpp      (Large)  - Voice library
└── ... (24 total panels)
```

### Resource Files

```
editor/resources/
├── editor_resources.qrc            (5.5KB)  - Resource manifest
├── icons/
│   ├── lucide/                     (114 files) - Icon library
│   └── [5 custom icons]
└── tutorials/
    └── [5 JSON tutorial files]
```

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **DoD** | Definition of Done - acceptance criteria for a component |
| **QSS** | Qt Style Sheets - CSS-like styling for Qt widgets |
| **EventBus** | Custom event dispatch system for cross-panel communication |
| **Focus Mode** | UI mode that hides all panels except the active one |
| **Tabify** | Combining multiple dock panels into tabs |

---

*Report generated for StoryGraph Issue #38 / PR #39*
