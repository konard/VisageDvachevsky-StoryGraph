/**
 * @file build_utils.cpp
 * @brief Build utility functions for NovelMind Build System
 */

#include "NovelMind/editor/build_system.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// BuildUtils Implementation
// ============================================================================

namespace BuildUtils {

std::string getPlatformName(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return "Windows";
  case BuildPlatform::Linux:
    return "Linux";
  case BuildPlatform::MacOS:
    return "macOS";
  case BuildPlatform::Web:
    return "Web (WebAssembly)";
  case BuildPlatform::Android:
    return "Android";
  case BuildPlatform::iOS:
    return "iOS";
  case BuildPlatform::All:
    return "All Platforms";
  }
  return "Unknown";
}

std::string getExecutableExtension(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return ".exe";
  case BuildPlatform::Linux:
  case BuildPlatform::MacOS:
    return "";
  case BuildPlatform::Web:
    return ".html"; // Entry point for web builds
  case BuildPlatform::Android:
    return ".apk";
  case BuildPlatform::iOS:
    return ".ipa";
  case BuildPlatform::All:
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
  }
  return "";
}

BuildPlatform getCurrentPlatform() {
#ifdef _WIN32
  return BuildPlatform::Windows;
#elif defined(__APPLE__)
  return BuildPlatform::MacOS;
#else
  return BuildPlatform::Linux;
#endif
}

std::string formatFileSize(i64 bytes) {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  i32 unitIndex = 0;
  f64 size = static_cast<f64>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  std::ostringstream oss;
  if (unitIndex == 0) {
    oss << bytes << " " << units[unitIndex];
  } else {
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
  }
  return oss.str();
}

std::string formatDuration(f64 milliseconds) {
  if (milliseconds < 1000) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << milliseconds << " ms";
    return oss.str();
  }

  f64 seconds = milliseconds / 1000.0;
  if (seconds < 60) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << seconds << " s";
    return oss.str();
  }

  i32 minutes = static_cast<i32>(seconds) / 60;
  i32 secs = static_cast<i32>(seconds) % 60;
  std::ostringstream oss;
  oss << minutes << " min " << secs << " s";
  return oss.str();
}

i64 calculateDirectorySize(const std::string& path) {
  i64 totalSize = 0;
  try {
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        totalSize += static_cast<i64>(entry.file_size());
      }
    }
  } catch (const fs::filesystem_error&) {
    // Directory doesn't exist or permission denied
  }
  return totalSize;
}

Result<void> copyDirectory(const std::string& source, const std::string& destination) {
  try {
    fs::copy(source, destination,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to copy directory: ") + e.what());
  }
}

Result<void> deleteDirectory(const std::string& path) {
  try {
    if (fs::exists(path)) {
      fs::remove_all(path);
    }
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to delete directory: ") + e.what());
  }
}

Result<void> createDirectories(const std::string& path) {
  try {
    fs::create_directories(path);
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to create directories: ") + e.what());
  }
}

} // namespace BuildUtils

} // namespace NovelMind::editor
