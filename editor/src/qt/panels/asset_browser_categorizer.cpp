#include "NovelMind/editor/qt/panels/asset_browser_categorizer.hpp"

#include <QDir>
#include <QFileInfo>

namespace NovelMind::editor::qt {

bool isImageExtension(const QString &extension) {
  const QString ext = extension.toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif";
}

bool isAudioExtension(const QString &extension) {
  const QString ext = extension.toLower();
  return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
}

NMAssetFilterProxy::NMAssetFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent) {}

void NMAssetFilterProxy::setNameFilterText(const QString &text) {
  m_nameFilter = text.trimmed();
  invalidateFilter();
}

void NMAssetFilterProxy::setTypeFilterIndex(int index) {
  m_typeFilterIndex = index;
  invalidateFilter();
}

void NMAssetFilterProxy::setPinnedDirectory(const QString &path) {
  m_pinnedDirectory = QDir::cleanPath(path);
  invalidateFilter();
}

bool NMAssetFilterProxy::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const {
  auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
  if (!fsModel) {
    return true;
  }

  QModelIndex idx = fsModel->index(sourceRow, 0, sourceParent);
  if (!idx.isValid()) {
    return false;
  }

  const QFileInfo info = fsModel->fileInfo(idx);
  if (info.isDir()) {
    return true;
  }

  if (!info.isFile()) {
    return false;
  }

  if (!m_nameFilter.isEmpty()) {
    if (!info.fileName().contains(m_nameFilter, Qt::CaseInsensitive)) {
      return false;
    }
  }

  if (m_typeFilterIndex == 0) {
    return true;
  }

  const QString ext = info.suffix().toLower();
  switch (m_typeFilterIndex) {
  case 1: // Images
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
           ext == "gif";
  case 2: // Audio
    return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
  case 3: // Fonts
    return ext == "ttf" || ext == "otf";
  case 4: // Scripts
    return ext == "nms" || ext == "nmscene";
  case 5: // Data
    return ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml";
  default:
    return true;
  }
}

} // namespace NovelMind::editor::qt
