# NM Script Editor UI/UX Analysis Report

**Issue**: #227 - Scripting Analysis (UI/UX Extension)
**Date**: 2026-01-08
**Status**: Analysis Complete - No Fixes Applied

## Executive Summary

This report provides an in-depth UI/UX evaluation of the NM Script Editor integration within the StoryGraph/NovelMind editor, as requested in issue #227. The analysis focuses on user interface design, usability, workflow efficiency, visual design, accessibility, and integration with other editor panels.

**Overall UI/UX Rating**: 7.5/10

**Key Strengths**:
- Professional VSCode-like interface with familiar keyboard shortcuts
- Comprehensive IDE features (minimap, folding, find/replace, command palette)  
- Excellent visual feedback (syntax highlighting, bracket matching, error underlines)
- Well-structured panel layout with file tree, symbol list, and editor tabs

**Key Weaknesses**:
- Limited visual discoverability of advanced features
- Missing onboarding and feature hints for new users
- Inconsistent integration between Script Editor, Story Graph, and Scene Preview
- No visual indicators for script-scene relationships
- Limited customization and accessibility options

---

## 1. Visual Design & Layout Analysis

### Current Panel Structure

```
┌─────────────────────────────────────────────────────────────┐
│ Toolbar: [Save] [Save All] [Format] [Insert] [Symbols]     │
├─────────────────────────────────────────────────────────────┤
│ Breadcrumb Bar: scene main > choice                         │
├──────────────┬──────────────────────────────────────────────┤
│ File Tree    │  Editor Tabs                                 │
│              │  ┌────────────────────────────────────────┐  │
│ ├─ scenes/   │  │ Line# | Code Editor        | Minimap │  │
│ │  └─ intro  │  │   1   | scene intro {      │         │  │
│ └─ chars.nms │  │   2   |   say Hero "..."   │    ::   │  │
│──────────────│  │       |                    │    ::   │  │
│ Symbols      │  └────────────────────────────────────────┘  │
│ ┌──────────┐ │                                              │
│ │🔵 intro  │ │  [Find/Replace Widget (hidden by default)]  │
│ │🟡 Hero   │ │                                              │
│ └──────────┘ │                                              │
└──────────────┴──────────────────────────────────────────────┤
│ Status Bar: Syntax hint: "goto scene_name"  | Ln 5, Col 12 │
└─────────────────────────────────────────────────────────────┘
```

### Identified Issues

**UI-1: Panel Size Constraints**
- File tree minimum width (180px) may be too narrow for long file names
- Minimap fixed width (120px) cannot be resized
- No way to temporarily hide minimap without toggling it off
- Splitter positions not saved between sessions

**UI-2: Visual Hierarchy Weak**
- All panels have same visual weight
- Active editor tab not distinctly highlighted
- No visual separator between file tree and symbol list
- Status bar blends into background

**UI-3: Wasted Screen Space**
- Toolbar takes full width with sparse buttons
- Breadcrumb bar always visible even for single-level files
- Status bar could be condensed
- No compact mode for small screens

---

## 2. Color Scheme & Theming

### Current Palette
```cpp
bgDarkest: #0D1014
bgDark: #14181E
bgMedium: #1C2129
bgLight: #262D38
accentPrimary: #3B9EFF (blue)
scriptEditor: #FF9B66 (coral) - Panel accent
```

### Issues Found

**UI-4: Insufficient Visual Feedback for Interactive Elements**
- Hover states barely visible
- Buttons and clickable elements lack affordance
- No ripple or animation feedback on clicks
- Focus indicators weak

**UI-5: Syntax Highlighting Limited**
- Scene names not distinct from regular identifiers
- Character names blend with variables
- Commands (say, show, goto) not differentiated from keywords
- No semantic highlighting

**UI-6: No Theme Customization**
- Users cannot change color scheme
- No light theme option
- Font size not adjustable in UI
- Contrast ratio not WCAG AA compliant for all elements

---

## 3. Interactive Elements & Controls

### Toolbar Usability Issues

**UI-9: Poor Discoverability**
- Buttons show only text, no icons
- No visual grouping
- Advanced features hidden

**Proposed toolbar with icons**:
```
[💾] [💾✓] ║ [⚡] ║ [📋+] [🔍] ║ [☰] [▼]
Save SaveAll Format Insert Symbols More View
```

**UI-10: Missing Common Actions**
- No "New File" button in toolbar
- No "Run Script" or "Test" button
- No undo/redo visual indicators
- No word count/line count display

### Context Menu Issues

**UI-11: Limited Context-Specific Actions**
Missing actions:
- No "Create Scene" for undefined scene reference
- No "Add Character" quick action
- No "Format Selection" option
- No "Comment/Uncomment" toggle
- No "Extract to Scene" refactoring

**UI-12: No Visual Feedback for Context**
- Menu items not dynamically updated with current symbol
- No preview of action result

### Find/Replace Widget Issues

**UI-13: Limited Search Scope Options**
- Cannot limit search to current function/scene
- No "Find in Selection" option
- No search history dropdown

**UI-14: Visual Feedback Weak**
- Current match not highlighted differently
- No "next match is off-screen" indicator
- Search results not shown in minimap

**UI-15: Replace Preview Missing**
- No preview before applying
- Replace All destructive without undo confirmation

---

## 4. Feature Discoverability & Onboarding

### First-Time User Experience

**UI-19: Zero Onboarding for New Users**
When opening Script Editor first time:
- No welcome screen or tour
- No hints about available features
- No sample scripts to explore
- No quick start guide link

**UI-20: Hidden Features**
Advanced features exist but are invisible:
- Minimap (enabled by default but no explanation)
- Code folding (indicators too small)
- Breadcrumbs (no interaction hint)
- Snippet system (Ctrl+J not discoverable)
- Command palette (Ctrl+Shift+P not obvious)

### Keyboard Shortcuts Discoverability

**UI-21: No Keyboard Shortcut Reference**
- No built-in shortcut cheatsheet
- No "View > Keyboard Shortcuts" menu
- Cannot print keyboard reference

Hidden shortcuts:
- `Ctrl+J`: Insert Snippet ❌
- `Ctrl+Shift+O`: Symbol Navigator ❌
- `Ctrl+Shift+P`: Command Palette ❌
- `Shift+F12`: Find References ❌
- `Ctrl+Shift+G`: Navigate to Graph ❌

---

## 5. Integration with Other Panels

### Story Graph Integration

**UI-22: One-Way Integration**
- ✅ Script → Graph: Navigate to node
- ❌ Graph → Script: Cannot jump to script defining node
- ❌ No visual indicator which graph nodes have scripts
- ❌ No preview of script content in graph node

**UI-23: No Visual Linkage Indicators**
```nms
scene intro {     // ❌ No visual hint this connects to graph node
    say Hero "Welcome"
    goto chapter1  // ❌ No indication if "chapter1" node exists
}
```

Should show:
```nms
🔗 scene intro {     // Icon showing graph connection
       say Hero "Welcome"
   ⚠️    goto chapter1  // Warning: node not found in graph
   }
```

**UI-24: Broken Bidirectional Workflow**
User cannot:
- See script preview when hovering over graph node
- Edit script inline from graph node
- Create script from graph node
- See which scenes in file tree are in graph

### Inspector Panel Integration

**UI-25: No Runtime Variable Viewer**
During script execution, cannot see:
- Current variable values
- Flag states
- Call stack (scene history)
- Current execution line

**UI-26: No Script Object Inspector**
When viewing scene reference:
- Cannot see scene object list in inspector
- Cannot see background/character properties
- Cannot validate object existence

**UI-27: No Diagnostic Integration**
Inspector doesn't show:
- Compilation errors
- Runtime errors
- Performance metrics

### Scene Preview Integration

**UI-28: No Live Preview of Script Changes**
```nms
show Hero at left      // ❌ No live preview of position
transition fade 1.0    // ❌ No preview of transition effect
say Hero "Hello" {     // ❌ No preview of dialogue box
    color=#FF0000
}
```

**UI-29: No Asset Validation**
Script editor doesn't validate:
- Background images exist
- Character sprites exist
- Voice files exist
- Music/sound files exist

**UI-30: Cannot Test Script in Isolation**
- No "Run from Cursor" feature
- No "Test Scene" quick action
- Must run entire game to test changes

---

## 6. Accessibility Analysis

### Keyboard Navigation

**UI-31: Incomplete Keyboard Access**
Cannot do with keyboard only:
- Toggle minimap visibility
- Fold/unfold code sections (no shortcut)
- Navigate between file tree / symbols / editor
- Access all context menu items

**UI-32: No Customizable Shortcuts**
- Cannot rebind keyboard shortcuts
- No shortcut conflict detection

### Screen Reader Support

**UI-33: Poor Screen Reader Support**
- Line numbers not announced
- Syntax highlighting not conveyed semantically
- Error underlines not announced
- Minimap not described
- Status bar updates not announced

**UI-34: No Alternative Text for Icons**
- Toolbar icons (if added) need alt text
- Panel icons need labels
- Diagnostic icons need text equivalents

### Visual Accessibility

**UI-35: Color-Dependent Information**
Information conveyed only by color:
- Error (red underline) vs Warning (yellow underline)
- Scene vs Character in symbol list
- Active tab (color accent only)

Should add:
- Icons alongside colors (❌ error, ⚠️ warning)
- Shape/pattern differentiation
- Text labels

**UI-36: Insufficient Contrast**
Some color combinations may fail WCAG AA:
- Text secondary on bgMedium = ~4.5:1 (borderline)
- Hover bgLight vs bgMedium = ~1.2:1 (too low)

**UI-37: No High Contrast Mode**
No option for users with low vision

---

## 7. Performance & Responsiveness

**UI-38: Large File Performance**
For files > 1000 lines:
- Minimap rendering may lag
- Syntax highlighting may slow down
- Find/replace "highlight all" could freeze UI

**UI-39: No Progress Indicators**
Long operations lack feedback:
- "Replace All" in large file
- "Find References" across project
- Syntax validation of all files

**UI-40: Minimap Image Caching**
- Entire minimap cached as image
- For very large files, image could be huge
- Multiple open files = multiple cached images

---

## 8. Workflow Efficiency

### Common Task Analysis

**Task 1: Create New Scene**
Current steps: 5 (create file, open, type boilerplate, add to graph manually)
Improved: 3 (click button, enter name, auto-generated)

**Task 2: Find and Fix Error**
Current steps: 5 (see underline, hover, read, manually fix, wait)
Improved: 3 (see underline with lightbulb, click, auto-fix)

**Task 3: Navigate Script ↔ Graph**
Current steps: 4 (find scene, remember name, press shortcut, search)
Improved: 1 (click graph icon in gutter)

**UI-41: Too Many Steps for Common Tasks**
- Creating scenes requires boilerplate typing
- Fixing errors manual
- Navigation indirect

**UI-42: No Workflow Templates**
- No project templates
- No scene templates
- No character templates

---

## 9. Visual Feedback & Affordances

**UI-43: Weak Visual Affordances**
Elements that should look clickable but don't:
- Breadcrumb items (actually buttons, look like labels)
- Symbol list items (no cursor change)
- Folding indicators (very small)
- Line numbers (no hover effect)

**UI-44: No Loading States**
Operations lack "in progress" indicators:
- File saving
- Diagnostics running
- Symbol index updating

**UI-45: No Success Confirmation**
Actions lack confirmation:
- File saved (no feedback)
- Script formatted (no notification)
- References found (no "Found 5 references" message)

---

## 10. Customization & Preferences

**UI-46: No Preferences UI**
No way to customize:
- Theme colors
- Font family/size
- Indent size
- Tab vs spaces
- Line wrapping
- Minimap visibility persistence
- Diagnostic delay (600ms hardcoded)

**UI-47: Settings Not Persisted**
UI state not saved between sessions:
- Splitter positions
- Open files
- Active tab
- Minimap on/off
- Folded regions

---

## 11. Prioritized Improvement Recommendations

### High Priority (Next Sprint)

1. **#237: Add Toolbar Icons** (UI-9)
   - Effort: Low (1-2 days)
   - Impact: High
   - Improves feature discoverability significantly

2. **#238: Add Welcome Dialog and Onboarding** (UI-19, UI-20)
   - Effort: Medium (3-4 days)
   - Impact: Very High
   - Dramatically improves new user experience

3. **#239: Add Graph Integration Indicators** (UI-22, UI-23, UI-24)
   - Effort: Medium (3-4 days)
   - Impact: Very High
   - Solves major workflow pain point

4. **#240: Add Live Scene Preview** (UI-28)
   - Effort: High (5-7 days)
   - Impact: Very High
   - Game-changer for workflow efficiency

5. **#241: Add Asset Validation** (UI-29)
   - Effort: Medium (3-5 days)
   - Impact: High
   - Prevents broken builds, saves debugging time

6. **#242: Add Settings Dialog** (UI-46, UI-47)
   - Effort: Medium (4-5 days)
   - Impact: High
   - Enables customization and state persistence

### Medium Priority (2-3 Sprints)

7. **#243: Add Keyboard Shortcut Reference** (UI-21)
   - Effort: Low (1-2 days)
   - Impact: Medium
   - Improves learnability

8. **#244: Add Runtime Inspector/Debugger** (UI-25, UI-26, UI-27)
   - Effort: Very High (1-2 weeks)
   - Impact: Very High
   - Essential for serious development

9. **Quick Fixes System** (UI-11)
   - Effort: High (5-7 days)
   - Impact: High
   - Reduces friction

10. **Enhanced Find/Replace** (UI-13, UI-14, UI-15)
    - Effort: Medium (3-4 days)
    - Impact: Medium
    - Power user feature

11. **Workflow Templates** (UI-42)
    - Effort: Medium (3-4 days)
    - Impact: Medium
    - Improves productivity

### Low Priority (Future)

12. **Screen Reader Support** (UI-33, UI-34)
    - Effort: Medium (3-5 days)
    - Impact: Medium
    - Accessibility compliance

13. **High Contrast Mode** (UI-36, UI-37)
    - Effort: Medium (3-5 days)
    - Impact: Medium
    - Accessibility for low vision users

14. **Large File Performance** (UI-38, UI-40)
    - Effort: High (1-2 weeks)
    - Impact: Medium
    - Handles scale better

15. **Enhanced Command Palette** (UI-16, UI-17, UI-18)
    - Effort: Medium (3-4 days)
    - Impact: Low
    - Nice refinement

---

## 12. Conclusion

The NM Script Editor has a **solid technical foundation** with professional-grade IDE features. However, the **UI/UX needs significant polish** to match the quality of the underlying implementation.

**Overall Assessment**: 7.5/10
- Excellent IDE features (9/10)
- Good visual design (7/10)
- Poor discoverability (5/10)
- Weak integration (6/10)
- Insufficient accessibility (4/10)

**Key Takeaways**:
1. **Visual Design**: Good dark theme, needs better affordances and feedback
2. **Discoverability**: Advanced features hidden, needs onboarding
3. **Integration**: Script Editor isolated, needs tighter coupling with Graph and Preview
4. **Workflow**: Common tasks require too many steps
5. **Customization**: No user preferences
6. **Accessibility**: Missing features for users with disabilities

**Recommended Focus Areas**:
1. **Phase 1 (1-2 months)**: Quick wins (icons, tooltips, settings, notifications)
2. **Phase 2 (2-3 months)**: Integration (graph indicators, live preview, asset validation)
3. **Phase 3 (3-6 months)**: Advanced tooling (debugger, quick fixes, templates)
4. **Phase 4 (ongoing)**: Accessibility and refinement

With these improvements, the NM Script Editor could reach **9/10 UI/UX quality** and become a best-in-class visual novel development tool.

---

## 13. Issues Created

Based on this analysis, the following issues have been created:

### High Priority
- #237: [High][Scripting][UI/UX] Add Toolbar Icons and Visual Grouping
- #238: [High][Scripting][UI/UX] Add Welcome Dialog and Feature Onboarding
- #239: [High][Scripting][UI/UX] Add Graph Integration Visual Indicators
- #240: [High][Scripting][UI/UX] Add Live Scene Preview Split-View Mode
- #241: [High][Scripting][UI/UX] Add Asset Validation and Autocomplete with Thumbnails
- #242: [High][Scripting][UI/UX] Add Settings Dialog for Customization

### Medium Priority
- #243: [Medium][Scripting][UI/UX] Add Keyboard Shortcut Reference Dialog
- #244: [Medium][Scripting][UI/UX] Add Script Runtime Inspector and Debugger Panel

---

*UI/UX analysis complete for issue #227*
