#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include "miniaudio/miniaudio.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace NovelMind::audio {

// ============================================================================
// Custom Deleters Implementation
// ============================================================================

void MaEngineDeleter::operator()(ma_engine* engine) const {
  if (engine) {
    ma_engine_uninit(engine);
    delete engine;
  }
}

void MaDecoderDeleter::operator()(ma_decoder* decoder) const {
  if (decoder) {
    ma_decoder_uninit(decoder);
    delete decoder;
  }
}

// ============================================================================
// AudioSource Implementation
// ============================================================================

AudioSource::AudioSource() = default;
AudioSource::~AudioSource() = default;

void AudioSource::play() {
  if (m_state == PlaybackState::Paused) {
    m_state = PlaybackState::Playing;
  } else if (m_state == PlaybackState::Stopped) {
    m_state = PlaybackState::Playing;
    m_position = 0.0f;
  }

  if (m_soundReady && m_sound) {
    ma_sound_start(m_sound.get());
  }
}

void AudioSource::pause() {
  if (m_state == PlaybackState::Playing || m_state == PlaybackState::FadingIn ||
      m_state == PlaybackState::FadingOut) {
    m_state = PlaybackState::Paused;
  }

  if (m_soundReady && m_sound) {
    ma_sound_stop(m_sound.get());
  }
}

void AudioSource::stop() {
  m_state = PlaybackState::Stopped;
  m_position = 0.0f;
  m_fadeTimer = 0.0f;

  if (m_soundReady && m_sound) {
    ma_sound_stop(m_sound.get());
    ma_sound_seek_to_pcm_frame(m_sound.get(), 0);
  }
}

void AudioSource::update(f64 deltaTime) {
  if (m_state == PlaybackState::Stopped || m_state == PlaybackState::Paused) {
    return;
  }

  // Update fading
  if (m_state == PlaybackState::FadingIn ||
      m_state == PlaybackState::FadingOut) {
    m_fadeTimer += static_cast<f32>(deltaTime);

    if (m_fadeTimer >= m_fadeDuration) {
      m_volume = m_fadeTargetVolume;

      if (m_state == PlaybackState::FadingOut && m_stopAfterFade) {
        stop();
        return;
      } else {
        m_state = PlaybackState::Playing;
      }
    } else {
      // Protect against division by zero
      if (m_fadeDuration > 0.0f) {
        f32 t = m_fadeTimer / m_fadeDuration;
        m_volume =
            m_fadeStartVolume + (m_fadeTargetVolume - m_fadeStartVolume) * t;
      } else {
        // Invalid fade duration - complete fade immediately
        NOVELMIND_LOG_WARN("AudioSource: Invalid fade duration ({}), completing fade immediately", m_fadeDuration);
        m_volume = m_fadeTargetVolume;
        m_state = PlaybackState::Playing;
      }
    }
  }

  // Update playback position
  if (m_soundReady && m_sound) {
    float cursorSeconds = 0.0f;
    float lengthSeconds = 0.0f;
    ma_sound_get_cursor_in_seconds(m_sound.get(), &cursorSeconds);
    ma_sound_get_length_in_seconds(m_sound.get(), &lengthSeconds);
    m_position = cursorSeconds;
    m_duration = lengthSeconds;
  } else {
    m_position += static_cast<f32>(deltaTime);
  }

  // Check for loop or end
  if (m_duration > 0.0f && m_position >= m_duration) {
    if (m_loop) {
      m_position = std::fmod(m_position, m_duration);
    } else {
      stop();
    }
  }
}

void AudioSource::setVolume(f32 volume) {
  m_volume = std::max(0.0f, std::min(1.0f, volume));
  m_targetVolume = m_volume;
}

void AudioSource::setPitch(f32 pitch) {
  m_pitch = std::max(0.1f, std::min(4.0f, pitch));
  if (m_soundReady && m_sound) {
    ma_sound_set_pitch(m_sound.get(), m_pitch);
  }
}

void AudioSource::setPan(f32 pan) {
  m_pan = std::max(-1.0f, std::min(1.0f, pan));
  if (m_soundReady && m_sound) {
    ma_sound_set_pan(m_sound.get(), m_pan);
  }
}

void AudioSource::setLoop(bool loop) { m_loop = loop; }

void AudioSource::fadeIn(f32 duration) {
  if (duration <= 0.0f) {
    m_volume = m_targetVolume;
    m_state = PlaybackState::Playing;
    return;
  }

  m_fadeStartVolume = 0.0f;
  m_fadeTargetVolume = m_targetVolume;
  m_fadeDuration = duration;
  m_fadeTimer = 0.0f;
  m_stopAfterFade = false;
  m_volume = 0.0f;
  m_state = PlaybackState::FadingIn;
}

void AudioSource::fadeOut(f32 duration, bool stopWhenDone) {
  if (duration <= 0.0f) {
    if (stopWhenDone) {
      stop();
    } else {
      m_volume = 0.0f;
    }
    return;
  }

  m_fadeStartVolume = m_volume;
  m_fadeTargetVolume = 0.0f;
  m_fadeDuration = duration;
  m_fadeTimer = 0.0f;
  m_stopAfterFade = stopWhenDone;
  m_state = PlaybackState::FadingOut;
}

// ============================================================================
// AudioManager Implementation
// ============================================================================

AudioManager::AudioManager() {
  // Initialize default channel volumes
  m_channelVolumes[AudioChannel::Master] = 1.0f;
  m_channelVolumes[AudioChannel::Music] = 0.8f;
  m_channelVolumes[AudioChannel::Sound] = 1.0f;
  m_channelVolumes[AudioChannel::Voice] = 1.0f;
  m_channelVolumes[AudioChannel::Ambient] = 0.7f;
  m_channelVolumes[AudioChannel::UI] = 0.8f;

  for (int i = 0; i < 6; ++i) {
    m_channelMuted[static_cast<AudioChannel>(i)] = false;
  }
}

AudioManager::~AudioManager() { shutdown(); }

Result<void> AudioManager::initialize() {
  if (m_initialized) {
    return Result<void>::ok();
  }

  auto engine = std::make_unique<ma_engine>();
  ma_engine_config config = ma_engine_config_init();
  if (ma_engine_init(&config, engine.get()) != MA_SUCCESS) {
    return Result<void>::error("Failed to initialize audio engine");
  }
  m_engine = MaEnginePtr(engine.release());
  m_engineInitialized = true;

  m_initialized = true;
  return Result<void>::ok();
}

void AudioManager::shutdown() {
  if (!m_initialized) {
    return;
  }

  stopAll(0.0f);

  {
    std::unique_lock<std::shared_mutex> lock(m_sourcesMutex);
    for (auto &source : m_sources) {
      if (source && source->m_soundReady && source->m_sound) {
        ma_sound_uninit(source->m_sound.get());
      }
    }
    m_sources.clear();
  }

  if (m_engineInitialized && m_engine) {
    m_engine.reset();
    m_engineInitialized = false;
  }

  m_initialized = false;
}

void AudioManager::update(f64 deltaTime) {
  if (!m_initialized) {
    return;
  }

  // Update master fade
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_masterFadeDuration > 0.0f) {
      m_masterFadeTimer += static_cast<f32>(deltaTime);
      if (m_masterFadeTimer >= m_masterFadeDuration) {
        m_masterFadeVolume = m_masterFadeTarget;
        m_masterFadeDuration = 0.0f;
      } else {
        f32 t = m_masterFadeTimer / m_masterFadeDuration;
        m_masterFadeVolume = m_masterFadeStartVolume +
                             (m_masterFadeTarget - m_masterFadeStartVolume) * t;
      }
    }
  }

  // Update ducking
  updateDucking(deltaTime);

  // Update all sources (use unique_lock for potential modification)
  {
    std::unique_lock<std::shared_mutex> lock(m_sourcesMutex);
    for (auto &source : m_sources) {
      if (source && source->isPlaying()) {
        if (source->m_soundReady && source->m_sound) {
          const f32 effective =
              source->m_volume * calculateEffectiveVolume(*source);
          ma_sound_set_volume(source->m_sound.get(), effective);
          ma_sound_set_pan(source->m_sound.get(), source->m_pan);
          ma_sound_set_pitch(source->m_sound.get(), source->m_pitch);
          ma_sound_set_looping(source->m_sound.get(), source->m_loop);
        }
        source->update(deltaTime);
      }
    }

    // Remove stopped sources (but keep some pooled)
    m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(),
                                   [](const auto &s) {
                                     return s &&
                                            s->getState() ==
                                                PlaybackState::Stopped &&
                                            s->channel != AudioChannel::Music;
                                   }),
                    m_sources.end());
  }

  // Check voice playback status
  if (m_voicePlaying.load(std::memory_order_acquire)) {
    AudioHandle voiceHandle;
    {
      std::lock_guard<std::mutex> lock(m_stateMutex);
      voiceHandle = m_currentVoiceHandle;
    }
    auto *voiceSource = getSource(voiceHandle);
    if (!voiceSource || !voiceSource->isPlaying()) {
      m_voicePlaying.store(false, std::memory_order_release);
      m_targetDuckLevel.store(1.0f, std::memory_order_release);
      fireEvent(AudioEvent::Type::Stopped, voiceHandle, "voice");
    }
  }
}

AudioHandle AudioManager::playSound(const std::string &id,
                                    const PlaybackConfig &config) {
  if (!m_initialized) {
    return {};
  }

  // Check source limit (need unique lock for potential modification)
  {
    std::unique_lock<std::shared_mutex> lock(m_sourcesMutex);
    if (m_sources.size() >= m_maxSounds.load(std::memory_order_relaxed)) {
      // Find and remove lowest priority sound
      auto lowest = std::min_element(m_sources.begin(), m_sources.end(),
                                     [](const auto &a, const auto &b) {
                                       return a->priority < b->priority;
                                     });

      if (lowest != m_sources.end() && (*lowest)->priority < config.priority) {
        m_sources.erase(lowest);
      } else {
        return {}; // Can't play
      }
    }
  }

  AudioHandle handle = createSource(id, config.channel);
  auto *source = getSource(handle);
  if (!source) {
    return {};
  }

  source->setVolume(config.volume);
  source->setPitch(config.pitch);
  source->setPan(config.pan);
  source->setLoop(config.loop);
  source->priority = config.priority;

  if (config.startTime > 0.0f && source->m_soundReady && source->m_sound &&
      m_engineInitialized && m_engine) {
    ma_uint32 sampleRate = ma_engine_get_sample_rate(m_engine.get());
    if (sampleRate > 0) {
      ma_uint64 startFrames = static_cast<ma_uint64>(
          config.startTime * static_cast<f32>(sampleRate));
      ma_sound_seek_to_pcm_frame(source->m_sound.get(), startFrames);
    }
  }

  if (config.fadeInDuration > 0.0f) {
    source->fadeIn(config.fadeInDuration);
  } else {
    source->play();
  }

  fireEvent(AudioEvent::Type::Started, handle, id);
  return handle;
}

AudioHandle AudioManager::playSound(const std::string &id, f32 volume,
                                    bool loop) {
  PlaybackConfig config;
  config.volume = volume;
  config.loop = loop;
  return playSound(id, config);
}

void AudioManager::stopSound(AudioHandle handle, f32 fadeDuration) {
  auto *source = getSource(handle);
  if (source) {
    if (fadeDuration > 0.0f) {
      source->fadeOut(fadeDuration, true);
    } else {
      source->stop();
      fireEvent(AudioEvent::Type::Stopped, handle, source->trackId);
    }
  }
}

void AudioManager::stopAllSounds(f32 fadeDuration) {
  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  for (auto &source : m_sources) {
    if (source && source->channel == AudioChannel::Sound) {
      if (fadeDuration > 0.0f) {
        source->fadeOut(fadeDuration, true);
      } else {
        source->stop();
      }
    }
  }
}

AudioHandle AudioManager::playMusic(const std::string &id,
                                    const MusicConfig &config) {
  if (!m_initialized) {
    return {};
  }

  // Stop current music
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_currentMusicHandle.isValid()) {
      stopMusic(0.0f);
    }
  }

  AudioHandle handle = createSource(id, AudioChannel::Music);
  auto *source = getSource(handle);
  if (!source) {
    return {};
  }

  source->setVolume(config.volume);
  source->setLoop(config.loop);

  if (config.startTime > 0.0f && source->m_soundReady && source->m_sound &&
      m_engineInitialized && m_engine) {
    ma_uint32 sampleRate = ma_engine_get_sample_rate(m_engine.get());
    if (sampleRate > 0) {
      ma_uint64 startFrames = static_cast<ma_uint64>(
          config.startTime * static_cast<f32>(sampleRate));
      ma_sound_seek_to_pcm_frame(source->m_sound.get(), startFrames);
    }
  }

  if (config.fadeInDuration > 0.0f) {
    source->fadeIn(config.fadeInDuration);
  } else {
    source->play();
  }

  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_currentMusicHandle = handle;
    m_currentMusicId = id;
  }

  fireEvent(AudioEvent::Type::Started, handle, id);
  return handle;
}

AudioHandle AudioManager::crossfadeMusic(const std::string &id, f32 duration,
                                         const MusicConfig &config) {
  if (!m_initialized) {
    return {};
  }

  // Fade out current music
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_currentMusicHandle.isValid()) {
      auto *current = getSource(m_currentMusicHandle);
      if (current) {
        current->fadeOut(duration, true);
      }
    }
  }

  // Start new music with fade in
  MusicConfig newConfig = config;
  newConfig.fadeInDuration = duration;
  return playMusic(id, newConfig);
}

void AudioManager::stopMusic(f32 fadeDuration) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  if (!m_currentMusicHandle.isValid()) {
    return;
  }

  auto *source = getSource(m_currentMusicHandle);
  if (source) {
    if (fadeDuration > 0.0f) {
      source->fadeOut(fadeDuration, true);
    } else {
      source->stop();
      fireEvent(AudioEvent::Type::Stopped, m_currentMusicHandle,
                m_currentMusicId);
    }
  }

  m_currentMusicHandle.invalidate();
  m_currentMusicId.clear();
}

void AudioManager::pauseMusic() {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  auto *source = getSource(m_currentMusicHandle);
  if (source) {
    source->pause();
    fireEvent(AudioEvent::Type::Paused, m_currentMusicHandle, m_currentMusicId);
  }
}

void AudioManager::resumeMusic() {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  auto *source = getSource(m_currentMusicHandle);
  if (source) {
    source->play();
    fireEvent(AudioEvent::Type::Resumed, m_currentMusicHandle,
              m_currentMusicId);
  }
}

bool AudioManager::isMusicPlaying() const {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  if (!m_currentMusicHandle.isValid()) {
    return false;
  }

  std::shared_lock<std::shared_mutex> sourceLock(m_sourcesMutex);
  for (const auto &source : m_sources) {
    if (source && source->handle.id == m_currentMusicHandle.id) {
      return source->isPlaying();
    }
  }
  return false;
}

const std::string &AudioManager::getCurrentMusicId() const {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  return m_currentMusicId;
}

f32 AudioManager::getMusicPosition() const {
  AudioHandle handle;
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    handle = m_currentMusicHandle;
  }

  std::shared_lock<std::shared_mutex> sourceLock(m_sourcesMutex);
  for (const auto &source : m_sources) {
    if (source && source->handle.id == handle.id) {
      return source->getPlaybackPosition();
    }
  }
  return 0.0f;
}

void AudioManager::seekMusic(f32 position) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  auto *source = getSource(m_currentMusicHandle);
  if (!source || !source->m_soundReady || !source->m_sound) {
    return;
  }

  if (!m_engineInitialized || !m_engine) {
    return;
  }
  ma_uint32 sampleRate = ma_engine_get_sample_rate(m_engine.get());
  if (sampleRate == 0) {
    return;
  }

  // Clamp position to valid range
  f32 clampedPosition = position;
  if (source->m_duration > 0.0f) {
    clampedPosition = std::max(0.0f, std::min(position, source->m_duration));
  } else {
    clampedPosition = std::max(0.0f, position);
  }

  ma_uint64 targetFrames =
      static_cast<ma_uint64>(clampedPosition * static_cast<f32>(sampleRate));
  ma_sound_seek_to_pcm_frame(source->m_sound.get(), targetFrames);

  // Update the source's position to match the seek
  source->m_position = clampedPosition;
}

AudioHandle AudioManager::playVoice(const std::string &id,
                                    const VoiceConfig &config) {
  if (!m_initialized) {
    return {};
  }

  // Stop current voice
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_currentVoiceHandle.isValid()) {
      stopVoice(0.0f);
    }
  }

  AudioHandle handle = createSource(id, AudioChannel::Voice);
  auto *source = getSource(handle);
  if (!source) {
    return {};
  }

  source->setVolume(config.volume);
  source->setLoop(false);
  source->play();

  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_currentVoiceHandle = handle;
  }
  m_voicePlaying.store(true, std::memory_order_release);

  // Apply ducking with validation
  if (config.duckMusic && m_autoDuckingEnabled.load(std::memory_order_relaxed)) {
    // Validate and clamp duck parameters to prevent division by zero
    constexpr f32 MIN_FADE_DURATION = 0.001f;
    f32 duckAmount = std::max(0.0f, std::min(1.0f, config.duckAmount));
    f32 duckFadeDuration = config.duckFadeDuration;

    if (duckFadeDuration < 0.0f) {
      NOVELMIND_LOG_WARN("AudioManager: Invalid negative duck fade duration in VoiceConfig ({}), using 0", duckFadeDuration);
      duckFadeDuration = 0.0f;
    } else if (duckFadeDuration > 0.0f && duckFadeDuration < MIN_FADE_DURATION) {
      NOVELMIND_LOG_WARN("AudioManager: Duck fade duration too small in VoiceConfig ({}), clamping to minimum ({})",
                         duckFadeDuration, MIN_FADE_DURATION);
      duckFadeDuration = MIN_FADE_DURATION;
    }

    m_duckVolume.store(duckAmount, std::memory_order_relaxed);
    m_duckFadeDuration.store(duckFadeDuration, std::memory_order_relaxed);
    m_targetDuckLevel.store(duckAmount, std::memory_order_release);
  }

  fireEvent(AudioEvent::Type::Started, handle, id);
  return handle;
}

void AudioManager::stopVoice(f32 fadeDuration) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  if (!m_currentVoiceHandle.isValid()) {
    return;
  }

  auto *source = getSource(m_currentVoiceHandle);
  if (source) {
    if (fadeDuration > 0.0f) {
      source->fadeOut(fadeDuration, true);
    } else {
      source->stop();
    }
  }

  m_voicePlaying.store(false, std::memory_order_release);
  m_targetDuckLevel.store(1.0f, std::memory_order_release);
  m_currentVoiceHandle.invalidate();
}

bool AudioManager::isVoicePlaying() const {
  return m_voicePlaying.load(std::memory_order_acquire);
}

void AudioManager::skipVoice() { stopVoice(0.0f); }

void AudioManager::setChannelVolume(AudioChannel channel, f32 volume) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  m_channelVolumes[channel] = std::max(0.0f, std::min(1.0f, volume));
}

f32 AudioManager::getChannelVolume(AudioChannel channel) const {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  auto it = m_channelVolumes.find(channel);
  return (it != m_channelVolumes.end()) ? it->second : 1.0f;
}

void AudioManager::setMasterVolume(f32 volume) {
  setChannelVolume(AudioChannel::Master, volume);
}

f32 AudioManager::getMasterVolume() const {
  return getChannelVolume(AudioChannel::Master);
}

void AudioManager::setChannelMuted(AudioChannel channel, bool muted) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  m_channelMuted[channel] = muted;
}

bool AudioManager::isChannelMuted(AudioChannel channel) const {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  auto it = m_channelMuted.find(channel);
  return (it != m_channelMuted.end()) ? it->second : false;
}

void AudioManager::muteAll() {
  m_allMuted.store(true, std::memory_order_release);
}

void AudioManager::unmuteAll() {
  m_allMuted.store(false, std::memory_order_release);
}

void AudioManager::fadeAllTo(f32 targetVolume, f32 duration) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  m_masterFadeTarget = std::max(0.0f, std::min(1.0f, targetVolume));
  m_masterFadeStartVolume = m_masterFadeVolume;
  m_masterFadeDuration = duration;
  m_masterFadeTimer = 0.0f;
}

void AudioManager::pauseAll() {
  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  for (auto &source : m_sources) {
    if (source && source->isPlaying()) {
      source->pause();
    }
  }
}

void AudioManager::resumeAll() {
  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  for (auto &source : m_sources) {
    if (source && source->getState() == PlaybackState::Paused) {
      source->play();
    }
  }
}

void AudioManager::stopAll(f32 fadeDuration) {
  {
    std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
    for (auto &source : m_sources) {
      if (source && source->isPlaying()) {
        if (fadeDuration > 0.0f) {
          source->fadeOut(fadeDuration, true);
        } else {
          source->stop();
        }
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_currentMusicHandle.invalidate();
    m_currentMusicId.clear();
    m_currentVoiceHandle.invalidate();
  }
  m_voicePlaying.store(false, std::memory_order_release);
}

AudioSource *AudioManager::getSource(AudioHandle handle) {
  if (!handle.isValid()) {
    return nullptr;
  }

  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  for (auto &source : m_sources) {
    if (source && source->handle.id == handle.id) {
      return source.get();
    }
  }
  return nullptr;
}

bool AudioManager::isPlaying(AudioHandle handle) const {
  if (!handle.isValid()) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  for (const auto &source : m_sources) {
    if (source && source->handle.id == handle.id) {
      return source->isPlaying();
    }
  }
  return false;
}

std::vector<AudioHandle> AudioManager::getActiveSources() const {
  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  std::vector<AudioHandle> handles;
  for (const auto &source : m_sources) {
    if (source && source->isPlaying()) {
      handles.push_back(source->handle);
    }
  }
  return handles;
}

size_t AudioManager::getActiveSourceCount() const {
  std::shared_lock<std::shared_mutex> lock(m_sourcesMutex);
  return static_cast<size_t>(
      std::count_if(m_sources.begin(), m_sources.end(),
                    [](const auto &s) { return s && s->isPlaying(); }));
}

void AudioManager::setEventCallback(AudioCallback callback) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  m_eventCallback = std::move(callback);
}

void AudioManager::setDataProvider(DataProvider provider) {
  std::lock_guard<std::mutex> lock(m_stateMutex);
  m_dataProvider = std::move(provider);
}

void AudioManager::setMaxSounds(size_t max) {
  m_maxSounds.store(max, std::memory_order_relaxed);
}

void AudioManager::setAutoDuckingEnabled(bool enabled) {
  m_autoDuckingEnabled.store(enabled, std::memory_order_relaxed);
  if (!enabled) {
    m_targetDuckLevel.store(1.0f, std::memory_order_release);
  }
}

void AudioManager::setDuckingParams(f32 duckVolume, f32 fadeDuration) {
  // Clamp duck volume to valid range [0, 1]
  f32 clampedDuckVolume = std::max(0.0f, std::min(1.0f, duckVolume));

  // Validate and clamp fade duration - must be positive or zero
  // Use minimum threshold to prevent division by zero
  constexpr f32 MIN_FADE_DURATION = 0.001f;
  f32 clampedFadeDuration = fadeDuration;

  if (fadeDuration < 0.0f) {
    NOVELMIND_LOG_WARN("AudioManager: Invalid negative duck fade duration ({}), clamping to 0", fadeDuration);
    clampedFadeDuration = 0.0f;
  } else if (fadeDuration > 0.0f && fadeDuration < MIN_FADE_DURATION) {
    NOVELMIND_LOG_WARN("AudioManager: Duck fade duration too small ({}), clamping to minimum ({})",
                       fadeDuration, MIN_FADE_DURATION);
    clampedFadeDuration = MIN_FADE_DURATION;
  }

  m_duckVolume.store(clampedDuckVolume, std::memory_order_relaxed);
  m_duckFadeDuration.store(clampedFadeDuration, std::memory_order_relaxed);
}

AudioHandle AudioManager::createSource(const std::string &trackId,
                                       AudioChannel channel) {
  if (!m_engineInitialized || !m_engine) {
    return {};
  }
  auto source = std::make_unique<AudioSource>();

  AudioHandle handle;
  handle.id = m_nextHandleId.fetch_add(1, std::memory_order_relaxed);
  handle.valid = true;

  source->handle = handle;
  source->trackId = trackId;
  source->channel = channel;

  auto sound = std::make_unique<ma_sound>();
  ma_uint32 flags = 0;
  if (channel == AudioChannel::Music || channel == AudioChannel::Voice ||
      channel == AudioChannel::Ambient) {
    flags |= MA_SOUND_FLAG_STREAM;
  }

  bool loaded = false;
  DataProvider provider;
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    provider = m_dataProvider;
  }
  if (provider) {
    auto dataResult = provider(trackId);
    if (dataResult.isOk() && !dataResult.value().empty()) {
      source->m_memoryData = std::move(dataResult.value());
      auto decoder = std::make_unique<ma_decoder>();
      ma_decoder_config config = ma_decoder_config_init(
          ma_format_f32, ma_engine_get_channels(m_engine.get()),
          ma_engine_get_sample_rate(m_engine.get()));
      if (ma_decoder_init_memory(source->m_memoryData.data(),
                                 source->m_memoryData.size(), &config,
                                 decoder.get()) == MA_SUCCESS) {
        // Transfer ownership to MaDecoderPtr only after successful init
        source->m_decoder = MaDecoderPtr(decoder.release());
        if (ma_sound_init_from_data_source(m_engine.get(), source->m_decoder.get(),
                                           flags, nullptr,
                                           sound.get()) == MA_SUCCESS) {
          loaded = true;
          source->m_decoderReady = true;
        } else {
          // Custom deleter will handle ma_decoder_uninit automatically
          source->m_decoder.reset();
          source->m_memoryData.clear();
        }
      } else {
        // Decoder init failed, decoder will be cleaned up automatically
        source->m_memoryData.clear();
      }
    }
  }

  if (!loaded) {
    if (ma_sound_init_from_file(m_engine.get(), trackId.c_str(), flags, nullptr,
                                nullptr, sound.get()) != MA_SUCCESS) {
      fireEvent(AudioEvent::Type::Error, handle, trackId);
      return {};
    }
  }
  source->m_sound = std::move(sound);
  source->m_soundReady = true;

  float lengthSeconds = 0.0f;
  ma_sound_get_length_in_seconds(source->m_sound.get(), &lengthSeconds);
  source->m_duration = lengthSeconds;

  {
    std::unique_lock<std::shared_mutex> lock(m_sourcesMutex);
    m_sources.push_back(std::move(source));
  }
  return handle;
}

void AudioManager::releaseSource(AudioHandle handle) {
  std::unique_lock<std::shared_mutex> lock(m_sourcesMutex);
  m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(),
                                 [&handle](const auto &s) {
                                   if (!s || s->handle.id != handle.id) {
                                     return false;
                                   }
                                   if (s->m_soundReady && s->m_sound) {
                                     ma_sound_uninit(s->m_sound.get());
                                   }
                                   // Decoder cleanup handled by MaDecoderPtr custom deleter
                                   return true;
                                 }),
                  m_sources.end());
}

void AudioManager::fireEvent(AudioEvent::Type type, AudioHandle handle,
                             const std::string &trackId) {
  AudioCallback callback;
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    callback = m_eventCallback;
  }
  if (callback) {
    AudioEvent event;
    event.type = type;
    event.handle = handle;
    event.trackId = trackId;
    callback(event);
  }
}

void AudioManager::updateDucking(f64 deltaTime) {
  f32 currentLevel = m_currentDuckLevel.load(std::memory_order_relaxed);
  f32 targetLevel = m_targetDuckLevel.load(std::memory_order_acquire);

  if (std::abs(currentLevel - targetLevel) < 0.001f) {
    m_currentDuckLevel.store(targetLevel, std::memory_order_release);
    return;
  }

  f32 fadeDuration = m_duckFadeDuration.load(std::memory_order_relaxed);
  f32 speed = fadeDuration > 0.0f ? 1.0f / fadeDuration : 10.0f;
  f32 newLevel;
  if (currentLevel < targetLevel) {
    newLevel = std::min(targetLevel,
                        currentLevel + static_cast<f32>(deltaTime) * speed);
  } else {
    newLevel = std::max(targetLevel,
                        currentLevel - static_cast<f32>(deltaTime) * speed);
  }
  m_currentDuckLevel.store(newLevel, std::memory_order_release);
}

f32 AudioManager::calculateEffectiveVolume(const AudioSource &source) const {
  if (m_allMuted.load(std::memory_order_acquire) ||
      isChannelMuted(source.channel)) {
    return 0.0f;
  }

  f32 volume = getChannelVolume(AudioChannel::Master);
  volume *= getChannelVolume(source.channel);

  f32 masterFade;
  {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    masterFade = m_masterFadeVolume;
  }
  volume *= masterFade;

  // Apply ducking to music
  if (source.channel == AudioChannel::Music) {
    volume *= m_currentDuckLevel.load(std::memory_order_acquire);
  }

  return volume;
}

} // namespace NovelMind::audio
