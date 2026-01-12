#pragma once

/**
 * @file asset_browser_categorizer.hpp
 * @brief Asset categorization and filtering utilities for the Asset Browser
 *
 * Provides:
 * - Asset type detection by file extension
 * - Proxy model for filtering assets by name and type
 * - Asset metadata categorization
 */

#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QString>

namespace NovelMind::editor::qt {

/**
 * @brief Check if a file extension is an image format
 */
bool isImageExtension(const QString &extension);

/**
 * @brief Check if a file extension is an audio format
 */
bool isAudioExtension(const QString &extension);

/**
 * @brief Proxy model for filtering assets by name and type
 */
class NMAssetFilterProxy final : public QSortFilterProxyModel {
  Q_OBJECT

public:
  explicit NMAssetFilterProxy(QObject *parent = nullptr);

  /**
   * @brief Set the name filter text
   */
  void setNameFilterText(const QString &text);

  /**
   * @brief Set the type filter index (0=All, 1=Images, 2=Audio, etc.)
   */
  void setTypeFilterIndex(int index);

  /**
   * @brief Set the pinned directory for filtering
   */
  void setPinnedDirectory(const QString &path);

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override;

private:
  QString m_nameFilter;
  QString m_pinnedDirectory;
  int m_typeFilterIndex = 0;
};

} // namespace NovelMind::editor::qt
