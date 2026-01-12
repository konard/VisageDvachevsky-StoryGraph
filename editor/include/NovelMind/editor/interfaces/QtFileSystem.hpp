#pragma once

/**
 * @file QtFileSystem.hpp
 * @brief Qt-based implementation of IFileSystem interface
 *
 * Implements the IFileSystem interface using Qt's file system classes
 * (QFile, QDir, QFileInfo).
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/IFileSystem.hpp"

namespace NovelMind::editor {

/**
 * @brief Qt-based implementation of IFileSystem
 *
 * This class wraps Qt file system classes to provide
 * platform-independent file operations through the IFileSystem interface.
 */
class QtFileSystem : public IFileSystem {
public:
  QtFileSystem() = default;
  ~QtFileSystem() override = default;

  // =========================================================================
  // IFileSystem Implementation
  // =========================================================================

  [[nodiscard]] bool fileExists(const std::string& path) const override;
  [[nodiscard]] bool directoryExists(const std::string& path) const override;
  [[nodiscard]] bool pathExists(const std::string& path) const override;

  [[nodiscard]] std::string readFile(const std::string& path) const override;
  [[nodiscard]] std::vector<u8> readBinaryFile(const std::string& path) const override;
  bool writeFile(const std::string& path, const std::string& content) override;
  bool writeBinaryFile(const std::string& path, const std::vector<u8>& data) override;
  bool deleteFile(const std::string& path) override;
  bool copyFile(const std::string& src, const std::string& dest) override;
  bool moveFile(const std::string& src, const std::string& dest) override;

  bool createDirectory(const std::string& path) override;
  bool createDirectories(const std::string& path) override;
  bool deleteDirectory(const std::string& path, bool recursive = false) override;

  [[nodiscard]] std::vector<std::string> listFiles(const std::string& directory,
                                                   const std::string& filter = "*") const override;
  [[nodiscard]] std::vector<std::string>
  listDirectories(const std::string& directory) const override;
  [[nodiscard]] std::vector<std::string>
  listFilesRecursive(const std::string& directory, const std::string& filter = "*") const override;

  [[nodiscard]] FileInfo getFileInfo(const std::string& path) const override;
  [[nodiscard]] u64 getFileSize(const std::string& path) const override;
  [[nodiscard]] u64 getLastModified(const std::string& path) const override;

  [[nodiscard]] std::string getFileName(const std::string& path) const override;
  [[nodiscard]] std::string getBaseName(const std::string& path) const override;
  [[nodiscard]] std::string getExtension(const std::string& path) const override;
  [[nodiscard]] std::string getParentDirectory(const std::string& path) const override;
  [[nodiscard]] std::string normalizePath(const std::string& path) const override;
  [[nodiscard]] std::string joinPath(const std::string& base,
                                     const std::string& component) const override;
};

} // namespace NovelMind::editor
