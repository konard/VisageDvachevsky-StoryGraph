#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::platform {

class IFileSystem {
public:
  virtual ~IFileSystem() = default;

  [[nodiscard]] virtual Result<std::vector<u8>> readFile(const std::string& path) const = 0;

  [[nodiscard]] virtual Result<void> writeFile(const std::string& path,
                                               const std::vector<u8>& data) const = 0;

  [[nodiscard]] virtual bool exists(const std::string& path) const = 0;
  [[nodiscard]] virtual bool isFile(const std::string& path) const = 0;
  [[nodiscard]] virtual bool isDirectory(const std::string& path) const = 0;

  [[nodiscard]] virtual std::string getExecutablePath() const = 0;
  [[nodiscard]] virtual std::string getUserDataPath() const = 0;
};

std::unique_ptr<IFileSystem> createFileSystem();

} // namespace NovelMind::platform
