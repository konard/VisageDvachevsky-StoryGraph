# Fix Plan for Audio Recorder Race Conditions

## Summary of Changes

### 1. Protect `m_outputPath` with mutex
**Issue**: `m_outputPath` (std::string) is accessed from multiple threads without synchronization.
**Fix**: Use `m_recordMutex` to protect all access to `m_outputPath`.

**Affected locations**:
- `startRecording()` line 354 - WRITE (assign)
- `cancelRecording()` line 440, 458 - READ, WRITE (read, clear)
- `getRecordingDuration()` - no m_outputPath access (OK)
- `finalizeRecording()` lines 648, 655, 659 - READ, WRITE (read, clear)
- `processRecording()` lines 685, 699, 778 - READ

**Implementation**:
- In `finalizeRecording()`: At the very beginning (after first `m_cancelRequested` check), make a local copy of `m_outputPath` under `m_recordMutex` protection.
- Use the local copy throughout `finalizeRecording()` and `processRecording()`.
- Clear `m_outputPath` under `m_recordMutex` at the end.
- In `startRecording()`: Protect the write with `m_recordMutex`.
- In `cancelRecording()`: Protect reads/writes with `m_recordMutex`.

### 2. Ensure audio callback has stopped before resetting state
**Issue**: `m_samplesRecorded` can still be modified by audio callback while `finalizeRecording()` is running.
**Fix**: Stop the capture device (which stops the audio callback) before reading final `m_samplesRecorded` value.

**Implementation**:
- In `finalizeRecording()`, stop the capture device BEFORE creating the RecordingResult.
- The existing code at lines 605-617 already does this, but we need to ensure it happens before line 648-650 where we read `m_samplesRecorded`.
- Move the device-stopping code earlier in the function.

### 3. Fix `m_processingActive` premature setting
**Issue**: `m_processingActive = false` is set at line 661 before the callback at line 673.
**Fix**: Move `m_processingActive = false` to the very end of the function, after all work including callbacks.

**Implementation**:
- Move line 661 to after the callback (after line 675).
- Update all early return paths to still set `m_processingActive = false`.

### 4. Add memory barriers for proper synchronization
**Issue**: No explicit memory ordering between threads.
**Fix**: Since we're using std::mutex, the mutex operations provide the necessary memory barriers. No additional changes needed.

## Detailed Code Changes

### Change 1: `finalizeRecording()` - Protect `m_outputPath`

```cpp
void AudioRecorder::finalizeRecording() {
  // Check for early cancellation before doing any work
  if (m_cancelRequested) {
    m_processingActive = false;
    return;
  }

  setState(RecordingState::Processing);

  // Check again after state change
  if (m_cancelRequested) {
    m_processingActive = false;
    return;
  }

  // CHANGE 1a: Make a local copy of output path under mutex protection
  std::string outputPath;
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);
    outputPath = m_outputPath;

    // Check cancellation after acquiring lock
    if (m_cancelRequested) {
      m_processingActive = false;
      return;
    }
  }

  // Stop capture if not metering (protected by mutex)
  {
    std::lock_guard<std::mutex> lock(m_resourceMutex);

    // Check cancellation after acquiring lock
    if (m_cancelRequested) {
      m_processingActive = false;
      return;
    }

    if (m_captureDevice && !m_meteringActive) {
      ma_device_stop(m_captureDevice.get());
    }
  }

  // Finalize encoder (protected by mutex)
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);

    // Check cancellation after acquiring lock
    if (m_cancelRequested) {
      m_processingActive = false;
      return;
    }

    if (m_encoder) {
      ma_encoder_uninit(m_encoder.get());
      m_encoder.reset();
    }
  }

  // Check cancellation before post-processing
  if (m_cancelRequested) {
    m_processingActive = false;
    return;
  }

  // Post-processing (if enabled) - use local copy of outputPath
  bool wasTrimmed = false;
  bool wasNormalized = false;
  processRecording(outputPath, wasTrimmed, wasNormalized);

  // Prepare result - use local copy of outputPath
  RecordingResult result;
  result.filePath = outputPath;
  result.duration = getRecordingDuration();
  result.sampleRate = m_format.sampleRate;
  result.channels = m_format.channels;
  result.trimmed = wasTrimmed;
  result.normalized = wasNormalized;

  if (fs::exists(outputPath)) {
    result.fileSize = fs::file_size(outputPath);
  }

  // CHANGE 1b: Clear m_outputPath and m_samplesRecorded under mutex
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);
    m_outputPath.clear();
    m_samplesRecorded = 0;
  }

  setState(RecordingState::Idle);

  // Fire callback only if not cancelled (copy callback while holding lock)
  if (!m_cancelRequested) {
    OnRecordingComplete callback;
    {
      std::lock_guard<std::mutex> lock(m_callbackMutex);
      callback = m_onRecordingComplete;
    }
    if (callback) {
      callback(result);
    }
  }

  // CHANGE 3: Move m_processingActive = false to the very end
  m_processingActive = false;
}
```

### Change 2: `processRecording()` - Accept outputPath as parameter

```cpp
void AudioRecorder::processRecording(const std::string& outputPath,
                                     bool& outTrimmed,
                                     bool& outNormalized) {
  // Post-processing: silence trimming and normalization
  // These operations modify the recorded WAV file in-place

  outTrimmed = false;
  outNormalized = false;

  if (!fs::exists(outputPath)) {
    return;
  }

  // ... rest of function uses outputPath parameter instead of m_outputPath
}
```

### Change 3: `startRecording()` - Protect `m_outputPath` write

```cpp
Result<void> AudioRecorder::startRecording(const std::string& outputPath) {
  // ... existing validation code ...

  setState(RecordingState::Preparing);

  // CHANGE: Protect m_outputPath write with mutex
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);
    m_outputPath = outputPath;
    m_recordBuffer.clear();
    m_samplesRecorded = 0;
  }

  // Initialize encoder
  // Note: encoder initialization uses outputPath parameter, not m_outputPath
  m_encoder = std::make_unique<ma_encoder>();
  // ... rest of function ...
}
```

### Change 4: `cancelRecording()` - Protect `m_outputPath` access

```cpp
void AudioRecorder::cancelRecording() {
  if (m_state == RecordingState::Idle) {
    return;
  }

  // Signal cancellation to any running background thread
  m_cancelRequested = true;

  setState(RecordingState::Canceling);

  // Wait for background thread to finish (it will check m_cancelRequested and exit early)
  joinProcessingThread();

  // Now safe to cleanup resources - background thread has completed
  cleanupResources();

  // CHANGE: Protect m_outputPath access with mutex
  std::string fileToDelete;
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);
    fileToDelete = m_outputPath;
    m_outputPath.clear();
    m_samplesRecorded = 0;
  }

  // Delete incomplete file (outside mutex to avoid holding lock during I/O)
  if (!fileToDelete.empty() && fs::exists(fileToDelete)) {
    try {
      fs::remove(fileToDelete);
    } catch (const fs::filesystem_error& e) {
      // ... error handling ...
    } catch (const std::exception& e) {
      // ... error handling ...
    }
  }

  m_recordBuffer.clear();

  setState(RecordingState::Idle);
}
```

### Change 5: Update header file - Change `processRecording()` signature

```cpp
// In audio_recorder.hpp, line 375:
void processRecording(const std::string& outputPath,
                      bool& outTrimmed,
                      bool& outNormalized);
```

## Testing Strategy

1. **Unit Tests**: Add tests to `test_audio_recorder.cpp`:
   - `test_audio_recorder_thread_safety`: Verify concurrent stop/cancel operations
   - `test_audio_recorder_start_stop_rapid`: Rapid start/stop cycles

2. **Thread Sanitizer**: Run tests with TSan enabled:
   ```bash
   cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
   make
   ctest
   ```

3. **Stress Test**: Create a test that spawns multiple threads calling start/stop/cancel simultaneously for 10+ seconds.

## Expected Results

1. No data races reported by TSan
2. No crashes or data corruption under concurrent access
3. All existing tests continue to pass
4. New thread safety tests pass consistently
