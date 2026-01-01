#pragma once

/**
 * @file audio_recorder.hpp
 * @brief Audio Recording System - Microphone capture for voice authoring
 *
 * Provides comprehensive audio recording capabilities:
 * - Device enumeration and selection
 * - Real-time level metering (VU meter)
 * - Audio monitoring (live playback through speakers)
 * - Recording to file with configurable format
 * - Automatic silence trimming
 * - Non-blocking recording with worker thread
 *
 * Uses miniaudio backend for cross-platform audio capture.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declarations for miniaudio types
struct ma_device;
struct ma_encoder;
struct ma_context;

namespace NovelMind::audio {

/**
 * @brief Audio device information
 */
struct AudioDeviceInfo {
  std::string id;            // Unique device ID
  std::string name;          // Human-readable device name
  bool isDefault = false;    // Is this the system default device
  u32 maxInputChannels = 0;  // Maximum supported input channels
  u32 maxOutputChannels = 0; // Maximum supported output channels
  std::vector<u32> supportedSampleRates;
};

/**
 * @brief Recording format configuration
 */
struct RecordingFormat {
  u32 sampleRate = 48000; // Sample rate in Hz
  u8 channels = 1;        // Number of channels (1 = mono, 2 = stereo)
  u8 bitsPerSample = 16;  // Bits per sample (16, 24, 32)

  // Output format
  enum class FileFormat : u8 {
    WAV,  // Uncompressed WAV
    FLAC, // Lossless compression
    OGG   // Lossy Vorbis compression (requires conversion)
  };
  FileFormat outputFormat = FileFormat::WAV;

  // Processing options
  bool autoTrimSilence = false;  // Remove silence from start/end
  f32 silenceThreshold = -40.0f; // Silence threshold in dB
  f32 silenceMinDuration = 0.1f; // Minimum silence duration to trim (seconds)

  bool normalize = false;      // Normalize audio level
  f32 normalizeTarget = -1.0f; // Target level in dB (negative)
};

/**
 * @brief Recording state
 */
enum class RecordingState : u8 {
  Idle,       // Not recording
  Preparing,  // Setting up recording
  Recording,  // Actively recording
  Stopping,   // Stopping recording
  Canceling,  // Canceling recording
  Processing, // Post-processing (trimming, normalizing)
  Error       // Error state
};

/**
 * @brief Level meter reading
 */
struct LevelMeter {
  f32 peakLevel = 0.0f;     // Peak level (0.0 to 1.0)
  f32 rmsLevel = 0.0f;      // RMS level (0.0 to 1.0)
  f32 peakLevelDb = -60.0f; // Peak level in dB
  f32 rmsLevelDb = -60.0f;  // RMS level in dB
  bool clipping = false;    // True if signal is clipping
};

/**
 * @brief Recording result
 */
struct RecordingResult {
  std::string filePath;    // Path to recorded file
  f32 duration = 0.0f;     // Duration in seconds
  u32 sampleRate = 0;      // Sample rate
  u8 channels = 0;         // Number of channels
  u64 fileSize = 0;        // File size in bytes
  bool trimmed = false;    // Was silence trimmed
  bool normalized = false; // Was audio normalized
};

/**
 * @brief Callback types
 *
 * IMPORTANT: These callbacks are invoked from the audio thread, NOT the main/UI
 * thread. If you need to update UI elements, you MUST use thread-safe
 * mechanisms:
 * - Qt: Use QMetaObject::invokeMethod(..., Qt::QueuedConnection)
 * - Other frameworks: Post to main thread event queue
 *
 * Example (Qt):
 * @code
 * recorder.setOnLevelUpdate([this](const LevelMeter& level) {
 *     QMetaObject::invokeMethod(m_progressBar, [=] {
 *         m_progressBar->setValue(level.rmsLevelDb);
 *     }, Qt::QueuedConnection);
 * });
 * @endcode
 */
using OnLevelUpdate = std::function<void(const LevelMeter &level)>;
using OnRecordingStateChanged = std::function<void(RecordingState state)>;
using OnRecordingComplete = std::function<void(const RecordingResult &result)>;
using OnRecordingError = std::function<void(const std::string &error)>;

/**
 * @brief Audio Recorder - Microphone capture for voice authoring
 *
 * The Audio Recorder provides a complete solution for recording voice lines
 * directly in the editor, including:
 *
 * 1. Device enumeration and selection
 * 2. Real-time level monitoring
 * 3. Recording with auto-naming
 * 4. Post-processing (silence trim, normalize)
 *
 * Example usage:
 * @code
 * AudioRecorder recorder;
 * recorder.initialize();
 *
 * // List available devices
 * auto devices = recorder.getInputDevices();
 * recorder.setInputDevice(devices[0].id);
 *
 * // Set up level monitoring
 * recorder.setOnLevelUpdate([](const LevelMeter& level) {
 *     updateVUMeter(level.rmsLevelDb);
 * });
 *
 * // Start recording
 * recorder.startRecording("output/voice_line_001.wav");
 *
 * // ... user speaks ...
 *
 * // Stop and save
 * recorder.stopRecording();
 * @endcode
 */
class AudioRecorder {
public:
  AudioRecorder();
  ~AudioRecorder();

  // Non-copyable
  AudioRecorder(const AudioRecorder &) = delete;
  AudioRecorder &operator=(const AudioRecorder &) = delete;

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize the audio recorder
   * @return Success or error
   */
  Result<void> initialize();

  /**
   * @brief Shutdown the recorder
   */
  void shutdown();

  /**
   * @brief Check if recorder is initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // =========================================================================
  // Device Management
  // =========================================================================

  /**
   * @brief Get list of available input devices
   */
  [[nodiscard]] std::vector<AudioDeviceInfo> getInputDevices() const;

  /**
   * @brief Get list of available output devices (for monitoring)
   */
  [[nodiscard]] std::vector<AudioDeviceInfo> getOutputDevices() const;

  /**
   * @brief Get current input device
   */
  [[nodiscard]] const AudioDeviceInfo *getCurrentInputDevice() const;

  /**
   * @brief Get current output device (for monitoring)
   */
  [[nodiscard]] const AudioDeviceInfo *getCurrentOutputDevice() const;

  /**
   * @brief Set input device by ID
   * @param deviceId Device ID (empty for default)
   * @return Success or error
   */
  Result<void> setInputDevice(const std::string &deviceId);

  /**
   * @brief Set output device for monitoring
   * @param deviceId Device ID (empty for default)
   * @return Success or error
   */
  Result<void> setOutputDevice(const std::string &deviceId);

  /**
   * @brief Refresh device list
   */
  void refreshDevices();

  // =========================================================================
  // Recording Format
  // =========================================================================

  /**
   * @brief Set recording format
   */
  void setRecordingFormat(const RecordingFormat &format);

  /**
   * @brief Get current recording format
   */
  [[nodiscard]] const RecordingFormat &getRecordingFormat() const {
    return m_format;
  }

  // =========================================================================
  // Monitoring
  // =========================================================================

  /**
   * @brief Enable/disable input monitoring (live playback)
   * @param enabled True to enable monitoring
   */
  void setMonitoringEnabled(bool enabled);

  /**
   * @brief Check if monitoring is enabled
   */
  [[nodiscard]] bool isMonitoringEnabled() const { return m_monitoringEnabled; }

  /**
   * @brief Set monitoring volume
   * @param volume Volume level (0.0 to 1.0)
   */
  void setMonitoringVolume(f32 volume);

  /**
   * @brief Get monitoring volume
   */
  [[nodiscard]] f32 getMonitoringVolume() const { return m_monitoringVolume; }

  /**
   * @brief Start level metering without recording
   * Useful for checking input levels before recording
   */
  Result<void> startMetering();

  /**
   * @brief Stop level metering
   */
  void stopMetering();

  /**
   * @brief Check if metering is active
   */
  [[nodiscard]] bool isMeteringActive() const { return m_meteringActive; }

  /**
   * @brief Get current level reading
   */
  [[nodiscard]] LevelMeter getCurrentLevel() const;

  // =========================================================================
  // Recording
  // =========================================================================

  /**
   * @brief Start recording to file
   * @param outputPath Path to output file
   * @return Success or error
   */
  Result<void> startRecording(const std::string &outputPath);

  /**
   * @brief Stop recording
   * Recording will be finalized asynchronously
   * @return Success or error
   */
  Result<void> stopRecording();

  /**
   * @brief Cancel recording (discard data)
   */
  void cancelRecording();

  /**
   * @brief Get current recording state
   */
  [[nodiscard]] RecordingState getState() const { return m_state; }

  /**
   * @brief Check if currently recording
   */
  [[nodiscard]] bool isRecording() const {
    return m_state == RecordingState::Recording;
  }

  /**
   * @brief Get current recording duration
   */
  [[nodiscard]] f32 getRecordingDuration() const;

  /**
   * @brief Get current recording path
   */
  [[nodiscard]] const std::string &getRecordingPath() const {
    return m_outputPath;
  }

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnLevelUpdate(OnLevelUpdate callback);
  void setOnRecordingStateChanged(OnRecordingStateChanged callback);
  void setOnRecordingComplete(OnRecordingComplete callback);
  void setOnRecordingError(OnRecordingError callback);

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Convert linear level to dB
   */
  static f32 linearToDb(f32 linear);

  /**
   * @brief Convert dB to linear level
   */
  static f32 dbToLinear(f32 db);

private:
  // Miniaudio callback (called from audio thread)
  static void audioCallback(ma_device *device, void *output, const void *input,
                            u32 frameCount);

  // Internal methods
  void processAudioData(const void *input, u32 frameCount);
  void updateLevelMeter(const f32 *samples, u32 sampleCount);
  void writeToFile(const f32 *samples, u32 sampleCount);
  void finalizeRecording();
  void processRecording();
  void setState(RecordingState state);

  // Initialization state
  bool m_initialized = false;
  std::unique_ptr<ma_context> m_context;

  // Devices
  std::vector<AudioDeviceInfo> m_inputDevices;
  std::vector<AudioDeviceInfo> m_outputDevices;
  std::string m_currentInputDeviceId;
  std::string m_currentOutputDeviceId;

  // Recording format
  RecordingFormat m_format;

  // Device handles
  std::unique_ptr<ma_device> m_captureDevice;
  std::unique_ptr<ma_device> m_playbackDevice;

  // Monitoring
  std::atomic<bool> m_monitoringEnabled{false};
  std::atomic<f32> m_monitoringVolume{1.0f};

  // Level metering
  std::atomic<bool> m_meteringActive{false};
  mutable std::mutex m_levelMutex;
  LevelMeter m_currentLevel;
  static constexpr f32 LEVEL_DECAY_RATE = 0.95f;

  // Recording state
  std::atomic<RecordingState> m_state{RecordingState::Idle};
  std::string m_outputPath;
  std::unique_ptr<ma_encoder> m_encoder;
  std::mutex m_recordMutex;

  // Recording buffer
  std::vector<f32> m_recordBuffer;
  std::atomic<u64> m_samplesRecorded{0};

  // Callbacks (protected by m_callbackMutex for thread-safe access from audio thread)
  mutable std::mutex m_callbackMutex;
  OnLevelUpdate m_onLevelUpdate;
  OnRecordingStateChanged m_onStateChanged;
  OnRecordingComplete m_onRecordingComplete;
  OnRecordingError m_onRecordingError;

  // Processing thread
  std::thread m_processingThread;
  std::atomic<bool> m_processingActive{false};

  // Thread safety for concurrent stop/cancel operations
  // Lock ordering to prevent deadlocks (always acquire in this order):
  // 1. m_callbackMutex (if needed)
  // 2. m_resourceMutex (if needed)
  // 3. m_recordMutex (if needed)
  // 4. m_levelMutex (if needed)
  mutable std::mutex m_resourceMutex; // Protects m_encoder and m_captureDevice
                                      // during stop/cancel
  std::atomic<bool> m_cancelRequested{
      false}; // Signals background thread to abort

  // Helper methods for thread-safe operations
  void joinProcessingThread();
  void cleanupResources();
};

} // namespace NovelMind::audio
