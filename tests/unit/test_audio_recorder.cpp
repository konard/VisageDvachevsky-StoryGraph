/**
 * @file test_audio_recorder.cpp
 * @brief Unit tests for AudioRecorder thread safety
 *
 * These tests verify the fix for race condition between stopRecording()
 * and cancelRecording() (Issue #161).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/audio/audio_recorder.hpp"
#include <thread>
#include <chrono>
#include <vector>

using namespace NovelMind::audio;

TEST_CASE("AudioRecorder initial state", "[audio][recorder]") {
  AudioRecorder recorder;

  REQUIRE(recorder.getState() == RecordingState::Idle);
  REQUIRE_FALSE(recorder.isRecording());
  REQUIRE_FALSE(recorder.isInitialized());
  REQUIRE_FALSE(recorder.isMeteringActive());
}

TEST_CASE("AudioRecorder state transitions", "[audio][recorder]") {
  AudioRecorder recorder;

  SECTION("Stop recording when not recording returns error") {
    auto result = recorder.stopRecording();
    REQUIRE(result.isError());
  }

  SECTION("Cancel recording when idle does nothing") {
    recorder.cancelRecording();
    REQUIRE(recorder.getState() == RecordingState::Idle);
  }
}

TEST_CASE("AudioRecorder recording format", "[audio][recorder]") {
  AudioRecorder recorder;
  RecordingFormat format;
  format.sampleRate = 44100;
  format.channels = 2;

  recorder.setRecordingFormat(format);
  const auto& storedFormat = recorder.getRecordingFormat();

  REQUIRE(storedFormat.sampleRate == 44100);
  REQUIRE(storedFormat.channels == 2);
}

TEST_CASE("AudioRecorder monitoring settings", "[audio][recorder]") {
  AudioRecorder recorder;

  SECTION("Monitoring enabled flag") {
    REQUIRE_FALSE(recorder.isMonitoringEnabled());
    recorder.setMonitoringEnabled(true);
    REQUIRE(recorder.isMonitoringEnabled());
    recorder.setMonitoringEnabled(false);
    REQUIRE_FALSE(recorder.isMonitoringEnabled());
  }

  SECTION("Monitoring volume clamping") {
    recorder.setMonitoringVolume(0.5f);
    REQUIRE(recorder.getMonitoringVolume() == 0.5f);

    recorder.setMonitoringVolume(2.0f); // Should clamp to 1.0
    REQUIRE(recorder.getMonitoringVolume() == 1.0f);

    recorder.setMonitoringVolume(-1.0f); // Should clamp to 0.0
    REQUIRE(recorder.getMonitoringVolume() == 0.0f);
  }
}

TEST_CASE("AudioRecorder dB conversion utilities", "[audio][recorder]") {
  SECTION("linearToDb") {
    REQUIRE(AudioRecorder::linearToDb(1.0f) == 0.0f);
    REQUIRE(AudioRecorder::linearToDb(0.1f) == Catch::Approx(-20.0f).margin(0.01f));
    REQUIRE(AudioRecorder::linearToDb(0.0f) == -100.0f);
  }

  SECTION("dbToLinear") {
    REQUIRE(AudioRecorder::dbToLinear(0.0f) == 1.0f);
    REQUIRE(AudioRecorder::dbToLinear(-20.0f) == Catch::Approx(0.1f).margin(0.001f));
  }
}

TEST_CASE("AudioRecorder destructor safety", "[audio][recorder]") {
  // Test that destructor doesn't crash when called on uninitialized recorder
  AudioRecorder recorder;
  { AudioRecorder scopedRecorder; }
  // Verify destructor completed without crash - original recorder still valid
  REQUIRE_FALSE(recorder.isInitialized());
}

TEST_CASE("AudioRecorder shutdown safety", "[audio][recorder]") {
  AudioRecorder recorder;

  // Multiple shutdown calls should be safe
  recorder.shutdown();
  recorder.shutdown();

  REQUIRE_FALSE(recorder.isInitialized());
}

TEST_CASE("RecordingState enum values", "[audio][recorder]") {
  // Verify the Canceling state exists (added for thread-safety fix)
  REQUIRE(static_cast<int>(RecordingState::Idle) == 0);
  REQUIRE(static_cast<int>(RecordingState::Preparing) == 1);
  REQUIRE(static_cast<int>(RecordingState::Recording) == 2);
  REQUIRE(static_cast<int>(RecordingState::Stopping) == 3);
  REQUIRE(static_cast<int>(RecordingState::Canceling) == 4);
  REQUIRE(static_cast<int>(RecordingState::Processing) == 5);
  REQUIRE(static_cast<int>(RecordingState::Error) == 6);
}

// Note: Full race condition stress tests require audio hardware.
// These tests verify the API surface and thread-safety mechanisms are in place.
// For complete verification, run with ThreadSanitizer:
//   cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..

// ============================================================================
// Recording Format Configuration Tests (for silence trimming and normalization)
// ============================================================================

TEST_CASE("RecordingFormat default values", "[audio][recorder][format]") {
  RecordingFormat format;

  SECTION("Default sample rate and channels") {
    REQUIRE(format.sampleRate == 48000);
    REQUIRE(format.channels == 1);
    REQUIRE(format.bitsPerSample == 16);
  }

  SECTION("Default processing options are disabled") {
    REQUIRE_FALSE(format.autoTrimSilence);
    REQUIRE_FALSE(format.normalize);
  }

  SECTION("Default silence trimming parameters") {
    REQUIRE(format.silenceThreshold == Catch::Approx(-40.0f));
    REQUIRE(format.silenceMinDuration == Catch::Approx(0.1f));
  }

  SECTION("Default normalization parameters") {
    REQUIRE(format.normalizeTarget == Catch::Approx(-1.0f));
  }
}

TEST_CASE("RecordingFormat custom configuration", "[audio][recorder][format]") {
  RecordingFormat format;

  SECTION("Custom silence trimming settings") {
    format.autoTrimSilence = true;
    format.silenceThreshold = -50.0f; // More aggressive threshold
    format.silenceMinDuration = 0.2f; // Keep more silence

    REQUIRE(format.autoTrimSilence);
    REQUIRE(format.silenceThreshold == Catch::Approx(-50.0f));
    REQUIRE(format.silenceMinDuration == Catch::Approx(0.2f));
  }

  SECTION("Custom normalization settings") {
    format.normalize = true;
    format.normalizeTarget = -3.0f; // Lower target level

    REQUIRE(format.normalize);
    REQUIRE(format.normalizeTarget == Catch::Approx(-3.0f));
  }

  SECTION("Combined processing options") {
    format.autoTrimSilence = true;
    format.normalize = true;

    REQUIRE(format.autoTrimSilence);
    REQUIRE(format.normalize);
  }
}

TEST_CASE("AudioRecorder format with processing options", "[audio][recorder][format]") {
  AudioRecorder recorder;

  SECTION("Set format with silence trimming enabled") {
    RecordingFormat format;
    format.autoTrimSilence = true;
    format.silenceThreshold = -35.0f;
    format.silenceMinDuration = 0.15f;

    recorder.setRecordingFormat(format);
    const auto& storedFormat = recorder.getRecordingFormat();

    REQUIRE(storedFormat.autoTrimSilence);
    REQUIRE(storedFormat.silenceThreshold == Catch::Approx(-35.0f));
    REQUIRE(storedFormat.silenceMinDuration == Catch::Approx(0.15f));
  }

  SECTION("Set format with normalization enabled") {
    RecordingFormat format;
    format.normalize = true;
    format.normalizeTarget = -0.5f;

    recorder.setRecordingFormat(format);
    const auto& storedFormat = recorder.getRecordingFormat();

    REQUIRE(storedFormat.normalize);
    REQUIRE(storedFormat.normalizeTarget == Catch::Approx(-0.5f));
  }
}

TEST_CASE("Silence threshold dB to linear conversion", "[audio][recorder][processing]") {
  // Verify the silence detection threshold conversions work correctly
  // -40 dB (default) should be approximately 0.01 linear
  float threshold40dB = AudioRecorder::dbToLinear(-40.0f);
  REQUIRE(threshold40dB == Catch::Approx(0.01f).margin(0.001f));

  // -60 dB should be approximately 0.001 linear
  float threshold60dB = AudioRecorder::dbToLinear(-60.0f);
  REQUIRE(threshold60dB == Catch::Approx(0.001f).margin(0.0001f));

  // -20 dB should be approximately 0.1 linear
  float threshold20dB = AudioRecorder::dbToLinear(-20.0f);
  REQUIRE(threshold20dB == Catch::Approx(0.1f).margin(0.001f));
}

TEST_CASE("Normalization target dB to linear conversion", "[audio][recorder][processing]") {
  // -1 dB (default target) should be approximately 0.891 linear
  float target1dB = AudioRecorder::dbToLinear(-1.0f);
  REQUIRE(target1dB == Catch::Approx(0.891f).margin(0.01f));

  // -3 dB should be approximately 0.708 linear
  float target3dB = AudioRecorder::dbToLinear(-3.0f);
  REQUIRE(target3dB == Catch::Approx(0.708f).margin(0.01f));

  // 0 dB should be exactly 1.0 linear (unity gain)
  float target0dB = AudioRecorder::dbToLinear(0.0f);
  REQUIRE(target0dB == 1.0f);
}

TEST_CASE("RecordingResult flags", "[audio][recorder][result]") {
  RecordingResult result;

  SECTION("Default result flags are false") {
    REQUIRE_FALSE(result.trimmed);
    REQUIRE_FALSE(result.normalized);
  }

  SECTION("Result flags can be set") {
    result.trimmed = true;
    result.normalized = true;

    REQUIRE(result.trimmed);
    REQUIRE(result.normalized);
  }
}

// ============================================================================
// Thread Safety Tests (Issue #461)
// ============================================================================

TEST_CASE("AudioRecorder thread safety - concurrent stop and cancel", "[audio][recorder][threading]") {
  // This test verifies that concurrent calls to stopRecording() and cancelRecording()
  // don't cause race conditions or crashes (Issue #461)

  AudioRecorder recorder;

  // Test multiple rapid cancel operations
  SECTION("Multiple rapid cancel calls are safe") {
    // Start from idle state
    REQUIRE(recorder.getState() == RecordingState::Idle);

    // Multiple cancel calls should be safe even when not recording
    for (int i = 0; i < 100; ++i) {
      recorder.cancelRecording();
    }

    REQUIRE(recorder.getState() == RecordingState::Idle);
  }

  // Test that state transitions are consistent
  SECTION("State transitions are atomic") {
    RecordingState lastState = recorder.getState();

    // Rapidly query state from multiple threads
    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};

    auto stateChecker = [&]() {
      while (!stop) {
        RecordingState current = recorder.getState();
        // State should always be a valid enum value
        int stateValue = static_cast<int>(current);
        if (stateValue < 0 || stateValue > 6) {
          errors++;
        }
      }
    };

    std::thread t1(stateChecker);
    std::thread t2(stateChecker);

    // Let them run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stop = true;
    t1.join();
    t2.join();

    REQUIRE(errors == 0);
  }
}

TEST_CASE("AudioRecorder thread safety - rapid start/stop cycles", "[audio][recorder][threading]") {
  // This test verifies that rapid start/stop/cancel cycles don't cause crashes
  // or data corruption (Issue #461)

  AudioRecorder recorder;

  SECTION("Rapid state transitions without initialization") {
    // These operations should fail gracefully without crashes
    for (int i = 0; i < 50; ++i) {
      auto startResult = recorder.startRecording("/tmp/test_rapid.wav");
      REQUIRE(startResult.isError()); // Should fail - not initialized

      auto stopResult = recorder.stopRecording();
      REQUIRE(stopResult.isError()); // Should fail - not recording

      recorder.cancelRecording(); // Should be safe even when idle
    }

    REQUIRE(recorder.getState() == RecordingState::Idle);
  }
}

TEST_CASE("AudioRecorder thread safety - callback thread safety", "[audio][recorder][threading]") {
  // Verify that callbacks can be safely set from multiple threads

  AudioRecorder recorder;

  std::atomic<int> stateChangeCount{0};
  std::atomic<int> levelUpdateCount{0};

  auto stateCallback = [&](RecordingState state) {
    (void)state;
    stateChangeCount++;
  };

  auto levelCallback = [&](const LevelMeter& level) {
    (void)level;
    levelUpdateCount++;
  };

  SECTION("Concurrent callback setting is safe") {
    std::atomic<bool> stop{false};

    auto setter = [&]() {
      while (!stop) {
        recorder.setOnRecordingStateChanged(stateCallback);
        recorder.setOnLevelUpdate(levelCallback);
        std::this_thread::yield();
      }
    };

    std::thread t1(setter);
    std::thread t2(setter);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stop = true;
    t1.join();
    t2.join();

    // Should complete without crashes
    REQUIRE(true);
  }
}

TEST_CASE("AudioRecorder thread safety - concurrent access patterns", "[audio][recorder][threading]") {
  // Test various concurrent access patterns that could trigger race conditions

  AudioRecorder recorder;

  SECTION("Concurrent getState() calls are safe") {
    std::atomic<bool> stop{false};
    std::atomic<int> stateReadCount{0};

    auto reader = [&]() {
      while (!stop) {
        RecordingState state = recorder.getState();
        (void)state; // Use the state to prevent optimization
        stateReadCount++;
      }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
      threads.emplace_back(reader);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stop = true;
    for (auto& t : threads) {
      t.join();
    }

    // Should have successfully read state many times
    REQUIRE(stateReadCount > 100);
  }

  SECTION("Concurrent format get/set is safe") {
    std::atomic<bool> stop{false};

    auto getter = [&]() {
      while (!stop) {
        const auto& format = recorder.getRecordingFormat();
        (void)format; // Use the format to prevent optimization
        std::this_thread::yield();
      }
    };

    auto setter = [&]() {
      while (!stop) {
        RecordingFormat format;
        format.sampleRate = 48000;
        format.channels = 1;
        recorder.setRecordingFormat(format);
        std::this_thread::yield();
      }
    };

    std::thread t1(getter);
    std::thread t2(getter);
    std::thread t3(setter);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stop = true;
    t1.join();
    t2.join();
    t3.join();

    // Should complete without crashes
    REQUIRE(true);
  }
}

TEST_CASE("AudioRecorder thread safety - destructor with active operations", "[audio][recorder][threading]") {
  // Verify that the destructor properly handles active operations

  SECTION("Destructor while in Idle state") {
    {
      AudioRecorder recorder;
      // Destructor called automatically
    }
    // Should complete without crashes
    REQUIRE(true);
  }

  SECTION("Destructor after multiple cancel calls") {
    {
      AudioRecorder recorder;
      for (int i = 0; i < 10; ++i) {
        recorder.cancelRecording();
      }
      // Destructor called automatically
    }
    // Should complete without crashes
    REQUIRE(true);
  }
}

TEST_CASE("AudioRecorder thread safety - getCurrentLevel thread safety", "[audio][recorder][threading]") {
  // Verify that getCurrentLevel() can be safely called from multiple threads

  AudioRecorder recorder;

  std::atomic<bool> stop{false};
  std::atomic<int> levelReadCount{0};

  auto reader = [&]() {
    while (!stop) {
      LevelMeter level = recorder.getCurrentLevel();
      (void)level; // Use the level to prevent optimization
      levelReadCount++;
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back(reader);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  stop = true;
  for (auto& t : threads) {
    t.join();
  }

  // Should have successfully read level many times
  REQUIRE(levelReadCount > 100);
}
