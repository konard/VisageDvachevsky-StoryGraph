#pragma once

/**
 * @file MockFileSystem.hpp
 * @brief Mock implementation of IFileSystem for testing
 *
 * Provides an in-memory file system that can be used for:
 * - Unit testing without disk I/O
 * - CI/CD testing environments
 * - Verifying file operations
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/IFileSystem.hpp"

#include <map>
#include <set>

namespace NovelMind::editor {

/**
 * @brief Mock implementation of IFileSystem for testing
 *
 * This class provides an in-memory file system that:
 * - Stores files in memory (no disk I/O)
 * - Tracks all operations for verification
 * - Runs ~100x faster than real file operations
 * - Is isolated from the real file system
 */
class MockFileSystem : public IFileSystem {
public:
  MockFileSystem() = default;
  ~MockFileSystem() override = default;

  // =========================================================================
  // IFileSystem Implementation
  // =========================================================================

  [[nodiscard]] bool fileExists(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    return m_files.find(normalized) != m_files.end();
  }

  [[nodiscard]] bool directoryExists(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    return m_directories.find(normalized) != m_directories.end();
  }

  [[nodiscard]] bool pathExists(const std::string& path) const override {
    return fileExists(path) || directoryExists(path);
  }

  [[nodiscard]] std::string readFile(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    auto it = m_files.find(normalized);
    if (it != m_files.end()) {
      return it->second;
    }
    return "";
  }

  [[nodiscard]] std::vector<u8> readBinaryFile(const std::string& path) const override {
    std::string content = readFile(path);
    return std::vector<u8>(content.begin(), content.end());
  }

  bool writeFile(const std::string& path, const std::string& content) override {
    std::string normalized = normalizePath(path);
    m_files[normalized] = content;
    m_fileInfo[normalized] = {static_cast<u64>(content.size()), ++m_currentTimestamp};
    m_writeCount++;
    return true;
  }

  bool writeBinaryFile(const std::string& path, const std::vector<u8>& data) override {
    return writeFile(path, std::string(data.begin(), data.end()));
  }

  bool deleteFile(const std::string& path) override {
    std::string normalized = normalizePath(path);
    auto it = m_files.find(normalized);
    if (it != m_files.end()) {
      m_files.erase(it);
      m_fileInfo.erase(normalized);
      m_deleteCount++;
      return true;
    }
    return false;
  }

  bool copyFile(const std::string& src, const std::string& dest) override {
    std::string srcNorm = normalizePath(src);
    std::string destNorm = normalizePath(dest);

    auto it = m_files.find(srcNorm);
    if (it == m_files.end()) {
      return false;
    }

    m_files[destNorm] = it->second;
    m_fileInfo[destNorm] = {static_cast<u64>(it->second.size()), ++m_currentTimestamp};
    m_copyCount++;
    return true;
  }

  bool moveFile(const std::string& src, const std::string& dest) override {
    if (copyFile(src, dest)) {
      return deleteFile(src);
    }
    return false;
  }

  bool createDirectory(const std::string& path) override {
    std::string normalized = normalizePath(path);
    m_directories.insert(normalized);
    m_createDirCount++;
    return true;
  }

  bool createDirectories(const std::string& path) override {
    std::string normalized = normalizePath(path);

    // Create all parent directories
    std::string current;
    for (char c : normalized) {
      current += c;
      if (c == '/' || c == '\\') {
        // Insert without trailing slash
        if (current.size() > 1) {
          m_directories.insert(current.substr(0, current.size() - 1));
        }
      }
    }
    m_directories.insert(normalized);
    m_createDirCount++;
    return true;
  }

  bool deleteDirectory(const std::string& path, bool recursive = false) override {
    std::string normalized = normalizePath(path);

    if (m_directories.find(normalized) == m_directories.end()) {
      return false;
    }

    if (recursive) {
      // Delete all files and subdirectories
      std::vector<std::string> toDelete;
      for (const auto& file : m_files) {
        if (file.first.find(normalized) == 0) {
          toDelete.push_back(file.first);
        }
      }
      for (const auto& f : toDelete) {
        m_files.erase(f);
        m_fileInfo.erase(f);
      }

      std::vector<std::string> dirsToDelete;
      for (const auto& dir : m_directories) {
        if (dir.find(normalized) == 0) {
          dirsToDelete.push_back(dir);
        }
      }
      for (const auto& d : dirsToDelete) {
        m_directories.erase(d);
      }
    } else {
      m_directories.erase(normalized);
    }

    m_deleteDirCount++;
    return true;
  }

  [[nodiscard]] std::vector<std::string> listFiles(const std::string& directory,
                                                   const std::string& filter = "*") const override {
    std::string normalized = normalizePath(directory);
    std::vector<std::string> result;

    for (const auto& file : m_files) {
      std::string parent = getParentDirectory(file.first);
      if (parent == normalized) {
        if (filter == "*" || matchesFilter(file.first, filter)) {
          result.push_back(file.first);
        }
      }
    }
    return result;
  }

  [[nodiscard]] std::vector<std::string>
  listDirectories(const std::string& directory) const override {
    std::string normalized = normalizePath(directory);
    std::vector<std::string> result;

    for (const auto& dir : m_directories) {
      std::string parent = getParentDirectory(dir);
      if (parent == normalized && dir != normalized) {
        result.push_back(dir);
      }
    }
    return result;
  }

  [[nodiscard]] std::vector<std::string>
  listFilesRecursive(const std::string& directory, const std::string& filter = "*") const override {
    std::string normalized = normalizePath(directory);
    std::vector<std::string> result;

    for (const auto& file : m_files) {
      if (file.first.find(normalized) == 0) {
        if (filter == "*" || matchesFilter(file.first, filter)) {
          result.push_back(file.first);
        }
      }
    }
    return result;
  }

  [[nodiscard]] FileInfo getFileInfo(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    FileInfo info;
    info.path = normalized;
    info.name = getFileName(normalized);
    info.extension = getExtension(normalized);
    info.isDirectory = directoryExists(normalized);
    info.exists = pathExists(normalized);

    auto it = m_fileInfo.find(normalized);
    if (it != m_fileInfo.end()) {
      info.size = it->second.size;
      info.lastModified = it->second.lastModified;
    }

    return info;
  }

  [[nodiscard]] u64 getFileSize(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    auto it = m_files.find(normalized);
    if (it != m_files.end()) {
      return static_cast<u64>(it->second.size());
    }
    return 0;
  }

  [[nodiscard]] u64 getLastModified(const std::string& path) const override {
    std::string normalized = normalizePath(path);
    auto it = m_fileInfo.find(normalized);
    if (it != m_fileInfo.end()) {
      return it->second.lastModified;
    }
    return 0;
  }

  [[nodiscard]] std::string getFileName(const std::string& path) const override {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      return path.substr(pos + 1);
    }
    return path;
  }

  [[nodiscard]] std::string getBaseName(const std::string& path) const override {
    std::string name = getFileName(path);
    size_t pos = name.find_last_of('.');
    if (pos != std::string::npos && pos > 0) {
      return name.substr(0, pos);
    }
    return name;
  }

  [[nodiscard]] std::string getExtension(const std::string& path) const override {
    std::string name = getFileName(path);
    size_t pos = name.find_last_of('.');
    if (pos != std::string::npos) {
      return name.substr(pos);
    }
    return "";
  }

  [[nodiscard]] std::string getParentDirectory(const std::string& path) const override {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      return path.substr(0, pos);
    }
    return "";
  }

  [[nodiscard]] std::string normalizePath(const std::string& path) const override {
    // Simple normalization: convert backslashes to forward slashes
    std::string result = path;
    for (char& c : result) {
      if (c == '\\') {
        c = '/';
      }
    }
    // Remove trailing slash
    while (!result.empty() && result.back() == '/') {
      result.pop_back();
    }
    return result;
  }

  [[nodiscard]] std::string joinPath(const std::string& base,
                                     const std::string& component) const override {
    if (base.empty()) {
      return component;
    }
    if (component.empty()) {
      return base;
    }

    std::string result = base;
    if (result.back() != '/' && result.back() != '\\') {
      result += '/';
    }
    result += component;
    return normalizePath(result);
  }

  // =========================================================================
  // Mock Configuration
  // =========================================================================

  /**
   * @brief Add a mock file to the file system
   * @param path File path
   * @param content File content
   */
  void addMockFile(const std::string& path, const std::string& content) {
    std::string normalized = normalizePath(path);
    m_files[normalized] = content;
    m_fileInfo[normalized] = {static_cast<u64>(content.size()), ++m_currentTimestamp};

    // Ensure parent directories exist
    std::string parent = getParentDirectory(normalized);
    while (!parent.empty()) {
      m_directories.insert(parent);
      parent = getParentDirectory(parent);
    }
  }

  /**
   * @brief Add a mock directory
   * @param path Directory path
   */
  void addMockDirectory(const std::string& path) {
    std::string normalized = normalizePath(path);
    m_directories.insert(normalized);

    // Ensure parent directories exist
    std::string parent = getParentDirectory(normalized);
    while (!parent.empty()) {
      m_directories.insert(parent);
      parent = getParentDirectory(parent);
    }
  }

  // =========================================================================
  // Test Helpers - Verification
  // =========================================================================

  /**
   * @brief Get number of write operations
   * @return Write count
   */
  [[nodiscard]] int getWriteCount() const { return m_writeCount; }

  /**
   * @brief Get number of delete operations
   * @return Delete count
   */
  [[nodiscard]] int getDeleteCount() const { return m_deleteCount; }

  /**
   * @brief Get number of copy operations
   * @return Copy count
   */
  [[nodiscard]] int getCopyCount() const { return m_copyCount; }

  /**
   * @brief Get number of directory creation operations
   * @return Create directory count
   */
  [[nodiscard]] int getCreateDirCount() const { return m_createDirCount; }

  /**
   * @brief Get number of directory deletion operations
   * @return Delete directory count
   */
  [[nodiscard]] int getDeleteDirCount() const { return m_deleteDirCount; }

  /**
   * @brief Get all files in the mock file system
   * @return Map of path to content
   */
  [[nodiscard]] const std::map<std::string, std::string>& getFiles() const { return m_files; }

  /**
   * @brief Get all directories in the mock file system
   * @return Set of directory paths
   */
  [[nodiscard]] const std::set<std::string>& getDirectories() const { return m_directories; }

  /**
   * @brief Reset all files, directories, and counters
   */
  void reset() {
    m_files.clear();
    m_directories.clear();
    m_fileInfo.clear();
    m_writeCount = 0;
    m_deleteCount = 0;
    m_copyCount = 0;
    m_createDirCount = 0;
    m_deleteDirCount = 0;
    m_currentTimestamp = 0;
  }

private:
  struct MockFileInfo {
    u64 size = 0;
    u64 lastModified = 0;
  };

  bool matchesFilter(const std::string& path, const std::string& filter) const {
    // Simple wildcard matching
    if (filter == "*") {
      return true;
    }
    if (filter.empty()) {
      return true;
    }

    // Handle *.ext pattern
    if (filter.front() == '*') {
      std::string ext = filter.substr(1);
      return path.size() >= ext.size() && path.substr(path.size() - ext.size()) == ext;
    }

    return path.find(filter) != std::string::npos;
  }

  std::map<std::string, std::string> m_files;
  std::set<std::string> m_directories;
  std::map<std::string, MockFileInfo> m_fileInfo;

  int m_writeCount = 0;
  int m_deleteCount = 0;
  int m_copyCount = 0;
  int m_createDirCount = 0;
  int m_deleteDirCount = 0;
  u64 m_currentTimestamp = 0;
};

} // namespace NovelMind::editor
