/**
 * @file ServiceLocator.cpp
 * @brief Service locator implementation
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/ServiceLocator.hpp"
#include "NovelMind/editor/interfaces/QtAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/QtFileSystem.hpp"

namespace NovelMind::editor {

// Static member definitions
std::unique_ptr<IAudioPlayer> ServiceLocator::s_audioPlayer;
std::unique_ptr<IFileSystem> ServiceLocator::s_fileSystem;
AudioPlayerFactory ServiceLocator::s_audioPlayerFactory;
FileSystemFactory ServiceLocator::s_fileSystemFactory;
std::mutex ServiceLocator::s_mutex;

void ServiceLocator::initializeDefaults() {
  std::lock_guard<std::mutex> lock(s_mutex);

  // Register Qt-based implementations as defaults
  if (!s_audioPlayer) {
    s_audioPlayer = std::make_unique<QtAudioPlayer>();
  }

  if (!s_fileSystem) {
    s_fileSystem = std::make_unique<QtFileSystem>();
  }

  // Register factories
  if (!s_audioPlayerFactory) {
    s_audioPlayerFactory = []() -> std::unique_ptr<IAudioPlayer> {
      return std::make_unique<QtAudioPlayer>();
    };
  }

  if (!s_fileSystemFactory) {
    s_fileSystemFactory = []() -> std::unique_ptr<IFileSystem> {
      return std::make_unique<QtFileSystem>();
    };
  }
}

} // namespace NovelMind::editor
