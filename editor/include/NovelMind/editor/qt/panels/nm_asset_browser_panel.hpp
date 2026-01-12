#pragma once

/**
 * @file nm_asset_browser_panel.hpp
 * @brief Asset Browser panel for managing project assets
 *
 * Provides:
 * - Directory tree navigation
 * - Asset grid/list view toggle
 * - Asset preview (images, audio waveforms, duration/format)
 * - Context menu: rename, delete, duplicate, reimport, show in explorer
 * - AssetDatabase info: ID, type, size
 * - Stable IDs during renaming
 * - Import/export controls
 * - Undo/redo for asset operations
 * - Lazy thumbnail loading with task cancellation
 * - Memory-bounded thumbnail cache with LRU eviction
 */

#include "NovelMind/editor/qt/lazy_thumbnail_loader.hpp"
#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_categorizer.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_preview.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_scanner.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_thumbnails.hpp"
#include <QAction>
#include <QCache>
#include <QComboBox>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QThreadPool>
#include <QToolBar>
#include <QTreeView>
#include <QUndoStack>
#include <memory>

namespace NovelMind::editor::qt {

/**
 * @brief View mode for asset list
 */
enum class AssetViewMode { Grid, List };

/**
 * @brief Asset Browser panel for asset management
 */
class NMAssetBrowserPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMAssetBrowserPanel(QWidget *parent = nullptr);
  ~NMAssetBrowserPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Set the root path for the asset browser
   */
  void setRootPath(const QString &path);

  /**
   * @brief Get the currently selected asset path
   */
  [[nodiscard]] QString selectedAssetPath() const;

  /**
   * @brief Refresh the asset view
   */
  void refresh();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

  /**
   * @brief Set the view mode (grid/list)
   */
  void setViewMode(AssetViewMode mode);
  [[nodiscard]] AssetViewMode viewMode() const { return m_viewMode; }

  /**
   * @brief Get asset metadata
   */
  [[nodiscard]] AssetMetadata getAssetMetadata(const QString &path) const;

  /**
   * @brief Rename an asset (maintains stable ID)
   */
  bool renameAsset(const QString &oldPath, const QString &newName);

  /**
   * @brief Delete an asset
   */
  bool deleteAsset(const QString &path);

  /**
   * @brief Duplicate an asset
   */
  QString duplicateAsset(const QString &path);

  /**
   * @brief Reimport an asset
   */
  bool reimportAsset(const QString &path);

  /**
   * @brief Show asset in system file explorer
   */
  void showInExplorer(const QString &path);

  /**
   * @brief Navigate selection history
   */
  void navigateBack();
  void navigateForward();

signals:
  void assetSelected(const QString &path);
  void assetDoubleClicked(const QString &path);
  void assetContextMenu(const QString &path, const QPoint &globalPos);
  void assetRenamed(const QString &oldPath, const QString &newPath);
  void assetDeleted(const QString &path);
  void assetDuplicated(const QString &originalPath, const QString &newPath);

private slots:
  void onTreeClicked(const QModelIndex &index);
  void onListDoubleClicked(const QModelIndex &index);
  void onListContextMenu(const QPoint &pos);
  void onImportAssets();
  void onListSelectionChanged(const QModelIndex &current,
                              const QModelIndex &previous);
  void onFilterTextChanged(const QString &text);
  void onTypeFilterChanged(int index);
  void onViewModeChanged(int index);
  void onRenameAction();
  void onDeleteAction();
  void onDuplicateAction();
  void onReimportAction();
  void onShowInExplorerAction();
  void onCopyPathAction();
  void onCopyIdAction();
  void onThumbnailReady(const QString &path, const QPixmap &pixmap);

private:
  void setupToolBar();
  void setupContent();
  void applyFilters();
  void setPreviewVisible(bool visible);
  void updateThumbnailSize(int size);
  void updatePreview(const QString &path);
  void clearPreview();
  void importFiles(const QStringList &files, bool interactive);
  void scheduleVisibleThumbnails();
  void cancelPendingThumbnails();

public: // Used by NMAssetIconProvider
  QPixmap generateAudioWaveform(const QString &path, const QSize &size) const;
  bool isThumbnailValid(const QString &path,
                        const ThumbnailCacheEntry &entry) const;

  // PERF-3: Request async thumbnail loading (for use by NMAssetIconProvider)
  void requestAsyncThumbnail(const QString &path, const QSize &size,
                             int priority = 5);

  // Thumbnail cache with LRU eviction (max size in KB)
  QCache<QString, ThumbnailCacheEntry> m_thumbnailCache;

private:
  QSplitter *m_splitter = nullptr;
  QTreeView *m_treeView = nullptr;
  QListView *m_listView = nullptr;
  QWidget *m_listPane = nullptr;
  NMAssetPreviewFrame *m_previewFrame = nullptr;
  QFileSystemModel *m_treeModel = nullptr;
  QFileSystemModel *m_listModel = nullptr;
  QSortFilterProxyModel *m_filterProxy = nullptr;
  QFileIconProvider *m_iconProvider = nullptr;
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  QLineEdit *m_filterEdit = nullptr;
  QComboBox *m_typeFilter = nullptr;
  QAction *m_togglePreviewAction = nullptr;
  QComboBox *m_thumbSizeCombo = nullptr;

  QString m_rootPath;
  QString m_currentPath;
  int m_thumbSize = 80;
  bool m_previewVisible = true;
  AssetViewMode m_viewMode = AssetViewMode::Grid;

  // Asset metadata cache
  mutable QHash<QString, AssetMetadata> m_metadataCache;

  // Selection history for back/forward navigation
  QStringList m_selectionHistory;
  int m_historyIndex = -1;

  // Context menu actions
  QAction *m_renameAction = nullptr;
  QAction *m_deleteAction = nullptr;
  QAction *m_duplicateAction = nullptr;
  QAction *m_reimportAction = nullptr;
  QAction *m_showInExplorerAction = nullptr;
  QAction *m_copyPathAction = nullptr;
  QAction *m_copyIdAction = nullptr;

  // View mode toggle
  QComboBox *m_viewModeCombo = nullptr;
  QAction *m_backAction = nullptr;
  QAction *m_forwardAction = nullptr;

  // Undo stack for asset operations
  QUndoStack *m_undoStack = nullptr;

  // Audio waveform display
  QLabel *m_waveformLabel = nullptr;

  // Thread pool for background thumbnail loading (legacy - kept for
  // compatibility)
  QPointer<QThreadPool> m_thumbnailThreadPool;

  // Pending thumbnail requests (for cancellation)
  QSet<QString> m_pendingThumbnails;

  // Lazy thumbnail loader with task cancellation and parallelism limits
  std::unique_ptr<LazyThumbnailLoader> m_lazyLoader;

  // Visible items tracking for prioritizing thumbnail loading
  QSet<QString> m_visiblePaths;
  QTimer *m_visibilityUpdateTimer = nullptr;

  void updateVisibleItems();
  void onLazyThumbnailReady(const QString &path, const QPixmap &pixmap);
};

} // namespace NovelMind::editor::qt
