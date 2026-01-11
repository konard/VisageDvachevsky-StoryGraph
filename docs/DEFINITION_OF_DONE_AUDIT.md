# NovelMind Editor - Definition of Done Audit Report

**Audit Date**: 2026-01-11
**Auditor**: Tech Lead Auditor (C++20/Qt6)
**Document Version**: 1.0

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Project Map](#2-project-map)
3. [Issues by Area](#3-issues-by-area)
4. [Roadmap by Phases](#4-roadmap-by-phases)
5. [Architecture Guardrails](#5-architecture-guardrails)
6. [Quick Wins vs Big Rocks](#6-quick-wins-vs-big-rocks)

---

## 1. Executive Summary

### Current State vs Definition of Done

| Criterion | Target | Current | Gap |
|-----------|--------|---------|-----|
| Critical/High Bugs | 0 known | ~8 identified | 8 issues |
| Core Test Coverage | 100% | ~40% estimated | 60% gap |
| UI Test Coverage | Maximum possible | ~25% estimated | Needs audit |
| CI Stability | All passing + sanitizers | ✅ Passing | Minor gaps |
| UX: Scene Preview | Usable & predictable | 85% | Scale gizmo issues |
| UX: Play Controls | Unity-like, dockable | 95% | Already dockable |
| Documentation | Comprehensive | 70% | User guide missing |

### Overall Readiness Score: **75%**

---

## 2. Project Map

### 2.1 Core Subsystems

```
NovelMind/
├── engine_core/          # Runtime engine (libengine_core.a)
│   ├── core/             # Logger, Timer, Property System
│   ├── scene/            # Scene Graph 2.0, Objects, Animations
│   ├── scripting/        # Lexer, Parser, Compiler, VM, IR
│   ├── renderer/         # 2D Sprite, Text, Transform
│   ├── vfs/              # Virtual File System, Pack Reader
│   ├── audio/            # Audio Manager (miniaudio)
│   ├── resource/         # Resource Manager, Caching
│   ├── save/             # Save Manager (v2 format)
│   └── localization/     # Localization Manager
│
├── editor/               # Visual Editor (libnovelmind_editor.a)
│   ├── qt/               # Qt6 GUI Layer
│   │   ├── panels/       # 53 panel implementations
│   │   └── widgets/      # Reusable widgets
│   ├── mediators/        # Event-based communication
│   └── guided_learning/  # Tutorial system
│
├── compiler/             # nmc CLI compiler
├── runtime/              # Standalone player
└── tests/                # 54 test files
```

### 2.2 Key Entry Points

| Component | Entry Point | Status |
|-----------|-------------|--------|
| Editor GUI | `editor/src/qt/main.cpp` | ✅ Working |
| Compiler CLI | `compiler/src/main.cpp` | ✅ Working |
| Runtime | `runtime/src/` | ⚠️ Needs verification |
| Tests | `tests/unit/`, `tests/integration/` | ✅ Running |

---

## 3. Issues by Area

### 3.1 Scene View / Gizmos

#### Issue SV-001: Scale Gizmo Hit Testing is Difficult
- **Area**: SceneView/Gizmos
- **Severity**: High
- **Priority**: P1
- **File**: `editor/src/qt/panels/nm_scene_view_gizmo.cpp:450-478`

**Repro Steps**:
1. Select object in Scene View
2. Switch to Scale mode (R key)
3. Try to grab corner handles

**Expected**: Easy to grab handles from any approach angle
**Actual**: Handles are hard to target, especially from diagonal approaches

**Root Cause**:
- Handle size is 16x16 pixels (`createScaleGizmo()` line 472)
- Hit area is just the visible ellipse, no extended hit zone
- No visual feedback when hovering near handles

**Fix Plan**:
1. Increase handle visual size from 16 to 20-24 pixels
2. Add `NMGizmoHitArea` around each corner (similar to move gizmo)
3. Add hover highlight effect for approaching cursor
4. Consider adding snap-to-handle behavior when cursor is within 10px

**Acceptance Criteria**:
- [ ] Scale handles can be grabbed within 10px tolerance
- [ ] Visual hover feedback when approaching handle
- [ ] Cursor changes before mouse-down
- [ ] Works consistently across DPI scales

**Tests**:
- Unit: `test_gizmo_hit_testing.cpp` - Hit area geometry tests
- Integration: `test_scene_view_interaction.cpp` - End-to-end gizmo use
- GUI: Manual regression checklist

**Dependencies**: None

---

#### Issue SV-002: Rotate Gizmo Behavior is Non-intuitive
- **Area**: SceneView/Gizmos
- **Severity**: Medium
- **Priority**: P2
- **File**: `editor/src/qt/panels/nm_scene_view_gizmo.cpp:285-302`

**Repro Steps**:
1. Select object in Scene View
2. Switch to Rotate mode (E key)
3. Drag anywhere in the rotation circle

**Expected**: Rotation should follow intuitive angular motion
**Actual**: Rotation "jumps" when crossing certain angles, angle calculation can be confusing

**Root Cause**:
```cpp
const qreal startAngle = QLineF(center, startPoint).angle();
const qreal currentAngle = QLineF(center, currentPoint).angle();
const qreal deltaAngle = startAngle - currentAngle;
```
- QLineF::angle() returns 0-360 degrees
- When crossing 0/360 boundary, delta can jump by ±360

**Fix Plan**:
1. Normalize angle differences to -180 to +180 range
2. Add angle snapping (15°, 45°, 90° with Shift key)
3. Show angle indicator during rotation
4. Add center point visual indicator

**Acceptance Criteria**:
- [ ] Smooth rotation without jumps at 0/360 boundary
- [ ] Shift+drag snaps to 15° increments
- [ ] Angle tooltip shows current rotation value
- [ ] Visual arc shows rotation amount

**Tests**:
- Unit: Angle normalization function tests
- Integration: Full rotation workflow test

**Dependencies**: None

---

#### Issue SV-003: Move Gizmo Axis Constraint Sensitivity
- **Area**: SceneView/Gizmos
- **Severity**: Low
- **Priority**: P3
- **File**: `editor/src/qt/panels/nm_scene_view_gizmo.cpp:339-422`

**Description**: X/Y axis constraints work but hit areas could be larger for easier targeting.

**Current State**:
- Arrow lines have 5px width (line 347, 378)
- Hit areas are 16px wide (lines 352-353, 383-384)
- This is adequate but could be improved

**Fix Plan**:
1. Increase hit area width to 20-24px
2. Add visual feedback when hovering over axis
3. Consider axis highlighting on hover

**Acceptance Criteria**:
- [ ] Axes can be targeted within 12px tolerance
- [ ] Axis highlights on hover

**Tests**: Include in gizmo hit testing suite

**Dependencies**: Issue SV-001 (shared approach)

---

### 3.2 Play Controls

#### Issue PC-001: Play Controls Panel Already Meets Unity-like Requirements
- **Area**: UI/PlayControls
- **Severity**: None (Informational)
- **Priority**: N/A
- **File**: `editor/src/qt/panels/nm_play_toolbar_panel.cpp`

**Analysis**:
The current Play Controls implementation is already:
- ✅ Compact toolbar design
- ✅ Dockable via `NMDockPanel` base class
- ✅ Scrollable with `NMScrollableToolBar`
- ✅ Has Play/Pause/Stop/Step buttons
- ✅ Keyboard shortcuts (F5, Shift+F5, F10)
- ✅ Save/Load slot support
- ✅ Auto-save functionality
- ✅ Playback source selector (Script/Graph/Mixed)
- ✅ Status indicator with color coding

**Recommendation**: No changes needed. Current implementation exceeds requirements.

---

### 3.3 Compiler / Parser

#### Issue CP-001: Parser Error Messages Lack Location Context
- **Area**: Compiler
- **Severity**: Medium
- **Priority**: P2
- **File**: `engine_core/src/scripting/parser.cpp:71-77`

**Repro Steps**:
1. Create NM Script with syntax error
2. Compile or run in Play mode
3. Observe error message

**Expected**: "Parse error at line 42, column 15: Expected dialogue text"
**Actual**: "Parse error: Expected dialogue text" (no line/column)

**Root Cause**:
```cpp
void Parser::error(const std::string &message) {
  m_errors.emplace_back(message, peek().location);
}
```
The location IS stored in ParseError, but the Result::error() only takes message string.

**Fix Plan**:
1. Extend Result<T> to carry structured error info
2. Include line/column in error formatting
3. Add source context preview (show the offending line)

**Acceptance Criteria**:
- [ ] All parse errors show line:column
- [ ] Error messages include 2-line source context
- [ ] Errors are navigable from Diagnostics panel

**Tests**:
- Unit: Error message formatting tests
- Integration: Error navigation from Diagnostics

**Dependencies**: Issue DG-001 (Diagnostics navigation)

---

#### Issue CP-002: Parser "0 scenes" vs "1 scene" Discrepancy
- **Area**: Compiler/Runtime
- **Severity**: High
- **Priority**: P1
- **File**: Multiple - requires investigation

**Description**:
As mentioned in the issue, logs show "Parser found 0 scenes" but "Total scenes available: 1" during play mode.

**Root Cause Hypothesis**:
1. Scene Registry may be adding scenes from JSON files
2. Parser only counts scenes in NM Script source
3. These are different data sources that should be unified

**Fix Plan**:
1. Audit scene counting logic in both paths
2. Ensure consistent scene registration
3. Add logging to clarify source of scenes
4. Document the scene discovery flow

**Acceptance Criteria**:
- [ ] Scene counts are consistent in logs
- [ ] Source of each scene is clearly logged
- [ ] No "0 scenes found" when scenes exist

**Tests**:
- Integration: Scene loading from multiple sources

**Dependencies**: None

---

### 3.4 Event Bus / Performance

#### Issue EB-001: Potential Event Spam from SelectionMediator
- **Area**: EventBus
- **Severity**: Medium
- **Priority**: P2
- **File**: `editor/src/mediators/selection_mediator.cpp`

**Description**:
The issue mentions suspicious SelectionMediator spam in logs. Events may be published multiple times per action.

**Root Cause Hypothesis**:
1. Selection change triggers multiple handlers
2. Each handler may publish its own SelectionChanged event
3. No debouncing or coalescing of rapid-fire events

**Fix Plan**:
1. Add event coalescing for SelectionChanged (50ms debounce)
2. Implement "batch update" mode for bulk operations
3. Add event count logging in Debug builds
4. Profile event throughput during typical operations

**Acceptance Criteria**:
- [ ] No more than 1 SelectionChanged per user action
- [ ] Debug mode shows event counts
- [ ] No visible lag on selection changes

**Tests**:
- Unit: Event coalescing tests
- Performance: Event throughput benchmarks

**Dependencies**: None

---

#### Issue EB-002: SceneDocumentModifiedEvent May Fire Excessively
- **Area**: EventBus/Scene
- **Severity**: Medium
- **Priority**: P2
- **File**: `editor/src/qt/panels/nm_scene_view_panel_document.cpp`

**Description**:
Document modification events may fire on every property change during editing.

**Fix Plan**:
1. Implement dirty flag coalescing
2. Only fire document modified on actual content change
3. Consider change batching for undo groups

**Acceptance Criteria**:
- [ ] Document modified fires max once per edit operation
- [ ] Batched edits fire single event

**Tests**: Event count verification tests

**Dependencies**: Issue EB-001 (shared event infrastructure)

---

### 3.5 Test Coverage

#### Issue TC-001: Core Test Coverage Below 100% Target
- **Area**: Testing/Core
- **Severity**: High
- **Priority**: P0
- **File**: `tests/unit/`, `engine_core/`

**Current State**:
- Parser: ~50% coverage (basic tests exist in `test_parser.cpp`)
- Lexer: ~60% coverage (`test_lexer.cpp`)
- VM: ~40% coverage (`test_vm.cpp`, `test_vm_vn.cpp`)
- Compiler: ~30% coverage (minimal)
- Validator: ~30% coverage (`test_validator.cpp`)
- Scene Graph: ~50% coverage (`test_scene_graph.cpp`, `test_scene_graph_deep.cpp`)

**Gap Analysis**:
| Module | Current | Target | Gap |
|--------|---------|--------|-----|
| Lexer | 60% | 100% | 40% |
| Parser | 50% | 100% | 50% |
| Compiler | 30% | 100% | 70% |
| VM | 40% | 100% | 60% |
| Validator | 30% | 100% | 70% |
| Scene Graph | 50% | 100% | 50% |

**Fix Plan**:
1. Generate coverage report with `--coverage` build
2. Identify uncovered branches in critical paths
3. Add tests for:
   - All error paths in parser
   - All statement types in compiler
   - All opcodes in VM
   - All validation rules

**Acceptance Criteria**:
- [ ] 100% statement coverage for scripting/
- [ ] 100% branch coverage for parser error paths
- [ ] All public APIs have at least one test

**Tests**: Meta - the tests ARE the fix

**Dependencies**: None

---

#### Issue TC-002: Qt/UI Test Coverage Insufficient
- **Area**: Testing/UI
- **Severity**: Medium
- **Priority**: P2

**Current State**:
- Integration tests exist for workflows
- No dedicated GUI automation tests
- QTest framework available but underutilized

**Exclusions List** (acceptable gaps):
- QOpenGLWidget rendering internals
- Platform-specific plugin behavior
- Font rendering variations
- Window manager interactions

**Fix Plan**:
1. Add QTest-based panel unit tests
2. Add signal/slot verification tests
3. Add keyboard shortcut tests
4. Document exclusions with rationale

**Acceptance Criteria**:
- [ ] Each panel has constructor/destructor test
- [ ] Critical signals are verified
- [ ] Keyboard shortcuts trigger correct actions

**Dependencies**: None

---

### 3.6 CI/Build Infrastructure

#### Issue CI-001: Coverage Threshold Too Low
- **Area**: CI
- **Severity**: Low
- **Priority**: P3
- **File**: `.github/workflows/ci.yml:315-320`

**Current**: Coverage threshold is 25% (informational only)
**Target**: Should enforce minimum and trend upward

**Fix Plan**:
1. Increase threshold to 40% as blocking
2. Add coverage trend tracking
3. Fail PR if coverage decreases

**Acceptance Criteria**:
- [ ] CI fails if coverage < 40%
- [ ] Coverage badge in README

**Dependencies**: Issue TC-001 (need to increase coverage first)

---

#### Issue CI-002: No Valgrind Job in Standard CI
- **Area**: CI
- **Severity**: Low
- **Priority**: P3
- **File**: `.github/workflows/ci.yml`

**Current**: Only ASan/TSan/UBSan sanitizers run
**Target**: Add Valgrind for memory leak detection (if applicable)

**Fix Plan**:
1. Add optional Valgrind job for nightly builds
2. Focus on engine_core tests
3. Suppress known Qt false positives

**Acceptance Criteria**:
- [ ] Valgrind job runs weekly
- [ ] No new memory leaks introduced

**Dependencies**: None

---

### 3.7 Diagnostics Panel

#### Issue DG-001: Navigation to Source Not Implemented
- **Area**: UI/Diagnostics
- **Severity**: Medium
- **Priority**: P2
- **File**: `editor/src/qt/panels/nm_diagnostics_panel.cpp`

**Current State**:
- Diagnostics panel shows errors and warnings ✅
- Categories and severity highlighting work ✅
- Navigation to source (StoryGraph/Script/Asset) is NOT implemented ❌

**Fix Plan**:
1. Add double-click handler on diagnostic items
2. Parse location string (format: `Type:path:line:col`)
3. Emit navigation signal to appropriate panel
4. Implement navigation receivers in:
   - StoryGraph panel (node focus)
   - Script Editor (cursor position)
   - Asset Browser (file selection)

**Acceptance Criteria**:
- [ ] Double-click opens source location
- [ ] StoryGraph nodes can be focused
- [ ] Script editor scrolls to line

**Tests**:
- Integration: Navigation from Diagnostics to each target

**Dependencies**: None

---

### 3.8 Documentation

#### Issue DOC-001: User Guide Documentation Missing
- **Area**: Documentation
- **Severity**: Medium
- **Priority**: P3

**Current State**:
- Architecture docs exist ✅
- API docs exist ✅
- User-facing "How to Use" guide missing ❌

**Fix Plan**:
1. Create `docs/USER_GUIDE.md`
2. Cover: Project creation, scene editing, scripting, building
3. Add screenshots/GIFs for key workflows
4. Link from README

**Acceptance Criteria**:
- [ ] Complete workflow from project creation to build
- [ ] All major features documented with examples

**Dependencies**: None

---

### 3.9 Project Format / Data Integrity

#### Issue PF-001: Scene JSON Format Version Migration
- **Area**: Data/Persistence
- **Severity**: Medium
- **Priority**: P2

**Description**:
As the project evolves, scene JSON format may change. Need migration strategy.

**Fix Plan**:
1. Add `formatVersion` field to scene JSON
2. Implement version checking on load
3. Add migration functions for version upgrades
4. Warn user if downgrade detected

**Acceptance Criteria**:
- [ ] Format version tracked in all JSON files
- [ ] Graceful handling of older formats
- [ ] Clear error for incompatible formats

**Tests**:
- Unit: Format version detection
- Integration: Migration scenarios

**Dependencies**: None

---

## 4. Roadmap by Phases

### Phase 0: Infrastructure (Foundation)
**Duration**: 1-2 weeks
**Risk**: Low

| Issue | Title | Priority | Dependencies |
|-------|-------|----------|--------------|
| TC-001 | Core Test Coverage | P0 | None |
| CI-001 | Coverage Threshold | P3 | TC-001 |
| CI-002 | Valgrind Job | P3 | None |

**Exit Criteria**:
- [ ] Core test coverage ≥ 80%
- [ ] CI enforces coverage threshold
- [ ] All sanitizers green

---

### Phase 1: Crash/Compile Blockers + Data Safety
**Duration**: 1 week
**Risk**: Medium

| Issue | Title | Priority | Dependencies |
|-------|-------|----------|--------------|
| CP-001 | Parser Error Locations | P2 | None |
| CP-002 | Scene Count Discrepancy | P1 | None |
| PF-001 | Format Version Migration | P2 | None |

**Exit Criteria**:
- [ ] No parse errors without location
- [ ] Scene counts are consistent
- [ ] Format versioning in place

---

### Phase 2: Scene Preview / Gizmos Overhaul
**Duration**: 1-2 weeks
**Risk**: Medium

| Issue | Title | Priority | Dependencies |
|-------|-------|----------|--------------|
| SV-001 | Scale Gizmo Hit Testing | P1 | None |
| SV-002 | Rotate Gizmo Behavior | P2 | None |
| SV-003 | Move Gizmo Sensitivity | P3 | SV-001 |

**Exit Criteria**:
- [ ] All gizmos easily targetable
- [ ] No rotation jumps
- [ ] Smooth visual feedback

---

### Phase 3: Play Controls UI (Already Done!)
**Duration**: 0 (completed)
**Risk**: None

The Play Controls implementation (`NMPlayToolbarPanel`) already meets Unity-like requirements:
- Compact toolbar design ✅
- Dockable ✅
- Full playback controls ✅
- Keyboard shortcuts ✅

---

### Phase 4: Polish, Performance & Documentation
**Duration**: 2-3 weeks
**Risk**: Low

| Issue | Title | Priority | Dependencies |
|-------|-------|----------|--------------|
| EB-001 | Event Spam Prevention | P2 | None |
| EB-002 | Document Modified Coalescing | P2 | EB-001 |
| DG-001 | Diagnostics Navigation | P2 | None |
| TC-002 | UI Test Coverage | P2 | None |
| DOC-001 | User Guide | P3 | None |

**Exit Criteria**:
- [ ] Event counts within bounds
- [ ] All diagnostic navigation works
- [ ] User guide complete

---

### Dependency Graph

```
                    ┌─────────────┐
                    │  Phase 0    │
                    │ Infrastructure│
                    └──────┬──────┘
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
     ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
     │  Phase 1    │ │  Phase 2    │ │  Phase 3    │
     │  Blockers   │ │   Gizmos    │ │  PlayCtrl   │
     │             │ │             │ │  (DONE!)    │
     └──────┬──────┘ └──────┬──────┘ └─────────────┘
            │              │
            └──────┬───────┘
                   ▼
            ┌─────────────┐
            │  Phase 4    │
            │   Polish    │
            └─────────────┘
```

---

## 5. Architecture Guardrails

### 5.1 Event Publishing Rules

**RULE EB-R01**: Maximum event rate
Events of the same type MUST NOT be published more than once per 50ms without explicit batching.

**Implementation**:
```cpp
// In EventBus
void publishWithDebounce(const EditorEvent& event,
                         std::chrono::milliseconds debounce = 50ms);
```

**RULE EB-R02**: No events in handlers
Event handlers MUST NOT directly publish new events. Use `queueEvent()` instead.

**RULE EB-R03**: Handler duration limit
Event handlers MUST complete within 10ms. Long operations should be queued.

---

### 5.2 Single Source of Truth

**RULE SST-R01**: SceneGraph is truth
The `engine_core::SceneGraph` is the authoritative source for scene state. GUI panels MUST read from it, never cache stale state.

**RULE SST-R02**: Commands for mutations
All state mutations MUST go through `QUndoCommand` subclasses. Direct modification is forbidden.

**RULE SST-R03**: Selection single source
`SelectionSystem` is the single source for selection state. Panels MUST subscribe and react.

---

### 5.3 Memory Management

**RULE MEM-R01**: No raw pointers in QVariant
NEVER store raw pointers in `QVariant::setData()`. Use stable IDs (QString, int).

**RULE MEM-R02**: engine_core uses smart pointers
All `engine_core` objects MUST be managed via `std::unique_ptr` or `std::shared_ptr`.

**RULE MEM-R03**: QObject parent ownership
Qt widgets MUST have proper parent ownership chains. Orphan widgets are forbidden.

---

### 5.4 Signal/Slot Hygiene

**RULE SIG-R01**: No duplicate connections
Before calling `connect()`, verify no existing connection exists or use `Qt::UniqueConnection`.

**RULE SIG-R02**: Disconnect on destruction
Mediators and long-lived objects MUST disconnect in destructor or use `ScopedEventSubscription`.

---

### 5.5 Performance Budgets

| Operation | Budget | Enforcement |
|-----------|--------|-------------|
| Event handler | < 10ms | Profiling in Debug |
| Panel update | < 16ms | 60 FPS target |
| Project open | < 3s | Startup benchmark |
| Selection change | < 100ms | UX requirement |

---

## 6. Quick Wins vs Big Rocks

### Quick Wins (1-2 days each)

1. **SV-003 Move Gizmo Sensitivity** - Increase hit area widths
   - 2 hours
   - Low risk
   - Immediate UX improvement

2. **CI-001 Coverage Threshold** - Update YAML
   - 1 hour
   - Requires TC-001 first

3. **DOC-001 User Guide Start** - Create skeleton
   - 4 hours
   - No code risk

4. **CP-001 Parser Error Locations** - Format improvement
   - 4 hours
   - Self-contained

### Big Rocks (1 week+)

1. **TC-001 Core Test Coverage** - 80%→100%
   - 2-3 weeks
   - Critical for stability
   - Foundation for everything else

2. **SV-001 + SV-002 Gizmo Overhaul**
   - 1 week
   - Visible UX improvement
   - Medium complexity

3. **EB-001 + EB-002 Event System Hardening**
   - 1 week
   - Performance and reliability
   - Requires careful testing

4. **DG-001 Diagnostics Navigation**
   - 3-4 days
   - Involves multiple panels
   - Cross-cutting concern

---

## Appendix A: File References

### Key Files Analyzed

| Component | File | Lines |
|-----------|------|-------|
| Gizmo | `nm_scene_view_gizmo.cpp` | 488 |
| Play Controls | `nm_play_toolbar_panel.cpp` | 520 |
| Event Bus | `event_bus.hpp` | 685 |
| Parser | `parser.cpp` | 200+ |
| CI | `ci.yml` | 331 |

### Test Files Reviewed

- `tests/unit/test_parser.cpp` - Parser unit tests
- `tests/unit/test_lexer.cpp` - Lexer tests
- `tests/unit/test_vm.cpp` - VM tests
- `tests/integration/test_scene_workflow.cpp` - Scene integration

---

## Appendix B: Definition of Done Checklist

### For 100% Completion

- [ ] **Zero Critical/High Bugs**: All issues in this report resolved
- [ ] **Test Coverage**:
  - [ ] 100% statement/branch for core/scripting/
  - [ ] ≥60% for editor components
  - [ ] Documented exclusions for Qt internals
- [ ] **CI Stability**:
  - [ ] ASan green
  - [ ] TSan green
  - [ ] UBSan green
  - [ ] Coverage enforced
- [ ] **UX Complete**:
  - [ ] Gizmos usable
  - [ ] Play Controls functional
  - [ ] Settings persist
  - [ ] No event spam
- [ ] **Documentation**:
  - [ ] User guide complete
  - [ ] Architecture docs current
  - [ ] API documented

---

*Document generated by automated audit process. Last updated: 2026-01-11*
