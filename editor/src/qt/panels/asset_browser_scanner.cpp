#include "NovelMind/editor/qt/panels/asset_browser_scanner.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_categorizer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace NovelMind::editor::qt {

QString importDestinationForExtension(const QString &extension,
                                      const QString &rootPath) {
  auto &projectManager = ProjectManager::instance();
  if (!projectManager.hasOpenProject()) {
    return QString();
  }

  const QString ext = extension.toLower();
  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
      ext == "gif") {
    return QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Images));
  }
  if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac") {
    return QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Audio));
  }
  if (ext == "ttf" || ext == "otf") {
    return QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Fonts));
  }
  if (ext == "nms") {
    return QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Scripts));
  }
  if (ext == "nmscene") {
    return QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Scenes));
  }

  return QString::fromStdString(
      projectManager.getFolderPath(ProjectFolder::Assets));
}

QString generateUniquePath(const QString &directory,
                          const QString &fileName) {
  QDir dir(directory);
  QString baseName = QFileInfo(fileName).completeBaseName();
  QString suffix = QFileInfo(fileName).suffix();
  QString candidate = dir.filePath(fileName);
  int counter = 1;

  while (QFileInfo::exists(candidate)) {
    QString numbered =
        suffix.isEmpty()
            ? QString("%1_%2").arg(baseName).arg(counter)
            : QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
    candidate = dir.filePath(numbered);
    counter++;
  }

  return candidate;
}

AssetMetadata getAssetMetadata(const QString &path) {
  AssetMetadata meta;
  QFileInfo info(path);

  if (!info.exists()) {
    return meta;
  }

  // Generate a stable ID based on path
  meta.id =
      QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex();
  meta.path = path;
  meta.size = info.size();
  meta.modified = info.lastModified();
  meta.format = info.suffix().toLower();

  // Determine type based on extension
  const QString ext = info.suffix().toLower();
  if (isImageExtension(ext)) {
    meta.type = "image";
    QImageReader reader(path);
    meta.width = reader.size().width();
    meta.height = reader.size().height();
  } else if (isAudioExtension(ext)) {
    meta.type = "audio";
    // Audio duration would require audio library integration
  } else if (ext == "ttf" || ext == "otf" || ext == "woff" || ext == "woff2") {
    meta.type = "font";
  } else if (ext == "nvs" || ext == "json" || ext == "lua") {
    meta.type = "script";
  } else {
    meta.type = "data";
  }

  return meta;
}

} // namespace NovelMind::editor::qt
