#pragma once

/**
 * @file audio_manager.hpp
 * @brief Audio System 2.0 - Full-featured audio management
 *
 * Provides:
 * - Music playback with streaming
 * - Sound effects with pooling
 * - Voice playback for VN dialogue
 * - Volume groups and master control
 * - Audio transitions (fade in/out, crossfade)
 * - Auto-ducking (music dims during voice)
 * - 3D positioning (optional)
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ma_engine;
struct ma_sound;
struct ma_decoder;

namespace NovelMind::audio {

// Forward declarations
class AudioSource;
class AudioBuffer;

/**
 * @brief Custom deleter for ma_engine to ensure proper cleanup
 * Implementation in audio_manager.cpp to avoid incomplete type issues
 */
struct MaEngineDeleter {
  void operator()(ma_engine* engine) const;
};

/**
 * @brief Custom deleter for ma_decoder to ensure proper cleanup
 * Implementation in audio_manager.cpp to avoid incomplete type issues
 */
struct MaDecoderDeleter {
  void operator()(ma_decoder* decoder) const;
};

using MaEnginePtr = std::unique_ptr<ma_engine, MaEngineDeleter>;
using MaDecoderPtr = std::unique_ptr<ma_decoder, MaDecoderDeleter>;

/**
 * @brief Audio channel types for volume control
 */
enum class AudioChannel : u8 {
  Master,  // Overall volume
  Music,   // Background music
  Sound,   // Sound effects
  Voice,   // Character voice lines
  Ambient, // Environmental audio
  UI       // UI feedback sounds
};

/**
 * @brief Audio playback state
 */
enum class PlaybackState : u8 { Stopped, Playing, Paused, FadingIn, FadingOut };

/**
 * @brief Audio source handle for tracking active playback
 */
struct AudioHandle {
  u32 id = 0;
  bool valid = false;

  [[nodiscard]] bool isValid() const { return valid && id != 0; }
  void invalidate() {
    valid = false;
    id = 0;
  }
};

/**
 * @brief Configuration for audio playback
 */
struct PlaybackConfig {
  f32 volume = 1.0f;
  f32 pitch = 1.0f;
  f32 pan = 0.0f; // -1 = left, 0 = center, 1 = right
  bool loop = false;
  f32 fadeInDuration = 0.0f;
  f32 startTime = 0.0f; // Start position in seconds
  AudioChannel channel = AudioChannel::Sound;
  i32 priority = 0; // Higher = more important
};

/**
 * @brief Music playback configuration
 */
struct MusicConfig {
  f32 volume = 1.0f;
  bool loop = true;
  f32 fadeInDuration = 0.0f;
  f32 crossfadeDuration = 0.0f;
  f32 startTime = 0.0f;
};

/**
 * @brief Voice playback configuration
 */
struct VoiceConfig {
  f32 volume = 1.0f;
  bool duckMusic = true;       // Reduce music volume during voice
  f32 duckAmount = 0.3f;       // Music volume during voice (0.3 = 30%)
  f32 duckFadeDuration = 0.2f; // Fade time for ducking
};

/**
 * @brief Audio transition types
 */
enum class AudioTransition : u8 {
  Immediate, // Stop immediately
  FadeOut,   // Fade out then stop
  CrossFade  // Crossfade to new track
};

/**
 * @brief Audio event for callbacks
 *
 * IMPORTANT: AudioCallback may be invoked from background threads.
 * If you need to update UI elements, use thread-safe mechanisms:
 * - Qt: Use QMetaObject::invokeMethod(..., Qt::QueuedConnection)
 * - Other frameworks: Post to main thread event queue
 */
struct AudioEvent {
  enum class Type : u8 {
    Started,
    Stopped,
    Paused,
    Resumed,
    Looped,
    FadeComplete,
    Error
  };

  Type type;
  AudioHandle handle;
  std::string trackId;
  std::string errorMessage;
};

using AudioCallback = std::function<void(const AudioEvent &)>;

/**
 * @brief Internal audio source representation
 */
class AudioSource {
public:
  AudioSource();
  ~AudioSource();

  void play();
  void pause();
  void stop();
  void update(f64 deltaTime);

  void setVolume(f32 volume);
  void setPitch(f32 pitch);
  void setPan(f32 pan);
  void setLoop(bool loop);

  void fadeIn(f32 duration);
  void fadeOut(f32 duration, bool stopWhenDone = true);

  [[nodiscard]] PlaybackState getState() const { return m_state; }
  [[nodiscard]] f32 getPlaybackPosition() const { return m_position; }
  [[nodiscard]] f32 getDuration() const { return m_duration; }
  [[nodiscard]] bool isPlaying() const {
    return m_state == PlaybackState::Playing ||
           m_state == PlaybackState::FadingIn ||
           m_state == PlaybackState::FadingOut;
  }

  AudioHandle handle;
  std::string trackId;
  AudioChannel channel = AudioChannel::Sound;
  i32 priority = 0;

private:
  friend class AudioManager;

  std::atomic<PlaybackState> m_state{PlaybackState::Stopped};
  f32 m_volume = 1.0f;
  f32 m_targetVolume = 1.0f;
  f32 m_pitch = 1.0f;
  f32 m_pan = 0.0f;
  bool m_loop = false;

  f32 m_position = 0.0f;
  f32 m_duration = 0.0f;

  f32 m_fadeTimer = 0.0f;
  f32 m_fadeDuration = 0.0f;
  f32 m_fadeStartVolume = 0.0f;
  f32 m_fadeTargetVolume = 0.0f;
  bool m_stopAfterFade = false;

  std::unique_ptr<ma_sound> m_sound;
  bool m_soundReady = false;
  std::vector<u8> m_memoryData;
  MaDecoderPtr m_decoder;
  bool m_decoderReady = false;
};

/**
 * @brief Audio Manager 2.0 - Central audio management
 */
class AudioManager {
public:
  using DataProvider =
      std::function<Result<std::vector<u8>>(const std::string &id)>;
  AudioManager();
  ~AudioManager();

  /**
   * @brief Initialize the audio system
   */
  Result<void> initialize();

  /**
   * @brief Shutdown the audio system
   */
  void shutdown();

  /**
   * @brief Update audio state (call each frame)
   */
  void update(f64 deltaTime);

  // =========================================================================
  // Sound Effects
  // =========================================================================

  /**
   * @brief Play a sound effect
   */
  AudioHandle playSound(const std::string &id,
                        const PlaybackConfig &config = {});

  /**
   * @brief Play a sound effect with simple parameters
   */
  AudioHandle playSound(const std::string &id, f32 volume, bool loop = false);

  /**
   * @brief Stop a specific sound
   */
  void stopSound(AudioHandle handle, f32 fadeDuration = 0.0f);

  /**
   * @brief Stop all sound effects
   */
  void stopAllSounds(f32 fadeDuration = 0.0f);

  // =========================================================================
  // Music
  // =========================================================================

  /**
   * @brief Play background music
   */
  AudioHandle playMusic(const std::string &id, const MusicConfig &config = {});

  /**
   * @brief Play music with crossfade from current track
   */
  AudioHandle crossfadeMusic(const std::string &id, f32 duration,
                             const MusicConfig &config = {});

  /**
   * @brief Stop music
   */
  void stopMusic(f32 fadeDuration = 0.0f);

  /**
   * @brief Pause music
   */
  void pauseMusic();

  /**
   * @brief Resume music
   */
  void resumeMusic();

  /**
   * @brief Check if music is playing
   */
  [[nodiscard]] bool isMusicPlaying() const;

  /**
   * @brief Get current music track ID
   */
  [[nodiscard]] const std::string &getCurrentMusicId() const;

  /**
   * @brief Get music playback position
   */
  [[nodiscard]] f32 getMusicPosition() const;

  /**
   * @brief Seek music to position
   */
  void seekMusic(f32 position);

  // =========================================================================
  // Voice (VN-specific)
  // =========================================================================

  /**
   * @brief Play voice line (auto-ducks music)
   */
  AudioHandle playVoice(const std::string &id, const VoiceConfig &config = {});

  /**
   * @brief Stop voice playback
   */
  void stopVoice(f32 fadeDuration = 0.0f);

  /**
   * @brief Check if voice is playing
   */
  [[nodiscard]] bool isVoicePlaying() const;

  /**
   * @brief Skip current voice line
   */
  void skipVoice();

  // =========================================================================
  // Volume Control
  // =========================================================================

  /**
   * @brief Set volume for a channel
   */
  void setChannelVolume(AudioChannel channel, f32 volume);

  /**
   * @brief Get volume for a channel
   */
  [[nodiscard]] f32 getChannelVolume(AudioChannel channel) const;

  /**
   * @brief Set master volume
   */
  void setMasterVolume(f32 volume);

  /**
   * @brief Get master volume
   */
  [[nodiscard]] f32 getMasterVolume() const;

  /**
   * @brief Mute/unmute a channel
   */
  void setChannelMuted(AudioChannel channel, bool muted);

  /**
   * @brief Check if channel is muted
   */
  [[nodiscard]] bool isChannelMuted(AudioChannel channel) const;

  /**
   * @brief Mute all audio
   */
  void muteAll();

  /**
   * @brief Unmute all audio
   */
  void unmuteAll();

  // =========================================================================
  // Global Transitions
  // =========================================================================

  /**
   * @brief Fade all audio
   */
  void fadeAllTo(f32 targetVolume, f32 duration);

  /**
   * @brief Pause all audio
   */
  void pauseAll();

  /**
   * @brief Resume all audio
   */
  void resumeAll();

  /**
   * @brief Stop all audio
   */
  void stopAll(f32 fadeDuration = 0.0f);

  // =========================================================================
  // Source Management
  // =========================================================================

  /**
   * @brief Get source by handle
   */
  AudioSource *getSource(AudioHandle handle);

  /**
   * @brief Check if handle is still valid and playing
   */
  [[nodiscard]] bool isPlaying(AudioHandle handle) const;

  /**
   * @brief Get all active sources
   */
  [[nodiscard]] std::vector<AudioHandle> getActiveSources() const;

  /**
   * @brief Get number of active sources
   */
  [[nodiscard]] size_t getActiveSourceCount() const;

  // =========================================================================
  // Callbacks
  // =========================================================================

  /**
   * @brief Set callback for audio events
   */
  void setEventCallback(AudioCallback callback);

  void setDataProvider(DataProvider provider);

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Set maximum concurrent sounds
   */
  void setMaxSounds(size_t max);

  /**
   * @brief Enable/disable auto-ducking
   */
  void setAutoDuckingEnabled(bool enabled);

  /**
   * @brief Set ducking parameters
   */
  void setDuckingParams(f32 duckVolume, f32 fadeDuration);

private:
  AudioHandle createSource(const std::string &trackId, AudioChannel channel);
  void releaseSource(AudioHandle handle);
  void fireEvent(AudioEvent::Type type, AudioHandle handle,
                 const std::string &trackId = "");

  void updateDucking(f64 deltaTime);
  f32 calculateEffectiveVolume(const AudioSource &source) const;

  bool m_initialized = false;
  MaEnginePtr m_engine;
  bool m_engineInitialized = false;

  // Channel volumes (protected by m_stateMutex)
  mutable std::mutex m_stateMutex;
  std::unordered_map<AudioChannel, f32> m_channelVolumes;
  std::unordered_map<AudioChannel, bool> m_channelMuted;
  std::atomic<bool> m_allMuted{false};

  // Active sources (protected by m_sourcesMutex for thread-safe access)
  mutable std::shared_mutex m_sourcesMutex;
  std::vector<std::unique_ptr<AudioSource>> m_sources;
  std::atomic<u32> m_nextHandleId{1};
  std::atomic<size_t> m_maxSounds{32};

  // Music state (protected by m_stateMutex)
  AudioHandle m_currentMusicHandle;
  AudioHandle m_crossfadeMusicHandle;
  std::string m_currentMusicId;

  // Voice state (protected by m_stateMutex)
  AudioHandle m_currentVoiceHandle;
  std::atomic<bool> m_voicePlaying{false};

  // Ducking state (atomics for real-time audio performance)
  std::atomic<bool> m_autoDuckingEnabled{true};
  std::atomic<f32> m_duckVolume{0.3f};
  std::atomic<f32> m_duckFadeDuration{0.2f};
  std::atomic<f32> m_currentDuckLevel{1.0f};
  std::atomic<f32> m_targetDuckLevel{1.0f};

  // Master fade (protected by m_stateMutex)
  f32 m_masterFadeVolume = 1.0f;
  f32 m_masterFadeStartVolume = 1.0f;
  f32 m_masterFadeTarget = 1.0f;
  f32 m_masterFadeTimer = 0.0f;
  f32 m_masterFadeDuration = 0.0f;

  // Callback (protected by m_stateMutex)
  AudioCallback m_eventCallback;
  DataProvider m_dataProvider;
};

} // namespace NovelMind::audio
