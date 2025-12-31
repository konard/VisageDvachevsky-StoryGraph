#pragma once

/**
 * @file IFileSystem.hpp
 * @brief File system interface for decoupling from QFile/QDir
 *
 * This interface provides an abstraction layer for file system operations,
 * allowing:
 * - Unit testing with in-memory file systems
 * - Mocking for CI/CD testing
 * - Easy swap of file system backends
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/core/types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief File information structure
 */
struct FileInfo {
  std::string path;
  std::string name;
  std::string extension;
  u64 size = 0;
  u64 lastModified = 0;
  bool isDirectory = false;
  bool exists = false;
};

/**
 * @brief File system interface
 *
 * Provides platform-independent file system operations.
 * Implementations should handle path normalization internally.
 */
class IFileSystem {
public:
  virtual ~IFileSystem() = default;

  // =========================================================================
  // File Existence Checks
  // =========================================================================

  /**
   * @brief Check if a file exists
   * @param path Path to the file
   * @return true if file exists and is a regular file
   */
  [[nodiscard]] virtual bool fileExists(const std::string &path) const = 0;

  /**
   * @brief Check if a directory exists
   * @param path Path to the directory
   * @return true if path exists and is a directory
   */
  [[nodiscard]] virtual bool directoryExists(const std::string &path) const = 0;

  /**
   * @brief Check if a path exists (file or directory)
   * @param path Path to check
   * @return true if path exists
   */
  [[nodiscard]] virtual bool pathExists(const std::string &path) const = 0;

  // =========================================================================
  // File Operations
  // =========================================================================

  /**
   * @brief Read entire file content as string
   * @param path Path to the file
   * @return File content, empty string if read fails
   */
  [[nodiscard]] virtual std::string readFile(const std::string &path) const = 0;

  /**
   * @brief Read file content as binary data
   * @param path Path to the file
   * @return File content as byte vector
   */
  [[nodiscard]] virtual std::vector<u8>
  readBinaryFile(const std::string &path) const = 0;

  /**
   * @brief Write string content to file
   * @param path Path to the file
   * @param content Content to write
   * @return true if write succeeded
   */
  virtual bool writeFile(const std::string &path,
                         const std::string &content) = 0;

  /**
   * @brief Write binary data to file
   * @param path Path to the file
   * @param data Data to write
   * @return true if write succeeded
   */
  virtual bool writeBinaryFile(const std::string &path,
                               const std::vector<u8> &data) = 0;

  /**
   * @brief Delete a file
   * @param path Path to the file
   * @return true if deletion succeeded
   */
  virtual bool deleteFile(const std::string &path) = 0;

  /**
   * @brief Copy a file
   * @param src Source file path
   * @param dest Destination file path
   * @return true if copy succeeded
   */
  virtual bool copyFile(const std::string &src, const std::string &dest) = 0;

  /**
   * @brief Move/rename a file
   * @param src Source file path
   * @param dest Destination file path
   * @return true if move succeeded
   */
  virtual bool moveFile(const std::string &src, const std::string &dest) = 0;

  // =========================================================================
  // Directory Operations
  // =========================================================================

  /**
   * @brief Create a directory
   * @param path Path to create
   * @return true if creation succeeded or directory already exists
   */
  virtual bool createDirectory(const std::string &path) = 0;

  /**
   * @brief Create directory and all parent directories
   * @param path Path to create
   * @return true if creation succeeded
   */
  virtual bool createDirectories(const std::string &path) = 0;

  /**
   * @brief Delete a directory
   * @param path Path to the directory
   * @param recursive If true, delete contents recursively
   * @return true if deletion succeeded
   */
  virtual bool deleteDirectory(const std::string &path, bool recursive = false) = 0;

  // =========================================================================
  // Directory Listing
  // =========================================================================

  /**
   * @brief List files in a directory
   * @param directory Directory path
   * @param filter Optional file extension filter (e.g., "*.txt")
   * @return List of file paths
   */
  [[nodiscard]] virtual std::vector<std::string>
  listFiles(const std::string &directory,
            const std::string &filter = "*") const = 0;

  /**
   * @brief List subdirectories in a directory
   * @param directory Directory path
   * @return List of directory paths
   */
  [[nodiscard]] virtual std::vector<std::string>
  listDirectories(const std::string &directory) const = 0;

  /**
   * @brief List files recursively in a directory
   * @param directory Directory path
   * @param filter Optional file extension filter
   * @return List of file paths
   */
  [[nodiscard]] virtual std::vector<std::string>
  listFilesRecursive(const std::string &directory,
                     const std::string &filter = "*") const = 0;

  // =========================================================================
  // File Information
  // =========================================================================

  /**
   * @brief Get file information
   * @param path Path to file or directory
   * @return FileInfo structure
   */
  [[nodiscard]] virtual FileInfo getFileInfo(const std::string &path) const = 0;

  /**
   * @brief Get file size in bytes
   * @param path Path to file
   * @return File size, 0 if file doesn't exist
   */
  [[nodiscard]] virtual u64 getFileSize(const std::string &path) const = 0;

  /**
   * @brief Get file last modification time
   * @param path Path to file
   * @return Modification time as Unix timestamp (ms since epoch)
   */
  [[nodiscard]] virtual u64
  getLastModified(const std::string &path) const = 0;

  // =========================================================================
  // Path Utilities
  // =========================================================================

  /**
   * @brief Get the file name from a path
   * @param path Full file path
   * @return File name with extension
   */
  [[nodiscard]] virtual std::string getFileName(const std::string &path) const = 0;

  /**
   * @brief Get the file name without extension
   * @param path Full file path
   * @return File name without extension
   */
  [[nodiscard]] virtual std::string getBaseName(const std::string &path) const = 0;

  /**
   * @brief Get the file extension
   * @param path Full file path
   * @return File extension including dot (e.g., ".txt")
   */
  [[nodiscard]] virtual std::string getExtension(const std::string &path) const = 0;

  /**
   * @brief Get the parent directory of a path
   * @param path Full path
   * @return Parent directory path
   */
  [[nodiscard]] virtual std::string
  getParentDirectory(const std::string &path) const = 0;

  /**
   * @brief Normalize a path (resolve . and .., use native separators)
   * @param path Path to normalize
   * @return Normalized path
   */
  [[nodiscard]] virtual std::string normalizePath(const std::string &path) const = 0;

  /**
   * @brief Join path components
   * @param base Base path
   * @param component Path component to append
   * @return Combined path
   */
  [[nodiscard]] virtual std::string
  joinPath(const std::string &base, const std::string &component) const = 0;
};

/**
 * @brief Factory function type for creating file systems
 */
using FileSystemFactory = std::function<std::unique_ptr<IFileSystem>()>;

} // namespace NovelMind::editor
