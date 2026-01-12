/**
 * @file script_project_context.cpp
 * @brief Implementation of project context for script validation
 */

#include "NovelMind/editor/script_project_context.hpp"
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace NovelMind::editor {

namespace {

// Standard image extensions for backgrounds and sprites
const std::vector<QString> kImageExtensions = {".png", ".jpg", ".jpeg", ".webp",
                                               ".bmp", ".gif", ".tga"};

// Standard audio extensions
const std::vector<QString> kAudioExtensions = {".wav", ".ogg", ".mp3", ".opus"};

} // namespace

ScriptProjectContext::ScriptProjectContext(const QString& projectPath)
    : m_projectPath(projectPath) {}

void ScriptProjectContext::setProjectPath(const QString& projectPath) {
  m_projectPath = projectPath;
}

QString ScriptProjectContext::assetsPath() const {
  return m_projectPath + "/assets";
}

QString ScriptProjectContext::backgroundsPath() const {
  // Try both "backgrounds" and "images" directories
  QString bgPath = assetsPath() + "/backgrounds";
  if (QDir(bgPath).exists()) {
    return bgPath;
  }
  return assetsPath() + "/images";
}

QString ScriptProjectContext::audioPath(const std::string& mediaType) const {
  QString audioDir = assetsPath() + "/audio";

  // Map media type to subdirectory
  if (mediaType == "music") {
    QString musicPath = audioDir + "/music";
    if (QDir(musicPath).exists()) {
      return musicPath;
    }
  } else if (mediaType == "sound") {
    QString sfxPath = audioDir + "/sfx";
    if (QDir(sfxPath).exists()) {
      return sfxPath;
    }
    // Also check "sounds" directory
    QString soundsPath = audioDir + "/sounds";
    if (QDir(soundsPath).exists()) {
      return soundsPath;
    }
  } else if (mediaType == "voice") {
    QString voicePath = audioDir + "/voice";
    if (QDir(voicePath).exists()) {
      return voicePath;
    }
  }

  // Fallback to main audio directory
  return audioDir;
}

QString ScriptProjectContext::spritesPath() const {
  // Try both "sprites" and "characters" directories
  QString spritesDir = assetsPath() + "/sprites";
  if (QDir(spritesDir).exists()) {
    return spritesDir;
  }

  QString charsDir = assetsPath() + "/characters";
  if (QDir(charsDir).exists()) {
    return charsDir;
  }

  // Fallback to images directory
  return assetsPath() + "/images";
}

bool ScriptProjectContext::fileExistsWithExtensions(const QString& directory,
                                                    const QString& baseName,
                                                    const std::vector<QString>& extensions) const {
  QDir dir(directory);
  if (!dir.exists()) {
    return false;
  }

  for (const QString& ext : extensions) {
    QString fullPath = directory + "/" + baseName + ext;
    if (QFileInfo::exists(fullPath)) {
      return true;
    }
  }

  return false;
}

std::vector<std::string>
ScriptProjectContext::getFilesInDirectory(const QString& directory,
                                          const std::vector<QString>& extensions) const {
  std::vector<std::string> result;

  QDir dir(directory);
  if (!dir.exists()) {
    return result;
  }

  QStringList nameFilters;
  for (const QString& ext : extensions) {
    nameFilters << ("*" + ext);
  }

  QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files, QDir::Name);
  for (const QFileInfo& fileInfo : files) {
    result.push_back(fileInfo.completeBaseName().toStdString());
  }

  return result;
}

bool ScriptProjectContext::backgroundExists(const std::string& assetId) const {
  QString bgId = QString::fromStdString(assetId);
  return fileExistsWithExtensions(backgroundsPath(), bgId, kImageExtensions);
}

bool ScriptProjectContext::audioExists(const std::string& assetPath,
                                       const std::string& mediaType) const {
  QString audioFile = QString::fromStdString(assetPath);
  QString audioDir = audioPath(mediaType);

  // Check if file has extension already
  QFileInfo fileInfo(audioFile);
  if (fileInfo.suffix().isEmpty()) {
    // No extension, try all audio extensions
    return fileExistsWithExtensions(audioDir, audioFile, kAudioExtensions);
  } else {
    // Has extension, check directly
    QString fullPath = audioDir + "/" + audioFile;
    return QFileInfo::exists(fullPath);
  }
}

bool ScriptProjectContext::characterSpriteExists(const std::string& characterId) const {
  QString charId = QString::fromStdString(characterId);
  QString spritePath = spritesPath();

  // Check for files starting with "char_" prefix
  QString withPrefix = "char_" + charId;
  if (fileExistsWithExtensions(spritePath, withPrefix, kImageExtensions)) {
    return true;
  }

  // Check for files with character ID directly
  if (fileExistsWithExtensions(spritePath, charId, kImageExtensions)) {
    return true;
  }

  // Check for character-specific subdirectory
  QString charDir = spritePath + "/" + charId;
  if (QDir(charDir).exists()) {
    QDir dir(charDir);
    QStringList nameFilters;
    for (const QString& ext : kImageExtensions) {
      nameFilters << ("*" + ext);
    }
    if (!dir.entryList(nameFilters, QDir::Files).isEmpty()) {
      return true;
    }
  }

  return false;
}

std::vector<std::string> ScriptProjectContext::getAvailableBackgrounds() const {
  return getFilesInDirectory(backgroundsPath(), kImageExtensions);
}

std::vector<std::string>
ScriptProjectContext::getAvailableAudio(const std::string& mediaType) const {
  return getFilesInDirectory(audioPath(mediaType), kAudioExtensions);
}

std::vector<std::string> ScriptProjectContext::getAvailableCharacters() const {
  std::vector<std::string> characters;
  QString spritePath = spritesPath();

  // Get all image files
  std::vector<std::string> files = getFilesInDirectory(spritePath, kImageExtensions);

  // Extract character names (remove "char_" prefix if present)
  for (const std::string& file : files) {
    std::string charName = file;
    if (charName.starts_with("char_")) {
      charName = charName.substr(5); // Remove "char_" prefix
    }
    characters.push_back(charName);
  }

  // Also check for subdirectories (each directory represents a character)
  QDir dir(spritePath);
  if (dir.exists()) {
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& subdir : subdirs) {
      characters.push_back(subdir.fileName().toStdString());
    }
  }

  // Remove duplicates and sort
  std::sort(characters.begin(), characters.end());
  characters.erase(std::unique(characters.begin(), characters.end()), characters.end());

  return characters;
}

} // namespace NovelMind::editor
