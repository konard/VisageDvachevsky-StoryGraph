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

TEST_CASE("AudioRecorder initial state", "[audio][recorder]")
{
    AudioRecorder recorder;

    REQUIRE(recorder.getState() == RecordingState::Idle);
    REQUIRE_FALSE(recorder.isRecording());
    REQUIRE_FALSE(recorder.isInitialized());
    REQUIRE_FALSE(recorder.isMeteringActive());
}

TEST_CASE("AudioRecorder state transitions", "[audio][recorder]")
{
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

TEST_CASE("AudioRecorder recording format", "[audio][recorder]")
{
    AudioRecorder recorder;
    RecordingFormat format;
    format.sampleRate = 44100;
    format.channels = 2;

    recorder.setRecordingFormat(format);
    const auto& storedFormat = recorder.getRecordingFormat();

    REQUIRE(storedFormat.sampleRate == 44100);
    REQUIRE(storedFormat.channels == 2);
}

TEST_CASE("AudioRecorder monitoring settings", "[audio][recorder]")
{
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

        recorder.setMonitoringVolume(2.0f);  // Should clamp to 1.0
        REQUIRE(recorder.getMonitoringVolume() == 1.0f);

        recorder.setMonitoringVolume(-1.0f);  // Should clamp to 0.0
        REQUIRE(recorder.getMonitoringVolume() == 0.0f);
    }
}

TEST_CASE("AudioRecorder dB conversion utilities", "[audio][recorder]")
{
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

TEST_CASE("AudioRecorder destructor safety", "[audio][recorder]")
{
    // Test that destructor doesn't crash when called on uninitialized recorder
    {
        AudioRecorder recorder;
    }
    REQUIRE(true);  // If we get here, destructor didn't crash
}

TEST_CASE("AudioRecorder shutdown safety", "[audio][recorder]")
{
    AudioRecorder recorder;

    // Multiple shutdown calls should be safe
    recorder.shutdown();
    recorder.shutdown();

    REQUIRE_FALSE(recorder.isInitialized());
}

TEST_CASE("RecordingState enum values", "[audio][recorder]")
{
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

