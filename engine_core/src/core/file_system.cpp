#include "NovelMind/platform/file_system.hpp"
#include "NovelMind/core/logger.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif

namespace NovelMind::platform {

namespace fs = std::filesystem;

class NativeFileSystem : public IFileSystem {
public:
  Result<std::vector<u8>> readFile(const std::string& path) const override {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      return Result<std::vector<u8>>::error("Failed to open file: " + path);
    }

    file.seekg(0, std::ios::end);
    const std::streampos size = file.tellg();
    if (size < 0) {
      return Result<std::vector<u8>>::error("Failed to read file size: " + path);
    }

    std::vector<u8> data(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!data.empty()) {
      file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
      if (!file) {
        return Result<std::vector<u8>>::error("Failed to read file: " + path);
      }
    }

    return Result<std::vector<u8>>::ok(std::move(data));
  }

  Result<void> writeFile(const std::string& path, const std::vector<u8>& data) const override {
    fs::path target(path);
    if (target.has_parent_path()) {
      std::error_code ec;
      fs::create_directories(target.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
      return Result<void>::error("Failed to open file for writing: " + path);
    }

    if (!data.empty()) {
      file.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
      if (!file) {
        return Result<void>::error("Failed to write file: " + path);
      }
    }

    return Result<void>::ok();
  }

  bool exists(const std::string& path) const override { return fs::exists(path); }

  bool isFile(const std::string& path) const override { return fs::is_regular_file(path); }

  bool isDirectory(const std::string& path) const override { return fs::is_directory(path); }

  std::string getExecutablePath() const override {
#if defined(_WIN32)
    char buffer[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return len > 0 ? std::string(buffer, len) : std::string();
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
      return std::string(buffer);
    }
    return {};
#else
    std::error_code ec;
    auto path = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
      return path.string();
    }
    return {};
#endif
  }

  std::string getUserDataPath() const override {
#if defined(_WIN32)
    char* appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData) {
      std::string path(appData);
      free(appData);
      return path;
    }
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
      return std::string(home);
    }
    return ".";
#endif
  }
};

std::unique_ptr<IFileSystem> createFileSystem() {
  return std::make_unique<NativeFileSystem>();
}

} // namespace NovelMind::platform
