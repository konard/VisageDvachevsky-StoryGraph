#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "nm_dialogs_detail.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVector>

namespace NovelMind::editor::qt {

namespace {

struct FilterEntry {
  QString label;
  QStringList patterns;
};

QVector<FilterEntry> parseFilterEntries(const QString &filter) {
  QVector<FilterEntry> entries;
  const QString trimmed = filter.trimmed();
  if (trimmed.isEmpty()) {
    return entries;
  }

  const QStringList parts = trimmed.split(";;", Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    FilterEntry entry;
    entry.label = part.trimmed();
    const qsizetype start = entry.label.indexOf('(');
    const qsizetype end = entry.label.lastIndexOf(')');
    if (start >= 0 && end > start) {
      const QString inside = entry.label.mid(start + 1, end - start - 1);
      entry.patterns = inside.split(' ', Qt::SkipEmptyParts);
      entry.label = entry.label.left(start).trimmed();
    }
    if (entry.label.isEmpty()) {
      entry.label = QObject::tr("Files");
    }
    if (entry.patterns.isEmpty()) {
      entry.patterns << "*";
    }
    entries.push_back(entry);
  }

  return entries;
}

class NMFileFilterProxy final : public QSortFilterProxyModel {
public:
  explicit NMFileFilterProxy(QObject *parent = nullptr)
      : QSortFilterProxyModel(parent) {}

  void setNameFilters(const QStringList &filters) {
    m_filters = filters;
    invalidateFilter();
  }

  void setShowHidden(bool showHidden) {
    m_showHidden = showHidden;
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override {
    auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel) {
      return true;
    }

    const QModelIndex index = fsModel->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
      return false;
    }

    const QFileInfo info = fsModel->fileInfo(index);
    if (!m_showHidden && info.isHidden()) {
      return false;
    }

    if (fsModel->isDir(index)) {
      return true;
    }

    if (m_filters.isEmpty()) {
      return true;
    }

    const QString name = fsModel->fileName(index);
    return QDir::match(m_filters, name);
  }

private:
  QStringList m_filters;
  bool m_showHidden = false;
};

} // namespace

NMFileDialog::NMFileDialog(QWidget *parent, const QString &title, Mode mode,
                           const QString &dir, const QString &filter)
    : QDialog(parent), m_mode(mode) {
  setWindowTitle(title);
  setModal(true);
  setObjectName("NMFileDialog");
  setMinimumSize(760, 480);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  m_dirModel = new QFileSystemModel(this);
  m_dirModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
  m_dirModel->setRootPath(QString());

  m_fileModel = new QFileSystemModel(this);
  if (m_mode == Mode::SelectDirectory) {
    m_fileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
  } else if (m_mode == Mode::SaveFile) {
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot |
                           QDir::Drives);
  } else {
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot |
                           QDir::Drives);
  }
  m_fileModel->setRootPath(QString());

  m_filterProxy = new NMFileFilterProxy(this);
  m_filterProxy->setSourceModel(m_fileModel);

  buildUi();
  applyFilter(filter);

  const QString initialDir = dir.trimmed().isEmpty() ? QDir::homePath() : dir;
  setDirectory(initialDir);
  detail::applyDialogFrameStyle(this);
}

void NMFileDialog::buildUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(10);

  auto *pathLayout = new QHBoxLayout();
  pathLayout->setSpacing(6);

  auto& iconMgr = NMIconManager::instance();

  m_upButton = new QPushButton(tr("Up"), this);
  m_upButton->setIcon(iconMgr.getIcon("arrow-up", 16));
  m_upButton->setObjectName("NMSecondaryButton");
  connect(m_upButton, &QPushButton::clicked, this, [this]() {
    QDir dir(m_currentDir);
    if (dir.cdUp()) {
      setDirectory(dir.absolutePath());
    }
  });

  m_pathEdit = new QLineEdit(this);
  m_pathEdit->setPlaceholderText(tr("Path"));
  connect(m_pathEdit, &QLineEdit::editingFinished, this, [this]() {
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
      return;
    }
    QFileInfo info(path);
    if (info.exists() && info.isDir()) {
      setDirectory(info.absoluteFilePath());
      return;
    }
    if (info.exists() && info.isFile() && m_mode != Mode::SelectDirectory) {
      m_selectedPaths = {info.absoluteFilePath()};
      accept();
    }
  });

  pathLayout->addWidget(m_upButton);
  pathLayout->addWidget(m_pathEdit, 1);
  mainLayout->addLayout(pathLayout);

  auto *splitter = new QSplitter(Qt::Horizontal, this);

  m_treeView = new QTreeView(splitter);
  m_treeView->setModel(m_dirModel);
  m_treeView->setHeaderHidden(true);
  for (int i = 1; i < m_dirModel->columnCount(); ++i) {
    m_treeView->hideColumn(i);
  }
  m_treeView->setAnimated(true);
  m_treeView->setIndentation(16);
  connect(m_treeView, &QTreeView::clicked, this,
          [this](const QModelIndex &idx) {
            if (!idx.isValid()) {
              return;
            }
            const QString path = m_dirModel->filePath(idx);
            if (!path.isEmpty()) {
              setDirectory(path);
            }
          });

  m_listView = new QListView(splitter);
  m_listView->setModel(m_filterProxy);
  m_listView->setSelectionMode(m_mode == Mode::OpenFiles
                                   ? QAbstractItemView::ExtendedSelection
                                   : QAbstractItemView::SingleSelection);
  m_listView->setViewMode(QListView::ListMode);
  m_listView->setUniformItemSizes(true);
  m_listView->setAlternatingRowColors(true);
  m_listView->setSpacing(2);

  connect(
      m_listView, &QListView::doubleClicked, this,
      [this](const QModelIndex &index) {
        if (!index.isValid()) {
          return;
        }
        const QModelIndex source =
            static_cast<NMFileFilterProxy *>(m_filterProxy)->mapToSource(index);
        const QString path = m_fileModel->filePath(source);
        if (path.isEmpty()) {
          return;
        }
        if (m_fileModel->isDir(source)) {
          setDirectory(path);
          return;
        }
        if (m_mode != Mode::SelectDirectory) {
          m_selectedPaths = {path};
          accept();
        }
      });

  auto *previewWidget = new QFrame(splitter);
  previewWidget->setFrameShape(QFrame::StyledPanel);
  previewWidget->setMinimumWidth(200);
  auto *previewLayout = new QVBoxLayout(previewWidget);
  previewLayout->setContentsMargins(10, 10, 10, 10);
  previewLayout->setSpacing(6);

  m_previewImage = new QLabel(previewWidget);
  m_previewImage->setFixedSize(160, 160);
  m_previewImage->setAlignment(Qt::AlignCenter);
  m_previewImage->setStyleSheet(
      "QLabel { background: rgba(20, 20, 24, 180); border: 1px solid #2f2f37; "
      "border-radius: 6px; }");

  m_previewName = new QLabel(previewWidget);
  m_previewName->setWordWrap(true);
  QFont nameFont = m_previewName->font();
  nameFont.setBold(true);
  m_previewName->setFont(nameFont);

  m_previewMeta = new QLabel(previewWidget);
  m_previewMeta->setWordWrap(true);
  m_previewMeta->setStyleSheet("QLabel { color: #a8a8b0; }");

  previewLayout->addWidget(m_previewImage, 0, Qt::AlignCenter);
  previewLayout->addWidget(m_previewName);
  previewLayout->addWidget(m_previewMeta);
  previewLayout->addStretch();

  splitter->addWidget(m_treeView);
  splitter->addWidget(m_listView);
  splitter->addWidget(previewWidget);
  splitter->setSizes({200, 480, 220});

  mainLayout->addWidget(splitter, 1);

  m_selectionLabel = new QLabel(this);
  m_selectionLabel->setText(tr("No selection"));
  mainLayout->addWidget(m_selectionLabel);

  // Add filename edit for save mode
  if (m_mode == Mode::SaveFile) {
    auto *filenameLayout = new QHBoxLayout();
    filenameLayout->setSpacing(6);
    auto *filenameLabel = new QLabel(tr("File name:"), this);
    m_filenameEdit = new QLineEdit(this);
    m_filenameEdit->setPlaceholderText(tr("Enter file name"));
    connect(m_filenameEdit, &QLineEdit::textChanged, this,
            &NMFileDialog::updateAcceptState);
    filenameLayout->addWidget(filenameLabel);
    filenameLayout->addWidget(m_filenameEdit, 1);
    mainLayout->addLayout(filenameLayout);
  }

  auto *footerLayout = new QHBoxLayout();
  footerLayout->setSpacing(6);

  m_filterCombo = new QComboBox(this);
  m_filterCombo->setMinimumWidth(220);
  if (m_mode == Mode::SelectDirectory) {
    m_filterCombo->setVisible(false);
  }
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { applyFilter(m_filterCombo->currentText()); });

  footerLayout->addWidget(m_filterCombo, 0, Qt::AlignLeft);
  footerLayout->addStretch();

  QString acceptText;
  if (m_mode == Mode::SelectDirectory) {
    acceptText = tr("Select");
  } else if (m_mode == Mode::SaveFile) {
    acceptText = tr("Save");
  } else {
    acceptText = tr("Open");
  }
  m_acceptButton = new QPushButton(acceptText, this);
  m_acceptButton->setIcon(NMIconManager::instance().getIcon("status-success", 16));
  m_acceptButton->setObjectName("NMPrimaryButton");
  connect(m_acceptButton, &QPushButton::clicked, this,
          &NMFileDialog::acceptSelection);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_cancelButton->setIcon(NMIconManager::instance().getIcon("file-close", 16));
  m_cancelButton->setObjectName("NMSecondaryButton");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  footerLayout->addWidget(m_cancelButton);
  footerLayout->addWidget(m_acceptButton);

  mainLayout->addLayout(footerLayout);

  if (auto *selectionModel = m_listView->selectionModel()) {
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
            [this]() {
              updateAcceptState();
              updatePreview();
            });
  }
}

void NMFileDialog::applyFilter(const QString &filterText) {
  QStringList patterns;
  if (m_filterCombo && m_filterCombo->count() > 0) {
    patterns = m_filterCombo->currentData().toStringList();
  } else {
    const QVector<FilterEntry> entries = parseFilterEntries(filterText);
    if (!entries.isEmpty()) {
      patterns = entries.front().patterns;
    }
    if (m_filterCombo && m_filterCombo->count() == 0) {
      if (entries.isEmpty()) {
        m_filterCombo->addItem(tr("All Files"), QStringList{"*"});
        patterns = QStringList{"*"};
      } else {
        for (const auto &entry : entries) {
          m_filterCombo->addItem(entry.label, entry.patterns);
        }
      }
    }
  }

  if (m_filterProxy) {
    auto *proxy = static_cast<NMFileFilterProxy *>(m_filterProxy);
    if (proxy) {
      proxy->setNameFilters(patterns);
    }
  }
  updateAcceptState();
}

void NMFileDialog::setDirectory(const QString &path) {
  QFileInfo info(path);
  if (!info.exists() || !info.isDir()) {
    return;
  }

  const QString cleaned = QDir(path).absolutePath();
  m_currentDir = cleaned;
  if (m_pathEdit) {
    m_pathEdit->setText(cleaned);
  }

  if (m_treeView && m_dirModel) {
    const QModelIndex dirIndex = m_dirModel->index(cleaned);
    if (dirIndex.isValid()) {
      m_treeView->setCurrentIndex(dirIndex);
      m_treeView->scrollTo(dirIndex);
    }
  }

  if (m_listView && m_fileModel) {
    m_fileModel->setRootPath(cleaned);
    const QModelIndex rootIndex = m_fileModel->index(cleaned);
    if (rootIndex.isValid()) {
      const QModelIndex proxyIndex =
          static_cast<NMFileFilterProxy *>(m_filterProxy)
              ->mapFromSource(rootIndex);
      m_listView->setRootIndex(proxyIndex);
    }
  }

  updateAcceptState();
  updatePreview();
}

QStringList NMFileDialog::selectedFilePaths() const {
  QStringList results;
  if (!m_listView || !m_listView->selectionModel()) {
    return results;
  }

  const QModelIndexList selected = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &index : selected) {
    const QModelIndex source =
        static_cast<NMFileFilterProxy *>(m_filterProxy)->mapToSource(index);
    if (!source.isValid()) {
      continue;
    }
    if (m_fileModel->isDir(source)) {
      continue;
    }
    results << m_fileModel->filePath(source);
  }
  return results;
}

QString NMFileDialog::selectedDirectoryPath() const {
  if (!m_listView || !m_listView->selectionModel()) {
    return m_currentDir;
  }

  const QModelIndexList selected = m_listView->selectionModel()->selectedRows();
  for (const QModelIndex &index : selected) {
    const QModelIndex source =
        static_cast<NMFileFilterProxy *>(m_filterProxy)->mapToSource(index);
    if (!source.isValid()) {
      continue;
    }
    if (m_fileModel->isDir(source)) {
      return m_fileModel->filePath(source);
    }
  }
  return m_currentDir;
}

void NMFileDialog::updateAcceptState() {
  QStringList selected = selectedFilePaths();
  if (m_mode == Mode::SelectDirectory) {
    m_acceptButton->setEnabled(!m_currentDir.isEmpty());
    if (m_selectionLabel) {
      m_selectionLabel->setText(tr("Current folder: %1").arg(m_currentDir));
    }
    return;
  }

  if (m_mode == Mode::SaveFile) {
    const bool hasFilename =
        m_filenameEdit && !m_filenameEdit->text().trimmed().isEmpty();
    m_acceptButton->setEnabled(hasFilename && !m_currentDir.isEmpty());
    if (m_selectionLabel) {
      if (hasFilename) {
        m_selectionLabel->setText(
            QDir(m_currentDir).filePath(m_filenameEdit->text().trimmed()));
      } else {
        m_selectionLabel->setText(tr("Enter a file name"));
      }
    }
    return;
  }

  m_acceptButton->setEnabled(!selected.isEmpty());
  if (m_selectionLabel) {
    if (selected.isEmpty()) {
      m_selectionLabel->setText(tr("No selection"));
    } else {
      m_selectionLabel->setText(selected.join("; "));
    }
  }
}

void NMFileDialog::updatePreview() {
  if (!m_previewImage || !m_previewName || !m_previewMeta || !m_listView ||
      !m_listView->selectionModel()) {
    return;
  }

  const QModelIndexList selected = m_listView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    m_previewImage->setPixmap(QPixmap());
    m_previewImage->setText(tr("No preview"));
    m_previewName->setText(tr("No selection"));
    m_previewMeta->setText(tr("Select a file to see details."));
    return;
  }

  if (selected.size() > 1) {
    m_previewImage->setPixmap(QPixmap());
    m_previewImage->setText(tr("Multiple"));
    m_previewName->setText(tr("%1 files selected").arg(selected.size()));
    m_previewMeta->setText(tr("Preview disabled for multi-selection."));
    return;
  }

  if (!m_filterProxy) {
    return;
  }

  const QModelIndex source = static_cast<NMFileFilterProxy *>(m_filterProxy)
                                 ->mapToSource(selected.front());
  if (!source.isValid()) {
    return;
  }

  const QString path = m_fileModel->filePath(source);
  if (path.isEmpty()) {
    return;
  }

  QFileInfo info(path);
  m_previewName->setText(info.fileName());

  if (info.isDir()) {
    QFileIconProvider iconProvider;
    m_previewImage->setPixmap(
        iconProvider.icon(info).pixmap(m_previewImage->size()));
    m_previewMeta->setText(tr("Folder"));
    return;
  }

  const QString suffix = info.suffix().toLower();
  if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" ||
      suffix == "bmp" || suffix == "gif") {
    QImage image(path);
    if (!image.isNull()) {
      QPixmap pix = QPixmap::fromImage(image);
      m_previewImage->setPixmap(pix.scaled(m_previewImage->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
      m_previewMeta->setText(tr("%1 x %2 | %3 KB")
                                 .arg(image.width())
                                 .arg(image.height())
                                 .arg(static_cast<int>(info.size() / 1024)));
      return;
    }
  }

  QFileIconProvider iconProvider;
  m_previewImage->setPixmap(
      iconProvider.icon(info).pixmap(m_previewImage->size()));
  m_previewMeta->setText(tr("%1 KB").arg(static_cast<int>(info.size() / 1024)));
}

void NMFileDialog::acceptSelection() {
  if (m_mode == Mode::SelectDirectory) {
    const QString dir = selectedDirectoryPath();
    if (!dir.isEmpty()) {
      m_selectedPaths = {dir};
      accept();
    }
    return;
  }

  if (m_mode == Mode::SaveFile) {
    if (!m_filenameEdit || m_filenameEdit->text().trimmed().isEmpty()) {
      return;
    }
    const QString fullPath =
        QDir(m_currentDir).filePath(m_filenameEdit->text().trimmed());
    m_selectedPaths = {fullPath};
    accept();
    return;
  }

  const QStringList files = selectedFilePaths();
  if (files.isEmpty()) {
    return;
  }
  m_selectedPaths = files;
  accept();
}

QString NMFileDialog::getOpenFileName(QWidget *parent, const QString &title,
                                      const QString &dir,
                                      const QString &filter) {
  NMFileDialog dialog(parent, title, Mode::OpenFile, dir, filter);
  if (dialog.exec() == QDialog::Accepted) {
    const QStringList files = dialog.m_selectedPaths;
    return files.isEmpty() ? QString() : files.front();
  }
  return QString();
}

QStringList NMFileDialog::getOpenFileNames(QWidget *parent,
                                           const QString &title,
                                           const QString &dir,
                                           const QString &filter) {
  NMFileDialog dialog(parent, title, Mode::OpenFiles, dir, filter);
  if (dialog.exec() == QDialog::Accepted) {
    return dialog.m_selectedPaths;
  }
  return {};
}

QString NMFileDialog::getSaveFileName(QWidget *parent, const QString &title,
                                      const QString &dir,
                                      const QString &filter) {
  NMFileDialog dialog(parent, title, Mode::SaveFile, dir, filter);
  if (dialog.exec() == QDialog::Accepted) {
    const QStringList files = dialog.m_selectedPaths;
    return files.isEmpty() ? QString() : files.front();
  }
  return QString();
}

QString NMFileDialog::getExistingDirectory(QWidget *parent,
                                           const QString &title,
                                           const QString &dir) {
  NMFileDialog dialog(parent, title, Mode::SelectDirectory, dir, QString());
  if (dialog.exec() == QDialog::Accepted) {
    return dialog.m_selectedPaths.isEmpty() ? QString()
                                            : dialog.m_selectedPaths.front();
  }
  return QString();
}

} // namespace NovelMind::editor::qt
