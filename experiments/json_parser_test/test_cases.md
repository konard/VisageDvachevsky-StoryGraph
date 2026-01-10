# JSON Parser Test Cases (Issue #396)

## Purpose
Verify that the JSON parser safety fixes prevent hangs on malformed input while maintaining compatibility with valid JSON.

## Test Cases

### 1. Unterminated String (Malformed)
**File:** `malformed_unterminated_string.json`
```json
{
  "nodes": [
    {"title": "This string never closes...
    ... (continues for potentially many bytes)
  ]
}
```

**Expected behavior:**
- Parser should detect unterminated string after reading up to 1 MB
- Should return error: "String too long (> 1 MB) - possibly unterminated string"
- Should NOT hang or block UI
- Should complete within milliseconds

### 2. Large File (>10 MB)
**File size:** > 10 MB

**Expected behavior:**
- File size check should reject before parsing
- Should return error: "Story graph file too large (X bytes, max 10485760 bytes)"
- Should complete immediately (just a file size check)

### 3. Normal Story Graph (Valid)
**File:** `valid_story_graph.json`
```json
{
  "nodes": [
    {
      "id": "scene1",
      "type": "Scene",
      "title": "Start",
      "dialogueText": "Welcome to the story",
      "speaker": "Narrator"
    },
    {
      "id": "scene2",
      "type": "Choice",
      "dialogueText": "What will you do?",
      "choiceTargets": {
        "Go left": "scene_left",
        "Go right": "scene_right"
      }
    }
  ],
  "entry": "scene1"
}
```

**Expected behavior:**
- Should parse successfully
- Should generate valid NMScript output
- Should complete within milliseconds
- No regression in functionality

### 4. Normal String with Escapes (Valid)
```json
{
  "nodes": [{
    "dialogueText": "She said: \"Hello!\\nHow are you?\\tI'm fine.\""
  }]
}
```

**Expected behavior:**
- Should handle escape sequences correctly
- Should not be affected by string length limit (< 1 MB)
- Should preserve newlines, tabs, quotes

## Performance Requirements

All operations should complete quickly:
- File size check: < 1 ms
- Valid JSON parsing (< 100 KB): < 10 ms
- Malformed JSON detection: < 100 ms (fail fast)

## Manual Testing Steps

1. Build the project with the fix
2. Create a test project with story graph
3. Manually corrupt `.novelmind/story_graph.json`:
   - Remove closing quote from a string value
   - Save and try to load project in Graph mode
4. Verify error message appears quickly (not hang)
5. Fix the JSON and verify it works again

## Safety Limits

The following limits are enforced:
- **File size:** 10 MB maximum (checked before parsing)
- **String length:** 1 MB maximum (checked during parsing)

These limits are generous for story graph files, which are typically < 100 KB.
