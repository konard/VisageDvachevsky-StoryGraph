/**
 * @file test_eq_filter_stability.cpp
 * @brief Test EQ filter stability with extreme parameter values
 *
 * Tests for issue #466: EQ filter instability
 * Validates that the filter remains stable across all valid parameter ranges
 */

#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace NovelMind::editor::qt;

// Helper function to check if samples are valid (not NaN or Inf)
bool samplesAreValid(const std::vector<float> &samples) {
  for (float sample : samples) {
    if (std::isnan(sample) || std::isinf(sample)) {
      return false;
    }
  }
  return true;
}

// Helper function to check if samples are within valid range [-1, 1]
bool samplesInRange(const std::vector<float> &samples) {
  for (float sample : samples) {
    if (sample < -1.0f || sample > 1.0f) {
      return false;
    }
  }
  return true;
}

// Test case 1: Extreme positive gain values
void test_eq_filter_extreme_positive_gain() {
  std::cout << "Test 1: Extreme positive gain values..." << std::endl;

  // Create test signal: 1 second of sine wave at 1kHz
  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply extreme positive gains (should be clamped to +24dB)
  AudioProcessor::applyEQ(samples, 50.0f, 50.0f, 50.0f, 300.0f, 3000.0f,
                          sampleRate);

  // Validate results
  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Extreme positive gains handled correctly" << std::endl;
}

// Test case 2: Extreme negative gain values
void test_eq_filter_extreme_negative_gain() {
  std::cout << "Test 2: Extreme negative gain values..." << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply extreme negative gains (should be clamped to -24dB)
  AudioProcessor::applyEQ(samples, -100.0f, -100.0f, -100.0f, 300.0f, 3000.0f,
                          sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Extreme negative gains handled correctly" << std::endl;
}

// Test case 3: Extreme frequency values (very low)
void test_eq_filter_extreme_low_frequency() {
  std::cout << "Test 3: Extreme low frequency values..." << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply with extremely low frequencies (should be clamped to min 20Hz)
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 1.0f, 5.0f, sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Extreme low frequencies handled correctly" << std::endl;
}

// Test case 4: Extreme frequency values (very high)
void test_eq_filter_extreme_high_frequency() {
  std::cout << "Test 4: Extreme high frequency values..." << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply with extremely high frequencies (should be clamped to Nyquist)
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 100.0f, 50000.0f,
                          sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Extreme high frequencies handled correctly" << std::endl;
}

// Test case 5: Inverted frequency order (highFreq < lowFreq)
void test_eq_filter_inverted_frequencies() {
  std::cout << "Test 5: Inverted frequency order..." << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply with inverted frequencies (highFreq < lowFreq)
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 5000.0f, 500.0f,
                          sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Inverted frequencies handled correctly" << std::endl;
}

// Test case 6: Maximum gain with loud signal
void test_eq_filter_max_gain_loud_signal() {
  std::cout << "Test 6: Maximum gain with loud signal..." << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  // Create loud input signal (0.9 amplitude)
  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.9f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply maximum allowed gain
  AudioProcessor::applyEQ(samples, 24.0f, 24.0f, 24.0f, 300.0f, 3000.0f,
                          sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Maximum gain with loud signal handled correctly"
            << std::endl;
}

// Test case 7: Zero samples edge case
void test_eq_filter_empty_samples() {
  std::cout << "Test 7: Empty samples edge case..." << std::endl;

  std::vector<float> samples;
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 300.0f, 3000.0f, 48000);

  // Should handle gracefully without crashing
  assert(samples.empty() && "Empty samples should remain empty");

  std::cout << "  ✓ Empty samples handled correctly" << std::endl;
}

// Test case 8: Single sample edge case
void test_eq_filter_single_sample() {
  std::cout << "Test 8: Single sample edge case..." << std::endl;

  std::vector<float> samples = {0.5f};
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 300.0f, 3000.0f, 48000);

  assert(samplesAreValid(samples) && "Sample contains NaN or Inf");
  assert(samplesInRange(samples) && "Sample exceeds valid range [-1, 1]");

  std::cout << "  ✓ Single sample handled correctly" << std::endl;
}

// Test case 9: All bands at different extreme gains
void test_eq_filter_mixed_extreme_gains() {
  std::cout << "Test 9: Mixed extreme gains (low+, mid-, high+)..."
            << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Mixed extreme gains
  AudioProcessor::applyEQ(samples, 100.0f, -100.0f, 100.0f, 300.0f, 3000.0f,
                          sampleRate);

  assert(samplesAreValid(samples) && "Samples contain NaN or Inf");
  assert(samplesInRange(samples) && "Samples exceed valid range [-1, 1]");

  std::cout << "  ✓ Mixed extreme gains handled correctly" << std::endl;
}

// Test case 10: Verify no audio artifacts (check output is reasonable)
void test_eq_filter_no_silence() {
  std::cout << "Test 10: Verify filter doesn't produce total silence..."
            << std::endl;

  const uint32_t sampleRate = 48000;
  const size_t numSamples = sampleRate;
  std::vector<float> samples(numSamples);

  // Create non-zero input
  for (size_t i = 0; i < numSamples; ++i) {
    samples[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f *
                                  static_cast<float>(i) /
                                  static_cast<float>(sampleRate));
  }

  // Apply normal EQ
  AudioProcessor::applyEQ(samples, 6.0f, 0.0f, 6.0f, 300.0f, 3000.0f,
                          sampleRate);

  // Check that output is not completely silent
  bool hasNonZero = false;
  for (float sample : samples) {
    if (std::abs(sample) > 0.001f) {
      hasNonZero = true;
      break;
    }
  }

  assert(hasNonZero && "Filter produced complete silence from non-zero input");

  std::cout << "  ✓ Filter produces non-zero output for non-zero input"
            << std::endl;
}

int main() {
  std::cout << "\n=== EQ Filter Stability Tests ===" << std::endl;
  std::cout << "Testing issue #466 fixes\n" << std::endl;

  try {
    test_eq_filter_extreme_positive_gain();
    test_eq_filter_extreme_negative_gain();
    test_eq_filter_extreme_low_frequency();
    test_eq_filter_extreme_high_frequency();
    test_eq_filter_inverted_frequencies();
    test_eq_filter_max_gain_loud_signal();
    test_eq_filter_empty_samples();
    test_eq_filter_single_sample();
    test_eq_filter_mixed_extreme_gains();
    test_eq_filter_no_silence();

    std::cout << "\n✅ All tests passed!" << std::endl;
    std::cout << "EQ filter is stable across all tested parameter ranges."
              << std::endl;

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
    return 1;
  }
}
