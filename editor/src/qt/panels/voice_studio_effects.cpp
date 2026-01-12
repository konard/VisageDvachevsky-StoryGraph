/**
 * @file voice_studio_effects.cpp
 * @brief Audio effects processing implementation
 */

#include "NovelMind/editor/qt/panels/voice_studio_effects.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <algorithm>
#include <cmath>

namespace NovelMind::editor::qt {

namespace {
constexpr float MIN_DB = -60.0f;
} // namespace

// ============================================================================
// AudioProcessor
// ============================================================================

std::vector<float> AudioProcessor::process(const std::vector<float> &source,
                                           const VoiceClipEdit &edit,
                                           const AudioFormat &format) {
  if (source.empty())
    return {};

  // Apply trim first
  auto result = applyTrim(source, edit.trimStartSamples, edit.trimEndSamples);

  // Apply pre-gain
  if (edit.preGainDb != 0.0f) {
    applyGain(result, edit.preGainDb);
  }

  // Apply high-pass filter
  if (edit.highPassEnabled) {
    applyHighPass(result, edit.highPassFreqHz, format.sampleRate);
  }

  // Apply low-pass filter
  if (edit.lowPassEnabled) {
    applyLowPass(result, edit.lowPassFreqHz, format.sampleRate);
  }

  // Apply EQ
  if (edit.eqEnabled) {
    applyEQ(result, edit.eqLowGainDb, edit.eqMidGainDb, edit.eqHighGainDb,
            edit.eqLowFreqHz, edit.eqHighFreqHz, format.sampleRate);
  }

  // Apply noise gate
  if (edit.noiseGateEnabled) {
    applyNoiseGate(result, edit.noiseGateThresholdDb, edit.noiseGateReductionDb,
                   edit.noiseGateAttackMs, edit.noiseGateReleaseMs,
                   format.sampleRate);
  }

  // Apply fades
  if (edit.fadeInMs > 0 || edit.fadeOutMs > 0) {
    applyFades(result, edit.fadeInMs, edit.fadeOutMs, format.sampleRate);
  }

  // Apply normalization last
  if (edit.normalizeEnabled) {
    applyNormalize(result, edit.normalizeTargetDbFS);
  }

  return result;
}

std::vector<float> AudioProcessor::applyTrim(const std::vector<float> &samples,
                                             int64_t trimStart,
                                             int64_t trimEnd) {
  if (samples.empty())
    return {};

  int64_t totalSamples = static_cast<int64_t>(samples.size());
  int64_t start = std::clamp(trimStart, int64_t(0), totalSamples);
  int64_t end = totalSamples - std::clamp(trimEnd, int64_t(0), totalSamples);

  if (end <= start)
    return {};

  return std::vector<float>(samples.begin() + start, samples.begin() + end);
}

void AudioProcessor::applyFades(std::vector<float> &samples, float fadeInMs,
                                float fadeOutMs, uint32_t sampleRate) {
  if (samples.empty())
    return;

  // Fade in
  if (fadeInMs > 0) {
    int fadeInSamples =
        static_cast<int>(fadeInMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeInSamples = std::min(fadeInSamples, static_cast<int>(samples.size()));

    for (int i = 0; i < fadeInSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeInSamples);
      // Use equal power fade (sine curve)
      samples[static_cast<size_t>(i)] *= std::sin(t * 3.14159f / 2.0f);
    }
  }

  // Fade out
  if (fadeOutMs > 0) {
    int fadeOutSamples =
        static_cast<int>(fadeOutMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeOutSamples = std::min(fadeOutSamples, static_cast<int>(samples.size()));

    int startIdx = static_cast<int>(samples.size()) - fadeOutSamples;
    for (int i = 0; i < fadeOutSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeOutSamples);
      samples[static_cast<size_t>(startIdx + i)] *=
          std::cos(t * 3.14159f / 2.0f);
    }
  }
}

void AudioProcessor::applyGain(std::vector<float> &samples, float gainDb) {
  if (samples.empty() || gainDb == 0.0f)
    return;

  float gainLinear = std::pow(10.0f, gainDb / 20.0f);

  for (auto &sample : samples) {
    sample *= gainLinear;
    // Soft clip to prevent harsh distortion
    sample = std::tanh(sample);
  }
}

void AudioProcessor::applyNormalize(std::vector<float> &samples,
                                    float targetDbFS) {
  if (samples.empty())
    return;

  // Find current peak
  float currentPeak = 0.0f;
  for (const auto &sample : samples) {
    float absVal = std::abs(sample);
    if (absVal > currentPeak)
      currentPeak = absVal;
  }

  if (currentPeak < 0.0001f)
    return; // Too quiet, skip

  // Calculate required gain
  float targetLinear = std::pow(10.0f, targetDbFS / 20.0f);
  float gain = targetLinear / currentPeak;

  for (auto &sample : samples) {
    sample *= gain;
  }
}

void AudioProcessor::applyHighPass(std::vector<float> &samples, float cutoffHz,
                                   uint32_t sampleRate) {
  if (samples.empty() || cutoffHz <= 0 || sampleRate == 0)
    return;

  // Simple 1-pole high-pass filter
  float rc = 1.0f / (2.0f * 3.14159f * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = rc / (rc + dt);

  float prevInput = samples[0];
  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = alpha * (prevOutput + samples[i] - prevInput);
    prevInput = samples[i];
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyLowPass(std::vector<float> &samples, float cutoffHz,
                                  uint32_t sampleRate) {
  if (samples.empty() || cutoffHz <= 0 || sampleRate == 0)
    return;

  // Simple 1-pole low-pass filter
  float rc = 1.0f / (2.0f * 3.14159f * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = dt / (rc + dt);

  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = prevOutput + alpha * (samples[i] - prevOutput);
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyEQ(std::vector<float> &samples, float lowGainDb,
                             float midGainDb, float highGainDb, float lowFreq,
                             float highFreq, uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0)
    return;

  // Clamp gain values to safe ranges to prevent instability
  // Allow ±24dB which is reasonable for EQ
  lowGainDb = std::clamp(lowGainDb, -24.0f, 24.0f);
  midGainDb = std::clamp(midGainDb, -24.0f, 24.0f);
  highGainDb = std::clamp(highGainDb, -24.0f, 24.0f);

  // Validate and clamp frequency ranges
  // Nyquist frequency is half the sample rate
  float nyquistFreq = static_cast<float>(sampleRate) / 2.0f;

  // Low frequency should be in range [20Hz, Nyquist/2]
  lowFreq = std::clamp(lowFreq, 20.0f, nyquistFreq * 0.45f);

  // High frequency should be above low frequency and below Nyquist
  highFreq = std::clamp(highFreq, lowFreq + 100.0f, nyquistFreq * 0.45f);

  // Simple 3-band EQ using crossover filters
  // This is a simplified implementation

  // Convert gains to linear
  float lowGain = std::pow(10.0f, lowGainDb / 20.0f);
  float midGain = std::pow(10.0f, midGainDb / 20.0f);
  float highGain = std::pow(10.0f, highGainDb / 20.0f);

  // Filter coefficients
  float lowRc = 1.0f / (2.0f * 3.14159f * lowFreq);
  float highRc = 1.0f / (2.0f * 3.14159f * highFreq);
  float dt = 1.0f / static_cast<float>(sampleRate);

  float lowAlpha = dt / (lowRc + dt);
  float highAlpha = highRc / (highRc + dt);

  // Clamp filter coefficients to stable ranges [0, 1]
  lowAlpha = std::clamp(lowAlpha, 0.0f, 1.0f);
  highAlpha = std::clamp(highAlpha, 0.0f, 1.0f);

  float lowPrev = samples[0];
  float highPrevIn = samples[0];
  float highPrevOut = 0.0f;

  for (size_t i = 0; i < samples.size(); ++i) {
    // Low band (LP filter)
    float low = lowPrev + lowAlpha * (samples[i] - lowPrev);
    lowPrev = low;

    // High band (HP filter)
    float high = highAlpha * (highPrevOut + samples[i] - highPrevIn);
    highPrevIn = samples[i];
    highPrevOut = high;

    // Mid band (remainder)
    float mid = samples[i] - low - high;

    // Apply gains and sum
    float output = low * lowGain + mid * midGain + high * highGain;

    // Final safety clamp to prevent overflow
    samples[i] = std::clamp(output, -1.0f, 1.0f);
  }
}

void AudioProcessor::applyNoiseGate(std::vector<float> &samples,
                                    float thresholdDb, float reductionDb,
                                    float attackMs, float releaseMs,
                                    uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0)
    return;

  float threshold = std::pow(10.0f, thresholdDb / 20.0f);
  float reduction = std::pow(10.0f, reductionDb / 20.0f);

  int attackSamples =
      static_cast<int>(attackMs * static_cast<float>(sampleRate) / 1000.0f);
  int releaseSamples =
      static_cast<int>(releaseMs * static_cast<float>(sampleRate) / 1000.0f);

  float envelope = 0.0f;
  float gateGain = reduction;

  for (size_t i = 0; i < samples.size(); ++i) {
    float absVal = std::abs(samples[i]);

    // Update envelope
    if (absVal > envelope) {
      envelope +=
          (absVal - envelope) / static_cast<float>(std::max(1, attackSamples));
    } else {
      envelope -=
          (envelope - absVal) / static_cast<float>(std::max(1, releaseSamples));
    }

    // Gate logic
    if (envelope > threshold) {
      // Open gate
      if (gateGain < 1.0f) {
        gateGain +=
            (1.0f - gateGain) / static_cast<float>(std::max(1, attackSamples));
      }
    } else {
      // Close gate
      if (gateGain > reduction) {
        gateGain -= (gateGain - reduction) /
                    static_cast<float>(std::max(1, releaseSamples));
      }
    }

    samples[i] *= gateGain;
  }
}

float AudioProcessor::calculatePeakDb(const std::vector<float> &samples) {
  if (samples.empty())
    return MIN_DB;

  float peak = 0.0f;
  for (const auto &sample : samples) {
    float absVal = std::abs(sample);
    if (absVal > peak)
      peak = absVal;
  }

  if (peak < 0.0001f)
    return MIN_DB;
  return 20.0f * std::log10(peak);
}

float AudioProcessor::calculateRmsDb(const std::vector<float> &samples) {
  if (samples.empty())
    return MIN_DB;

  float sum = 0.0f;
  for (const auto &sample : samples) {
    sum += sample * sample;
  }
  float rms = std::sqrt(sum / static_cast<float>(samples.size()));

  if (rms < 0.0001f)
    return MIN_DB;
  return 20.0f * std::log10(rms);
}

} // namespace NovelMind::editor::qt
