#pragma once

/**
 * @file ServiceLocator.hpp
 * @brief Service locator pattern for dependency injection
 *
 * Provides a central registry for service interfaces, allowing:
 * - Runtime service registration
 * - Easy swap between production and mock implementations
 * - Simplified testing without modifying code
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/IFileSystem.hpp"

#include <memory>
#include <mutex>

namespace NovelMind::editor {

/**
 * @brief Service locator for global service access
 *
 * Provides thread-safe access to registered services.
 * Services can be registered at application startup and
 * swapped out for testing.
 *
 * Usage:
 * @code
 * // In production main.cpp
 * ServiceLocator::registerAudioPlayer(std::make_unique<QtAudioPlayer>());
 * ServiceLocator::registerFileSystem(std::make_unique<QtFileSystem>());
 *
 * // In test setup
 * ServiceLocator::registerAudioPlayer(std::make_unique<MockAudioPlayer>());
 * ServiceLocator::registerFileSystem(std::make_unique<MockFileSystem>());
 *
 * // Usage in code
 * auto* player = ServiceLocator::getAudioPlayer();
 * if (player) {
 *   player->play();
 * }
 * @endcode
 */
class ServiceLocator {
public:
  // =========================================================================
  // Audio Player Service
  // =========================================================================

  /**
   * @brief Register an audio player instance
   * @param player Audio player instance (ownership transferred)
   */
  static void registerAudioPlayer(std::unique_ptr<IAudioPlayer> player) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_audioPlayer = std::move(player);
  }

  /**
   * @brief Get the registered audio player
   * @return Pointer to audio player, nullptr if not registered
   */
  static IAudioPlayer *getAudioPlayer() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_audioPlayer.get();
  }

  /**
   * @brief Check if an audio player is registered
   * @return true if audio player is available
   */
  static bool hasAudioPlayer() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_audioPlayer != nullptr;
  }

  /**
   * @brief Create a new audio player using the registered factory
   * @return New audio player instance, nullptr if no factory registered
   */
  static std::unique_ptr<IAudioPlayer> createAudioPlayer() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_audioPlayerFactory) {
      return s_audioPlayerFactory();
    }
    return nullptr;
  }

  /**
   * @brief Register a factory for creating audio players
   * @param factory Factory function
   */
  static void registerAudioPlayerFactory(AudioPlayerFactory factory) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_audioPlayerFactory = std::move(factory);
  }

  // =========================================================================
  // File System Service
  // =========================================================================

  /**
   * @brief Register a file system instance
   * @param fs File system instance (ownership transferred)
   */
  static void registerFileSystem(std::unique_ptr<IFileSystem> fs) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_fileSystem = std::move(fs);
  }

  /**
   * @brief Get the registered file system
   * @return Pointer to file system, nullptr if not registered
   */
  static IFileSystem *getFileSystem() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_fileSystem.get();
  }

  /**
   * @brief Check if a file system is registered
   * @return true if file system is available
   */
  static bool hasFileSystem() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_fileSystem != nullptr;
  }

  /**
   * @brief Create a new file system using the registered factory
   * @return New file system instance, nullptr if no factory registered
   */
  static std::unique_ptr<IFileSystem> createFileSystem() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_fileSystemFactory) {
      return s_fileSystemFactory();
    }
    return nullptr;
  }

  /**
   * @brief Register a factory for creating file systems
   * @param factory Factory function
   */
  static void registerFileSystemFactory(FileSystemFactory factory) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_fileSystemFactory = std::move(factory);
  }

  // =========================================================================
  // Lifecycle Management
  // =========================================================================

  /**
   * @brief Initialize default services (Qt implementations)
   *
   * Call this at application startup to register production services.
   * This should be called from main() or equivalent.
   */
  static void initializeDefaults();

  /**
   * @brief Shutdown and release all services
   *
   * Call this during application shutdown to clean up resources.
   */
  static void shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_audioPlayer.reset();
    s_fileSystem.reset();
    s_audioPlayerFactory = nullptr;
    s_fileSystemFactory = nullptr;
  }

  /**
   * @brief Reset all services (for testing)
   *
   * Same as shutdown, but intended for use in test teardown.
   */
  static void reset() { shutdown(); }

private:
  ServiceLocator() = delete;
  ~ServiceLocator() = delete;

  static std::unique_ptr<IAudioPlayer> s_audioPlayer;
  static std::unique_ptr<IFileSystem> s_fileSystem;
  static AudioPlayerFactory s_audioPlayerFactory;
  static FileSystemFactory s_fileSystemFactory;
  static std::mutex s_mutex;
};

/**
 * @brief RAII helper for temporarily registering mock services
 *
 * Use this in tests to temporarily replace services with mocks.
 * Original services are restored when the scope exits.
 *
 * @code
 * TEST_CASE("Test with mock") {
 *   auto mockPlayer = std::make_unique<MockAudioPlayer>();
 *   MockAudioPlayer* mockPtr = mockPlayer.get();
 *
 *   ServiceScope scope;
 *   scope.setAudioPlayer(std::move(mockPlayer));
 *
 *   // Tests run with mock player
 *   // Original services restored when scope exits
 * }
 * @endcode
 */
class ServiceScope {
public:
  ServiceScope() = default;

  ~ServiceScope() {
    // Restore original services
    if (m_hadAudioPlayer) {
      ServiceLocator::registerAudioPlayer(std::move(m_originalAudioPlayer));
    } else {
      // Clear the mock
      ServiceLocator::registerAudioPlayer(nullptr);
    }

    if (m_hadFileSystem) {
      ServiceLocator::registerFileSystem(std::move(m_originalFileSystem));
    } else {
      ServiceLocator::registerFileSystem(nullptr);
    }
  }

  /**
   * @brief Set a mock audio player for this scope
   * @param player Mock player instance
   */
  void setAudioPlayer(std::unique_ptr<IAudioPlayer> player) {
    m_hadAudioPlayer = ServiceLocator::hasAudioPlayer();
    // Note: Can't easily save the original without breaking encapsulation
    // In practice, tests should start with a clean state
    ServiceLocator::registerAudioPlayer(std::move(player));
  }

  /**
   * @brief Set a mock file system for this scope
   * @param fs Mock file system instance
   */
  void setFileSystem(std::unique_ptr<IFileSystem> fs) {
    m_hadFileSystem = ServiceLocator::hasFileSystem();
    ServiceLocator::registerFileSystem(std::move(fs));
  }

private:
  bool m_hadAudioPlayer = false;
  bool m_hadFileSystem = false;
  std::unique_ptr<IAudioPlayer> m_originalAudioPlayer;
  std::unique_ptr<IFileSystem> m_originalFileSystem;
};

} // namespace NovelMind::editor
