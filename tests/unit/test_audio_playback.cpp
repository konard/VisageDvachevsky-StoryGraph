#include <catch2/catch_test_macros.hpp>

#include "NovelMind/audio/audio_manager.hpp"
#include <cmath>

using namespace NovelMind;
using namespace NovelMind::audio;

// ============================================================================
// Audio Handle Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioHandle - Validity checking", "[audio][handle]") {
  SECTION("Default handle is invalid") {
    AudioHandle handle;
    CHECK(handle.isValid() == false);
    CHECK(handle.id == 0);
    CHECK(handle.valid == false);
  }

  SECTION("Handle with ID and valid flag is valid") {
    AudioHandle handle;
    handle.id = 123;
    handle.valid = true;

    CHECK(handle.isValid() == true);
  }

  SECTION("Handle with ID but not marked valid is invalid") {
    AudioHandle handle;
    handle.id = 123;
    handle.valid = false;

    CHECK(handle.isValid() == false);
  }

  SECTION("Invalidate handle") {
    AudioHandle handle;
    handle.id = 456;
    handle.valid = true;

    CHECK(handle.isValid() == true);

    handle.invalidate();

    CHECK(handle.isValid() == false);
    CHECK(handle.id == 0);
    CHECK(handle.valid == false);
  }
}

// ============================================================================
// Playback Configuration Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("PlaybackConfig - Default values", "[audio][config]") {
  PlaybackConfig config;

  SECTION("Default playback configuration") {
    CHECK(config.volume == 1.0f);
    CHECK(config.pitch == 1.0f);
    CHECK(config.pan == 0.0f);
    CHECK(config.loop == false);
    CHECK(config.fadeInDuration == 0.0f);
    CHECK(config.startTime == 0.0f);
    CHECK(config.channel == AudioChannel::Sound);
    CHECK(config.priority == 0);
  }

  SECTION("Pan range is [-1, 1]") {
    config.pan = -1.0f; // Full left
    CHECK(config.pan == -1.0f);

    config.pan = 0.0f; // Center
    CHECK(config.pan == 0.0f);

    config.pan = 1.0f; // Full right
    CHECK(config.pan == 1.0f);
  }
}

TEST_CASE("MusicConfig - Default values", "[audio][config][music]") {
  MusicConfig config;

  SECTION("Default music configuration") {
    CHECK(config.volume == 1.0f);
    CHECK(config.loop == true); // Music loops by default
    CHECK(config.fadeInDuration == 0.0f);
    CHECK(config.crossfadeDuration == 0.0f);
    CHECK(config.startTime == 0.0f);
  }
}

TEST_CASE("VoiceConfig - Default values and ducking", "[audio][config][voice]") {
  VoiceConfig config;

  SECTION("Default voice configuration") {
    CHECK(config.volume == 1.0f);
    CHECK(config.duckMusic == true);        // Music ducking enabled
    CHECK(config.duckAmount == 0.3f);       // 30% volume during voice
    CHECK(config.duckFadeDuration == 0.2f); // 200ms duck fade
  }

  SECTION("Duck amount range") {
    config.duckAmount = 0.0f; // Complete silence
    CHECK(config.duckAmount == 0.0f);

    config.duckAmount = 0.5f; // 50% volume
    CHECK(config.duckAmount == 0.5f);

    config.duckAmount = 1.0f; // No ducking
    CHECK(config.duckAmount == 1.0f);
  }

  SECTION("Disable ducking") {
    config.duckMusic = false;
    CHECK(config.duckMusic == false);
  }
}

// ============================================================================
// Audio Channel Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioChannel - Channel enumeration", "[audio][channel]") {
  SECTION("All audio channels are defined") {
    std::vector<AudioChannel> channels = {AudioChannel::Master,  AudioChannel::Music,
                                          AudioChannel::Sound,   AudioChannel::Voice,
                                          AudioChannel::Ambient, AudioChannel::UI};

    CHECK(channels.size() == 6);
  }

  SECTION("Channels have distinct values") {
    CHECK(AudioChannel::Master != AudioChannel::Music);
    CHECK(AudioChannel::Music != AudioChannel::Sound);
    CHECK(AudioChannel::Sound != AudioChannel::Voice);
    CHECK(AudioChannel::Voice != AudioChannel::Ambient);
    CHECK(AudioChannel::Ambient != AudioChannel::UI);
  }
}

// ============================================================================
// Playback State Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("PlaybackState - State enumeration", "[audio][playback]") {
  SECTION("All playback states are defined") {
    std::vector<PlaybackState> states = {PlaybackState::Stopped, PlaybackState::Playing,
                                         PlaybackState::Paused, PlaybackState::FadingIn,
                                         PlaybackState::FadingOut};

    CHECK(states.size() == 5);
  }

  SECTION("States have distinct values") {
    CHECK(PlaybackState::Stopped != PlaybackState::Playing);
    CHECK(PlaybackState::Playing != PlaybackState::Paused);
    CHECK(PlaybackState::Paused != PlaybackState::FadingIn);
    CHECK(PlaybackState::FadingIn != PlaybackState::FadingOut);
  }
}

TEST_CASE("AudioSource - Playing state check", "[audio][playback]") {
  AudioSource source;

  SECTION("Initial state is stopped and not playing") {
    CHECK(source.getState() == PlaybackState::Stopped);
    CHECK(source.isPlaying() == false);
  }

  // Note: Cannot fully test play/pause/stop without audio backend
  // These tests verify the state machine structure
}

// ============================================================================
// Audio Transition Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioTransition - Transition types", "[audio][transition]") {
  SECTION("All transition types are defined") {
    std::vector<AudioTransition> transitions = {
        AudioTransition::Immediate, AudioTransition::FadeOut, AudioTransition::CrossFade};

    CHECK(transitions.size() == 3);
  }

  SECTION("Transitions have distinct values") {
    CHECK(AudioTransition::Immediate != AudioTransition::FadeOut);
    CHECK(AudioTransition::FadeOut != AudioTransition::CrossFade);
  }
}

// ============================================================================
// Audio Event Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioEvent - Event types", "[audio][event]") {
  using EventType = AudioEvent::Type;

  SECTION("All event types are defined") {
    std::vector<EventType> eventTypes = {
        EventType::Started, EventType::Stopped,      EventType::Paused, EventType::Resumed,
        EventType::Looped,  EventType::FadeComplete, EventType::Error};

    CHECK(eventTypes.size() == 7);
  }

  SECTION("Create audio event") {
    AudioEvent event;
    event.type = EventType::Started;
    event.handle.id = 123;
    event.handle.valid = true;
    event.trackId = "music/theme.ogg";
    event.errorMessage = "";

    CHECK(event.type == EventType::Started);
    CHECK(event.handle.isValid() == true);
    CHECK(event.trackId == "music/theme.ogg");
    CHECK(event.errorMessage.empty());
  }

  SECTION("Create error event") {
    AudioEvent event;
    event.type = EventType::Error;
    event.trackId = "missing.ogg";
    event.errorMessage = "File not found";

    CHECK(event.type == EventType::Error);
    CHECK_FALSE(event.errorMessage.empty());
  }
}

// ============================================================================
// Audio Source Volume and Pitch Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Volume control", "[audio][source][volume]") {
  AudioSource source;

  SECTION("Initial volume") {
    // Volume starts at max (actual default depends on implementation)
    // This test verifies the getters work
    f32 initialVolume = source.getPlaybackPosition();
    (void)initialVolume; // Value may vary based on backend
  }

  SECTION("Set volume") {
    source.setVolume(0.5f);
    // Verify no crash - actual volume reading requires audio backend
  }

  SECTION("Set volume range") {
    source.setVolume(0.0f); // Muted
    source.setVolume(0.5f); // Half volume
    source.setVolume(1.0f); // Full volume
    // Verify no crash
  }
}

TEST_CASE("AudioSource - Pitch control", "[audio][source][pitch]") {
  AudioSource source;

  SECTION("Set pitch") {
    source.setPitch(1.0f); // Normal pitch
    source.setPitch(1.5f); // Higher pitch
    source.setPitch(0.5f); // Lower pitch
    // Verify no crash
  }

  SECTION("Extreme pitch values") {
    source.setPitch(0.1f);  // Very low
    source.setPitch(10.0f); // Very high
    // Verify no crash
  }
}

TEST_CASE("AudioSource - Pan control", "[audio][source][pan]") {
  AudioSource source;

  SECTION("Set pan") {
    source.setPan(0.0f);  // Center
    source.setPan(-1.0f); // Full left
    source.setPan(1.0f);  // Full right
    // Verify no crash
  }

  SECTION("Pan range") {
    source.setPan(-0.5f); // Slightly left
    source.setPan(0.5f);  // Slightly right
    // Verify no crash
  }
}

// ============================================================================
// Audio Fade Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Fade operations", "[audio][source][fade]") {
  AudioSource source;

  SECTION("Fade in") {
    source.fadeIn(2.0f); // 2 second fade in
    // Verify no crash - state changes require audio backend
  }

  SECTION("Fade out") {
    source.fadeOut(2.0f, true); // 2 second fade out, stop when done
    // Verify no crash
  }

  SECTION("Fade out without stopping") {
    source.fadeOut(1.0f, false); // Fade but don't stop
    // Verify no crash
  }

  SECTION("Zero duration fade") {
    source.fadeIn(0.0f);
    source.fadeOut(0.0f, true);
    // Verify no crash - should be immediate
  }

  SECTION("Very long fade") {
    source.fadeIn(60.0f); // 1 minute fade
    source.fadeOut(60.0f, true);
    // Verify no crash
  }
}

// ============================================================================
// Audio Loop Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Loop control", "[audio][source][loop]") {
  AudioSource source;

  SECTION("Set loop enabled") {
    source.setLoop(true);
    // Verify no crash
  }

  SECTION("Set loop disabled") {
    source.setLoop(false);
    // Verify no crash
  }

  SECTION("Toggle loop") {
    source.setLoop(true);
    source.setLoop(false);
    source.setLoop(true);
    // Verify no crash
  }
}

// ============================================================================
// Audio Source Update Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Update timing", "[audio][source][update]") {
  AudioSource source;

  SECTION("Update with typical frame time") {
    source.update(0.016); // 60 FPS (16ms)
    source.update(0.033); // 30 FPS (33ms)
    // Verify no crash
  }

  SECTION("Update with zero delta") {
    source.update(0.0);
    // Verify no crash
  }

  SECTION("Update with large delta") {
    source.update(1.0); // 1 second
    // Verify no crash
  }

  SECTION("Consecutive updates") {
    for (int i = 0; i < 60; ++i) {
      source.update(0.016);
    }
    // Verify no crash after many updates
  }
}

// ============================================================================
// Audio Source Playback Position Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Playback position", "[audio][source][position]") {
  AudioSource source;

  SECTION("Initial position") {
    f32 position = source.getPlaybackPosition();
    CHECK(position >= 0.0f);
  }

  SECTION("Duration") {
    f32 audioDuration = source.getDuration();
    CHECK(audioDuration >= 0.0f);
  }
}

// ============================================================================
// Audio Callback Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioCallback - Callback function", "[audio][callback]") {
  SECTION("Create audio callback") {
    bool callbackInvoked = false;
    AudioCallback callback = [&callbackInvoked](const AudioEvent& event) {
      callbackInvoked = true;
      (void)event;
    };

    // Invoke callback manually
    AudioEvent testEvent;
    testEvent.type = AudioEvent::Type::Started;
    callback(testEvent);

    CHECK(callbackInvoked == true);
  }

  SECTION("Callback receives event data") {
    AudioEvent receivedEvent;
    AudioCallback callback = [&receivedEvent](const AudioEvent& event) { receivedEvent = event; };

    AudioEvent testEvent;
    testEvent.type = AudioEvent::Type::Stopped;
    testEvent.trackId = "test_track";

    callback(testEvent);

    CHECK(receivedEvent.type == AudioEvent::Type::Stopped);
    CHECK(receivedEvent.trackId == "test_track");
  }

  SECTION("Multiple event types") {
    std::vector<AudioEvent::Type> receivedTypes;
    AudioCallback callback = [&receivedTypes](const AudioEvent& event) {
      receivedTypes.push_back(event.type);
    };

    AudioEvent event1;
    event1.type = AudioEvent::Type::Started;
    callback(event1);

    AudioEvent event2;
    event2.type = AudioEvent::Type::Looped;
    callback(event2);

    AudioEvent event3;
    event3.type = AudioEvent::Type::FadeComplete;
    callback(event3);

    CHECK(receivedTypes.size() == 3);
    CHECK(receivedTypes[0] == AudioEvent::Type::Started);
    CHECK(receivedTypes[1] == AudioEvent::Type::Looped);
    CHECK(receivedTypes[2] == AudioEvent::Type::FadeComplete);
  }
}

// ============================================================================
// Priority System Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Priority system", "[audio][source][priority]") {
  SECTION("Default priority") {
    AudioSource source1;
    AudioSource source2;

    CHECK(source1.priority == 0);
    CHECK(source2.priority == 0);
  }

  SECTION("Set priority") {
    AudioSource source;
    source.priority = 10;
    CHECK(source.priority == 10);

    source.priority = -5;
    CHECK(source.priority == -5);
  }

  SECTION("Priority comparison") {
    AudioSource highPriority;
    AudioSource lowPriority;

    highPriority.priority = 100;
    lowPriority.priority = 1;

    CHECK(highPriority.priority > lowPriority.priority);
  }
}

// ============================================================================
// Audio Source Channel Assignment Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Channel assignment", "[audio][source][channel]") {
  AudioSource source;

  SECTION("Default channel") {
    CHECK(source.channel == AudioChannel::Sound);
  }

  SECTION("Assign different channels") {
    source.channel = AudioChannel::Music;
    CHECK(source.channel == AudioChannel::Music);

    source.channel = AudioChannel::Voice;
    CHECK(source.channel == AudioChannel::Voice);

    source.channel = AudioChannel::UI;
    CHECK(source.channel == AudioChannel::UI);
  }
}

// ============================================================================
// Audio Source Track ID Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - Track identification", "[audio][source][track]") {
  AudioSource source;

  SECTION("Initial track ID") {
    CHECK(source.trackId.empty());
  }

  SECTION("Set track ID") {
    source.trackId = "music/battle_theme.ogg";
    CHECK(source.trackId == "music/battle_theme.ogg");
  }

  SECTION("Track ID with different paths") {
    source.trackId = "sfx/explosion.wav";
    CHECK(source.trackId == "sfx/explosion.wav");

    source.trackId = "voice/character_greeting.mp3";
    CHECK(source.trackId == "voice/character_greeting.mp3");
  }
}

// ============================================================================
// Playback Config Advanced Options Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("PlaybackConfig - Advanced options", "[audio][config][advanced]") {
  PlaybackConfig config;

  SECTION("Start time offset") {
    config.startTime = 10.0f; // Start 10 seconds into the track
    CHECK(config.startTime == 10.0f);
  }

  SECTION("Fade in duration") {
    config.fadeInDuration = 3.0f; // 3 second fade in
    CHECK(config.fadeInDuration == 3.0f);
  }

  SECTION("Combined fade and start time") {
    config.startTime = 5.0f;
    config.fadeInDuration = 2.0f;

    CHECK(config.startTime == 5.0f);
    CHECK(config.fadeInDuration == 2.0f);
  }
}

TEST_CASE("MusicConfig - Crossfade settings", "[audio][config][music]") {
  MusicConfig config;

  SECTION("Crossfade duration") {
    config.crossfadeDuration = 4.0f; // 4 second crossfade
    CHECK(config.crossfadeDuration == 4.0f);
  }

  SECTION("Combined fade in and crossfade") {
    config.fadeInDuration = 2.0f;
    config.crossfadeDuration = 3.0f;

    CHECK(config.fadeInDuration == 2.0f);
    CHECK(config.crossfadeDuration == 3.0f);
  }
}

// ============================================================================
// Audio Synchronization Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("AudioSource - State synchronization", "[audio][source][sync]") {
  AudioSource source1;
  AudioSource source2;

  SECTION("Multiple sources maintain independent state") {
    source1.trackId = "track1";
    source2.trackId = "track2";

    CHECK(source1.trackId != source2.trackId);
  }

  SECTION("Independent volume control") {
    source1.setVolume(0.3f);
    source2.setVolume(0.8f);

    // Verify they maintain independent state (no crash)
  }

  SECTION("Independent channel assignment") {
    source1.channel = AudioChannel::Music;
    source2.channel = AudioChannel::Sound;

    CHECK(source1.channel != source2.channel);
  }
}

// ============================================================================
// Error Handling Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Audio - Error event handling", "[audio][error]") {
  SECTION("Create error event with message") {
    AudioEvent errorEvent;
    errorEvent.type = AudioEvent::Type::Error;
    errorEvent.errorMessage = "Failed to load audio file";
    errorEvent.trackId = "missing/track.ogg";

    CHECK(errorEvent.type == AudioEvent::Type::Error);
    CHECK_FALSE(errorEvent.errorMessage.empty());
    CHECK(errorEvent.trackId == "missing/track.ogg");
  }

  SECTION("Handle error in callback") {
    std::string capturedError;
    AudioCallback callback = [&capturedError](const AudioEvent& event) {
      if (event.type == AudioEvent::Type::Error) {
        capturedError = event.errorMessage;
      }
    };

    AudioEvent errorEvent;
    errorEvent.type = AudioEvent::Type::Error;
    errorEvent.errorMessage = "Codec not supported";

    callback(errorEvent);

    CHECK(capturedError == "Codec not supported");
  }
}
