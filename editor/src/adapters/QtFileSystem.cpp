/**
 * @file QtFileSystem.cpp
 * @brief Qt-based implementation of IFileSystem interface
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/QtFileSystem.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace NovelMind::editor {

bool QtFileSystem::fileExists(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.exists() && info.isFile();
}

bool QtFileSystem::directoryExists(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.exists() && info.isDir();
}

bool QtFileSystem::pathExists(const std::string &path) const {
  return QFileInfo::exists(QString::fromStdString(path));
}

std::string QtFileSystem::readFile(const std::string &path) const {
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return "";
  }

  QByteArray content = file.readAll();
  file.close();
  return content.toStdString();
}

std::vector<u8> QtFileSystem::readBinaryFile(const std::string &path) const {
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  QByteArray content = file.readAll();
  file.close();

  std::vector<u8> result(static_cast<size_t>(content.size()));
  std::copy(content.begin(), content.end(), result.begin());
  return result;
}

bool QtFileSystem::writeFile(const std::string &path,
                             const std::string &content) {
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  qint64 written = file.write(content.c_str(), static_cast<qint64>(content.size()));
  file.close();
  return written == static_cast<qint64>(content.size());
}

bool QtFileSystem::writeBinaryFile(const std::string &path,
                                   const std::vector<u8> &data) {
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  qint64 written =
      file.write(reinterpret_cast<const char *>(data.data()),
                 static_cast<qint64>(data.size()));
  file.close();
  return written == static_cast<qint64>(data.size());
}

bool QtFileSystem::deleteFile(const std::string &path) {
  return QFile::remove(QString::fromStdString(path));
}

bool QtFileSystem::copyFile(const std::string &src, const std::string &dest) {
  // Remove destination if it exists (QFile::copy fails if dest exists)
  if (QFile::exists(QString::fromStdString(dest))) {
    QFile::remove(QString::fromStdString(dest));
  }
  return QFile::copy(QString::fromStdString(src),
                     QString::fromStdString(dest));
}

bool QtFileSystem::moveFile(const std::string &src, const std::string &dest) {
  return QFile::rename(QString::fromStdString(src),
                       QString::fromStdString(dest));
}

bool QtFileSystem::createDirectory(const std::string &path) {
  QDir dir;
  return dir.mkdir(QString::fromStdString(path));
}

bool QtFileSystem::createDirectories(const std::string &path) {
  QDir dir;
  return dir.mkpath(QString::fromStdString(path));
}

bool QtFileSystem::deleteDirectory(const std::string &path, bool recursive) {
  QDir dir(QString::fromStdString(path));
  if (!dir.exists()) {
    return false;
  }

  if (recursive) {
    return dir.removeRecursively();
  }
  return dir.rmdir(".");
}

std::vector<std::string>
QtFileSystem::listFiles(const std::string &directory,
                        const std::string &filter) const {
  QDir dir(QString::fromStdString(directory));
  if (!dir.exists()) {
    return {};
  }

  QStringList filters;
  filters << QString::fromStdString(filter);

  QStringList files = dir.entryList(filters, QDir::Files);

  std::vector<std::string> result;
  result.reserve(static_cast<size_t>(files.size()));
  for (const QString &file : files) {
    result.push_back(dir.filePath(file).toStdString());
  }
  return result;
}

std::vector<std::string>
QtFileSystem::listDirectories(const std::string &directory) const {
  QDir dir(QString::fromStdString(directory));
  if (!dir.exists()) {
    return {};
  }

  QStringList dirs =
      dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  std::vector<std::string> result;
  result.reserve(static_cast<size_t>(dirs.size()));
  for (const QString &d : dirs) {
    result.push_back(dir.filePath(d).toStdString());
  }
  return result;
}

std::vector<std::string>
QtFileSystem::listFilesRecursive(const std::string &directory,
                                 const std::string &filter) const {
  std::vector<std::string> result;

  QStringList filters;
  filters << QString::fromStdString(filter);

  QDirIterator it(QString::fromStdString(directory), filters,
                  QDir::Files, QDirIterator::Subdirectories);

  while (it.hasNext()) {
    result.push_back(it.next().toStdString());
  }
  return result;
}

FileInfo QtFileSystem::getFileInfo(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));

  FileInfo result;
  result.path = info.filePath().toStdString();
  result.name = info.fileName().toStdString();
  result.extension = info.suffix().toStdString();
  if (!result.extension.empty()) {
    result.extension = "." + result.extension;
  }
  result.size = static_cast<u64>(info.size());
  result.lastModified =
      static_cast<u64>(info.lastModified().toMSecsSinceEpoch());
  result.isDirectory = info.isDir();
  result.exists = info.exists();

  return result;
}

u64 QtFileSystem::getFileSize(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.exists() ? static_cast<u64>(info.size()) : 0;
}

u64 QtFileSystem::getLastModified(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.exists()
             ? static_cast<u64>(info.lastModified().toMSecsSinceEpoch())
             : 0;
}

std::string QtFileSystem::getFileName(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.fileName().toStdString();
}

std::string QtFileSystem::getBaseName(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.baseName().toStdString();
}

std::string QtFileSystem::getExtension(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  QString suffix = info.suffix();
  return suffix.isEmpty() ? "" : ("." + suffix).toStdString();
}

std::string QtFileSystem::getParentDirectory(const std::string &path) const {
  QFileInfo info(QString::fromStdString(path));
  return info.path().toStdString();
}

std::string QtFileSystem::normalizePath(const std::string &path) const {
  QDir dir(QString::fromStdString(path));
  return dir.cleanPath(QString::fromStdString(path)).toStdString();
}

std::string QtFileSystem::joinPath(const std::string &base,
                                   const std::string &component) const {
  QDir dir(QString::fromStdString(base));
  return dir.filePath(QString::fromStdString(component)).toStdString();
}

} // namespace NovelMind::editor
