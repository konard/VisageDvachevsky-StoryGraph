# NovelMind Editor - Comprehensive Deep Audit Report

**Date**: 2026-01-11
**Scope**: Complete Definition of Done audit covering all subsystems
**Issue**: https://github.com/VisageDvachevsky/StoryGraph/issues/550
**Branch**: issue-550-5c4fcea976cd

---

## Executive Summary

This comprehensive audit examined the entire NovelMind Editor codebase - a C++20/Qt6 visual novel editor - for bugs, architectural issues, anti-patterns, incomplete functionality, and quality gaps. The audit covers:

- **79 engine core source files** across 13 modules
- **135 editor source files** with 49 Qt UI panels
- **54 test files** (unit, integration, abstraction)
- **3 CI workflows** with sanitizer coverage
- **Build system** with asset packing and encryption

### Summary Statistics

| Category | Critical | High | Medium | Low | Total |
|----------|----------|------|--------|-----|-------|
| Scripting Subsystem | 8 | 7 | 8 | - | 23 |
| Audio Subsystem | 4 | 6 | 4 | - | 14 |
| VFS Subsystem | 2 | 4 | 7 | - | 13 |
| Scene Subsystem | 3 | 8 | 10 | 7 | 28 |
| Editor Panels | 3 | 5 | 8 | - | 16 |
| Mediators & EventBus | 2 | 5 | 6 | 4 | 17 |
| Build System | 5 | 5 | 3 | - | 13 |
| Tests & CI | - | 6 | 4 | 2 | 12 |
| **TOTAL** | **27** | **46** | **50** | **13** | **136** |

### Key Findings

1. **27 Critical Bugs** requiring immediate attention (crashes, security, data corruption)
2. **6 Modules with 0% Test Coverage** (Localization, Resource, Save, UI, Renderer partial, VFS Security partial)
3. **58 Trivial Test Assertions** (`REQUIRE(true)`) that test nothing
4. **Multiple Race Conditions** in EventBus, AudioRecorder, PropertyMediator
5. **Security Vulnerabilities** in path traversal, encryption key handling, pack file parsing
6. **Missing Features** claimed in headers but not implemented (code signing, texture atlas, image optimization)

---

## 1. Scripting Subsystem Audit

### Critical Bugs (8)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| SC-001 | `vm.cpp:647-655` | Stack underflow returns `monostate{}` silently | Logic corruption, infinite loops |
| SC-002 | `vm.cpp:320-330` | DIV by zero pushes `int(0)` instead of `float(0.0f)` | Type confusion |
| SC-003 | `vm.cpp:426-433` | MOD by zero silently returns 0 | Hidden errors |
| SC-004 | `vm.cpp:657-668` | getString() returns reference to static empty string | Heap corruption possible |
| SC-005 | `lexer.cpp:586-600` | Color literal size subtraction underflow | UB on `#` alone |
| SC-006 | `lexer.cpp:540-545` | `std::stoi/stof` can throw on overflow | Crash on bad input |
| SC-007 | `ir_conversion.cpp:44-66` | No null check after `createNode()` | Null dereference crash |
| SC-008 | `vm_debugger.cpp:12-17` | Constructor logs null VM but continues | Guaranteed crash later |

### High Priority (7)

- Parser array bounds check off-by-one (`parser.cpp:237-240`)
- Compiler jump bounds check doesn't catch overflow (`compiler.cpp:93-105`)
- UTF-8 decoding position corruption on error (`lexer.cpp:49-95`)
- Script runtime entry point not validated (`script_runtime.cpp:86-113`)
- Variable history ring buffer unbounded (`vm_debugger.hpp:449`)
- Breakpoint condition evaluation stub (`vm_debugger.cpp:430-435`)
- Security guard doesn't clear allowed functions on reset (`vm_security.cpp:9-14`)

### Architecture Issues (8)

- Broad catch-all in compiler masks real errors
- Parser synchronization misses recovery cases
- Race condition in debugger breakpoint lookup
- String table deduplication is O(n) per string (O(n²) total)
- No thread safety in debugger variable history
- Memory leak on failed IR node creation
- Exception handling patterns inconsistent
- Node ID allocation has no overflow protection

---

## 2. Audio Subsystem Audit

### Critical Bugs (4)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| AU-001 | `audio_manager.cpp:739-740` | Handle ID overflow at u32 max | Collision, use-after-free |
| AU-002 | `audio_manager.cpp:302-318` | createSource called outside lock | Race, exceeds max sounds |
| AU-003 | `voice_manifest.cpp:784-852` | Naive JSON regex fails on nested objects | Data corruption |
| AU-004 | `voice_manifest.cpp:490` | Path traversal in file validation | Security bypass |

### High Priority (6)

- Device ID collision when names match (`audio_recorder.cpp:203-204`)
- Lock ordering violation documented but not followed (`audio_recorder.hpp:437-442`)
- Voice playback state race condition (`audio_manager.cpp:495-497`)
- Raw pointer lifetime after lock release (`audio_manager.cpp:663-675`)
- String search beyond array end (`voice_manifest.cpp:791`)
- Regex injection via malformed locale (`voice_manifest.cpp:831-833`)

### Medium Priority (4)

- Missing audio format parameter validation
- No resource limits or memory tracking
- Callback invocation pattern issues
- Custom JSON parser fragility

---

## 3. VFS Subsystem Audit

### Critical Bugs (2)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| VFS-001 | `pack_reader.cpp:270-273` | Integer overflow in offset calculation | Read arbitrary data |
| VFS-002 | `pack_security.cpp:185-190` | Overflow in string boundary check | Bypass validation |

### High Priority (4)

- Buffer underflow in MemoryFileHandle::read() (`file_handle.cpp:68`)
- Race condition in CachedFileSystem LRU (`cached_file_system.cpp:93-102`)
- String table size limit can overflow (`pack_reader.cpp:192-206`)
- Path traversal in pack discovery (`multi_pack_manager.cpp:391-431`)

### Medium Priority (7)

- Missing null pointer check before CRC calculation
- Unvalidated file size reads throw uncaught
- Cache entry lifecycle inconsistent (dual-map)
- Dead code with unused index variable
- Resource count allocation before seek validation
- Inconsistent const method mutations
- String table type mismatch warnings

---

## 4. Scene Subsystem Audit

### Critical Bugs (3)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| SN-001 | `scene_inspector.cpp:485-517` | moveObject() only undoes X, not Y | Undo inconsistency |
| SN-002 | `scene_inspector.cpp:668-680` | Undo stack limiting reverses order | History corruption |
| SN-003 | `scene_object_handle.hpp:50-51` | Race condition in isValid() | Use-after-free |

### Major Issues (8)

- Unsafe static_cast without type verification (multiple locations)
- Integer truncation in choice menu navigation causes buffer overrun
- String comparison instead of enum for layers (fragile serialization)
- DialogueBox text size race during animation
- Observer notification while iterating (iterator invalidation)
- PropertyChange notification loses float precision
- getUndoHistory/getRedoHistory always return empty vectors
- Object creation notifies observers before returning (use-after-free risk)

### Incomplete Implementations (7)

- Layer type enum not extensible
- Serialization format not specified
- Animation timeline duration not tracked
- Child objects not saved in state
- Transition render methods are stubs
- No texture filtering settings
- Resource manager assignment incomplete

---

## 5. Editor Panels Audit

### Critical Bugs (3)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| ED-001 | `nm_build_settings_panel.cpp:594` | BuildSystem shared_ptr not stored | Memory corruption |
| ED-002 | `nm_asset_browser_panel.cpp:184` | Null check missing before dereference | Crash |
| ED-003 | `nm_animation_adapter.cpp:281-291` | Object cache race condition | Dangling pointer |

### High Priority (5)

- Signal/slot lifetime issues in HelpHubPanel
- Memory leak potential in ConsolePanel
- Integer overflow in LocalizationPanel percentage
- Double-delete in CurveEditorPanel grid
- Missing audio manager null check

### Medium Priority (8)

- Dangling references in InspectorPanel
- Fragile enum-index mapping in HierarchyPanel
- No asset browser refresh on import
- Incomplete build system integration
- Missing progress cancel functionality
- Inefficient filter application
- Excessive tree rebuilds
- Tight coupling between panels

---

## 6. Mediators & EventBus Audit

### Critical Bugs (2)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| MB-001 | `scene_mediator.cpp:214` | Node lookup after deletion | Memory leak |
| MB-002 | `event_bus.cpp:79-109` | Race condition in dispatcher | Inconsistent state |

### High Priority (5)

- Feedback loop prevention inadequate in PropertyMediator
- Double-lock potential in SelectionMediator
- TOCTOU null pointer in WorkflowMediator
- Stack overflow risk in SceneMediator sync
- EventBus type casting can fail silently

### Architecture Issues (6)

- Circular dependencies between mediators and EventBus
- Tight coupling between PropertyMediator and undo system
- Mixed Qt signals and EventBus patterns
- No event unsubscribe verification
- No event coalescing/batching
- No handler execution priority

### Adapter Issues (4)

- QtAudioPlayer doesn't handle initialization failure
- QtFileSystem missing error information
- deleteDirectory() with rmdir(".") ambiguous
- History update performance bottleneck

---

## 7. Build System Audit

### Security Issues (5)

| ID | Location | Issue | Impact |
|----|----------|-------|--------|
| BS-001 | `build_size_analyzer.cpp:1157` | Weak hash for duplicate detection | False positives |
| BS-002 | `build_system.cpp:405-432` | No exception handling for key parsing | Crash on bad key |
| BS-003 | `build_system.cpp:1029-1072` | Path traversal in asset mapping | Write outside output |
| BS-004 | `build_system.cpp:372` | Unvalidated signing paths | Code injection risk |
| BS-005 | `build_system.hpp:113` | Encryption key in plain memory | Memory exposure |

### Logic Bugs (5)

- Integer overflow in file size calculations
- Pack file hash calculated from unencrypted data
- Build timestamp not consistent across packs
- Metadata JSON path not sanitized
- Header reading bounds assumptions

### Missing Implementations (3)

- Texture atlas generation (stub)
- Image/audio optimization (stub)
- Code signing (stub)

---

## 8. Tests & CI Audit

### Missing Test Coverage (Critical)

| Module | Coverage | Status |
|--------|----------|--------|
| Localization | 0% | **MISSING** |
| Resource | 0% | **MISSING** |
| Save | 0% | **MISSING** |
| UI (engine) | 0% | **MISSING** |
| Renderer | ~17% | **CRITICAL** |
| Scene | ~20% | **CRITICAL** |
| VFS Security | ~25% | **CRITICAL** |
| Editor Panels | <5% | **CRITICAL** (49 panels, 2 tested) |

### Test Quality Issues

- **58 trivial assertions** (`REQUIRE(true)`)
- **5 skipped audio tests** due to hardware dependency
- **1 commented test file** (`test_scene_graph_deep.cpp`)
- Timing-dependent tests (flaky risk)
- Cross-platform gaps

### CI Gaps

- LeakSanitizer (LSAN) not enabled
- MemorySanitizer (MSAN) not configured
- TSan only runs nightly (not on PRs)
- clang-tidy only checks first 20 files
- 25% coverage threshold too low
- Windows sanitizer builds missing

---

## 9. Phased Roadmap

### Phase 1: Critical Security & Stability (Weeks 1-2)

**Priority: P0 - Must fix immediately**

1. **Fix all critical bugs** (27 issues)
   - Stack underflow handling in VM
   - Race conditions in EventBus and AudioRecorder
   - Path traversal vulnerabilities
   - Encryption key handling

2. **Enable missing sanitizers**
   - Add LSAN to CI
   - Run TSan on PRs (not just nightly)
   - Fix clang-tidy to check all files

3. **Fix commented test file**
   - Uncomment `test_scene_graph_deep.cpp`
   - Update renderer mock API

### Phase 2: High Priority Fixes (Weeks 3-4)

**Priority: P1 - Fix before any release**

1. **Fix high priority bugs** (46 issues)
   - Integer overflows in VFS
   - Type confusion in VM
   - Memory leaks in scene inspector
   - Panel lifecycle issues

2. **Add critical test coverage**
   - Localization manager tests
   - Save manager tests
   - VFS security tests
   - Scene graph tests

3. **Replace trivial assertions**
   - Fix 58 `REQUIRE(true)` cases
   - Add meaningful validations

### Phase 3: Medium Priority & Architecture (Weeks 5-8)

**Priority: P2 - Important improvements**

1. **Fix medium priority bugs** (50 issues)
   - Improve error handling
   - Add missing validations
   - Fix observer patterns

2. **Add UI panel tests**
   - Scene View Panel
   - Timeline Editor Panel
   - Inspector Panel
   - Project Panel
   - Console Panel

3. **Architecture improvements**
   - Consolidate EventBus patterns
   - Fix circular dependencies
   - Add event coalescing

### Phase 4: Polish & Completion (Weeks 9-12)

**Priority: P3 - Quality improvements**

1. **Complete stub implementations**
   - Texture atlas generation
   - Image optimization
   - Audio optimization
   - Code signing

2. **Improve CI coverage**
   - Add Windows sanitizers
   - Screenshot-based GUI tests
   - Performance benchmarks

3. **Documentation**
   - Update API documentation
   - Add security guidelines
   - Create testing guide

---

## 10. Dependency Graph

```
Phase 1 (Critical) ──┬── EventBus race fix
                     ├── VM stack underflow fix
                     ├── Path traversal fixes
                     ├── Enable LSAN
                     └── Fix clang-tidy scope

        │
        ▼

Phase 2 (High) ──────┬── Integer overflow fixes (depends on Phase 1 sanitizers)
                     ├── Type safety improvements
                     ├── Memory leak fixes
                     ├── Add missing tests (Localization, Save, VFS)
                     └── Replace trivial assertions

        │
        ▼

Phase 3 (Medium) ────┬── Observer pattern fixes
                     ├── Architecture consolidation
                     ├── UI panel tests
                     └── Error handling improvements

        │
        ▼

Phase 4 (Polish) ────┬── Complete stub implementations
                     ├── Advanced CI features
                     └── Documentation updates
```

---

## 11. Existing Issues Cross-Reference

The following existing issues (#444-#550) were identified during this audit and should not be duplicated:

- Issues #180-#187: [UltraAudit] categories from previous audit
- Issues #444-#549: Various bug reports and feature requests

This audit extends the previous ULTRA_AUDIT_REPORT.md with:
- Deeper code-level analysis
- Build system coverage
- CI infrastructure review
- Complete phased roadmap

---

## 12. Acceptance Criteria for Production-Ready

The NovelMind Editor will be considered production-ready when:

1. **All 27 critical bugs fixed** and verified with regression tests
2. **Test coverage above 50%** for engine core, 40% for editor
3. **Zero trivial assertions** remaining
4. **All sanitizers passing** (ASAN, UBSAN, TSAN, LSAN)
5. **CI checks all source files** with clang-tidy
6. **Zero commented-out tests**
7. **All stub implementations complete** or clearly marked as future work
8. **Security vulnerabilities resolved** (path traversal, encryption, injection)
9. **Race conditions eliminated** or documented with thread safety guarantees
10. **Documentation complete** for public APIs

---

## Appendix A: Files Audited

### Engine Core (79 files)
- audio/ (4 files)
- core/ (8 files)
- input/ (1 file)
- localization/ (1 file)
- platform/ (1 file)
- renderer/ (7 files)
- resource/ (1 file)
- save/ (1 file)
- scene/ (19 files)
- scripting/ (10 files)
- ui/ (5 files)
- vfs/ (16 files)

### Editor (135 files)
- adapters/ (2 files)
- build_system (2 files)
- mediators/ (8 files)
- qt/dialogs/ (12 files)
- qt/panels/ (49 files)
- qt/controllers/ (5 files)
- qt/other/ (57 files)

### Tests (54 files)
- unit/ (38 files)
- integration/ (14 files)
- abstraction/ (2 files)

---

*Generated with Claude Code as part of Definition of Done audit for issue #550*
