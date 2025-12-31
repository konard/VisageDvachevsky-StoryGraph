/**
 * @file audio_recorder.cpp
 * @brief Audio Recorder implementation using miniaudio
 */

#include "NovelMind/audio/audio_recorder.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

// miniaudio implementation is in miniaudio_impl.cpp
#include "miniaudio/miniaudio.h"

namespace NovelMind::audio {

namespace fs = std::filesystem;

// ============================================================================
// AudioRecorder Implementation
// ============================================================================

AudioRecorder::AudioRecorder() = default;

AudioRecorder::~AudioRecorder() { shutdown(); }

// ============================================================================
// Initialization
// ============================================================================

Result<void> AudioRecorder::initialize() {
  if (m_initialized) {
    return {};
  }

  m_context = std::make_unique<ma_context>();
  ma_context_config config = ma_context_config_init();

  if (ma_context_init(nullptr, 0, &config, m_context.get()) != MA_SUCCESS) {
    m_context.reset();
    return Result<void>::error("Failed to initialize audio context");
  }

  refreshDevices();
  m_initialized = true;

  return {};
}

void AudioRecorder::shutdown() {
  if (!m_initialized) {
    return;
  }

  // Signal cancellation to any running background thread
  m_cancelRequested = true;

  // Stop any active recording (this will join the processing thread)
  if (m_state != RecordingState::Idle) {
    cancelRecording();
  }

  // Stop metering
  stopMetering();

  // Ensure processing thread is stopped
  m_processingActive = false;
  joinProcessingThread();

  // Cleanup devices (protected by mutex)
  {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    if (m_captureDevice) {
      ma_device_uninit(m_captureDevice.get());
      m_captureDevice.reset();
    }

    if (m_playbackDevice) {
      ma_device_uninit(m_playbackDevice.get());
      m_playbackDevice.reset();
    }
  }

  // Cleanup context
  if (m_context) {
    ma_context_uninit(m_context.get());
    m_context.reset();
  }

  m_initialized = false;
}

// ============================================================================
// Device Management
// ============================================================================

std::vector<AudioDeviceInfo> AudioRecorder::getInputDevices() const {
  return m_inputDevices;
}

std::vector<AudioDeviceInfo> AudioRecorder::getOutputDevices() const {
  return m_outputDevices;
}

const AudioDeviceInfo *AudioRecorder::getCurrentInputDevice() const {
  for (const auto &device : m_inputDevices) {
    if (device.id == m_currentInputDeviceId ||
        (m_currentInputDeviceId.empty() && device.isDefault)) {
      return &device;
    }
  }
  return m_inputDevices.empty() ? nullptr : &m_inputDevices[0];
}

const AudioDeviceInfo *AudioRecorder::getCurrentOutputDevice() const {
  for (const auto &device : m_outputDevices) {
    if (device.id == m_currentOutputDeviceId ||
        (m_currentOutputDeviceId.empty() && device.isDefault)) {
      return &device;
    }
  }
  return m_outputDevices.empty() ? nullptr : &m_outputDevices[0];
}

Result<void> AudioRecorder::setInputDevice(const std::string &deviceId) {
  // Validate device exists
  bool found = deviceId.empty();
  for (const auto &device : m_inputDevices) {
    if (device.id == deviceId) {
      found = true;
      break;
    }
  }

  if (!found) {
    return Result<void>::error("Input device not found: " + deviceId);
  }

  // If recording, stop first
  bool wasRecording = isRecording();
  bool wasMetering = isMeteringActive();

  if (wasRecording) {
    stopRecording();
  }
  if (wasMetering) {
    stopMetering();
  }

  m_currentInputDeviceId = deviceId;

  // Restart metering if it was active
  if (wasMetering) {
    return startMetering();
  }

  return {};
}

Result<void> AudioRecorder::setOutputDevice(const std::string &deviceId) {
  bool found = deviceId.empty();
  for (const auto &device : m_outputDevices) {
    if (device.id == deviceId) {
      found = true;
      break;
    }
  }

  if (!found) {
    return Result<void>::error("Output device not found: " + deviceId);
  }

  m_currentOutputDeviceId = deviceId;
  return {};
}

void AudioRecorder::refreshDevices() {
  if (!m_context) {
    return;
  }

  m_inputDevices.clear();
  m_outputDevices.clear();

  ma_device_info *captureInfos = nullptr;
  ma_uint32 captureCount = 0;
  ma_device_info *playbackInfos = nullptr;
  ma_uint32 playbackCount = 0;

  if (ma_context_get_devices(m_context.get(), &playbackInfos, &playbackCount,
                             &captureInfos, &captureCount) != MA_SUCCESS) {
    return;
  }

  // Enumerate capture devices
  for (ma_uint32 i = 0; i < captureCount; ++i) {
    AudioDeviceInfo info;
    // Use device name as ID since the custom field is a union
    info.id = std::string(captureInfos[i].name);
    info.name = captureInfos[i].name;
    info.isDefault = captureInfos[i].isDefault != 0;
    info.maxInputChannels = 2; // Assume stereo support

    // Common sample rates
    info.supportedSampleRates = {44100, 48000, 96000};

    m_inputDevices.push_back(info);
  }

  // Enumerate playback devices
  for (ma_uint32 i = 0; i < playbackCount; ++i) {
    AudioDeviceInfo info;
    // Use device name as ID since the custom field is a union
    info.id = std::string(playbackInfos[i].name);
    info.name = playbackInfos[i].name;
    info.isDefault = playbackInfos[i].isDefault != 0;
    info.maxOutputChannels = 2;
    info.supportedSampleRates = {44100, 48000, 96000};

    m_outputDevices.push_back(info);
  }
}

// ============================================================================
// Recording Format
// ============================================================================

void AudioRecorder::setRecordingFormat(const RecordingFormat &format) {
  m_format = format;
}

// ============================================================================
// Monitoring
// ============================================================================

void AudioRecorder::setMonitoringEnabled(bool enabled) {
  m_monitoringEnabled = enabled;
}

void AudioRecorder::setMonitoringVolume(f32 volume) {
  m_monitoringVolume = std::clamp(volume, 0.0f, 1.0f);
}

Result<void> AudioRecorder::startMetering() {
  if (!m_initialized) {
    return Result<void>::error("Recorder not initialized");
  }

  if (m_meteringActive) {
    return {};
  }

  // Initialize capture device if not already
  if (!m_captureDevice) {
    m_captureDevice = std::make_unique<ma_device>();

    ma_device_config deviceConfig =
        ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = m_format.channels;
    deviceConfig.sampleRate = m_format.sampleRate;
    deviceConfig.dataCallback = &AudioRecorder::audioCallback;
    deviceConfig.pUserData = this;

    // Set device ID if specified - find matching device
    if (!m_currentInputDeviceId.empty()) {
      // Find the device in our list and get its index for miniaudio
      for (ma_uint32 i = 0; i < static_cast<ma_uint32>(m_inputDevices.size());
           ++i) {
        if (m_inputDevices[i].id == m_currentInputDeviceId) {
          // Get device info array from context
          ma_device_info *captureInfos = nullptr;
          ma_uint32 captureCount = 0;
          if (ma_context_get_devices(m_context.get(), nullptr, nullptr,
                                     &captureInfos,
                                     &captureCount) == MA_SUCCESS) {
            if (i < captureCount) {
              deviceConfig.capture.pDeviceID = &captureInfos[i].id;
            }
          }
          break;
        }
      }
    }

    if (ma_device_init(m_context.get(), &deviceConfig, m_captureDevice.get()) !=
        MA_SUCCESS) {
      m_captureDevice.reset();
      return Result<void>::error("Failed to initialize capture device");
    }
  }

  if (ma_device_start(m_captureDevice.get()) != MA_SUCCESS) {
    return Result<void>::error("Failed to start capture device");
  }

  m_meteringActive = true;
  return {};
}

void AudioRecorder::stopMetering() {
  if (!m_meteringActive || !m_captureDevice) {
    return;
  }

  // Only stop if not recording
  if (!isRecording()) {
    ma_device_stop(m_captureDevice.get());
  }

  m_meteringActive = false;
}

LevelMeter AudioRecorder::getCurrentLevel() const {
  std::lock_guard<std::mutex> lock(m_levelMutex);
  return m_currentLevel;
}

// ============================================================================
// Recording
// ============================================================================

Result<void> AudioRecorder::startRecording(const std::string &outputPath) {
  if (!m_initialized) {
    return Result<void>::error("Recorder not initialized");
  }

  if (isRecording()) {
    return Result<void>::error("Already recording");
  }

  // Create output directory if needed
  fs::path outPath(outputPath);
  if (outPath.has_parent_path()) {
    try {
      fs::create_directories(outPath.parent_path());
    } catch (const fs::filesystem_error &e) {
      return Result<void>::error("Failed to create output directory: " +
                                 std::string(e.what()));
    }
  }

  setState(RecordingState::Preparing);

  m_outputPath = outputPath;
  m_recordBuffer.clear();
  m_samplesRecorded = 0;

  // Initialize encoder
  m_encoder = std::make_unique<ma_encoder>();

  ma_encoder_config encoderConfig =
      ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32,
                             m_format.channels, m_format.sampleRate);

  if (ma_encoder_init_file(outputPath.c_str(), &encoderConfig,
                           m_encoder.get()) != MA_SUCCESS) {
    m_encoder.reset();
    setState(RecordingState::Error);
    if (m_onRecordingError) {
      m_onRecordingError("Failed to initialize encoder");
    }
    return Result<void>::error("Failed to initialize encoder");
  }

  // Start capture device if not already running
  if (!m_captureDevice) {
    auto result = startMetering();
    if (result.isError()) {
      ma_encoder_uninit(m_encoder.get());
      m_encoder.reset();
      setState(RecordingState::Error);
      return result;
    }
  } else if (!m_meteringActive) {
    if (ma_device_start(m_captureDevice.get()) != MA_SUCCESS) {
      ma_encoder_uninit(m_encoder.get());
      m_encoder.reset();
      setState(RecordingState::Error);
      return Result<void>::error("Failed to start capture device");
    }
  }

  setState(RecordingState::Recording);
  return {};
}

Result<void> AudioRecorder::stopRecording() {
  if (!isRecording()) {
    return Result<void>::error("Not recording");
  }

  setState(RecordingState::Stopping);

  // Reset cancel flag before starting finalization
  m_cancelRequested = false;

  // Wait for any previous processing thread to complete
  joinProcessingThread();

  // Start finalization in background thread
  m_processingActive = true;
  m_processingThread = std::thread(&AudioRecorder::finalizeRecording, this);

  return {};
}

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

  // Delete incomplete file
  if (!m_outputPath.empty() && fs::exists(m_outputPath)) {
    try {
      fs::remove(m_outputPath);
    } catch (...) {
    }
  }

  m_recordBuffer.clear();
  m_samplesRecorded = 0;
  m_outputPath.clear();

  setState(RecordingState::Idle);
}

f32 AudioRecorder::getRecordingDuration() const {
  if (m_format.sampleRate == 0) {
    return 0.0f;
  }
  return static_cast<f32>(m_samplesRecorded) /
         static_cast<f32>(m_format.sampleRate * m_format.channels);
}

// ============================================================================
// Callbacks
// ============================================================================

void AudioRecorder::setOnLevelUpdate(OnLevelUpdate callback) {
  m_onLevelUpdate = std::move(callback);
}

void AudioRecorder::setOnRecordingStateChanged(
    OnRecordingStateChanged callback) {
  m_onStateChanged = std::move(callback);
}

void AudioRecorder::setOnRecordingComplete(OnRecordingComplete callback) {
  m_onRecordingComplete = std::move(callback);
}

void AudioRecorder::setOnRecordingError(OnRecordingError callback) {
  m_onRecordingError = std::move(callback);
}

// ============================================================================
// Utility
// ============================================================================

f32 AudioRecorder::linearToDb(f32 linear) {
  if (linear <= 0.0f) {
    return -100.0f;
  }
  return 20.0f * std::log10(linear);
}

f32 AudioRecorder::dbToLinear(f32 db) { return std::pow(10.0f, db / 20.0f); }

// ============================================================================
// Internal Methods
// ============================================================================

void AudioRecorder::audioCallback(ma_device *device, void *output,
                                  const void *input, u32 frameCount) {
  (void)output;

  auto *recorder = static_cast<AudioRecorder *>(device->pUserData);
  if (!recorder) {
    return;
  }

  recorder->processAudioData(input, frameCount);
}

void AudioRecorder::processAudioData(const void *input, u32 frameCount) {
  if (!input || frameCount == 0) {
    return;
  }

  const auto *samples = static_cast<const f32 *>(input);
  u32 sampleCount = frameCount * m_format.channels;

  // Update level meter
  updateLevelMeter(samples, sampleCount);

  // Write to file if recording
  if (m_state == RecordingState::Recording) {
    writeToFile(samples, sampleCount);
  }
}

void AudioRecorder::updateLevelMeter(const f32 *samples, u32 sampleCount) {
  f32 peak = 0.0f;
  f32 sumSquares = 0.0f;

  for (u32 i = 0; i < sampleCount; ++i) {
    f32 sample = std::abs(samples[i]);
    peak = std::max(peak, sample);
    sumSquares += samples[i] * samples[i];
  }

  f32 rms = std::sqrt(sumSquares / static_cast<f32>(sampleCount));

  {
    std::lock_guard<std::mutex> lock(m_levelMutex);

    // Apply decay to peak (for smoother display)
    m_currentLevel.peakLevel =
        std::max(peak, m_currentLevel.peakLevel * LEVEL_DECAY_RATE);
    m_currentLevel.rmsLevel = rms;
    m_currentLevel.peakLevelDb = linearToDb(m_currentLevel.peakLevel);
    m_currentLevel.rmsLevelDb = linearToDb(m_currentLevel.rmsLevel);
    m_currentLevel.clipping = peak >= 1.0f;
  }

  // Fire callback on level update
  if (m_onLevelUpdate) {
    m_onLevelUpdate(m_currentLevel);
  }
}

void AudioRecorder::writeToFile(const f32 *samples, u32 sampleCount) {
  std::lock_guard<std::mutex> lock(m_recordMutex);

  if (!m_encoder) {
    return;
  }

  ma_encoder_write_pcm_frames(m_encoder.get(), samples,
                              sampleCount / m_format.channels, nullptr);
  m_samplesRecorded += sampleCount;
}

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

  // Post-processing (if enabled)
  processRecording();

  // Prepare result
  RecordingResult result;
  result.filePath = m_outputPath;
  result.duration = getRecordingDuration();
  result.sampleRate = m_format.sampleRate;
  result.channels = m_format.channels;
  result.trimmed = false;
  result.normalized = false;

  if (fs::exists(m_outputPath)) {
    result.fileSize = fs::file_size(m_outputPath);
  }

  m_outputPath.clear();
  m_samplesRecorded = 0;
  m_processingActive = false;

  setState(RecordingState::Idle);

  // Fire callback only if not cancelled
  if (!m_cancelRequested && m_onRecordingComplete) {
    m_onRecordingComplete(result);
  }
}

void AudioRecorder::processRecording() {
  // Post-processing would go here:
  // - Silence trimming
  // - Normalization
  // - Format conversion (e.g., WAV to OGG)
  //
  // For now, we just record directly to the target format.
  // Full implementation would use a separate audio processing library.

  if (m_format.autoTrimSilence) {
    // TODO: Implement silence trimming
  }

  if (m_format.normalize) {
    // TODO: Implement normalization
  }
}

void AudioRecorder::setState(RecordingState state) {
  m_state = state;
  if (m_onStateChanged) {
    m_onStateChanged(state);
  }
}

void AudioRecorder::joinProcessingThread() {
  if (m_processingThread.joinable()) {
    m_processingThread.join();
  }
}

void AudioRecorder::cleanupResources() {
  std::lock_guard<std::mutex> lock(m_resourceMutex);

  // Stop capture device if not metering
  if (m_captureDevice && !m_meteringActive) {
    ma_device_stop(m_captureDevice.get());
  }

  // Cleanup encoder
  {
    std::lock_guard<std::mutex> recordLock(m_recordMutex);
    if (m_encoder) {
      ma_encoder_uninit(m_encoder.get());
      m_encoder.reset();
    }
  }
}

} // namespace NovelMind::audio
