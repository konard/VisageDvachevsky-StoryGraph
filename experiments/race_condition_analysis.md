# Audio Recorder Race Condition Analysis

## Threads Involved
1. **Main Thread**: Calls `stopRecording()`, `cancelRecording()`, `startRecording()`
2. **Audio Thread**: Calls `audioCallback()` → `processAudioData()` → `writeToFile()`
3. **Processing Thread**: Calls `finalizeRecording()` (spawned by `stopRecording()`)

## Shared State Variables

### Declared as `std::atomic`
- `m_state` (RecordingState) - line 416 in header
- `m_monitoringEnabled` - line 406 in header
- `m_monitoringVolume` - line 407 in header
- `m_meteringActive` - line 410 in header
- `m_samplesRecorded` - line 423 in header
- `m_processingActive` - line 434 in header
- `m_cancelRequested` - line 444 in header

### NOT atomic (plain variables)
- `m_outputPath` (std::string) - line 417 in header
- `m_recordBuffer` (std::vector<f32>) - line 422 in header
- `m_encoder` (std::unique_ptr<ma_encoder>) - line 418 in header
- `m_captureDevice` (std::unique_ptr<ma_device>) - line 402 in header

### Protected by mutexes
- `m_encoder` - protected by `m_recordMutex` (line 419 in header)
- `m_captureDevice` - protected by `m_resourceMutex` (line 442 in header)
- Callbacks - protected by `m_callbackMutex` (line 426 in header)
- `m_currentLevel` - protected by `m_levelMutex` (line 411 in header)

## Race Conditions Identified

### Race 1: `m_outputPath` access (lines 648, 655, 659, 685, 699, 778)
**Location**: `finalizeRecording()` lines 648-659
**Threads**: Processing Thread vs Main Thread (in `cancelRecording()` line 458)

```cpp
// Processing Thread in finalizeRecording():
result.filePath = m_outputPath;         // line 648 - READ
if (fs::exists(m_outputPath)) {         // line 655 - READ
  result.fileSize = fs::file_size(m_outputPath); // line 656 - READ
}
m_outputPath.clear();                   // line 659 - WRITE

// Main Thread in cancelRecording():
if (!m_outputPath.empty() && ...) {     // line 440 - READ
  fs::remove(m_outputPath);             // line 442 - READ
}
m_outputPath.clear();                   // line 458 - WRITE
```

**Problem**: `m_outputPath` is a std::string accessed from multiple threads without synchronization. This can cause:
- Use-after-free if one thread clears while another reads
- Corrupted string data
- Filesystem operations on wrong/partial paths

### Race 2: `m_samplesRecorded` non-atomic operations (lines 586, 660, 468)
**Location**: Multiple locations
**Threads**: Audio Thread vs Processing Thread

```cpp
// Audio Thread in writeToFile():
m_samplesRecorded += sampleCount;       // line 586 - READ-MODIFY-WRITE

// Processing Thread in finalizeRecording():
m_samplesRecorded = 0;                  // line 660 - WRITE

// Main Thread in getRecordingDuration():
return static_cast<f32>(m_samplesRecorded) / ... // line 468 - READ
```

**Problem**: Although `m_samplesRecorded` is declared as `std::atomic<u64>`, the atomic operations are not properly synchronized with other state changes. The race is in the sequence:
1. Audio thread still writing (`writeToFile()` line 586)
2. Processing thread resets to 0 (line 660)
This can cause incorrect duration calculation.

### Race 3: `m_processingActive` and `m_cancelRequested` ordering
**Location**: `finalizeRecording()` lines 591-661
**Threads**: Processing Thread vs Main Thread

```cpp
// Processing Thread in finalizeRecording():
if (m_cancelRequested) {
  m_processingActive = false;           // line 592
  return;
}

// Later...
m_processingActive = false;             // line 661

// Main Thread in shutdown():
m_processingActive = false;             // line 71
joinProcessingThread();                 // line 72
```

**Problem**: Setting `m_processingActive = false` before actually finishing all work can cause:
- Main thread thinks processing is done when it's not
- Resources cleaned up while still in use

### Race 4: Access to `m_outputPath` in `processRecording()` (lines 685, 699, 778)
**Location**: `processRecording()` called from `finalizeRecording()`
**Threads**: Processing Thread (but can race with cancel)

```cpp
// In processRecording():
if (!fs::exists(m_outputPath)) {        // line 685 - READ
  return;
}

if (!readWavFile(m_outputPath, ...)) {  // line 699 - READ
  return;
}

writeWavFile(m_outputPath, ...);        // line 778 - READ
```

**Problem**: These accesses happen after checking `m_cancelRequested` at line 690, but `m_outputPath` could be cleared by `cancelRecording()` between checks.

### Race 5: State transitions without proper synchronization
**Location**: `setState()` line 930
**Threads**: Multiple threads

```cpp
void AudioRecorder::setState(RecordingState state) {
  m_state = state;                      // line 931 - atomic write

  OnRecordingStateChanged callback;
  {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    callback = m_onStateChanged;        // line 937
  }
  if (callback) {
    callback(state);                    // line 940
  }
}
```

**Problem**: While `m_state` is atomic, the callback is fired AFTER releasing the lock, and the state might have changed again before the callback executes. This is a TOCTOU (time-of-check-time-of-use) issue.

## Specific Issues in Lines 589-676 (finalizeRecording)

1. **Line 591-594, 599-602, 609-612, 624-627, 636-639**: Multiple checks of `m_cancelRequested` with early returns setting `m_processingActive = false` are good, but...

2. **Line 648, 655, 659**: `m_outputPath` is accessed without any mutex protection. This races with `cancelRecording()` line 440-458.

3. **Line 660**: `m_samplesRecorded = 0` without ensuring audio callback has stopped writing to it.

4. **Line 661**: `m_processingActive = false` is set before callback at line 673, which could cause shutdown() to think processing is done while callback is still running.

## Root Causes

1. **Missing mutex protection for `m_outputPath`**: This std::string is accessed from multiple threads without synchronization.

2. **No synchronization between audio callback and finalization**: The audio callback (`writeToFile`) can still be running while `finalizeRecording()` resets state.

3. **TOCTOU issues with `m_cancelRequested` checks**: Multiple checks scattered throughout with operations in between.

4. **Premature `m_processingActive = false`**: Set before all work (including callback) is complete.

## Recommended Fixes

1. **Add mutex for `m_outputPath`**: Create a dedicated mutex or use existing `m_recordMutex` to protect all access to `m_outputPath`.

2. **Ensure audio callback has stopped**: Before resetting `m_samplesRecorded` or accessing `m_encoder`, ensure the device has stopped and no more audio callbacks will occur.

3. **Move `m_processingActive = false` to very end**: Only set after ALL work including callbacks is done.

4. **Use a copy of `m_outputPath`**: In `finalizeRecording()`, make a local copy of `m_outputPath` under mutex protection at the start, then use the local copy throughout.

5. **Add fence/barrier operations**: Ensure memory ordering between audio thread and processing thread.

6. **Consider lock-free ring buffer**: For audio data, use a lock-free SPSC (single-producer-single-consumer) ring buffer to avoid mutex in audio callback.
