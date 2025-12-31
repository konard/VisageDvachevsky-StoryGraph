#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/performance_metrics.hpp"
#include "NovelMind/editor/qt/qt_event_bus.hpp"

#include <QDebug>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPixmapCache>
#include <QProcess>
#include <QPushButton>
#include <QRunnable>
#include <QScrollBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#if defined(__GNUC__)
// Suppress GCC false positives in Qt container inlines for this translation
// unit.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

namespace NovelMind::editor::qt {

namespace {

constexpr int kAssetThumbSize = 80;
constexpr int kPreviewHeight = 140;

bool isImageExtension(const QString &extension) {
  const QString ext = extension.toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif";
}

bool isAudioExtension(const QString &extension) {
  const QString ext = extension.toLower();
  return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
}

class NMAssetFilterProxy final : public QSortFilterProxyModel {
public:
  explicit NMAssetFilterProxy(QObject *parent = nullptr)
      : QSortFilterProxyModel(parent) {}

  void setNameFilterText(const QString &text) {
    m_nameFilter = text.trimmed();
    invalidateFilter();
  }

  void setTypeFilterIndex(int index) {
    m_typeFilterIndex = index;
    invalidateFilter();
  }

  void setPinnedDirectory(const QString &path) {
    m_pinnedDirectory = QDir::cleanPath(path);
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override {
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

private:
  QString m_nameFilter;
  QString m_pinnedDirectory;
  int m_typeFilterIndex = 0;
};

} // namespace

class NMAssetIconProvider : public QFileIconProvider {
public:
  explicit NMAssetIconProvider(NMAssetBrowserPanel *panel, QSize iconSize)
      : QFileIconProvider(), m_panel(panel), m_iconSize(iconSize) {}

  void setIconSize(const QSize &size) { m_iconSize = size; }

  QIcon icon(const QFileInfo &info) const override {
    if (!info.isFile()) {
      return QFileIconProvider::icon(info);
    }

    const QString path = info.absoluteFilePath();
    const QString ext = info.suffix();

    // Check for images
    if (isImageExtension(ext)) {
      // Try to get from panel's cache
      if (m_panel && m_panel->m_thumbnailCache.contains(path)) {
        auto *entry = m_panel->m_thumbnailCache.object(path);
        if (entry && m_panel->isThumbnailValid(path, *entry)) {
          return QIcon(entry->pixmap);
        }
      }

      // Load synchronously for now (lazy loading will be added later)
      QImageReader reader(path);
      reader.setAutoTransform(true);
      QImage image = reader.read();
      if (!image.isNull()) {
        QPixmap pix = QPixmap::fromImage(image.scaled(
            m_iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // Cache it
        if (m_panel) {
          auto *entry =
              new ThumbnailCacheEntry{pix, info.lastModified(), info.size()};
          m_panel->m_thumbnailCache.insert(
              path, entry,
              static_cast<int>(pix.width() * pix.height() * 4 / 1024));
        }
        return QIcon(pix);
      }
    }

    // Check for audio files
    if (isAudioExtension(ext)) {
      if (m_panel) {
        QPixmap waveform = m_panel->generateAudioWaveform(path, m_iconSize);
        if (!waveform.isNull()) {
          return QIcon(waveform);
        }
      }
    }

    return QFileIconProvider::icon(info);
  }

private:
  QPointer<NMAssetBrowserPanel> m_panel;
  QSize m_iconSize;
};

NMAssetBrowserPanel::NMAssetBrowserPanel(QWidget *parent)
    : NMDockPanel(tr("Asset Browser"), parent) {
  setPanelId("AssetBrowser");

  // Asset Browser needs width for thumbnails grid and tree view,
  // height to show multiple rows of assets
  setMinimumPanelSize(300, 200);

  // Initialize thumbnail cache (max 50MB)
  m_thumbnailCache.setMaxCost(50 * 1024);

  // Initialize thread pool for background loading (legacy)
  m_thumbnailThreadPool = new QThreadPool(this);
  m_thumbnailThreadPool->setMaxThreadCount(2);

  // Initialize lazy thumbnail loader with proper config
  ThumbnailLoaderConfig loaderConfig;
  loaderConfig.maxConcurrentTasks = 2;
  loaderConfig.maxCacheSizeKB = 50 * 1024; // 50 MB
  loaderConfig.thumbnailSize = 80;
  loaderConfig.queueHighWaterMark = 100;
  m_lazyLoader = std::make_unique<LazyThumbnailLoader>(loaderConfig, this);

  // Connect lazy loader signals
  connect(m_lazyLoader.get(), &LazyThumbnailLoader::thumbnailReady, this,
          &NMAssetBrowserPanel::onLazyThumbnailReady);

  // Setup visibility update timer for lazy loading visible items
  m_visibilityUpdateTimer = new QTimer(this);
  m_visibilityUpdateTimer->setInterval(100); // 100ms debounce
  m_visibilityUpdateTimer->setSingleShot(true);
  connect(m_visibilityUpdateTimer, &QTimer::timeout, this,
          &NMAssetBrowserPanel::updateVisibleItems);

  setupContent();
  setupToolBar();

  // Subscribe to asset import events to auto-refresh when assets are imported
  // from other panels (e.g., Inspector Panel)
  connect(&QtEventBus::instance(), &QtEventBus::assetImported, this,
          [this](const QString &assetPath, const QString &targetDir) {
            Q_UNUSED(assetPath);
            // Refresh the view if the import target is within our current view
            if (!targetDir.isEmpty() && !m_rootPath.isEmpty()) {
              if (targetDir.startsWith(m_rootPath)) {
                qDebug() << "Asset Browser: Auto-refreshing after import to"
                         << targetDir;
                refresh();
              }
            } else {
              // Always refresh if no target dir specified
              refresh();
            }
          });

  // Subscribe to asset deleted events
  connect(&QtEventBus::instance(), &QtEventBus::assetDeleted, this,
          [this](const QString &assetPath) {
            Q_UNUSED(assetPath);
            refresh();
          });

  // Subscribe to asset renamed events
  connect(&QtEventBus::instance(), &QtEventBus::assetRenamed, this,
          [this](const QString &oldPath, const QString &newPath) {
            Q_UNUSED(oldPath);
            Q_UNUSED(newPath);
            refresh();
          });
}

NMAssetBrowserPanel::~NMAssetBrowserPanel() {
  // Stop visibility timer first
  if (m_visibilityUpdateTimer) {
    m_visibilityUpdateTimer->stop();
  }

  // Cancel all pending lazy loading tasks (safe shutdown)
  if (m_lazyLoader) {
    m_lazyLoader->cancelPending();
    // LazyThumbnailLoader destructor handles waiting for tasks
  }

  // Legacy cleanup
  cancelPendingThumbnails();
  if (m_thumbnailThreadPool) {
    m_thumbnailThreadPool->waitForDone(1000);
  }
}

void NMAssetBrowserPanel::onInitialize() {
  auto &projectManager = ProjectManager::instance();
  if (projectManager.hasOpenProject()) {
    setRootPath(QString::fromStdString(
        projectManager.getFolderPath(ProjectFolder::Assets)));
  } else {
    setRootPath(QDir::homePath());
  }
}

void NMAssetBrowserPanel::onUpdate(double /*deltaTime*/) {
  // No continuous update needed
}

void NMAssetBrowserPanel::setRootPath(const QString &path) {
  m_rootPath = path;
  m_currentPath = path;

  if (m_treeModel) {
    m_treeModel->setRootPath(path);
    m_treeView->setRootIndex(m_treeModel->index(path));
  }

  if (m_listModel) {
    if (m_filterProxy) {
      if (auto *proxy = static_cast<NMAssetFilterProxy *>(m_filterProxy)) {
        proxy->setPinnedDirectory(path);
      }
    }
    m_listModel->setRootPath(path);
    QModelIndex idx = m_listModel->index(path);
    m_listView->setRootIndex(m_filterProxy ? m_filterProxy->mapFromSource(idx)
                                           : idx);
  }
  updatePreview(selectedAssetPath());
}

QString NMAssetBrowserPanel::selectedAssetPath() const {
  if (m_listView && m_listModel) {
    QModelIndex index = m_listView->currentIndex();
    if (index.isValid()) {
      QModelIndex source =
          m_filterProxy ? m_filterProxy->mapToSource(index) : index;
      return m_listModel->filePath(source);
    }
  }
  return QString();
}

void NMAssetBrowserPanel::refresh() {
  if (m_listModel && !m_currentPath.isEmpty()) {
    if (m_filterProxy) {
      if (auto *proxy = static_cast<NMAssetFilterProxy *>(m_filterProxy)) {
        proxy->setPinnedDirectory(m_currentPath);
      }
    }
    QModelIndex idx = m_listModel->index(m_currentPath);
    m_listView->setRootIndex(m_filterProxy ? m_filterProxy->mapFromSource(idx)
                                           : idx);
  }
  updatePreview(selectedAssetPath());
}

void NMAssetBrowserPanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("AssetBrowserToolBar");
  m_toolBar->setIconSize(QSize(14, 14));

  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter by name..."));
  m_filterEdit->setClearButtonEnabled(true);
  m_filterEdit->setMaximumWidth(200);
  m_filterEdit->setToolTip(tr("Filter assets by name"));
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &NMAssetBrowserPanel::onFilterTextChanged);
  m_toolBar->addWidget(m_filterEdit);

  m_typeFilter = new QComboBox(this);
  m_typeFilter->addItems({tr("All"), tr("Images"), tr("Audio"), tr("Fonts"),
                          tr("Scripts"), tr("Data")});
  m_typeFilter->setMaximumWidth(120);
  m_typeFilter->setToolTip(tr("Filter assets by type"));
  connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMAssetBrowserPanel::onTypeFilterChanged);
  m_toolBar->addWidget(m_typeFilter);

  m_toolBar->addSeparator();

  QAction *actionRefresh = m_toolBar->addAction(tr("Refresh"));
  actionRefresh->setToolTip(tr("Refresh Asset List"));
  connect(actionRefresh, &QAction::triggered, this,
          &NMAssetBrowserPanel::refresh);

  m_togglePreviewAction = m_toolBar->addAction(tr("Preview"));
  m_togglePreviewAction->setCheckable(true);
  m_togglePreviewAction->setChecked(m_previewVisible);
  m_togglePreviewAction->setToolTip(tr("Toggle asset preview pane"));
  connect(m_togglePreviewAction, &QAction::toggled, this,
          &NMAssetBrowserPanel::setPreviewVisible);

  m_thumbSizeCombo = new QComboBox(this);
  m_thumbSizeCombo->addItem(tr("Small"), 64);
  m_thumbSizeCombo->addItem(tr("Medium"), 80);
  m_thumbSizeCombo->addItem(tr("Large"), 96);
  const int thumbIndex = m_thumbSizeCombo->findData(m_thumbSize);
  if (thumbIndex >= 0) {
    m_thumbSizeCombo->setCurrentIndex(thumbIndex);
  } else {
    m_thumbSizeCombo->setCurrentIndex(1);
  }
  m_thumbSizeCombo->setToolTip(tr("Thumbnail size"));
  connect(m_thumbSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            if (!m_thumbSizeCombo) {
              return;
            }
            const int size = m_thumbSizeCombo->currentData().toInt();
            if (size > 0) {
              updateThumbnailSize(size);
            }
          });
  m_toolBar->addWidget(m_thumbSizeCombo);

  m_toolBar->addSeparator();

  // View mode toggle
  m_viewModeCombo = new QComboBox(this);
  m_viewModeCombo->addItem(tr("Grid"), static_cast<int>(AssetViewMode::Grid));
  m_viewModeCombo->addItem(tr("List"), static_cast<int>(AssetViewMode::List));
  m_viewModeCombo->setCurrentIndex(0);
  m_viewModeCombo->setToolTip(tr("Toggle between grid and list view"));
  connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMAssetBrowserPanel::onViewModeChanged);
  m_toolBar->addWidget(m_viewModeCombo);

  m_toolBar->addSeparator();

  QAction *actionImport = m_toolBar->addAction(tr("Import"));
  actionImport->setToolTip(tr("Import Assets"));
  connect(actionImport, &QAction::triggered, this,
          &NMAssetBrowserPanel::onImportAssets);

  if (auto *layout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_toolBar);
  }
}

void NMAssetBrowserPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create splitter for tree/list view
  m_splitter = new QSplitter(Qt::Horizontal, m_contentWidget);

  // Tree view for directory navigation
  m_treeModel = new QFileSystemModel(this);
  m_treeModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

  m_treeView = new QTreeView(m_splitter);
  m_treeView->setModel(m_treeModel);
  m_treeView->setHeaderHidden(true);
  // Hide all columns except name
  for (int i = 1; i < m_treeModel->columnCount(); ++i) {
    m_treeView->hideColumn(i);
  }
  m_treeView->setAnimated(true);
  m_treeView->setIndentation(16);

  connect(m_treeView, &QTreeView::clicked, this,
          &NMAssetBrowserPanel::onTreeClicked);

  m_splitter->addWidget(m_treeView);

  // List view for asset display
  m_listModel = new QFileSystemModel(this);
  m_listModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

  m_filterProxy = new NMAssetFilterProxy(this);
  m_filterProxy->setSourceModel(m_listModel);

  m_listView = new QListView(m_splitter);
  m_listView->setObjectName("AssetBrowserListView");
  m_listView->setModel(m_filterProxy);
  m_listView->setViewMode(QListView::IconMode);
  m_listView->setIconSize(QSize(kAssetThumbSize, kAssetThumbSize));
  m_listView->setGridSize(QSize(kAssetThumbSize + 28, kAssetThumbSize + 32));
  m_listView->setSpacing(6);
  m_listView->setUniformItemSizes(true);
  m_listView->setWordWrap(true);
  m_listView->setResizeMode(QListView::Adjust);
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  m_listView->setDragEnabled(true);
  m_listView->setDragDropMode(QAbstractItemView::DragOnly);
  m_listView->setAcceptDrops(true);
  m_listView->viewport()->setAcceptDrops(true);
  m_listView->installEventFilter(this);

  connect(m_listView, &QListView::doubleClicked, this,
          &NMAssetBrowserPanel::onListDoubleClicked);
  connect(m_listView, &QListView::customContextMenuRequested, this,
          &NMAssetBrowserPanel::onListContextMenu);
  if (auto *selectionModel = m_listView->selectionModel()) {
    connect(selectionModel, &QItemSelectionModel::currentChanged, this,
            &NMAssetBrowserPanel::onListSelectionChanged);
  }

  m_listPane = new QWidget(m_splitter);
  auto *listLayout = new QVBoxLayout(m_listPane);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(4);
  listLayout->addWidget(m_listView, 1);

  m_previewFrame = new QFrame(m_listPane);
  m_previewFrame->setObjectName("AssetPreviewFrame");
  m_previewFrame->setFrameShape(QFrame::StyledPanel);
  auto *previewLayout = new QVBoxLayout(m_previewFrame);
  previewLayout->setContentsMargins(6, 6, 6, 6);
  previewLayout->setSpacing(3);

  m_previewImage = new QLabel(m_previewFrame);
  m_previewImage->setAlignment(Qt::AlignCenter);
  m_previewImage->setMinimumHeight(kPreviewHeight);
  m_previewImage->setText(tr("No preview"));

  m_previewName = new QLabel(m_previewFrame);
  m_previewName->setWordWrap(true);
  m_previewMeta = new QLabel(m_previewFrame);
  m_previewMeta->setWordWrap(true);

  previewLayout->addWidget(m_previewImage);
  previewLayout->addWidget(m_previewName);
  previewLayout->addWidget(m_previewMeta);

  listLayout->addWidget(m_previewFrame);

  m_splitter->addWidget(m_listPane);

  // Set splitter sizes (30% tree, 70% list)
  m_splitter->setSizes({180, 520});

  layout->addWidget(m_splitter);

  setContentWidget(m_contentWidget);
  applyFilters();

  m_iconProvider = new NMAssetIconProvider(this, m_listView->iconSize());
  m_listModel->setIconProvider(m_iconProvider);
  m_listModel->setObjectName("AssetBrowserListModel");
  m_treeModel->setObjectName("AssetBrowserTreeModel");

  m_treeView->setAcceptDrops(true);
  m_treeView->viewport()->setAcceptDrops(true);
  m_treeView->installEventFilter(this);

  updateThumbnailSize(kAssetThumbSize);
  setPreviewVisible(m_previewVisible);
}

void NMAssetBrowserPanel::onTreeClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;

  QString path = m_treeModel->filePath(index);
  m_currentPath = path;

  if (m_listModel) {
    if (m_filterProxy) {
      if (auto *proxy = static_cast<NMAssetFilterProxy *>(m_filterProxy)) {
        proxy->setPinnedDirectory(path);
      }
    }
    m_listModel->setRootPath(path);
    QModelIndex idx = m_listModel->index(path);
    m_listView->setRootIndex(m_filterProxy ? m_filterProxy->mapFromSource(idx)
                                           : idx);
  }
}

void NMAssetBrowserPanel::onListDoubleClicked(const QModelIndex &index) {
  if (!index.isValid())
    return;

  QModelIndex source =
      m_filterProxy ? m_filterProxy->mapToSource(index) : index;
  QString path = m_listModel->filePath(source);
  emit assetDoubleClicked(path);
}

void NMAssetBrowserPanel::onListContextMenu(const QPoint &pos) {
  QModelIndex index = m_listView->indexAt(pos);
  if (!index.isValid())
    return;

  QModelIndex source =
      m_filterProxy ? m_filterProxy->mapToSource(index) : index;
  QString path = m_listModel->filePath(source);

  // Emit signal for external handlers
  emit assetContextMenu(path, m_listView->mapToGlobal(pos));

  // Create and show context menu with actions
  QMenu menu(this);

  QAction *renameAction = menu.addAction(tr("Rename"));
  connect(renameAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onRenameAction);

  QAction *duplicateAction = menu.addAction(tr("Duplicate"));
  connect(duplicateAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onDuplicateAction);

  menu.addSeparator();

  QAction *reimportAction = menu.addAction(tr("Reimport"));
  connect(reimportAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onReimportAction);

  menu.addSeparator();

  QAction *showInExplorerAction = menu.addAction(tr("Show in Explorer"));
  connect(showInExplorerAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onShowInExplorerAction);

  QAction *copyPathAction = menu.addAction(tr("Copy Path"));
  connect(copyPathAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onCopyPathAction);

  QAction *copyIdAction = menu.addAction(tr("Copy ID"));
  connect(copyIdAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onCopyIdAction);

  menu.addSeparator();

  QAction *deleteAction = menu.addAction(tr("Delete"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this,
          &NMAssetBrowserPanel::onDeleteAction);

  menu.exec(m_listView->mapToGlobal(pos));
}

void NMAssetBrowserPanel::onListSelectionChanged(
    const QModelIndex &current, const QModelIndex & /*previous*/) {
  QString path;
  if (current.isValid()) {
    QModelIndex source =
        m_filterProxy ? m_filterProxy->mapToSource(current) : current;
    path = m_listModel->filePath(source);
  }
  updatePreview(path);
  if (!path.isEmpty()) {
    emit assetSelected(path);
  }
}

void NMAssetBrowserPanel::onImportAssets() {
  auto &projectManager = ProjectManager::instance();
  if (!projectManager.hasOpenProject()) {
    NMMessageDialog::showInfo(this, tr("Import Assets"),
                              tr("Open a project before importing assets."));
    return;
  }

  const QString filter =
      tr("Assets (*.png *.jpg *.jpeg *.bmp *.gif *.wav *.mp3 *.ogg *.flac "
         "*.ttf *.otf *.nms *.nmscene *.json *.xml *.yaml *.yml)");
  QStringList files = NMFileDialog::getOpenFileNames(this, tr("Import Assets"),
                                                     QDir::homePath(), filter);
  if (files.isEmpty()) {
    return;
  }

  importFiles(files, true);
}

void NMAssetBrowserPanel::importFiles(const QStringList &files,
                                      bool interactive) {
  auto &projectManager = ProjectManager::instance();
  if (!projectManager.hasOpenProject() || files.isEmpty()) {
    return;
  }

  enum class ImportTargetMode { AutoByType, CurrentFolder, CustomFolder };

  const QString assetsRoot = QString::fromStdString(
      projectManager.getFolderPath(ProjectFolder::Assets));
  QString currentTarget;
  if (!m_currentPath.isEmpty()) {
    QDir currentDir(m_currentPath);
    if (currentDir.exists() && m_currentPath.startsWith(assetsRoot) &&
        projectManager.isPathInProject(m_currentPath.toStdString())) {
      currentTarget = m_currentPath;
    }
  }

  ImportTargetMode targetMode = ImportTargetMode::AutoByType;
  QString customTarget;
  auto chooseImportOptions = [&](ImportTargetMode &mode,
                                 QString &customDir) -> bool {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Import Assets"));
    dialog.setModal(true);
    dialog.setMinimumSize(600, 380);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *header = new QLabel(
        tr("Selected %1 file(s) for import").arg(files.size()), &dialog);
    layout->addWidget(header);

    auto *list = new QListWidget(&dialog);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(48, 48));
    list->setResizeMode(QListWidget::Adjust);
    list->setSpacing(6);
    list->setMinimumHeight(150);

    QFileIconProvider iconProvider;
    for (const QString &path : files) {
      QFileInfo info(path);
      QListWidgetItem *item = new QListWidgetItem(info.fileName(), list);
      item->setToolTip(info.absoluteFilePath());
      if (info.suffix().compare("png", Qt::CaseInsensitive) == 0 ||
          info.suffix().compare("jpg", Qt::CaseInsensitive) == 0 ||
          info.suffix().compare("jpeg", Qt::CaseInsensitive) == 0 ||
          info.suffix().compare("bmp", Qt::CaseInsensitive) == 0 ||
          info.suffix().compare("gif", Qt::CaseInsensitive) == 0) {
        QPixmap pix(path);
        if (!pix.isNull()) {
          item->setIcon(QIcon(pix.scaled(56, 56, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation)));
          continue;
        }
      }
      item->setIcon(iconProvider.icon(info));
    }
    layout->addWidget(list, 1);

    auto *destLabel = new QLabel(tr("Import destination"), &dialog);
    layout->addWidget(destLabel);

    auto *destRow = new QHBoxLayout();
    auto *destCombo = new QComboBox(&dialog);
    destCombo->addItem(tr("Auto by type (Images/Audio/Fonts/etc)"),
                       static_cast<int>(ImportTargetMode::AutoByType));
    const QString currentLabel =
        currentTarget.isEmpty()
            ? tr("Current folder")
            : tr("Current folder: %1")
                  .arg(QDir(assetsRoot).relativeFilePath(currentTarget));
    destCombo->addItem(currentLabel,
                       static_cast<int>(ImportTargetMode::CurrentFolder));
    destCombo->addItem(tr("Choose folder..."),
                       static_cast<int>(ImportTargetMode::CustomFolder));

    auto *customEdit = new QLineEdit(&dialog);
    customEdit->setPlaceholderText(tr("Select a folder inside Assets"));
    customEdit->setEnabled(false);
    auto *browseButton = new QPushButton(tr("Browse"), &dialog);
    browseButton->setEnabled(false);

    destRow->addWidget(destCombo, 1);
    destRow->addWidget(customEdit, 2);
    destRow->addWidget(browseButton);
    layout->addLayout(destRow);

    auto updateCustomState = [&]() {
      const auto modeValue =
          static_cast<ImportTargetMode>(destCombo->currentData().toInt());
      const bool custom = (modeValue == ImportTargetMode::CustomFolder);
      customEdit->setEnabled(custom);
      browseButton->setEnabled(custom);
    };
    updateCustomState();

    connect(destCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dialog, [&](int) { updateCustomState(); });
    connect(browseButton, &QPushButton::clicked, &dialog, [&]() {
      const QString dir = NMFileDialog::getExistingDirectory(
          &dialog, tr("Select Import Folder"), assetsRoot);
      if (!dir.isEmpty()) {
        customEdit->setText(dir);
      }
    });

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
      return false;
    }

    mode = static_cast<ImportTargetMode>(destCombo->currentData().toInt());
    if (mode == ImportTargetMode::CustomFolder) {
      customDir = customEdit->text().trimmed();
      if (customDir.isEmpty()) {
        NMMessageDialog::showWarning(
            this, tr("Import Assets"),
            tr("Please choose a destination folder inside Assets."));
        return false;
      }
      if (!customDir.startsWith(assetsRoot)) {
        NMMessageDialog::showWarning(
            this, tr("Import Assets"),
            tr("Destination must be inside the Assets folder."));
        return false;
      }
    }

    return true;
  };

  if (interactive) {
    if (!chooseImportOptions(targetMode, customTarget)) {
      return;
    }
  } else {
    targetMode = currentTarget.isEmpty() ? ImportTargetMode::AutoByType
                                         : ImportTargetMode::CurrentFolder;
  }

  int importedCount = 0;
  QSet<QString> targetDirs;
  QStringList failedFiles;
  for (const QString &filePath : files) {
    QFileInfo info(filePath);
    if (!info.exists()) {
      failedFiles.append(info.fileName());
      QtEventBus::instance().publish(
          {QtEditorEventType::ErrorOccurred,
           "AssetImport",
           {{"message", tr("Missing source file: %1").arg(info.fileName())}}});
      continue;
    }

    const QString extension = info.suffix().toLower();
    QString targetDir;
    switch (targetMode) {
    case ImportTargetMode::AutoByType:
      targetDir = currentTarget.isEmpty()
                      ? importDestinationForExtension(extension)
                      : currentTarget;
      break;
    case ImportTargetMode::CurrentFolder:
      targetDir = currentTarget.isEmpty() ? assetsRoot : currentTarget;
      break;
    case ImportTargetMode::CustomFolder:
      targetDir = customTarget;
      break;
    }

    if (targetDir.isEmpty()) {
      targetDir = assetsRoot;
    }

    QDir target(targetDir);
    if (!target.exists()) {
      target.mkpath(".");
    }

    const QString destPath =
        generateUniquePath(target.absolutePath(), info.fileName());

    if (auto *database = projectManager.assetDatabase()) {
      auto result = database->importAssetToPath(filePath.toStdString(),
                                                destPath.toStdString());
      if (result.isError()) {
        failedFiles.append(info.fileName());
        QtEventBus::instance().publish(
            {QtEditorEventType::ErrorOccurred,
             "AssetImport",
             {{"message", tr("Import failed: %1").arg(info.fileName())},
              {"details", QString::fromStdString(result.error())}}});
        continue;
      }
    } else {
      if (!QFile::copy(filePath, destPath)) {
        failedFiles.append(info.fileName());
        QtEventBus::instance().publish(
            {QtEditorEventType::ErrorOccurred,
             "AssetImport",
             {{"message", tr("Import failed: %1").arg(info.fileName())},
              {"details", tr("File copy failed")}}});
        continue;
      }
    }

    importedCount++;
    targetDirs.insert(target.absolutePath());

    // Publish asset imported event so other Asset Browser panels can refresh
    QtEventBus::instance().publishAssetImported(destPath, targetDir);
  }

  if (importedCount > 0) {
    if (targetDirs.size() == 1) {
      const QString targetDir = *targetDirs.begin();
      m_currentPath = targetDir;
      if (m_listModel && m_listView) {
        m_listModel->setRootPath(targetDir);
        const QModelIndex idx = m_listModel->index(targetDir);
        m_listView->setRootIndex(
            m_filterProxy ? m_filterProxy->mapFromSource(idx) : idx);
      }
      if (m_treeModel && m_treeView) {
        const QModelIndex index = m_treeModel->index(targetDir);
        if (index.isValid()) {
          m_treeView->setCurrentIndex(index);
          m_treeView->scrollTo(index);
        }
      }
    }
  }
  if (importedCount == 0) {
    NMMessageDialog::showWarning(
        this, tr("Import Assets"),
        tr("No assets were imported. Check file permissions or formats."));
  } else if (!failedFiles.isEmpty()) {
    NMMessageDialog::showWarning(
        this, tr("Import Assets"),
        tr("Imported %1 asset(s). Failed to import:\n%2")
            .arg(importedCount)
            .arg(failedFiles.join("\n")));
  }
  refresh();
}

bool NMAssetBrowserPanel::eventFilter(QObject *watched, QEvent *event) {
  if ((watched == m_listView || watched == m_treeView ||
       watched == m_listView->viewport() ||
       watched == m_treeView->viewport()) &&
      event) {
    if (event->type() == QEvent::DragEnter) {
      auto *dragEvent = static_cast<QDragEnterEvent *>(event);
      if (dragEvent->mimeData() && dragEvent->mimeData()->hasUrls()) {
        dragEvent->acceptProposedAction();
        return true;
      }
    } else if (event->type() == QEvent::DragMove) {
      auto *dragEvent = static_cast<QDragMoveEvent *>(event);
      if (dragEvent->mimeData() && dragEvent->mimeData()->hasUrls()) {
        dragEvent->acceptProposedAction();
        return true;
      }
    } else if (event->type() == QEvent::Drop) {
      auto *dropEvent = static_cast<QDropEvent *>(event);
      if (dropEvent->mimeData() && dropEvent->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : dropEvent->mimeData()->urls()) {
          if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
          }
        }
        importFiles(paths, false);
        dropEvent->acceptProposedAction();
        return true;
      }
    }
  }
  return NMDockPanel::eventFilter(watched, event);
}

void NMAssetBrowserPanel::onFilterTextChanged(const QString &text) {
  if (m_filterProxy) {
    auto *proxy = static_cast<NMAssetFilterProxy *>(m_filterProxy);
    proxy->setNameFilterText(text);
  }
}

void NMAssetBrowserPanel::onTypeFilterChanged(int index) {
  Q_UNUSED(index);
  applyFilters();
}

void NMAssetBrowserPanel::applyFilters() {
  if (!m_listModel || !m_filterProxy) {
    return;
  }

  auto *proxy = static_cast<NMAssetFilterProxy *>(m_filterProxy);
  proxy->setTypeFilterIndex(m_typeFilter ? m_typeFilter->currentIndex() : 0);
  if (m_filterEdit) {
    proxy->setNameFilterText(m_filterEdit->text());
  }
  updatePreview(selectedAssetPath());
}

void NMAssetBrowserPanel::setPreviewVisible(bool visible) {
  m_previewVisible = visible;
  if (m_previewFrame) {
    m_previewFrame->setVisible(visible);
  }
  if (visible) {
    updatePreview(selectedAssetPath());
  }
}

void NMAssetBrowserPanel::updateThumbnailSize(int size) {
  if (size < 48) {
    size = 48;
  }
  m_thumbSize = size;
  if (m_listView) {
    m_listView->setIconSize(QSize(size, size));
    m_listView->setGridSize(QSize(size + 28, size + 32));
    m_listView->update();
  }
  if (m_iconProvider) {
    auto *provider = static_cast<NMAssetIconProvider *>(m_iconProvider);
    provider->setIconSize(QSize(size, size));
  }
}

void NMAssetBrowserPanel::updatePreview(const QString &path) {
  if (!m_previewImage || !m_previewName || !m_previewMeta) {
    return;
  }
  if (!m_previewVisible) {
    return;
  }

  m_previewPath = path;
  if (path.isEmpty()) {
    clearPreview();
    return;
  }

  QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    clearPreview();
    return;
  }

  m_previewName->setText(info.fileName());
  const QString sizeText =
      tr("%1 KB").arg(QString::number((info.size() + 1023) / 1024));

  if (isImageExtension(info.suffix())) {
    QImageReader reader(info.absoluteFilePath());
    reader.setAutoTransform(true);
    QSize imgSize = reader.size();
    QImage image = reader.read();
    if (!image.isNull()) {
      QPixmap pix = QPixmap::fromImage(image);
      QSize target = m_previewImage->size();
      QPixmap scaled =
          pix.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      m_previewImage->setPixmap(scaled);
      m_previewImage->setText(QString());
      m_previewMeta->setText(tr("%1 x %2 | %3")
                                 .arg(imgSize.width())
                                 .arg(imgSize.height())
                                 .arg(sizeText));
      return;
    }
  }

  m_previewImage->setPixmap(QPixmap());
  m_previewImage->setText(tr("No preview"));
  m_previewMeta->setText(sizeText);
}

void NMAssetBrowserPanel::clearPreview() {
  m_previewPath.clear();
  if (m_previewImage) {
    m_previewImage->setPixmap(QPixmap());
    m_previewImage->setText(tr("No preview"));
  }
  if (m_previewName) {
    m_previewName->setText(QString());
  }
  if (m_previewMeta) {
    m_previewMeta->setText(QString());
  }
}

QString NMAssetBrowserPanel::importDestinationForExtension(
    const QString &extension) const {
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

QString NMAssetBrowserPanel::generateUniquePath(const QString &directory,
                                                const QString &fileName) const {
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

QPixmap NMAssetBrowserPanel::generateAudioWaveform(const QString &path,
                                                   const QSize &size) const {
  Q_UNUSED(path);

  // Create a deterministic placeholder waveform
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Background
  QColor bgColor(60, 60, 80);
  painter.fillRect(pixmap.rect(), bgColor);

  // Border
  painter.setPen(QPen(QColor(100, 100, 120), 1));
  painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));

  // Draw "AUDIO" text
  painter.setPen(QColor(200, 200, 220));
  QFont font = painter.font();
  font.setPointSize(size.height() / 6);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(pixmap.rect(), Qt::AlignCenter, "AUDIO");

  // Draw deterministic waveform bars
  const int barCount = 16;
  const int barWidth = (size.width() - 20) / barCount;
  const int maxBarHeight = size.height() - 30;

  // Use file path hash for deterministic heights
  QByteArray pathData = path.toUtf8();
  uint seed = static_cast<uint>(qHash(pathData));

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(120, 200, 255, 180));

  for (int i = 0; i < barCount; ++i) {
    // Generate deterministic height based on position and seed
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    const double normalized = static_cast<double>(seed % 1000) / 1000.0;
    const int barHeight =
        static_cast<int>(maxBarHeight * normalized * 0.8 + maxBarHeight * 0.2);

    const int x = 10 + i * barWidth;
    const int y = (size.height() - barHeight) / 2;

    painter.drawRect(x, y, barWidth - 2, barHeight);
  }

  return pixmap;
}

bool NMAssetBrowserPanel::isThumbnailValid(
    const QString &path, const ThumbnailCacheEntry &entry) const {
  QFileInfo info(path);
  if (!info.exists()) {
    return false;
  }

  // Check if file has been modified
  if (info.lastModified() != entry.lastModified) {
    return false;
  }

  // Check if file size has changed
  if (info.size() != entry.fileSize) {
    return false;
  }

  return true;
}

void NMAssetBrowserPanel::scheduleVisibleThumbnails() {
  // Trigger visibility update with debouncing to avoid excessive calls
  if (m_visibilityUpdateTimer && !m_visibilityUpdateTimer->isActive()) {
    m_visibilityUpdateTimer->start();
  }
}

void NMAssetBrowserPanel::cancelPendingThumbnails() {
  m_pendingThumbnails.clear();

  // Cancel via the lazy loader (supports proper cancellation)
  if (m_lazyLoader) {
    m_lazyLoader->cancelPending();
  }

  // Note: Legacy QThreadPool doesn't support cancellation of running tasks
  // We rely on QPointer checks in the runnable to abort gracefully
}

void NMAssetBrowserPanel::onViewModeChanged(int index) {
  if (!m_viewModeCombo || !m_listView) {
    return;
  }

  const auto mode =
      static_cast<AssetViewMode>(m_viewModeCombo->itemData(index).toInt());
  setViewMode(mode);
}

void NMAssetBrowserPanel::setViewMode(AssetViewMode mode) {
  m_viewMode = mode;

  if (!m_listView) {
    return;
  }

  switch (mode) {
  case AssetViewMode::Grid:
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setIconSize(QSize(m_thumbSize, m_thumbSize));
    m_listView->setGridSize(QSize(m_thumbSize + 28, m_thumbSize + 32));
    m_listView->setSpacing(6);
    break;

  case AssetViewMode::List:
    m_listView->setViewMode(QListView::ListMode);
    m_listView->setIconSize(QSize(24, 24));
    m_listView->setGridSize(QSize());
    m_listView->setSpacing(2);
    break;
  }

  m_listView->update();
}

void NMAssetBrowserPanel::onThumbnailReady(const QString &path,
                                           const QPixmap &pixmap) {
  m_pendingThumbnails.remove(path);

  if (!pixmap.isNull() && m_listView) {
    // Force model to refresh the icon for this item
    m_listView->update();
  }
}

void NMAssetBrowserPanel::updateVisibleItems() {
  if (!m_listView || !m_listModel || !m_lazyLoader) {
    return;
  }

  // Get visible rect
  QRect visibleRect = m_listView->viewport()->rect();

  // Clear previous visible paths
  m_visiblePaths.clear();

  // Iterate through visible items
  QModelIndex rootIndex = m_listView->rootIndex();
  for (int row = 0;
       row < m_listModel->rowCount(m_filterProxy
                                       ? m_filterProxy->mapToSource(rootIndex)
                                       : rootIndex);
       ++row) {
    QModelIndex proxyIndex =
        m_filterProxy
            ? m_filterProxy->index(row, 0, rootIndex)
            : m_listModel->index(row, 0,
                                 m_filterProxy
                                     ? m_filterProxy->mapToSource(rootIndex)
                                     : rootIndex);

    QRect itemRect = m_listView->visualRect(proxyIndex);
    if (visibleRect.intersects(itemRect)) {
      QModelIndex sourceIndex =
          m_filterProxy ? m_filterProxy->mapToSource(proxyIndex) : proxyIndex;
      QString path = m_listModel->filePath(sourceIndex);
      QFileInfo info(path);

      if (info.isFile() && isImageExtension(info.suffix())) {
        m_visiblePaths.insert(path);

        // Request thumbnail with high priority for visible items
        m_lazyLoader->requestThumbnail(path, m_listView->iconSize(), 10);
      }
    }
  }

  // Record metrics
  PerformanceMetrics::instance().recordCount(
      "AssetBrowser.visibleItems", static_cast<int>(m_visiblePaths.size()));
  auto stats = m_lazyLoader->getStats();
  PerformanceMetrics::instance().recordCount(
      PerformanceMetrics::METRIC_CACHE_SIZE_KB,
      static_cast<int>(stats.cacheSizeKB));
}

void NMAssetBrowserPanel::onLazyThumbnailReady(const QString &path,
                                               const QPixmap &pixmap) {
  // Update the legacy cache for icon provider compatibility
  if (!pixmap.isNull()) {
    QFileInfo info(path);
    auto *entry =
        new ThumbnailCacheEntry{pixmap, info.lastModified(), info.size()};
    m_thumbnailCache.insert(
        path, entry,
        static_cast<int>(pixmap.width() * pixmap.height() * 4 / 1024));
  }

  // Record successful load
  PerformanceMetrics::instance().recordCount(
      PerformanceMetrics::METRIC_THUMBNAIL_CACHE_HIT, 1);

  // Update the list view to show the new thumbnail
  if (m_listView) {
    m_listView->update();
  }
}

void NMAssetBrowserPanel::onRenameAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  QFileInfo info(path);
  const QString oldName = info.fileName();

  bool ok = false;
  QString newName =
      NMInputDialog::getText(this, tr("Rename Asset"), tr("Enter new name:"),
                             QLineEdit::Normal, oldName, &ok);

  if (!ok || newName.isEmpty() || newName == oldName) {
    return;
  }

  if (renameAsset(path, newName)) {
    refresh();
  }
}

void NMAssetBrowserPanel::onDeleteAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  QFileInfo info(path);
  const QString fileName = info.fileName();

  NMDialogButton result = NMMessageDialog::showQuestion(
      this, tr("Delete Asset"),
      tr("Are you sure you want to delete '%1'?\n\nThis action cannot be "
         "undone.")
          .arg(fileName),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result == NMDialogButton::Yes) {
    if (deleteAsset(path)) {
      refresh();
    }
  }
}

void NMAssetBrowserPanel::onDuplicateAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  const QString newPath = duplicateAsset(path);
  if (!newPath.isEmpty()) {
    refresh();
    // Select the new file
    if (m_listModel && m_listView) {
      QModelIndex idx = m_listModel->index(newPath);
      if (m_filterProxy) {
        idx = m_filterProxy->mapFromSource(idx);
      }
      m_listView->setCurrentIndex(idx);
    }
  }
}

void NMAssetBrowserPanel::onReimportAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  if (reimportAsset(path)) {
    // Clear cached metadata and thumbnail for the reimported asset
    m_metadataCache.remove(path);
    m_thumbnailCache.remove(path);
    updatePreview(path);
    refresh();
  }
}

void NMAssetBrowserPanel::onShowInExplorerAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  showInExplorer(path);
}

void NMAssetBrowserPanel::onCopyPathAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  QApplication::clipboard()->setText(path);
}

void NMAssetBrowserPanel::onCopyIdAction() {
  const QString path = selectedAssetPath();
  if (path.isEmpty()) {
    return;
  }

  AssetMetadata meta = getAssetMetadata(path);
  if (!meta.id.isEmpty()) {
    QApplication::clipboard()->setText(meta.id);
  } else {
    // Fall back to copying the relative path as ID
    QFileInfo info(path);
    QString relativePath = info.fileName();
    if (!m_rootPath.isEmpty()) {
      QDir root(m_rootPath);
      relativePath = root.relativeFilePath(path);
    }
    QApplication::clipboard()->setText(relativePath);
  }
}

AssetMetadata NMAssetBrowserPanel::getAssetMetadata(const QString &path) const {
  // Check cache first
  auto it = m_metadataCache.find(path);
  if (it != m_metadataCache.end()) {
    return it.value();
  }

  // Build metadata from file info
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

  // Cache the result
  m_metadataCache[path] = meta;
  return meta;
}

bool NMAssetBrowserPanel::renameAsset(const QString &oldPath,
                                      const QString &newName) {
  QFileInfo info(oldPath);
  if (!info.exists()) {
    return false;
  }

  QString newPath = info.absolutePath() + "/" + newName;
  QFile file(oldPath);

  if (file.rename(newPath)) {
    // Update cache
    if (m_metadataCache.contains(oldPath)) {
      AssetMetadata meta = m_metadataCache.take(oldPath);
      meta.path = newPath;
      m_metadataCache[newPath] = meta;
    }
    m_thumbnailCache.remove(oldPath);
    emit assetRenamed(oldPath, newPath);
    return true;
  }
  return false;
}

bool NMAssetBrowserPanel::deleteAsset(const QString &path) {
  QFileInfo info(path);
  if (!info.exists()) {
    return false;
  }

  QFile file(path);
  if (file.remove()) {
    m_metadataCache.remove(path);
    m_thumbnailCache.remove(path);
    emit assetDeleted(path);
    return true;
  }
  return false;
}

QString NMAssetBrowserPanel::duplicateAsset(const QString &path) {
  QFileInfo info(path);
  if (!info.exists()) {
    return QString();
  }

  // Generate unique name
  QString baseName = info.completeBaseName();
  QString extension = info.suffix();
  QString directory = info.absolutePath();
  QString newPath =
      generateUniquePath(directory, baseName + "_copy." + extension);

  QFile file(path);
  if (file.copy(newPath)) {
    emit assetDuplicated(path, newPath);
    return newPath;
  }
  return QString();
}

bool NMAssetBrowserPanel::reimportAsset(const QString &path) {
  // For now, reimporting just invalidates the cache
  // A full implementation would re-process the asset through the pipeline
  m_metadataCache.remove(path);
  m_thumbnailCache.remove(path);

  // Force refresh of the asset in views
  if (m_listView) {
    m_listView->update();
  }

  return true;
}

void NMAssetBrowserPanel::showInExplorer(const QString &path) {
  QFileInfo info(path);
  QString folder = info.isDir() ? path : info.absolutePath();

#if defined(Q_OS_WIN)
  QString param = QString("/select,\"%1\"").arg(QDir::toNativeSeparators(path));
  QProcess::startDetached("explorer.exe", {param});
#elif defined(Q_OS_MACOS)
  QProcess::startDetached("open", {"-R", path});
#else
  // Linux - open the containing folder
  QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

} // namespace NovelMind::editor::qt

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
