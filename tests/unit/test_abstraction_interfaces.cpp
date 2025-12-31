/**
 * @file test_abstraction_interfaces.cpp
 * @brief Unit tests for abstraction interfaces (issue #150)
 *
 * Tests the IAudioPlayer and IFileSystem interfaces with mock implementations.
 * These tests demonstrate that the abstraction layer works correctly
 * and enables proper unit testing without hardware dependencies.
 */

#include <catch2/catch_test_macros.hpp>

#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/IFileSystem.hpp"
#include "NovelMind/editor/interfaces/MockAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/MockFileSystem.hpp"
#include "NovelMind/editor/interfaces/ServiceLocator.hpp"

using namespace NovelMind;
using namespace NovelMind::editor;

// ============================================================================
// MockAudioPlayer Tests
// ============================================================================

TEST_CASE("MockAudioPlayer basic operations", "[audio][mock]") {
  MockAudioPlayer player;

  SECTION("initial state") {
    REQUIRE(player.isStopped());
    REQUIRE_FALSE(player.isPlaying());
    REQUIRE_FALSE(player.isPaused());
    REQUIRE(player.getPlaybackState() == AudioPlaybackState::Stopped);
    REQUIRE(player.getCurrentFilePath().empty());
    REQUIRE(player.getLoadCount() == 0);
    REQUIRE(player.getPlayCount() == 0);
  }

  SECTION("load file") {
    bool result = player.load("/path/to/audio.wav");
    REQUIRE(result);
    REQUIRE(player.getLoadedFile() == "/path/to/audio.wav");
    REQUIRE(player.getCurrentFilePath() == "/path/to/audio.wav");
    REQUIRE(player.getLoadCount() == 1);
  }

  SECTION("play and stop") {
    player.load("/path/to/audio.wav");
    player.play();

    REQUIRE(player.isPlaying());
    REQUIRE_FALSE(player.isStopped());
    REQUIRE(player.getPlaybackState() == AudioPlaybackState::Playing);
    REQUIRE(player.getPlayCount() == 1);

    player.stop();
    REQUIRE(player.isStopped());
    REQUIRE_FALSE(player.isPlaying());
    REQUIRE(player.getStopCount() == 1);
  }

  SECTION("pause and resume") {
    player.load("/path/to/audio.wav");
    player.play();
    player.pause();

    REQUIRE(player.isPaused());
    REQUIRE_FALSE(player.isPlaying());
    REQUIRE(player.getPlaybackState() == AudioPlaybackState::Paused);
    REQUIRE(player.getPauseCount() == 1);

    player.play();
    REQUIRE(player.isPlaying());
    REQUIRE(player.getPlayCount() == 2);
  }

  SECTION("volume control") {
    REQUIRE(player.getVolume() == 1.0f);

    player.setVolume(0.5f);
    REQUIRE(player.getVolume() == 0.5f);
    REQUIRE(player.getVolumeChangeCount() == 1);

    player.setVolume(0.0f);
    REQUIRE(player.getVolume() == 0.0f);
    REQUIRE(player.getVolumeChangeCount() == 2);
  }

  SECTION("duration and position") {
    player.setMockDuration(10.0f);
    REQUIRE(player.getDuration() == 10.0f);
    REQUIRE(player.getDurationMs() == 10000);

    player.setPosition(5.0f);
    REQUIRE(player.getCurrentPosition() == 5.0f);
    REQUIRE(player.getPositionMs() == 5000);

    player.setPositionMs(7500);
    REQUIRE(player.getPositionMs() == 7500);
  }

  SECTION("clear source") {
    player.load("/path/to/audio.wav");
    player.play();
    player.clearSource();

    REQUIRE(player.getCurrentFilePath().empty());
    REQUIRE(player.isStopped());
  }

  SECTION("reset") {
    player.load("/path/to/audio.wav");
    player.play();
    player.setVolume(0.5f);

    player.reset();

    REQUIRE(player.getLoadedFile().empty());
    REQUIRE(player.isStopped());
    REQUIRE(player.getVolume() == 1.0f);
    REQUIRE(player.getLoadCount() == 0);
    REQUIRE(player.getPlayCount() == 0);
  }
}

TEST_CASE("MockAudioPlayer callbacks", "[audio][mock][callbacks]") {
  MockAudioPlayer player;

  SECTION("playback state changed callback") {
    AudioPlaybackState receivedState = AudioPlaybackState::Stopped;
    int callCount = 0;

    player.setOnPlaybackStateChanged([&](AudioPlaybackState state) {
      receivedState = state;
      callCount++;
    });

    player.load("/path/to/audio.wav");
    player.play();

    REQUIRE(receivedState == AudioPlaybackState::Playing);
    REQUIRE(callCount == 1);

    player.pause();
    REQUIRE(receivedState == AudioPlaybackState::Paused);
    REQUIRE(callCount == 2);

    player.stop();
    REQUIRE(receivedState == AudioPlaybackState::Stopped);
    REQUIRE(callCount == 3);
  }

  SECTION("playback finished callback") {
    bool finished = false;
    player.setOnPlaybackFinished([&]() { finished = true; });

    player.load("/path/to/audio.wav");
    player.play();
    player.simulatePlaybackFinished();

    REQUIRE(finished);
    REQUIRE(player.isStopped());
  }

  SECTION("error callback") {
    std::string receivedError;
    player.setOnError([&](const std::string &error) { receivedError = error; });

    player.simulateError("Test error message");

    REQUIRE(receivedError == "Test error message");
    REQUIRE(player.getErrorString() == "Test error message");
  }

  SECTION("duration changed callback") {
    i64 receivedDuration = 0;
    player.setOnDurationChanged([&](i64 duration) { receivedDuration = duration; });

    player.simulateDurationChanged(5000);

    REQUIRE(receivedDuration == 5000);
    REQUIRE(player.getDurationMs() == 5000);
  }

  SECTION("position changed callback") {
    i64 receivedPosition = 0;
    player.setOnPositionChanged([&](i64 position) { receivedPosition = position; });

    player.simulatePositionChanged(2500);

    REQUIRE(receivedPosition == 2500);
    REQUIRE(player.getPositionMs() == 2500);
  }
}

TEST_CASE("MockAudioPlayer mock configuration", "[audio][mock]") {
  MockAudioPlayer player;

  SECTION("mock load failure") {
    player.setMockLoadSuccess(false);
    REQUIRE_FALSE(player.load("/path/to/audio.wav"));
  }

  SECTION("mock media status") {
    player.setMockMediaStatus(AudioMediaStatus::Loading);
    REQUIRE(player.getMediaStatus() == AudioMediaStatus::Loading);

    player.setMockMediaStatus(AudioMediaStatus::Loaded);
    REQUIRE(player.getMediaStatus() == AudioMediaStatus::Loaded);
  }
}

// ============================================================================
// MockFileSystem Tests
// ============================================================================

TEST_CASE("MockFileSystem basic operations", "[filesystem][mock]") {
  MockFileSystem fs;

  SECTION("initial state") {
    REQUIRE_FALSE(fs.fileExists("/any/path.txt"));
    REQUIRE_FALSE(fs.directoryExists("/any/dir"));
    REQUIRE(fs.getWriteCount() == 0);
  }

  SECTION("write and read file") {
    REQUIRE(fs.writeFile("/test/file.txt", "Hello, World!"));
    REQUIRE(fs.fileExists("/test/file.txt"));
    REQUIRE(fs.readFile("/test/file.txt") == "Hello, World!");
    REQUIRE(fs.getWriteCount() == 1);
  }

  SECTION("delete file") {
    fs.writeFile("/test/file.txt", "Content");
    REQUIRE(fs.fileExists("/test/file.txt"));

    REQUIRE(fs.deleteFile("/test/file.txt"));
    REQUIRE_FALSE(fs.fileExists("/test/file.txt"));
    REQUIRE(fs.getDeleteCount() == 1);
  }

  SECTION("copy file") {
    fs.writeFile("/src/file.txt", "Original content");
    REQUIRE(fs.copyFile("/src/file.txt", "/dest/file.txt"));

    REQUIRE(fs.fileExists("/src/file.txt"));
    REQUIRE(fs.fileExists("/dest/file.txt"));
    REQUIRE(fs.readFile("/dest/file.txt") == "Original content");
    REQUIRE(fs.getCopyCount() == 1);
  }

  SECTION("move file") {
    fs.writeFile("/src/file.txt", "Content");
    REQUIRE(fs.moveFile("/src/file.txt", "/dest/file.txt"));

    REQUIRE_FALSE(fs.fileExists("/src/file.txt"));
    REQUIRE(fs.fileExists("/dest/file.txt"));
    REQUIRE(fs.readFile("/dest/file.txt") == "Content");
  }
}

TEST_CASE("MockFileSystem directory operations", "[filesystem][mock]") {
  MockFileSystem fs;

  SECTION("create directory") {
    REQUIRE(fs.createDirectory("/test/dir"));
    REQUIRE(fs.directoryExists("/test/dir"));
    REQUIRE(fs.getCreateDirCount() == 1);
  }

  SECTION("create directories recursively") {
    REQUIRE(fs.createDirectories("/a/b/c/d"));
    REQUIRE(fs.directoryExists("/a/b/c/d"));
    REQUIRE(fs.directoryExists("/a/b/c"));
    REQUIRE(fs.directoryExists("/a/b"));
    REQUIRE(fs.directoryExists("/a"));
  }

  SECTION("delete directory") {
    fs.createDirectory("/test/dir");
    REQUIRE(fs.deleteDirectory("/test/dir"));
    REQUIRE_FALSE(fs.directoryExists("/test/dir"));
    REQUIRE(fs.getDeleteDirCount() == 1);
  }

  SECTION("delete directory recursively") {
    fs.addMockFile("/test/dir/file1.txt", "Content 1");
    fs.addMockFile("/test/dir/file2.txt", "Content 2");
    fs.addMockFile("/test/dir/subdir/file3.txt", "Content 3");

    REQUIRE(fs.deleteDirectory("/test/dir", true));
    REQUIRE_FALSE(fs.directoryExists("/test/dir"));
    REQUIRE_FALSE(fs.fileExists("/test/dir/file1.txt"));
  }
}

TEST_CASE("MockFileSystem directory listing", "[filesystem][mock]") {
  MockFileSystem fs;

  fs.addMockFile("/project/src/main.cpp", "int main() {}");
  fs.addMockFile("/project/src/util.cpp", "void util() {}");
  fs.addMockFile("/project/src/test.hpp", "class Test {};");
  fs.addMockFile("/project/include/header.hpp", "#pragma once");

  SECTION("list files in directory") {
    auto files = fs.listFiles("/project/src");
    REQUIRE(files.size() == 3);
  }

  SECTION("list files with filter") {
    auto cppFiles = fs.listFiles("/project/src", "*.cpp");
    REQUIRE(cppFiles.size() == 2);
  }

  SECTION("list files recursively") {
    auto allFiles = fs.listFilesRecursive("/project");
    REQUIRE(allFiles.size() == 4);
  }

  SECTION("list directories") {
    auto dirs = fs.listDirectories("/project");
    REQUIRE(dirs.size() == 2); // src and include
  }
}

TEST_CASE("MockFileSystem file info", "[filesystem][mock]") {
  MockFileSystem fs;
  fs.addMockFile("/test/sample.txt", "Sample content");

  SECTION("get file info") {
    auto info = fs.getFileInfo("/test/sample.txt");
    REQUIRE(info.exists);
    REQUIRE_FALSE(info.isDirectory);
    REQUIRE(info.name == "sample.txt");
    REQUIRE(info.extension == ".txt");
    REQUIRE(info.size == 14); // "Sample content" length
  }

  SECTION("get file size") {
    REQUIRE(fs.getFileSize("/test/sample.txt") == 14);
  }

  SECTION("get last modified") {
    REQUIRE(fs.getLastModified("/test/sample.txt") > 0);
  }
}

TEST_CASE("MockFileSystem path utilities", "[filesystem][mock]") {
  MockFileSystem fs;

  SECTION("get file name") {
    REQUIRE(fs.getFileName("/path/to/file.txt") == "file.txt");
    REQUIRE(fs.getFileName("file.txt") == "file.txt");
  }

  SECTION("get base name") {
    REQUIRE(fs.getBaseName("/path/to/file.txt") == "file");
    REQUIRE(fs.getBaseName("/path/to/file") == "file");
  }

  SECTION("get extension") {
    REQUIRE(fs.getExtension("/path/to/file.txt") == ".txt");
    REQUIRE(fs.getExtension("/path/to/file") == "");
  }

  SECTION("get parent directory") {
    REQUIRE(fs.getParentDirectory("/path/to/file.txt") == "/path/to");
    REQUIRE(fs.getParentDirectory("/path/to/dir") == "/path/to");
  }

  SECTION("normalize path") {
    REQUIRE(fs.normalizePath("path\\to\\file") == "path/to/file");
    REQUIRE(fs.normalizePath("/path/to/dir/") == "/path/to/dir");
  }

  SECTION("join path") {
    REQUIRE(fs.joinPath("/base", "component") == "/base/component");
    REQUIRE(fs.joinPath("/base/", "component") == "/base/component");
    REQUIRE(fs.joinPath("", "component") == "component");
  }
}

TEST_CASE("MockFileSystem reset", "[filesystem][mock]") {
  MockFileSystem fs;

  fs.addMockFile("/test/file.txt", "Content");
  fs.addMockDirectory("/test/dir");
  fs.writeFile("/another/file.txt", "More content");

  REQUIRE(fs.getFiles().size() == 2);
  REQUIRE(fs.getWriteCount() == 1);

  fs.reset();

  REQUIRE(fs.getFiles().empty());
  REQUIRE(fs.getDirectories().empty());
  REQUIRE(fs.getWriteCount() == 0);
}

// ============================================================================
// ServiceLocator Tests
// ============================================================================

TEST_CASE("ServiceLocator audio player registration", "[servicelocator]") {
  // Ensure clean state
  ServiceLocator::reset();

  SECTION("no player registered initially") {
    REQUIRE_FALSE(ServiceLocator::hasAudioPlayer());
    REQUIRE(ServiceLocator::getAudioPlayer() == nullptr);
  }

  SECTION("register and retrieve player") {
    auto mockPlayer = std::make_unique<MockAudioPlayer>();
    MockAudioPlayer *rawPtr = mockPlayer.get();

    ServiceLocator::registerAudioPlayer(std::move(mockPlayer));

    REQUIRE(ServiceLocator::hasAudioPlayer());
    REQUIRE(ServiceLocator::getAudioPlayer() == rawPtr);
  }

  SECTION("audio player factory") {
    ServiceLocator::registerAudioPlayerFactory(
        []() { return std::make_unique<MockAudioPlayer>(); });

    auto player = ServiceLocator::createAudioPlayer();
    REQUIRE(player != nullptr);
  }

  // Cleanup
  ServiceLocator::reset();
}

TEST_CASE("ServiceLocator file system registration", "[servicelocator]") {
  // Ensure clean state
  ServiceLocator::reset();

  SECTION("no file system registered initially") {
    REQUIRE_FALSE(ServiceLocator::hasFileSystem());
    REQUIRE(ServiceLocator::getFileSystem() == nullptr);
  }

  SECTION("register and retrieve file system") {
    auto mockFs = std::make_unique<MockFileSystem>();
    MockFileSystem *rawPtr = mockFs.get();

    ServiceLocator::registerFileSystem(std::move(mockFs));

    REQUIRE(ServiceLocator::hasFileSystem());
    REQUIRE(ServiceLocator::getFileSystem() == rawPtr);
  }

  SECTION("file system factory") {
    ServiceLocator::registerFileSystemFactory(
        []() { return std::make_unique<MockFileSystem>(); });

    auto fs = ServiceLocator::createFileSystem();
    REQUIRE(fs != nullptr);
  }

  // Cleanup
  ServiceLocator::reset();
}

TEST_CASE("ServiceLocator shutdown", "[servicelocator]") {
  auto mockPlayer = std::make_unique<MockAudioPlayer>();
  auto mockFs = std::make_unique<MockFileSystem>();

  ServiceLocator::registerAudioPlayer(std::move(mockPlayer));
  ServiceLocator::registerFileSystem(std::move(mockFs));

  REQUIRE(ServiceLocator::hasAudioPlayer());
  REQUIRE(ServiceLocator::hasFileSystem());

  ServiceLocator::shutdown();

  REQUIRE_FALSE(ServiceLocator::hasAudioPlayer());
  REQUIRE_FALSE(ServiceLocator::hasFileSystem());
}
