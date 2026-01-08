# Test Plan: Story Graph Playback Issue #204

## Problem Statement
User reports: "Я вроде собрал небольшой граф, нажал запустить и ничего"
Translation: "I assembled a small graph, pressed play and nothing happened"

## Root Cause Hypothesis
The `.novelmind/story_graph.json` file may not exist or may not be properly populated when:
1. User creates a new graph from scratch
2. User modifies existing graph nodes
3. Playback mode is set to "Graph" but the JSON file is missing/empty

## Diagnostic Logging Added
- `NMPlayModeController::play()` - Tracks play button clicks
- `NMPlayModeController::loadProject()` - Shows project paths
- `EditorRuntimeHost::compileProject()` - Shows playback mode
- `loadStoryGraphScript()` - Shows if story_graph.json exists

## Test Scenarios

### Scenario 1: Fresh Project with No Graph
1. Create new empty project
2. Don't create any story graph nodes
3. Set playback mode to "Graph"
4. Click Play
**Expected**: Error message about missing story graph
**Logs should show**: "Story graph file NOT FOUND"

### Scenario 2: Project with Graph but No Entry Node
1. Create new empty project
2. Create 2-3 story graph nodes
3. Don't set entry node
4. Click Play
**Expected**: Error or use first scene
**Logs should show**: Graph loaded, but no entry scene

### Scenario 3: Complete Graph
1. Create new empty project
2. Create story graph with:
   - Entry scene node
   - Dialogue node
   - Connection between them
3. Set entry node (right-click → Set as Entry)
4. Click Play
**Expected**: Play should start successfully
**Logs should show**: Successful compilation and execution

### Scenario 4: Mixed Mode Fallback
1. Create project with both scripts and graph
2. Set mode to "Mixed"
3. Click Play
**Expected**: Should work even if graph is incomplete

## Fix Strategy

Based on findings, the fix should:

1. **Immediate Fix**: Ensure `saveGraphLayout()` is called when:
   - New node is created
   - Node data changes (dialogue text, choices, etc.)
   - Connections are added/removed
   - Entry node is set

2. **Verification**: Check that `story_graph.json` includes:
   - All node data (dialogue, choices, conditions)
   - All connections (via choiceTargets, conditionTargets)
   - Entry scene ID

3. **Error Handling**: When Play fails:
   - Show clear error message in UI
   - Suggest switching to Script mode if graph is unavailable
   - Provide actionable guidance

4. **Fallback**: Consider defaulting to Script mode if graph is empty/missing

## Expected Log Output on Success

```
[PlayMode] === PLAY BUTTON CLICKED ===
[PlayMode] Current mode: 0
[PlayMode] Starting from stopped state, loading runtime...
[PlayMode] === LOADING PROJECT ===
[PlayMode] Project path: /path/to/project
[PlayMode] Scripts path: /path/to/project/Scripts
[PlayMode] Start scene: intro_scene
[PlayMode] Calling EditorRuntimeHost::loadProject()...
[EditorRuntimeHost] === COMPILING PROJECT ===
[EditorRuntimeHost] Playback source mode: 1 (0=Script, 1=Graph, 2=Mixed)
[EditorRuntimeHost] Graph mode: Loading story graph...
[loadStoryGraphScript] Looking for graph at: /path/to/project/.novelmind/story_graph.json
[loadStoryGraphScript] File exists, opening...
[loadStoryGraphScript] File loaded, size: 1234 bytes
[loadStoryGraphScript] JSON parsed successfully, converting to script...
[loadStoryGraphScript] Script generated successfully, length: 567 characters
[EditorRuntimeHost] Story graph loaded successfully, converting to script...
[EditorRuntimeHost] Generated script length: 567 characters
[EditorRuntimeHost] Total script content: 567 characters
[EditorRuntimeHost] Starting compilation pipeline...
[EditorRuntimeHost] Step 1/4: Lexer tokenization...
[EditorRuntimeHost] Lexer: Generated 42 tokens
[EditorRuntimeHost] Step 2/4: Parser...
[EditorRuntimeHost] Parser: Found 2 scenes
[EditorRuntimeHost] Step 3/4: Validator...
[EditorRuntimeHost] Validator: No errors
[EditorRuntimeHost] Step 4/4: Compiler...
[EditorRuntimeHost] === COMPILATION SUCCESSFUL ===
[EditorRuntimeHost] Scenes available: 2
[PlayMode] Project loaded successfully!
[PlayMode] Total scenes available: 2
[PlayMode] Runtime loaded successfully, calling play()...
[PlayMode] Runtime started successfully!
[PlayMode] Play mode activated, timer started
```

## Expected Log Output on Failure (Missing Graph)

```
[PlayMode] === PLAY BUTTON CLICKED ===
[PlayMode] Starting from stopped state, loading runtime...
[PlayMode] === LOADING PROJECT ===
[PlayMode] Calling EditorRuntimeHost::loadProject()...
[EditorRuntimeHost] === COMPILING PROJECT ===
[EditorRuntimeHost] Playback source mode: 1 (Graph mode)
[EditorRuntimeHost] Graph mode: Loading story graph...
[loadStoryGraphScript] Looking for graph at: /path/.novelmind/story_graph.json
[loadStoryGraphScript] Story graph file NOT FOUND!
[loadStoryGraphScript] Expected location: /path/.novelmind/story_graph.json
[loadStoryGraphScript] This file should be created when you modify nodes in Story Graph panel
[EditorRuntimeHost] GRAPH LOAD FAILED: Story graph file not found
[EditorRuntimeHost] Expected file: /path/.novelmind/story_graph.json
[PlayMode] Failed to load project for runtime: Graph mode selected but story graph not available
[PlayMode] CRITICAL: Runtime initialization failed
```
