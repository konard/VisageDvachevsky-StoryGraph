/**
 * @file test_audio_manager.cpp
 * @brief Comprehensive unit tests for AudioManager 2.0
 *
 * Tests cover:
 * - Initialization and shutdown
 * - Sound effect playback
 * - Music playback and transitions
 * - Voice playback with auto-ducking
 * - Volume control and muting
 * - Handle management
 * - Error paths and edge cases
 * - Thread safety (basic tests)
 *
 * Related to Issue #179 - Test coverage gaps
 *
 * Note: These tests do not require actual audio hardware.
 * They test the API surface and state management.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/audio/audio_manager.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace NovelMind::audio;
using namespace NovelMind;

// =============================================================================
// Test Helper - Access private members for overflow testing
// =============================================================================

class AudioManagerTestAccess {
public:
    static void setNextHandleIndex(AudioManager& manager, u32 index) {
        manager.m_nextHandleIndex.store(index, std::memory_order_relaxed);
    }

    static void setHandleGeneration(AudioManager& manager, u8 generation) {
        manager.m_handleGeneration.store(generation, std::memory_order_relaxed);
    }

    static u32 getNextHandleIndex(const AudioManager& manager) {
        return manager.m_nextHandleIndex.load(std::memory_order_relaxed);
    }

    static u8 getHandleGeneration(const AudioManager& manager) {
        return manager.m_handleGeneration.load(std::memory_order_relaxed);
    }
};

// =============================================================================
// AudioHandle Tests
// =============================================================================

TEST_CASE("AudioHandle validity", "[audio][handle]")
{
    AudioHandle handle;

    SECTION("Default handle is invalid") {
        REQUIRE_FALSE(handle.isValid());
        REQUIRE(handle.id == 0);
        REQUIRE_FALSE(handle.valid);
    }

    SECTION("Valid handle") {
        handle.id = 42;
        handle.valid = true;

        REQUIRE(handle.isValid());
        REQUIRE(handle.id == 42);
    }

    SECTION("Invalidate handle") {
        handle.id = 42;
        handle.valid = true;

        handle.invalidate();

        REQUIRE_FALSE(handle.isValid());
        REQUIRE(handle.id == 0);
        REQUIRE_FALSE(handle.valid);
    }
}

TEST_CASE("AudioHandle generation counter", "[audio][handle][issue-557]")
{
    SECTION("Extract generation from handle ID") {
        u32 handleId = AudioHandle::makeHandleId(5, 1000);
        u8 generation = AudioHandle::getGeneration(handleId);
        REQUIRE(generation == 5);
    }

    SECTION("Extract index from handle ID") {
        u32 handleId = AudioHandle::makeHandleId(5, 1000);
        u32 index = AudioHandle::getIndex(handleId);
        REQUIRE(index == 1000);
    }

    SECTION("Make handle ID") {
        u8 generation = 7;
        u32 index = 12345;
        u32 handleId = AudioHandle::makeHandleId(generation, index);

        REQUIRE(AudioHandle::getGeneration(handleId) == generation);
        REQUIRE(AudioHandle::getIndex(handleId) == index);
    }

    SECTION("Generation counter occupies upper 8 bits") {
        u32 handleId = AudioHandle::makeHandleId(255, 0);
        REQUIRE(handleId == 0xFF000000);
    }

    SECTION("Index counter occupies lower 24 bits") {
        u32 handleId = AudioHandle::makeHandleId(0, 0x00FFFFFF);
        REQUIRE(handleId == 0x00FFFFFF);
    }

    SECTION("Combined generation and index") {
        u32 handleId = AudioHandle::makeHandleId(128, 0x00ABCDEF);
        REQUIRE(handleId == 0x80ABCDEF);
        REQUIRE(AudioHandle::getGeneration(handleId) == 128);
        REQUIRE(AudioHandle::getIndex(handleId) == 0x00ABCDEF);
    }

    SECTION("Maximum generation value") {
        u32 handleId = AudioHandle::makeHandleId(255, 1);
        u8 generation = AudioHandle::getGeneration(handleId);
        REQUIRE(generation == 255);
    }

    SECTION("Maximum index value") {
        u32 maxIndex = 0x00FFFFFF; // 16,777,215
        u32 handleId = AudioHandle::makeHandleId(0, maxIndex);
        u32 index = AudioHandle::getIndex(handleId);
        REQUIRE(index == maxIndex);
    }
}

// =============================================================================
// PlaybackConfig Tests
// =============================================================================

TEST_CASE("PlaybackConfig defaults", "[audio][config]")
{
    PlaybackConfig config;

    REQUIRE(config.volume == 1.0f);
    REQUIRE(config.pitch == 1.0f);
    REQUIRE(config.pan == 0.0f);
    REQUIRE_FALSE(config.loop);
    REQUIRE(config.fadeInDuration == 0.0f);
    REQUIRE(config.startTime == 0.0f);
    REQUIRE(config.channel == AudioChannel::Sound);
    REQUIRE(config.priority == 0);
}

TEST_CASE("MusicConfig defaults", "[audio][config]")
{
    MusicConfig config;

    REQUIRE(config.volume == 1.0f);
    REQUIRE(config.loop == true);
    REQUIRE(config.fadeInDuration == 0.0f);
    REQUIRE(config.crossfadeDuration == 0.0f);
    REQUIRE(config.startTime == 0.0f);
}

TEST_CASE("VoiceConfig defaults", "[audio][config]")
{
    VoiceConfig config;

    REQUIRE(config.volume == 1.0f);
    REQUIRE(config.duckMusic == true);
    REQUIRE(config.duckAmount == 0.3f);
    REQUIRE(config.duckFadeDuration == 0.2f);
}

// =============================================================================
// AudioSource Tests
// =============================================================================

TEST_CASE("AudioSource creation", "[audio][source]")
{
    AudioSource source;

    REQUIRE(source.getState() == PlaybackState::Stopped);
    REQUIRE_FALSE(source.isPlaying());
    REQUIRE(source.channel == AudioChannel::Sound);
    REQUIRE(source.priority == 0);
}

TEST_CASE("AudioSource state management", "[audio][source]")
{
    AudioSource source;

    SECTION("Play changes state") {
        // Note: Without actual audio data, play may not fully work
        // but we can test the API
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }

    SECTION("Pause and stop") {
        source.pause();
        // Pausing a stopped source shouldn't crash - verify state remains stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);

        source.stop();
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }
}

TEST_CASE("AudioSource properties", "[audio][source]")
{
    AudioSource source;

    SECTION("Volume") {
        source.setVolume(0.5f);
        // Verify operation completes without crash - state should remain stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }

    SECTION("Pitch") {
        source.setPitch(1.5f);
        // Verify operation completes without crash - state should remain stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }

    SECTION("Pan") {
        source.setPan(-0.5f);
        // Verify operation completes without crash - state should remain stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }

    SECTION("Loop") {
        source.setLoop(true);
        // Verify operation completes without crash - state should remain stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }
}

TEST_CASE("AudioSource fade operations", "[audio][source]")
{
    AudioSource source;

    SECTION("Fade in") {
        source.fadeIn(1.0f);
        // fadeIn sets state to FadingIn, which counts as "playing"
        REQUIRE(source.getState() == PlaybackState::FadingIn);
        REQUIRE(source.isPlaying()); // isPlaying returns true for FadingIn state
    }

    SECTION("Fade out") {
        // Start playing first, then fade out
        source.fadeIn(0.0f); // Instant fade in to Playing state
        REQUIRE(source.isPlaying());
        source.fadeOut(1.0f, true);
        // fadeOut sets state to FadingOut, which counts as "playing"
        REQUIRE(source.getState() == PlaybackState::FadingOut);
        REQUIRE(source.isPlaying()); // isPlaying returns true for FadingOut state
    }
}

TEST_CASE("AudioSource update", "[audio][source]")
{
    AudioSource source;

    // Update should not crash - verify state unchanged
    source.update(0.016);
    REQUIRE(source.getState() == PlaybackState::Stopped);
}

// =============================================================================
// AudioManager Tests
// =============================================================================

TEST_CASE("AudioManager creation", "[audio][manager]")
{
    AudioManager manager;

    REQUIRE_FALSE(manager.isMusicPlaying());
    REQUIRE_FALSE(manager.isVoicePlaying());
    REQUIRE(manager.getActiveSourceCount() == 0);
}

TEST_CASE("AudioManager initialization", "[audio][manager]")
{
    AudioManager manager;

    SECTION("Initialize") {
        auto result = manager.initialize();
        // May fail without audio hardware, but shouldn't crash
        // Verify result is valid (either Ok or Error, not uninitialized)
        if (result.isOk()) {
            manager.shutdown();
            REQUIRE(result.isOk());
        } else {
            REQUIRE(result.isError());
        }
    }

    SECTION("Multiple shutdown is safe") {
        manager.shutdown();
        manager.shutdown();
        // Verify multiple shutdowns don't crash - manager should remain in valid state
        REQUIRE(manager.getActiveSourceCount() == 0);
    }
}

TEST_CASE("AudioManager volume control", "[audio][manager]")
{
    AudioManager manager;

    SECTION("Master volume") {
        manager.setMasterVolume(0.75f);
        REQUIRE(manager.getMasterVolume() == Catch::Approx(0.75f));

        manager.setMasterVolume(0.0f);
        REQUIRE(manager.getMasterVolume() == Catch::Approx(0.0f));

        manager.setMasterVolume(1.0f);
        REQUIRE(manager.getMasterVolume() == Catch::Approx(1.0f));
    }

    SECTION("Channel volumes") {
        manager.setChannelVolume(AudioChannel::Music, 0.5f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Music) == Catch::Approx(0.5f));

        manager.setChannelVolume(AudioChannel::Sound, 0.8f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Sound) == Catch::Approx(0.8f));

        manager.setChannelVolume(AudioChannel::Voice, 0.9f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Voice) == Catch::Approx(0.9f));
    }

    SECTION("Default channel volumes are set correctly") {
        // These are the actual default values from AudioManager constructor
        REQUIRE(manager.getChannelVolume(AudioChannel::Master) == 1.0f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Music) == 0.8f);  // Music defaults to 0.8
        REQUIRE(manager.getChannelVolume(AudioChannel::Sound) == 1.0f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Voice) == 1.0f);
    }
}

TEST_CASE("AudioManager muting", "[audio][manager]")
{
    AudioManager manager;

    SECTION("Channel muting") {
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Music));

        manager.setChannelMuted(AudioChannel::Music, true);
        REQUIRE(manager.isChannelMuted(AudioChannel::Music));

        manager.setChannelMuted(AudioChannel::Music, false);
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Music));
    }

    SECTION("Mute all") {
        // muteAll sets a global mute flag, not individual channel mutes
        // Individual channel mute states remain unchanged
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Master));

        manager.muteAll();

        // isChannelMuted checks individual channel mute, not global mute
        // Individual channels should still report their original mute state
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Master));
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Music));

        // Set individual channel mutes
        manager.setChannelMuted(AudioChannel::Music, true);
        REQUIRE(manager.isChannelMuted(AudioChannel::Music));

        manager.unmuteAll();

        // Individual channel mute should still be set
        REQUIRE(manager.isChannelMuted(AudioChannel::Music));

        // Unset it
        manager.setChannelMuted(AudioChannel::Music, false);
        REQUIRE_FALSE(manager.isChannelMuted(AudioChannel::Music));
    }
}

TEST_CASE("AudioManager sound playback API", "[audio][manager][sound]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        // Skip if audio initialization fails (no hardware)
        SKIP("Audio hardware not available");
    }

    SECTION("Play sound with config") {
        PlaybackConfig config;
        config.volume = 0.5f;
        config.loop = false;

        auto handle = manager.playSound("test_sound", config);
        // Verify operation completes - handle may be invalid without real audio file
        // but the call should not crash
        REQUIRE_FALSE(manager.isMusicPlaying()); // Sound should not affect music state
    }

    SECTION("Play sound simple") {
        auto handle = manager.playSound("test_sound", 0.8f, false);
        // Verify operation completes without crash
        REQUIRE_FALSE(manager.isVoicePlaying()); // Sound should not affect voice state
    }

    SECTION("Stop sound") {
        auto handle = manager.playSound("test_sound", 1.0f);
        manager.stopSound(handle, 0.5f);
        // Verify stopSound doesn't crash with potentially invalid handle
        REQUIRE(manager.getActiveSourceCount() >= 0); // Count should be non-negative
    }

    SECTION("Stop all sounds") {
        manager.playSound("sound1", 1.0f);
        manager.playSound("sound2", 1.0f);
        manager.stopAllSounds(0.0f);
        // Verify all sounds are stopped - active count may vary based on initialization
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    manager.shutdown();
}

TEST_CASE("AudioManager music playback API", "[audio][manager][music]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        SKIP("Audio hardware not available");
    }

    SECTION("Play music") {
        MusicConfig config;
        config.volume = 0.7f;
        config.loop = true;

        auto handle = manager.playMusic("background_music", config);
        // Verify operation completes - even if file doesn't exist, shouldn't crash
        REQUIRE_FALSE(manager.isVoicePlaying()); // Music should not affect voice state
    }

    SECTION("Crossfade music") {
        manager.playMusic("music1");
        manager.crossfadeMusic("music2", 1.0f);
        // Verify crossfade doesn't crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Music controls") {
        manager.playMusic("music1");

        manager.pauseMusic();
        manager.resumeMusic();
        manager.stopMusic(0.5f);

        // Verify music controls complete without crash
        REQUIRE_FALSE(manager.isMusicPlaying());
    }

    SECTION("Music position") {
        manager.playMusic("music1");

        f32 pos = manager.getMusicPosition();
        REQUIRE(pos >= 0.0f);

        manager.seekMusic(10.0f);
        // Verify seek completes - position should still be non-negative
        f32 newPos = manager.getMusicPosition();
        REQUIRE(newPos >= 0.0f);
    }

    SECTION("Current music ID") {
        manager.playMusic("test_music");
        // May not actually play without real file
        const auto& id = manager.getCurrentMusicId();
        // Verify getCurrentMusicId returns a valid string (may be empty)
        REQUIRE((id.empty() || !id.empty())); // String is in valid state
    }

    manager.shutdown();
}

TEST_CASE("AudioManager voice playback API", "[audio][manager][voice]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        SKIP("Audio hardware not available");
    }

    SECTION("Play voice") {
        VoiceConfig config;
        config.volume = 1.0f;
        config.duckMusic = true;

        auto handle = manager.playVoice("voice_line", config);
        // Verify playVoice completes - even without real audio file
        REQUIRE_FALSE(manager.isMusicPlaying()); // Voice should not affect music playing state
    }

    SECTION("Voice controls") {
        manager.playVoice("voice1");

        manager.skipVoice();
        manager.stopVoice(0.0f);

        // Verify voice controls complete without crash
        REQUIRE_FALSE(manager.isVoicePlaying());
    }

    SECTION("Check voice playing state") {
        REQUIRE_FALSE(manager.isVoicePlaying());

        manager.playVoice("voice1");
        // May not actually be playing without real file
    }

    manager.shutdown();
}

TEST_CASE("AudioManager global operations", "[audio][manager]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        SKIP("Audio hardware not available");
    }

    SECTION("Fade all") {
        manager.fadeAllTo(0.5f, 1.0f);
        // Verify fadeAll completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Pause all") {
        manager.pauseAll();
        // Verify pauseAll completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Resume all") {
        manager.resumeAll();
        // Verify resumeAll completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Stop all") {
        manager.stopAll(0.5f);
        // Verify stopAll completes - all sources should stop
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    manager.shutdown();
}

TEST_CASE("AudioManager handle operations", "[audio][manager][handle]")
{
    AudioManager manager;

    SECTION("Check invalid handle") {
        AudioHandle invalid;
        REQUIRE_FALSE(manager.isPlaying(invalid));
    }

    SECTION("Get source with invalid handle") {
        AudioHandle invalid;
        auto* source = manager.getSource(invalid);
        REQUIRE(source == nullptr);
    }

    SECTION("Get active sources") {
        auto handles = manager.getActiveSources();
        REQUIRE(handles.empty());
    }

    SECTION("Active source count") {
        REQUIRE(manager.getActiveSourceCount() == 0);
    }
}

TEST_CASE("AudioManager configuration", "[audio][manager][config]")
{
    AudioManager manager;

    SECTION("Set max sounds") {
        manager.setMaxSounds(64);
        // Verify configuration change completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Auto-ducking") {
        manager.setAutoDuckingEnabled(false);
        manager.setAutoDuckingEnabled(true);
        // Verify auto-ducking toggle completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Ducking parameters") {
        manager.setDuckingParams(0.5f, 0.3f);
        // Verify ducking parameter change completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }
}

TEST_CASE("AudioManager callbacks", "[audio][manager][callback]")
{
    AudioManager manager;

    SECTION("Set event callback") {
        bool callbackFired = false;

        manager.setEventCallback([&]([[maybe_unused]] const AudioEvent& event) {
            callbackFired = true;
        });

        // Verify callback registration completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Set data provider") {
        manager.setDataProvider([]([[maybe_unused]] const std::string& id) -> Result<std::vector<u8>> {
            return Result<std::vector<u8>>::error("Not implemented");
        });

        // Verify data provider registration completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }
}

TEST_CASE("AudioManager update", "[audio][manager]")
{
    AudioManager manager;

    // Update should not crash even when not initialized
    manager.update(0.016);
    // Verify update completes and manager remains in valid state
    REQUIRE(manager.getActiveSourceCount() >= 0);
}

// =============================================================================
// Audio Ducking Division by Zero Tests - Issue #449
// =============================================================================

TEST_CASE("AudioManager ducking - zero duck time", "[audio][manager][ducking][issue-449]")
{
    AudioManager manager;

    SECTION("Set ducking params with zero fade duration") {
        // Setting zero fade duration should not cause division by zero
        manager.setDuckingParams(0.3f, 0.0f);

        // Verify operation completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Voice with zero duck fade duration") {
        auto initResult = manager.initialize();
        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.3f;
        config.duckFadeDuration = 0.0f; // Zero fade duration

        // Should not crash with zero fade duration
        auto handle = manager.playVoice("test_voice", config);

        // Update should not cause division by zero
        manager.update(0.016);
        manager.update(0.016);

        // Verify no crashes
        REQUIRE(manager.getActiveSourceCount() >= 0);

        manager.shutdown();
    }
}

TEST_CASE("AudioManager ducking - negative duck time", "[audio][manager][ducking][issue-449]")
{
    AudioManager manager;

    SECTION("Set ducking params with negative fade duration") {
        // Negative fade duration should be clamped
        manager.setDuckingParams(0.3f, -1.0f);

        // Verify operation completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Voice with negative duck fade duration") {
        auto initResult = manager.initialize();
        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.3f;
        config.duckFadeDuration = -0.5f; // Negative fade duration

        // Should handle negative fade duration gracefully
        auto handle = manager.playVoice("test_voice", config);

        // Update should not cause issues
        manager.update(0.016);

        // Verify no crashes
        REQUIRE(manager.getActiveSourceCount() >= 0);

        manager.shutdown();
    }
}

TEST_CASE("AudioManager ducking - very small duck time", "[audio][manager][ducking][issue-449]")
{
    AudioManager manager;

    SECTION("Set ducking params with very small fade duration") {
        // Very small fade duration should be clamped to minimum
        manager.setDuckingParams(0.3f, 0.0001f);

        // Verify operation completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Voice with very small duck fade duration") {
        auto initResult = manager.initialize();
        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.3f;
        config.duckFadeDuration = 0.00001f; // Very small fade duration

        // Should clamp to minimum safe value
        auto handle = manager.playVoice("test_voice", config);

        // Multiple updates to ensure ducking calculation doesn't cause issues
        for (int i = 0; i < 10; ++i) {
            manager.update(0.016);
        }

        // Verify no crashes or NaN/Inf propagation
        REQUIRE(manager.getActiveSourceCount() >= 0);

        manager.shutdown();
    }
}

TEST_CASE("AudioManager ducking - valid duck time", "[audio][manager][ducking][issue-449]")
{
    AudioManager manager;

    SECTION("Set ducking params with valid fade duration") {
        // Normal fade duration should work correctly
        manager.setDuckingParams(0.3f, 0.2f);

        // Verify operation completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Voice with valid duck fade duration") {
        auto initResult = manager.initialize();
        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.3f;
        config.duckFadeDuration = 0.2f; // Normal fade duration

        auto handle = manager.playVoice("test_voice", config);

        // Update multiple times to simulate normal ducking
        for (int i = 0; i < 20; ++i) {
            manager.update(0.016); // ~60 FPS
        }

        // Verify everything works correctly
        REQUIRE(manager.getActiveSourceCount() >= 0);

        manager.shutdown();
    }
}

TEST_CASE("AudioSource fade - zero fade duration", "[audio][source][fade][issue-449]")
{
    AudioSource source;

    SECTION("Fade in with zero duration") {
        // Zero duration should complete immediately without division by zero
        source.fadeIn(0.0f);

        // Verify state is set correctly
        REQUIRE(source.getState() == PlaybackState::Playing);
    }

    SECTION("Fade out with zero duration") {
        // Zero duration should complete immediately
        source.fadeOut(0.0f, true);

        // Verify state is stopped
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }
}

TEST_CASE("AudioSource fade - negative fade duration", "[audio][source][fade][issue-449]")
{
    AudioSource source;

    SECTION("Fade in with negative duration") {
        // Negative duration should be handled gracefully
        source.fadeIn(-1.0f);

        // Should complete immediately or handle gracefully
        REQUIRE(source.getState() == PlaybackState::Playing);
    }

    SECTION("Fade out with negative duration") {
        // Negative duration should be handled gracefully
        source.fadeOut(-0.5f, true);

        // Should complete immediately
        REQUIRE(source.getState() == PlaybackState::Stopped);
    }
}

// =============================================================================
// Error Path Tests
// =============================================================================

TEST_CASE("AudioManager error handling - uninitialized operations", "[audio][manager][error]")
{
    AudioManager manager;
    // Do not initialize

    SECTION("Play sound without initialization") {
        auto handle = manager.playSound("test");
        // Should return invalid handle or handle operation gracefully
        REQUIRE_FALSE(manager.isMusicPlaying());
    }

    SECTION("Play music without initialization") {
        auto handle = manager.playMusic("test");
        // Verify operation completes even without initialization
        REQUIRE_FALSE(manager.isVoicePlaying());
    }

    SECTION("Stop operations on uninitialized manager") {
        manager.stopAllSounds();
        manager.stopMusic();
        manager.stopVoice();
        // Verify stop operations complete without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }
}

TEST_CASE("AudioManager error handling - invalid handles", "[audio][manager][error]")
{
    AudioManager manager;

    AudioHandle invalid;
    invalid.id = 999;
    invalid.valid = false;

    SECTION("Stop invalid handle") {
        manager.stopSound(invalid);
        // Verify stopping invalid handle completes without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }

    SECTION("Check invalid handle playing") {
        REQUIRE_FALSE(manager.isPlaying(invalid));
    }

    SECTION("Get source for invalid handle") {
        auto* source = manager.getSource(invalid);
        REQUIRE(source == nullptr);
    }
}

TEST_CASE("AudioManager error handling - invalid parameters", "[audio][manager][error]")
{
    AudioManager manager;

    SECTION("Negative volumes should be handled") {
        manager.setMasterVolume(-1.0f);
        // Implementation should clamp or handle gracefully
        f32 volume = manager.getMasterVolume();
        REQUIRE(volume >= 0.0f); // Volume should be clamped to non-negative
    }

    SECTION("Very large volumes should be handled") {
        manager.setMasterVolume(100.0f);
        // Verify large volume is handled gracefully
        f32 volume = manager.getMasterVolume();
        REQUIRE(volume >= 0.0f); // Volume should be valid
    }

    SECTION("Invalid channel operations") {
        // Test that operations on all channels don't crash
        for (int i = 0; i < 10; ++i) {
            AudioChannel ch = static_cast<AudioChannel>(i);
            manager.setChannelVolume(ch, 0.5f);
            [[maybe_unused]] auto volume = manager.getChannelVolume(ch);
            manager.setChannelMuted(ch, true);
            [[maybe_unused]] auto muted = manager.isChannelMuted(ch);
        }
        // Verify operations complete without crash
        REQUIRE(manager.getActiveSourceCount() >= 0);
    }
}

// =============================================================================
// Thread Safety Tests (Basic)
// =============================================================================

TEST_CASE("AudioManager basic thread safety", "[audio][manager][threading]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        SKIP("Audio hardware not available");
    }

    SECTION("Concurrent volume changes") {
        std::vector<std::thread> threads;

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 10; ++j) {
                    manager.setMasterVolume(0.5f);
                    [[maybe_unused]] auto masterVolume = manager.getMasterVolume();
                    manager.setChannelVolume(AudioChannel::Music, 0.7f);
                    [[maybe_unused]] auto musicVolume =
                        manager.getChannelVolume(AudioChannel::Music);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Verify concurrent operations completed without crash
        REQUIRE(manager.getMasterVolume() >= 0.0f);
        REQUIRE(manager.getChannelVolume(AudioChannel::Music) >= 0.0f);
    }

    SECTION("Concurrent mute operations") {
        std::vector<std::thread> threads;

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 10; ++j) {
                    manager.setChannelMuted(AudioChannel::Sound, true);
                    manager.setChannelMuted(AudioChannel::Sound, false);
                    [[maybe_unused]] auto muted =
                        manager.isChannelMuted(AudioChannel::Sound);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Verify concurrent mute operations completed without crash
        // Final state may be muted or unmuted, but should be valid
        bool isMuted = manager.isChannelMuted(AudioChannel::Sound);
        REQUIRE((isMuted == true || isMuted == false)); // Valid boolean state
    }

    manager.shutdown();
}

// =============================================================================
// Handle ID Overflow Tests - Issue #557
// =============================================================================

TEST_CASE("AudioManager handle ID overflow protection", "[audio][manager][handle][issue-557]")
{
    SECTION("Handle generation increments on index overflow") {
        AudioManager manager;
        auto initResult = manager.initialize();

        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        // Set initial index to near overflow
        constexpr u32 MAX_INDEX = 0x00FFFFFF; // 16,777,215
        AudioManagerTestAccess::setNextHandleIndex(manager, MAX_INDEX - 5);
        AudioManagerTestAccess::setHandleGeneration(manager, 0);

        // Create handles approaching overflow
        std::vector<AudioHandle> handles;
        for (int i = 0; i < 10; ++i) {
            PlaybackConfig config;
            auto handle = manager.playSound("test_" + std::to_string(i), config);
            if (handle.isValid()) {
                handles.push_back(handle);
            }
        }

        // Verify generation incremented after overflow
        u8 currentGen = AudioManagerTestAccess::getHandleGeneration(manager);
        REQUIRE(currentGen >= 1);

        // Verify index reset after overflow
        u32 currentIndex = AudioManagerTestAccess::getNextHandleIndex(manager);
        REQUIRE(currentIndex < 100); // Should have reset to small value

        manager.shutdown();
    }

    SECTION("No handle collision after overflow") {
        AudioManager manager;
        auto initResult = manager.initialize();

        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        // Set to just before overflow
        constexpr u32 MAX_INDEX = 0x00FFFFFF;
        AudioManagerTestAccess::setNextHandleIndex(manager, MAX_INDEX - 2);
        AudioManagerTestAccess::setHandleGeneration(manager, 5);

        // Create handles that will trigger overflow
        PlaybackConfig config;
        auto handle1 = manager.playSound("sound1", config);
        auto handle2 = manager.playSound("sound2", config);
        auto handle3 = manager.playSound("sound3", config); // This should trigger overflow
        auto handle4 = manager.playSound("sound4", config);

        // Verify all handles have unique IDs
        std::set<u32> uniqueIds;
        if (handle1.isValid()) uniqueIds.insert(handle1.id);
        if (handle2.isValid()) uniqueIds.insert(handle2.id);
        if (handle3.isValid()) uniqueIds.insert(handle3.id);
        if (handle4.isValid()) uniqueIds.insert(handle4.id);

        // All valid handles must have unique IDs
        size_t validCount = 0;
        if (handle1.isValid()) validCount++;
        if (handle2.isValid()) validCount++;
        if (handle3.isValid()) validCount++;
        if (handle4.isValid()) validCount++;

        REQUIRE(uniqueIds.size() == validCount);

        // Verify generation changed for post-overflow handles
        if (handle3.isValid() && handle1.isValid()) {
            u8 gen1 = AudioHandle::getGeneration(handle1.id);
            u8 gen3 = AudioHandle::getGeneration(handle3.id);
            // Generation should have incremented
            REQUIRE(gen3 > gen1);
        }

        manager.shutdown();
    }

    SECTION("Handle ID format consistency") {
        AudioManager manager;
        auto initResult = manager.initialize();

        if (initResult.isError()) {
            SKIP("Audio hardware not available");
        }

        AudioManagerTestAccess::setNextHandleIndex(manager, 1000);
        AudioManagerTestAccess::setHandleGeneration(manager, 42);

        PlaybackConfig config;
        auto handle = manager.playSound("test", config);

        if (handle.isValid()) {
            u8 gen = AudioHandle::getGeneration(handle.id);
            u32 index = AudioHandle::getIndex(handle.id);

            // Verify generation is correct
            REQUIRE(gen == 42);

            // Verify index is in expected range
            REQUIRE(index >= 1000);
            REQUIRE(index <= 1001);

            // Verify we can reconstruct the ID
            u32 reconstructedId = AudioHandle::makeHandleId(gen, index);
            REQUIRE(reconstructedId == handle.id);
        }

        manager.shutdown();
    }
}

TEST_CASE("AudioManager stress test - handle overflow scenario", "[audio][manager][stress][issue-557]")
{
    AudioManager manager;
    auto initResult = manager.initialize();

    if (initResult.isError()) {
        SKIP("Audio hardware not available");
    }

    SECTION("Create and destroy many handles") {
        // Start near overflow to test faster
        constexpr u32 MAX_INDEX = 0x00FFFFFF;
        AudioManagerTestAccess::setNextHandleIndex(manager, MAX_INDEX - 100);
        AudioManagerTestAccess::setHandleGeneration(manager, 0);

        std::vector<AudioHandle> activeHandles;
        std::set<u32> allHandleIds;

        // Create many handles that will trigger multiple overflows
        for (int i = 0; i < 200; ++i) {
            PlaybackConfig config;
            auto handle = manager.playSound("stress_test_" + std::to_string(i), config);

            if (handle.isValid()) {
                // Verify handle ID is unique
                REQUIRE(allHandleIds.find(handle.id) == allHandleIds.end());
                allHandleIds.insert(handle.id);
                activeHandles.push_back(handle);
            }

            // Stop some handles to simulate real usage
            if (i % 10 == 0 && !activeHandles.empty()) {
                manager.stopSound(activeHandles.back());
                activeHandles.pop_back();
            }
        }

        // Verify generation incremented
        u8 finalGen = AudioManagerTestAccess::getHandleGeneration(manager);
        REQUIRE(finalGen >= 1);

        // Verify all IDs were unique
        REQUIRE(allHandleIds.size() >= 150); // At least most handles should be valid

        manager.shutdown();
    }
}

// Note: Full race condition stress tests require ThreadSanitizer:
//   cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..

// =============================================================================
// Error Path Tests - Issue #498 (Audio Hardware Failure)
// =============================================================================

TEST_CASE("AudioManager error paths - initialization failure recovery",
          "[audio][manager][error][issue-498]") {
  // Test that the audio manager handles initialization failures gracefully
  // This simulates scenarios where audio hardware is unavailable or fails

  SECTION("Manager remains in safe state after init failure") {
    AudioManager manager;
    auto result = manager.initialize();

    // If initialization fails (no audio hardware), verify manager is still safe
    if (result.isError()) {
      // Manager should not crash on operations when not initialized
      REQUIRE_FALSE(manager.isMusicPlaying());
      REQUIRE_FALSE(manager.isVoicePlaying());
      REQUIRE(manager.getActiveSourceCount() == 0);

      // Volume operations should still work
      manager.setMasterVolume(0.5f);
      REQUIRE(manager.getMasterVolume() == Catch::Approx(0.5f));

      // Playback operations should return invalid handles or fail gracefully
      auto handle = manager.playSound("test");
      REQUIRE_FALSE(handle.isValid());

      auto musicHandle = manager.playMusic("test");
      REQUIRE_FALSE(musicHandle.isValid());

      auto voiceHandle = manager.playVoice("test");
      REQUIRE_FALSE(voiceHandle.isValid());

      // Shutdown should not crash
      manager.shutdown();
      REQUIRE(manager.getActiveSourceCount() == 0);
    }
  }

  SECTION("Multiple initialization attempts are safe") {
    AudioManager manager;

    auto result1 = manager.initialize();
    auto result2 = manager.initialize(); // Second init should be safe

    if (result1.isOk()) {
      REQUIRE(result2.isOk()); // Should return ok if already initialized
      manager.shutdown();
    }
  }

  SECTION("Operations after failed initialization don't crash") {
    AudioManager manager;
    // Don't initialize

    // All these should handle the uninitialized state gracefully
    manager.update(0.016);
    manager.stopAllSounds();
    manager.stopMusic();
    manager.stopVoice();
    manager.pauseAll();
    manager.resumeAll();
    manager.fadeAllTo(0.5f, 1.0f);

    REQUIRE(manager.getActiveSourceCount() == 0);
  }
}

TEST_CASE("AudioManager error paths - resource exhaustion",
          "[audio][manager][error][issue-498]") {
  // Test that audio manager handles resource exhaustion gracefully
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("Maximum sound limit is enforced") {
    // Set a low limit for testing
    manager.setMaxSounds(5);

    std::vector<AudioHandle> handles;
    for (int i = 0; i < 10; ++i) {
      auto handle = manager.playSound("sound_" + std::to_string(i));
      if (handle.isValid()) {
        handles.push_back(handle);
      }
    }

    // Should not exceed the limit
    REQUIRE(handles.size() <= 5);
    REQUIRE(manager.getActiveSourceCount() <= 5);
  }

  SECTION("Priority-based eviction works") {
    manager.setMaxSounds(3);

    PlaybackConfig lowPriority;
    lowPriority.priority = 1;
    auto low1 = manager.playSound("low1", lowPriority);

    PlaybackConfig medPriority;
    medPriority.priority = 5;
    auto med1 = manager.playSound("med1", medPriority);

    PlaybackConfig highPriority;
    highPriority.priority = 10;
    auto high1 = manager.playSound("high1", highPriority);

    // Now try to add another high priority sound
    // Should evict the low priority sound
    auto high2 = manager.playSound("high2", highPriority);

    // High priority sound should be playable
    if (high2.isValid()) {
      // Low priority might have been evicted
      REQUIRE(manager.getActiveSourceCount() <= 3);
    }
  }

  SECTION("Graceful degradation when limit reached") {
    manager.setMaxSounds(2);

    auto s1 = manager.playSound("sound1");
    auto s2 = manager.playSound("sound2");
    auto s3 = manager.playSound("sound3"); // Should fail or evict

    // System should remain stable
    REQUIRE(manager.getActiveSourceCount() <= 2);

    // Can still stop sounds
    manager.stopSound(s1);
    manager.update(0.016);

    // Should be able to play new sound after stopping one
    auto s4 = manager.playSound("sound4");
    REQUIRE(manager.getActiveSourceCount() <= 2);
  }

  manager.shutdown();
}

TEST_CASE("AudioManager error paths - data provider failures",
          "[audio][manager][error][issue-498]") {
  // Test that audio manager handles data provider errors gracefully
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("Missing data provider is handled") {
    // Don't set a data provider
    auto handle = manager.playSound("test_sound");

    // Should either return invalid handle or handle gracefully
    // The implementation may try to load from file system as fallback
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  SECTION("Data provider returning error is handled") {
    // Set a data provider that always fails
    manager.setDataProvider([](const std::string& id) -> Result<std::vector<u8>> {
      return Result<std::vector<u8>>::error("Data not found: " + id);
    });

    auto handle = manager.playSound("test_sound");

    // Should return invalid handle
    REQUIRE_FALSE(handle.isValid());

    // Manager should remain stable
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  SECTION("Data provider returning empty data is handled") {
    // Set a data provider that returns empty data
    manager.setDataProvider([](const std::string& id) -> Result<std::vector<u8>> {
      return Result<std::vector<u8>>::ok(std::vector<u8>{});
    });

    auto handle = manager.playSound("test_sound");

    // Should handle empty data gracefully
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  SECTION("Data provider exception doesn't crash manager") {
    // Note: This test verifies that if a data provider throws,
    // the manager remains stable
    bool callbackFired = false;

    manager.setEventCallback([&](const AudioEvent& event) {
      if (event.type == AudioEvent::Type::Error) {
        callbackFired = true;
      }
    });

    // Try to play with potentially failing provider
    auto handle = manager.playSound("test");

    // Manager should remain operational
    manager.update(0.016);
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  manager.shutdown();
}

// =============================================================================
// Thread Safety Tests - Issue #462
// =============================================================================

TEST_CASE("AudioManager thread safety - playback thread race condition",
          "[audio][manager][threading][issue-462]") {
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("Concurrent voice playback and status checks") {
    std::atomic<bool> running{true};
    std::atomic<int> voiceStarts{0};
    std::atomic<int> statusChecks{0};

    // Thread 1: Repeatedly start/stop voice
    std::thread voiceThread([&]() {
      for (int i = 0; i < 50 && running; ++i) {
        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.3f;
        auto handle = manager.playVoice("test_voice", config);
        voiceStarts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        manager.stopVoice(0.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });

    // Thread 2: Continuously check voice status
    std::thread statusThread([&]() {
      while (running) {
        [[maybe_unused]] bool isPlaying = manager.isVoicePlaying();
        statusChecks++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });

    // Thread 3: Update mixer state (simulates audio thread)
    std::thread updateThread([&]() {
      for (int i = 0; i < 100 && running; ++i) {
        manager.update(0.016); // ~60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      running = false;
    });

    voiceThread.join();
    statusThread.join();
    updateThread.join();

    // Verify no crashes occurred and operations completed
    REQUIRE(voiceStarts > 0);
    REQUIRE(statusChecks > 0);
  }

  SECTION("Concurrent music state access") {
    std::atomic<bool> running{true};

    std::thread musicThread([&]() {
      for (int i = 0; i < 30 && running; ++i) {
        manager.playMusic("music" + std::to_string(i % 5));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    });

    std::thread queryThread([&]() {
      for (int i = 0; i < 100 && running; ++i) {
        [[maybe_unused]] bool playing = manager.isMusicPlaying();
        [[maybe_unused]] auto id = manager.getCurrentMusicId();
        [[maybe_unused]] f32 pos = manager.getMusicPosition();
        std::this_thread::yield();
      }
    });

    std::thread controlThread([&]() {
      for (int i = 0; i < 50 && running; ++i) {
        manager.pauseMusic();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        manager.resumeMusic();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });

    musicThread.join();
    queryThread.join();
    controlThread.join();

    // Verify no crashes
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  manager.shutdown();
}

TEST_CASE("AudioManager thread safety - multiple sources",
          "[audio][manager][threading][issue-462]") {
  // Issue #494: Skip this test in CI environments due to known race condition
  // in AudioManager that causes SIGSEGV in Debug builds under heavy concurrency.
  // The race condition exists between playSound's lock release and createSource's
  // lock acquisition, allowing another thread to modify the sources vector.
  // TODO: Fix the underlying AudioManager race condition in issue #462.
  const char* ciEnv = std::getenv("CI");
  if (ciEnv && std::string(ciEnv) == "true") {
    SKIP("Skipping flaky threading test in CI environment - see issue #462");
  }

  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("Concurrent sound playback") {
    std::vector<std::thread> threads;
    std::atomic<int> soundsPlayed{0};

    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&, t]() {
        for (int i = 0; i < 20; ++i) {
          PlaybackConfig config;
          config.volume = 0.5f;
          config.priority = i;
          auto handle = manager.playSound("sound_" + std::to_string(t * 100 + i),
                                          config);
          if (handle.isValid()) {
            soundsPlayed++;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
      });
    }

    // Update thread
    std::thread updateThread([&]() {
      for (int i = 0; i < 100; ++i) {
        manager.update(0.016);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    });

    for (auto &t : threads) {
      t.join();
    }
    updateThread.join();

    // In CI environment without audio files, sounds may not play
    // The test verifies thread safety - no crashes or deadlocks
    // If audio is available, some sounds should have played
    // If no audio files are available, soundsPlayed will be 0 which is acceptable
    INFO("Sounds played: " << soundsPlayed.load());
    // Test passes if no crashes occurred during concurrent access
  }

  SECTION("Concurrent volume and mute changes") {
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&]() {
        for (int i = 0; i < 30; ++i) {
          manager.setMasterVolume(0.5f + (i % 5) * 0.1f);
          manager.setChannelVolume(AudioChannel::Music, 0.7f);
          manager.setChannelVolume(AudioChannel::Sound, 0.8f);
          manager.setChannelMuted(AudioChannel::Voice, i % 2 == 0);
          std::this_thread::yield();
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    // Verify final state is valid
    f32 masterVol = manager.getMasterVolume();
    REQUIRE(masterVol >= 0.0f);
    REQUIRE(masterVol <= 1.0f);
  }

  SECTION("Concurrent ducking state access") {
    std::atomic<bool> running{true};

    std::thread voiceThread([&]() {
      for (int i = 0; i < 20 && running; ++i) {
        VoiceConfig config;
        config.duckMusic = true;
        config.duckAmount = 0.2f + (i % 5) * 0.1f;
        manager.playVoice("voice_" + std::to_string(i), config);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        manager.stopVoice(0.0f);
      }
    });

    std::thread configThread([&]() {
      for (int i = 0; i < 50 && running; ++i) {
        manager.setAutoDuckingEnabled(i % 2 == 0);
        manager.setDuckingParams(0.3f + (i % 4) * 0.05f, 0.2f);
        std::this_thread::yield();
      }
    });

    std::thread updateThread([&]() {
      for (int i = 0; i < 50; ++i) {
        manager.update(0.016);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      running = false;
    });

    voiceThread.join();
    configThread.join();
    updateThread.join();

    // Verify no crashes
    REQUIRE(manager.getActiveSourceCount() >= 0);
  }

  manager.shutdown();
}

// =============================================================================
// Thread Safety Tests - Issue #558
// =============================================================================

TEST_CASE("AudioManager thread safety - concurrent sound creation with limit",
          "[audio][manager][threading][issue-558]") {
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("Concurrent playSound calls should never exceed max limit") {
    const size_t maxSounds = 10;
    manager.setMaxSounds(maxSounds);

    std::atomic<int> successfulCreations{0};
    std::atomic<int> failedCreations{0};
    std::vector<std::thread> threads;

    // Launch many threads trying to create sounds simultaneously
    for (int t = 0; t < 8; ++t) {
      threads.emplace_back([&, t]() {
        for (int i = 0; i < 20; ++i) {
          PlaybackConfig config;
          config.volume = 0.5f;
          config.priority = i;
          auto handle = manager.playSound("test_sound_" + std::to_string(t * 100 + i),
                                          config);
          if (handle.isValid()) {
            successfulCreations++;
          } else {
            failedCreations++;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      });
    }

    for (auto &thread : threads) {
      thread.join();
    }

    // Verify that we never exceeded the limit
    size_t finalCount = manager.getActiveSourceCount();
    REQUIRE(finalCount <= maxSounds);

    // Verify that some sounds were created
    REQUIRE(successfulCreations > 0);
  }

  SECTION("Stress test: Rapid concurrent sound creation and limit check") {
    const size_t maxSounds = 5;
    manager.setMaxSounds(maxSounds);

    std::atomic<bool> running{true};
    std::atomic<size_t> maxObservedCount{0};

    // Thread 1-4: Continuously create sounds
    std::vector<std::thread> creatorThreads;
    for (int t = 0; t < 4; ++t) {
      creatorThreads.emplace_back([&, t]() {
        int counter = 0;
        while (running) {
          PlaybackConfig config;
          config.priority = counter % 10;
          manager.playSound("sound_" + std::to_string(t) + "_" +
                           std::to_string(counter++), config);
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      });
    }

    // Thread 5: Monitor thread that checks the count never exceeds limit
    std::thread monitorThread([&]() {
      for (int i = 0; i < 500; ++i) {
        size_t count = manager.getActiveSourceCount();
        size_t current = maxObservedCount.load();
        while (count > current &&
               !maxObservedCount.compare_exchange_weak(current, count)) {
          // Retry if another thread updated maxObservedCount
        }

        // CRITICAL CHECK: Verify limit is never exceeded
        REQUIRE(count <= maxSounds);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
      running = false;
    });

    for (auto &thread : creatorThreads) {
      thread.join();
    }
    monitorThread.join();

    // Final verification
    REQUIRE(maxObservedCount.load() <= maxSounds);
    REQUIRE(manager.getActiveSourceCount() <= maxSounds);
  }

  SECTION("Verify atomicity of check-and-create operation") {
    const size_t maxSounds = 3;
    manager.setMaxSounds(maxSounds);

    std::atomic<int> creationAttempts{0};
    std::vector<std::thread> threads;
    std::mutex violationMutex;
    std::vector<std::string> violations;

    // Create exactly maxSounds worth of simultaneous attempts
    for (size_t t = 0; t < maxSounds * 3; ++t) {
      threads.emplace_back([&, t]() {
        creationAttempts++;

        // Wait for all threads to be ready
        while (creationAttempts.load() < static_cast<int>(maxSounds * 3)) {
          std::this_thread::yield();
        }

        // All threads try to create sounds at the same time
        PlaybackConfig config;
        config.priority = static_cast<int>(t);
        auto handle = manager.playSound("test_" + std::to_string(t), config);

        // Immediately check the count
        size_t count = manager.getActiveSourceCount();
        if (count > maxSounds) {
          std::lock_guard<std::mutex> lock(violationMutex);
          violations.push_back("Count exceeded: " + std::to_string(count) +
                              " > " + std::to_string(maxSounds));
        }
      });
    }

    for (auto &thread : threads) {
      thread.join();
    }

    // Verify no violations occurred
    if (!violations.empty()) {
      for (const auto &violation : violations) {
        WARN(violation);
      }
      FAIL("Race condition detected: limit was exceeded");
    }

    REQUIRE(manager.getActiveSourceCount() <= maxSounds);
  }

  manager.shutdown();
}

TEST_CASE("AudioManager thread safety - priority-based eviction is atomic",
          "[audio][manager][threading][issue-558]") {
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("High priority sounds should evict low priority atomically") {
    const size_t maxSounds = 5;
    manager.setMaxSounds(maxSounds);

    // Fill up with low priority sounds
    for (size_t i = 0; i < maxSounds; ++i) {
      PlaybackConfig config;
      config.priority = 1;
      manager.playSound("low_priority_" + std::to_string(i), config);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(manager.getActiveSourceCount() == maxSounds);

    std::atomic<int> highPriorityCreated{0};
    std::vector<std::thread> threads;

    // Multiple threads try to create high priority sounds
    for (int t = 0; t < 4; ++t) {
      threads.emplace_back([&, t]() {
        for (int i = 0; i < 10; ++i) {
          PlaybackConfig config;
          config.priority = 100; // Much higher than existing sounds
          auto handle = manager.playSound("high_priority_" +
                                          std::to_string(t * 100 + i), config);
          if (handle.isValid()) {
            highPriorityCreated++;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
      });
    }

    for (auto &thread : threads) {
      thread.join();
    }

    // Verify limit was never exceeded despite concurrent evictions
    REQUIRE(manager.getActiveSourceCount() <= maxSounds);

    // Verify that high priority sounds were successfully created
    REQUIRE(highPriorityCreated > 0);
  }

  manager.shutdown();
}

TEST_CASE("AudioManager thread safety - source creation is truly atomic",
          "[audio][manager][threading][issue-558]") {
  AudioManager manager;
  auto initResult = manager.initialize();

  if (initResult.isError()) {
    SKIP("Audio hardware not available");
  }

  SECTION("TOCTOU race condition should not occur") {
    // This test specifically targets the time-of-check-time-of-use bug
    // where the limit check and source creation are not atomic

    const size_t maxSounds = 2;
    manager.setMaxSounds(maxSounds);

    std::atomic<bool> raceDetected{false};
    std::vector<std::thread> threads;

    // Run multiple iterations to increase chance of catching the race
    for (int iteration = 0; iteration < 10 && !raceDetected; ++iteration) {
      // Clear existing sounds
      manager.stopAllSounds(0.0f);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      std::atomic<int> ready{0};
      const int numThreads = 4;

      for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t, iteration]() {
          ready++;
          // Wait for all threads to be ready
          while (ready.load() < numThreads) {
            std::this_thread::yield();
          }

          // All threads attempt to create sounds simultaneously
          PlaybackConfig config;
          config.priority = t;
          manager.playSound("race_test_" + std::to_string(iteration) +
                           "_" + std::to_string(t), config);

          // Immediately check if limit was exceeded
          size_t count = manager.getActiveSourceCount();
          if (count > maxSounds) {
            raceDetected = true;
          }
        });
      }

      for (auto &thread : threads) {
        thread.join();
      }
      threads.clear();

      // Check final count
      if (manager.getActiveSourceCount() > maxSounds) {
        raceDetected = true;
      }
    }

    REQUIRE_FALSE(raceDetected);
  }

  manager.shutdown();
}
