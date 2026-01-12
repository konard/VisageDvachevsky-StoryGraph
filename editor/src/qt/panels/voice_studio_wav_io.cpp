/**
 * @file voice_studio_wav_io.cpp
 * @brief WAV file I/O operations implementation
 */

#include "NovelMind/editor/qt/panels/voice_studio_wav_io.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <algorithm>
#include <fstream>

namespace NovelMind::editor::qt {

bool VoiceStudioWavIO::loadWavFile(const QString &filePath,
                                   std::unique_ptr<VoiceClip> &clip) {
  std::ifstream file(filePath.toStdString(), std::ios::binary);
  if (!file.is_open())
    return false;

  // Read WAV header
  char riffHeader[4];
  file.read(riffHeader, 4);
  if (std::string(riffHeader, 4) != "RIFF")
    return false;

  uint32_t fileSize;
  file.read(reinterpret_cast<char *>(&fileSize), 4);

  char waveHeader[4];
  file.read(waveHeader, 4);
  if (std::string(waveHeader, 4) != "WAVE")
    return false;

  // Find fmt chunk
  char chunkId[4];
  uint32_t chunkSize;
  uint16_t audioFormat, numChannels;
  uint32_t sampleRate, byteRate;
  uint16_t blockAlign, bitsPerSample;

  while (file.read(chunkId, 4)) {
    file.read(reinterpret_cast<char *>(&chunkSize), 4);

    if (std::string(chunkId, 4) == "fmt ") {
      file.read(reinterpret_cast<char *>(&audioFormat), 2);
      file.read(reinterpret_cast<char *>(&numChannels), 2);
      file.read(reinterpret_cast<char *>(&sampleRate), 4);
      file.read(reinterpret_cast<char *>(&byteRate), 4);
      file.read(reinterpret_cast<char *>(&blockAlign), 2);
      file.read(reinterpret_cast<char *>(&bitsPerSample), 2);

      // Skip any extra format bytes
      if (chunkSize > 16) {
        file.seekg(chunkSize - 16, std::ios::cur);
      }
    } else if (std::string(chunkId, 4) == "data") {
      // Read audio data
      clip = std::make_unique<VoiceClip>();
      clip->format.sampleRate = sampleRate;
      clip->format.channels = static_cast<uint8_t>(numChannels);
      clip->format.bitsPerSample = static_cast<uint8_t>(bitsPerSample);
      clip->sourcePath = filePath.toStdString();

      size_t numSamples = chunkSize / (bitsPerSample / 8) / numChannels;
      clip->samples.resize(numSamples);

      if (bitsPerSample == 16) {
        std::vector<int16_t> rawSamples(numSamples * numChannels);
        file.read(reinterpret_cast<char *>(rawSamples.data()), chunkSize);

        // Convert to float and mono if needed
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            sample += static_cast<float>(
                          rawSamples[i * static_cast<size_t>(numChannels) +
                                     static_cast<size_t>(ch)]) /
                      32768.0f;
          }
          clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      } else if (bitsPerSample == 24) {
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            uint8_t bytes[3];
            file.read(reinterpret_cast<char *>(bytes), 3);
            int32_t value = (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
            if (value & 0x800000)
              value = static_cast<int32_t>(static_cast<uint32_t>(value) |
                                           0xFF000000u); // Sign extend
            sample += static_cast<float>(value) / 8388608.0f;
          }
          clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      } else if (bitsPerSample == 32) {
        std::vector<int32_t> rawSamples(numSamples * numChannels);
        file.read(reinterpret_cast<char *>(rawSamples.data()), chunkSize);

        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            sample += static_cast<float>(
                          rawSamples[i * static_cast<size_t>(numChannels) +
                                     static_cast<size_t>(ch)]) /
                      2147483648.0f;
          }
          clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      }

      return true;
    } else {
      // Skip unknown chunk
      file.seekg(chunkSize, std::ios::cur);
    }
  }

  return false;
}

bool VoiceStudioWavIO::saveWavFile(const QString &filePath,
                                   const VoiceClip *clip) {
  if (!clip)
    return false;

  std::ofstream file(filePath.toStdString(), std::ios::binary);
  if (!file.is_open())
    return false;

  const auto &samples = clip->samples;
  size_t numSamples = samples.size();
  uint32_t sampleRate = clip->format.sampleRate;
  uint16_t numChannels = 1; // Always mono output
  uint16_t bitsPerSample = 16;
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);
  uint32_t byteRate = sampleRate * blockAlign;
  uint32_t dataSize = static_cast<uint32_t>(numSamples * blockAlign);
  uint32_t fileSize = 36 + dataSize;

  // Write RIFF header
  file.write("RIFF", 4);
  file.write(reinterpret_cast<char *>(&fileSize), 4);
  file.write("WAVE", 4);

  // Write fmt chunk
  file.write("fmt ", 4);
  uint32_t fmtSize = 16;
  file.write(reinterpret_cast<char *>(&fmtSize), 4);
  uint16_t audioFormat = 1; // PCM
  file.write(reinterpret_cast<char *>(&audioFormat), 2);
  file.write(reinterpret_cast<char *>(&numChannels), 2);
  file.write(reinterpret_cast<char *>(&sampleRate), 4);
  file.write(reinterpret_cast<char *>(&byteRate), 4);
  file.write(reinterpret_cast<char *>(&blockAlign), 2);
  file.write(reinterpret_cast<char *>(&bitsPerSample), 2);

  // Write data chunk
  file.write("data", 4);
  file.write(reinterpret_cast<char *>(&dataSize), 4);

  // Convert float samples to 16-bit PCM
  for (size_t i = 0; i < numSamples; ++i) {
    float sample = std::clamp(samples[i], -1.0f, 1.0f);
    int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);
    file.write(reinterpret_cast<char *>(&pcmSample), 2);
  }

  return true;
}

} // namespace NovelMind::editor::qt
